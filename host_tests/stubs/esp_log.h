#pragma once
// Host stub — logging no-ops for unit tests.
#include <cstdarg>

typedef int esp_log_level_t;
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3
#define ESP_LOG_DEBUG 4
#define ESP_LOG_VERBOSE 5

inline void esp_log_writev(esp_log_level_t, const char*, const char*, va_list) {}
