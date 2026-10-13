/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * Экран тестирования и калибровки тача (Touch Test / Calibration Mode)
 * для 7-дюймового дисплея JD9165 (1024x600).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "app_lcd.h"
#include "camos/ui.h"
#include "camos/ui_icons.h"
#include "camos/touch.h"
#include "browser/touch_calib.h"

static const char *TAG = "touch_calib";

#define LCD_W EXAMPLE_LCD_H_RES
#define LCD_H EXAMPLE_LCD_V_RES

/* Зоны 4 углов (120x120 px) */
#define CORNER_SIZE 120

typedef struct {
    int x0, y0, x1, y1;
    int cx, cy;
    const char *name;
    const char *coord_hint;
} corner_zone_t;

static const corner_zone_t s_corners[4] = {
    { 0, 0, CORNER_SIZE, CORNER_SIZE, 60, 60, "TL", "(0, 0)" },
    { LCD_W - CORNER_SIZE, 0, LCD_W, CORNER_SIZE, LCD_W - 60, 60, "TR", "(1024, 0)" },
    { 0, LCD_H - CORNER_SIZE, CORNER_SIZE, LCD_H, 60, LCD_H - 60, "BL", "(0, 600)" },
    { LCD_W - CORNER_SIZE, LCD_H - CORNER_SIZE, LCD_W, LCD_H, LCD_W - 60, LCD_H - 60, "BR", "(1024, 600)" }
};

/* Верхняя HUD панель */
#define HUD_X 130
#define HUD_Y 6
#define HUD_W 764
#define HUD_H 68

static const ui_button_t s_btn_reset = {
    HUD_X + HUD_W - 180, HUD_Y + 12, 80, 44, "СБРОС", 1, 1
};
static const ui_button_t s_btn_exit = {
    HUD_X + HUD_W - 90, HUD_Y + 12, 80, 44, "ВЫХОД", 2, 1
};

/* История следа касания */
#define TRAIL_MAX_POINTS 256
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t is_break;
} trail_pt_t;

static struct {
    uint8_t tested_corners;             /* битовая маска проверенных углов (биты 0..3) */
    int current_corner;                 /* угол под пальцем прямо сейчас (-1 если нет) */

    touch_point_t last_pts[TOUCH_MAX_POINTS];
    int last_n_pts;
    bool is_touching;

    trail_pt_t trail[TRAIL_MAX_POINTS];
    int trail_count;
    int trail_head;
} s_tc;

void touch_calib_reset(void)
{
    s_tc.tested_corners = 0;
    s_tc.current_corner = -1;
    s_tc.last_n_pts = 0;
    s_tc.is_touching = false;
    s_tc.trail_count = 0;
    s_tc.trail_head = 0;
    touch_diag_reset_extremes();
    ESP_LOGI(TAG, "экран калибровки тача сброшен");
}

static void trail_add_point(int x, int y, bool is_new_stroke)
{
    s_tc.trail[s_tc.trail_head].x = (int16_t)x;
    s_tc.trail[s_tc.trail_head].y = (int16_t)y;
    s_tc.trail[s_tc.trail_head].is_break = is_new_stroke ? 1 : 0;

    s_tc.trail_head = (s_tc.trail_head + 1) % TRAIL_MAX_POINTS;
    if (s_tc.trail_count < TRAIL_MAX_POINTS) {
        s_tc.trail_count++;
    }
}

