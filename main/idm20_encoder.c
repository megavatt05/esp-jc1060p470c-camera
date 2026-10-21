#include "idm20_encoder.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "vfd_easydrive.h"

#include "nvs.h"

static const char *TAG = "IDM20_ENCODER";
#define NVS_NAMESPACE "winder_cfg"

// Глобальное состояние станка намотки (по умолчанию колесо 102 мм, 2 датчика, Z=8 зубьев)
winder_context_t g_winder = {
    .target_length_m     = 100.0f,
    .slowdown_dist_m     = 2.50f,
    .coast_comp_m        = 0.10f,
    .roller_diam_mm      = 102.0f,  // Диаметр мерного колеса 102.0 мм (L_об = pi * 102 = 320.442 мм)
    .cable_diam_mm       = 6.0f,    // Диаметр кабеля по умолчанию
    .bend_factor         = 0.025f,  // Коэффициент огибания
    .teeth_z             = 8,       // Число зубьев/меток диска (как в паспорте ИД-2)
    .encoder_ppr_x4      = 32,      // 2 датчика дают учетверение: 4 * 8 = 32 инкремента/об
    .cal_coeff           = 1.00000f,// Номинальный поправочный коэффициент
    .ref_length_m        = 10.0f,   // Эталонный отрезок для калибровки по умолчанию (10.00 м)
    .total_production_m  = 0.0,     // Суммарный счетчик выработки за смену/всего
    .spool_initial_len_m = 2000.0f, // Начальный метраж бухты по умолчанию (до 2000 м)
    .spool_remaining_len_m = 2000.0f,// Текущий остаток в бухте (при старте = всей бухте)
    .const_line_speed_en = true,
    .target_speed_m_min  = 40.0f,
    .base_freq_hz        = 35.0f,
    .creep_freq_hz       = 6.0f,
    .max_current_jam_a   = 6.5f,
    .total_pulses        = 0,
    .current_length_m    = 0.0f,
    .line_speed_m_min    = 0.0f,
    .state               = STATE_IDLE
};

static pcnt_unit_handle_t s_pcnt_unit = NULL;
static pcnt_channel_handle_t s_pcnt_chan_a = NULL;
static pcnt_channel_handle_t s_pcnt_chan_b = NULL;

/* =========================================================================
 * 1. МАТЕМАТИКА СЧЕТЧИКА ИД-2 И КАЛИБРОВКА ПО ЭТАЛОНУ
 * ========================================================================= */
float idm20_calculate_length_m(int64_t pulses, const winder_context_t *ctx)
{
    // Эффективный диаметр с учетом толщины кабеля: Diam_eff = D_колеса + k_огиб * d_кабеля
    float eff_diam_m = (ctx->roller_diam_mm + (ctx->bend_factor * ctx->cable_diam_mm)) / 1000.0f;
    float eff_circumference_m = (float)M_PI * eff_diam_m;

    // Число инкрементов на оборот (для 2 датчиков: 4 * Z)
    uint32_t ppr = ctx->encoder_ppr_x4;
    if (ppr == 0) {
        ppr = (ctx->teeth_z > 0) ? (ctx->teeth_z * 4) : 32;
    }

    // Длина дуги окружности на 1 инкремент (паспорт ИД-2 разд. 5.3):
    // Длина дуги = (pi * Diam) / (4 * Z)
    float step_per_inc_m = (eff_circumference_m / (float)ppr) * ctx->cal_coeff;

    return (float)pulses * step_per_inc_m;
}

