/**
 * @file lv_conf.h
 * Конфигурация LVGL v9.5.0 для промышленного станка намотки кабеля PON
 * (ESP32-P4, 1024x600 IPS MIPI-DSI, GT911 touch, 32MB PSRAM)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>
#include "esp_log.h"

/*====================
   ЦВЕТ И БУФЕРЫ
 *====================*/
#define LV_COLOR_DEPTH 16
#define LV_DEF_REFR_PERIOD 15

/*=========================
   УПРАВЛЕНИЕ ПАМЯТЬЮ
 *=========================*/
/* Использовать стандартную системную кучу ESP-IDF (подключена к 32 МБ PSRAM) */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/* На случай отката к BUILTIN - выделяем 2 МБ из PSRAM */
#define LV_MEM_SIZE (2048 * 1024U)

/*====================
   БЕЗОПАСНОСТЬ / ASSERTS
 *====================*/
/* Никогда не вешать промышленный контроллер в вечном цикле while(1);! */
#define LV_USE_ASSERT_NULL              0
#define LV_USE_ASSERT_MALLOC            0
#define LV_USE_ASSERT_STYLE             0
#define LV_USE_ASSERT_MEM_INTEGRITY     0
#define LV_USE_ASSERT_OBJ               0

#define LV_ASSERT_HANDLER               ESP_LOGE("LVGL", "Assertion occurred, continuing execution...");

/*====================
   ТЕКСТ И КОДИРОВКА
 *====================*/
#define LV_TXT_ENC_UTF8 1

/*====================
   ШРИФТЫ
 *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_FONT_PLACEHOLDER 1

/*====================
   ВИДЖЕТЫ
 *====================*/
#define LV_USE_ANIMIMG          1
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BUTTON           1
#define LV_USE_BUTTONMATRIX     1
#define LV_USE_CALENDAR         0
#define LV_USE_CANVAS           1
#define LV_USE_CHART            0
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_IMAGE            1
#define LV_USE_IMAGEBUTTON      1
#define LV_USE_KEYBOARD         1
#define LV_USE_LABEL            1
#define LV_USE_LED              1
#define LV_USE_LINE             1
#define LV_USE_LIST             1
#define LV_USE_LOTTIE           0
#define LV_USE_MENU             0
#define LV_USE_MSGBOX           1
#define LV_USE_ROLLER           1
#define LV_USE_SCALE            1
#define LV_USE_SLIDER           1
#define LV_USE_SPAN             1
#define LV_USE_SPINBOX          1
#define LV_USE_SPINNER          1
#define LV_USE_SWITCH           1
#define LV_USE_TEXTAREA         1
#define LV_USE_TABLE            1
#define LV_USE_TABVIEW          1
#define LV_USE_TILEVIEW         1
#define LV_USE_WIN              0

/*====================
   ОТРИСОВКА (DRAW)
 *====================*/
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_DRAW_SW_COMPLEX 1

#endif /* LV_CONF_H */
