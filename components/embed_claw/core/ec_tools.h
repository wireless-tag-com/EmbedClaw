/**
 * @file ec_tools.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __EC_TOOLS_H__
#define __EC_TOOLS_H__

/* ==================== [Includes] ========================================== */

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct _ec_tools_t {
    const char *name;
    const char *description;
    const char *input_schema_json;  /* JSON Schema string for input */
    esp_err_t (*execute)(const char *input_json, char *output, size_t output_size);
} ec_tools_t;

typedef enum _ec_tools_enmu_t {
    _EC_TOOLS_ENMU_NONE = -1,
#define EC_TOOLS_ENMU
#include "tools/ec_tools_reg.inc"
    _EC_TOOLS_ENMU_MAX
} ec_tools_enmu_t;


/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 注册所有工具
 *
 * @return esp_err_t
 *  - ESP_OK 成功
 *  - ESP_FAIL 注册失败
 */
esp_err_t ec_tools_register_all(void);

/**
 * @brief 注册单个工具
 *
 * @param tool 注册工具的结构体指针，
 *  @attention tool 必须在全局或静态内存中，不能是栈上变量，否则可能会被覆盖导致执行时崩溃
 * @return esp_err_t
 *  - ESP_OK 成功
 *  - ESP_ERR_NO_MEM 内存不足
 */
esp_err_t ec_tools_register(const ec_tools_t *tool);

/**
 * @brief 执行指定名称的工具
 *
 * @param name 工具名称
 * @param input_json 输入的JSON字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return esp_err_t
 * - ESP_OK 成功，output包含工具执行结果
 * - ESP_ERR_NOT_FOUND 未找到工具，output包含错误信息
 */
esp_err_t ec_tools_execute(const char *name, const char *input_json, char *output, size_t output_size);

/**
 * @brief 获取注册工具的JSON表示
 *
 * @return const char* json字符串，包含所有注册工具的名称、描述和输入参数说明
 */
const char *ec_tools_get_json(void);

/**
 * @brief 释放json中申请的字符串内存
 * 
 */
void ec_tools_free_json(void);


/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_TOOLS_H__
