/**
 * @file ec_agent.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-05
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_AGENT_H__
#define __EC_AGENT_H__

/* ==================== [Includes] ========================================== */

#include "esp_err.h"


#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct {
    char channel[16];       /* Channel name such as g_ec_channel_ws/g_ec_channel_qq */
    char chat_id[64];       /* Channel-specific id (feishu/qq target id, WS client id) */
    char *content;          /* Heap-allocated message text (caller must free) */
} ec_msg_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 启动Agent Loop任务，处理入站消息并生成回复发送到出站队列
 * 
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_ERR_NO_MEM 内存不足，无法创建队列或任务
 *  - ESP_FAIL 其他错误
 */
esp_err_t ec_agent_start(void);

/**
 * @brief 向Agent Loop的入站队列发送消息
 * 
 * @param msg 消息内容，bus将接管msg->content的内存管理
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_ERR_NO_MEM 入站队列已满，无法添加消息
 */
esp_err_t ec_agent_inbound(const ec_msg_t *msg);

/**
 * @brief 从Agent Loop的出站队列获取消息
 * 
 * @param msg 消息内容，caller负责msg->content的内存管理（调用后需要free）
 * @param timeout_ms 超时时间（毫秒）
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_ERR_TIMEOUT 超时，未获取到消息
 */
esp_err_t ec_agent_outbound(ec_msg_t *msg, uint32_t timeout_ms);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __AGENT_H__
