#ifndef __OFFLINE_CACHE_H__
#define __OFFLINE_CACHE_H__

#include <rtthread.h>

/* 缓存数据类型 */
typedef enum {
    CACHE_TYPE_ADC = 1,
    CACHE_TYPE_CAN = 2,
} CacheType;

/* 缓存记录结构体 (16 bytes) */
typedef struct {
    rt_uint32_t timestamp; /* 时间戳 */
    rt_uint32_t value_raw; /* 原始值 (ADC raw 或 CAN ID) */
    float       value_f;   /* 浮点值 (ADC voltage) */
    rt_uint8_t  type;      /* 数据类型 */
    rt_uint8_t  reserved[3]; /* 保留对齐 */
} CacheRecord;

/* API */
int offline_cache_init(void);
int offline_cache_write(CacheType type, float val_f, rt_uint32_t val_raw);
int offline_cache_read(CacheRecord *record);
int offline_cache_pop(void); /* 确认读取成功，移动读指针 */
int offline_cache_is_empty(void);
int offline_cache_get_count(void);

#endif
