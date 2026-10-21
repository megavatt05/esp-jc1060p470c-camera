#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#include "board_jc1060p470c.h"
#include "idm20_encoder.h"
#include "vfd_easydrive.h"
#include "eez_ui.h"

static const char *TAG = "PON_STANOK_APP";

void app_main(void)
{
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  СТАНОК НАМОТКИ ОПТИЧЕСКОГО КАБЕЛЯ PON (ИДМ-20 + ESP32-P4)");
    ESP_LOGI(TAG, "  Плата: GUITION JC1060P470C 7\" MIPI-DSI (1024x600, GT911)  ");
    ESP_LOGI(TAG, "  ESP-IDF: v6.1.0 | LVGL: 9.x | ЧП: Easydrive Mini-S        ");
    ESP_LOGI(TAG, "============================================================");

    // 1. Инициализация энергонезависимой памяти NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Инициализация оборудования платы JC1060P470C (LDO -> DSI -> JD9165 -> GT911 -> LVGL)
    lv_display_t *disp = board_jc1060p470c_init();
    if (!disp) {
        ESP_LOGE(TAG, "Критическая ошибка: инициализация дисплея не удалась!");
        return;
    }

    // 3. Инициализация драйвера квадратурного энкодера ИДМ-20 (PCNT)
    ESP_ERROR_CHECK(idm20_encoder_init());

    // 4. Инициализация драйвера частотника Easydrive Mini-S (RS-485 Modbus RTU)
    ESP_ERROR_CHECK(vfd_easydrive_init());

    // 5. Построение графического интерфейса под мьютексом LVGL
    if (lvgl_port_lock(0)) {
        eez_ui_init();
        lvgl_port_unlock();
        ESP_LOGI(TAG, "Панель EASYDRIVE (EEZ Studio) готова к приему команд пользователя!");
    } else {
        ESP_LOGE(TAG, "Не удалось захватить мьютекс LVGL для построения UI");
    }


    // 6. Главный цикл диагностики
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "ЧП: %.2f Гц (уставка %.2f) | ток %.2f А | %ld об/мин | статус 0x%04X | опросов %lu",
                 g_vfd.out_freq_hz, g_vfd.set_freq_hz, g_vfd.out_cur_a,
                 (long)g_vfd.rpm, g_vfd.status_word, (unsigned long)g_vfd.polls_ok);
    }
}
