#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Построение главного интерфейса намоточного станка
 *        для экрана 1024x600 на базе LVGL 9
 */
void winder_ui_init(void);

/**
 * @brief Валидация и юнит-тестирование кириллических шрифтов и покрытия глифов
 * @return true если все русские буквы найдены в шрифтах без квадратиков/tofu
 */
bool winder_ui_test_fonts(void);

/**
 * @brief Валидация вместимости текстовых надписей и предотвращение наложений (HMI Text Assert)
 * @return true если все тексты умещаются в рамки без наложений
 */
bool winder_ui_validate_text_layout(void);

#ifdef __cplusplus
}
#endif
