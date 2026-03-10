/**
 * @file ec_llm_openai.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-05
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_LLM_OPENAI_H__
#define __EC_LLM_OPENAI_H__

/* ==================== [Includes] ========================================== */

#include "esp_err.h"
#include "ec_llm_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

ec_llm_provider_t  *ec_llm_openai_get_provider(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_LLM_OPENAI_H__
