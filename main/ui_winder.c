#include "ui_winder.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "idm20_encoder.h"
#include "vfd_easydrive.h"
#include "fonts/cyrillic_fonts.h"

static const char *TAG = "UI_WINDER";

// Кириллические шрифты DejaVu Sans (16, 24, 32 px) и числовые шрифты Consolas
#define FONT_RU                   (&lv_font_dejavu_16)
#define FONT_TITLE                (&lv_font_dejavu_24)
#define FONT_LARGE                (&lv_font_dejavu_32)
#define FONT_DIGIT_HERO           (&lv_font_digits_120)  // 120px Consolas Bold (одометр увеличен ровно в 2 раза!)
#define FONT_DIGIT_INPUT          (&lv_font_digits_48)   // 48px Consolas Bold (крупные цифры уставки и бухты)
#define FONT_DIGIT_MID            (&lv_font_dejavu_24)
#define FONT_DIGIT_SMALL          (&lv_font_dejavu_16)

// =========================================================================
// ЦВЕТОВАЯ ПАЛИТРА ПРЕМИАЛЬНОГО ИНДУСТРИАЛЬНОГО ДИЗАЙНА
// =========================================================================
#define COLOR_SCREEN_BG      lv_color_hex(0x101418) // Глубокий темный фон матрицы
#define COLOR_TAB_BAR_BG     lv_color_hex(0x181F26) // Фон верхней панели вкладок
#define COLOR_TAB_ACTIVE     lv_color_hex(0x26323D) // Фон активной вкладки
#define COLOR_CARD_BG        lv_color_hex(0x1E262E) // Основные карточки
#define COLOR_CARD_BORDER    lv_color_hex(0x2C3844) // Контур карточек
#define COLOR_BOX_BG         lv_color_hex(0x141920) // Вложенные боксы параметров
#define COLOR_BOX_BORDER     lv_color_hex(0x28323E) // Контур вложенных боксов
#define COLOR_BAR_BG         lv_color_hex(0x1E2630) // Подложка шкалы
#define COLOR_BAR_BORDER     lv_color_hex(0x323F4E) // Контур шкалы

#define COLOR_CYAN           lv_color_hex(0x00E5FF) // Неоновый циан
#define COLOR_AMBER          lv_color_hex(0xFFD54F) // Янтарный акцент
#define COLOR_ORANGE         lv_color_hex(0xEF6C00) // Оранжевый (JOG)
#define COLOR_GREEN_DARK     lv_color_hex(0x2E7D32) // Зеленый для кнопок
#define COLOR_GREEN_BORDER   lv_color_hex(0x4CAF50) // Яркая зеленая рамка
#define COLOR_GREEN_BRIGHT   lv_color_hex(0x00E676) // Неоновый зеленый LED
#define COLOR_RED_DARK       lv_color_hex(0xC62828) // Красный для кнопок
#define COLOR_RED_BORDER     lv_color_hex(0xEF5350) // Яркая красная рамка
#define COLOR_RED_BRIGHT     lv_color_hex(0xFF1744) // Неоновый красный LED
#define COLOR_BLUE_DARK      lv_color_hex(0x1565C0) // Синий
#define COLOR_BLUE_BORDER    lv_color_hex(0x42A5F5) // Синяя рамка
#define COLOR_BLUE_BRIGHT    lv_color_hex(0x29B6F6) // Голубой LED
#define COLOR_GREY_BTN       lv_color_hex(0x37474F) // Кнопки шага
#define COLOR_GREY_BORDER    lv_color_hex(0x455A64) // Рамка кнопок шага
#define COLOR_GREY_OFF       lv_color_hex(0x455A64) // Выключенный LED

#define COLOR_TEXT_MAIN      lv_color_hex(0xECEFF1) // Основной текст
#define COLOR_TEXT_DIM       lv_color_hex(0x90A4AE) // Подписи
#define COLOR_TEXT_MUTED     lv_color_hex(0x607D8B) // Мелкие шкалы

// =========================================================================
// ДИНАМИЧЕСКИЕ ВИДЖЕТЫ LVGL
// =========================================================================
// Единая цифровая клавиатура
static lv_obj_t *s_numpad_kb = NULL;

// Вкладка 1: Главный пост намотки
static lv_obj_t *led_status_banner;
static lv_obj_t *lbl_status_banner;
static lv_obj_t *lbl_cur_len_val;
static lv_obj_t *lbl_fact_prod;          // Выработка за смену
static lv_obj_t *ta_target_len;          // Поле ввода уставки (крупные цифры, до 2000 м)
static lv_obj_t *ta_spool_init_len;      // Поле ввода начального размера бухты (до 2000 м)
static lv_obj_t *lbl_spool_rem_val;      // Остаток кабеля в бобине
static lv_obj_t *lbl_spool_status;       // Статус достаточности остатка бобины
static lv_obj_t *bar_spool_rem;          // Шкала остатка в бобине (%)
static lv_obj_t *lbl_speed_val;
static lv_obj_t *bar_progress;
static lv_obj_t *lbl_bar_percent;
static lv_obj_t *lbl_telemetry_line;
static lv_obj_t *lbl_link_banner;        // «НЕТ СВЯЗИ С ПЧ» поверх всех цифр

// Двухшаговый пуск. Эта панель отдаёт «призрачные» касания (одиночные точки
// без касания), поэтому одно нажатие клавиши ПУСК только взводит автомат,
// а ход даёт второе нажатие в окно 3 с.
static lv_obj_t *s_btn_start;
static lv_obj_t *s_btn_start_lbl;
static int64_t   s_arm_until_us;

// Вкладка 2: Калибровка и параметры ИД-2
static lv_obj_t *lbl_idm_params_col1;
static lv_obj_t *lbl_idm_params_col2;
static lv_obj_t *lbl_cal_monitor_cur;
static lv_obj_t *ta_ref_len;             // Поле ввода длины эталона L_эт
static lv_obj_t *lbl_cal_monitor_k;
static lv_obj_t *lbl_cal_status_msg;
static char s_cal_status_text[128] = "Готов к замеру. Протяните кабель и нажмите «Применить калибровку»";

// Вкладка 3: Автоматика
static lv_obj_t *lbl_auto_items[6];

// Вкладка 4: Премиальный дашборд привода Easydrive Mini-S
static lv_obj_t *lbl_vfd_freq_val;
static lv_obj_t *bar_vfd_freq;
static lv_obj_t *lbl_vfd_curr_val;
static lv_obj_t *bar_vfd_curr;
static lv_obj_t *lbl_vfd_dc_val;
static lv_obj_t *lbl_vfd_rpm_val;
static lv_obj_t *lbl_vfd_pwr_val;

static lv_obj_t *led_run;
static lv_obj_t *lbl_badge_run_txt;
static lv_obj_t *led_ready;
static lv_obj_t *led_alarm;
static lv_obj_t *lbl_badge_alarm_txt;
static lv_obj_t *led_comm;

static lv_obj_t *lbl_vfd_diag_status;
static lv_obj_t *lbl_vfd_diag_fault;
static lv_obj_t *lbl_vfd_diag_temp;
static lv_obj_t *lbl_vfd_diag_packets;
static lv_obj_t *lbl_vfd_diag_latency;

static lv_obj_t *ta_test_freq;           // Поле ввода частоты теста
static lv_obj_t *lbl_vfd_test_status;
static float s_test_freq_hz = 35.0f;
static bool s_is_test_running = false;

// =========================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ СТИЛИЗАЦИИ
// =========================================================================
static lv_obj_t *create_hmi_card(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static lv_obj_t *create_inner_box(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_color(box, COLOR_BOX_BG, 0);
    lv_obj_set_style_border_color(box, COLOR_BOX_BORDER, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_pad_all(box, 8, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

static lv_obj_t *create_styled_bar(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t ind_color, int32_t min, int32_t max)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, min, max);

    lv_obj_set_style_bg_color(bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, COLOR_BAR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);

    lv_obj_set_style_bg_color(bar, ind_color, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
    return bar;
}

static lv_obj_t *create_styled_btn(lv_obj_t *parent, int32_t w, int32_t h, 
                                   lv_color_t bg_color, lv_color_t border_color, 
                                   int32_t radius, const char *text, const lv_font_t *font,
                                   lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_border_color(btn, border_color, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, radius, 0);
    lv_obj_set_style_shadow_width(btn, 6, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_40, 0);

    // Сброс широких отступов темы LVGL 9 для кнопок
    lv_obj_set_style_pad_all(btn, 2, 0);

    lv_obj_set_style_transform_height(btn, -2, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 2, LV_STATE_PRESSED);

    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, font ? font : FONT_RU, 0);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_MAIN, 0);
    lv_label_set_text(lbl, text);

    // Центрирование и защита от выхода за пределы кнопки
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);

    lv_obj_center(lbl);

    return btn;
}

static lv_obj_t *create_led_dot(lv_obj_t *parent, int32_t diameter, lv_color_t color)
{
    lv_obj_t *led = lv_obj_create(parent);
    lv_obj_set_size(led, diameter, diameter);
    lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(led, color, 0);
    lv_obj_set_style_border_color(led, lv_color_hex(0x181F26), 0);
    lv_obj_set_style_border_width(led, 1, 0);
    lv_obj_set_style_pad_all(led, 0, 0);
    lv_obj_remove_flag(led, LV_OBJ_FLAG_SCROLLABLE);
    return led;
}

/**
 * @brief Создание стилизованного текстового поля для числового ввода через цифровую клавиатуру
 */
static lv_obj_t *create_numeric_textarea(lv_obj_t *parent, int32_t w, int32_t h, lv_color_t border_color, const lv_font_t *font)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, w, h);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 8);
    lv_textarea_set_accepted_chars(ta, "0123456789.-");

    lv_obj_set_style_bg_color(ta, COLOR_BOX_BG, 0);
    lv_obj_set_style_border_color(ta, border_color, 0);
    lv_obj_set_style_border_width(ta, 2, 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_text_color(ta, border_color, 0);
    lv_obj_set_style_text_font(ta, font ? font : FONT_TITLE, 0);
    lv_obj_set_style_pad_top(ta, 4, 0);
    lv_obj_set_style_pad_bottom(ta, 4, 0);
    lv_obj_set_style_pad_left(ta, 8, 0);
    lv_obj_set_style_pad_right(ta, 8, 0);

    return ta;
}

// =========================================================================
// ОБРАБОТЧИКИ ЦИФРОВОЙ КЛАВИАТУРЫ LVGL
// =========================================================================
static void numpad_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_target(e);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_t *ta = lv_keyboard_get_textarea(kb);
        if (ta && code == LV_EVENT_READY) {
            const char *text = lv_textarea_get_text(ta);
            float val = (float)atof(text);

            if (ta == ta_target_len) {
                if (val < 1.0f) val = 1.0f;
                if (val > 2000.0f) val = 2000.0f; // Ограничение: не более 2000 м
                g_winder.target_length_m = val;
                ESP_LOGI(TAG, "Клавиатура -> Уставка длины: %.2f м", g_winder.target_length_m);
            } else if (ta == ta_spool_init_len) {
                if (val < 0.0f) val = 0.0f;
                if (val > 2000.0f) val = 2000.0f; // Ограничение: не более 2000 м
                g_winder.spool_initial_len_m = val;
                float rem = g_winder.spool_initial_len_m - g_winder.current_length_m;
                g_winder.spool_remaining_len_m = (rem > 0.0f) ? rem : 0.0f;
                idm20_save_settings_nvs();
                ESP_LOGI(TAG, "Клавиатура -> Бухта (нач.): %.2f м (Остаток: %.2f м)",
                         g_winder.spool_initial_len_m, g_winder.spool_remaining_len_m);
            } else if (ta == ta_ref_len) {
                if (val < 0.1f) val = 0.1f;
                g_winder.ref_length_m = val;
                ESP_LOGI(TAG, "Клавиатура -> Эталон L_эт: %.2f м", g_winder.ref_length_m);
                idm20_save_settings_nvs();
            } else if (ta == ta_test_freq) {
                if (val < 1.0f) val = 1.0f;
                if (val > 60.0f) val = 60.0f;
                s_test_freq_hz = val;
                if (s_is_test_running) {
                    vfd_cmd_set_frequency(s_test_freq_hz);
                }
                ESP_LOGI(TAG, "Клавиатура -> Частота теста: %.1f Гц", s_test_freq_hz);
            }
        }

        // Скрыть цифровую клавиатуру
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        if (ta) {
            lv_obj_remove_state(ta, LV_STATE_FOCUSED);
        }
    }
}

