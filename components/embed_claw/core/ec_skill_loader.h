/**
 * @file ec_skill_loader.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_SKILL_LOADER_H__
#define __EC_SKILL_LOADER_H__

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
 * @brief 初始化技能系统，安装内置技能文件到SPIFFS（如果尚未存在）
 * 
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_FAIL 安装失败（如SPIFFS错误）
 */
esp_err_t ec_skill_loader_init(void);

/**
 * @brief 构建所有可用技能的摘要信息，用于系统提示词。
 * 
 * @param buf   输出缓冲区
 * @param size  缓冲区大小
 * @return size_t 写入的字节数（如果未找到技能则返回0）
 */
size_t ec_skill_loader_build_summary(char *buf, size_t size);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_SKILL_LOADER_H__
