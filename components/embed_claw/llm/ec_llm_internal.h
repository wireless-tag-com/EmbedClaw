/**
 * @file ec_llm_internal.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-05
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_LLM_INTERNAL_H__
#define __EC_LLM_INTERNAL_H__

/* ==================== [Includes] ========================================== */
#include "ec_llm.h"


#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct _ec_llm_provider_t ec_llm_provider_t;

typedef struct _ec_llm_provider_vtable_t{
    esp_err_t (*init)(ec_llm_provider_t *self, const ec_llm_provider_ctx_t* provider_ctx);
    esp_err_t (*chat_tools)(ec_llm_provider_t *self,
                            const char *system_prompt,
                            cJSON *messages,
                            const char *tools_json,
                            ec_llm_response_t *resp);
} ec_llm_provider_vtable_t;

typedef struct _ec_llm_provider_t{
    const char *name;
    ec_llm_provider_ctx_t instance;
    const ec_llm_provider_vtable_t *vtable;
} ec_llm_provider_t;

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_LLM_INTERNAL_H__