bool touch_calib_handle_input(const touch_point_t *pts, int n_pts,
                              bool tap_edge, int tap_x, int tap_y)
{
    /* Проверка нажатия кнопок HUD панели */
    if (tap_edge) {
        if (ui_button_hit(&s_btn_exit, tap_x, tap_y)) {
            ESP_LOGI(TAG, "нажата кнопка ВЫХОД из калибровки");
            return true;
        }
        if (ui_button_hit(&s_btn_reset, tap_x, tap_y)) {
            touch_calib_reset();
            return false;
        }
    }

    if (n_pts > 0 && pts != NULL) {
        s_tc.is_touching = true;
        s_tc.last_n_pts = (n_pts > TOUCH_MAX_POINTS) ? TOUCH_MAX_POINTS : n_pts;
        for (int i = 0; i < s_tc.last_n_pts; i++) {
            s_tc.last_pts[i] = pts[i];
        }

        /* Добавляем точку в след */
        trail_add_point(pts[0].x, pts[0].y, tap_edge);

        /* Проверяем попадание в угол экрана для основного пальца */
        int cur_corner = -1;
        for (int i = 0; i < 4; i++) {
            const corner_zone_t *c = &s_corners[i];
            if (pts[0].x >= c->x0 && pts[0].x <= c->x1 &&
                pts[0].y >= c->y0 && pts[0].y <= c->y1) {
                cur_corner = i;
                s_tc.tested_corners |= (1 << i);
                break;
            }
        }
        s_tc.current_corner = cur_corner;
    } else {
        s_tc.is_touching = false;
        s_tc.current_corner = -1;
    }

    return false;
}

/* Отрисовка координатной сетки и центральных осей */
static void draw_grid(uint16_t *fb)
{
    /* Заливка фона */
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, LCD_H, UI_COLOR_BG);

    /* Координатная сетка: вертикальные линии каждые 128 px */
    for (int x = 128; x < LCD_W; x += 128) {
        uint16_t col = (x == 512) ? UI_COLOR_BORDER : 0x0986;
        int thick = (x == 512) ? 2 : 1;
        ui_fill_rect(fb, LCD_W, LCD_H, x - (thick / 2), 0, x + ((thick + 1) / 2), LCD_H, col);
    }

    /* Координатная сетка: горизонтальные линии каждые 100 px */
    for (int y = 100; y < LCD_H; y += 100) {
        uint16_t col = (y == 300) ? UI_COLOR_BORDER : 0x0986;
        int thick = (y == 300) ? 2 : 1;
        ui_fill_rect(fb, LCD_W, LCD_H, 0, y - (thick / 2), LCD_W, y + ((thick + 1) / 2), col);
    }

    /* Подписи центральных осей */
    ui_text(fb, LCD_W, LCD_H, 516, 304, "ЦЕНТР (512, 300)", 1, UI_COLOR_DIM, UI_COLOR_BG);
}

/* Отрисовка непрерывного следа касания */
static void draw_trail(uint16_t *fb)
{
    if (s_tc.trail_count < 2) {
        return;
    }

    int start_idx = (s_tc.trail_head - s_tc.trail_count + TRAIL_MAX_POINTS) % TRAIL_MAX_POINTS;
    int prev_x = -1, prev_y = -1;

    for (int i = 0; i < s_tc.trail_count; i++) {
        int idx = (start_idx + i) % TRAIL_MAX_POINTS;
        const trail_pt_t *p = &s_tc.trail[idx];

        if (!p->is_break && prev_x >= 0 && prev_y >= 0) {
            ui_line(fb, LCD_W, LCD_H, prev_x, prev_y, p->x, p->y, UI_COLOR_ACCENT2);
            /* Толщина 2 px для четкой видимости */
            ui_line(fb, LCD_W, LCD_H, prev_x + 1, prev_y, p->x + 1, p->y, UI_COLOR_ACCENT2);
        }
        prev_x = p->x;
        prev_y = p->y;
    }
}

