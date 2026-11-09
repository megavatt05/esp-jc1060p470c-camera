#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "test_endpoints.h"
#include "vfd_easydrive.h"
#include "idm20_encoder.h"

#ifdef CONFIG_STANOK_ENABLE_TEST_ENDPOINTS

static const char *TAG = "TEST_ENDPOINTS";

#if CONFIG_STANOK_TEST_SIMULATE_VFD
static bool s_sim_vfd = true;
#else
static bool s_sim_vfd = false;
#endif

#if CONFIG_STANOK_TEST_SIMULATE_ENCODER
static bool s_sim_encoder = true;
#else
static bool s_sim_encoder = false;
#endif

void test_endpoints_set_sim_vfd(bool enable)
{
    s_sim_vfd = enable;
    if (enable) {
        g_vfd.link = VFD_LINK_OK;
        g_vfd.link_stale = false;
    }
    ESP_LOGI(TAG, "Эмуляция частотника Easydrive: %s", enable ? "ВКЛЮЧЕНА" : "ВЫКЛЮЧЕНА");
}

bool test_endpoints_is_sim_vfd(void)
{
    return s_sim_vfd;
}

void test_endpoints_set_sim_encoder(bool enable)
{
    s_sim_encoder = enable;
    ESP_LOGI(TAG, "Эмуляция энкодера ИДМ-20: %s", enable ? "ВКЛЮЧЕНА" : "ВЫКЛЮЧЕНА");
}

bool test_endpoints_is_sim_encoder(void)
{
    return s_sim_encoder;
}

void test_endpoints_simulate_length(float meters)
{
    g_winder.current_length_m = meters;
    if (g_winder.cal_coeff > 0.000001f) {
        g_winder.total_pulses = (int64_t)(meters / g_winder.cal_coeff);
    }
    float rem = g_winder.spool_initial_len_m - meters;
    g_winder.spool_remaining_len_m = (rem > 0.0f) ? rem : 0.0f;
    ESP_LOGI(TAG, "Установлена тестовая длина: %.2f м (остаток бобины: %.2f м)", meters, g_winder.spool_remaining_len_m);
}

void test_endpoints_inject_fault(uint8_t fault_code)
{
    if (fault_code > 0) {
        g_vfd.is_fault = true;
        g_vfd.fault_code = fault_code;
        g_vfd.status_word |= VFD_ST_FAULT;
        ESP_LOGW(TAG, "Смоделирована авария привода Er%02u!", fault_code);
    } else {
        g_vfd.is_fault = false;
        g_vfd.fault_code = 0;
        g_vfd.status_word &= ~VFD_ST_FAULT;
        ESP_LOGI(TAG, "Сброс тестовой аварии привода -> НОРМА");
    }
}

void test_endpoints_execute_cmd(const char *cmd_name)
{
    if (!cmd_name) return;
    if (strcmp(cmd_name, "start") == 0 || strcmp(cmd_name, "пуск") == 0) {
        idm20_cmd_start();
        float f = g_vfd.set_freq_hz > 0.1f ? g_vfd.set_freq_hz : 25.0f;
        vfd_cmd_run_fwd(f);
        if (s_sim_vfd) {
            g_vfd.is_running = true;
            g_vfd.status_word |= VFD_ST_RUNNING;
        }
        ESP_LOGI(TAG, "Эндпоинт: Выполнена команда ПУСК (уставка %.2f Гц)", f);
    } else if (strcmp(cmd_name, "stop") == 0 || strcmp(cmd_name, "стоп") == 0) {
        idm20_cmd_stop();
        vfd_cmd_stop();
        if (s_sim_vfd) {
            g_vfd.is_running = false;
            g_vfd.status_word &= ~VFD_ST_RUNNING;
        }
        ESP_LOGI(TAG, "Эндпоинт: Выполнена команда СТОП");
    } else if (strcmp(cmd_name, "jog") == 0 || strcmp(cmd_name, "толчок") == 0) {
        vfd_cmd_jog(false, 5.0f);
        if (s_sim_vfd) {
            g_vfd.is_running = true;
            g_vfd.set_freq_hz = 5.0f;
        }
        ESP_LOGI(TAG, "Эндпоинт: Выполнена команда ТОЛЧОК (5.00 Гц)");
    } else if (strcmp(cmd_name, "rev") == 0 || strcmp(cmd_name, "реверс") == 0) {
        float f = g_vfd.set_freq_hz > 0.1f ? g_vfd.set_freq_hz : 25.0f;
        vfd_cmd_run_rev(f);
        if (s_sim_vfd) {
            g_vfd.is_running = true;
            g_vfd.status_word |= VFD_ST_RUNNING;
        }
        ESP_LOGI(TAG, "Эндпоинт: Выполнена команда РЕВЕРС (уставка %.2f Гц)", f);
    } else if (strcmp(cmd_name, "reset") == 0 || strcmp(cmd_name, "сброс") == 0) {
        vfd_cmd_reset_fault();
        test_endpoints_inject_fault(0);
        ESP_LOGI(TAG, "Эндпоинт: Выполнена команда СБРОС АВАРИИ");
    } else if (strcmp(cmd_name, "clear_len") == 0) {
        idm20_reset_length();
        ESP_LOGI(TAG, "Эндпоинт: Сброшен счетчик текущей длины в 0.00 м");
    } else {
        ESP_LOGW(TAG, "Неизвестная команда управления: '%s'", cmd_name);
    }
}

