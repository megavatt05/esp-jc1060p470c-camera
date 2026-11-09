#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация высокоточного микросекундомера для тестирования задержки камеры.
 * Создает экран LVGL 9 с крупными цифрами, оптическим маркёром синхронизации
 * и поддержкой поворота на 90/180/270 градусов под физическую ориентацию стенда.
 * 
 * @param disp Указатель на объект дисплея LVGL
 */
void ui_stopwatch_init(lv_display_t *disp);

/**
 * @brief Установка угла поворота экрана (0, 90, 180, 270 градусов)
 */
void ui_stopwatch_set_rotation(int deg);

/**
 * @brief Получить текущий угол поворота в градусах
 */
int ui_stopwatch_get_rotation(void);

/**
 * @brief Запуск отсчета времени
 */
void ui_stopwatch_start(void);

/**
 * @brief Остановка / пауза отсчета времени
 */
void ui_stopwatch_stop(void);

/**
 * @brief Сброс секундомера в 00.000000
 */
void ui_stopwatch_reset(void);

/**
 * @brief Переключение состояния (Старт <-> Стоп)
 */
void ui_stopwatch_toggle(void);

/**
 * @brief Текущее значение секундомера в микросекундах (1 мкс = 0.000001 с)
 */
int64_t ui_stopwatch_get_time_us(void);

/**
 * @brief Проверка, запущен ли секундомер
 */
bool ui_stopwatch_is_running(void);

/**
 * @brief Создать оптическую и аппаратную отметку (SYNC marker) для замера задержки
 */
void ui_stopwatch_trigger_mark(void);

#ifdef __cplusplus
}
#endif
