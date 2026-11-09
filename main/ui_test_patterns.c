#include "ui_test_patterns.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "fonts/cyrillic_fonts.h"

static const char *TAG = "UI_PATTERNS";

static pattern_mode_t s_current_mode = PATTERN_MODE_STOPWATCH;
static lv_obj_t *s_root_scr = NULL;
static lv_obj_t *s_pattern_cont = NULL;

// Внешняя функция перестройки секундомера из ui_stopwatch.c
extern void ui_stopwatch_rebuild(void);

static const char *s_mode_names[] = {
    "STOPWATCH",
    "FOCUS_SIEMENS",
    "COLOR_BARS",
    "GRID_DISTORTION",
    "GRAYSCALE_WDR"
};

const char* ui_test_patterns_get_name(pattern_mode_t mode)
{
    if (mode < PATTERN_MODE_MAX) {
        return s_mode_names[mode];
    }
    return "UNKNOWN";
}

pattern_mode_t ui_test_patterns_get_mode(void)
{
    return s_current_mode;
}

// =========================================================================
// ОБРАБОТЧИКИ НАВИГАЦИИ
// =========================================================================
static void btn_next_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_test_patterns_next();
}

static void btn_exit_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_test_patterns_set_mode(PATTERN_MODE_STOPWATCH);
}