bool idm20_calibrate_by_reference(float real_measured_meters)
{
    if (g_winder.current_length_m > 0.05f && real_measured_meters > 0.05f) {
        // Формула ИД-2 п. 5.3.4: K_нов = K_тек * (L_эталон / L_счетчик)
        float ratio = real_measured_meters / g_winder.current_length_m;
        g_winder.cal_coeff *= ratio;

        // Защитный диапазон калибровки (0.50000 .. 1.50000)
        if (g_winder.cal_coeff < 0.50000f) g_winder.cal_coeff = 0.50000f;
        if (g_winder.cal_coeff > 1.50000f) g_winder.cal_coeff = 1.50000f;

        // Пересчитываем текущую измеренную длину
        g_winder.current_length_m = idm20_calculate_length_m(g_winder.total_pulses, &g_winder);

        ESP_LOGI(TAG, "УСПЕШНАЯ КАЛИБРОВКА ПО ЭТАЛОНУ: L_эт=%.2f м, K_нов=%.5f, L_факт=%.2f м",
                 real_measured_meters, g_winder.cal_coeff, g_winder.current_length_m);

        idm20_save_settings_nvs();
        return true;
    }

    ESP_LOGW(TAG, "Отказ калибровки: длина отрезка слишком мала (<0.05 м)");
    return false;
}

void idm20_reset_calibration(void)
{
    g_winder.cal_coeff = 1.00000f;
    g_winder.current_length_m = idm20_calculate_length_m(g_winder.total_pulses, &g_winder);
    idm20_save_settings_nvs();
    ESP_LOGI(TAG, "Калибровочный коэффициент K_кал сброшен в номинал 1.00000");
}

void idm20_reset_length(void)
{
    if (g_winder.state == STATE_IDLE || g_winder.state == STATE_DONE) {
        if (s_pcnt_unit) {
            pcnt_unit_clear_count(s_pcnt_unit);
        }
        // Накопление в суммарную выработку
        g_winder.total_production_m += g_winder.current_length_m;
        g_winder.total_pulses = 0;
        g_winder.current_length_m = 0.0f;
        g_winder.line_speed_m_min = 0.0f;
        g_winder.state = STATE_IDLE;
        idm20_save_settings_nvs();
        ESP_LOGI(TAG, "Счетчик партии сброшен в 0.00 м (Всего выпущено: %.2f м)", g_winder.total_production_m);
    }
}

void idm20_reset_production(void)
{
    g_winder.total_production_m = 0.0;
    idm20_save_settings_nvs();
    ESP_LOGI(TAG, "Суммарный счетчик выработки сброшен в 0.00 м");
}

/* =========================================================================
 * 2. СОХРАНЕНИЕ И ЗАГРУЗКА ПАРАМЕТРОВ В ЭНЕРГОНЕЗАВИСИМУЮ ПАМЯТЬ NVS
 * ========================================================================= */
