/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Модуль OTA-обновления сопроцессора ESP32-C6 по шине SDIO через esp-hosted.
 */

#ifndef C6_UPDATE_H
#define C6_UPDATE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Проверяет версию сопроцессора ESP32-C6 и при необходимости выполняет
 * автоматическое OTA-обновление до версии 3.0.9 по шине SDIO.
 *
 * @return ESP_OK если версия совпадает или обновление успешно завершено;
 *         код ошибки при сбое.
 */
esp_err_t c6_update_check_and_flash(void);

#ifdef __cplusplus
}
#endif

#endif /* C6_UPDATE_H */
