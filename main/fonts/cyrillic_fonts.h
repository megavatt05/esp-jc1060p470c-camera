#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Кириллические шрифты DejaVu Sans (полный набор ASCII + Cyrillic 0x400..0x4FF + FontAwesome)
LV_FONT_DECLARE(lv_font_dejavu_16);
LV_FONT_DECLARE(lv_font_dejavu_24);
LV_FONT_DECLARE(lv_font_dejavu_32);
LV_FONT_DECLARE(lv_font_digits_48);
LV_FONT_DECLARE(lv_font_digits_72);
LV_FONT_DECLARE(lv_font_digits_120);

#ifdef __cplusplus
}
#endif
