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

/**
 * @brief 校验指定 channel 的 chat_id 是否符合格式要求
 *
 * 所有进入系统的消息都应带有非空 chat_id。对于 Feishu、QQ 这类
 * 对 chat_id 格式有要求的 channel，会进一步按各自规则校验。
 * 对于 websocket、system 等仅要求非空 chat_id 的来源，非空即视为合法。
 *
 * @param channel 指定 channel 名称
 * @param chat_id 待校验的 chat_id
 * @return true chat_id 合法
 * @return false chat_id 非法，或 channel/chat_id 为空
 */
bool ec_channel_validate_chat_id(const char *channel, const char *chat_id);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_CHANNEL_H__
