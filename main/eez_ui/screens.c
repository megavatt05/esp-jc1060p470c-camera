#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "idm20_encoder.h"
#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"
#include "cyrillic_fonts.h"

#include <string.h>

objects_t objects;

//
// Event handlers
//

lv_obj_t *tick_value_change_obj;

__attribute__((unused)) static void event_handler_cb_main_sld_sp(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            set_var_sp_raw(value);
        }
    }
}

__attribute__((unused)) static void event_handler_cb_main_spn_ust(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_ust_raw(value);
        }
    }
}

__attribute__((unused)) static void event_handler_cb_main_spn_ust_1(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_ust_raw(value);
        }
    }
}

__attribute__((unused)) static void event_handler_cb_main_spn_ust_2(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_ust_raw(value);
        }
    }
}

static void event_handler_cb_params_spn_channel(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_par_channel(value);
        }
    }
}

static void event_handler_cb_params_spn_fmax(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_par_fmax(value);
        }
    }
}

static void event_handler_cb_params_spn_fmin(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_par_fmin(value);
        }
    }
}

static void event_handler_cb_params_spn_fbase(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_par_fbase(value);
        }
    }
}

static void event_handler_cb_params_spn_accel(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_par_accel(value);
        }
    }
}

static void event_handler_cb_params_spn_decel(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_par_decel(value);
        }
    }
}

static void event_handler_cb_params_spn_jog(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_spinbox_get_value(ta);
            set_var_par_jog(value);
        }
    }
}

//
// Screens
//

// =========================================================================
// ЦИФРОВАЯ КЛАВИАТУРА (NUMPAD) ВВОДА УСТАВКИ ДЛЯ ОПЕРАТОРА СТАНКА
// =========================================================================
static char s_target_input_buf[16] = "2000.00";
static bool s_target_is_editing = false;
static lv_obj_t *s_lbl_target_val = NULL;

static void update_target_display(void)
{
    if (s_lbl_target_val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s м", s_target_input_buf);
        lv_label_set_text(s_lbl_target_val, buf);
        if (s_target_is_editing) {
            lv_obj_set_style_text_color(s_lbl_target_val, lv_color_hex(0xf59e0b), 0); // Янтарный при наборе
        } else {
            lv_obj_set_style_text_color(s_lbl_target_val, lv_color_hex(0x38bdf8), 0); // Голубой зафиксированный
        }
    }
}

static void numpad_btn_event_cb(lv_event_t *e)
{
    const char *key = (const char *)lv_event_get_user_data(e);
    if (!key) return;

    if (strcmp(key, "OK") == 0) {
        float val = (float)atof(s_target_input_buf);
        if (val < 1.0f) val = 1.0f;
        if (val > 2000.0f) val = 2000.0f;
        snprintf(s_target_input_buf, sizeof(s_target_input_buf), "%.2f", val);
        s_target_is_editing = false;
        
        int32_t raw = (int32_t)(val * 100.0f + 0.5f);
        set_var_ust_raw(raw);
        g_winder.target_length_m = val;
        if (objects.spn_ust) lv_spinbox_set_value(objects.spn_ust, raw);
        ESP_LOGI("NUMPAD", "Уставка намотки подтверждена: %.2f м", val);
        update_target_display();
        return;
    }

    if (strcmp(key, "CLR") == 0) {
        strcpy(s_target_input_buf, "0");
        s_target_is_editing = true;
        update_target_display();
        return;
    }

    if (strcmp(key, "DEL") == 0) {
        size_t len = strlen(s_target_input_buf);
        if (len > 1) {
            s_target_input_buf[len - 1] = '\0';
        } else {
            strcpy(s_target_input_buf, "0");
        }
        s_target_is_editing = true;
        update_target_display();
        return;
    }

    if (strcmp(key, "+10") == 0) {
        float val = (float)atof(s_target_input_buf) + 10.0f;
        if (val > 2000.0f) val = 2000.0f;
        snprintf(s_target_input_buf, sizeof(s_target_input_buf), "%.2f", val);
        s_target_is_editing = false;
        int32_t raw = (int32_t)(val * 100.0f + 0.5f);
        set_var_ust_raw(raw);
        g_winder.target_length_m = val;
        if (objects.spn_ust) lv_spinbox_set_value(objects.spn_ust, raw);
        update_target_display();
        return;
    }

    if (!s_target_is_editing) {
        s_target_input_buf[0] = '\0';
        s_target_is_editing = true;
    }

    size_t len = strlen(s_target_input_buf);
    if (len >= 7) return;

    if (strcmp(key, ".") == 0) {
        if (strchr(s_target_input_buf, '.') == NULL) {
            if (len == 0) strcat(s_target_input_buf, "0.");
            else strcat(s_target_input_buf, ".");
        }
    } else if (strcmp(key, "00") == 0) {
        if (len > 0 && strcmp(s_target_input_buf, "0") != 0 && len <= 5) {
            strcat(s_target_input_buf, "00");
        }
    } else {
        if (strcmp(s_target_input_buf, "0") == 0) {
            strcpy(s_target_input_buf, key);
        } else {
            strcat(s_target_input_buf, key);
        }
    }

    update_target_display();
}

