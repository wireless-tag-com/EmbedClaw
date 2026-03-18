#include "core/ec_channel_rule.h"

#if EC_WS_ENABLE
EC_CHANNEL_REG(ws)
#endif

#if EC_FEISHU_ENABLE
EC_CHANNEL_REG(feishu)
#endif

#if EC_QQ_ENABLE
EC_CHANNEL_REG(qq)
#endif