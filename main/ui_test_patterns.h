#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PATTERN_MODE_STOPWATCH = 0,
    PATTERN_MODE_FOCUS = 1,
    PATTERN_MODE_COLOR_BARS = 2,
    PATTERN_MODE_GRID = 3,
    PATTERN_MODE_GRAYSCALE = 4,
    PATTERN_MODE_MAX
} pattern_mode_t;

/**
 * @brief Инициализация подсистемы калибровочных таблиц
 */
void ui_test_patterns_init(lv_obj_t *root_scr);

/**
 * @brief Установка текущего режима тестовой таблицы
 */
void ui_test_patterns_set_mode(pattern_mode_t mode);

/**
 * @brief Переключение на следующий шаблон по кругу
 */
void ui_test_patterns_next(void);

/**
 * @brief Получить текущий активный режим
 */
pattern_mode_t ui_test_patterns_get_mode(void);

/**
 * @brief Получить строковое название текущего режима
 */
const char* ui_test_patterns_get_name(pattern_mode_t mode);

/**
 * @brief Обработка строковой команды (например, "FOCUS", "COLOR", "GRID", "GRAY", "TIMER", "NEXT")
 */
bool ui_test_patterns_cmd(const char *cmd_name);

#ifdef __cplusplus
}
#endif
