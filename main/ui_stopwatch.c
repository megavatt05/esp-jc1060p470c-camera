#include "ui_stopwatch.h"
#include "ui_test_patterns.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl.h"
#include "fonts/cyrillic_fonts.h"

static const char *TAG = "UI_STOPWATCH";

// =========================================================================
// СОСТОЯНИЕ МИКРОСЕКУНДОМЕРА
// =========================================================================
static volatile bool s_running = true;
static volatile int64_t s_start_us = 0;
static volatile int64_t s_accumulated_us = 0;
static volatile uint32_t s_frame_counter = 0;
static volatile uint32_t s_mark_counter = 0;
static volatile int64_t s_last_mark_us = 0;

// Расчет FPS
static int64_t s_fps_last_time_us = 0;
static uint32_t s_fps_frame_count = 0;
static float s_current_fps = 60.0f;

// Дисплей и ориентация
static lv_display_t *s_disp = NULL;
static int s_current_rotation_deg = 90; // По умолчанию 90° для вертикального стенда

// Виджеты LVGL
static lv_obj_t *s_scr_root = NULL;
static lv_obj_t *s_lbl_sec = NULL;
static lv_obj_t *s_lbl_us = NULL;
static lv_obj_t *s_lbl_main_time = NULL;
static lv_obj_t *s_box_sync = NULL;
static lv_obj_t *s_lbl_sync = NULL;
static lv_obj_t *s_lbl_stats = NULL;
static lv_obj_t *s_btn_start_stop = NULL;
static lv_obj_t *s_lbl_btn_start = NULL;

static lv_timer_t *s_render_timer = NULL;

// Буферы для форматирования
static char s_buf_sec[16] = "00";
static char s_buf_us[32] = ".000000";
static char s_buf_main[64] = "00.000000";
static char s_buf_stats[256] = "";
static char s_buf_sync[32] = "SYNC [0]";

// =========================================================================
// ФУНКЦИИ УПРАВЛЕНИЯ ВРЕМЕНЕМ
// =========================================================================
int64_t ui_stopwatch_get_time_us(void)
{
    if (s_running) {
        return s_accumulated_us + (esp_timer_get_time() - s_start_us);
    }
    return s_accumulated_us;
}

bool ui_stopwatch_is_running(void)
{
    return s_running;
}

void ui_stopwatch_start(void)
{
    if (!s_running) {
        s_start_us = esp_timer_get_time();
        s_running = true;
        ESP_LOGI(TAG, "Секундомер ЗАПУЩЕН с отметки %lld мкс", (long long)s_accumulated_us);
    }
}

void ui_stopwatch_stop(void)
{
    if (s_running) {
        s_accumulated_us += (esp_timer_get_time() - s_start_us);
        s_running = false;
        ESP_LOGI(TAG, "Секундомер ОСТАНОВЛЕН на %lld мкс", (long long)s_accumulated_us);
    }
}

void ui_stopwatch_reset(void)
{
    s_accumulated_us = 0;
    s_start_us = esp_timer_get_time();
    s_frame_counter = 0;
    ESP_LOGI(TAG, "Секундомер СБРОШЕН в 00.000000");
}

void ui_stopwatch_toggle(void)
{
    if (s_running) {
        ui_stopwatch_stop();
    } else {
        ui_stopwatch_start();
    }
}

void ui_stopwatch_trigger_mark(void)
{
    s_last_mark_us = ui_stopwatch_get_time_us();
    s_mark_counter++;
    printf("{\"event\":\"MARK\",\"id\":%lu,\"time_us\":%lld}\r\n",
           (unsigned long)s_mark_counter, (long long)s_last_mark_us);
}

// =========================================================================
// ОБРАБОТЧИКИ КНОПОК
// =========================================================================
static void btn_start_stop_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_stopwatch_toggle();
}

static void btn_reset_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_stopwatch_reset();
}

static void btn_mark_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_stopwatch_trigger_mark();
}

static void btn_pattern_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_test_patterns_next();
}