void create_screen_main() {
    // =========================================================================
    // ЭКРАН ОПЕРАТОРА СТАНКА (1024x600 IPS)
    // Только гигантский одометр, поле уставки, Numpad и кнопки ПУСК/СТОП/СБРОС.
    // =========================================================================
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1024, 600);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x070b12), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *parent_obj = obj;

    // 1. ТАБЛО ОДОМЕТРА ТЕКУЩЕГО МЕТРАЖА (1004 x 180 px, шрифты 120 px)
    {
        lv_obj_t *c_len = lv_obj_create(parent_obj);
        objects.len = c_len;
        lv_obj_set_pos(c_len, 10, 8);
        lv_obj_set_size(c_len, 1004, 180);
        lv_obj_set_style_bg_color(c_len, lv_color_hex(0x0c1322), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(c_len, lv_color_hex(0x2563eb), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(c_len, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(c_len, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(c_len, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *cap_len = lv_label_create(c_len);
        objects.lbl_len_cap = cap_len;
        lv_obj_set_pos(cap_len, 24, 10);
        lv_obj_set_size(cap_len, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(cap_len, lv_color_hex(0x60a5fa), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(cap_len, &lv_font_dejavu_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_static(cap_len, "ТЕКУЩИЙ МЕТРАЖ НАМОТКИ");

        // Гигантские цифры 120 px
        lv_obj_t *num_len = lv_label_create(c_len);
        objects.lbl_len = num_len;
        lv_obj_set_size(num_len, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(num_len, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(num_len, &lv_font_digits_120, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(num_len, LV_ALIGN_CENTER, -60, 10);
        lv_label_set_text(num_len, "0000.00");

        // Единица измерения крупно справа
        lv_obj_t *unit_m = lv_label_create(c_len);
        lv_obj_set_size(unit_m, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(unit_m, lv_color_hex(0x38bdf8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(unit_m, &lv_font_dejavu_32, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(unit_m, LV_ALIGN_RIGHT_MID, -36, 12);
        lv_label_set_text_static(unit_m, "МЕТРОВ");

        // Скрытая выработка
        objects.lbl_prod = lv_label_create(c_len);
        lv_obj_add_flag(objects.lbl_prod, LV_OBJ_FLAG_HIDDEN);
    }

    // 2. ЛЕВАЯ КОЛОНКА (w=532 px): УСТАВКА + КНОПКИ УПРАВЛЕНИЯ
    {
        // 2.1. Карточка Уставки партии
        lv_obj_t *c_ust = lv_obj_create(parent_obj);
        objects.ust = c_ust;
        lv_obj_set_pos(c_ust, 10, 196);
        lv_obj_set_size(c_ust, 532, 76);
        lv_obj_set_style_bg_color(c_ust, lv_color_hex(0x131a26), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(c_ust, lv_color_hex(0x2563eb), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(c_ust, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(c_ust, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(c_ust, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *cap_u = lv_label_create(c_ust);
        lv_obj_set_pos(cap_u, 20, 24);
        lv_obj_set_style_text_color(cap_u, lv_color_hex(0x94a3b8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(cap_u, &lv_font_dejavu_24, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_static(cap_u, "УСТАВКА:");

        s_lbl_target_val = lv_label_create(c_ust);
        lv_obj_set_pos(s_lbl_target_val, 160, 20);
        lv_obj_set_style_text_color(s_lbl_target_val, lv_color_hex(0x38bdf8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(s_lbl_target_val, &lv_font_dejavu_32, LV_PART_MAIN | LV_STATE_DEFAULT);
        snprintf(s_target_input_buf, sizeof(s_target_input_buf), "%.2f", g_winder.target_length_m > 0.1f ? g_winder.target_length_m : 2000.0f);
        update_target_display();

        // 2.2. Кнопка "ПУСК" (258 x 156 px)
        lv_obj_t *b_start = lv_button_create(parent_obj);
        objects.btn_start = b_start;
        lv_obj_set_pos(b_start, 10, 280);
        lv_obj_set_size(b_start, 258, 156);
        lv_obj_add_event_cb(b_start, action_btn_start, LV_EVENT_CLICKED, 0);
        lv_obj_set_style_bg_color(b_start, lv_color_hex(0x16a34a), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(b_start, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(b_start, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(b_start, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *b_start_l = lv_label_create(b_start);
        objects.btn_start_lbl = b_start_l;
        lv_obj_center(b_start_l);
        lv_obj_set_style_text_color(b_start_l, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(b_start_l, &lv_font_dejavu_32, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_static(b_start_l, "ПУСК");

        // 2.3. Кнопка "СТОП" (258 x 156 px)
        lv_obj_t *b_stop = lv_button_create(parent_obj);
        objects.btn_stop = b_stop;
        lv_obj_set_pos(b_stop, 284, 280);
        lv_obj_set_size(b_stop, 258, 156);
        lv_obj_add_event_cb(b_stop, action_btn_stop, LV_EVENT_CLICKED, 0);
        lv_obj_set_style_bg_color(b_stop, lv_color_hex(0xdc2626), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(b_stop, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(b_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(b_stop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *b_stop_l = lv_label_create(b_stop);
        objects.btn_stop_lbl = b_stop_l;
        lv_obj_center(b_stop_l);
        lv_obj_set_style_text_color(b_stop_l, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(b_stop_l, &lv_font_dejavu_32, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_static(b_stop_l, "СТОП");

        // 2.4. Кнопка "СБРОС МЕТРАЖА" (532 x 86 px)
        lv_obj_t *b_reset = lv_button_create(parent_obj);
        objects.btn_fault_reset = b_reset;
        lv_obj_set_pos(b_reset, 10, 448);
        lv_obj_set_size(b_reset, 532, 86);
        lv_obj_add_event_cb(b_reset, action_btn_fault_reset, LV_EVENT_CLICKED, 0);
        lv_obj_set_style_bg_color(b_reset, lv_color_hex(0xd97706), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(b_reset, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(b_reset, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(b_reset, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *b_reset_l = lv_label_create(b_reset);
        objects.btn_fault_reset_lbl = b_reset_l;
        lv_obj_center(b_reset_l);
        lv_obj_set_style_text_color(b_reset_l, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(b_reset_l, &lv_font_dejavu_24, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_static(b_reset_l, "СБРОС МЕТРАЖА");
    }

    // 3. ПРАВАЯ КОЛОНКА (w=458 px): ПОЛНОРАЗМЕРНЫЙ ЦИФРОВОЙ NUMPAD
    {
        lv_obj_t *c_pad = lv_obj_create(parent_obj);
        lv_obj_set_pos(c_pad, 556, 196);
        lv_obj_set_size(c_pad, 458, 338);
        lv_obj_set_style_bg_color(c_pad, lv_color_hex(0x131a26), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(c_pad, lv_color_hex(0x27354a), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(c_pad, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(c_pad, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(c_pad, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

        // Определение кнопок 4x4
        struct NumpadBtnDef {
            const char *label;
            const char *val;
            int col; // 0..3
            int row; // 0..3
            uint32_t bg;
            const lv_font_t *font;
        } pad_btns[] = {
            { "7",    "7",   0, 0, 0x1f2937, &lv_font_dejavu_32 },
            { "8",    "8",   1, 0, 0x1f2937, &lv_font_dejavu_32 },
            { "9",    "9",   2, 0, 0x1f2937, &lv_font_dejavu_32 },
            { "<-",   "DEL", 3, 0, 0x3f3f46, &lv_font_dejavu_24 },

            { "4",    "4",   0, 1, 0x1f2937, &lv_font_dejavu_32 },
            { "5",    "5",   1, 1, 0x1f2937, &lv_font_dejavu_32 },
            { "6",    "6",   2, 1, 0x1f2937, &lv_font_dejavu_32 },
            { "C",    "CLR", 3, 1, 0x3f3f46, &lv_font_dejavu_24 },

            { "1",    "1",   0, 2, 0x1f2937, &lv_font_dejavu_32 },
            { "2",    "2",   1, 2, 0x1f2937, &lv_font_dejavu_32 },
            { "3",    "3",   2, 2, 0x1f2937, &lv_font_dejavu_32 },
            { "+10",  "+10", 3, 2, 0x1e3a8a, &lv_font_dejavu_24 },

            { "0",    "0",   0, 3, 0x1f2937, &lv_font_dejavu_32 },
            { ".",    ".",   1, 3, 0x1f2937, &lv_font_dejavu_32 },
            { "00",   "00",  2, 3, 0x1f2937, &lv_font_dejavu_32 },
            { "ВВОД", "OK",  3, 3, 0x16a34a, &lv_font_dejavu_24 }
        };

        const int btn_w = 104;
        const int btn_h = 74;
        const int step_x = 112;
        const int step_y = 82;

        for (int i = 0; i < 16; i++) {
            lv_obj_t *b = lv_button_create(c_pad);
            lv_obj_set_pos(b, pad_btns[i].col * step_x, pad_btns[i].row * step_y);
            lv_obj_set_size(b, btn_w, btn_h);
            lv_obj_add_event_cb(b, numpad_btn_event_cb, LV_EVENT_CLICKED, (void *)pad_btns[i].val);
            lv_obj_set_style_bg_color(b, lv_color_hex(pad_btns[i].bg), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(b, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(b, lv_color_hex(0x475569), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(b, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_t *bl = lv_label_create(b);
            lv_obj_center(bl);
            lv_obj_set_style_text_color(bl, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(bl, pad_btns[i].font, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(bl, pad_btns[i].label);
        }
    }

    // 4. НАВИГАЦИОННАЯ ПАНЕЛЬ ЭКРАНОВ (h=50px)
    {
        lv_obj_t *nav = lv_obj_create(parent_obj);
        objects.nav_main = nav;
        lv_obj_set_pos(nav, 0, 544);
        lv_obj_set_size(nav, 1024, 56);
        lv_obj_set_style_bg_color(nav, lv_color_hex(0x0c1322), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(nav, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(nav, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(nav, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        struct { lv_obj_t **btn; lv_obj_t **lbl; const char *txt; int x; uint32_t bg; lv_event_cb_t cb; } tabs[] = {
            { &objects.btn_nav_main_main,   &objects.btn_nav_main_main_lbl,   "1. ОПЕРАТОР",    10,  0x2563eb, action_nav_main },
            { &objects.btn_nav_params_main, &objects.btn_nav_params_main_lbl, "2. ПАРАМЕТРЫ ЧП", 262, 0x1e293b, action_nav_params },
            { &objects.btn_nav_alarm_main,  &objects.btn_nav_alarm_main_lbl,  "3. АВАРИИ",       514, 0x1e293b, action_nav_alarm },
            { &objects.btn_nav_conn_main,   &objects.btn_nav_conn_main_lbl,   "4. ЛИНИЯ И 485",  766, 0x1e293b, action_nav_conn }
        };
        for (int i = 0; i < 4; i++) {
            lv_obj_t *b = lv_button_create(nav);
            *(tabs[i].btn) = b;
            lv_obj_set_pos(b, tabs[i].x, 6);
            lv_obj_set_size(b, 242, 44);
            lv_obj_add_event_cb(b, tabs[i].cb, LV_EVENT_CLICKED, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(tabs[i].bg), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(b, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_t *bl = lv_label_create(b);
            *(tabs[i].lbl) = bl;
            lv_obj_center(bl);
            lv_obj_set_style_text_color(bl, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(bl, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(bl, tabs[i].txt);
        }
    }

    // 5. СКРЫТЫЕ СЛУЖЕБНЫЕ ОБЪЕКТЫ (ДЛЯ ПОЛНОЙ ОБРАТНОЙ СОВМЕСТИМОСТИ С EEZ_GLUE И TICK)
    {
        objects.top_main = lv_obj_create(parent_obj); lv_obj_add_flag(objects.top_main, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_title_main = lv_label_create(objects.top_main);
        objects.link = lv_obj_create(parent_obj); lv_obj_add_flag(objects.link, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_link = lv_label_create(objects.link);
        objects.lbl_speed = lv_label_create(parent_obj); lv_obj_add_flag(objects.lbl_speed, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_state = lv_label_create(parent_obj); lv_obj_add_flag(objects.lbl_state, LV_OBJ_FLAG_HIDDEN);

        objects.spn_ust = lv_spinbox_create(parent_obj); lv_obj_add_flag(objects.spn_ust, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_ust_cap = lv_label_create(parent_obj); lv_obj_add_flag(objects.lbl_ust_cap, LV_OBJ_FLAG_HIDDEN);
        objects.btn_ust_d100 = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_ust_d100, LV_OBJ_FLAG_HIDDEN);
        objects.btn_ust_d100_lbl = lv_label_create(objects.btn_ust_d100);
        objects.btn_ust_d10 = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_ust_d10, LV_OBJ_FLAG_HIDDEN);
        objects.btn_ust_d10_lbl = lv_label_create(objects.btn_ust_d10);
        objects.btn_ust_d1 = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_ust_d1, LV_OBJ_FLAG_HIDDEN);
        objects.btn_ust_d1_lbl = lv_label_create(objects.btn_ust_d1);
        objects.btn_ust_u1 = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_ust_u1, LV_OBJ_FLAG_HIDDEN);
        objects.btn_ust_u1_lbl = lv_label_create(objects.btn_ust_u1);
        objects.btn_ust_u10 = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_ust_u10, LV_OBJ_FLAG_HIDDEN);
        objects.btn_ust_u10_lbl = lv_label_create(objects.btn_ust_u10);
        objects.btn_ust_u100 = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_ust_u100, LV_OBJ_FLAG_HIDDEN);
        objects.btn_ust_u100_lbl = lv_label_create(objects.btn_ust_u100);
        objects.spn_ust_1 = lv_spinbox_create(parent_obj); lv_obj_add_flag(objects.spn_ust_1, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_ust_cap_1 = lv_label_create(parent_obj); lv_obj_add_flag(objects.lbl_ust_cap_1, LV_OBJ_FLAG_HIDDEN);
        objects.spn_ust_2 = lv_spinbox_create(parent_obj); lv_obj_add_flag(objects.spn_ust_2, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_ust_cap_2 = lv_label_create(parent_obj); lv_obj_add_flag(objects.lbl_ust_cap_2, LV_OBJ_FLAG_HIDDEN);

        objects.btn_jog = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_jog, LV_OBJ_FLAG_HIDDEN);
        objects.btn_jog_lbl = lv_label_create(objects.btn_jog);
        objects.btn_rev = lv_button_create(parent_obj); lv_obj_add_flag(objects.btn_rev, LV_OBJ_FLAG_HIDDEN);
        objects.btn_rev_lbl = lv_label_create(objects.btn_rev);

        objects.mon = lv_obj_create(parent_obj); lv_obj_add_flag(objects.mon, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_freq_cap = lv_label_create(objects.mon); objects.lbl_freq = lv_label_create(objects.mon);
        objects.lbl_cur_cap = lv_label_create(objects.mon); objects.lbl_cur = lv_label_create(objects.mon);
        objects.lbl_volt_cap = lv_label_create(objects.mon); objects.lbl_volt = lv_label_create(objects.mon);
        objects.lbl_rpm_cap = lv_label_create(objects.mon); objects.lbl_rpm = lv_label_create(objects.mon);

        objects.sp = lv_obj_create(parent_obj); lv_obj_add_flag(objects.sp, LV_OBJ_FLAG_HIDDEN);
        objects.lbl_sp_cap = lv_label_create(objects.sp); objects.lbl_sp_val = lv_label_create(objects.sp);
        objects.btn_sp_dn = lv_button_create(objects.sp); objects.btn_sp_dn_lbl = lv_label_create(objects.btn_sp_dn);
        objects.btn_sp_up = lv_button_create(objects.sp); objects.btn_sp_up_lbl = lv_label_create(objects.btn_sp_up);
        objects.sld_sp = lv_slider_create(objects.sp);
    }

    tick_screen_main();
}

void tick_screen_main() {
    // Динамический статус станка в шапке одометра и подсветка рамки
    if (objects.lbl_len_cap && objects.len) {
        static winder_state_t s_last_rendered_state = (winder_state_t)-1;
        if (g_winder.state != s_last_rendered_state) {
            s_last_rendered_state = g_winder.state;
            switch (g_winder.state) {
            case STATE_STARTING:
                lv_label_set_text_static(objects.lbl_len_cap, "РАЗГОН ЧАСТОТНИКА... ОЖИДАНИЕ ХОДА");
                lv_obj_set_style_text_color(objects.lbl_len_cap, lv_color_hex(0xfacc15), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(objects.len, lv_color_hex(0xeab308), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case STATE_RUNNING:
                lv_label_set_text_static(objects.lbl_len_cap, "ИДЕТ НАМОТКА (ХОД)");
                lv_obj_set_style_text_color(objects.lbl_len_cap, lv_color_hex(0x4ade80), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(objects.len, lv_color_hex(0x22c55e), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case STATE_SLOWDOWN:
                lv_label_set_text_static(objects.lbl_len_cap, "ДОТЯЖКА ДО УСТАВКИ (ЗАМЕДЛЕНИЕ)");
                lv_obj_set_style_text_color(objects.lbl_len_cap, lv_color_hex(0xfb923c), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(objects.len, lv_color_hex(0xf97316), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case STATE_DONE:
                lv_label_set_text_static(objects.lbl_len_cap, "НАМОТКА ЗАВЕРШЕНА! НАЖМИТЕ СБРОС");
                lv_obj_set_style_text_color(objects.lbl_len_cap, lv_color_hex(0x2dd4bf), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(objects.len, lv_color_hex(0x14b8a6), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case STATE_ALARM_BREAK:
            case STATE_ALARM_JAM:
            case STATE_ALARM_VFD:
                lv_label_set_text_static(objects.lbl_len_cap, "АВАРИЯ! СТАНОК ОСТАНОВЛЕН");
                lv_obj_set_style_text_color(objects.lbl_len_cap, lv_color_hex(0xf87171), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(objects.len, lv_color_hex(0xef4444), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case STATE_IDLE:
            default:
                lv_label_set_text_static(objects.lbl_len_cap, "СТАНОК ОСТАНОВЛЕН (ГОТОВ К ПУСКУ)");
                lv_obj_set_style_text_color(objects.lbl_len_cap, lv_color_hex(0x60a5fa), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(objects.len, lv_color_hex(0x2563eb), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            }
        }
    }

    {
        const char *new_val = get_var_link_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_link);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_link;
            lv_label_set_text(objects.lbl_link, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_len_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_len);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_len;
            lv_label_set_text(objects.lbl_len, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_speed_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_speed);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_speed;
            lv_label_set_text(objects.lbl_speed, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_prod_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_prod);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_prod;
            lv_label_set_text(objects.lbl_prod, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_sp_raw();
        int32_t cur_val = lv_slider_get_value(objects.sld_sp);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.sld_sp;
            lv_slider_set_value(objects.sld_sp, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_sp_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_sp_val);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_sp_val;
            lv_label_set_text(objects.lbl_sp_val, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_freq_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_freq);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_freq;
            lv_label_set_text(objects.lbl_freq, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_cur_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_cur);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_cur;
            lv_label_set_text(objects.lbl_cur, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_volt_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_volt);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_volt;
            lv_label_set_text(objects.lbl_volt, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_rpm_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_rpm);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_rpm;
            lv_label_set_text(objects.lbl_rpm, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_state_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_state);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_state;
            lv_label_set_text(objects.lbl_state, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_ust_raw();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_ust);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_ust;
            lv_spinbox_set_value(objects.spn_ust, new_val);
            tick_value_change_obj = NULL;
            if (!s_target_is_editing) {
                float fval = new_val / 100.0f;
                snprintf(s_target_input_buf, sizeof(s_target_input_buf), "%.2f", fval);
                update_target_display();
            }
        }
    }
    {
        int32_t new_val = get_var_ust_raw();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_ust_1);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_ust_1;
            lv_spinbox_set_value(objects.spn_ust_1, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_ust_raw();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_ust_2);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_ust_2;
            lv_spinbox_set_value(objects.spn_ust_2, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_params() {
    // params
    lv_obj_t *obj = lv_obj_create(0);
    objects.params = obj;
    lv_obj_set_pos(obj, 1100, 0);
    lv_obj_set_size(obj, 1024, 600);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x10151c), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // top_params
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.top_params = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 1024, 56);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x16202e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // lbl_title_params
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_title_params = obj;
                    lv_obj_set_pos(obj, 16, 12);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_head26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ПАРАМЕТРЫ ПРЕОБРАЗОВАТЕЛЯ");
                }
                {
                    // lbl_top_right_params
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_top_right_params = obj;
                    lv_obj_set_pos(obj, 800, 18);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "АДРЕС 1  9600 8E1");
                }
            }
        }
        {
            // btn_p_read
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_p_read = obj;
            lv_obj_set_pos(obj, 16, 72);
            lv_obj_set_size(obj, 240, 48);
            lv_obj_add_event_cb(obj, action_par_read, LV_EVENT_CLICKED, (void *)0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_p_read_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.btn_p_read_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ЧИТАТЬ");
                }
            }
        }
        {
            // btn_p_write
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_p_write = obj;
            lv_obj_set_pos(obj, 272, 72);
            lv_obj_set_size(obj, 240, 48);
            lv_obj_add_event_cb(obj, action_par_write, LV_EVENT_CLICKED, (void *)0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x16a34a), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_p_write_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.btn_p_write_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ЗАПИСАТЬ");
                }
            }
        }
        {
            // lbl_p_msg
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p_msg = obj;
            lv_obj_set_pos(obj, 540, 86);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // lbl_p0
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p0 = obj;
            lv_obj_set_pos(obj, 20, 148);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "P0.01  Канал задания (3 = Modbus)");
        }
        {
            // spn_channel
            lv_obj_t *obj = lv_spinbox_create(parent_obj);
            objects.spn_channel = obj;
            lv_obj_set_pos(obj, 760, 136);
            lv_obj_set_size(obj, 240, 44);
            lv_spinbox_set_digit_format(obj, 1, 0);
            lv_spinbox_set_range(obj, 0, 8);
            lv_spinbox_set_rollover(obj, false);
            lv_spinbox_set_step(obj, 1);
            lv_obj_add_event_cb(obj, event_handler_cb_params_spn_channel, LV_EVENT_ALL, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x263042), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_p1
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p1 = obj;
            lv_obj_set_pos(obj, 20, 198);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "P0.06  Верхняя частота, Гц");
        }
        {
            // spn_fmax
            lv_obj_t *obj = lv_spinbox_create(parent_obj);
            objects.spn_fmax = obj;
            lv_obj_set_pos(obj, 760, 186);
            lv_obj_set_size(obj, 240, 44);
            lv_spinbox_set_digit_format(obj, 5, 2);
            lv_spinbox_set_range(obj, 0, 40000);
            lv_spinbox_set_rollover(obj, false);
            lv_spinbox_set_step(obj, 1);
            lv_obj_add_event_cb(obj, event_handler_cb_params_spn_fmax, LV_EVENT_ALL, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x263042), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_p2
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p2 = obj;
            lv_obj_set_pos(obj, 20, 248);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "P0.07  Нижняя частота, Гц");
        }
        {
            // spn_fmin
            lv_obj_t *obj = lv_spinbox_create(parent_obj);
            objects.spn_fmin = obj;
            lv_obj_set_pos(obj, 760, 236);
            lv_obj_set_size(obj, 240, 44);
            lv_spinbox_set_digit_format(obj, 5, 2);
            lv_spinbox_set_range(obj, 0, 40000);
            lv_spinbox_set_rollover(obj, false);
            lv_spinbox_set_step(obj, 1);
            lv_obj_add_event_cb(obj, event_handler_cb_params_spn_fmin, LV_EVENT_ALL, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x263042), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_p3
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p3 = obj;
            lv_obj_set_pos(obj, 20, 298);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "P0.08  Базовая частота, Гц");
        }
        {
            // spn_fbase
            lv_obj_t *obj = lv_spinbox_create(parent_obj);
            objects.spn_fbase = obj;
            lv_obj_set_pos(obj, 760, 286);
            lv_obj_set_size(obj, 240, 44);
            lv_spinbox_set_digit_format(obj, 5, 2);
            lv_spinbox_set_range(obj, 0, 40000);
            lv_spinbox_set_rollover(obj, false);
            lv_spinbox_set_step(obj, 1);
            lv_obj_add_event_cb(obj, event_handler_cb_params_spn_fbase, LV_EVENT_ALL, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x263042), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_p4
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p4 = obj;
            lv_obj_set_pos(obj, 20, 348);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "P0.14  Время разгона, ед. по P3.09");
        }
        {
            // spn_accel
            lv_obj_t *obj = lv_spinbox_create(parent_obj);
            objects.spn_accel = obj;
            lv_obj_set_pos(obj, 760, 336);
            lv_obj_set_size(obj, 240, 44);
            lv_spinbox_set_digit_format(obj, 5, 1);
            lv_spinbox_set_range(obj, 1, 36000);
            lv_spinbox_set_rollover(obj, false);
            lv_spinbox_set_step(obj, 1);
            lv_obj_add_event_cb(obj, event_handler_cb_params_spn_accel, LV_EVENT_ALL, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x263042), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_p5
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p5 = obj;
            lv_obj_set_pos(obj, 20, 398);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "P0.15  Время торможения, ед. P3.09");
        }
        {
            // spn_decel
            lv_obj_t *obj = lv_spinbox_create(parent_obj);
            objects.spn_decel = obj;
            lv_obj_set_pos(obj, 760, 386);
            lv_obj_set_size(obj, 240, 44);
            lv_spinbox_set_digit_format(obj, 5, 1);
            lv_spinbox_set_range(obj, 1, 36000);
            lv_spinbox_set_rollover(obj, false);
            lv_spinbox_set_step(obj, 1);
            lv_obj_add_event_cb(obj, event_handler_cb_params_spn_decel, LV_EVENT_ALL, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x263042), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // lbl_p6
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.lbl_p6 = obj;
            lv_obj_set_pos(obj, 20, 448);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "P2.19  Частота толчка JOG, Гц");
        }
        {
            // spn_jog
            lv_obj_t *obj = lv_spinbox_create(parent_obj);
            objects.spn_jog = obj;
            lv_obj_set_pos(obj, 760, 436);
            lv_obj_set_size(obj, 240, 44);
            lv_spinbox_set_digit_format(obj, 4, 2);
            lv_spinbox_set_range(obj, 0, 5000);
            lv_spinbox_set_rollover(obj, false);
            lv_spinbox_set_step(obj, 1);
            lv_obj_add_event_cb(obj, event_handler_cb_params_spn_jog, LV_EVENT_ALL, 0);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x263042), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // nav_params
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.nav_params = obj;
            lv_obj_set_pos(obj, 0, 516);
            lv_obj_set_size(obj, 1024, 84);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x16202e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_nav_main_params
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_main_params = obj;
                    lv_obj_set_pos(obj, 8, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_main, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_main_params_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_main_params_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ГЛАВНАЯ");
                        }
                    }
                }
                {
                    // btn_nav_params_params
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_params_params = obj;
                    lv_obj_set_pos(obj, 262, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_params, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x2563eb), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_params_params_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_params_params_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ПАРАМЕТРЫ");
                        }
                    }
                }
                {
                    // btn_nav_alarm_params
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_alarm_params = obj;
                    lv_obj_set_pos(obj, 516, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_alarm, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_alarm_params_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_alarm_params_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "АВАРИИ");
                        }
                    }
                }
                {
                    // btn_nav_conn_params
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_conn_params = obj;
                    lv_obj_set_pos(obj, 770, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_conn, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_conn_params_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_conn_params_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ЛИНИЯ");
                        }
                    }
                }
            }
        }
    }
    
    tick_screen_params();
}

void tick_screen_params() {
    {
        const char *new_val = get_var_p_msg_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_p_msg);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_p_msg;
            lv_label_set_text(objects.lbl_p_msg, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_par_channel();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_channel);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_channel;
            lv_spinbox_set_value(objects.spn_channel, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_par_fmax();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_fmax);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_fmax;
            lv_spinbox_set_value(objects.spn_fmax, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_par_fmin();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_fmin);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_fmin;
            lv_spinbox_set_value(objects.spn_fmin, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_par_fbase();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_fbase);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_fbase;
            lv_spinbox_set_value(objects.spn_fbase, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_par_accel();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_accel);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_accel;
            lv_spinbox_set_value(objects.spn_accel, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_par_decel();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_decel);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_decel;
            lv_spinbox_set_value(objects.spn_decel, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_par_jog();
        int32_t cur_val = lv_spinbox_get_value(objects.spn_jog);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.spn_jog;
            lv_spinbox_set_value(objects.spn_jog, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_alarm() {
    // alarm
    lv_obj_t *obj = lv_obj_create(0);
    objects.alarm = obj;
    lv_obj_set_pos(obj, 2200, 0);
    lv_obj_set_size(obj, 1024, 600);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x10151c), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // top_alarm
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.top_alarm = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 1024, 56);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x16202e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // lbl_title_alarm
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_title_alarm = obj;
                    lv_obj_set_pos(obj, 16, 12);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_head26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "АВАРИИ И ЖУРНАЛ ПРИВОДА");
                }
            }
        }
        {
            // cur
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.cur = obj;
            lv_obj_set_pos(obj, 16, 68);
            lv_obj_set_size(obj, 992, 120);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x28161a), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // lbl_acode
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_acode = obj;
                    lv_obj_set_pos(obj, 20, 16);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xdc2626), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_head26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_aname
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_aname = obj;
                    lv_obj_set_pos(obj, 180, 22);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_adesc
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_adesc = obj;
                    lv_obj_set_pos(obj, 180, 58);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_atime
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_atime = obj;
                    lv_obj_set_pos(obj, 20, 86);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // btn_a_read
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_a_read = obj;
            lv_obj_set_pos(obj, 16, 200);
            lv_obj_set_size(obj, 260, 44);
            lv_obj_add_event_cb(obj, action_journal_read, LV_EVENT_CLICKED, (void *)0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_a_read_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.btn_a_read_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ОБНОВИТЬ ЖУРНАЛ");
                }
            }
        }
        {
            // btn_a_reset
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_a_reset = obj;
            lv_obj_set_pos(obj, 292, 200);
            lv_obj_set_size(obj, 260, 44);
            lv_obj_add_event_cb(obj, action_fault_reset, LV_EVENT_CLICKED, (void *)0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xf59e0b), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_a_reset_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.btn_a_reset_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x10151c), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "СБРОС АВАРИИ");
                }
            }
        }
        {
            // jrn
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.jrn = obj;
            lv_obj_set_pos(obj, 16, 256);
            lv_obj_set_size(obj, 992, 248);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a2230), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // lbl_jrn_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_jrn_cap = obj;
                    lv_obj_set_pos(obj, 20, 10);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ПОСЛЕДНИЕ АВАРИИ ПРИВОДА");
                }
                {
                    // lbl_jrn0
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_jrn0 = obj;
                    lv_obj_set_pos(obj, 20, 36);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_jrn1
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_jrn1 = obj;
                    lv_obj_set_pos(obj, 20, 58);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_jrn2
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_jrn2 = obj;
                    lv_obj_set_pos(obj, 20, 80);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_snap_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_snap_cap = obj;
                    lv_obj_set_pos(obj, 520, 10);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ПАРАМЕТРЫ ПРИ ПОСЛЕДНЕЙ АВАРИИ");
                }
                {
                    // lbl_snap0
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_snap0 = obj;
                    lv_obj_set_pos(obj, 520, 36);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_snap1
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_snap1 = obj;
                    lv_obj_set_pos(obj, 520, 58);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_snap2
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_snap2 = obj;
                    lv_obj_set_pos(obj, 520, 80);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_snap3
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_snap3 = obj;
                    lv_obj_set_pos(obj, 520, 102);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_snap4
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_snap4 = obj;
                    lv_obj_set_pos(obj, 520, 124);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_snap5
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_snap5 = obj;
                    lv_obj_set_pos(obj, 520, 146);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // nav_alarm
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.nav_alarm = obj;
            lv_obj_set_pos(obj, 0, 516);
            lv_obj_set_size(obj, 1024, 84);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x16202e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_nav_main_alarm
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_main_alarm = obj;
                    lv_obj_set_pos(obj, 8, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_main, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_main_alarm_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_main_alarm_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ГЛАВНАЯ");
                        }
                    }
                }
                {
                    // btn_nav_params_alarm
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_params_alarm = obj;
                    lv_obj_set_pos(obj, 262, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_params, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_params_alarm_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_params_alarm_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ПАРАМЕТРЫ");
                        }
                    }
                }
                {
                    // btn_nav_alarm_alarm
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_alarm_alarm = obj;
                    lv_obj_set_pos(obj, 516, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_alarm, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x2563eb), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_alarm_alarm_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_alarm_alarm_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "АВАРИИ");
                        }
                    }
                }
                {
                    // btn_nav_conn_alarm
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_conn_alarm = obj;
                    lv_obj_set_pos(obj, 770, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_conn, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_conn_alarm_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_conn_alarm_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ЛИНИЯ");
                        }
                    }
                }
            }
        }
    }
    
    tick_screen_alarm();
}

void tick_screen_alarm() {
    {
        const char *new_val = get_var_fault_code_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_acode);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_acode;
            lv_label_set_text(objects.lbl_acode, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_fault_name_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_aname);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_aname;
            lv_label_set_text(objects.lbl_aname, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_fault_desc_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_adesc);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_adesc;
            lv_label_set_text(objects.lbl_adesc, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_fault_time_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_atime);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_atime;
            lv_label_set_text(objects.lbl_atime, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_jrn0_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_jrn0);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_jrn0;
            lv_label_set_text(objects.lbl_jrn0, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_jrn1_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_jrn1);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_jrn1;
            lv_label_set_text(objects.lbl_jrn1, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_jrn2_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_jrn2);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_jrn2;
            lv_label_set_text(objects.lbl_jrn2, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_snap0_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_snap0);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_snap0;
            lv_label_set_text(objects.lbl_snap0, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_snap1_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_snap1);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_snap1;
            lv_label_set_text(objects.lbl_snap1, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_snap2_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_snap2);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_snap2;
            lv_label_set_text(objects.lbl_snap2, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_snap3_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_snap3);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_snap3;
            lv_label_set_text(objects.lbl_snap3, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_snap4_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_snap4);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_snap4;
            lv_label_set_text(objects.lbl_snap4, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_snap5_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_snap5);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_snap5;
            lv_label_set_text(objects.lbl_snap5, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_conn() {
    // conn
    lv_obj_t *obj = lv_obj_create(0);
    objects.conn = obj;
    lv_obj_set_pos(obj, 3300, 0);
    lv_obj_set_size(obj, 1024, 600);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x10151c), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // top_conn
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.top_conn = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 1024, 56);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x16202e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // lbl_title_conn
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_title_conn = obj;
                    lv_obj_set_pos(obj, 16, 12);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_head26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ЛИНИЯ RS-485 / MODBUS RTU");
                }
            }
        }
        {
            // line
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.line = obj;
            lv_obj_set_pos(obj, 16, 68);
            lv_obj_set_size(obj, 992, 380);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a2230), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // lbl_c_state
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_state = obj;
                    lv_obj_set_pos(obj, 20, 16);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xdc2626), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_head26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_cfg_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cfg_cap = obj;
                    lv_obj_set_pos(obj, 20, 68);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "НАСТРОЙКА ДРАЙВЕРА");
                }
                {
                    // lbl_c_cfg
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cfg = obj;
                    lv_obj_set_pos(obj, 20, 92);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_cfg2
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cfg2 = obj;
                    lv_obj_set_pos(obj, 20, 112);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_cnt_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cnt_cap = obj;
                    lv_obj_set_pos(obj, 20, 150);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "СЧЁТЧИКИ ОБМЕНА");
                }
                {
                    // lbl_c_cnt
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cnt = obj;
                    lv_obj_set_pos(obj, 20, 174);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_cnt2
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cnt2 = obj;
                    lv_obj_set_pos(obj, 20, 194);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_cmd_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cmd_cap = obj;
                    lv_obj_set_pos(obj, 520, 68);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ПОСЛЕДНЯЯ КОМАНДА");
                }
                {
                    // lbl_c_cmd
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cmd = obj;
                    lv_obj_set_pos(obj, 520, 92);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_cmd2
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_cmd2 = obj;
                    lv_obj_set_pos(obj, 520, 112);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_err_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_err_cap = obj;
                    lv_obj_set_pos(obj, 520, 150);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ПОСЛЕДНИЙ СБОЙ ОБМЕНА");
                }
                {
                    // lbl_c_err
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_err = obj;
                    lv_obj_set_pos(obj, 520, 174);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_frame
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_frame = obj;
                    lv_obj_set_pos(obj, 520, 194);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // lbl_c_note
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_c_note = obj;
                    lv_obj_set_pos(obj, 20, 250);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0x9aa7b8), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Параметры линии задаются в menuconfig и в прошивке не меняются.");
                }
            }
        }
        {
            // btn_c_reset
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.btn_c_reset = obj;
            lv_obj_set_pos(obj, 16, 460);
            lv_obj_set_size(obj, 300, 44);
            lv_obj_add_event_cb(obj, action_counters_reset, LV_EVENT_CLICKED, (void *)0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_c_reset_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.btn_c_reset_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_body16, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "СБРОС СЧЁТЧИКОВ");
                }
            }
        }
        {
            // nav_conn
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.nav_conn = obj;
            lv_obj_set_pos(obj, 0, 516);
            lv_obj_set_size(obj, 1024, 84);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x16202e), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // btn_nav_main_conn
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_main_conn = obj;
                    lv_obj_set_pos(obj, 8, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_main, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_main_conn_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_main_conn_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ГЛАВНАЯ");
                        }
                    }
                }
                {
                    // btn_nav_params_conn
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_params_conn = obj;
                    lv_obj_set_pos(obj, 262, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_params, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_params_conn_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_params_conn_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ПАРАМЕТРЫ");
                        }
                    }
                }
                {
                    // btn_nav_alarm_conn
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_alarm_conn = obj;
                    lv_obj_set_pos(obj, 516, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_alarm, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1e293b), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_alarm_conn_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_alarm_conn_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "АВАРИИ");
                        }
                    }
                }
                {
                    // btn_nav_conn_conn
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.btn_nav_conn_conn = obj;
                    lv_obj_set_pos(obj, 770, 12);
                    lv_obj_set_size(obj, 246, 60);
                    lv_obj_add_event_cb(obj, action_nav_conn, LV_EVENT_CLICKED, (void *)0);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x2563eb), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // btn_nav_conn_conn_lbl
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.btn_nav_conn_conn_lbl = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &ui_font_label20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ЛИНИЯ");
                        }
                    }
                }
            }
        }
    }
    
    tick_screen_conn();
}

void tick_screen_conn() {
    {
        const char *new_val = get_var_c_state_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_state);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_state;
            lv_label_set_text(objects.lbl_c_state, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_cfg1_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_cfg);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_cfg;
            lv_label_set_text(objects.lbl_c_cfg, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_cfg2_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_cfg2);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_cfg2;
            lv_label_set_text(objects.lbl_c_cfg2, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_cnt1_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_cnt);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_cnt;
            lv_label_set_text(objects.lbl_c_cnt, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_cnt2_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_cnt2);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_cnt2;
            lv_label_set_text(objects.lbl_c_cnt2, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_cmd1_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_cmd);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_cmd;
            lv_label_set_text(objects.lbl_c_cmd, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_cmd2_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_cmd2);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_cmd2;
            lv_label_set_text(objects.lbl_c_cmd2, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_err_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_err);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_err;
            lv_label_set_text(objects.lbl_c_err, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_c_frame_txt();
        const char *cur_val = lv_label_get_text(objects.lbl_c_frame);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.lbl_c_frame;
            lv_label_set_text(objects.lbl_c_frame, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    tick_screen_params,
    tick_screen_alarm,
    tick_screen_conn,
};
void tick_screen(int screen_index) {
    if (screen_index >= 0 && screen_index < 4) {
        tick_screen_funcs[screen_index]();
    }
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen(screenId - 1);
}

//
// Fonts
//

ext_font_desc_t fonts[] = {
    { "body16", &ui_font_body16 },
    { "label20", &ui_font_label20 },
    { "head26", &ui_font_head26 },
    { "big56", &ui_font_big56 },
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;

//
//
//

void create_screens() {

// Set default LVGL theme
    lv_display_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);
    
    // Initialize screens
    // Create screens
    create_screen_main();
    create_screen_params();
    create_screen_alarm();
    create_screen_conn();
}