esp_err_t idm20_save_settings_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    uint32_t k_cal_int = (uint32_t)(g_winder.cal_coeff * 100000.0f);
    uint32_t cable_d_int = (uint32_t)(g_winder.cable_diam_mm * 100.0f);
    uint32_t wheel_d_int = (uint32_t)(g_winder.roller_diam_mm * 100.0f);
    uint32_t ref_len_int = (uint32_t)(g_winder.ref_length_m * 100.0f);
    uint64_t prod_mm = (uint64_t)(g_winder.total_production_m * 1000.0);

    nvs_set_u32(h, "k_cal", k_cal_int);
    nvs_set_u32(h, "cable_d", cable_d_int);
    nvs_set_u32(h, "wheel_d", wheel_d_int);
    nvs_set_u16(h, "teeth_z", g_winder.teeth_z);
    nvs_set_u32(h, "ref_len", ref_len_int);
    nvs_set_u64(h, "tot_prod", prod_mm);
    nvs_set_u32(h, "spool_len", (uint32_t)(g_winder.spool_initial_len_m * 100.0f));

    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t idm20_load_settings_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    uint32_t k_cal_int = 0;
    if (nvs_get_u32(h, "k_cal", &k_cal_int) == ESP_OK && k_cal_int > 0) {
        g_winder.cal_coeff = (float)k_cal_int / 100000.0f;
    }

    uint32_t cable_d_int = 0;
    if (nvs_get_u32(h, "cable_d", &cable_d_int) == ESP_OK && cable_d_int > 0) {
        g_winder.cable_diam_mm = (float)cable_d_int / 100.0f;
    }

    uint32_t wheel_d_int = 0;
    if (nvs_get_u32(h, "wheel_d", &wheel_d_int) == ESP_OK && wheel_d_int > 0) {
        g_winder.roller_diam_mm = (float)wheel_d_int / 100.0f;
    }

    uint16_t teeth_z = 0;
    if (nvs_get_u16(h, "teeth_z", &teeth_z) == ESP_OK && teeth_z > 0) {
        g_winder.teeth_z = teeth_z;
        g_winder.encoder_ppr_x4 = teeth_z * 4;
    }

    uint32_t ref_len_int = 0;
    if (nvs_get_u32(h, "ref_len", &ref_len_int) == ESP_OK && ref_len_int > 0) {
        g_winder.ref_length_m = (float)ref_len_int / 100.0f;
    }

    uint64_t prod_mm = 0;
    if (nvs_get_u64(h, "tot_prod", &prod_mm) == ESP_OK) {
        g_winder.total_production_m = (double)prod_mm / 1000.0;
    }

    uint32_t spool_len_int = 0;
    if (nvs_get_u32(h, "spool_len", &spool_len_int) == ESP_OK && spool_len_int > 0) {
        g_winder.spool_initial_len_m = (float)spool_len_int / 100.0f;
    }
    if (g_winder.spool_initial_len_m > 2000.0f) g_winder.spool_initial_len_m = 2000.0f;
    g_winder.spool_remaining_len_m = g_winder.spool_initial_len_m;

    nvs_close(h);
    ESP_LOGI(TAG, "Параметры из NVS загружены: D=%.1f мм, Z=%u, K_кал=%.5f, Выработка=%.2f м, Бухта=%.2f м",
             g_winder.roller_diam_mm, g_winder.teeth_z, g_winder.cal_coeff, g_winder.total_production_m, g_winder.spool_initial_len_m);
    return ESP_OK;
}

/* =========================================================================
 * 2. ФОНОВАЯ ЗАДАЧА ОПРОСА PCNT И АВТОМАТИКИ СТАНКА
 * ========================================================================= */
static void idm20_task(void *pvParameters)
{
    int64_t last_time_us = esp_timer_get_time();
    int64_t last_pulses = 0;
    int64_t last_motion_us = esp_timer_get_time();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50)); // Период опроса 50 мс (20 Гц)

        int pulse_delta = 0;
        if (s_pcnt_unit) {
            pcnt_unit_get_count(s_pcnt_unit, &pulse_delta);
            pcnt_unit_clear_count(s_pcnt_unit);
        }

        // Накопление общего числа импульсов
        g_winder.total_pulses += pulse_delta;
        g_winder.current_length_m = idm20_calculate_length_m(g_winder.total_pulses, &g_winder);

        // Расчет остатка кабеля в подающей бобине
        if (g_winder.spool_initial_len_m > 0.0f) {
            float rem = g_winder.spool_initial_len_m - g_winder.current_length_m;
            g_winder.spool_remaining_len_m = (rem > 0.0f) ? rem : 0.0f;
        } else {
            g_winder.spool_remaining_len_m = 0.0f;
        }

        int64_t now_us = esp_timer_get_time();
        float dt_sec = (float)(now_us - last_time_us) / 1000000.0f;

        if (dt_sec >= 0.04f) {
            // Расчет приращения длины и линейной скорости
            int64_t d_pulses = g_winder.total_pulses - last_pulses;
            float d_len_m = idm20_calculate_length_m(d_pulses, &g_winder);
            float inst_speed_m_min = (d_len_m / dt_sec) * 60.0f;

            // Фильтр низких частот (экспоненциальное сглаживание)
            g_winder.line_speed_m_min = 0.7f * g_winder.line_speed_m_min + 0.3f * inst_speed_m_min;
            if (fabsf(g_winder.line_speed_m_min) < 0.05f) {
                g_winder.line_speed_m_min = 0.0f;
            }

            if (d_pulses != 0) {
                last_motion_us = now_us;
            }

            last_time_us = now_us;
            last_pulses = g_winder.total_pulses;
        }

        // =====================================================================
        // ЛОГИКА АВТОМАТА НАМОТКИ И ЗАЩИТ
        // =====================================================================
        float remaining_m = g_winder.target_length_m - g_winder.current_length_m;

        switch (g_winder.state) {
        case STATE_RUNNING:
            // 1. Проверка дотяжки перед остановкой
            if (remaining_m <= g_winder.slowdown_dist_m) {
                ESP_LOGI(TAG, "Вход в зону дотяжки (осталось %.2f м). Снижение частоты до %.1f Гц",
                         remaining_m, g_winder.creep_freq_hz);
                g_winder.state = STATE_SLOWDOWN;
                vfd_cmd_set_frequency(g_winder.creep_freq_hz);
            }

            // 2. Защита от обрыва кабеля / окончания бухты (нет импульсов > 1.5 сек)
            if ((now_us - last_motion_us) > 1500000) {
                ESP_LOGW(TAG, "АВАРИЯ: Обрыв кабеля или окончание отдающей бухты!");
                g_winder.state = STATE_ALARM_BREAK;
                vfd_cmd_stop();
            }
            break;

        case STATE_SLOWDOWN:
            // Остановка с учетом упреждения выбега
            if (remaining_m <= g_winder.coast_comp_m) {
                ESP_LOGI(TAG, "Намотка завершена! Итоговая длина: %.2f м (уставка %.2f м)",
                         g_winder.current_length_m, g_winder.target_length_m);
                g_winder.state = STATE_DONE;
                vfd_cmd_stop();
            }
            break;

        default:
            last_motion_us = now_us;
            break;
        }
    }
}