char *test_endpoints_get_status_json(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return buf;

    const char *link_str = (g_vfd.link == VFD_LINK_OK) ? "OK" :
                          ((g_vfd.link == VFD_LINK_DEGRADED) ? "DEGRADED" : "DEAD");
    const char *state_str = g_vfd.is_fault ? "FAULT" : (g_vfd.is_running ? "RUN" : "STOP");

    snprintf(buf, max_len,
             "{\"vfd\":{\"freq\":%.2f,\"set\":%.2f,\"cur\":%.2f,\"volt\":%.1f,\"rpm\":%ld,\"state\":\"%s\",\"fault\":%u,\"link\":\"%s\"},"
             "\"winder\":{\"len\":%.2f,\"target\":%.2f,\"rem\":%.2f,\"speed\":%.1f,\"prod\":%.1f,\"pulses\":%lld},"
             "\"sim\":{\"vfd\":%s,\"encoder\":%s}}",
             g_vfd.out_freq_hz, g_vfd.set_freq_hz, g_vfd.out_cur_a,
             g_vfd.out_volt_v, (long)g_vfd.rpm, state_str, g_vfd.fault_code, link_str,
             g_winder.current_length_m, g_winder.target_length_m,
             g_winder.spool_remaining_len_m, g_winder.line_speed_m_min,
             (float)g_winder.total_production_m, (long long)g_winder.total_pulses,
             s_sim_vfd ? "true" : "false", s_sim_encoder ? "true" : "false");

    return buf;
}

