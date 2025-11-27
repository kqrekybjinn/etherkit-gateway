#ifndef _ONENET_CONFIG_H_
#define _ONENET_CONFIG_H_

/* ================= OneNET 配置区域 ================= */
/* 请使用 scripts/onenet_token.py 生成 Token */
#define ONENET_HOST        "mqtts.heclouds.com"
#define ONENET_PORT        "1883"
#define ONENET_PROD_ID     "4cqPeql2a4"/* 产品 ID */
#define ONENET_DEV_NAME    "gate"     /* 设备名称 */
#define ONENET_TOKEN       "version=2018-10-31&res=products%2F4cqPeql2a4%2Fdevices%2Fgate&et=1795737600&method=md5&sign=RaBFQ5yUFGYhVZ1JkiAtNw%3D%3D" /* Token */
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