/* Отрисовка 4 угловых зон и мишеней */
static void draw_corners(uint16_t *fb)
{
    for (int i = 0; i < 4; i++) {
        const corner_zone_t *c = &s_corners[i];
        bool is_hit = (s_tc.current_corner == i);
        bool is_tested = (s_tc.tested_corners & (1 << i)) != 0;

        /* Фон карточки угла */
        uint16_t bg_col = is_hit ? UI_COLOR_ACCENT : (is_tested ? UI_COLOR_PANEL : UI_COLOR_KEY);
        uint16_t border_col = is_hit ? UI_COLOR_FG : (is_tested ? UI_COLOR_OK : UI_COLOR_BORDER);

        ui_rrect(fb, LCD_W, LCD_H, c->x0, c->y0, c->x1, c->y1, UI_R_CARD, bg_col);
        ui_rrect_frame(fb, LCD_W, LCD_H, c->x0, c->y0, c->x1, c->y1, UI_R_CARD, border_col);
        if (is_hit || is_tested) {
            /* Дополнительная внутренняя рамка для объема */
            ui_rrect_frame(fb, LCD_W, LCD_H, c->x0 + 1, c->y0 + 1, c->x1 - 1, c->y1 - 1, UI_R_CARD, border_col);
        }

        /* Мишень (концентрические окружности и перекрестие) */
        uint16_t target_col = is_hit ? UI_COLOR_BG : (is_tested ? UI_COLOR_OK : UI_COLOR_DIM);
        ui_circle_frame(fb, LCD_W, LCD_H, c->cx, c->cy, 24, target_col);
        ui_circle_frame(fb, LCD_W, LCD_H, c->cx, c->cy, 12, target_col);
        ui_circle(fb, LCD_W, LCD_H, c->cx, c->cy, 3, target_col);

        /* Лучи мишени */
        ui_line(fb, LCD_W, LCD_H, c->cx - 30, c->cy, c->cx + 30, c->cy, target_col);
        ui_line(fb, LCD_W, LCD_H, c->cx, c->cy - 30, c->cx, c->cy + 30, target_col);

        /* Иконка галочки при подтверждении */
        if (is_tested && !is_hit) {
            ui_icon(&ui_icon_check, fb, LCD_W, LCD_H, c->cx - 8, c->cy - 8, 1, UI_COLOR_OK);
        }

        /* Текстовая метка угла */
        char lbl[32];
        if (is_hit) {
            snprintf(lbl, sizeof(lbl), "%s НАЖАТ!", c->name);
            uint16_t txt_col = UI_COLOR_BG;
            int tw = ui_text_width(lbl, 1);
            int tx = c->cx - tw / 2;
            int ty = (c->y0 == 0) ? (c->y1 - 16) : (c->y0 + 6);
            ui_text(fb, LCD_W, LCD_H, tx, ty, lbl, 1, txt_col, bg_col);
        } else if (is_tested) {
            snprintf(lbl, sizeof(lbl), "%s OK %s", c->name, c->coord_hint);
            uint16_t txt_col = UI_COLOR_OK;
            int tw = ui_text_width(lbl, 1);
            int tx = c->cx - tw / 2;
            int ty = (c->y0 == 0) ? (c->y1 - 16) : (c->y0 + 6);
            ui_text(fb, LCD_W, LCD_H, tx, ty, lbl, 1, txt_col, bg_col);
        } else {
            snprintf(lbl, sizeof(lbl), "%s %s", c->name, c->coord_hint);
            uint16_t txt_col = UI_COLOR_DIM;
            int tw = ui_text_width(lbl, 1);
            int tx = c->cx - tw / 2;
            int ty = (c->y0 == 0) ? (c->y1 - 16) : (c->y0 + 6);
            ui_text(fb, LCD_W, LCD_H, tx, ty, lbl, 1, txt_col, bg_col);
        }
    }
}

