/*
 * Copyright (c) 2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-11-27     Gateway      First version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "mqttclient.h"
#include "onenet_config.h"
#include "onenet_app.h"

/* 定义设备名称，与 factory_test.h 中保持一致或使用标准名称 */
#define CAN_DEV_NAME       "canfd0"
#define ADC_DEV_NAME       "adc0"
#define ADC_DEV_CHANNEL    0
#define RS485_DEV_NAME     "uart5"

/* 信号量 */
static struct rt_semaphore can_rx_sem;

/* CAN 接收回调 */
static rt_err_t can_rx_callback(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&can_rx_sem);
    return RT_EOK;
}

/* CAN 处理线程 */
static void can_thread_entry(void *parameter)
{
    rt_device_t dev = (rt_device_t)parameter;
    struct rt_can_msg rxmsg = {0};
    extern mqtt_client_t *kawaii_client; /* 引用全局客户端 */

    while (1)
    {
        /* 等待 MQTT 客户端初始化完成 */
        if (kawaii_client == NULL || kawaii_client->mqtt_client_state != CLIENT_STATE_CONNECTED) {
            rt_thread_mdelay(1000);
            continue;
        }

        rxmsg.hdr_index = -1;
        if (rt_sem_take(&can_rx_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            if (rt_device_read(dev, 0, &rxmsg, sizeof(rxmsg)) > 0)
            {
                /* 使用 onenet_app 模块上报数据 */
                onenet_upload_can(kawaii_client, rxmsg.id, rxmsg.data, rxmsg.len);
            }
        }
    }
}

/* 传感器采集线程 (ADC) */
static void sensor_thread_entry(void *parameter)
{
    rt_adc_device_t adc_dev = (rt_adc_device_t)rt_device_find(ADC_DEV_NAME);
    extern mqtt_client_t *kawaii_client; /* 引用全局客户端 */

    if (adc_dev == RT_NULL)
    {
        rt_kprintf("ADC device %s not found!\n", ADC_DEV_NAME);
        return;
    }

    rt_adc_enable(adc_dev, ADC_DEV_CHANNEL);

    while (1)
    {
        /* 等待 MQTT 客户端初始化完成 */
        if (kawaii_client == NULL || kawaii_client->mqtt_client_state != CLIENT_STATE_CONNECTED) {
            rt_thread_mdelay(1000);
            continue;
        }

        rt_uint32_t value = rt_adc_read(adc_dev, ADC_DEV_CHANNEL);
        /* 参考电压 3.3V, 12位精度 */
        float voltage = (float)value * 3.3f / 4096.0f;

        /* 使用 onenet_app 模块上报数据 */
        onenet_upload_adc(kawaii_client, voltage, value);

        rt_thread_mdelay(2000); /* 2秒采集一次 */
    }
}

/* RS485 (UART) 接收线程示例 */
static void rs485_thread_entry(void *parameter)
{
    rt_device_t serial = rt_device_find(RS485_DEV_NAME);
    char ch;
    
    if (!serial)
    {
        rt_kprintf("RS485 device %s not found!\n", RS485_DEV_NAME);
        return;
    }

    /* 以中断接收模式打开 */
    rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    
    /* 注意：这里为了简单演示，使用轮询或简单的阻塞读取。
       实际项目中建议配合 struct rt_semaphore 使用 rx_indicate 回调 */
    
    while (1)
    {
        /* 简单的回显测试或协议解析 */
        if (rt_device_read(serial, 0, &ch, 1) > 0)
        {
            // rt_kprintf("RS485 Rx: %02X\n", ch);
            // 解析 Modbus 或其他协议...
        }
        rt_thread_mdelay(10);
    }
}

/* 应用初始化入口 */
int app_task_init(void)
{
    rt_err_t res;

    /* 1. 初始化 CAN */
    rt_device_t can_dev = rt_device_find(CAN_DEV_NAME);
    if (can_dev)
    {
        rt_sem_init(&can_rx_sem, "can_sem", 0, RT_IPC_FLAG_FIFO);
        rt_device_set_rx_indicate(can_dev, can_rx_callback);
        res = rt_device_open(can_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
        if (res == RT_EOK)
        {
            rt_thread_t tid = rt_thread_create("app_can", can_thread_entry, can_dev, 2048, 20, 10);
            if (tid) rt_thread_startup(tid);
        }
    }
    else
    {
        rt_kprintf("CAN device not found, skip.\n");
    }

    /* 2. 初始化 ADC 采集任务 */
    rt_thread_t adc_tid = rt_thread_create("app_adc", sensor_thread_entry, RT_NULL, 2048, 21, 10);
    if (adc_tid) rt_thread_startup(adc_tid);

    /* 3. 初始化 RS485 (可选) */
    // rt_thread_t rs485_tid = rt_thread_create("app_485", rs485_thread_entry, RT_NULL, 2048, 22, 10);
    // if (rs485_tid) rt_thread_startup(rs485_tid);

    return 0;
}
/* 导出命令，方便在 Shell 中手动启动测试 */
MSH_CMD_EXPORT(app_task_init, Start application tasks);
