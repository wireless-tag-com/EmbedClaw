/**
 * @file embed_claw.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EMBED_CLAW_H__
#define __EMBED_CLAW_H__

/* ==================== [Includes] ========================================== */
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 启动embed_claw
 * 
 * @return esp_err_t 
 *  - ESP_OK 启动成功
 */
esp_err_t ec_embed_claw_start(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EMBED_CLAW_H__