/* Отрисовка интерактивного перекрестия и бейджа координат */
static void draw_crosshair_and_badge(uint16_t *fb)
{
    if (!s_tc.is_touching || s_tc.last_n_pts <= 0) {
        return;
    }

    /* Отрисовка мультитач точек (если точек > 1) */
    for (int i = 1; i < s_tc.last_n_pts; i++) {
        int px = s_tc.last_pts[i].x;
        int py = s_tc.last_pts[i].y;
        ui_circle_frame(fb, LCD_W, LCD_H, px, py, 16, UI_COLOR_WARN);
        ui_circle_frame(fb, LCD_W, LCD_H, px, py, 26, UI_COLOR_WARN);
        ui_circle(fb, LCD_W, LCD_H, px, py, 4, UI_COLOR_FG);
        char mt_lbl[32];
        snprintf(mt_lbl, sizeof(mt_lbl), "#%d (%d,%d)", i + 1, px, py);
        ui_box_text(fb, LCD_W, LCD_H, px + 10, py - 18, mt_lbl, 1,
                    UI_COLOR_WARN, UI_COLOR_PANEL, UI_COLOR_BORDER, 2);
    }

    /* Основная точка касания #0: полноэкранное перекрестие */
    int x = s_tc.last_pts[0].x;
    int y = s_tc.last_pts[0].y;
    int raw_x = s_tc.last_pts[0].raw_x;
    int raw_y = s_tc.last_pts[0].raw_y;

    /* Горизонтальная линия во весь экран (толщина 2 px) */
    ui_fill_rect(fb, LCD_W, LCD_H, 0, y - 1, LCD_W, y + 2, UI_COLOR_ACCENT);

    /* Вертикальная линия во весь экран (толщина 2 px) */
    ui_fill_rect(fb, LCD_W, LCD_H, x - 1, 0, x + 2, LCD_H, UI_COLOR_ACCENT);

    /* Концентрические кольца прицела в точке касания */
    ui_circle(fb, LCD_W, LCD_H, x, y, 4, UI_COLOR_FG);
    ui_circle_frame(fb, LCD_W, LCD_H, x, y, 16, UI_COLOR_ACCENT);
    ui_circle_frame(fb, LCD_W, LCD_H, x, y, 28, UI_COLOR_ACCENT2);

    /* Плавающий бейдж координат рядом с пальцем */
    int bw = 210, bh = 46;
    int bx = (x > LCD_W - bw - 20) ? (x - bw - 20) : (x + 24);
    int by = (y < bh + 20) ? (y + 24) : (y - bh - 16);

    ui_rrect(fb, LCD_W, LCD_H, bx, by, bx + bw, by + bh, UI_R_CARD, UI_COLOR_PANEL);
    ui_rrect_frame(fb, LCD_W, LCD_H, bx, by, bx + bw, by + bh, UI_R_CARD, UI_COLOR_ACCENT);

    char c_str[64];
    snprintf(c_str, sizeof(c_str), "X: %d  Y: %d", x, y);
    ui_text(fb, LCD_W, LCD_H, bx + 10, by + 6, c_str, UI_SCALE_TEXT, UI_COLOR_FG, UI_COLOR_PANEL);

    char r_str[64];
    snprintf(r_str, sizeof(r_str), "sensor raw: (%d, %d)", raw_x, raw_y);
    ui_text(fb, LCD_W, LCD_H, bx + 10, by + 26, r_str, 1, UI_COLOR_ACCENT, UI_COLOR_PANEL);
}

