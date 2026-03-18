/**
 * @file ec_config_internal.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-12
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __EC_CONFIG_INTERNAL_H__
#define __EC_CONFIG_INTERNAL_H__

/* ==================== [Includes] ========================================== */

/* Project/test overrides are injected with compiler `-include` options. */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/*
 * [通用配置覆盖规则]
 * 如何配置:
 * - 在 main/ec_config.h 中 `#define` 同名宏即可覆盖默认值。
 * 作用:
 * - 本文件只提供默认值；项目参数和密钥建议放到 main/ec_config.h。
 */

/*
 * [storage/filesystem]
 * 如何配置:
 * - 按需覆盖下列路径宏，保持与 SPIFFS 挂载点一致。
 * 作用:
 * - 控制会话、记忆、人格、技能、cron 等文件的默认存储位置。
 */
#ifndef EC_FS_BASE
#define EC_FS_BASE                  "/spiffs"
#endif

#ifndef EC_FS_SESSION_DIR
#define EC_FS_SESSION_DIR           EC_FS_BASE "/session"
#endif

#ifndef EC_FS_MEMORY_DIR
#define EC_FS_MEMORY_DIR            EC_FS_BASE "/memory"
#endif

#ifndef EC_MEMORY_FILE
#define EC_MEMORY_FILE              EC_FS_MEMORY_DIR "/MEMORY.md"
#endif

#ifndef EC_SOUL_FILE
#define EC_SOUL_FILE                EC_FS_BASE "/config/SOUL.md"
#endif

#ifndef EC_USER_FILE
#define EC_USER_FILE                EC_FS_BASE "/config/USER.md"
#endif

#ifndef EC_CRON_FILE
#define EC_CRON_FILE                EC_FS_BASE "/cron.json"
#endif

#ifndef EC_SKILLS_PREFIX
#define EC_SKILLS_PREFIX            EC_FS_BASE "/skills/"
#endif

/*
 * [embed_claw/llm]
 * 如何配置:
 * - 覆盖 provider 名称、API 地址、密钥、模型和响应缓冲参数。
 * 作用:
 * - 影响 LLM provider 选择、接入目标、推理容量、内存占用和工具调用解析上限。
 */
#ifndef EC_LLM_API_URL
#define EC_LLM_API_URL              "https://dashscope-intl.aliyuncs.com/compatible-mode/v1/chat/completions"
#endif

#ifndef EC_LLM_API_KEY
#define EC_LLM_API_KEY              ""
#endif

#ifndef EC_LLM_MODEL
#define EC_LLM_MODEL                "qwen-plus"
#endif

#ifndef EC_LLM_PROVIDER_NAME
#define EC_LLM_PROVIDER_NAME        "openai"
#endif

#ifndef EC_LLM_MAX_TOKENS
#define EC_LLM_MAX_TOKENS           4096
#endif

#ifndef EC_LLM_STREAM_BUF_SIZE
#define EC_LLM_STREAM_BUF_SIZE      (32 * 1024)
#endif

#ifndef EC_LLM_MAX_TOOL_CALLS
#define EC_LLM_MAX_TOOL_CALLS           4
#endif

/*
 * [core/ec_agent]
 * 如何配置:
 * - 覆盖任务栈/优先级/核心绑定和上下文相关缓冲参数。
 * 作用:
 * - 影响主 Agent 任务调度、并发队列容量和单轮推理内存开销。
 */
#ifndef EC_AGENT_BUS_QUEUE_LEN
#define EC_AGENT_BUS_QUEUE_LEN            16
#endif

#ifndef EC_AGENT_STACK
#define EC_AGENT_STACK              (24 * 1024)
#endif

#ifndef EC_AGENT_PRIO
#define EC_AGENT_PRIO               6
#endif

#ifndef EC_AGENT_CORE
#define EC_AGENT_CORE               0
#endif

#ifndef EC_AGENT_CONTEXT_BUF_SIZE
#define EC_AGENT_CONTEXT_BUF_SIZE         (16 * 1024)
#endif

#ifndef EC_AGENT_MAX_TOOL_ITER
#define EC_AGENT_MAX_TOOL_ITER      10
#endif

#ifndef EC_AGENT_MAX_HISTORY
#define EC_AGENT_MAX_HISTORY        20
#endif

#ifndef EC_AGENT_SEND_WORKING_STATUS
#define EC_AGENT_SEND_WORKING_STATUS 1
#endif