/* =========================================================================
 * 3. ИНИЦИАЛИЗАЦИЯ ДРАЙВЕРА PULSE COUNT (PCNT)
 * ========================================================================= */
esp_err_t idm20_encoder_init(void)
{
    // Загрузка сохраненных параметров калибровки из Flash NVS
    idm20_load_settings_nvs();

#if !defined(CONFIG_STANOK_USE_IDM20) || !CONFIG_STANOK_USE_IDM20
    /* Датчик не подключён: PCNT не создаём, пины A/B остаются свободными,
     * длина стоит на 0.00 м, автомат останова по длине не срабатывает.
     * Панель и связь с частотником при этом полностью рабочие. */
    ESP_LOGW(TAG, "ИДМ-20 ОТКЛЮЧЁН (CONFIG_STANOK_USE_IDM20=n): "
                  " длина не измеряется, останов по уставке не сработает");
    return ESP_OK;
#endif

    ESP_LOGI(TAG, "Инициализация квадратурного энкодера (A=GPIO%d, B=GPIO%d, D=%.1fмм, Z=%u)...",
             IDM20_ENC_A_GPIO, IDM20_ENC_B_GPIO, g_winder.roller_diam_mm, g_winder.teeth_z);

    pcnt_unit_config_t unit_config = {
        .low_limit = -32000,
        .high_limit = 32000,
        .flags.accum_count = true,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &s_pcnt_unit));

    // Аппаратный фильтр от помех (дребезг до 1000 нс)
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config));

    // Канал A: такт по фазе A, направление по фазе B
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = IDM20_ENC_A_GPIO,
        .level_gpio_num = IDM20_ENC_B_GPIO,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_a_config, &s_pcnt_chan_a));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(s_pcnt_chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(s_pcnt_chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // Канал B: такт по фазе B, направление по фазе A
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = IDM20_ENC_B_GPIO,
        .level_gpio_num = IDM20_ENC_A_GPIO,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_b_config, &s_pcnt_chan_b));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(s_pcnt_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(s_pcnt_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt_unit));

    // Запуск FreeRTOS-задачи обработки энкодера
    xTaskCreatePinnedToCore(idm20_task, "idm20_task", 4096, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Энкодер ИДМ-20 успешно запущен в режиме x4!");
    return ESP_OK;
}