/* Отрисовка верхней HUD панели с кнопками и статистикой */
static void draw_hud(uint16_t *fb)
{
    /* Панель */
    ui_rrect(fb, LCD_W, LCD_H, HUD_X, HUD_Y, HUD_X + HUD_W, HUD_Y + HUD_H,
             UI_R_CARD, UI_COLOR_PANEL);
    ui_rrect_frame(fb, LCD_W, LCD_H, HUD_X, HUD_Y, HUD_X + HUD_W, HUD_Y + HUD_H,
                   UI_R_CARD, UI_COLOR_BORDER);

    /* Кнопки СБРОС и ВЫХОД */
    ui_button(fb, LCD_W, LCD_H, &s_btn_reset, false);
    ui_button(fb, LCD_W, LCD_H, &s_btn_exit, false);

    /* Диагностика сенсора */
    touch_diag_extremes_t diag;
    touch_diag_get_extremes(&diag);

    char line1[128];
    snprintf(line1, sizeof(line1), "Чип: %s | Касание: %s",
             touch_chip_name(),
             s_tc.is_touching ? "АКТИВНО" : "Ожидание...");
    ui_text(fb, LCD_W, LCD_H, HUD_X + 16, HUD_Y + 10, line1, 1,
            s_tc.is_touching ? UI_COLOR_ACCENT : UI_COLOR_FG, UI_COLOR_PANEL);

    char line2[128];
    if (diag.total_points > 0) {
        snprintf(line2, sizeof(line2), "Экран X:[%d..%d] Y:[%d..%d] | Точек: %lu",
                 diag.screen_min_x, diag.screen_max_x,
                 diag.screen_min_y, diag.screen_max_y,
                 (unsigned long)diag.total_points);
    } else {
        snprintf(line2, sizeof(line2), "Коснитесь 4 углов экрана для проверки точности");
    }
    ui_text(fb, LCD_W, LCD_H, HUD_X + 16, HUD_Y + 28, line2, 1, UI_COLOR_DIM, UI_COLOR_PANEL);

    char line3[128];
    if (diag.total_points > 0 && diag.raw_max_x >= 0) {
        snprintf(line3, sizeof(line3), "Raw сенсора: X=[%d..%d], Y=[%d..%d]",
                 diag.raw_min_x, diag.raw_max_x, diag.raw_min_y, diag.raw_max_y);
    } else {
        snprintf(line3, sizeof(line3), "Перекрестие и координаты отслеживаются онлайн");
    }
    ui_text(fb, LCD_W, LCD_H, HUD_X + 16, HUD_Y + 46, line3, 1, UI_COLOR_ACCENT2, UI_COLOR_PANEL);

    /* Индикаторы 4 углов (в центре HUD панели) */
    int ind_x = HUD_X + 430;
    int ind_y = HUD_Y + 12;
    const char *c_names[4] = { "TL", "TR", "BL", "BR" };

    for (int i = 0; i < 4; i++) {
        bool tested = (s_tc.tested_corners & (1 << i)) != 0;
        int bx = ind_x + i * 36;
        ui_rrect(fb, LCD_W, LCD_H, bx, ind_y, bx + 32, ind_y + 22, UI_R_KEY,
                 tested ? UI_COLOR_OK : UI_COLOR_KEY);
        ui_rrect_frame(fb, LCD_W, LCD_H, bx, ind_y, bx + 32, ind_y + 22, UI_R_KEY,
                       tested ? UI_COLOR_FG : UI_COLOR_BORDER);
        ui_text(fb, LCD_W, LCD_H, bx + 8, ind_y + 7, c_names[i], 1,
                tested ? UI_COLOR_BG : UI_COLOR_DIM, tested ? UI_COLOR_OK : UI_COLOR_KEY);
    }

    /* Надпись статуса калибровки всех 4 углов */
    if ((s_tc.tested_corners & 0x0F) == 0x0F) {
        const char *ok_msg = "ВСЕ 4 УГЛА OK!";
        ui_text(fb, LCD_W, LCD_H, ind_x + 10, ind_y + 32, ok_msg, 1, UI_COLOR_OK, UI_COLOR_PANEL);
    } else {
        char need_msg[64];
        int left = 4;
        for (int i = 0; i < 4; i++) {
            if (s_tc.tested_corners & (1 << i)) left--;
        }
        snprintf(need_msg, sizeof(need_msg), "Осталось углов: %d", left);
        ui_text(fb, LCD_W, LCD_H, ind_x, ind_y + 32, need_msg, 1, UI_COLOR_WARN, UI_COLOR_PANEL);
    }
}

void touch_calib_draw(uint16_t *fb)
{
    if (fb == NULL) {
        return;
    }

    /* 1. Сетка и разметка */
    draw_grid(fb);

    /* 2. След касания пальцем */
    draw_trail(fb);

    /* 3. Угловые мишени с подсветкой */
    draw_corners(fb);

    /* 4. Полноэкранное перекрестие и координаты касания */
    draw_crosshair_and_badge(fb);

    /* 5. Верхняя управляющая HUD-панель */
    draw_hud(fb);
}
