#ifdef EC_CHANNEL_REG
#undef EC_CHANNEL_REG
#endif

/* 注册变成枚举，用于获取最大注册数目 */
#ifdef EC_CHANNEL_ENMU
#define EC_CHANNEL_REG(name) _EC_CHANNEL_ENMU_##name,
#endif

/* 注册变成函数声明，用于声明注册的函数 */
#ifdef EC_CHANNEL_REG_EXTERN
#define EC_CHANNEL_REG(name) extern esp_err_t ec_channel_##name(void);
#endif

/* 注册变成函数调用，用于调用注册的函数 */
#ifdef EC_CHANNEL_REG_FUNC
#define EC_CHANNEL_REG(name) do { \
    esp_err_t err = ec_channel_##name(); \
    if (err != ESP_OK) { \
        ESP_LOGE(TAG, "Register tool " #name " failed: %s", esp_err_to_name(err)); \
        return err; \
    } else { \
        ESP_LOGI(TAG, "Registered tool: " #name); \
    } \
} while (0);

#endif // __EC_CHANNEL_RULE_H__

#undef EC_CHANNEL_ENMU
#undef EC_CHANNEL_REG_EXTERN
#undef EC_CHANNEL_REG_FUNC