// =========================================================================
// ТАЙМЕР ОБНОВЛЕНИЯ ЭКРАНА (60 FPS)
// =========================================================================
static void stopwatch_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (ui_test_patterns_get_mode() != PATTERN_MODE_STOPWATCH) {
        return;
    }

    int64_t now_us = esp_timer_get_time();
    int64_t elapsed_us = s_running ? (s_accumulated_us + (now_us - s_start_us)) : s_accumulated_us;
    s_frame_counter++;

    // Расчет фактического FPS
    s_fps_frame_count++;
    if (now_us - s_fps_last_time_us >= 1000000) {
        s_current_fps = (float)(s_fps_frame_count * 1000000.0f) / (float)(now_us - s_fps_last_time_us);
        s_fps_last_time_us = now_us;
        s_fps_frame_count = 0;
    }

    // Разложение на секунды и микросекунды
    int64_t total_sec = elapsed_us / 1000000;
    int64_t us_rem = elapsed_us % 1000000;
    int sec = (int)(total_sec % 100);

    // 1. Обновление значений времени
    if (s_lbl_sec && s_lbl_us) {
        snprintf(s_buf_sec, sizeof(s_buf_sec), "%02d", sec);
        lv_label_set_text(s_lbl_sec, s_buf_sec);

        snprintf(s_buf_us, sizeof(s_buf_us), ".%03lld %03lld", (long long)(us_rem / 1000), (long long)(us_rem % 1000));
        lv_label_set_text(s_lbl_us, s_buf_us);
    }
    if (s_lbl_main_time) {
        snprintf(s_buf_main, sizeof(s_buf_main), "%02d.%03lld %03lld", sec, (long long)(us_rem / 1000), (long long)(us_rem % 1000));
        lv_label_set_text(s_lbl_main_time, s_buf_main);
    }

    // 2. Статистика с отображением Wi-Fi IP
    if (s_lbl_stats) {
        snprintf(s_buf_stats, sizeof(s_buf_stats),
                 "#%06lu | %.1f FPS | Wi-Fi: 192.168.4.1",
                 (unsigned long)s_frame_counter, s_current_fps);
        lv_label_set_text(s_lbl_stats, s_buf_stats);
    }

    // 3. Оптический маркер синхронизации (10 Гц)
    bool sync_phase = ((elapsed_us / 100000) % 2) != 0;
    if (s_box_sync && s_lbl_sync) {
        if (sync_phase) {
            lv_obj_set_style_bg_color(s_box_sync, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_color(s_lbl_sync, lv_color_hex(0x000000), 0);
            snprintf(s_buf_sync, sizeof(s_buf_sync), "SYNC [1]");
        } else {
            lv_obj_set_style_bg_color(s_box_sync, lv_color_hex(0x000000), 0);
            lv_obj_set_style_text_color(s_lbl_sync, lv_color_hex(0xFFFFFF), 0);
            snprintf(s_buf_sync, sizeof(s_buf_sync), "SYNC [0]");
        }
        lv_label_set_text(s_lbl_sync, s_buf_sync);
    }

    // 4. Кнопка Старт/Пауза
    if (s_btn_start_stop && s_lbl_btn_start) {
        if (s_running) {
            lv_obj_set_style_bg_color(s_btn_start_stop, lv_color_hex(0xD83B01), 0);
            lv_label_set_text(s_lbl_btn_start, "ПАУЗА");
        } else {
            lv_obj_set_style_bg_color(s_btn_start_stop, lv_color_hex(0x107C41), 0);
            lv_label_set_text(s_lbl_btn_start, "СТАРТ");
        }
    }
}

