#include "ec_config_internal.h"

#ifdef EC_TOOLS_REG
#undef EC_TOOLS_REG
#endif

/* 注册变成枚举，用于获取最大注册数目 */
#ifdef EC_TOOLS_ENMU
#define EC_TOOLS_REG(name) EC_TOOLS_ENMU_##name,
#endif

/* 注册变成函数声明，用于声明注册的函数 */
#ifdef EC_TOOLS_REG_EXTERN
#define EC_TOOLS_REG(name) extern esp_err_t ec_tools_##name(void);
#endif

/* 注册变成函数调用，用于调用注册的函数 */
#ifdef EC_TOOLS_REG_FUNC
#define EC_TOOLS_REG(name) do { \
    esp_err_t err = ec_tools_##name(); \
    if (err != ESP_OK) { \
        ESP_LOGE(TAG, "Register tool " #name " failed: %s", esp_err_to_name(err)); \
        return err; \
    } else { \
        ESP_LOGI(TAG, "Registered tool: " #name); \
    } \
} while (0);

#endif

#undef EC_TOOLS_ENMU
#undef EC_TOOLS_REG_EXTERN
#undef EC_TOOLS_REG_FUNC
