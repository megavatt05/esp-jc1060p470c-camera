#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// =========================================================================
// Аппаратная конфигурация GUITION JC1060P470C (ESP32-P4)
// =========================================================================
#define BOARD_LCD_H_RES                 1024
#define BOARD_LCD_V_RES                 600

// Пины дисплея и подсветки
// СХЕМА V1.0: сброс панели висит на GPIO0 (цепочка GPIO0 -> R47 -> LCD_RST
// разъёма дисплея). GPIO27 — это RX1, вход встроенного MAX485, и занимать его
// нельзя: иначе линия 485 и сброс дисплея дергают друг друга.
#define BOARD_LCD_RESET_GPIO            GPIO_NUM_0
#define BOARD_LCD_BACKLIGHT_GPIO        GPIO_NUM_23

// LDO питания MIPI-DSI PHY (Обязательно до запуска DSI)
#define BOARD_MIPI_DSI_PHY_PWR_LDO_CHAN 3
#define BOARD_MIPI_DSI_PHY_PWR_LDO_MV   2500

// I2C шина периферии (Тач GT911, аудиокодек ES8311, RTC)
#define BOARD_I2C_PORT                  I2C_NUM_0
#define BOARD_I2C_SDA_GPIO              GPIO_NUM_7
#define BOARD_I2C_SCL_GPIO              GPIO_NUM_8

// Емкостный тачскрин GT911
// INT по умолчанию не используется: GPIO21 отдан гребёнке RS-485 (UART1 RX).
// GT911 на опросе работает на этой плате проверенно.
#ifndef CONFIG_STANOK_TOUCH_RST_GPIO
#define CONFIG_STANOK_TOUCH_RST_GPIO 22
#endif
#ifndef CONFIG_STANOK_TOUCH_INT_GPIO
#define CONFIG_STANOK_TOUCH_INT_GPIO (-1)
#endif
#define BOARD_TOUCH_RST_GPIO            ((gpio_num_t)CONFIG_STANOK_TOUCH_RST_GPIO)
#define BOARD_TOUCH_INT_GPIO            ((gpio_num_t)CONFIG_STANOK_TOUCH_INT_GPIO)

// Кнопка BOOT на плате (strapping GPIO)
#define BOARD_BOOT_BTN_GPIO             GPIO_NUM_35

/**
 * @brief Инициализация подсветки дисплея (LEDC ШИМ)
 */
esp_err_t board_backlight_init(void);

/**
 * @brief Установка яркости подсветки (0..100 %)
 */
esp_err_t board_backlight_set_percent(uint8_t percent);

/**
 * @brief Полная аппаратная инициализация платы JC1060P470C:
 *        LDO -> DSI -> JD9165 -> DMA2D -> I2C -> GT911 -> LVGL 9 Port
 * @return lv_display_t* Указатель на созданный LVGL-дисплей
 */
lv_display_t *board_jc1060p470c_init(void);

#ifdef __cplusplus
}
#endif
