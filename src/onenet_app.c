#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#include <stdio.h>
#include "onenet_app.h"
#include "onenet_config.h"

#define LED_PIN_0    BSP_IO_PORT_14_PIN_3
#define LED_PIN_1    BSP_IO_PORT_14_PIN_0
#define LED_PIN_2    BSP_IO_PORT_14_PIN_1

static void onenet_cmd_callback(void* client, message_data_t* msg)
{
    (void) client;
    char *payload = (char*)msg->message->payload;
    KAWAII_MQTT_LOG_I("Receive OneNET Command: %s", payload);

    /* 简单解析 JSON: 查找 "led_switch" */
    if (strstr(payload, "\"led_switch\":true") || strstr(payload, "\"led_switch\":1"))
    {
        KAWAII_MQTT_LOG_I("LED ON Command Received!");
        rt_pin_write(LED_PIN_0, PIN_HIGH);
        rt_pin_write(LED_PIN_1, PIN_HIGH);
        rt_pin_write(LED_PIN_2, PIN_HIGH);
    }
    else if (strstr(payload, "\"led_switch\":false") || strstr(payload, "\"led_switch\":0"))
    {
        KAWAII_MQTT_LOG_I("LED OFF Command Received!");
        rt_pin_write(LED_PIN_0, PIN_LOW);
        rt_pin_write(LED_PIN_1, PIN_LOW);
        rt_pin_write(LED_PIN_2, PIN_LOW);
    }

    /* 提取 ID 用于回复 */
    char request_id[32] = {0};
    char *id_start = strstr(payload, "\"id\":\"");
    if (id_start)
    {
        id_start += 6; /* skip "id":" */
        char *id_end = strchr(id_start, '\"');
        if (id_end && (id_end - id_start < sizeof(request_id)))
        {
            memcpy(request_id, id_start, id_end - id_start);
        }
    }

    /* 发送回复 */
    if (strlen(request_id) > 0)
    {
        char reply_payload[128];
        rt_snprintf(reply_payload, sizeof(reply_payload), "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", request_id);
        
        mqtt_message_t reply_msg;
        memset(&reply_msg, 0, sizeof(reply_msg));
        reply_msg.qos = QOS0;
        reply_msg.payload = (void *)reply_payload;
        
        mqtt_publish(client, ONENET_TOPIC_PROP_SET_REPLY, &reply_msg);
        KAWAII_MQTT_LOG_I("Reply sent: %s", reply_payload);
    }
}

static void onenet_post_reply_callback(void* client, message_data_t* msg)
{
    (void) client;
    KAWAII_MQTT_LOG_I("OneNET Post Reply: %s", (char*)msg->message->payload);
}

void onenet_app_init(mqtt_client_t *client)
{
    if (client) {
        /* 订阅 OneNET 属性设置 Topic */
        mqtt_subscribe(client, ONENET_TOPIC_PROP_SET, QOS0, onenet_cmd_callback);
        /* 订阅 OneNET 属性上报回复 Topic (用于调试上报失败原因) */
        mqtt_subscribe(client, ONENET_TOPIC_PROP_POST_REPLY, QOS0, onenet_post_reply_callback);
    }
}

void onenet_upload_can(mqtt_client_t *client, uint32_t can_id, uint8_t *data, uint8_t len)
{
    if (client == NULL || client->mqtt_client_state != CLIENT_STATE_CONNECTED) {
        return;
    }

    char payload[512];
    char can_data_str[64];
    
    memset(can_data_str, 0, sizeof(can_data_str));
    int data_len = (len > 8) ? 8 : len;
    
    for (int i = 0; i < data_len; i++)
    {
        rt_snprintf(can_data_str + i * 2, 3, "%02X", data[i]);
    }

    rt_snprintf(payload, sizeof(payload), 
                "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
                "\"can_id\":{\"value\":\"0x%08X\"},"
                "\"can_data\":{\"value\":\"%s\"}"
                "}}", 
                rt_tick_get(), can_id, can_data_str);
    
    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.qos = QOS1; /* 使用 QOS1 确保送达 */
    msg.payload = (void *)payload;
    mqtt_publish(client, ONENET_TOPIC_PROP_POST, &msg);
    rt_kprintf("[CAN] Pub: %s\n", payload);
}

void onenet_upload_adc(mqtt_client_t *client, float voltage, int32_t raw_value)
{
    if (client == NULL || client->mqtt_client_state != CLIENT_STATE_CONNECTED) {
        return;
    }

    /* RT-Thread 的 rt_snprintf 默认可能不支持浮点数 %f */
    /* 手动转换浮点数为整数+小数部分 */
    int vol_int = (int)voltage;
    int vol_dec = (int)((voltage - vol_int) * 100);

    char payload[256];
    rt_snprintf(payload, sizeof(payload), 
                "{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{"
                "\"voltage\":{\"value\":%d.%02d},"
                "\"raw_adc\":{\"value\":%d}"
                "}}", 
                rt_tick_get(), vol_int, vol_dec, raw_value);

    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.qos = QOS1; /* 使用 QOS1 确保送达 */
    msg.payload = (void *)payload;
    mqtt_publish(client, ONENET_TOPIC_PROP_POST, &msg);
    rt_kprintf("[ADC] Pub: %s\n", payload);
}