/*
 * [core/ec_session]
 * 如何配置:
 * - 覆盖会话历史保留条数。
 * 作用:
 * - 控制每个会话向 LLM 回放的最大历史消息数。
 */
#ifndef EC_SESSION_MAX_MSGS
#define EC_SESSION_MAX_MSGS         20
#endif

/*
 * [tools/tools_web_search]
 * 如何配置:
 * - 配置 Tavily 密钥和搜索缓冲/结果上限。
 * 作用:
 * - 控制联网搜索能力、返回条数以及内存占用。
 */
#ifndef EC_SECRET_SEARCH_KEY
#define EC_SECRET_SEARCH_KEY        ""
#endif

#ifndef EC_SEARCH_BUF_SIZE
#define EC_SEARCH_BUF_SIZE          (16 * 1024)
#endif

#ifndef EC_SEARCH_RESULT_COUNT
#define EC_SEARCH_RESULT_COUNT      5
#endif

/*
 * [tools/tools_get_time]
 * 如何配置:
 * - 覆盖 NTP 服务器和时区字符串。
 * 作用:
 * - 控制时间同步来源和本地时间格式化结果。
 */
#ifndef EC_GET_TIME_NTP_SERVER
#define EC_GET_TIME_NTP_SERVER      "ntp.aliyun.com"
#endif

#ifndef EC_TIMEZONE
#define EC_TIMEZONE                 "UTC-8"
#endif

/*
 * [tools/tools_cron]
 * 如何配置:
 * - 覆盖任务数上限和 cron 轮询任务参数。
 * 作用:
 * - 控制定时任务容量与后台扫描频率/资源占用。
 */
#ifndef EC_CRON_MAX_JOBS
#define EC_CRON_MAX_JOBS            16
#endif

#ifndef EC_CRON_TASK_PRIORITY
#define EC_CRON_TASK_PRIORITY       4
#endif

#ifndef EC_CRON_TASK_STACK_SIZE
#define EC_CRON_TASK_STACK_SIZE     4096
#endif

#ifndef EC_CRON_CHECK_INTERVAL_MS
#define EC_CRON_CHECK_INTERVAL_MS   (60 * 1000)
#endif

/*
 * [channel/ec_channel_ws]
 * 如何配置:
 * - 覆盖 WebSocket 通道开关、端口、最大客户端数。
 * 作用:
 * - 控制 WS 通道是否编译注册，以及监听规模。
 */
#ifndef EC_WS_ENABLE
#define EC_WS_ENABLE                1
#endif

#ifndef EC_WS_PORT
#define EC_WS_PORT                  18789
#endif

#ifndef EC_WS_MAX_CLIENTS
#define EC_WS_MAX_CLIENTS           4
#endif

/*
 * [channel/ec_channel_feishu]
 * 如何配置:
 * - 覆盖飞书通道开关、凭证和 ping 周期。
 * 作用:
 * - 控制飞书通道是否启用、鉴权参数和保活间隔。
 */
#ifndef EC_FEISHU_ENABLE
#define EC_FEISHU_ENABLE            1
#endif

#ifndef EC_SECRET_FEISHU_APP_ID
#define EC_SECRET_FEISHU_APP_ID     ""
#endif

#ifndef EC_SECRET_FEISHU_APP_SECRET
#define EC_SECRET_FEISHU_APP_SECRET ""
#endif

#ifndef EC_FEISHU_PING_INTERVAL_S
#define EC_FEISHU_PING_INTERVAL_S   120
#endif

/*
 * [channel/ec_channel_qq]
 * 如何配置:
 * - 覆盖 QQ 通道开关、凭证、重连间隔、订阅 intents。
 * 作用:
 * - 控制 QQ 通道是否启用，以及连接稳定性和事件订阅范围。
 */
#ifndef EC_QQ_ENABLE
#define EC_QQ_ENABLE                0
#endif

#ifndef EC_QQ_APP_ID
#define EC_QQ_APP_ID                ""
#endif

#ifndef EC_QQ_CLIENT_SECRET
#define EC_QQ_CLIENT_SECRET         ""
#endif

#ifndef EC_QQ_RECONNECT_MS
#define EC_QQ_RECONNECT_MS          10000
#endif

#ifndef EC_QQ_INTENTS
#define EC_QQ_INTENTS               (1 << 25)
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __EC_CONFIG_INTERNAL_H__
