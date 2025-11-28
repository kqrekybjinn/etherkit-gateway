/*
 * Copyright (c) 2006-2024 RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2024-03-11    Wangyuqiang   first version
 */

#include <rtthread.h>
#include "hal_data.h"
#include <rtdevice.h>
#include <board.h>
#include <arpa/inet.h>
#include <netdev.h>

#include "mqttclient.h"
#include "onenet_config.h"

#define LED_PIN_0    BSP_IO_PORT_14_PIN_3 
#define LED_PIN_1    BSP_IO_PORT_14_PIN_0 
#define LED_PIN_2    BSP_IO_PORT_14_PIN_1 

#include "onenet_config.h"
#include "onenet_app.h"

static rt_device_t wdg_dev = RT_NULL;

static void idle_hook(void)
{
    /* 在空闲线程的回调函数里喂狗 */
    if (wdg_dev)
    {
        rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, NULL);
    }
}

static char cid[64] = { 0 };
mqtt_client_t *kawaii_client = NULL; /* 全局 MQTT 客户端实例 */

static void onenet_reconnect_callback(void* client, void* parameter)
{
    KAWAII_MQTT_LOG_I("MQTT Reconnected automatically!");
    onenet_app_init((mqtt_client_t *)client);
}

static void kawaii_mqtt_demo(void *parameter)
{
    struct netdev *netdev = RT_NULL;
    while (1)
    {
        netdev = netdev_get_by_name("e0");
        if (netdev != RT_NULL && netdev->ip_addr.addr != 0)
        {
            KAWAII_MQTT_LOG_I("Network is up! IP: %s", inet_ntoa(netdev->ip_addr));
            break;
        }
        KAWAII_MQTT_LOG_W("Waiting for network IP...");
        rt_thread_mdelay(1000);
    }

    mqtt_log_init();
    kawaii_client = mqtt_lease();
    
    rt_snprintf(cid, sizeof(cid), "rtthread-5323-%d", rt_tick_get());
    
    mqtt_set_host(kawaii_client, ONENET_HOST);
    mqtt_set_port(kawaii_client, ONENET_PORT);
    mqtt_set_user_name(kawaii_client, ONENET_PROD_ID);
    mqtt_set_password(kawaii_client, ONENET_TOKEN);
    mqtt_set_client_id(kawaii_client, ONENET_DEV_NAME);
    mqtt_set_clean_session(kawaii_client, 1);
    
    /* 设置超时和缓冲区 */
    mqtt_set_cmd_timeout(kawaii_client, 5000);
    mqtt_set_read_buf_size(kawaii_client, 2048);
    mqtt_set_write_buf_size(kawaii_client, 2048);
    mqtt_set_keep_alive_interval(kawaii_client, 60); /* 设置心跳间隔为 60秒 */
    
    /* 设置自动重连回调和重试间隔 */
    mqtt_set_reconnect_handler(kawaii_client, onenet_reconnect_callback);
    mqtt_set_reconnect_try_duration(kawaii_client, 2000);

    KAWAII_MQTT_LOG_I("The ID of the Kawaii client is: %s ", ONENET_DEV_NAME);
    
    /* 标记 MQTT 线程是否活跃 */
    rt_bool_t mqtt_active = RT_FALSE;

    /* 2. 尝试连接并打印结果 */
    int res = mqtt_connect(kawaii_client);
    if (res != 0)
    {
        KAWAII_MQTT_LOG_E("MQTT connect failed! Error code: %d", res);
        KAWAII_MQTT_LOG_E("Please check: 1. Network/DNS  2. Token/Product ID  3. Firewall");
    }
    else
    {
        KAWAII_MQTT_LOG_I("MQTT connect success!");
        mqtt_active = RT_TRUE;
        onenet_app_init(kawaii_client);
    }
//启动采集
    extern int app_task_init(void);
    app_task_init();
    
    rt_bool_t last_link_status = RT_TRUE;

    while (1) {
        /* 检查网络是否就绪 (有 IP 地址 + 链路UP) */
        struct netdev *netdev = netdev_get_by_name("e0");
        rt_bool_t has_ip = (netdev != RT_NULL && netdev->ip_addr.addr != 0);
        rt_bool_t link_up = (netdev != RT_NULL && netdev_is_link_up(netdev));

        /* 状态变迁检测：从 UP 变为 DOWN */
        if (last_link_status && !link_up) {
            KAWAII_MQTT_LOG_W("Physical Link Down Detected! Disconnecting MQTT...");
            /* 物理链路断开，立即断开 MQTT 连接，防止 socket 僵死 */
            mqtt_disconnect(kawaii_client);
            mqtt_active = RT_FALSE;
        }
        last_link_status = link_up;

        if (!has_ip || !link_up) {
             /* 网络异常时，不频繁打印，避免干扰 */
             if (!has_ip && link_up) {
                 KAWAII_MQTT_LOG_W("Network down (No IP). Waiting...");
             }
             rt_thread_mdelay(2000);
             continue;
        }

        /* 链路正常时的处理 */
        if (!mqtt_active) {
            /* 如果 MQTT 线程已停止（例如之前因断网而断开），则尝试重新连接 */
            KAWAII_MQTT_LOG_W("Link is UP but MQTT stopped. Waiting for network stability...");
            
            /* 增加延时，确保网络栈（ARP/路由）完全就绪，避免 connect 阻塞或立即失败 */
            rt_thread_mdelay(3000);

            KAWAII_MQTT_LOG_I("Starting MQTT...");
            if (mqtt_connect(kawaii_client) == 0) {
                KAWAII_MQTT_LOG_I("MQTT Start Success!");
                mqtt_active = RT_TRUE;
                onenet_app_init(kawaii_client);
            } else {
                KAWAII_MQTT_LOG_E("MQTT Start Failed. Retrying in 5s...");
                rt_thread_mdelay(5000);
            }
        } else {
            /* MQTT 线程活跃中，由 kawaii-mqtt 自动处理心跳和软断线重连 */
            /* 我们只需要监控状态即可 */
            if (kawaii_client->mqtt_client_state != CLIENT_STATE_CONNECTED) {
                // KAWAII_MQTT_LOG_D("MQTT State: %d (Auto-reconnecting...)", kawaii_client->mqtt_client_state);
            }
            mqtt_sleep_ms(1000);
        }
    }
}

