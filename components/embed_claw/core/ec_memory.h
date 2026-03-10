/**
 * @file ec_memory.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_MEMORY_H__
#define __EC_MEMORY_H__

/* ==================== [Includes] ========================================== */

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */


/**
 * @brief 读取长期记忆（MEMORY.md）到buf
 * 
 * @param buf 读取缓冲区
 * @param size 缓冲区大小
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_ERR_NOT_FOUND 文件不存在
 */
esp_err_t ec_memory_read_long_term(char *buf, size_t size);

/**
 * @brief 写入内容到长期记忆（MEMORY.md）
 * 
 * @param content 要写入的内容
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_FAIL 写入失败（如文件无法打开）
 */
esp_err_t ec_memory_write_long_term(const char *content);

/**
 * @brief 追加一条笔记到今天的每日记忆文件（YYYY-MM-DD.md）
 * 
 * @param note 要追加的笔记内容
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_FAIL 写入失败（如文件无法打开）
 */
esp_err_t ec_memory_append_today(const char *note);

/**
 * @brief 读取最近的N天的每日记忆（last N days）到buf
 * 
 * @param buf 读取缓冲区
 * @param size 缓冲区大小
 * @param days 要回溯的天数（默认3天）
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_FAIL 读取失败（如文件无法打开）
 */
esp_err_t ec_memory_read_recent(char *buf, size_t size, int days);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_MEMORY_H__
