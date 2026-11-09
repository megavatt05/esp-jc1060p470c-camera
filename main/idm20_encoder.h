#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// =========================================================================
// ПИНЫ КВАДРАТУРНОГО ЭНКОДЕРА ИДМ-20 НА ESP32-P4
// Гребёнка Expand IO (JP1, 2x13): фаза A = GPIO1 = физический пин 7,
// фаза B = GPIO2 = физический пин 9. Значения берутся из
// main/Kconfig.projbuild; по умолчанию датчик ВКЛЮЧЁН.
// Питание датчиков — отдельный блок 12..24 В, сигнал только через
// открытый коллектор с pull-up на 3V3 (см. README, раздел ИДМ-20).
// =========================================================================
#ifndef CONFIG_STANOK_IDM20_A_GPIO
#define CONFIG_STANOK_IDM20_A_GPIO 1
#endif
#ifndef CONFIG_STANOK_IDM20_B_GPIO
#define CONFIG_STANOK_IDM20_B_GPIO 2
#endif
#define IDM20_ENC_A_GPIO          ((gpio_num_t)CONFIG_STANOK_IDM20_A_GPIO)
#define IDM20_ENC_B_GPIO          ((gpio_num_t)CONFIG_STANOK_IDM20_B_GPIO)
#define IDM20_RUN_OUT_GPIO        GPIO_NUM_3

typedef enum {
    STATE_IDLE = 0,
    STATE_STARTING,     // Команда ПУСК отправлена, ожидание подтверждения Bit4 от ЧП
    STATE_RUNNING,      // ЧП подтвердил ход, идет намотка
    STATE_SLOWDOWN,     // Дотяжка на малой скорости перед целью
    STATE_STOPPING,     // Команда СТОП отправлена, выбег/останов
    STATE_JOG,          // Ручная заправка кабеля
    STATE_DONE,         // Намотка завершена
    STATE_ALARM_BREAK,  // Обрыв кабеля / кончилась отдающая бухта
    STATE_ALARM_JAM,    // Заклинивание кабеля (перегруз по току)
    STATE_ALARM_VFD     // Ошибка частотника
} winder_state_t;

typedef struct {
    // Настройки уставки
    float target_length_m;        // Заданная длина намотки, м (напр. 100.00 м)
    float slowdown_dist_m;        // За сколько метров снижать скорость (напр. 2.50 м)
    float coast_comp_m;           // Упреждение остановки на выбег (напр. 0.12 м)

    // Параметры колеса и датчиков (паспорт СМОЛ ИД-2 / ИДМ)
    float roller_diam_mm;         // Диаметр мерного колеса (102.0 мм)
    float cable_diam_mm;          // Диаметр кабеля (напр. 3.0 .. 6.0 мм)
    float bend_factor;            // Коэффициент огибания ролика (0.025 по умолчанию)
    uint16_t teeth_z;             // Число зубьев/меток диска (8 по умолчанию)
    uint32_t encoder_ppr_x4;      // Число инкрементов на оборот (4 * Z = 32 при 2 датчиках)
    float cal_coeff;              // Калибровочный коэффициент K_кал (1.00000)

    // Режим калибровки по эталону (п. 5.3.3 и 5.3.4 паспорта ИД-2)
    float ref_length_m;           // Длина эталонного куска для калибровки (напр. 10.00 м)

    // Суммарный счетчик выработки (наработка за смену/всего)
    double total_production_m;    // Суммарный счетчик длины (м)

    // Управление подающей бобиной (бухтой) с которой сматывается кабель
    float spool_initial_len_m;    // Начальная длина кабеля в бобине (м, 0..2000 м)
    float spool_remaining_len_m;  // Текущий остаток кабеля в бобине (м)

    // Автоматика скорости и защит
    bool const_line_speed_en;     // Режим стабилизации линейной скорости (м/мин)
    float target_speed_m_min;     // Заданная линейная скорость кабеля (м/мин)
    float base_freq_hz;           // Базовая частота вращения ПЧ (Гц)
    float creep_freq_hz;          // Ползучая частота дотяжки (Гц, напр. 6.0 Гц)
    float max_current_jam_a;      // Ток отсечки при запутывании отдающей катушки (А)

    // Текущие расчетные переменные
    int64_t total_pulses;         // Суммарное число импульсов (инкрементов) с датчиков
    float current_length_m;       // Текущая длина отрезка партии, м
    float line_speed_m_min;       // Текущая линейная скорость кабеля, м/мин
    winder_state_t state;         // Текущее состояние станка
} winder_context_t;

extern winder_context_t g_winder;

/**
 * @brief Инициализация аппаратного PCNT и фоновой задачи расчета
 */
esp_err_t idm20_encoder_init(void);

/**
 * @brief Пересчет импульсов энкодера в метры
 */
float idm20_calculate_length_m(int64_t pulses, const winder_context_t *ctx);

/**
 * @brief Автоматическая калибровка K_кал по контрольному эталону (рулетке)
 * @return true если калибровка успешно применена и сохранена
 */
bool idm20_calibrate_by_reference(float real_measured_meters);

/**
 * @brief Сброс калибровочного коэффициента в номинал 1.00000
 */
void idm20_reset_calibration(void);

/**
 * @brief Сохранение настроек калибровки в энергонезависимую память NVS
 */
esp_err_t idm20_save_settings_nvs(void);

/**
 * @brief Загрузка настроек калибровки из NVS
 */
esp_err_t idm20_load_settings_nvs(void);

/**
 * @brief Сброс счетчика текущего отрезка в 0.00 м
 */
void idm20_reset_length(void);

/**
 * @brief Сброс суммарного счетчика выработки
 */
void idm20_reset_production(void);

/**
 * @brief Команда на запуск автомата станка (переход в STATE_STARTING)
 */
void idm20_cmd_start(void);

/**
 * @brief Команда на останов автомата станка (переход в STATE_IDLE, блокировка счета)
 */
void idm20_cmd_stop(void);

/**
 * @brief Получение текстового названия текущего состояния станка
 */
const char *idm20_get_state_name(winder_state_t st);

#ifdef __cplusplus
}
#endif
