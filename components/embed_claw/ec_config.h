/**
 * @file ec_config.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-02
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __EC_CONFIG_H__
#define __EC_CONFIG_H__

/* ==================== [Includes] ========================================== */

/* ==================== [Defines] =========================================== */

#define EC_FS_BASE                  "/spiffs"
#define EC_FS_CONFIG_DIR            EC_FS_BASE "/config"
#define EC_FS_SESSION_DIR           EC_FS_BASE "/session"
#define EC_FS_MEMORY_DIR            EC_FS_BASE "/memory"
#define EC_MEMORY_FILE              EC_FS_MEMORY_DIR "/MEMORY.md"
#define EC_SOUL_FILE                EC_FS_CONFIG_DIR "/SOUL.md"
#define EC_USER_FILE                EC_FS_CONFIG_DIR "/USER.md"
#define EC_SESSION_MAX_MSGS         20

#define EC_WS_PORT                  18789
#define EC_WS_MAX_CLIENTS           4

#define EC_MAX_CRON_JOBS            16

#define EC_CRON_TASK_PRIORITY       4
#define EC_CRON_TASK_STACK_SIZE     4096
#define EC_CRON_CHECK_INTERVAL_MS   (60 * 1000)

#define EC_GET_TIME_NTP_SERVER      "ntp.aliyun.com"
#define EC_TIMEZONE                 "UTC-8"

#define EC_SEARCH_BUF_SIZE          (16 * 1024)
#define EC_SEARCH_RESULT_COUNT      5
#define EC_QUERY_UTF8_MAX           256
#define EC_BUS_QUEUE_LEN            16

#define EC_SECRET_SEARCH_KEY        ""

#define EC_CRON_FILE                EC_FS_BASE "/cron.json"

#define EC_SKILLS_PREFIX            EC_FS_BASE "/skills/"

#define EC_LLM_API_URL              "https://dashscope-intl.aliyuncs.com/compatible-mode/v1/chat/completions"
#define EC_LLM_API_KEY              ""
#define EC_LLM_MODEL                "qwen-plus"
#define EC_LLM_MAX_TOKENS           4096
#define EC_LLM_STREAM_BUF_SIZE      (32 * 1024)
#define EC_MAX_TOOL_CALLS           4

#define EC_AGENT_STACK              (24 * 1024)
#define EC_AGENT_PRIO               6
#define EC_AGENT_CORE               1

#define EC_CONTEXT_BUF_SIZE         (16 * 1024)
#define EC_AGENT_MAX_TOOL_ITER      10
#define EC_AGENT_MAX_HISTORY        20
#define EC_AGENT_SEND_WORKING_STATUS 1

#define EC_SECRET_FEISHU_APP_ID     ""
#define EC_SECRET_FEISHU_APP_SECRET ""
#define EC_FEISHU_WS_URL_MAX        256
#define EC_FEISHU_PING_INTERVAL_S   120

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */


#endif // __EC_CONFIG_H__
