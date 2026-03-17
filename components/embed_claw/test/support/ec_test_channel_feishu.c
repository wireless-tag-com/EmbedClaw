#include "ec_test_hooks.h"

#define ec_channel_feishu ec_channel_feishu__test_impl
#include "../../channel/ec_channel_feishu.c"
#undef ec_channel_feishu

bool ec_channel_feishu_parse_frame_for_test(const uint8_t *buf, size_t len,
                                            int32_t *method, int32_t *service,
                                            uint64_t *seq_id, uint64_t *log_id,
                                            char *type_val, size_t type_len,
                                            char *payload, size_t payload_size)
{
    feishu_frame_t frame;
    bool ok = parse_feishu_frame(buf, len, &frame);

    if (!ok) {
        return false;
    }

    if (method) {
        *method = frame.method;
    }
    if (service) {
        *service = frame.service;
    }
    if (seq_id) {
        *seq_id = frame.seq_id;
    }
    if (log_id) {
        *log_id = frame.log_id;
    }
    if (type_val && type_len > 0) {
        snprintf(type_val, type_len, "%s", frame.type_val);
    }
    if (payload && payload_size > 0) {
        size_t copy_len = frame.payload_len;
        if (copy_len >= payload_size) {
            copy_len = payload_size - 1;
        }
        if (frame.payload && copy_len > 0) {
            memcpy(payload, frame.payload, copy_len);
        }
        payload[copy_len] = '\0';
    }

    return true;
}

int ec_channel_feishu_encode_ping_for_test(uint8_t *out, size_t out_max, int32_t service_id)
{
    return encode_ping_frame(out, out_max, service_id);
}

int ec_channel_feishu_encode_response_for_test(uint8_t *out, size_t out_max,
                                               uint64_t seq_id, uint64_t log_id, int32_t service)
{
    feishu_frame_t frame = {0};
    frame.seq_id = seq_id;
    frame.log_id = log_id;
    frame.service = service;
    return encode_response_frame(out, out_max, &frame);
}