// =========================================================================
// ПОСТРОЕНИЕ ГРАФИЧЕСКОГО ИНТЕРФЕЙСА ПОД ТЕКУЩУЮ ОРИЕНТАЦИЮ
// =========================================================================
static void build_ui(void)
{
    lv_obj_clean(s_scr_root);
    lv_obj_set_style_bg_color(s_scr_root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_scr_root, LV_OPA_COVER, 0);

    bool is_portrait = (s_current_rotation_deg == 90 || s_current_rotation_deg == 270);
    int32_t w = lv_obj_get_width(s_scr_root);
    int32_t h = lv_obj_get_height(s_scr_root);
    ESP_LOGI(TAG, "Построение интерфейса: ориентация %d°, W=%ld, H=%ld (портрет: %s)",
             s_current_rotation_deg, (long)w, (long)h, is_portrait ? "ДА" : "НЕТ");

    if (is_portrait) {
        // =================================================================
        // ПОРТРЕТНЫЙ РЕЖИМ (600 x 1024) - Для вертикального стенда
        // =================================================================
        lv_obj_t *cont = lv_obj_create(s_scr_root);
        lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_style_pad_all(cont, 15, 0);

        // 1. Блок СЕКУНДЫ (Шрифт 120px)
        lv_obj_t *box_sec = lv_obj_create(cont);
        lv_obj_set_size(box_sec, 560, 200);
        lv_obj_set_style_bg_color(box_sec, lv_color_hex(0x0A0D14), 0);
        lv_obj_set_style_border_color(box_sec, lv_color_hex(0xFFE600), 0); // Желтая рамка
        lv_obj_set_style_border_width(box_sec, 4, 0);
        lv_obj_set_style_radius(box_sec, 14, 0);
        lv_obj_set_flex_flow(box_sec, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(box_sec, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(box_sec, 4, 0);

        lv_obj_t *title_sec = lv_label_create(box_sec);
        lv_obj_set_style_text_font(title_sec, &lv_font_dejavu_24, 0);
        lv_obj_set_style_text_color(title_sec, lv_color_hex(0x94A3B8), 0);
        lv_label_set_text(title_sec, "СЕКУНДЫ");

        s_lbl_sec = lv_label_create(box_sec);
        lv_obj_set_style_text_font(s_lbl_sec, &lv_font_digits_120, 0);
        lv_obj_set_style_text_color(s_lbl_sec, lv_color_hex(0xFFE600), 0); // Ярко-желтый
        lv_label_set_text(s_lbl_sec, "00");

        // 2. Блок МИКРОСЕКУНДЫ (Шрифт 120px)
        lv_obj_t *box_us = lv_obj_create(cont);
        lv_obj_set_size(box_us, 560, 210);
        lv_obj_set_style_bg_color(box_us, lv_color_hex(0x0A0D14), 0);
        lv_obj_set_style_border_color(box_us, lv_color_hex(0x00FF41), 0); // Зеленая рамка
        lv_obj_set_style_border_width(box_us, 4, 0);
        lv_obj_set_style_radius(box_us, 14, 0);
        lv_obj_set_flex_flow(box_us, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(box_us, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(box_us, 4, 0);

        lv_obj_t *title_us = lv_label_create(box_us);
        lv_obj_set_style_text_font(title_us, &lv_font_dejavu_24, 0);
        lv_obj_set_style_text_color(title_us, lv_color_hex(0x94A3B8), 0);
        lv_label_set_text(title_us, "МИКРОСЕКУНДЫ");

        s_lbl_us = lv_label_create(box_us);
        lv_obj_set_style_text_font(s_lbl_us, &lv_font_digits_120, 0);
        lv_obj_set_style_text_color(s_lbl_us, lv_color_hex(0x00FF41), 0); // Ярко-зеленый
        lv_label_set_text(s_lbl_us, ".000000");

        // 3. Оптический синхро-маркер (для камеры)
        s_box_sync = lv_obj_create(cont);
        lv_obj_set_size(s_box_sync, 560, 90);
        lv_obj_set_style_bg_color(s_box_sync, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(s_box_sync, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_border_width(s_box_sync, 3, 0);
        lv_obj_set_style_radius(s_box_sync, 10, 0);

        s_lbl_sync = lv_label_create(s_box_sync);
        lv_obj_set_style_text_font(s_lbl_sync, &lv_font_dejavu_32, 0);
        lv_obj_set_style_text_color(s_lbl_sync, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(s_lbl_sync, "SYNC [0]");
        lv_obj_center(s_lbl_sync);

        // 4. Панель кнопок
        lv_obj_t *btn_row = lv_obj_create(cont);
        lv_obj_set_size(btn_row, 560, 80);
        lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn_row, 0, 0);
        lv_obj_set_style_pad_all(btn_row, 0, 0);

        s_btn_start_stop = lv_button_create(btn_row);
        lv_obj_set_size(s_btn_start_stop, 130, 70);
        lv_obj_set_style_bg_color(s_btn_start_stop, lv_color_hex(0xD83B01), 0);
        lv_obj_add_event_cb(s_btn_start_stop, btn_start_stop_cb, LV_EVENT_CLICKED, NULL);
        s_lbl_btn_start = lv_label_create(s_btn_start_stop);
        lv_obj_set_style_text_font(s_lbl_btn_start, &lv_font_dejavu_24, 0);
        lv_label_set_text(s_lbl_btn_start, "ПАУЗА");
        lv_obj_center(s_lbl_btn_start);

        lv_obj_t *btn_reset = lv_button_create(btn_row);
        lv_obj_set_size(btn_reset, 130, 70);
        lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0x0078D4), 0);
        lv_obj_add_event_cb(btn_reset, btn_reset_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_btn_reset = lv_label_create(btn_reset);
        lv_obj_set_style_text_font(lbl_btn_reset, &lv_font_dejavu_24, 0);
        lv_label_set_text(lbl_btn_reset, "СБРОС");
        lv_obj_center(lbl_btn_reset);

        lv_obj_t *btn_mark = lv_button_create(btn_row);
        lv_obj_set_size(btn_mark, 130, 70);
        lv_obj_set_style_bg_color(btn_mark, lv_color_hex(0x8764B8), 0);
        lv_obj_add_event_cb(btn_mark, btn_mark_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_btn_mark = lv_label_create(btn_mark);
        lv_obj_set_style_text_font(lbl_btn_mark, &lv_font_dejavu_24, 0);
        lv_label_set_text(lbl_btn_mark, "MARK");
        lv_obj_center(lbl_btn_mark);

        lv_obj_t *btn_pat = lv_button_create(btn_row);
        lv_obj_set_size(btn_pat, 130, 70);
        lv_obj_set_style_bg_color(btn_pat, lv_color_hex(0x008272), 0);
        lv_obj_add_event_cb(btn_pat, btn_pattern_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_btn_pat = lv_label_create(btn_pat);
        lv_obj_set_style_text_font(lbl_btn_pat, &lv_font_dejavu_24, 0);
        lv_label_set_text(lbl_btn_pat, "МИРЫ");
        lv_obj_center(lbl_btn_pat);

        // 5. Диагностика
        s_lbl_stats = lv_label_create(cont);
        lv_obj_set_style_text_font(s_lbl_stats, &lv_font_dejavu_16, 0);
        lv_obj_set_style_text_color(s_lbl_stats, lv_color_hex(0x64748B), 0);
        lv_label_set_text(s_lbl_stats, "60 FPS | JC1060P470C");

        s_lbl_main_time = NULL;
    } else {
        // =================================================================
        // АЛЬБОМНЫЙ РЕЖИМ (1024 x 600)
        // =================================================================
        lv_obj_t *box_digits = lv_obj_create(s_scr_root);
        lv_obj_set_size(box_digits, 984, 250);
        lv_obj_set_pos(box_digits, 20, 30);
        lv_obj_set_style_bg_color(box_digits, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(box_digits, lv_color_hex(0x00FF41), 0);
        lv_obj_set_style_border_width(box_digits, 4, 0);
        lv_obj_set_style_radius(box_digits, 16, 0);

        s_lbl_main_time = lv_label_create(box_digits);
        lv_obj_set_style_text_font(s_lbl_main_time, &lv_font_digits_120, 0);
        lv_obj_set_style_text_color(s_lbl_main_time, lv_color_hex(0xFFE600), 0);
        lv_label_set_text(s_lbl_main_time, "00.000000");
        lv_obj_center(s_lbl_main_time);

        // Оптический маркер
        s_box_sync = lv_obj_create(s_scr_root);
        lv_obj_set_size(s_box_sync, 260, 140);
        lv_obj_set_pos(s_box_sync, 20, 310);
        lv_obj_set_style_border_color(s_box_sync, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_border_width(s_box_sync, 3, 0);
        lv_obj_set_style_radius(s_box_sync, 12, 0);

        s_lbl_sync = lv_label_create(s_box_sync);
        lv_obj_set_style_text_font(s_lbl_sync, &lv_font_dejavu_32, 0);
        lv_label_set_text(s_lbl_sync, "SYNC [0]");
        lv_obj_center(s_lbl_sync);

        // Кнопки
        s_btn_start_stop = lv_button_create(s_scr_root);
        lv_obj_set_size(s_btn_start_stop, 210, 80);
        lv_obj_set_pos(s_btn_start_stop, 310, 310);
        lv_obj_set_style_bg_color(s_btn_start_stop, lv_color_hex(0xD83B01), 0);
        lv_obj_add_event_cb(s_btn_start_stop, btn_start_stop_cb, LV_EVENT_CLICKED, NULL);
        s_lbl_btn_start = lv_label_create(s_btn_start_stop);
        lv_obj_set_style_text_font(s_lbl_btn_start, &lv_font_dejavu_24, 0);
        lv_label_set_text(s_lbl_btn_start, "ПАУЗА");
        lv_obj_center(s_lbl_btn_start);

        lv_obj_t *btn_reset = lv_button_create(s_scr_root);
        lv_obj_set_size(btn_reset, 210, 80);
        lv_obj_set_pos(btn_reset, 545, 310);
        lv_obj_set_style_bg_color(btn_reset, lv_color_hex(0x0078D4), 0);
        lv_obj_add_event_cb(btn_reset, btn_reset_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_btn_reset = lv_label_create(btn_reset);
        lv_obj_set_style_text_font(lbl_btn_reset, &lv_font_dejavu_24, 0);
        lv_label_set_text(lbl_btn_reset, "СБРОС");
        lv_obj_center(lbl_btn_reset);

        lv_obj_t *btn_mark = lv_button_create(s_scr_root);
        lv_obj_set_size(btn_mark, 210, 80);
        lv_obj_set_pos(btn_mark, 780, 310);
        lv_obj_set_style_bg_color(btn_mark, lv_color_hex(0x8764B8), 0);
        lv_obj_add_event_cb(btn_mark, btn_mark_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_btn_mark = lv_label_create(btn_mark);
        lv_obj_set_style_text_font(lbl_btn_mark, &lv_font_dejavu_24, 0);
        lv_label_set_text(lbl_btn_mark, "MARK");
        lv_obj_center(lbl_btn_mark);

        s_lbl_stats = lv_label_create(s_scr_root);
        lv_obj_set_style_text_font(s_lbl_stats, &lv_font_dejavu_24, 0);
        lv_obj_set_style_text_color(s_lbl_stats, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_pos(s_lbl_stats, 310, 420);
        lv_label_set_text(s_lbl_stats, "60 FPS | JC1060P470C 1024x600");

        s_lbl_sec = NULL;
        s_lbl_us = NULL;
    }
}

// =========================================================================
// ПОВОРОТ ЭКРАНА
// =========================================================================
void ui_stopwatch_set_rotation(int deg)
{
    if (!s_disp) {
        ESP_LOGW(TAG, "Дисплей еще не привязан, сохраняем угол %d°", deg);
        s_current_rotation_deg = deg;
        return;
    }

    lv_display_rotation_t rot;
    switch (deg) {
        case 90:  rot = LV_DISPLAY_ROTATION_90; break;
        case 180: rot = LV_DISPLAY_ROTATION_180; break;
        case 270: rot = LV_DISPLAY_ROTATION_270; break;
        default:  rot = LV_DISPLAY_ROTATION_0; deg = 0; break;
    }

    s_current_rotation_deg = deg;
    ESP_LOGI(TAG, "Установка угла поворота дисплея: %d°", deg);
    lv_display_set_rotation(s_disp, rot);
    build_ui();
}

int ui_stopwatch_get_rotation(void)
{
    return s_current_rotation_deg;
}

void ui_stopwatch_rebuild(void)
{
    build_ui();
}

// =========================================================================
// ИНИЦИАЛИЗАЦИЯ
// =========================================================================
void ui_stopwatch_init(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Инициализация микросекундомера и тестовых таблиц (LVGL 9)...");
    s_disp = disp;

    s_start_us = esp_timer_get_time();
    s_fps_last_time_us = s_start_us;
    s_running = true;

    s_scr_root = lv_screen_active();

    // Инициализация модуля тестовых шаблонов
    ui_test_patterns_init(s_scr_root);

    // Применяем начальный поворот (90° по умолчанию для вертикального стенда)
    ui_stopwatch_set_rotation(s_current_rotation_deg);

    // Запуск таймера обновления экрана 60 раз в секунду (период 16 мс)
    if (!s_render_timer) {
        s_render_timer = lv_timer_create(stopwatch_timer_cb, 16, NULL);
    }
}
