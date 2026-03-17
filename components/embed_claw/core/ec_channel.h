/**
 * @file ec_channel.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_CHANNEL_H__
#define __EC_CHANNEL_H__

/* ==================== [Includes] ========================================== */

#include <stdbool.h>

#include "esp_err.h"
#include "ec_agent.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

#define EC_CHANNEL_NAME_EXTERN
#include "channel/ec_channel_reg.inc"

extern const char g_ec_channel_system[];

/* ==================== [Typedefs] ========================================== */

typedef struct _ec_channel_t ec_channel_t;

typedef struct _ec_channel_driver_vtable_t{
    esp_err_t (*start)(void);
    esp_err_t (*send)(const ec_msg_t *msg);
} ec_channel_driver_vtable_t;

typedef struct _ec_channel_t {
    const char *name;
    const ec_channel_driver_vtable_t vtable;
} ec_channel_t;

typedef enum _ec_channel_enmu_t {
    _EC_CHANNEL_ENMU_NONE = -1,
#define EC_CHANNEL_ENMU
#include "channel/ec_channel_reg.inc"
    _EC_CHANNEL_ENMU_MAX
} ec_channel_enmu_t;


/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 注册所有通道
 * 
 * @return esp_err_t 
 *  - ESP_OK 完成注册
 */
esp_err_t ec_channel_register_all(void);

/**
 * @brief 注册具体channel
 * 
 * @param driver channel 驱动
 * @return esp_err_t 
 *  - ESP_OK 完成注册
 */
esp_err_t ec_channel_register(const ec_channel_t *driver);

/**
 * @brief 启动指定 channel
 * 
 * @param channel 指定channel 名称
 * @return esp_err_t 
 *  - ESP_OK 启动成功
 */
esp_err_t ec_channel_start(void);

/**
 * @brief 发送发送信息给指定channel
 * 
 * @param msg 发送消息内容
 * @return esp_err_t 
 *  - ESP_OK 消息发送成功
 */
esp_err_t ec_channel_send(const ec_msg_t *msg);


/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_CHANNEL_H__
