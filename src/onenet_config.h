#ifndef _ONENET_CONFIG_H_
#define _ONENET_CONFIG_H_

/* ================= OneNET 配置区域 ================= */
/* 请使用 scripts/onenet_token.py 生成 Token */
#define ONENET_HOST        "mqtts.heclouds.com"
#define ONENET_PORT        "1883"
#define ONENET_PROD_ID    
#define ONENET_DEV_NAME       /* 设备名称 */
#define ONENET_TOKEN      
/* =================================================== */

/* OneNET 物模型上报 Topic */
/* 格式: $sys/{pid}/{device-name}/thing/property/post */
#define ONENET_TOPIC_PROP_POST  "$sys/" ONENET_PROD_ID "/" ONENET_DEV_NAME "/thing/property/post"

/* OneNET 物模型上报回复 Topic (上行) */
/* 格式: $sys/{pid}/{device-name}/thing/property/post/reply */
#define ONENET_TOPIC_PROP_POST_REPLY "$sys/" ONENET_PROD_ID "/" ONENET_DEV_NAME "/thing/property/post/reply"

/* OneNET 物模型属性设置 Topic (下行) */
/* 格式: $sys/{pid}/{device-name}/thing/property/set */
#define ONENET_TOPIC_PROP_SET   "$sys/" ONENET_PROD_ID "/" ONENET_DEV_NAME "/thing/property/set"

/* OneNET 物模型属性设置回复 Topic (上行) */
/* 格式: $sys/{pid}/{device-name}/thing/property/set_reply */
#define ONENET_TOPIC_PROP_SET_REPLY "$sys/" ONENET_PROD_ID "/" ONENET_DEV_NAME "/thing/property/set_reply"

#endif /* _ONENET_CONFIG_H_ */