static void numpad_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        if (s_numpad_kb) {
            lv_keyboard_set_textarea(s_numpad_kb, ta);
            lv_obj_clear_flag(s_numpad_kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_numpad_kb);
        }
    }
}


// =========================================================================
// ОБРАБОТЧИКИ НАЖАТИЙ НА СЕНСОРНОМ ЭКРАНЕ
// =========================================================================
/* Пуск разрешён только когда привод нас «видит» и не в аварии. Без этого
 * намотка идёт вслепую: панель показывает уставку, а привод её не получал. */
static bool start_allowed(void)
{
    if (vfd_link_is_down()) {
        ESP_LOGE(TAG, "ПУСК запрещён: связь с частотником потеряна");
        return false;
    }
    if (g_vfd.is_fault) {
        ESP_LOGE(TAG, "ПУСК запрещён: у привода авария Er%02u, сначала сброс",
                 g_vfd.fault_code);
        return false;
    }
    return true;
}

static void arm_release(void)
{
    s_arm_until_us = 0;
    if (s_btn_start_lbl) lv_label_set_text(s_btn_start_lbl, "ПУСК");
    if (s_btn_start) lv_obj_set_style_bg_color(s_btn_start, COLOR_GREEN_DARK, 0);
}

static void btn_start_cb(lv_event_t *e)
{
    if (g_winder.current_length_m >= g_winder.target_length_m) {
        ESP_LOGW(TAG, "Невозможно запустить: факт >= уставки. Сделайте сброс или увеличьте уставку.");
        return;
    }
    if (!start_allowed()) return;

    int64_t now = esp_timer_get_time();
    if (s_arm_until_us == 0 || now > s_arm_until_us) {
        s_arm_until_us = now + 3 * 1000 * 1000;
        if (s_btn_start_lbl) lv_label_set_text(s_btn_start_lbl, "ПУСК? ЕЩЁ РАЗ");
        if (s_btn_start) lv_obj_set_style_bg_color(s_btn_start, COLOR_AMBER, 0);
        ESP_LOGW(TAG, "пуск взведён: подтвердите нажатием в течение 3 с");
        return;
    }
    arm_release();
    ESP_LOGI(TAG, "Нажата кнопка ПУСК (подтверждено)");
    g_winder.state = STATE_RUNNING;
    vfd_cmd_run_fwd(g_winder.base_freq_hz);
}

static void btn_stop_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Нажата кнопка СТОП");
    arm_release();
    g_winder.state = STATE_IDLE;
    vfd_cmd_stop();
}

static void btn_reset_len_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Нажата кнопка СБРОС длины");
    idm20_reset_length();
}

static void btn_jog_cb(lv_event_t *e)
{
    if (g_winder.state == STATE_IDLE || g_winder.state == STATE_DONE) {
        if (!start_allowed()) return;
        ESP_LOGI(TAG, "Режим JOG (ручная заправка)");
        arm_release();
        g_winder.state = STATE_JOG;
        vfd_cmd_jog(false, g_winder.creep_freq_hz);
    } else if (g_winder.state == STATE_JOG) {
        g_winder.state = STATE_IDLE;
        vfd_cmd_stop();
    }
}

static void btn_target_adj_cb(lv_event_t *e)
{
    intptr_t delta = (intptr_t)lv_event_get_user_data(e);
    g_winder.target_length_m += (float)delta;
    if (g_winder.target_length_m < 1.0f) g_winder.target_length_m = 1.0f;
    if (g_winder.target_length_m > 2000.0f) g_winder.target_length_m = 2000.0f;
    ESP_LOGI(TAG, "Изменение уставки на %+d м -> %.2f м", (int)delta, g_winder.target_length_m);
}

static void btn_ref_len_adj_cb(lv_event_t *e)
{
    intptr_t delta_cm = (intptr_t)lv_event_get_user_data(e);
    g_winder.ref_length_m += (float)delta_cm / 100.0f;
    if (g_winder.ref_length_m < 0.1f) g_winder.ref_length_m = 0.1f;
    ESP_LOGI(TAG, "Изменение эталона L_эт -> %.2f м", g_winder.ref_length_m);
    idm20_save_settings_nvs();
}

static void btn_apply_calibration_cb(lv_event_t *e)
{
    if (g_winder.current_length_m < 0.05f) {
        snprintf(s_cal_status_text, sizeof(s_cal_status_text), "ОШИБКА: счетчик < 0.05м! Сначала протяните кабель");
        return;
    }
    float old_k = g_winder.cal_coeff;
    if (idm20_calibrate_by_reference(g_winder.ref_length_m)) {
        snprintf(s_cal_status_text, sizeof(s_cal_status_text), 
                 "УСПЕХ! K_кал: %.5f -> %.5f (сохранено во Flash NVS)", old_k, g_winder.cal_coeff);
    } else {
        snprintf(s_cal_status_text, sizeof(s_cal_status_text), "ОШИБКА КАЛИБРОВКИ!");
    }
}

static void btn_reset_cal_cb(lv_event_t *e)
{
    idm20_reset_calibration();
    snprintf(s_cal_status_text, sizeof(s_cal_status_text), "Коэффициент K_кал сброшен в номинал 1.00000");
}

static void btn_reset_prod_cb(lv_event_t *e)
{
    idm20_reset_production();
    snprintf(s_cal_status_text, sizeof(s_cal_status_text), "Суммарный счетчик выработки сброшен в 0.00 м");
}

static void btn_idm_adj_cb(lv_event_t *e)
{
    intptr_t code = (intptr_t)lv_event_get_user_data(e);
    switch (code) {
        case 1:  g_winder.cable_diam_mm += 0.5f; break;
        case -1: if (g_winder.cable_diam_mm > 0.5f) g_winder.cable_diam_mm -= 0.5f; break;
        case 2:  g_winder.teeth_z += 1; g_winder.encoder_ppr_x4 = g_winder.teeth_z * 4; break;
        case -2: if (g_winder.teeth_z > 1) { g_winder.teeth_z -= 1; g_winder.encoder_ppr_x4 = g_winder.teeth_z * 4; } break;
        default: break;
    }
    g_winder.current_length_m = idm20_calculate_length_m(g_winder.total_pulses, &g_winder);
    idm20_save_settings_nvs();
}

static void btn_vfd_set_freq_cb(lv_event_t *e)
{
    intptr_t f = (intptr_t)lv_event_get_user_data(e);
    s_test_freq_hz = (float)f;
    if (s_is_test_running) {
        vfd_cmd_set_frequency(s_test_freq_hz);
    }
    ESP_LOGI(TAG, "Установка частоты теста -> %.1f Гц", s_test_freq_hz);
}

static void btn_vfd_adj_freq_cb(lv_event_t *e)
{
    intptr_t delta = (intptr_t)lv_event_get_user_data(e);
    s_test_freq_hz += (float)delta;
    if (s_test_freq_hz < 1.0f) s_test_freq_hz = 1.0f;
    if (s_test_freq_hz > 60.0f) s_test_freq_hz = 60.0f;
    if (s_is_test_running) {
        vfd_cmd_set_frequency(s_test_freq_hz);
    }
    ESP_LOGI(TAG, "Подстройка частоты теста -> %.1f Гц", s_test_freq_hz);
}

static void btn_vfd_test_start_cb(lv_event_t *e)
{
    if (!start_allowed()) return;
    s_is_test_running = true;
    vfd_cmd_run_fwd(s_test_freq_hz);
    ESP_LOGI(TAG, "Сервисный тест ПУСК привода (%.1f Гц)", s_test_freq_hz);
}

static void btn_vfd_test_stop_cb(lv_event_t *e)
{
    s_is_test_running = false;
    vfd_cmd_stop();
    ESP_LOGI(TAG, "Сервисный тест СТОП привода");
}

static void btn_vfd_reset_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Сброс ошибки частотника через UI");
    vfd_cmd_reset_fault();
    if (g_winder.state == STATE_ALARM_VFD || g_winder.state == STATE_ALARM_BREAK || g_winder.state == STATE_ALARM_JAM) {
        g_winder.state = STATE_IDLE;
    }
}

