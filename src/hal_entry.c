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

#define LED_PIN_0    BSP_IO_PORT_14_PIN_3 /* Onboard LED pins */
#define LED_PIN_1    BSP_IO_PORT_14_PIN_0 /* Onboard LED pins */
#define LED_PIN_2    BSP_IO_PORT_14_PIN_1 /* Onboard LED pins */

#include "onenet_config.h"
#include "onenet_app.h"

static int mqtt_publish_handle1(mqtt_client_t *client)
{
    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.qos = QOS0;
    msg.payload = (void *)"this is a kawaii mqtt test ...";
    return mqtt_publish(client, "pub5323", &msg);
}

static char cid[64] = { 0 };
mqtt_client_t *kawaii_client = NULL; /* 全局 MQTT 客户端实例 */

static void kawaii_mqtt_demo(void *parameter)
{
    struct netdev *netdev = RT_NULL;
    while (1)
    {
        netdev = netdev_get_first_by_flags(NETDEV_FLAG_LINK_UP);
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
    
    KAWAII_MQTT_LOG_I("The ID of the Kawaii client is: %s ", ONENET_DEV_NAME);
    
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
        /* 初始化 OneNET 应用 (订阅 Topic) */
        onenet_app_init(kawaii_client);
    }
    
    /* 启动应用层采集任务 (确保在 MQTT 连接尝试后启动) */
    extern int app_task_init(void);
    app_task_init();
    
    while (1) {
        /* 只有连接状态才发送，且降低频率到 10秒一次，减轻网络负载 */
        if (kawaii_client->mqtt_client_state == CLIENT_STATE_CONNECTED) {
            /* 
             * 注意：如果 OneNET 平台对上行数据频率有限制（例如免费版限制 1条/秒），
             * 过于频繁的 publish 可能会导致连接被服务器强制断开。
             * 这里我们只发送心跳包，不发送业务数据，业务数据由 app_task.c 发送。
             */
            // mqtt_publish_handle1(kawaii_client); 
            KAWAII_MQTT_LOG_I("MQTT is Online. Keep alive...");
        } else {
            KAWAII_MQTT_LOG_W("MQTT is Offline! State: %d. Trying to reconnect...", kawaii_client->mqtt_client_state);
            mqtt_connect(kawaii_client);
        }
        mqtt_sleep_ms(10 * 1000);
    }
}

void hal_entry(void)
{
    rt_kprintf("\nHello RT-Thread!\n");
    rt_kprintf("==================================================\n");
    rt_kprintf("This example project is an mqtt component routine!\n");
    rt_kprintf("==================================================\n");

    rt_thread_t tid_mqtt;
    /* 3. 增大栈空间到 4096 */
    tid_mqtt = rt_thread_create("kawaii_demo", kawaii_mqtt_demo, RT_NULL, 4096, 17, 10);
    if (tid_mqtt != RT_NULL) {
        rt_thread_startup(tid_mqtt);
    }

    /* 移除这里的 app_task_init，移到 kawaii_mqtt_demo 内部 */
    // extern int app_task_init(void);
    // app_task_init();

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

