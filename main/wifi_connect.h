/**
 * @file wifi_connect.h
 * @brief C interface for WiFi connection control.
 */

#ifndef WIFI_CONNECT_H_
#define WIFI_CONNECT_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_connect_init(void);
void wifi_connect_start(void);

#ifdef __cplusplus
}
#endif

#endif  // WIFI_CONNECT_H_
