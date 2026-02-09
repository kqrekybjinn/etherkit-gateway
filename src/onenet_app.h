#ifndef _ONENET_APP_H_
#define _ONENET_APP_H_

#include <rtthread.h>
#include "mqttclient.h"

/* 初始化 OneNET 应用 (订阅 Topic 等) */
void onenet_app_init(mqtt_client_t *client);

/* 上报 CAN 数据 */
void onenet_upload_can(mqtt_client_t *client, uint32_t can_id, uint8_t *data, uint8_t len);

/* 上报 ADC 数据 */
void onenet_upload_adc(mqtt_client_t *client, float voltage, int32_t raw_value);

#endif /* _ONENET_APP_H_ */