// ---------------------------------------------------------------- фоновая задача симуляции
static void test_simulation_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100)); // 10 раз в секунду

        // 1. Симуляция частотника Easydrive
        if (s_sim_vfd) {
            g_vfd.link = VFD_LINK_OK;
            g_vfd.link_stale = false;
            g_vfd.latency_ms = 15;
            g_vfd.polls_ok++;

            if (g_vfd.is_running && !g_vfd.is_fault) {
                // Плавный разгон к уставке
                float target_f = g_vfd.set_freq_hz > 0.1f ? g_vfd.set_freq_hz : 25.0f;
                if (g_vfd.out_freq_hz < target_f) {
                    g_vfd.out_freq_hz += 0.8f;
                    if (g_vfd.out_freq_hz > target_f) g_vfd.out_freq_hz = target_f;
                } else if (g_vfd.out_freq_hz > target_f) {
                    g_vfd.out_freq_hz -= 0.8f;
                    if (g_vfd.out_freq_hz < target_f) g_vfd.out_freq_hz = target_f;
                }

                g_vfd.out_cur_a = 0.35f + (g_vfd.out_freq_hz / 50.0f) * 1.65f;
                g_vfd.rpm = (int32_t)(g_vfd.out_freq_hz * 28.4f);
                g_vfd.out_volt_v = 220.0f * (g_vfd.out_freq_hz / 50.0f);
            } else {
                // Плавное торможение до нуля
                if (g_vfd.out_freq_hz > 0.0f) {
                    g_vfd.out_freq_hz -= 1.2f;
                    if (g_vfd.out_freq_hz < 0.0f) g_vfd.out_freq_hz = 0.0f;
                }
                g_vfd.out_cur_a = (g_vfd.out_freq_hz > 0.1f) ? 0.2f : 0.0f;
                g_vfd.rpm = (int32_t)(g_vfd.out_freq_hz * 28.4f);
                g_vfd.out_volt_v = (g_vfd.out_freq_hz > 0.1f) ? (220.0f * (g_vfd.out_freq_hz / 50.0f)) : 0.0f;
            }
        }

        // 2. Симуляция энкодера длины ИДМ-20
        if (s_sim_encoder && g_vfd.out_freq_hz > 0.1f && !g_vfd.is_fault) {
            // Линейная скорость при 50 Гц ~ 60 м/мин = 1 м/с = 0.1 м за 100 мс
            float speed_factor = g_vfd.out_freq_hz / 50.0f;
            float delta_len_m = 0.10f * speed_factor;

            g_winder.current_length_m += delta_len_m;
            g_winder.total_production_m += delta_len_m;
            g_winder.line_speed_m_min = speed_factor * 60.0f;

            // Расчет остатка бобины
            float rem = g_winder.spool_initial_len_m - g_winder.current_length_m;
            g_winder.spool_remaining_len_m = (rem > 0.0f) ? rem : 0.0f;

            // Проверка достижения уставки
            if (g_winder.target_length_m > 0.5f && g_winder.current_length_m >= g_winder.target_length_m) {
                ESP_LOGW(TAG, "!!! УСТАВКА НАМОТКИ ДОСТИГНУТА (%.2f м) -> АВТОМАТИЧЕСКИЙ ОСТАНОВ !!!",
                         g_winder.current_length_m);
                test_endpoints_execute_cmd("stop");
            }
        } else if (!g_vfd.is_running && s_sim_encoder) {
            g_winder.line_speed_m_min = 0.0f;
        }
    }
}

// ---------------------------------------------------------------- разбор команд в CLI
static void process_cli_command(char *line)
{
    // Удаляем концевые \r и \n
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) {
        line[--len] = '\0';
    }
    // Пропускаем пробелы в начале
    while (*line == ' ') line++;
    if (*line == '\0') return;

    if (strcmp(line, "help") == 0 || strcmp(line, "test:help") == 0 || strcmp(line, "?") == 0) {
        printf("\r\n=== ЭНДПОИНТЫ УПРАВЛЕНИЯ И ТЕСТИРОВАНИЯ СТАНКА (ESP32-P4) ===\r\n");
        printf("  test:status            - Получить полную телеметрию в формате JSON\r\n");
        printf("  test:cmd <start|stop|jog|rev|reset|clear_len> - Команда станку\r\n");
        printf("  test:set_target <м>    - Задать уставку намотки (например test:set_target 120.5)\r\n");
        printf("  test:set_len <м>       - Принудительно установить текущую длину (test:set_len 99.8)\r\n");
        printf("  test:set_spool <м>     - Задать длину бобины/бухты (test:set_spool 1000)\r\n");
        printf("  test:set_freq <Гц>     - Задать частоту привода (test:set_freq 35.0)\r\n");
        printf("  test:sim_vfd <0|1>     - Включить/выключить эмуляцию частотника Easydrive\r\n");
        printf("  test:sim_encoder <0|1> - Включить/выключить эмуляцию энкодера ИДМ-20\r\n");
        printf("  test:sim_fault <код>   - Симуляция аварии привода (0=норма, 14=ошибка 485, 1..19)\r\n");
        printf("=================================================================\r\n\r\n");
        return;
    }

    if (strcmp(line, "test:status") == 0 || strcmp(line, "status") == 0) {
        char json_buf[512];
        test_endpoints_get_status_json(json_buf, sizeof(json_buf));
        printf("\r\n%s\r\n", json_buf);
        return;
    }

    if (strncmp(line, "test:cmd ", 9) == 0) {
        test_endpoints_execute_cmd(line + 9);
        return;
    }

    if (strncmp(line, "test:set_target ", 16) == 0) {
        float val = (float)atof(line + 16);
        if (val > 0.0f) {
            g_winder.target_length_m = val;
            ESP_LOGI(TAG, "Уставка партии намотки -> %.2f м", val);
        }
        return;
    }

    if (strncmp(line, "test:set_len ", 13) == 0) {
        float val = (float)atof(line + 13);
        test_endpoints_simulate_length(val);
        return;
    }

    if (strncmp(line, "test:set_spool ", 15) == 0) {
        float val = (float)atof(line + 15);
        if (val >= 0.0f) {
            g_winder.spool_initial_len_m = val;
            float rem = val - g_winder.current_length_m;
            g_winder.spool_remaining_len_m = rem > 0.0f ? rem : 0.0f;
            idm20_save_settings_nvs();
            ESP_LOGI(TAG, "Бобина установлена -> %.2f м (остаток %.2f м)", val, g_winder.spool_remaining_len_m);
        }
        return;
    }

    if (strncmp(line, "test:set_freq ", 14) == 0) {
        float val = (float)atof(line + 14);
        if (val >= 0.0f && val <= 50.0f) {
            g_vfd.set_freq_hz = val;
            vfd_cmd_set_frequency(val);
            ESP_LOGI(TAG, "Задана частота привода -> %.2f Гц", val);
        }
        return;
    }

    if (strncmp(line, "test:sim_vfd ", 13) == 0) {
        int v = atoi(line + 13);
        test_endpoints_set_sim_vfd(v != 0);
        return;
    }

    if (strncmp(line, "test:sim_encoder ", 17) == 0) {
        int v = atoi(line + 17);
        test_endpoints_set_sim_encoder(v != 0);
        return;
    }

    if (strncmp(line, "test:sim_fault ", 15) == 0) {
        int v = atoi(line + 15);
        test_endpoints_inject_fault((uint8_t)v);
        return;
    }

    ESP_LOGW(TAG, "Неизвестная команда '%s'. Введите 'test:help' для справки.", line);
}

