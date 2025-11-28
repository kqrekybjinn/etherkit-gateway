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
#include "hal_data.h"
#include <string.h>
#include "offline_cache.h"

/* 定义设备名称，与 factory_test.h 中保持一致或使用标准名称 */
#define CAN_DEV_NAME       "canfd0"
#define ADC_DEV_NAME       "adc0"
#define ADC_DEV_CHANNEL    0
#define RS485_DEV_NAME     "uart5"

/* 时间参数配置 (ms) */
#define CACHE_UPLOAD_INTERVAL_MS    200   
#define CACHE_UPLOAD_DELAY_MS       50    
#define SENSOR_REPORT_INTERVAL_MS   10000 
#define SENSOR_SAMPLE_INTERVAL_MS   1000 

/* 信号量 */
static struct rt_semaphore can_rx_sem;

/* CAN 接收回调 */
static rt_err_t can_rx_callback(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&can_rx_sem);
    return RT_EOK;
}
void rs485_callback(uart_callback_args_t * p_args)
{

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

/* 边缘计算模型：滑动窗口滤波 */
#define FILTER_WINDOW_SIZE 10
#define REPORT_INTERVAL_MS 10000  /* 正常上报周期 10秒 */

typedef struct {
    float buffer[FILTER_WINDOW_SIZE];
    rt_uint8_t index;
    rt_uint8_t count;
    float sum;
    float average;
    rt_tick_t last_report_tick;
} Edge_ADC_Model;

static void edge_model_init(Edge_ADC_Model *model) {
    memset(model, 0, sizeof(Edge_ADC_Model));
}

static void edge_model_input(Edge_ADC_Model *model, float new_val) {
    /* 移除旧值 */
    if (model->count >= FILTER_WINDOW_SIZE) {
        model->sum -= model->buffer[model->index];
    } else {
        model->count++;
    }
    
    /* 加入新值 */
    model->buffer[model->index] = new_val;
    model->sum += new_val;
    model->index = (model->index + 1) % FILTER_WINDOW_SIZE;
    
    /* 计算平均值 */
    model->average = model->sum / model->count;
}

static rt_tick_t last_cache_upload_tick = 0;

static void handle_data_upload(mqtt_client_t *client, float voltage, uint32_t raw_value)
{
    /* 1. 尝试发送当前实时数据 */
    int ret = onenet_upload_adc(client, voltage, raw_value);
    
    if (ret == 0) {
        /* 发送成功 */
        
        /* 2. 检查是否有离线缓存需要补传 */
        /* 策略：网络空闲时补传。这里简单策略：每次成功发送实时数据后，尝试补传一条缓存数据 */
        /* 但为了避免拥塞，限制补传频率，例如每 5 秒最多补传一条 */
        
        rt_tick_t current_tick = rt_tick_get();
        if (last_cache_upload_tick == 0) last_cache_upload_tick = current_tick; // 初始化

        if (!offline_cache_is_empty() && (current_tick - last_cache_upload_tick > CACHE_UPLOAD_INTERVAL_MS)) {
             CacheRecord record;
             if (offline_cache_read(&record) == RT_EOK) {
                 rt_kprintf("[Cache] Found cached data, uploading...\n");
                 
                 /* 稍微延时一下，避免和刚才的实时数据包挨得太近 */
                 rt_thread_mdelay(CACHE_UPLOAD_DELAY_MS); 
                 
                 if (onenet_upload_adc(client, record.value_f, record.value_raw) == 0) {
                     /* 补传成功，从缓存中移除 */
                     offline_cache_pop();
                     rt_kprintf("[Cache] Upload success, popped.\n");
                     last_cache_upload_tick = current_tick;
                 } else {
                     rt_kprintf("[Cache] Upload failed, keep in cache.\n");
                 }
             }
        }
        
    } else {
        /* 发送失败 (ret != 0) */
        rt_kprintf("[Edge] Upload failed (ret=%d), saving to cache...\n", ret);
        offline_cache_write(CACHE_TYPE_ADC, voltage, raw_value);

        /* 如果是发送错误 (如 -19 KAWAII_MQTT_SEND_PACKET_ERROR)，主动关闭连接触发重连 */
        if (ret != -1) { /* -1 是 onenet_upload_adc 内部判断未连接的返回值，无需处理 */
            rt_kprintf("[Edge] Connection error, closing MQTT client to force reconnect.\n");
            mqtt_disconnect(client);
        }
    }
}

/* 传感器采集线程 (ADC) */
static void sensor_thread_entry(void *parameter)
{
    rt_adc_device_t adc_dev = (rt_adc_device_t)rt_device_find(ADC_DEV_NAME);
    extern mqtt_client_t *kawaii_client; /* 引用全局客户端 */

    /* 初始化边缘模型 */
    Edge_ADC_Model adc_model;
    edge_model_init(&adc_model);

    if (adc_dev == RT_NULL)
    {
        rt_kprintf("ADC device %s not found!\n", ADC_DEV_NAME);
        return;
    }

    rt_adc_enable(adc_dev, ADC_DEV_CHANNEL);

    while (1)
    {
        /* 移除阻塞等待，允许离线采集和缓存 */
        /* if (kawaii_client == NULL || kawaii_client->mqtt_client_state != CLIENT_STATE_CONNECTED) { ... } */

        rt_uint32_t value = rt_adc_read(adc_dev, ADC_DEV_CHANNEL);
        /* 参考电压 3.3V, 12位精度 */
        float voltage = (float)value * 3.3f / 4096.0f;

        /* 输入到滤波模型 */
        edge_model_input(&adc_model, voltage);

        /* 检查是否达到上报时间间隔 */
        if (rt_tick_get() - adc_model.last_report_tick > SENSOR_REPORT_INTERVAL_MS) {
             int avg_int = (int)adc_model.average;
             int avg_dec = (int)((adc_model.average - avg_int) * 100);
             rt_kprintf("[Edge] Report Average: %d.%02dV (Window: %d)\n", avg_int, avg_dec, adc_model.count);
             
             /* 上报平均值 (使用带缓存功能的处理函数) */
             handle_data_upload(kawaii_client, adc_model.average, (int32_t)(adc_model.average * 4096.0f / 3.3f));

             adc_model.last_report_tick = rt_tick_get();
        }

        rt_thread_mdelay(SENSOR_SAMPLE_INTERVAL_MS); /* 采集间隔，用于更新滑动窗口 */
    }
}

/* RS485 (UART) 接收线程示例 */
#if 0
static void rs485_thread_entry(void *parameter)
{
    rt_device_t serial = rt_device_find(RS485_DEV_NAME);
    char ch;
    
    if (!serial)
    {
        rt_kprintf("Serial device %s not found!\n", RS485_DEV_NAME);
        return;
    }

    /* 以中断接收及轮询发送模式打开串口设备 */
    rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);

    while (1)
    {
        /* 从串口读取一个字节的数据，没有读取到则等待接收信号量 */
        while (rt_device_read(serial, -1, &ch, 1) != 1)
        {
            /* 阻塞等待接收信号量，等到信号量后再次读取数据 */
            rt_sem_take(&rx_sem, RT_WAITING_FOREVER);
        }
        /* 读取到的数据通过串口输出 */
        // rt_device_write(serial, 0, &ch, 1);
    }
}
#endif

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