void hal_entry(void)
{
    rt_kprintf("\netherkit gateway\n");

    /* 初始化看门狗 (WDT) */
    wdg_dev = rt_device_find("wdt");
    if (wdg_dev)
    {
        /* 设置超时时间为 10 秒 */
        rt_uint32_t timeout = 10;
        rt_device_init(wdg_dev);
        rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout);
        rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL);
        
        /* 设置空闲线程钩子，在空闲时自动喂狗 */
        rt_thread_idle_sethook(idle_hook);
        rt_kprintf("[WDT] Watchdog started. Timeout: %ds\n", timeout);
    }
    else
    {
        rt_kprintf("[WDT] Device 'wdt' not found!\n");
    }

    /* 初始化 LED 引脚模式 */

    rt_thread_t tid_mqtt;
    tid_mqtt = rt_thread_create("kawaii_demo", kawaii_mqtt_demo, RT_NULL, 4096, 17, 10);
    if (tid_mqtt != RT_NULL) {
        rt_thread_startup(tid_mqtt);
    }

    while (1)
    {
        rt_pin_write(LED_PIN_0, PIN_HIGH);
        rt_pin_write(LED_PIN_1, PIN_HIGH);
        rt_pin_write(LED_PIN_2, PIN_HIGH);
        rt_thread_mdelay(1000);
        rt_pin_write(LED_PIN_0, PIN_LOW);
        rt_pin_write(LED_PIN_1, PIN_LOW);
        rt_pin_write(LED_PIN_2, PIN_LOW);
        rt_thread_mdelay(1000);
    }
}