// ---------------------------------------------------------------- задача приема команд из консоли
static void test_console_task(void *arg)
{
    (void)arg;
    char rx_buf[128];
    int rx_pos = 0;

#if CONFIG_STANOK_TEST_CLI_PROMPT
    vTaskDelay(pdMS_TO_TICKS(1500)); // пауза, чтобы не смешиваться с логами инициализации
    printf("\r\n[ЭНДПОИНТЫ ТЕСТИРОВАНИЯ АКТИВНЫ] Введите 'test:help' или 'test:status'\r\n");
#endif

    while (1) {
        int c = fgetc(stdin);
        if (c == EOF || c < 0) {
            vTaskDelay(pdMS_TO_TICKS(25));
            continue;
        }

        if (c == '\r' || c == '\n') {
            if (rx_pos > 0) {
                rx_buf[rx_pos] = '\0';
                process_cli_command(rx_buf);
                rx_pos = 0;
            }
        } else if (c == '\b' || c == 0x7F) {
            if (rx_pos > 0) rx_pos--;
        } else {
            if (rx_pos < sizeof(rx_buf) - 1) {
                rx_buf[rx_pos++] = (char)c;
            }
        }
    }
}

esp_err_t test_endpoints_init(void)
{
    ESP_LOGI(TAG, "Инициализация эндпоинтов управления и тестирования параметров станка...");

    // Создаем задачу периодической симуляции (если включены симуляторы)
    BaseType_t res1 = xTaskCreate(test_simulation_task, "test_sim", 4096, NULL, 3, NULL);
    if (res1 != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу test_simulation_task");
        return ESP_FAIL;
    }

    // Создаем задачу консольного интерфейса управления
    BaseType_t res2 = xTaskCreate(test_console_task, "test_cli", 4096, NULL, 2, NULL);
    if (res2 != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу test_console_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Эндпоинты тестирования успешно запущены (симуляция VFD: %s, энкодер: %s)",
             s_sim_vfd ? "ВКЛ" : "ВЫКЛ", s_sim_encoder ? "ВКЛ" : "ВЫКЛ");
    return ESP_OK;
}

#endif // CONFIG_STANOK_ENABLE_TEST_ENDPOINTS
