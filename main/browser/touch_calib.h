/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * Экран тестирования и калибровки тача (Touch Test / Calibration Mode)
 * для 7-дюймового дисплея JD9165 (1024x600).
 */

#ifndef TOUCH_CALIB_H
#define TOUCH_CALIB_H

#include <stdbool.h>
#include <stdint.h>
#include "camos/touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Сбросить текущее состояние экрана калибровки (метки углов, след, точки).
 */
void touch_calib_reset(void);

/**
 * @brief Обработка касания в режиме калибровки.
 *
 * @param pts       Массив точек текущего опроса тача
 * @param n_pts     Количество точек (> 0 если экран нажат)
 * @param tap_edge  true если это передний фронт нажатия
 * @param tap_x     Координата X первого нажатия
 * @param tap_y     Координата Y первого нажатия
 * @return true если нажата кнопка "ВЫХОД" (возврат в главное меню), false иначе
 */
bool touch_calib_handle_input(const touch_point_t *pts, int n_pts,
                              bool tap_edge, int tap_x, int tap_y);

/**
 * @brief Отрисовка калибровочного экрана во фреймбуфер.
 *
 * @param fb Указатель на буфер RGB565 (1024x600)
 */
void touch_calib_draw(uint16_t *fb);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_CALIB_H */
