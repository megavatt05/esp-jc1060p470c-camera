#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация Wi-Fi 6 (ESP32-C6 через SDIO) и запуск Web Dashboard
 * 
 * Создает точку доступа SoftAP "Bench-JC1060-WiFi6" (IP: 192.168.4.1)
 * и запускает HTTP веб-сервер управления стендом на порту 80.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Получить IP-адрес стенда (SoftAP: 192.168.4.1)
 */
const char* wifi_manager_get_ip(void);

/**
 * @brief Получить IP-адрес стенда в домашней сети (STA)
 */
const char* wifi_manager_get_sta_ip(void);

/**
 * @brief Статус подключения к домашней сети (STA)
 */
bool wifi_manager_is_sta_connected(void);

#ifdef __cplusplus
}
#endif