// =========================================================================
// ТАЙМЕР ПЕРИОДИЧЕСКОГО ОБНОВЛЕНИЯ ДАННЫХ В UI (10 Гц / 100 мс)
// =========================================================================
static void ui_update_timer_cb(lv_timer_t *timer)
{
    char str_buf[256];

    // 0. Окно взвода пуска живёт 3 с; баннер линии — на всё время молчания.
    if (s_arm_until_us && esp_timer_get_time() > s_arm_until_us) arm_release();

    if (lbl_link_banner) {
        if (vfd_link_is_down()) {
            snprintf(str_buf, sizeof(str_buf),
                     "НЕТ СВЯЗИ С ПЧ | TX=GPIO%u RX=GPIO%u | битых %lu таймаутов %lu",
                     g_vfd.uart_tx_gpio, g_vfd.uart_rx_gpio,
                     (unsigned long)g_vfd.crc_err, (unsigned long)g_vfd.timeouts);
            lv_label_set_text(lbl_link_banner, str_buf);
            lv_obj_set_style_bg_color(lbl_link_banner, COLOR_RED_DARK, 0);
            lv_obj_clear_flag(lbl_link_banner, LV_OBJ_FLAG_HIDDEN);
        } else if (g_vfd.link == VFD_LINK_DEGRADED) {
            snprintf(str_buf, sizeof(str_buf), "ЛИНИЯ 485 ДЕГРАДИРУЕТ | ретраев %lu",
                     (unsigned long)g_vfd.retries);
            lv_label_set_text(lbl_link_banner, str_buf);
            lv_obj_set_style_bg_color(lbl_link_banner, COLOR_ORANGE, 0);
            lv_obj_clear_flag(lbl_link_banner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lbl_link_banner, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 1. Статусный баннер и светодиод
    const char *state_str = "ОЖИДАНИЕ ПУСКА";
    lv_color_t led_col = COLOR_BLUE_BRIGHT;

    switch (g_winder.state) {
        case STATE_IDLE:
            state_str = "СТАТУС: ГОТОВ К РАБОТЕ (ОЖИДАНИЕ ПУСКА)";
            led_col = COLOR_BLUE_BRIGHT;
            break;
        case STATE_RUNNING:
            state_str = "СТАТУС: ИДЕТ НАМОТКА (РАБОЧАЯ СКОРОСТЬ)";
            led_col = COLOR_GREEN_BRIGHT;
            break;
        case STATE_SLOWDOWN:
            state_str = "СТАТУС: ДОТЯЖКА ПЕРЕД ЦЕЛЬЮ (МАЛАЯ СКОРОСТЬ)";
            led_col = COLOR_AMBER;
            break;
        case STATE_JOG:
            state_str = "СТАТУС: РУЧНАЯ ЗАПРАВКА (ТОЛЧКОВЫЙ РЕЖИМ)";
            led_col = COLOR_ORANGE;
            break;
        case STATE_DONE:
            state_str = "СТАТУС: НАМОТКА УСПЕШНО ЗАВЕРШЕНА!";
            led_col = COLOR_CYAN;
            break;
        case STATE_ALARM_BREAK:
            state_str = "АВАРИЯ: ОБРЫВ КАБЕЛЯ / КОНЧИЛАСЬ БУХТА!";
            led_col = COLOR_RED_BRIGHT;
            break;
        case STATE_ALARM_JAM:
            state_str = "АВАРИЯ: ЗАКЛИНИВАНИЕ КАБЕЛЯ (ПЕРЕГРУЗ ПО ТОКУ)!";
            led_col = COLOR_RED_BRIGHT;
            break;
        case STATE_ALARM_VFD:
            state_str = "АВАРИЯ: ОШИБКА ЧАСТОТНОГО ПРЕОБРАЗОВАТЕЛЯ!";
            led_col = COLOR_RED_BRIGHT;
            break;
    }

    if (lbl_status_banner) {
        lv_label_set_text(lbl_status_banner, state_str);
    }
    if (led_status_banner) {
        lv_obj_set_style_bg_color(led_status_banner, led_col, 0);
    }

    // 2. Вкладка 1: Метраж, скорость, уставка, подающая бобина и прогресс
    if (lbl_cur_len_val) {
        // Огромный одометр 120px (в 2 раза больше!)
        snprintf(str_buf, sizeof(str_buf), "%07.2f", g_winder.current_length_m);
        lv_label_set_text(lbl_cur_len_val, str_buf);
    }

    // Обновляем текстовое поле уставки только если оператор сейчас не набирает с клавиатуры
    if (ta_target_len && !lv_obj_has_state(ta_target_len, LV_STATE_FOCUSED)) {
        snprintf(str_buf, sizeof(str_buf), "%.2f", g_winder.target_length_m);
        lv_textarea_set_text(ta_target_len, str_buf);
    }

    // Обновляем поле начального размера бухты только если оператор не редактирует
    if (ta_spool_init_len && !lv_obj_has_state(ta_spool_init_len, LV_STATE_FOCUSED)) {
        snprintf(str_buf, sizeof(str_buf), "%.2f", g_winder.spool_initial_len_m);
        lv_textarea_set_text(ta_spool_init_len, str_buf);
    }

    // Отображение текущего остатка кабеля в подающей бобине
    if (lbl_spool_rem_val) {
        snprintf(str_buf, sizeof(str_buf), "%.2f м", g_winder.spool_remaining_len_m);
        lv_label_set_text(lbl_spool_rem_val, str_buf);

        bool is_enough = (g_winder.spool_remaining_len_m >= g_winder.target_length_m);
        if (g_winder.spool_remaining_len_m <= 0.05f) {
            lv_obj_set_style_text_color(lbl_spool_rem_val, COLOR_RED_BRIGHT, 0);
            if (lbl_spool_status) {
                lv_label_set_text(lbl_spool_status, "× 0 (ПУСТА)");
                lv_obj_set_style_text_color(lbl_spool_status, COLOR_RED_BRIGHT, 0);
            }
        } else if (!is_enough) {
            lv_obj_set_style_text_color(lbl_spool_rem_val, COLOR_AMBER, 0);
            if (lbl_spool_status) {
                snprintf(str_buf, sizeof(str_buf), "× 0 (< уставки)");
                lv_label_set_text(lbl_spool_status, str_buf);
                lv_obj_set_style_text_color(lbl_spool_status, COLOR_AMBER, 0);
            }
        } else {
            lv_obj_set_style_text_color(lbl_spool_rem_val, COLOR_GREEN_BRIGHT, 0);
            if (lbl_spool_status) {
                int batches_left = (int)(g_winder.spool_remaining_len_m / g_winder.target_length_m);
                snprintf(str_buf, sizeof(str_buf), "× %d", batches_left);
                lv_label_set_text(lbl_spool_status, str_buf);
                lv_obj_set_style_text_color(lbl_spool_status, COLOR_GREEN_BRIGHT, 0);
            }
        }
    }
    if (bar_spool_rem) {
        int spool_pct = 0;
        if (g_winder.spool_initial_len_m > 0.01f) {
            spool_pct = (int)((g_winder.spool_remaining_len_m / g_winder.spool_initial_len_m) * 100.0f);
            if (spool_pct > 100) spool_pct = 100;
            if (spool_pct < 0) spool_pct = 0;
        }
        lv_bar_set_value(bar_spool_rem, spool_pct, LV_ANIM_OFF);
    }

    if (lbl_fact_prod) {
        snprintf(str_buf, sizeof(str_buf), "Выработка смены: %.1f м", (float)g_winder.total_production_m);
        lv_label_set_text(lbl_fact_prod, str_buf);
    }

    if (lbl_speed_val) {
        snprintf(str_buf, sizeof(str_buf), "⚡ Скорость: %.1f м/мин", g_winder.line_speed_m_min);
        lv_label_set_text(lbl_speed_val, str_buf);
    }

    int progress_pct = 0;
    if (g_winder.target_length_m > 0.01f) {
        progress_pct = (int)((g_winder.current_length_m / g_winder.target_length_m) * 100.0f);
        if (progress_pct > 100) progress_pct = 100;
        if (progress_pct < 0) progress_pct = 0;
    }

    if (bar_progress) {
        lv_bar_set_value(bar_progress, progress_pct, LV_ANIM_ON);
    }
    if (lbl_bar_percent) {
        snprintf(str_buf, sizeof(str_buf), "%d %%", progress_pct);
        lv_label_set_text(lbl_bar_percent, str_buf);
    }

    if (lbl_telemetry_line) {
        snprintf(str_buf, sizeof(str_buf),
            "Скорость: %5.1f м/мин  |  ПЧ Easydrive: %4.1f Гц  |  Ток: %4.2f А  |  Импульсов: %lld",
            g_winder.line_speed_m_min,
            g_vfd.out_freq_hz,
            g_vfd.out_cur_a,
            (long long)g_winder.total_pulses);
        lv_label_set_text(lbl_telemetry_line, str_buf);
    }

    // 3. Вкладка 2: Параметры ИД-2 и калибровка
    float eff_diam_mm = g_winder.roller_diam_mm + (g_winder.bend_factor * g_winder.cable_diam_mm);
    uint32_t ppr = g_winder.encoder_ppr_x4 ? g_winder.encoder_ppr_x4 : (g_winder.teeth_z * 4);
    float step_per_inc_m = ((float)M_PI * (eff_diam_mm / 1000.0f) / (float)ppr) * g_winder.cal_coeff;

    if (lbl_idm_params_col1) {
        snprintf(str_buf, sizeof(str_buf),
            "1. Мерное колесо: D = %.1f мм (L_об = %.3f мм)\n"
            "2. Диск датчиков: Z = %u зубьев (PPR = %lu, 4x)\n"
            "3. Сенсоры: 2 шт (оптопары/индуктивные)",
            g_winder.roller_diam_mm,
            (float)M_PI * g_winder.roller_diam_mm,
            g_winder.teeth_z,
            (unsigned long)ppr);
        lv_label_set_text(lbl_idm_params_col1, str_buf);
    }

    if (lbl_idm_params_col2) {
        snprintf(str_buf, sizeof(str_buf),
            "4. Кабель: D = %.1f мм (D_эфф = %.2f мм)\n"
            "5. Шаг имп.: %.5f м (%.2f мм)\n"
            "6. Выработка: %.1f м (K = %.5f)",
            g_winder.cable_diam_mm,
            eff_diam_mm,
            step_per_inc_m,
            step_per_inc_m * 1000.0f,
            (float)g_winder.total_production_m,
            g_winder.cal_coeff);
        lv_label_set_text(lbl_idm_params_col2, str_buf);
    }

    if (lbl_cal_monitor_cur) {
        snprintf(str_buf, sizeof(str_buf), "Факт: %05.2f м", g_winder.current_length_m);
        lv_label_set_text(lbl_cal_monitor_cur, str_buf);
    }

    if (ta_ref_len && !lv_obj_has_state(ta_ref_len, LV_STATE_FOCUSED)) {
        snprintf(str_buf, sizeof(str_buf), "%.2f", g_winder.ref_length_m);
        lv_textarea_set_text(ta_ref_len, str_buf);
    }

    if (lbl_cal_monitor_k) {
        snprintf(str_buf, sizeof(str_buf), "K_кал: %.5f", g_winder.cal_coeff);
        lv_label_set_text(lbl_cal_monitor_k, str_buf);
    }
    if (lbl_cal_status_msg) {
        lv_label_set_text(lbl_cal_status_msg, s_cal_status_text);
    }

    // 4. Вкладка 4: дашборд Easydrive Mini-S — только то, что реально прочитано

    if (lbl_vfd_freq_val) {
        snprintf(str_buf, sizeof(str_buf), "%.1f Гц", g_vfd.out_freq_hz);
        lv_label_set_text(lbl_vfd_freq_val, str_buf);
    }
    if (bar_vfd_freq) {
        int freq_val = (int)(g_vfd.out_freq_hz * 10.0f);
        if (freq_val > 500) freq_val = 500;
        lv_bar_set_value(bar_vfd_freq, freq_val, LV_ANIM_OFF);
    }

    if (lbl_vfd_curr_val) {
        snprintf(str_buf, sizeof(str_buf), "%.2f А", g_vfd.out_cur_a);
        lv_label_set_text(lbl_vfd_curr_val, str_buf);

        if (g_vfd.out_cur_a > 5.5f) {
            lv_obj_set_style_text_color(lbl_vfd_curr_val, COLOR_RED_BRIGHT, 0);
            if (bar_vfd_curr) lv_obj_set_style_bg_color(bar_vfd_curr, COLOR_RED_BRIGHT, LV_PART_INDICATOR);
        } else if (g_vfd.out_cur_a > 4.0f) {
            lv_obj_set_style_text_color(lbl_vfd_curr_val, COLOR_AMBER, 0);
            if (bar_vfd_curr) lv_obj_set_style_bg_color(bar_vfd_curr, COLOR_AMBER, LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_text_color(lbl_vfd_curr_val, COLOR_GREEN_BRIGHT, 0);
            if (bar_vfd_curr) lv_obj_set_style_bg_color(bar_vfd_curr, COLOR_GREEN_BRIGHT, LV_PART_INDICATOR);
        }
    }
    if (bar_vfd_curr) {
        int curr_val = (int)(g_vfd.out_cur_a * 100.0f);
        if (curr_val > 650) curr_val = 650;
        lv_bar_set_value(bar_vfd_curr, curr_val, LV_ANIM_OFF);
    }

    /* Обороты и мощность — из привода (d-04, d-03), а не пересчёт «от 50 Гц»:
     * на шкиве/редукторе эти две цифры разошлись бы, и панель врала бы. */
    if (lbl_vfd_dc_val) {
        snprintf(str_buf, sizeof(str_buf), "%.0f В (звено)", g_vfd.bus_volt_v);
        lv_label_set_text(lbl_vfd_dc_val, str_buf);
    }
    if (lbl_vfd_rpm_val) {
        snprintf(str_buf, sizeof(str_buf), "%d об/мин", (int)g_vfd.rpm);
        lv_label_set_text(lbl_vfd_rpm_val, str_buf);
    }
    if (lbl_vfd_pwr_val) {
        float p_kw = (g_vfd.out_volt_v * g_vfd.out_cur_a * 0.85f) / 1000.0f;
        snprintf(str_buf, sizeof(str_buf), "%.2f кВт (U=%.0f В)", p_kw, g_vfd.out_volt_v);
        lv_label_set_text(lbl_vfd_pwr_val, str_buf);
    }

    bool is_running = g_vfd.is_running || s_is_test_running;
    if (led_run) {
        lv_obj_set_style_bg_color(led_run, is_running ? COLOR_GREEN_BRIGHT : COLOR_GREY_OFF, 0);
    }
    if (lbl_badge_run_txt) {
        lv_label_set_text(lbl_badge_run_txt, is_running ? "ПУСК (РАБОТА)" : "ОСТАНОВЛЕН");
    }

    if (led_ready) {
        lv_obj_set_style_bg_color(led_ready, !g_vfd.is_fault ? COLOR_BLUE_BRIGHT : COLOR_GREY_OFF, 0);
    }

    if (led_alarm) {
        lv_obj_set_style_bg_color(led_alarm, g_vfd.is_fault ? COLOR_RED_BRIGHT : COLOR_GREY_OFF, 0);
    }
    if (lbl_badge_alarm_txt) {
        lv_label_set_text(lbl_badge_alarm_txt, g_vfd.is_fault ? "АВАРИЯ ПЧ!" : "НОРМА (ОК)");
    }

    if (led_comm) {
        static bool s_blink = false;
        s_blink = !s_blink;
        if (g_vfd.link == VFD_LINK_OK)
            lv_obj_set_style_bg_color(led_comm, s_blink ? COLOR_CYAN : lv_color_hex(0x00838F), 0);
        else if (g_vfd.link == VFD_LINK_DEGRADED)
            lv_obj_set_style_bg_color(led_comm, COLOR_AMBER, 0);
        else
            lv_obj_set_style_bg_color(led_comm, COLOR_GREY_OFF, 0);
    }

    if (lbl_vfd_diag_status) {
        snprintf(str_buf, sizeof(str_buf), "0x%04X (%s)",
                 g_vfd.status_word, is_running ? "РАБОТА" : "СТОП");
        lv_label_set_text(lbl_vfd_diag_status, str_buf);
    }
    if (lbl_vfd_diag_fault) {
        if (g_vfd.is_fault)
            snprintf(str_buf, sizeof(str_buf), "Er%02u (авария привода)", g_vfd.fault_code);
        else
            snprintf(str_buf, sizeof(str_buf), "нет аварии");
        lv_label_set_text(lbl_vfd_diag_fault, str_buf);
    }
    if (lbl_vfd_diag_temp) {
        snprintf(str_buf, sizeof(str_buf), "%.1f °C (радиатор)", g_vfd.igbt_temp_c);
        lv_label_set_text(lbl_vfd_diag_temp, str_buf);
    }
    if (lbl_vfd_diag_packets) {
        snprintf(str_buf, sizeof(str_buf), "кадров OK: %lu, команд: %lu",
                 (unsigned long)g_vfd.polls_ok, (unsigned long)g_vfd.cmd_ok);
        lv_label_set_text(lbl_vfd_diag_packets, str_buf);
    }
    if (lbl_vfd_diag_latency) {
        snprintf(str_buf, sizeof(str_buf),
                 "%u мс | битых: %lu | таймаутов: %lu | ретраев: %lu",
                 g_vfd.latency_ms, (unsigned long)g_vfd.crc_err,
                 (unsigned long)g_vfd.timeouts, (unsigned long)g_vfd.retries);
        lv_label_set_text(lbl_vfd_diag_latency, str_buf);
    }

    if (ta_test_freq && !lv_obj_has_state(ta_test_freq, LV_STATE_FOCUSED)) {
        snprintf(str_buf, sizeof(str_buf), "%.1f", s_test_freq_hz);
        lv_textarea_set_text(ta_test_freq, str_buf);
    }

    if (lbl_vfd_test_status) {
        if (s_is_test_running) {
            snprintf(str_buf, sizeof(str_buf), "▶ АКТИВЕН ТЕСТ: %.1f Гц", s_test_freq_hz);
            lv_obj_set_style_text_color(lbl_vfd_test_status, COLOR_GREEN_BRIGHT, 0);
        } else {
            snprintf(str_buf, sizeof(str_buf), "Привод готов к тесту");
            lv_obj_set_style_text_color(lbl_vfd_test_status, COLOR_TEXT_DIM, 0);
        }
        lv_label_set_text(lbl_vfd_test_status, str_buf);
    }
}

// =========================================================================
// ТЕСТИРОВАНИЕ КИРИЛЛИЧЕСКИХ ШРИФТОВ
// =========================================================================
bool winder_ui_test_fonts(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "[TEST_FONTS] Валидация кириллических шрифтов...");

    const lv_font_t *fonts[] = { &lv_font_dejavu_16, &lv_font_dejavu_24, &lv_font_dejavu_32 };
    const char *font_names[] = { "DejaVu Sans 16px", "DejaVu Sans 24px", "DejaVu Sans 32px" };

    uint32_t test_codepoints[80];
    int count = 0;
    for (uint32_t c = 0x0410; c <= 0x042F; c++) test_codepoints[count++] = c; // А..Я
    for (uint32_t c = 0x0430; c <= 0x044F; c++) test_codepoints[count++] = c; // а..я
    test_codepoints[count++] = 0x0401; // 'Ё'
    test_codepoints[count++] = 0x0451; // 'ё'
    test_codepoints[count++] = 0x2116; // '№'
    test_codepoints[count++] = 0x00B0; // '°'
    test_codepoints[count++] = 0x2022; // '•'

    bool all_passed = true;
    for (int f = 0; f < 3; f++) {
        const lv_font_t *font = fonts[f];
        uint32_t missing = 0;
        for (int i = 0; i < count; i++) {
            lv_font_glyph_dsc_t dsc;
            bool found = lv_font_get_glyph_dsc(font, &dsc, test_codepoints[i], 0);
            if (!found || dsc.is_placeholder || dsc.box_w == 0) {
                missing++;
                all_passed = false;
            }
        }
        if (missing == 0) {
            ESP_LOGI(TAG, "[TEST_FONTS] Шрифт %s: все %d кириллических глифов найдены (100%%)!", font_names[f], count);
        } else {
            ESP_LOGE(TAG, "[TEST_FONTS] Шрифт %s: пропущено %lu глифов из %d!", font_names[f], (unsigned long)missing, count);
        }
    }
    ESP_LOGI(TAG, "==================================================");
    return all_passed;
}

// =========================================================================
// ВСТРОЕННЫЙ ТЕСТ И АССЕРТ РАСПРЕДЕЛЕНИЯ ТЕКСТА И ГЕОМЕТРИИ (ESP-IDF / LVGL)
// =========================================================================
bool winder_ui_validate_text_layout(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "[LAYOUT_TEST] Валидация вместимости текстов и отсутствия наложений...");

    typedef struct {
        const char *name;
        const char *sample_text;
        const lv_font_t *font;
        int max_width_px;
    } text_slot_test_t;

    const text_slot_test_t slots[] = {
        {"Табло одометра (120px)", "0000.00", FONT_DIGIT_HERO, 498},
        {"Скорость в карточке (24px)", "⚡ Скорость: 120.0 м/мин", FONT_TITLE, 500},
        {"Выработка в карточке (24px)", "Выработка смены: 2000.0 м", FONT_TITLE, 500},
        {"Уставка заголовок (16px)", "УСТАВКА НАМОТКИ (макс. 2000 м):", FONT_RU, 460},
        {"Бобина заголовок (16px)", "ПОДАЮЩАЯ БОБИНА (БУХТА С КАБЕЛЕМ):", FONT_RU, 460},
        {"Остаток бобины (24px)", "2000.00 м", FONT_TITLE, 200},
        {"Телеметрия строка (16px)", "Скорость: 120.0 м/мин  |  ПЧ Easydrive: 50.0 Гц  |  Ток: 4.50 А  |  Импульсов: 999999", FONT_RU, 1000},
        {"Калибровка колонка 1 (16px)", "2. Диск датчиков: Z = 99 зубьев (PPR = 396, 4x)", FONT_RU, 460},
        {"Калибровка колонка 2 (16px)", "6. Выработка: 2000.0 м (K = 1.00000)", FONT_RU, 520},
    };

    bool all_ok = true;
    for (size_t i = 0; i < sizeof(slots)/sizeof(slots[0]); i++) {
        lv_point_t size;
        lv_text_get_size(&size, slots[i].sample_text, slots[i].font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x <= slots[i].max_width_px) {
            ESP_LOGI(TAG, "  ✓ [PASS] %s: длина %ld px <= лимит %d px (запас %ld px)",
                     slots[i].name, (long)size.x, slots[i].max_width_px, (long)(slots[i].max_width_px - size.x));
        } else {
            ESP_LOGE(TAG, "  ❌ [FAIL/COLLISION] %s: длина %ld px ПРЕВЫШАЕТ ЛИМИТ %d px на %ld px!",
                     slots[i].name, (long)size.x, slots[i].max_width_px, (long)(size.x - slots[i].max_width_px));
            all_ok = false;
        }
    }

    ESP_LOGI(TAG, "==================================================");
    return all_ok;
}

// =========================================================================
// ПОСТРОЕНИЕ ИНТЕРФЕЙСА ДЛЯ ЭКРАНА 1024x600 (LVGL 9)
// =========================================================================
void winder_ui_init(void)
{
    ESP_LOGI(TAG, "Создание HMI станка намотки с цифровой клавиатурой и 72px одометром...");

    winder_ui_test_fonts();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_SCREEN_BG, 0);
    lv_obj_set_style_text_font(scr, FONT_RU, 0);

    // Главный TabView (верхняя панель вкладок высотой 50 px)
    lv_obj_t *tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_size(tabview, 50);
    lv_obj_set_style_bg_color(tabview, COLOR_SCREEN_BG, 0);

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_style_bg_color(tab_bar, COLOR_TAB_BAR_BG, 0);
    lv_obj_set_style_border_color(tab_bar, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(tab_bar, 2, 0);
    lv_obj_set_style_text_font(tab_bar, FONT_TITLE, 0);
    lv_obj_set_style_text_color(tab_bar, COLOR_TEXT_DIM, 0);

    lv_obj_t *tab_main = lv_tabview_add_tab(tabview, "  1. НАМОТКА (ПОСТ)  ");
    lv_obj_t *tab_idm  = lv_tabview_add_tab(tabview, "  2. ИДМ-20 КАЛИБРОВКА  ");
    lv_obj_t *tab_auto = lv_tabview_add_tab(tabview, "  3. АВТОМАТИКА  ");
    lv_obj_t *tab_vfd  = lv_tabview_add_tab(tabview, "  4. ЧАСТОТНИК EASYDRIVE  ");

    // ВАЖНО: Сброс внутренних отступов (padding) темы LVGL 9, чтобы полезная область была ровно 1024x550
    lv_obj_t *content = lv_tabview_get_content(tabview);
    if (content) {
        lv_obj_set_style_pad_all(content, 0, 0);
        lv_obj_set_style_pad_gap(content, 0, 0);
        lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_set_style_pad_all(tab_main, 0, 0);
    lv_obj_set_style_pad_gap(tab_main, 0, 0);
    lv_obj_remove_flag(tab_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tab_idm, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tab_auto, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tab_vfd, LV_OBJ_FLAG_SCROLLABLE);

    /* =====================================================================
     * МОДУЛЬНАЯ СЕТКА HMI 1024x550 (MODULAR GRID SYSTEM)
     * ===================================================================== */
    // Поля экрана и рабочая область
    #define GRID_PAD_X              8     // Боковые поля (слева и справа по 8 px)
    #define GRID_CONTENT_W          1008  // Полезная ширина сетки (1024 - 16 = 1008 px)
    #define GRID_GAP_X              12    // Горизонтальный зазор между колонками
    #define GRID_GAP_Y              8     // Вертикальный зазор между слоями

    // Колонки верхнего яруса (ФАКТ МЕТРАЖА и УСТАВКА/БОБИНА)
    #define GRID_COL1_X             GRID_PAD_X                           // X = 8 px
    #define GRID_COL1_W             506                                  // Ширина левой карточки (одометр)
    #define GRID_COL2_X             (GRID_COL1_X + GRID_COL1_W + GRID_GAP_X) // X = 526 px
    #define GRID_COL2_W             (GRID_CONTENT_W - GRID_COL1_W - GRID_GAP_X) // 490 px

    // Вертикальные ярусы экрана (Rows)
    #define GRID_ROW0_Y             6     // Статусный баннер (H = 26 px, Y = 6..32)
    #define GRID_ROW0_H             26

    #define GRID_ROW1_Y             40    // Верхний ярус карточек (H = 270 px, Y = 40..310)
    #define GRID_ROW1_H             270

    #define GRID_ROW2_Y             318   // Прогресс-бар намотки (H = 34 px, Y = 318..352)
    #define GRID_ROW2_H             34

    #define GRID_ROW3_Y             360   // Строка телеметрии ПЧ (H = 32 px, Y = 360..392)
    #define GRID_ROW3_H             32

    #define GRID_ROW4_Y             450   // Панель силовых сенсорных кнопок (H = 88 px, Y = 450..538)
    #define GRID_ROW4_H             88    // Запас до нижнего края матрицы: 12 px (550 - 538 = 12 px)

    // Внутренняя сетка правой колонки (Уставка + Бобина)
    #define GRID_TARGET_H           138   // Карточка уставки (Y = 40..178)
    #define GRID_SPOOL_Y            (GRID_ROW1_Y + GRID_TARGET_H + GRID_GAP_Y) // Y = 186 px
    #define GRID_SPOOL_H            (GRID_ROW1_H - GRID_TARGET_H - GRID_GAP_Y) // 124 px (Y = 186..310)

    /* =====================================================================
     * СЛОЙ 0: ВЕРХНИЙ СТАТУСНЫЙ БАННЕР (ПО СЕТКЕ: X=8, Y=6, W=1008, H=26)
     * ===================================================================== */
    lv_obj_t *box_status = create_inner_box(tab_main, GRID_CONTENT_W, GRID_ROW0_H);
    lv_obj_align(box_status, LV_ALIGN_TOP_LEFT, GRID_PAD_X, GRID_ROW0_Y);
    lv_obj_set_style_pad_all(box_status, 4, 0);

    led_status_banner = create_led_dot(box_status, 12, COLOR_BLUE_BRIGHT);
    lv_obj_align(led_status_banner, LV_ALIGN_LEFT_MID, 6, 0);

    lbl_status_banner = lv_label_create(box_status);
    lv_obj_set_style_text_font(lbl_status_banner, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_status_banner, COLOR_TEXT_MAIN, 0);
    lv_label_set_text(lbl_status_banner, "СТАТУС: ИНИЦИАЛИЗАЦИЯ СТАНКА...");
    lv_obj_align(lbl_status_banner, LV_ALIGN_LEFT_MID, 26, 0);

    /* Баннер линии 485. Пока связи нет, все числа на экране — последние
     * прочитанные, а не текущие: оператор должен видеть это сразу, а не
     * сходить на вкладку диагностики. */
    lbl_link_banner = lv_label_create(box_status);
    lv_obj_set_style_text_font(lbl_link_banner, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_link_banner, lv_color_white(), 0);
    lv_obj_set_style_bg_color(lbl_link_banner, COLOR_RED_DARK, 0);
    lv_obj_set_style_bg_opa(lbl_link_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lbl_link_banner, 4, 0);
    lv_obj_set_style_pad_hor(lbl_link_banner, 8, 0);
    lv_label_set_text(lbl_link_banner, "НЕТ СВЯЗИ С ПЧ");
    lv_obj_align(lbl_link_banner, LV_ALIGN_RIGHT_MID, -6, 0);

    /* =====================================================================
     * СЛОЙ 1, КОЛОНКА 1: КАРТОЧКА ФАКТА МЕТРАЖА (ПО СЕТКЕ: X=8, Y=40, W=506, H=270)
     * ===================================================================== */
    lv_obj_t *card_fact = create_hmi_card(tab_main, GRID_COL1_W, GRID_ROW1_H);
    lv_obj_align(card_fact, LV_ALIGN_TOP_LEFT, GRID_COL1_X, GRID_ROW1_Y);

    // Заголовок карточки
    lv_obj_t *lbl_fact_title = lv_label_create(card_fact);
    lv_obj_set_style_text_font(lbl_fact_title, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_fact_title, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_fact_title, "ТЕКУЩАЯ ДЛИНА (ФАКТ МЕТРАЖА):");
    lv_obj_align(lbl_fact_title, LV_ALIGN_TOP_LEFT, 10, 6);

    // Внутренний бокс одометра (W = 486, H = 146 px)
    lv_obj_t *box_fact_display = create_inner_box(card_fact, GRID_COL1_W - 20, 146);
    lv_obj_align(box_fact_display, LV_ALIGN_TOP_LEFT, 10, 28);

    lbl_cur_len_val = lv_label_create(box_fact_display);
    lv_obj_set_style_text_font(lbl_cur_len_val, FONT_DIGIT_HERO, 0); // 120px Consolas Bold
    lv_obj_set_style_text_color(lbl_cur_len_val, COLOR_CYAN, 0);
    lv_label_set_text(lbl_cur_len_val, "0000.00");
    lv_obj_align(lbl_cur_len_val, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t *lbl_fact_unit = lv_label_create(box_fact_display);
    lv_obj_set_style_text_font(lbl_fact_unit, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_fact_unit, COLOR_CYAN, 0);
    lv_label_set_text(lbl_fact_unit, "м");
    lv_obj_align(lbl_fact_unit, LV_ALIGN_TOP_RIGHT, -10, 8);

    // Внутренняя строка телеметрии 1: Скорость линии (Y = 186 px от верха карточки)
    lbl_speed_val = lv_label_create(card_fact);
    lv_obj_set_style_text_font(lbl_speed_val, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_speed_val, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_speed_val, "⚡ Скорость: 0.0 м/мин");
    lv_obj_align(lbl_speed_val, LV_ALIGN_TOP_LEFT, 12, 186);

    // Внутренняя строка телеметрии 2: Выработка смены (Y = 226 px от верха карточки)
    lbl_fact_prod = lv_label_create(card_fact);
    lv_obj_set_style_text_font(lbl_fact_prod, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_fact_prod, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_fact_prod, "Выработка смены: 0.0 м");
    lv_obj_align(lbl_fact_prod, LV_ALIGN_TOP_LEFT, 12, 226);

    /* =====================================================================
     * СЛОЙ 1, КОЛОНКА 2A: КАРТОЧКА УСТАВКИ (ПО СЕТКЕ: X=526, Y=40, W=490, H=138)
     * ===================================================================== */
    lv_obj_t *card_target = create_hmi_card(tab_main, GRID_COL2_W, GRID_TARGET_H);
    lv_obj_align(card_target, LV_ALIGN_TOP_LEFT, GRID_COL2_X, GRID_ROW1_Y);

    lv_obj_t *lbl_target_title = lv_label_create(card_target);
    lv_obj_set_style_text_font(lbl_target_title, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_target_title, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_target_title, "УСТАВКА НАМОТКИ (макс. 2000 м):");
    lv_obj_align(lbl_target_title, LV_ALIGN_TOP_LEFT, 10, 6);

    // Числовое окно ввода уставки 48px по сетке (W = 144, H = 58)
    ta_target_len = create_numeric_textarea(card_target, 144, 58, COLOR_AMBER, FONT_DIGIT_INPUT);
    lv_obj_align(ta_target_len, LV_ALIGN_TOP_LEFT, 10, 32);
    lv_obj_add_event_cb(ta_target_len, numpad_ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_m_sym = lv_label_create(card_target);
    lv_obj_set_style_text_font(lbl_m_sym, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_m_sym, COLOR_AMBER, 0);
    lv_label_set_text(lbl_m_sym, "м");
    lv_obj_align(lbl_m_sym, LV_ALIGN_TOP_LEFT, 158, 48);

    // Сетка из 6 кнопок дельты (2 строки по 3 кнопки: 88x44 px, зазор 6 px)
    const int deltas[6] = {-100, -10, -1, 1, 10, 100};
    const char *delta_names[6] = {"-100", "-10", "-1", "+1", "+10", "+100"};
    for (int i = 0; i < 6; i++) {
        int col = i % 3;
        int row = i / 3;
        lv_obj_t *btn = create_styled_btn(card_target, 88, 44, 
                                          COLOR_GREY_BTN, COLOR_GREY_BORDER, 6, 
                                          delta_names[i], FONT_TITLE, 
                                          btn_target_adj_cb, (void *)(intptr_t)deltas[i]);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 196 + (col * 94), 32 + (row * 50));
    }

    /* =====================================================================
     * СЛОЙ 1, КОЛОНКА 2B: КАРТОЧКА БОБИНЫ (ПО СЕТКЕ: X=526, Y=186, W=490, H=124)
     * ===================================================================== */
    lv_obj_t *card_spool = create_hmi_card(tab_main, GRID_COL2_W, GRID_SPOOL_H);
    lv_obj_align(card_spool, LV_ALIGN_TOP_LEFT, GRID_COL2_X, GRID_SPOOL_Y);

    lv_obj_t *lbl_spool_title = lv_label_create(card_spool);
    lv_obj_set_style_text_font(lbl_spool_title, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_spool_title, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_spool_title, "ПОДАЮЩАЯ БОБИНА (БУХТА С КАБЕЛЕМ):");
    lv_obj_align(lbl_spool_title, LV_ALIGN_TOP_LEFT, 10, 6);

    // Левая подсекция бухты по сетке
    lv_obj_t *lbl_spool_in_lbl = lv_label_create(card_spool);
    lv_obj_set_style_text_font(lbl_spool_in_lbl, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_spool_in_lbl, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_spool_in_lbl, "Бухта (нач.):");
    lv_obj_align(lbl_spool_in_lbl, LV_ALIGN_TOP_LEFT, 10, 26);

    ta_spool_init_len = create_numeric_textarea(card_spool, 136, 46, COLOR_CYAN, FONT_TITLE);
    lv_obj_align(ta_spool_init_len, LV_ALIGN_TOP_LEFT, 10, 48);
    lv_obj_add_event_cb(ta_spool_init_len, numpad_ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_sp_u = lv_label_create(card_spool);
    lv_obj_set_style_text_font(lbl_sp_u, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_sp_u, COLOR_CYAN, 0);
    lv_label_set_text(lbl_sp_u, "м");
    lv_obj_align(lbl_sp_u, LV_ALIGN_TOP_LEFT, 150, 60);

    // Правая подсекция остатка бобины по сетке
    lv_obj_t *lbl_rem_title = lv_label_create(card_spool);
    lv_obj_set_style_text_font(lbl_rem_title, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_rem_title, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_rem_title, "ОСТАТОК В БОБИНЕ:");
    lv_obj_align(lbl_rem_title, LV_ALIGN_TOP_LEFT, 196, 26);

    lbl_spool_rem_val = lv_label_create(card_spool);
    lv_obj_set_style_text_font(lbl_spool_rem_val, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_spool_rem_val, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_spool_rem_val, "1000.00 м");
    lv_obj_align(lbl_spool_rem_val, LV_ALIGN_TOP_LEFT, 196, 48);

    bar_spool_rem = create_styled_bar(card_spool, 126, 8, COLOR_GREEN_BRIGHT, 0, 100);
    lv_obj_align(bar_spool_rem, LV_ALIGN_TOP_LEFT, 350, 32);

    lbl_spool_status = lv_label_create(card_spool);
    lv_obj_set_style_text_font(lbl_spool_status, FONT_LARGE, 0); // 32px
    lv_obj_set_style_text_color(lbl_spool_status, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_spool_status, "× 10");
    lv_obj_align(lbl_spool_status, LV_ALIGN_TOP_LEFT, 196, 78);

    /* =====================================================================
     * СЛОЙ 2: ПРОГРЕСС-БАР НАМОТКИ (ПО СЕТКЕ: X=8, Y=318, W=1008, H=34)
     * ===================================================================== */
    bar_progress = create_styled_bar(tab_main, GRID_CONTENT_W, GRID_ROW2_H, COLOR_CYAN, 0, 100);
    lv_obj_align(bar_progress, LV_ALIGN_TOP_LEFT, GRID_PAD_X, GRID_ROW2_Y);

    lbl_bar_percent = lv_label_create(bar_progress);
    lv_obj_set_style_text_font(lbl_bar_percent, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_bar_percent, lv_color_white(), 0);
    lv_label_set_text(lbl_bar_percent, "0 %");
    lv_obj_center(lbl_bar_percent);

    /* =====================================================================
     * СЛОЙ 3: СТРОКА ТЕЛЕМЕТРИИ ПЧ (ПО СЕТКЕ: X=8, Y=360, W=1008, H=32)
     * ===================================================================== */
    lv_obj_t *box_telem = create_inner_box(tab_main, GRID_CONTENT_W, GRID_ROW3_H);
    lv_obj_align(box_telem, LV_ALIGN_TOP_LEFT, GRID_PAD_X, GRID_ROW3_Y);
    lv_obj_set_style_pad_all(box_telem, 4, 0);

    lbl_telemetry_line = lv_label_create(box_telem);
    lv_obj_set_style_text_font(lbl_telemetry_line, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_telemetry_line, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_telemetry_line, "Телеметрия частотника: инициализация связи...");
    lv_obj_align(lbl_telemetry_line, LV_ALIGN_LEFT_MID, 12, 0);

    /* =====================================================================
     * СЛОЙ 4: ПАНЕЛЬ СИЛОВЫХ СЕНСОРНЫХ КЛАВИШ (ПО СЕТКЕ: Y=450, H=88)
     * ===================================================================== */
    #define GRID_BTN_GAP           12
    #define GRID_BTN_START_W       270
    #define GRID_BTN_STOP_W        270
    #define GRID_BTN_RESET_W       210
    #define GRID_BTN_JOG_W         222

    #define GRID_BTN_START_X       GRID_PAD_X                                            // X = 8
    #define GRID_BTN_STOP_X        (GRID_BTN_START_X + GRID_BTN_START_W + GRID_BTN_GAP)  // X = 290
    #define GRID_BTN_RESET_X       (GRID_BTN_STOP_X + GRID_BTN_STOP_W + GRID_BTN_GAP)    // X = 572
    #define GRID_BTN_JOG_X         (GRID_BTN_RESET_X + GRID_BTN_RESET_W + GRID_BTN_GAP)  // X = 794

    // Кнопка 1: ПУСК (W = 270, H = 88)
    s_btn_start = create_styled_btn(tab_main, GRID_BTN_START_W, GRID_ROW4_H,
                      COLOR_GREEN_DARK, COLOR_GREEN_BORDER, 14,
                      "ПУСК", FONT_TITLE,
                      btn_start_cb, NULL);
    s_btn_start_lbl = lv_obj_get_child(s_btn_start, 0);
    lv_obj_align(s_btn_start, LV_ALIGN_TOP_LEFT, GRID_BTN_START_X, GRID_ROW4_Y);

    // Кнопка 2: СТОП (W = 270, H = 88)
    create_styled_btn(tab_main, GRID_BTN_STOP_W, GRID_ROW4_H, 
                      COLOR_RED_DARK, COLOR_RED_BORDER, 14, 
                      "СТОП", FONT_TITLE, 
                      btn_stop_cb, NULL);
    lv_obj_align(lv_obj_get_child(tab_main, lv_obj_get_child_count(tab_main)-1), LV_ALIGN_TOP_LEFT, GRID_BTN_STOP_X, GRID_ROW4_Y);

    // Кнопка 3: СБРОС (W = 210, H = 88)
    create_styled_btn(tab_main, GRID_BTN_RESET_W, GRID_ROW4_H, 
                      COLOR_GREY_BTN, COLOR_GREY_BORDER, 14, 
                      "СБРОС 0м", FONT_TITLE, 
                      btn_reset_len_cb, NULL);
    lv_obj_align(lv_obj_get_child(tab_main, lv_obj_get_child_count(tab_main)-1), LV_ALIGN_TOP_LEFT, GRID_BTN_RESET_X, GRID_ROW4_Y);

    // Кнопка 4: ЗАПРАВКА (W = 222, H = 88)
    create_styled_btn(tab_main, GRID_BTN_JOG_W, GRID_ROW4_H, 
                      COLOR_ORANGE, lv_color_hex(0xFFA726), 14, 
                      "ЗАПРАВКА", FONT_TITLE, 
                      btn_jog_cb, NULL);
    lv_obj_align(lv_obj_get_child(tab_main, lv_obj_get_child_count(tab_main)-1), LV_ALIGN_TOP_LEFT, GRID_BTN_JOG_X, GRID_ROW4_Y);

    /* =====================================================================
     * ВКЛАДКА 2: КАЛИБРОВКА И ПАРАМЕТРЫ СМОЛ ИД-2 (ПО СЕТКЕ 1008 px)
     * ===================================================================== */
    lv_obj_t *card_idm = create_hmi_card(tab_idm, GRID_CONTENT_W, 144);
    lv_obj_align(card_idm, LV_ALIGN_TOP_LEFT, GRID_PAD_X, 6);

    lv_obj_t *lbl_idm_title = lv_label_create(card_idm);
    lv_obj_set_style_text_font(lbl_idm_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_idm_title, COLOR_CYAN, 0);
    lv_label_set_text(lbl_idm_title, "⚙ УЗЕЛ ИЗМЕРЕНИЯ: СМОЛ ИД-2 (КОЛЕСО 102 мм, 2 ДАТЧИКА)");
    lv_obj_align(lbl_idm_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_idm_params_col1 = lv_label_create(card_idm);
    lv_obj_set_style_text_font(lbl_idm_params_col1, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_idm_params_col1, COLOR_TEXT_MAIN, 0);
    lv_obj_align(lbl_idm_params_col1, LV_ALIGN_TOP_LEFT, 0, 34);

    lbl_idm_params_col2 = lv_label_create(card_idm);
    lv_obj_set_style_text_font(lbl_idm_params_col2, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_idm_params_col2, COLOR_TEXT_MAIN, 0);
    /* Колонки раздвинуты по-настоящему: строка «2. Диск датчиков: …» в
     * DejaVu 16 занимает 402 px, вторая колонка на x=350 налезала на неё. */
    lv_obj_align(lbl_idm_params_col2, LV_ALIGN_TOP_LEFT, 470, 34);

    // Сетка кнопок подстройки узла (2 колонки x 2 строки: W=130, H=44, зазор 10 px)
    const intptr_t idm_btn_codes[4] = {-1, 1, -2, 2};
    const char *idm_btn_labels[4] = {"Кабель -0.5", "Кабель +0.5", "Зубья Z -1", "Зубья Z +1"};
    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;
        lv_obj_t *b = create_styled_btn(card_idm, 130, 44,
                                        COLOR_GREY_BTN, COLOR_GREY_BORDER, 6,
                                        idm_btn_labels[i], FONT_RU,
                                        btn_idm_adj_cb, (void *)idm_btn_codes[i]);
        lv_obj_align(b, LV_ALIGN_TOP_LEFT, 704 + (col * 140), 24 + (row * 50));
    }

    // Нижняя карточка калибровки по эталону (ПО СЕТКЕ: W=1008, H=376, Y=158)
    lv_obj_t *card_cal = create_hmi_card(tab_idm, GRID_CONTENT_W, 376);
    lv_obj_align(card_cal, LV_ALIGN_TOP_LEFT, GRID_PAD_X, 158);

    lv_obj_t *lbl_cal_title = lv_label_create(card_cal);
    lv_obj_set_style_text_font(lbl_cal_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_cal_title, COLOR_AMBER, 0);
    lv_label_set_text(lbl_cal_title, "РЕЖИМ КАЛИБРОВКИ ПО ЭТАЛОНУ (ПАСПОРТ СМОЛ ИД-2, П. 5.3)");
    lv_obj_align(lbl_cal_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_cal_sub = lv_label_create(card_cal);
    lv_obj_set_style_text_font(lbl_cal_sub, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_cal_sub, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_cal_sub, "Методика: протяните кабель известной длины L_эт, введите L_эт и нажмите «ПРИМЕНИТЬ».");
    lv_obj_align(lbl_cal_sub, LV_ALIGN_TOP_LEFT, 0, 30);

    // Измерительный бокс с цифровым полем ввода эталона L_эт (W=984 по сетке)
    lv_obj_t *box_cal_mon = create_inner_box(card_cal, GRID_CONTENT_W - 24, 56);
    lv_obj_align(box_cal_mon, LV_ALIGN_TOP_LEFT, 0, 60);

    lbl_cal_monitor_cur = lv_label_create(box_cal_mon);
    lv_obj_set_style_text_font(lbl_cal_monitor_cur, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_cal_monitor_cur, COLOR_CYAN, 0);
    lv_label_set_text(lbl_cal_monitor_cur, "Факт: 00.00 м");
    lv_obj_align(lbl_cal_monitor_cur, LV_ALIGN_LEFT_MID, 16, 0);

    lv_obj_t *lbl_arr = lv_label_create(box_cal_mon);
    lv_obj_set_style_text_font(lbl_arr, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_arr, COLOR_AMBER, 0);
    lv_label_set_text(lbl_arr, "➔  Эталон:");
    lv_obj_align(lbl_arr, LV_ALIGN_LEFT_MID, 240, 0);

    // Текстовое поле ввода эталона L_эт для цифровой клавиатуры (клик открывает numpad)
    ta_ref_len = create_numeric_textarea(box_cal_mon, 160, 42, COLOR_AMBER, FONT_TITLE);
    lv_obj_align(ta_ref_len, LV_ALIGN_LEFT_MID, 380, 0);
    lv_obj_add_event_cb(ta_ref_len, numpad_ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_ref_unit = lv_label_create(box_cal_mon);
    lv_obj_set_style_text_font(lbl_ref_unit, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_ref_unit, COLOR_AMBER, 0);
    lv_label_set_text(lbl_ref_unit, "м");
    lv_obj_align(lbl_ref_unit, LV_ALIGN_LEFT_MID, 550, 0);

    lbl_cal_monitor_k = lv_label_create(box_cal_mon);
    lv_obj_set_style_text_font(lbl_cal_monitor_k, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_cal_monitor_k, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_cal_monitor_k, "K_кал: 1.00000");
    lv_obj_align(lbl_cal_monitor_k, LV_ALIGN_RIGHT_MID, -16, 0);

    // Сетка из 6 кнопок дельты эталонной длины L_эт (W=152, gap=14 px)
    const intptr_t ref_deltas[6] = {-1000, -100, -10, 10, 100, 1000};
    const char *ref_delta_labels[6] = {"-10 м", "-1 м", "-0.1 м", "+0.1 м", "+1 м", "+10 м"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t *b = create_styled_btn(card_cal, 152, 46,
                                        COLOR_BOX_BG, COLOR_BOX_BORDER, 6,
                                        ref_delta_labels[i], FONT_RU,
                                        btn_ref_len_adj_cb, (void *)ref_deltas[i]);
        lv_obj_align(b, LV_ALIGN_TOP_LEFT, i * 166, 126);
    }

    // Ряд кнопок действий по сетке (сумма ширин = 984 px)
    create_styled_btn(card_cal, 456, 64,
                      COLOR_GREEN_DARK, COLOR_GREEN_BORDER, 10,
                      "✔ ПРИМЕНИТЬ КАЛИБРОВКУ", FONT_TITLE,
                      btn_apply_calibration_cb, NULL);
    lv_obj_align(lv_obj_get_child(card_cal, lv_obj_get_child_count(card_cal)-1), LV_ALIGN_TOP_LEFT, 0, 184);

    create_styled_btn(card_cal, 250, 64,
                      COLOR_GREY_BTN, COLOR_GREY_BORDER, 10,
                      "СБРОС K_КАЛ\n (1.00000)", FONT_RU,
                      btn_reset_cal_cb, NULL);
    lv_obj_align(lv_obj_get_child(card_cal, lv_obj_get_child_count(card_cal)-1), LV_ALIGN_TOP_LEFT, 470, 184);

    create_styled_btn(card_cal, 250, 64,
                      COLOR_GREY_BTN, COLOR_GREY_BORDER, 10,
                      "СБРОС ВЫРАБОТКИ\n (0.00 м)", FONT_RU,
                      btn_reset_prod_cb, NULL);
    lv_obj_align(lv_obj_get_child(card_cal, lv_obj_get_child_count(card_cal)-1), LV_ALIGN_TOP_LEFT, 734, 184);

    lv_obj_t *box_cal_status = create_inner_box(card_cal, GRID_CONTENT_W - 24, 38);
    lv_obj_align(box_cal_status, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lbl_cal_status_msg = lv_label_create(box_cal_status);
    lv_obj_set_style_text_font(lbl_cal_status_msg, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_cal_status_msg, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_cal_status_msg, s_cal_status_text);
    lv_obj_align(lbl_cal_status_msg, LV_ALIGN_LEFT_MID, 10, 0);

    /* =====================================================================
     * ВКЛАДКА 3: АВТОМАТИКА И ЗАЩИТЫ (ПО СЕТКЕ: W=1008, H=532)
     * ===================================================================== */
    lv_obj_t *card_auto = create_hmi_card(tab_auto, GRID_CONTENT_W, 532);
    lv_obj_align(card_auto, LV_ALIGN_TOP_LEFT, GRID_PAD_X, 6);

    lv_obj_t *lbl_auto_title = lv_label_create(card_auto);
    lv_obj_set_style_text_font(lbl_auto_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_auto_title, COLOR_AMBER, 0);
    lv_label_set_text(lbl_auto_title, "⚙ АВТОМАТИЧЕСКИЕ РЕЖИМЫ И ЗАЩИТЫ СТАНКА НАМОТКИ");
    lv_obj_align(lbl_auto_title, LV_ALIGN_TOP_LEFT, 0, 0);

    const char *auto_texts[6] = {
        "1. Дистанция начала дотяжки: за 10.00 м до уставки (переход на ползучую частоту)",
        "2. Упреждение остановки на выбег: 0.35 м (компенсация инерции бухты)",
        "3. Частоты привода: базовая рабочая 45.0 Гц  |  ползучая скорость 5.0 Гц",
        "4. Защита от обрыва кабеля: таймаут вращения колеса 1.5 сек с мгновенным остановом",
        "5. Защита от заклинивания бухты: токовая отсечка 6.50 А (защита привода и изоляции)",
        "6. Контроль натяжения и раскладчика: автоматическая синхронизация шага намотки"
    };

    for (int i = 0; i < 6; i++) {
        lv_obj_t *item_box = create_inner_box(card_auto, GRID_CONTENT_W - 24, 66);
        lv_obj_align(item_box, LV_ALIGN_TOP_LEFT, 0, 40 + (i * 78));

        lv_obj_t *chk_led = create_led_dot(item_box, 14, COLOR_GREEN_BRIGHT);
        lv_obj_align(chk_led, LV_ALIGN_LEFT_MID, 10, 0);

        lbl_auto_items[i] = lv_label_create(item_box);
        lv_obj_set_style_text_font(lbl_auto_items[i], FONT_RU, 0);
        lv_obj_set_style_text_color(lbl_auto_items[i], COLOR_TEXT_MAIN, 0);
        lv_label_set_text(lbl_auto_items[i], auto_texts[i]);
        lv_obj_align(lbl_auto_items[i], LV_ALIGN_LEFT_MID, 36, 0);
    }

    /* =====================================================================
     * ВКЛАДКА 4: ПРЕМИАЛЬНЫЙ ДАШБОРД ПРИВОДА EASYDRIVE MINI-S (3 КОЛОНКИ ПО СЕТКЕ)
     * Сетка: 3 колонки по 328 px, зазор 12 px (328 + 12 + 328 + 12 + 328 = 1008 px)
     * ===================================================================== */
    #define VFD_GRID_COL_W           328
    #define VFD_GRID_GAP_X           12
    #define VFD_GRID_COL1_X          GRID_PAD_X
    #define VFD_GRID_COL2_X          (VFD_GRID_COL1_X + VFD_GRID_COL_W + VFD_GRID_GAP_X) // X = 348
    #define VFD_GRID_COL3_X          (VFD_GRID_COL2_X + VFD_GRID_COL_W + VFD_GRID_GAP_X) // X = 688
    #define VFD_INNER_W              (VFD_GRID_COL_W - 24) // 304 px (с учетом внутренних полей 12 px)

    // 1. Левая колонка: Силовые метрики (ПО СЕТКЕ: X=8, W=328, H=532)
    lv_obj_t *col_power = create_hmi_card(tab_vfd, VFD_GRID_COL_W, 532);
    lv_obj_align(col_power, LV_ALIGN_TOP_LEFT, VFD_GRID_COL1_X, 6);

    lv_obj_t *lbl_col_pwr_title = lv_label_create(col_power);
    lv_obj_set_style_text_font(lbl_col_pwr_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_col_pwr_title, COLOR_CYAN, 0);
    lv_label_set_text(lbl_col_pwr_title, "⚡ СИЛОВЫЕ МЕТРИКИ");
    lv_obj_align(lbl_col_pwr_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *box_freq = create_inner_box(col_power, VFD_INNER_W, 110);
    lv_obj_align(box_freq, LV_ALIGN_TOP_LEFT, 0, 36);

    lv_obj_t *lbl_f_tag = lv_label_create(box_freq);
    lv_obj_set_style_text_font(lbl_f_tag, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_f_tag, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_f_tag, "ВЫХОДНАЯ ЧАСТОТА:");
    lv_obj_align(lbl_f_tag, LV_ALIGN_TOP_LEFT, 2, 2);

    lbl_vfd_freq_val = lv_label_create(box_freq);
    lv_obj_set_style_text_font(lbl_vfd_freq_val, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_vfd_freq_val, COLOR_CYAN, 0);
    lv_label_set_text(lbl_vfd_freq_val, "0.0 Гц");
    lv_obj_align(lbl_vfd_freq_val, LV_ALIGN_TOP_RIGHT, -4, 0);

    bar_vfd_freq = create_styled_bar(box_freq, VFD_INNER_W - 16, 14, COLOR_CYAN, 0, 500);
    lv_obj_align(bar_vfd_freq, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *lbl_f_scale = lv_label_create(box_freq);
    lv_obj_set_style_text_font(lbl_f_scale, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_f_scale, COLOR_TEXT_MUTED, 0);
    lv_label_set_text(lbl_f_scale, "0 Гц              25 Гц              50 Гц");
    lv_obj_align(lbl_f_scale, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_obj_t *box_curr = create_inner_box(col_power, VFD_INNER_W, 110);
    lv_obj_align(box_curr, LV_ALIGN_TOP_LEFT, 0, 156);

    lv_obj_t *lbl_c_tag = lv_label_create(box_curr);
    lv_obj_set_style_text_font(lbl_c_tag, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_c_tag, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_c_tag, "ТОК ДВИГАТЕЛЯ:");
    lv_obj_align(lbl_c_tag, LV_ALIGN_TOP_LEFT, 2, 2);

    lbl_vfd_curr_val = lv_label_create(box_curr);
    lv_obj_set_style_text_font(lbl_vfd_curr_val, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_vfd_curr_val, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_vfd_curr_val, "0.00 А");
    lv_obj_align(lbl_vfd_curr_val, LV_ALIGN_TOP_RIGHT, -4, 0);

    bar_vfd_curr = create_styled_bar(box_curr, VFD_INNER_W - 16, 14, COLOR_GREEN_BRIGHT, 0, 650);
    lv_obj_align(bar_vfd_curr, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *lbl_c_scale = lv_label_create(box_curr);
    lv_obj_set_style_text_font(lbl_c_scale, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_c_scale, COLOR_TEXT_MUTED, 0);
    lv_label_set_text(lbl_c_scale, "0.0 А        4.5 А (Ном)        6.5 А (Отс)");
    lv_obj_align(lbl_c_scale, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_obj_t *box_more = create_inner_box(col_power, VFD_INNER_W, 200);
    lv_obj_align(box_more, LV_ALIGN_TOP_LEFT, 0, 276);

    lv_obj_t *l_dc_t = lv_label_create(box_more);
    lv_obj_set_style_text_font(l_dc_t, FONT_RU, 0);
    lv_obj_set_style_text_color(l_dc_t, COLOR_TEXT_DIM, 0);
    lv_label_set_text(l_dc_t, "Напряжение DC-шины:");
    lv_obj_align(l_dc_t, LV_ALIGN_TOP_LEFT, 6, 8);

    lbl_vfd_dc_val = lv_label_create(box_more);
    lv_obj_set_style_text_font(lbl_vfd_dc_val, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_vfd_dc_val, COLOR_CYAN, 0);
    lv_label_set_text(lbl_vfd_dc_val, "542.0 В");
    lv_obj_align(lbl_vfd_dc_val, LV_ALIGN_TOP_LEFT, 6, 28);

    lv_obj_t *l_rpm_t = lv_label_create(box_more);
    lv_obj_set_style_text_font(l_rpm_t, FONT_RU, 0);
    lv_obj_set_style_text_color(l_rpm_t, COLOR_TEXT_DIM, 0);
    lv_label_set_text(l_rpm_t, "Расчетные обороты вала:");
    lv_obj_align(l_rpm_t, LV_ALIGN_TOP_LEFT, 6, 68);

    lbl_vfd_rpm_val = lv_label_create(box_more);
    lv_obj_set_style_text_font(lbl_vfd_rpm_val, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_vfd_rpm_val, COLOR_AMBER, 0);
    lv_label_set_text(lbl_vfd_rpm_val, "0 об/мин");
    lv_obj_align(lbl_vfd_rpm_val, LV_ALIGN_TOP_LEFT, 6, 88);

    lv_obj_t *l_pwr_t = lv_label_create(box_more);
    lv_obj_set_style_text_font(l_pwr_t, FONT_RU, 0);
    lv_obj_set_style_text_color(l_pwr_t, COLOR_TEXT_DIM, 0);
    lv_label_set_text(l_pwr_t, "Потребляемая мощность:");
    lv_obj_align(l_pwr_t, LV_ALIGN_TOP_LEFT, 6, 128);

    lbl_vfd_pwr_val = lv_label_create(box_more);
    lv_obj_set_style_text_font(lbl_vfd_pwr_val, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_vfd_pwr_val, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_vfd_pwr_val, "0.00 кВт");
    lv_obj_align(lbl_vfd_pwr_val, LV_ALIGN_TOP_LEFT, 6, 148);

    // 2. Средняя колонка: Modbus RTU (ПО СЕТКЕ: X=348, W=328, H=532)
    lv_obj_t *col_diag = create_hmi_card(tab_vfd, VFD_GRID_COL_W, 532);
    lv_obj_align(col_diag, LV_ALIGN_TOP_LEFT, VFD_GRID_COL2_X, 6);

    lv_obj_t *lbl_col_diag_title = lv_label_create(col_diag);
    lv_obj_set_style_text_font(lbl_col_diag_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_col_diag_title, COLOR_CYAN, 0);
    lv_label_set_text(lbl_col_diag_title, "📡 MODBUS RTU СВЯЗЬ");
    lv_obj_align(lbl_col_diag_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_diag_sub = lv_label_create(col_diag);
    lv_obj_set_style_text_font(lbl_diag_sub, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_diag_sub, COLOR_GREEN_BRIGHT, 0);
    lv_label_set_text(lbl_diag_sub, "RS-485 (UART1: 9600 8N1 • ID 1)");
    lv_obj_align(lbl_diag_sub, LV_ALIGN_TOP_LEFT, 0, 26);

    // Бейджи статусов (сетка 2x2: W=148, H=52, зазор 8 px)
    lv_obj_t *b_run = create_inner_box(col_diag, 148, 52);
    lv_obj_align(b_run, LV_ALIGN_TOP_LEFT, 0, 56);
    led_run = create_led_dot(b_run, 14, COLOR_GREY_OFF);
    lv_obj_align(led_run, LV_ALIGN_LEFT_MID, 6, 0);
    lbl_badge_run_txt = lv_label_create(b_run);
    lv_obj_set_style_text_font(lbl_badge_run_txt, FONT_RU, 0);
    lv_label_set_text(lbl_badge_run_txt, "ОСТАНОВЛЕН");
    lv_obj_align(lbl_badge_run_txt, LV_ALIGN_LEFT_MID, 26, 0);

    lv_obj_t *b_ready = create_inner_box(col_diag, 148, 52);
    lv_obj_align(b_ready, LV_ALIGN_TOP_LEFT, 156, 56);
    led_ready = create_led_dot(b_ready, 14, COLOR_BLUE_BRIGHT);
    lv_obj_align(led_ready, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_t *l_ready_t = lv_label_create(b_ready);
    lv_obj_set_style_text_font(l_ready_t, FONT_RU, 0);
    lv_label_set_text(l_ready_t, "ГОТОВ К ПУСКУ");
    lv_obj_align(l_ready_t, LV_ALIGN_LEFT_MID, 26, 0);

    lv_obj_t *b_alarm = create_inner_box(col_diag, 148, 52);
    lv_obj_align(b_alarm, LV_ALIGN_TOP_LEFT, 0, 116);
    led_alarm = create_led_dot(b_alarm, 14, COLOR_GREY_OFF);
    lv_obj_align(led_alarm, LV_ALIGN_LEFT_MID, 6, 0);
    lbl_badge_alarm_txt = lv_label_create(b_alarm);
    lv_obj_set_style_text_font(lbl_badge_alarm_txt, FONT_RU, 0);
    lv_label_set_text(lbl_badge_alarm_txt, "НОРМА (ОК)");
    lv_obj_align(lbl_badge_alarm_txt, LV_ALIGN_LEFT_MID, 26, 0);

    lv_obj_t *b_comm = create_inner_box(col_diag, 148, 52);
    lv_obj_align(b_comm, LV_ALIGN_TOP_LEFT, 156, 116);
    led_comm = create_led_dot(b_comm, 14, COLOR_CYAN);
    lv_obj_align(led_comm, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_t *l_comm_t = lv_label_create(b_comm);
    lv_obj_set_style_text_font(l_comm_t, FONT_RU, 0);
    lv_label_set_text(l_comm_t, "СВЯЗЬ АКТИВНА");
    lv_obj_align(l_comm_t, LV_ALIGN_LEFT_MID, 26, 0);

    lv_obj_t *box_diag_tbl = create_inner_box(col_diag, VFD_INNER_W, 300);
    lv_obj_align(box_diag_tbl, LV_ALIGN_TOP_LEFT, 0, 178);

    const char *diag_labels[5] = {
        "Статус привода [0x2100]:",
        "Код аварии [0x2101]:",
        "Температура IGBT:",
        "Опрошено пакетов RTU:",
        "Время отклика шины:"
    };
    lv_obj_t **diag_vals[5] = {
        &lbl_vfd_diag_status,
        &lbl_vfd_diag_fault,
        &lbl_vfd_diag_temp,
        &lbl_vfd_diag_packets,
        &lbl_vfd_diag_latency
    };

    for (int i = 0; i < 5; i++) {
        int y_pos = 12 + (i * 56);
        lv_obj_t *lbl_t = lv_label_create(box_diag_tbl);
        lv_obj_set_style_text_font(lbl_t, FONT_RU, 0);
        lv_obj_set_style_text_color(lbl_t, COLOR_TEXT_DIM, 0);
        lv_label_set_text(lbl_t, diag_labels[i]);
        lv_obj_align(lbl_t, LV_ALIGN_TOP_LEFT, 8, y_pos);

        *diag_vals[i] = lv_label_create(box_diag_tbl);
        lv_obj_set_style_text_font(*diag_vals[i], FONT_TITLE, 0);
        lv_obj_set_style_text_color(*diag_vals[i], COLOR_TEXT_MAIN, 0);
        lv_label_set_text(*diag_vals[i], "...");
        lv_obj_align(*diag_vals[i], LV_ALIGN_TOP_LEFT, 8, y_pos + 20);
    }

    // 3. Правая колонка: Сервисный пульт и тест привода (ПО СЕТКЕ: X=688, W=328, H=532)
    lv_obj_t *col_ctrl = create_hmi_card(tab_vfd, VFD_GRID_COL_W, 532);
    lv_obj_align(col_ctrl, LV_ALIGN_TOP_LEFT, VFD_GRID_COL3_X, 6);

    lv_obj_t *lbl_col_ctrl_title = lv_label_create(col_ctrl);
    lv_obj_set_style_text_font(lbl_col_ctrl_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_col_ctrl_title, COLOR_AMBER, 0);
    lv_label_set_text(lbl_col_ctrl_title, "🛠 СЕРВИСНЫЙ ТЕСТ");
    lv_obj_align(lbl_col_ctrl_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *box_test_f = create_inner_box(col_ctrl, VFD_INNER_W, 54);
    lv_obj_align(box_test_f, LV_ALIGN_TOP_LEFT, 0, 34);

    lv_obj_t *lbl_tf_lbl = lv_label_create(box_test_f);
    lv_obj_set_style_text_font(lbl_tf_lbl, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_tf_lbl, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_tf_lbl, "Частота:");
    lv_obj_align(lbl_tf_lbl, LV_ALIGN_LEFT_MID, 4, 0);

    // Поле ввода тестовой частоты (клик открывает цифровую клавиатуру)
    ta_test_freq = create_numeric_textarea(box_test_f, 120, 40, COLOR_CYAN, FONT_TITLE);
    lv_obj_align(ta_test_freq, LV_ALIGN_LEFT_MID, 75, 0);
    lv_obj_add_event_cb(ta_test_freq, numpad_ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl_tf_u = lv_label_create(box_test_f);
    lv_obj_set_style_text_font(lbl_tf_u, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_tf_u, COLOR_CYAN, 0);
    lv_label_set_text(lbl_tf_u, "Гц");
    lv_obj_align(lbl_tf_u, LV_ALIGN_LEFT_MID, 205, 0);

    // Сетка пресетов частоты: 4 кнопки по W=70 px, зазор 8 px (сумма = 304 px)
    const intptr_t test_freqs[4] = {10, 25, 35, 50};
    const char *test_freq_names[4] = {"10 Гц", "25 Гц", "35 Гц", "50 Гц"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = create_styled_btn(col_ctrl, 70, 44,
                                        COLOR_BOX_BG, COLOR_BOX_BORDER, 6,
                                        test_freq_names[i], FONT_RU,
                                        btn_vfd_set_freq_cb, (void *)test_freqs[i]);
        lv_obj_align(b, LV_ALIGN_TOP_LEFT, i * 78, 96);
    }

    // Сетка кнопок дельты частоты: 2 кнопки по W=148 px, зазор 8 px (сумма = 304 px)
    const intptr_t delta_f[2] = {-1, 1};
    const char *delta_f_names[2] = {"-1.0 Гц", "+1.0 Гц"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = create_styled_btn(col_ctrl, 148, 44,
                                        COLOR_BOX_BG, COLOR_BOX_BORDER, 6,
                                        delta_f_names[i], FONT_RU,
                                        btn_vfd_adj_freq_cb, (void *)delta_f[i]);
        lv_obj_align(b, LV_ALIGN_TOP_LEFT, i * 156, 148);
    }

    // Кнопки ПУСК и СТОП теста: 2 кнопки по W=148 px, зазор 8 px (сумма = 304 px)
    create_styled_btn(col_ctrl, 148, 56,
                      COLOR_GREEN_DARK, COLOR_GREEN_BORDER, 8,
                      "▶ ПУСК", FONT_TITLE,
                      btn_vfd_test_start_cb, NULL);
    lv_obj_align(lv_obj_get_child(col_ctrl, lv_obj_get_child_count(col_ctrl)-1), LV_ALIGN_TOP_LEFT, 0, 202);

    create_styled_btn(col_ctrl, 148, 56,
                      COLOR_RED_DARK, COLOR_RED_BORDER, 8,
                      "■ СТОП", FONT_TITLE,
                      btn_vfd_test_stop_cb, NULL);
    lv_obj_align(lv_obj_get_child(col_ctrl, lv_obj_get_child_count(col_ctrl)-1), LV_ALIGN_TOP_LEFT, 156, 202);

    create_styled_btn(col_ctrl, VFD_INNER_W, 60,
                      COLOR_BLUE_DARK, COLOR_BLUE_BORDER, 10,
                      "⟳ СБРОС АВАРИИ ПЧ", FONT_TITLE,
                      btn_vfd_reset_cb, NULL);
    lv_obj_align(lv_obj_get_child(col_ctrl, lv_obj_get_child_count(col_ctrl)-1), LV_ALIGN_TOP_LEFT, 0, 268);

    lv_obj_t *box_test_st = create_inner_box(col_ctrl, VFD_INNER_W, 42);
    lv_obj_align(box_test_st, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lbl_vfd_test_status = lv_label_create(box_test_st);
    lv_obj_set_style_text_font(lbl_vfd_test_status, FONT_RU, 0);
    lv_obj_set_style_text_color(lbl_vfd_test_status, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_vfd_test_status, "Привод готов к тесту");
    lv_obj_align(lbl_vfd_test_status, LV_ALIGN_LEFT_MID, 6, 0);

    /* =====================================================================
     * ЕДИНАЯ ЦИФРОВАЯ КЛАВИАТУРА LVGL (МОДАЛЬНЫЙ НУМПАД ВНИЗУ ЭКРАНА)
     * ===================================================================== */
    s_numpad_kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(s_numpad_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(s_numpad_kb, 560, 255);
    lv_obj_align(s_numpad_kb, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Стили цифровой клавиатуры: темно-индустриальный с неоновым акцентом
    lv_obj_set_style_bg_color(s_numpad_kb, COLOR_BOX_BG, 0);
    lv_obj_set_style_border_color(s_numpad_kb, COLOR_CYAN, 0);
    lv_obj_set_style_border_width(s_numpad_kb, 3, 0);
    lv_obj_set_style_radius(s_numpad_kb, 12, 0);
    lv_obj_set_style_pad_all(s_numpad_kb, 8, 0);

    // Стили кнопок клавиатуры
    lv_obj_set_style_bg_color(s_numpad_kb, lv_color_hex(0x263238), LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_numpad_kb, COLOR_TEXT_MAIN, LV_PART_ITEMS);
    lv_obj_set_style_radius(s_numpad_kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_numpad_kb, lv_color_hex(0x37474F), LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_numpad_kb, 1, LV_PART_ITEMS);

    lv_obj_add_event_cb(s_numpad_kb, numpad_kb_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(s_numpad_kb, LV_OBJ_FLAG_HIDDEN); // По умолчанию скрыта

    // Таймер обновления данных
    lv_timer_create(ui_update_timer_cb, 100, NULL);
    ui_update_timer_cb(NULL);

    // Автоматическая валидация отсутствия наложений текста в рантайме
    winder_ui_validate_text_layout();

    ESP_LOGI(TAG, "Интерфейс станка намотки готов! Все тексты проверены на вместимость.");
}
