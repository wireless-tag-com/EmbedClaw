/**
 * @file ec_session.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_SESSION_H__
#define __EC_SESSION_H__

/* ==================== [Includes] ========================================== */

#include "esp_err.h"
#include "cJSON.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 添加一条消息到会话历史，存储在SPIFFS中JSONL格式的文件里。
 * 
 * @param chat_id 会话id
 * @param role 角色（如"user"或"assistant"）
 * @param content 消息内容
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_FAIL 写入失败（如文件无法打开）
 */
esp_err_t ec_session_append(const char *chat_id, const char *role, const char *content);

/**
 * @brief 添加一条完整的 cJSON 消息对象到会话历史（保留 tool_calls、数组 content 等结构）。
 *
 * @param chat_id 会话id
 * @param msg cJSON 消息对象（不会被修改或释放）
 * @return esp_err_t
 *  - ESP_OK 成功
 *  - ESP_FAIL 写入失败
 */
esp_err_t ec_session_append_msg(const char *chat_id, const cJSON *msg);

/**
 * @brief 获取会话历史的JSON字符串，格式为：
 * [{"role":"user","content":"..."},{"role":"assistant","content":"..."},...]
 * 只返回最近的max_msgs条消息。
 * 
 * @param chat_id 会话id
 * @param buf 输出缓冲区（由调用者分配）
 * @param size 缓冲区大小
 * @param max_msgs 最大消息数
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_FAIL 读取失败（如文件无法打开）
 */
esp_err_t ec_session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs);

/**
 * @brief 清除会话历史（删除对应的文件）。
 * 
 * @param chat_id 会话id
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_ERR_NOT_FOUND 文件不存在
 */
esp_err_t ec_session_clear(const char *chat_id);

/**
 * @brief 列出所有会话文件（打印到日志）。
 */
void ec_session_list(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_SESSION_H__
