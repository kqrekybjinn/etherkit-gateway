#include <rtthread.h>
#include <rtdevice.h>
#include "at24cxx.h"
#include "offline_cache.h"

/* 配置 */
#define CACHE_I2C_BUS_NAME  "i2c0" /* 连接到 IIC0 */
#define CACHE_EEPROM_ADDR   0      /* AT24C16 通常不需要 A0-A2，或者由驱动处理 */

/* 存储布局 */
/* 
 * Header (16 bytes):
 * [0-3] Magic Number (0xCAFEBABE)
 * [4-7] Head Index (Write Ptr)
 * [8-11] Tail Index (Read Ptr)
 * [12-15] Count
 */
#define CACHE_MAGIC         0xCAFEBABE
#define CACHE_HEADER_SIZE   16
#define CACHE_RECORD_SIZE   sizeof(CacheRecord)

/* AT24C16 容量为 2048 字节 */
#define EEPROM_SIZE         2048
#define MAX_RECORDS         ((EEPROM_SIZE - CACHE_HEADER_SIZE) / CACHE_RECORD_SIZE)

static at24cxx_device_t ee_dev = RT_NULL;
static rt_mutex_t cache_lock = RT_NULL;

typedef struct {
    rt_uint32_t magic;
    rt_uint32_t head;
    rt_uint32_t tail;
    rt_uint32_t count;
} CacheHeader;

static CacheHeader header;

static void save_header(void)
{
    if (ee_dev) {
        at24cxx_write(ee_dev, 0, (uint8_t *)&header, sizeof(CacheHeader));
    }
}

static void load_header(void)
{
    if (ee_dev) {
        at24cxx_read(ee_dev, 0, (uint8_t *)&header, sizeof(CacheHeader));
    }
}

int offline_cache_init(void)
{
    /* 初始化 I2C 设备 */
    /* 注意：at24cxx_init 内部会查找 i2c 总线设备 */
    ee_dev = at24cxx_init(CACHE_I2C_BUS_NAME, CACHE_EEPROM_ADDR);
    if (ee_dev == RT_NULL) {
        rt_kprintf("[Cache] Init failed! I2C bus %s not found or device error.\n", CACHE_I2C_BUS_NAME);
        return -RT_ERROR;
    }

    cache_lock = rt_mutex_create("cache_lock", RT_IPC_FLAG_FIFO);

    rt_kprintf("[Cache] Clearing EEPROM on startup...\n");
    header.magic = CACHE_MAGIC;
    header.head = 0;
    header.tail = 0;
    header.count = 0;
    save_header();


    return RT_EOK;
}

int offline_cache_write(CacheType type, float val_f, rt_uint32_t val_raw)
{
    if (ee_dev == RT_NULL) return -RT_ERROR;

    rt_mutex_take(cache_lock, RT_WAITING_FOREVER);

    /* 检查是否已满 (覆盖策略：如果满了，Tail 前移) */
    if (header.count >= MAX_RECORDS) {
        header.tail = (header.tail + 1) % MAX_RECORDS;
        header.count--; 
    }

    /* 准备记录 */
    CacheRecord record;
    record.timestamp = rt_tick_get();
    record.type = (rt_uint8_t)type;
    record.value_f = val_f;
    record.value_raw = val_raw;
    memset(record.reserved, 0, sizeof(record.reserved));

    /* 计算写入地址 */
    uint32_t addr = CACHE_HEADER_SIZE + header.head * CACHE_RECORD_SIZE;
    
    /* 写入 EEPROM */
    if (at24cxx_write(ee_dev, addr, (uint8_t *)&record, sizeof(CacheRecord)) == RT_EOK) {
        header.head = (header.head + 1) % MAX_RECORDS;
        header.count++;
        save_header();
        rt_mutex_release(cache_lock);
        return RT_EOK;
    }

    rt_mutex_release(cache_lock);
    return -RT_ERROR;
}

int offline_cache_read(CacheRecord *record)
{
    if (ee_dev == RT_NULL || record == RT_NULL) return -RT_ERROR;

    rt_mutex_take(cache_lock, RT_WAITING_FOREVER);

    if (header.count == 0) {
        rt_mutex_release(cache_lock);
        return -RT_EEMPTY;
    }

    /* 计算读取地址 */
    uint32_t addr = CACHE_HEADER_SIZE + header.tail * CACHE_RECORD_SIZE;

    if (at24cxx_read(ee_dev, addr, (uint8_t *)record, sizeof(CacheRecord)) == RT_EOK) {
        rt_mutex_release(cache_lock);
        return RT_EOK;
    }

    rt_mutex_release(cache_lock);
    return -RT_ERROR;
}

int offline_cache_pop(void)
{
    if (ee_dev == RT_NULL) return -RT_ERROR;

    rt_mutex_take(cache_lock, RT_WAITING_FOREVER);

    if (header.count > 0) {
        header.tail = (header.tail + 1) % MAX_RECORDS;
        header.count--;
        save_header();
    }

    rt_mutex_release(cache_lock);
    return RT_EOK;
}

int offline_cache_is_empty(void)
{
    return (header.count == 0);
}

int offline_cache_get_count(void)
{
    return header.count;
}
