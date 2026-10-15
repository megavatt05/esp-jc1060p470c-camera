/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Модуль OTA-обновления сопроцессора ESP32-C6 по шине SDIO через esp-hosted.
 */

#include "c6_update.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "eh_host_sys.h"
#include "eh_host_cp_ota.h"
#include "eh_host_port_power.h"

static const char *TAG = "c6_update";

extern const uint8_t c6_fw_bin_start[] asm("_binary_c6_fw_bin_start");
extern const uint8_t c6_fw_bin_end[]   asm("_binary_c6_fw_bin_end");

esp_err_t c6_update_check_and_flash(void)
{
    eh_host_coprocessor_fwver_t slave_ver = {0};
    esp_err_t err = eh_host_sys_get_cp_fw_version(&slave_ver);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Не удалось запросить версию C6: %s (возможно, C6 еще не готов)",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Версия прошивки сопроцессора ESP32-C6: %lu.%lu.%lu",
             (unsigned long)slave_ver.major1,
             (unsigned long)slave_ver.minor1,
             (unsigned long)slave_ver.patch1);

    if (slave_ver.major1 >= 3) {
        ESP_LOGI(TAG, "Coprocessor уже на версии 3.x, обновление не требуется");
        return ESP_OK;
    }

    const size_t fw_size = (size_t)(c6_fw_bin_end - c6_fw_bin_start);
    if (fw_size == 0) {
        ESP_LOGE(TAG, "Встроенный бинарник прошивки C6 пуст");
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGW(TAG, "***********************************************************");
    ESP_LOGW(TAG, "Обнаружена устаревшая версия C6 (%lu.%lu.%lu).",
             (unsigned long)slave_ver.major1,
             (unsigned long)slave_ver.minor1,
             (unsigned long)slave_ver.patch1);
    ESP_LOGW(TAG, "Запуск OTA-обновления ESP32-C6 по SDIO (размер: %zu байт)...", fw_size);
    ESP_LOGW(TAG, "НЕ ВЫКЛЮЧАЙТЕ ПИТАНИЕ УСТРОЙСТВА!");
    ESP_LOGW(TAG, "***********************************************************");

    err = eh_host_cp_ota_begin();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eh_host_cp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    const size_t chunk_size = 1024;
    size_t offset = 0;
    while (offset < fw_size) {
        size_t len = (fw_size - offset > chunk_size) ? chunk_size : (fw_size - offset);
        err = eh_host_cp_ota_write(c6_fw_bin_start + offset, (uint32_t)len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "eh_host_cp_ota_write ошибка на смещении %zu: %s",
                     offset, esp_err_to_name(err));
            eh_host_cp_ota_end();
            return err;
        }
        offset += len;
        if (offset % (64 * 1024) < chunk_size || offset == fw_size) {
            ESP_LOGI(TAG, "Прогресс OTA C6: %zu / %zu (%.1f%%)",
                     offset, fw_size, (float)offset * 100.0f / (float)fw_size);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    err = eh_host_cp_ota_end();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eh_host_cp_ota_end failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Активация новой прошивки C6...");
    eh_host_cp_ota_activate();

    ESP_LOGW(TAG, "OTA завершено успешно! Сброс C6 и перезагрузка хоста через 2 секунды...");
    eh_host_port_reset_slave();
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}