// Создание адаптивной шапки и подвала для портретного/альбомного режима
static void add_header_footer(lv_obj_t *parent, int32_t w, int32_t h, const char *title)
{
    // 1. Верхняя информационная панель (Header)
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, w, 50);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_90, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_obj_t *lbl_title = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_title, &lv_font_dejavu_24, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x00FF41), 0);
    lv_label_set_text(lbl_title, title);
    lv_obj_center(lbl_title);

    // 2. Нижняя панель сенсорных кнопок управления (Footer)
    lv_obj_t *footer = lv_obj_create(parent);
    lv_obj_set_size(footer, w, 70);
    lv_obj_set_pos(footer, 0, h - 70);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_90, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 6, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Кнопка переключения на следующий шаблон
    lv_obj_t *btn_next = lv_button_create(footer);
    lv_obj_set_size(btn_next, (w - 30) / 2, 56);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x008272), 0);
    lv_obj_add_event_cb(btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_next = lv_label_create(btn_next);
    lv_obj_set_style_text_font(lbl_next, &lv_font_dejavu_24, 0);
    lv_label_set_text(lbl_next, "СЛЕД. МИРА ❯");
    lv_obj_center(lbl_next);

    // Кнопка возврата к секундомеру
    lv_obj_t *btn_exit = lv_button_create(footer);
    lv_obj_set_size(btn_exit, (w - 30) / 2, 56);
    lv_obj_set_style_bg_color(btn_exit, lv_color_hex(0xD83B01), 0);
    lv_obj_add_event_cb(btn_exit, btn_exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_exit = lv_label_create(btn_exit);
    lv_obj_set_style_text_font(lbl_exit, &lv_font_dejavu_24, 0);
    lv_label_set_text(lbl_exit, "⏱ СЕКУНДОМЕР");
    lv_obj_center(lbl_exit);
}

// =========================================================================
// 1. АДАПТИВНАЯ ТАБЛИЦА ФОКУСА (SIEMENS STAR & MTF RESOLUTION)
// =========================================================================
static void build_focus_pattern(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    int32_t y_top = 50;
    int32_t y_bottom = h - 70;
    int32_t work_h = y_bottom - y_top;
    int32_t center_x = w / 2;
    int32_t center_y = y_top + (work_h / 2);

    // 1. Концентрические кольца в центре (диаметры точно рассчитаны под ширину W=600)
    int max_r = (w / 2) - 30; // 270 px
    int num_rings = 10;
    for (int i = 0; i < num_rings; i++) {
        int r = max_r * (num_rings - i) / num_rings;
        lv_obj_t *ring = lv_obj_create(parent);
        lv_obj_set_size(ring, r * 2, r * 2);
        lv_obj_set_pos(ring, center_x - r, center_y - r);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ring, (i < 3) ? 4 : ((i < 7) ? 3 : 2), 0);
        lv_obj_set_style_border_color(ring, (i % 2 == 0) ? lv_color_hex(0x000000) : lv_color_hex(0x555555), 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    }

    // 2. Центральная фокусная точка
    lv_obj_t *c_dot = lv_obj_create(parent);
    lv_obj_set_size(c_dot, 10, 10);
    lv_obj_set_pos(c_dot, center_x - 5, center_y - 5);
    lv_obj_set_style_radius(c_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c_dot, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(c_dot, 0, 0);

    // 3. Четыре угловые шахматные миры (краевая резкость и астигматизм)
    int box_sz = 64; // 4x4 клетки по 16px
    int corners[4][2] = {
        {16, y_top + 16},
        {w - 16 - box_sz, y_top + 16},
        {16, y_bottom - 16 - box_sz},
        {w - 16 - box_sz, y_bottom - 16 - box_sz}
    };
    for (int c = 0; c < 4; c++) {
        lv_obj_t *chk = lv_obj_create(parent);
        lv_obj_set_size(chk, box_sz, box_sz);
        lv_obj_set_pos(chk, corners[c][0], corners[c][1]);
        lv_obj_set_style_pad_all(chk, 0, 0);
        lv_obj_set_style_border_width(chk, 2, 0);
        lv_obj_set_style_border_color(chk, lv_color_hex(0x000000), 0);
        lv_obj_set_style_radius(chk, 0, 0);

        for (int r = 0; r < 4; r++) {
            for (int col = 0; col < 4; col++) {
                if ((r + col) % 2 == 0) {
                    lv_obj_t *tile = lv_obj_create(chk);
                    lv_obj_set_size(tile, 16, 16);
                    lv_obj_set_pos(tile, col * 16, r * 16);
                    lv_obj_set_style_bg_color(tile, lv_color_hex(0x000000), 0);
                    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
                    lv_obj_set_style_border_width(tile, 0, 0);
                    lv_obj_set_style_radius(tile, 0, 0);
                }
            }
        }
    }

    // 4. Тонкие горизонтальные и вертикальные решетки разрешения
    int thicknesses[] = {1, 2, 3, 5, 8};
    for (int b = 0; b < 5; b++) {
        // Выше центра
        lv_obj_t *h_line = lv_obj_create(parent);
        lv_obj_set_size(h_line, 120, thicknesses[b]);
        lv_obj_set_pos(h_line, center_x - 60, center_y - 180 - b * 16);
        lv_obj_set_style_bg_color(h_line, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(h_line, 0, 0);

        // Ниже центра
        lv_obj_t *v_line = lv_obj_create(parent);
        lv_obj_set_size(v_line, thicknesses[b], 80);
        lv_obj_set_pos(v_line, center_x - 40 + b * 20, center_y + 120);
        lv_obj_set_style_bg_color(v_line, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(v_line, 0, 0);
    }

    add_header_footer(parent, w, h, "🎯 ЮСТИРОВКА ФОКУСА (MTF)");
}

// =========================================================================
// 2. АДАПТИВНЫЕ ЦВЕТНЫЕ ПОЛОСЫ (SMPTE COLOR BARS)
// =========================================================================
static void build_color_bars_pattern(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);

    int32_t y_top = 50;
    int32_t y_bottom = h - 70;
    int32_t work_h = y_bottom - y_top;

    int32_t top_h = (work_h * 75) / 100;
    int32_t btm_h = work_h - top_h;

    uint32_t colors[8] = {
        0xFFFFFF, // Белый 100%
        0xFFFF00, // Желтый
        0x00FFFF, // Циан
        0x00FF00, // Зеленый
        0xFF00FF, // Маджента
        0xFF0000, // Красный
        0x0000FF, // Синий
        0x000000  // Черный
    };

    // Верхние 75% рабочей высоты: 8 цветных полос
    int32_t bar_w = w / 8; // Ровно 75 px при W=600!
    for (int i = 0; i < 8; i++) {
        lv_obj_t *bar = lv_obj_create(parent);
        int32_t cur_w = (i == 7) ? (w - (bar_w * 7)) : bar_w;
        lv_obj_set_size(bar, cur_w, top_h);
        lv_obj_set_pos(bar, i * bar_w, y_top);
        lv_obj_set_style_bg_color(bar, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
    }

    // Нижние 25% рабочей высоты: референсные полосы насыщения и шкалы серого
    uint32_t btm_colors[8] = {
        0x003366, 0xFFFFFF, 0x330066, 0x1A1A1A,
        0x000000, 0x333333, 0x666666, 0x999999
    };
    for (int i = 0; i < 8; i++) {
        lv_obj_t *b_bar = lv_obj_create(parent);
        int32_t cur_w = (i == 7) ? (w - (bar_w * 7)) : bar_w;
        lv_obj_set_size(b_bar, cur_w, btm_h);
        lv_obj_set_pos(b_bar, i * bar_w, y_top + top_h);
        lv_obj_set_style_bg_color(b_bar, lv_color_hex(btm_colors[i]), 0);
        lv_obj_set_style_bg_opa(b_bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b_bar, 0, 0);
        lv_obj_set_style_radius(b_bar, 0, 0);
    }

    add_header_footer(parent, w, h, "🌈 SMPTE COLOR BARS (ЦВЕТ & AWB)");
}

// =========================================================================
// 3. АДАПТИВНАЯ СЕТКА И ДИСТОРСИЯ (GRID & DISTORTION)
// =========================================================================
static void build_grid_pattern(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x050810), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    int32_t y_top = 50;
    int32_t y_bottom = h - 70;
    int32_t work_h = y_bottom - y_top;
    int32_t center_x = w / 2;
    int32_t center_y = y_top + (work_h / 2);

    int step = 50;

    // Вертикальные линии (строго в пределах W)
    for (int x = step; x < w; x += step) {
        lv_obj_t *vline = lv_obj_create(parent);
        bool is_mid = (x == center_x);
        lv_obj_set_size(vline, is_mid ? 2 : 1, work_h);
        lv_obj_set_pos(vline, x, y_top);
        lv_obj_set_style_bg_color(vline, is_mid ? lv_color_hex(0x00FF41) : lv_color_hex(0x1E3A5F), 0);
        lv_obj_set_style_bg_opa(vline, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(vline, 0, 0);
        lv_obj_set_style_radius(vline, 0, 0);
    }

    // Горизонтальные линии (строго в пределах work_h)
    for (int y = y_top + step; y < y_bottom; y += step) {
        lv_obj_t *hline = lv_obj_create(parent);
        bool is_mid = (y >= center_y - 25 && y <= center_y + 25);
        lv_obj_set_size(hline, w, is_mid ? 2 : 1);
        lv_obj_set_pos(hline, 0, y);
        lv_obj_set_style_bg_color(hline, is_mid ? lv_color_hex(0x00FF41) : lv_color_hex(0x1E3A5F), 0);
        lv_obj_set_style_bg_opa(hline, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(hline, 0, 0);
        lv_obj_set_style_radius(hline, 0, 0);
    }

    // Окружности дисторсии, рассчитанные строго под W=600
    int c_radii[] = {80, 160, 240, (w / 2) - 20};
    for (int i = 0; i < 4; i++) {
        int r = c_radii[i];
        lv_obj_t *circ = lv_obj_create(parent);
        lv_obj_set_size(circ, r * 2, r * 2);
        lv_obj_set_pos(circ, center_x - r, center_y - r);
        lv_obj_set_style_radius(circ, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(circ, 2, 0);
        lv_obj_set_style_border_color(circ, lv_color_hex(0xFFE600), 0);
        lv_obj_set_style_bg_opa(circ, LV_OPA_TRANSP, 0);
    }

    add_header_footer(parent, w, h, "📐 СЕТКА ДИСТОРСИИ & LDC");
}

// =========================================================================
// 4. АДАПТИВНЫЙ СЕРЫЙ КЛИН (16-STEP GRAYSCALE WDR ДЛЯ ПОРТРЕТНОГО ЭКРАНА)
// =========================================================================
static void build_grayscale_pattern(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);

    int32_t y_top = 50;
    int32_t y_bottom = h - 70;
    int32_t work_h = y_bottom - y_top;

    // В портретном режиме (600x1024) 16 ступеней строятся горизонтальными полосами по высоте!
    // Каждая ступень имеет ширину w=600px и высоту step_h = work_h / 16 (~55 px)!
    int32_t step_h = work_h / 16;

    for (int i = 0; i < 16; i++) {
        uint8_t val = (uint8_t)(i * 255 / 15);
        uint32_t col = (val << 16) | (val << 8) | val;

        int32_t cur_h = (i == 15) ? (work_h - (step_h * 15)) : step_h;
        lv_obj_t *step = lv_obj_create(parent);
        lv_obj_set_size(step, w, cur_h);
        lv_obj_set_pos(step, 0, y_top + i * step_h);
        lv_obj_set_style_bg_color(step, lv_color_hex(col), 0);
        lv_obj_set_style_bg_opa(step, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(step, 0, 0);
        lv_obj_set_style_radius(step, 0, 0);
        lv_obj_set_style_pad_all(step, 0, 0);

        // Крупная контрастная подпись процента и HEX-яркости
        lv_obj_t *lbl = lv_label_create(step);
        lv_obj_set_style_text_font(lbl, &lv_font_dejavu_24, 0);
        lv_obj_set_style_text_color(lbl, (val > 128) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text_fmt(lbl, "СТУПЕНЬ %2d: %3d%%  (#%02X%02X%02X)", i, (i * 100) / 15, val, val, val);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 25, 0);
    }

    add_header_footer(parent, w, h, "🌓 16 СТУПЕНЕЙ СЕРОГО (WDR & GAMMA)");
}

// =========================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// =========================================================================
void ui_test_patterns_init(lv_obj_t *root_scr)
{
    s_root_scr = root_scr;
    s_current_mode = PATTERN_MODE_STOPWATCH;
}

void ui_test_patterns_set_mode(pattern_mode_t mode)
{
    if (mode >= PATTERN_MODE_MAX) mode = PATTERN_MODE_STOPWATCH;
    s_current_mode = mode;

    ESP_LOGI(TAG, "Смена режима отображения на [%d] %s", mode, ui_test_patterns_get_name(mode));

    if (!s_root_scr) return;

    // Очищаем экран
    lv_obj_clean(s_root_scr);

    if (s_current_mode == PATTERN_MODE_STOPWATCH) {
        // Восстанавливаем интерфейс секундомера
        ui_stopwatch_rebuild();
        return;
    }

    // Получаем реальные физические габариты с учетом угла поворота
    int32_t w = lv_obj_get_width(s_root_scr);
    int32_t h = lv_obj_get_height(s_root_scr);
    ESP_LOGI(TAG, "Отрисовка шаблона под реальный размер экрана: W=%ld, H=%ld", (long)w, (long)h);

    // Создаем контейнер тестового паттерна
    s_pattern_cont = lv_obj_create(s_root_scr);
    lv_obj_set_size(s_pattern_cont, w, h);
    lv_obj_set_style_pad_all(s_pattern_cont, 0, 0);
    lv_obj_set_style_border_width(s_pattern_cont, 0, 0);
    lv_obj_set_style_radius(s_pattern_cont, 0, 0);

    switch (s_current_mode) {
        case PATTERN_MODE_FOCUS:
            build_focus_pattern(s_pattern_cont, w, h);
            break;
        case PATTERN_MODE_COLOR_BARS:
            build_color_bars_pattern(s_pattern_cont, w, h);
            break;
        case PATTERN_MODE_GRID:
            build_grid_pattern(s_pattern_cont, w, h);
            break;
        case PATTERN_MODE_GRAYSCALE:
            build_grayscale_pattern(s_pattern_cont, w, h);
            break;
        default:
            break;
    }
}

void ui_test_patterns_next(void)
{
    pattern_mode_t next = (s_current_mode + 1) % PATTERN_MODE_MAX;
    ui_test_patterns_set_mode(next);
}

bool ui_test_patterns_cmd(const char *cmd_name)
{
    if (!cmd_name) return false;

    if (strcasecmp(cmd_name, "NEXT") == 0) {
        ui_test_patterns_next();
        return true;
    } else if (strcasecmp(cmd_name, "TIMER") == 0 || strcasecmp(cmd_name, "STOPWATCH") == 0) {
        ui_test_patterns_set_mode(PATTERN_MODE_STOPWATCH);
        return true;
    } else if (strcasecmp(cmd_name, "FOCUS") == 0 || strcasecmp(cmd_name, "SIEMENS") == 0) {
        ui_test_patterns_set_mode(PATTERN_MODE_FOCUS);
        return true;
    } else if (strcasecmp(cmd_name, "COLOR") == 0 || strcasecmp(cmd_name, "BARS") == 0) {
        ui_test_patterns_set_mode(PATTERN_MODE_COLOR_BARS);
        return true;
    } else if (strcasecmp(cmd_name, "GRID") == 0) {
        ui_test_patterns_set_mode(PATTERN_MODE_GRID);
        return true;
    } else if (strcasecmp(cmd_name, "GRAY") == 0 || strcasecmp(cmd_name, "WDR") == 0) {
        ui_test_patterns_set_mode(PATTERN_MODE_GRAYSCALE);
        return true;
    }

    int id = -1;
    if (sscanf(cmd_name, "%d", &id) == 1 && id >= 0 && id < PATTERN_MODE_MAX) {
        ui_test_patterns_set_mode((pattern_mode_t)id);
        return true;
    }

    return false;
}
