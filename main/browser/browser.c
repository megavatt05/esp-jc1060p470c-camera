/*
 * SPDX-License-Identifier: CC0-1.0
 *
 * CamBrowser: touch-driven text browser on the JD9165 1024x600 LCD.
 *
 * State machine (single FreeRTOS task, core 1; the camera stream task is
 * stopped before this task owns the screen, see docs/ETHERNET_BROWSER.md):
 *
 *   SPLASH ──(net ready / tap)──▶ HOME (query + keyboard)
 *   HOME ──GO──▶ LOADING ──▶ RESULTS ──tap──▶ PAGELOAD ──▶ PAGE
 *   RESULTS/PAGE ──nav bar──▶ HOME / back / scroll
 *
 * All drawing goes through camos/ui.h into the DPI panel frame buffers
 * (app_lcd_get_fb / app_lcd_flush, double-buffered full-screen flush).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "app_lcd.h"
#include "net/app_net.h"
#include "net/speed_test.h"
#include "net/wifi_mgr.h"
#include "camos/ui.h"
#include "camos/ui_icons.h"
#include "camos/touch.h"
#include "browser/web_client.h"
#include "browser/html_text.h"
#include "browser/films.h"
#include "browser/touch_calib.h"
#include "browser/browser.h"
#if CONFIG_EB_MEDIA_ENABLE
#include "media/media_player.h"
#include "media/pstats.h"
#include "media/sdcard.h"
#include <dirent.h>
#endif

static const char *TAG = "browser";

/* --- Layout -------------------------------------------------------------- */

#define LCD_W           EXAMPLE_LCD_H_RES
#define LCD_H           EXAMPLE_LCD_V_RES

#define BAR_H           32      /* top status bar          */
#define URL_Y           (BAR_H) /* query row               */
#define URL_H           56
#define GO_W            110

#define CONTENT_Y0      (BAR_H + URL_H + 4)
#define CONTENT_Y1      368     /* keyboard starts below   */
#define CONTENT_Y1_FULL (LCD_H - 64) /* without keyboard   */

#define ROW_H           46      /* result/film card pitch   */
#define LINE_H          20      /* page text line          */
#define PAGE_COLS       ((LCD_W - 24) / (UI_FONT_W * UI_SCALE_TEXT))

/* Keyboard geometry: 5 rows of 80x44 keys, gaps 2 px vertical /
 * 4 px horizontal, centered.  Key labels draw at KB_KEY_SCALE so a glyph
 * (32x32 px) fills most of the button face.  Worst row is 12 keys:
 * 12 * 80 + 11 * 4 = 1004 <= LCD_W (1024).  Five rows of 44 + 4 gaps of 2
 * = 228 px, so the block spans CONTENT_Y1 + 2 .. 598 on a 600 px panel. */
#define KEY_W           80
#define KEY_H           44
#define KEY_GAP         4
#define KEY_VGAP        2
#define KB_KEY_SCALE    4

/* Button ids. All stay below BTN_CHAR (0x20): ids >= BTN_CHAR are
 * literal keyboard codepoints. */
enum {
    BTN_NONE = 0,
    BTN_GO = 1,
    BTN_ENGINE,
    BTN_SPC,
    BTN_DEL,
    BTN_HOME,
    BTN_BACK,
    BTN_UP,
    BTN_DOWN,
    BTN_KBD,
    BTN_URLROW,
    BTN_RADIO,
    BTN_VIDEO,
    BTN_FILMS,              /* open the archive.org films catalog  */
    BTN_MSTOP,
    BTN_MPAUSE,
    BTN_VOLUP,
    BTN_VOLDN,
    BTN_MBACK,
    BTN_MURL,
    BTN_SD,                 /* open the SD file list        */
    BTN_FREFR,              /* rescan the card              */
    BTN_FUP,
    BTN_FDOWN,
    BTN_FILEROW,            /* row = (y - SD_ROWS_Y0)/SD_ROW_STRIDE */
    BTN_STATION,            /* + 0..RADIO_STATIONS_N-1 */
    BTN_VIDPRESET,          /* + 0..VIDEO_PRESETS_N-1 */
    BTN_WIFI,               /* экран Wi‑Fi менеджера */
    BTN_WIFI_SCAN,
    BTN_WIFI_PORTAL,
    BTN_WIFI_TEST,
    BTN_WIFI_CONN,          /* подключить сохранённую */
    BTN_WIFI_ROW,           /* + index скан-сети */
    BTN_WIFI_DIAG_CLOSE,    /* закрыть терминал Wi-Fi */
    BTN_WIFI_DIAG_RECONN,   /* переподключить Wi-Fi */
    BTN_WIFI_DIAG_SCAN,     /* перейти в скан сетей */
    BTN_WIFI_DIAG_TEST,     /* тест сети в терминале */
    BTN_TOUCH_TEST,         /* экран тестирования и калибровки тача */
    BTN_CASE,               /* переключение регистра: ЗАГЛ / стр */
    BTN_CHAR = 0x1000,      /* маркер, что нажата буква/символ */
};

typedef enum {
    ST_SPLASH,
    ST_HOME,
    ST_LOADING,
    ST_RESULTS,
    ST_PAGELOAD,
    ST_PAGE,
    ST_BUSY,                /* live busy card: animator redraws it while
                             * the browser task is blocked on the network */
    ST_TOUCH_TEST,          /* тестовый экран тача и калибровки */
    ST_WIFI_DIAG,           /* терминал подключения Wi-Fi и диагностики */
#if CONFIG_EB_MEDIA_ENABLE
    ST_RADIO,               /* radio screen (stations + controls)      */
    ST_VIDEO,               /* video screen (presets + custom URL)     */
    ST_URLIN,               /* keyboard: type a media URL              */
    ST_VIDEOP,              /* video playing: the render owns the panel */
    ST_FILES,               /* media files on the SD card              */
    ST_FILMS,               /* keyboard: search the films catalog      */
    ST_FILMSR,              /* films search results                    */
    ST_WIFI,                /* менеджер Wi‑Fi: скан / портал / тест     */
    ST_WIFI_PASS,           /* ввод пароля сети на экранной клавиатуре */
#endif
} browser_state_t;

/* --- Static data ---------------------------------------------------------- */

static struct {
    int fb_idx;
    browser_state_t state;
    browser_state_t wifi_diag_back; /* откуда пришли в терминал Wi-Fi */

    char query[96];
    bool engine_google;
    bool kb_ru;             /* on-screen keyboard layout: false=EN true=RU */
    bool kb_upper;          /* регистр: false=строчные, true=ЗАГЛ */

    web_result_t results[WEB_RESULTS_MAX];
    int result_count;
    bool serp_error;

    char *page_text;        /* PSRAM plain text of the opened page */
    int  page_scroll;

    /* wrapped page lines (pointers into page_text) */
    struct { const char *s; int len; } *lines;
    int line_count;

    int splash_attempts;
#if CONFIG_EB_MEDIA_ENABLE
    bool urlin_video;       /* ST_URLIN target: false=radio true=video  */
#endif
} br;

/* --- Wrapping ------------------------------------------------------------- */

static void page_wrap(void)
{
    if (br.lines == NULL) {
        br.lines = heap_caps_malloc(4096 * sizeof(*br.lines), MALLOC_CAP_SPIRAM);
        if (br.lines == NULL) {
            return;
        }
    }
    br.line_count = 0;

    const char *p = br.page_text;
    if (p == NULL) {
        return;
    }

    while (*p != '\0' && br.line_count < 4090) {
        const char *line_start = p;
        const char *last_space = NULL;
        int col = 0;

        while (*p != '\0' && *p != '\n' && col < PAGE_COLS) {
            if (*p == ' ') {
                last_space = p;
            }
            if ((((unsigned char)*p) & 0xC0) != 0x80) {
                col++;          /* count codepoints, not UTF-8 bytes */
            }
            p++;
        }
        /* never split a UTF-8 sequence at the wrap boundary */
        while (p > line_start && (((unsigned char)*p) & 0xC0) == 0x80) {
            p--;
        }

        if (*p == '\n') {
            br.lines[br.line_count].s = line_start;
            br.lines[br.line_count].len = (int)(p - line_start);
            br.line_count++;
            p++;
            continue;
        }
        if (*p != '\0' && last_space != NULL && last_space > line_start) {
            p = last_space;             /* wrap at the space */
            br.lines[br.line_count].s = line_start;
            br.lines[br.line_count].len = (int)(p - line_start);
            br.line_count++;
            p++;                        /* skip the space */
        } else {
            br.lines[br.line_count].s = line_start;
            br.lines[br.line_count].len = (int)(p - line_start);
            br.line_count++;
        }
    }
}

static void page_free(void)
{
    if (br.page_text != NULL) {
        free(br.page_text);
        br.page_text = NULL;
    }
    br.line_count = 0;
    br.page_scroll = 0;
}

/* --- Drawing helpers (theme v2 "friendly dark") ---------------------------- */

/* Rounded search field + GO button; shared by HOME / URLIN / FILMS */
static void draw_url_row(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, URL_Y, LCD_W, URL_Y + URL_H, UI_COLOR_BG);

    /* field */
    ui_rrect(fb, LCD_W, LCD_H, 8, URL_Y + 4, LCD_W - GO_W - 16, URL_Y + URL_H - 2,
             UI_R_PILL, UI_COLOR_PANEL);
    ui_rrect_frame(fb, LCD_W, LCD_H, 8, URL_Y + 4, LCD_W - GO_W - 16, URL_Y + URL_H - 2,
                   UI_R_PILL, br.query[0] == '\0' ? UI_COLOR_BORDER : UI_COLOR_ACCENT);

    ui_icon(&ui_icon_search, fb, LCD_W, LCD_H, 20, URL_Y + (URL_H - 16) / 2, 1,
            UI_COLOR_DIM);

    char fitted[96];
    ui_text_fit(fitted, sizeof(fitted), br.query, LCD_W - GO_W - 60, UI_SCALE_URL);
    ui_text(fb, LCD_W, LCD_H, 44, URL_Y + (URL_H - UI_FONT_H * UI_SCALE_URL) / 2,
            fitted, UI_SCALE_URL, UI_COLOR_FG, UI_COLOR_PANEL);

    /* caret */
    int cx = 44 + ui_text_width(fitted, UI_SCALE_URL) + 2;
    if ((esp_timer_get_time() / 500000) & 1) {
        ui_fill_rect(fb, LCD_W, LCD_H, cx, URL_Y + 16, cx + UI_SCALE_URL,
                     URL_Y + URL_H - 16, UI_COLOR_ACCENT);
    }

    /* GO: accent pill, dark text */
    ui_rrect(fb, LCD_W, LCD_H, LCD_W - GO_W - 8, URL_Y + 6, LCD_W - 8, URL_Y + URL_H - 4,
             UI_R_KEY, UI_COLOR_ACCENT);
    char gol[] = "GO";
    int gw = ui_text_width(gol, 3);
    ui_text(fb, LCD_W, LCD_H, LCD_W - GO_W - 8 + (GO_W - gw) / 2,
            URL_Y + (URL_H - UI_FONT_H * 3) / 2 + 1, gol, 3, UI_COLOR_BG, UI_COLOR_ACCENT);
}

static void draw_status_bar(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, BAR_H, UI_COLOR_BAR);
    ui_fill_rect(fb, LCD_W, LCD_H, 0, BAR_H - 1, LCD_W, BAR_H, UI_COLOR_BORDER);

    /* левая плашка-кнопка: иконка Wi-Fi + SSID / IP */
    wifi_mgr_state_t wst = wifi_mgr_state();
    bool ready = wifi_mgr_ready();
    const char *ssid = wifi_mgr_saved_ssid();
    const char *ip = wifi_mgr_ip_str();

    char label[64];
    uint16_t icon_col = UI_COLOR_OK;
    const ui_icon_t *icon_wifi = &ui_icon_wifi;

    if (ready) {
        icon_wifi = &ui_icon_wifi;
        icon_col = UI_COLOR_OK;
        if (ssid[0]) {
            snprintf(label, sizeof(label), "%s  %s", ssid, ip);
        } else {
            snprintf(label, sizeof(label), "%s", ip);
        }
    } else if (wst == WIFI_MGR_CONNECTING) {
        icon_wifi = &ui_icon_wifi;
        icon_col = UI_COLOR_WARN;
        if (ssid[0]) {
            snprintf(label, sizeof(label), "Подкл. к %s...", ssid);
        } else {
            snprintf(label, sizeof(label), "Подключение Wi-Fi...");
        }
    } else if (wst == WIFI_MGR_SCANNING) {
        icon_wifi = &ui_icon_wifi;
        icon_col = UI_COLOR_ACCENT;
        snprintf(label, sizeof(label), "Скан сетей Wi-Fi...");
    } else if (wst == WIFI_MGR_AP_MODE) {
        icon_wifi = &ui_icon_wifi;
        icon_col = UI_COLOR_ACCENT2;
        snprintf(label, sizeof(label), "Портал: 192.168.4.1");
    } else {
        icon_wifi = &ui_icon_wifi_off;
        icon_col = UI_COLOR_ERR;
        snprintf(label, sizeof(label), "Wi-Fi откл (нажмите)");
    }

    /* Рамка-кнопка под иконку и надпись */
    int txt_w = ui_text_width(label, 1);
    int badge_w = 28 + txt_w + 12;
    if (badge_w > 360) {
        badge_w = 360;
    }
    ui_rrect(fb, LCD_W, LCD_H, 4, 3, badge_w, BAR_H - 3, 6, UI_COLOR_KEY);
    ui_rrect_frame(fb, LCD_W, LCD_H, 4, 3, badge_w, BAR_H - 3, 6, UI_COLOR_BORDER);

    /* Иконка Wi-Fi */
    ui_icon(icon_wifi, fb, LCD_W, LCD_H, 8, (BAR_H - 16) / 2, 1, icon_col);

    /* Детекция переключения состояния значка Wi-Fi для автотестов и логов */
    static wifi_mgr_state_t s_last_logged_wst = (wifi_mgr_state_t)-1;
    static bool s_last_logged_ready = false;
    if (wst != s_last_logged_wst || ready != s_last_logged_ready) {
        s_last_logged_wst = wst;
        s_last_logged_ready = ready;
        const char *icon_name = (icon_wifi == &ui_icon_wifi) ? "WIFI_ON" : "WIFI_OFF";
        ESP_LOGI("UI_WIFI_ICON", "📶 Значок Wi-Fi изменён: icon=%s col=0x%04X state=%s text=\"%s\"",
                 icon_name, icon_col, wifi_mgr_status_str(), label);
    }

    /* Текст статуса/сети */
    char fitted[64];
    ui_text_fit(fitted, sizeof(fitted), label, badge_w - 32, 1);
    ui_text(fb, LCD_W, LCD_H, 28, (BAR_H - UI_FONT_H) / 2,
            fitted, 1, UI_COLOR_FG, UI_COLOR_KEY);

    /* right: engine pill + radio-on note */
    int x = LCD_W - 8;
#if CONFIG_EB_MEDIA_ENABLE
    if (media_get_state() == MEDIA_STATE_PLAYING && !media_video_active()) {
        ui_icon(&ui_icon_music, fb, LCD_W, LCD_H, x - 14, (BAR_H - 16) / 2, 1, UI_COLOR_OK);
        x -= 20;
    }
#endif
    const char *eng = br.engine_google ? "GGL" : "DDG";
    int pw = ui_text_width(eng, 1) + 16;
    ui_rrect(fb, LCD_W, LCD_H, x - pw, 6, x, BAR_H - 6, (BAR_H - 12) / 2, UI_COLOR_KEY);
    ui_text(fb, LCD_W, LCD_H, x - pw + 8, (BAR_H - UI_FONT_H) / 2,
            eng, 1, UI_COLOR_ACCENT, UI_COLOR_KEY);
}

/* nav bar: rounded buttons, icons on HOME/BACK, chevrons for UP/DOWN */
static void draw_nav_bar(uint16_t *fb)
{
    static const ui_button_t nav[] = {
        { 8,   LCD_H - 60, 180, 56, "МЕНЮ",  BTN_HOME, 0 },
        { 196, LCD_H - 60, 180, 56, "НАЗАД", BTN_BACK, 0 },
        { 830, LCD_H - 60, 88,  56, "^",     BTN_UP,   0 },
        { 926, LCD_H - 60, 88,  56, "|",     BTN_DOWN, 0 },
    };

    ui_fill_rect(fb, LCD_W, LCD_H, 0, LCD_H - 64, LCD_W, LCD_H, UI_COLOR_BG);
    for (size_t i = 0; i < 2; i++) {
        const ui_button_t *b = &nav[i];
        ui_rrect(fb, LCD_W, LCD_H, b->x, b->y, b->x + b->w, b->y + b->h,
                 UI_R_KEY, UI_COLOR_KEY);
        ui_rrect_frame(fb, LCD_W, LCD_H, b->x, b->y, b->x + b->w, b->y + b->h,
                       UI_R_KEY, UI_COLOR_BORDER);
        const ui_icon_t *ic = (b->id == BTN_HOME) ? &ui_icon_home : &ui_icon_back;
        ui_icon(ic, fb, LCD_W, LCD_H, b->x + 18, b->y + (b->h - 16) / 2, 1, UI_COLOR_ACCENT);
        int tw = ui_text_width(b->label, 2);
        ui_text(fb, LCD_W, LCD_H, b->x + (b->w - tw - 24) / 2 + 24,
                b->y + (b->h - UI_FONT_H * 2) / 2, b->label, 2, UI_COLOR_FG, UI_COLOR_KEY);
    }
    for (size_t i = 2; i < 4; i++) {
        const ui_button_t *b = &nav[i];
        ui_rrect(fb, LCD_W, LCD_H, b->x, b->y, b->x + b->w, b->y + b->h,
                 UI_R_KEY, UI_COLOR_KEY);
        ui_rrect_frame(fb, LCD_W, LCD_H, b->x, b->y, b->x + b->w, b->y + b->h,
                       UI_R_KEY, UI_COLOR_BORDER);
        ui_icon(b->id == BTN_UP ? &ui_icon_up : &ui_icon_down,
                fb, LCD_W, LCD_H, b->x + (b->w - 16) / 2, b->y + (b->h - 16) / 2, 1,
                UI_COLOR_FG);
    }
}

static bool nav_hit(int x, int y, int *out_id)
{
    static const ui_button_t nav[] = {
        { 8,   LCD_H - 60, 180, 56, "МЕНЮ",  BTN_HOME, 0 },
        { 196, LCD_H - 60, 180, 56, "НАЗАД", BTN_BACK, 0 },
        { 830, LCD_H - 60, 88,  56, "^",     BTN_UP,   0 },
        { 926, LCD_H - 60, 88,  56, "|",     BTN_DOWN, 0 },
    };
    for (size_t i = 0; i < sizeof(nav) / sizeof(nav[0]); i++) {
        if (ui_button_hit(&nav[i], x, y)) {
            *out_id = nav[i].id;
            return true;
        }
    }
    return false;
}

/* Tap on the GO button in the URL row (top of the screen) */
static bool urlrow_go_hit(int x, int y)
{
    return (y >= URL_Y && y < URL_Y + URL_H &&
            x >= LCD_W - GO_W - 8 && x < LCD_W - 8);
}

/* Slim scrollbar on the right edge of a list zone */
static void draw_scrollbar(uint16_t *fb, int y0, int y1, int total,
                           int first, int visible)
{
    if (total <= visible) {
        return;
    }
    ui_fill_rect(fb, LCD_W, LCD_H, LCD_W - 5, y0, LCD_W - 3, y1, UI_COLOR_KEY);
    int th = (y1 - y0) * visible / total;
    if (th < 24) {
        th = 24;
    }
    int ty = y0 + (y1 - y0 - th) * first / (total - visible);
    ui_rrect(fb, LCD_W, LCD_H, LCD_W - 5, ty, LCD_W - 3, ty + th, 1, UI_COLOR_ACCENT);
}

/* --- Keyboard ---------------------------------------------------------------- */

/* Keys are Unicode codepoints: ASCII prints as-is, Cyrillic (U+0410..)
 * is UTF-8 encoded by query_append_cp() and rendered with the Cyrillic
 * glyph table in app_overlay_font8x8_cyr. 0 terminates a row. */

#define KB_ROWS_NUM 4
#define KB_ROW_MAX  13

static const uint32_t KB_EN_LOWER[KB_ROWS_NUM][KB_ROW_MAX] = {
    { '1','2','3','4','5','6','7','8','9','0','-','_', 0 },
    { 'q','w','e','r','t','y','u','i','o','p', 0 },
    { 'a','s','d','f','g','h','j','k','l','.', 0 },
    { 'z','x','c','v','b','n','m','_','/', 0 },
};

static const uint32_t KB_EN_UPPER[KB_ROWS_NUM][KB_ROW_MAX] = {
    { '1','2','3','4','5','6','7','8','9','0','-','_', 0 },
    { 'Q','W','E','R','T','Y','U','I','O','P', 0 },
    { 'A','S','D','F','G','H','J','K','L',':', 0 },
    { 'Z','X','C','V','B','N','M','?','/', 0 },
};

/* ЙЦУКЕН: строчные буквы */
static const uint32_t KB_RU_LOWER[KB_ROWS_NUM][KB_ROW_MAX] = {
    { 0x0451,'1','2','3','4','5','6','7','8','9','0','-', 0 },
    { 0x0439,0x0446,0x0443,0x043A,0x0435,0x043D,0x0433,0x0448,0x0449,0x0437,0x0445,0x044A, 0 },
    { 0x0444,0x044B,0x0432,0x0430,0x043F,0x0440,0x043E,0x043B,0x0434,0x0436,0x044D, 0 },
    { 0x044F,0x0447,0x0441,0x043C,0x0438,0x0442,0x044C,0x0431,0x044E,'.', 0 },
};

/* ЙЦУКЕН: ЗАГЛАВНЫЕ буквы (Ё, Й, Ц, У, К, Е, Н ...) */
static const uint32_t KB_RU_UPPER[KB_ROWS_NUM][KB_ROW_MAX] = {
    { 0x0401,'1','2','3','4','5','6','7','8','9','0','-', 0 },
    { 0x0419,0x0426,0x0423,0x041A,0x0415,0x041D,0x0413,0x0428,0x0429,0x0417,0x0425,0x042A, 0 },
    { 0x0424,0x042B,0x0412,0x0410,0x041F,0x0420,0x041E,0x041B,0x0414,0x0416,0x042D, 0 },
    { 0x042F,0x0427,0x0421,0x041C,0x0418,0x0422,0x042C,0x0411,0x042E,',', 0 },
};

static const uint32_t (*kb_rows(void))[KB_ROW_MAX]
{
    if (br.kb_ru) {
        return br.kb_upper ? KB_RU_UPPER : KB_RU_LOWER;
    } else {
        return br.kb_upper ? KB_EN_UPPER : KB_EN_LOWER;
    }
}

/* UTF-8 encode one key codepoint into buf (4 bytes max); returns byte length */
static int kb_key_label(uint32_t cp, char *buf)
{
    if (cp < 0x80) {
        buf[0] = (char)cp;
        buf[1] = '\0';
        return 1;
    }
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    buf[2] = '\0';
    return 2;
}

/* Нижний ряд: SPC / DEL / ЗАГЛ(Shift) / РУ(EN) / НАЗАД(GGL) / GO */
static const struct { int w; const char *label; int id; } kb_bottom[] = {
    { 180, "SPC",  BTN_SPC },
    { 130, "DEL",  BTN_DEL },
    { 130, NULL,   BTN_CASE },        /* ЗАГЛ | стр                 */
    { 130, NULL,   BTN_KBD },         /* EN | РУ                    */
    { 150, NULL,   BTN_ENGINE },      /* GGL | DDG | НАЗАД          */
    { 150, "GO",   BTN_GO },
};
#define KB_BOTTOM_N (sizeof(kb_bottom) / sizeof(kb_bottom[0]))

static const char *kb_bottom_label(size_t i)
{
    if (kb_bottom[i].label != NULL) {
        return kb_bottom[i].label;
    }
    if (kb_bottom[i].id == BTN_CASE) {
        return br.kb_upper ? "ЗАГЛ" : "стр";
    }
    if (kb_bottom[i].id == BTN_KBD) {
        return br.kb_ru ? "РУ" : "EN";
    }
#if CONFIG_EB_MEDIA_ENABLE
    /* on the media URL / films search screens the engine slot doubles as CANCEL */
    if (kb_bottom[i].id == BTN_ENGINE &&
        (br.state == ST_URLIN || br.state == ST_FILMS || br.state == ST_WIFI_PASS)) {
        return "НАЗАД";
    }
#endif
    return br.engine_google ? "GGL" : "DDG";
}

static int kb_bottom_x0(void)
{
    int total = 0;
    for (size_t i = 0; i < KB_BOTTOM_N; i++) {
        total += kb_bottom[i].w;
    }
    total += (int)(KB_BOTTOM_N - 1) * KEY_GAP;
    return (LCD_W - total) / 2;
}

static void kb_draw_key(uint16_t *fb, int x, int y, int w, const char *lab, int id)
{
    int sc = KB_KEY_SCALE;
    if (id == BTN_GO) {
        ui_rrect(fb, LCD_W, LCD_H, x, y, x + w, y + KEY_H, UI_R_KEY, UI_COLOR_ACCENT);
        int tw = ui_text_width(lab, sc);
        ui_text(fb, LCD_W, LCD_H, x + (w - tw) / 2, y + (KEY_H - UI_FONT_H * sc) / 2,
                lab, sc, UI_COLOR_BG, UI_COLOR_ACCENT);
        return;
    }
    uint16_t txt = UI_COLOR_FG;
    uint16_t border = UI_COLOR_BORDER;
    if (id == BTN_DEL) {
        txt = UI_COLOR_ERR;
    } else if (id == BTN_CASE && br.kb_upper) {
        txt = UI_COLOR_OK;
        border = UI_COLOR_OK;
    } else if (id == BTN_CASE || id == BTN_KBD || id == BTN_ENGINE) {
        txt = UI_COLOR_ACCENT;
        border = UI_COLOR_ACCENT2;
    }
    ui_rrect(fb, LCD_W, LCD_H, x, y, x + w, y + KEY_H, UI_R_KEY, UI_COLOR_KEY);
    ui_rrect_frame(fb, LCD_W, LCD_H, x, y, x + w, y + KEY_H, UI_R_KEY, border);
    int tw = ui_text_width(lab, sc);
    ui_text(fb, LCD_W, LCD_H, x + (w - tw) / 2, y + (KEY_H - UI_FONT_H * sc) / 2,
            lab, sc, txt, UI_COLOR_KEY);
}

static void draw_keyboard(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, CONTENT_Y1, LCD_W, LCD_H, UI_COLOR_BG);

    const uint32_t (*rows)[KB_ROW_MAX] = kb_rows();
    int y = CONTENT_Y1 + KEY_VGAP;
    for (int r = 0; r < KB_ROWS_NUM; r++) {
        int n = 0;
        while (n < KB_ROW_MAX && rows[r][n] != 0) {
            n++;
        }
        int row_w = n * KEY_W + (n - 1) * KEY_GAP;
        int x = (LCD_W - row_w) / 2;

        for (int k = 0; k < n; k++, x += KEY_W + KEY_GAP) {
            char lab[4];
            kb_key_label(rows[r][k], lab);
            ui_rrect(fb, LCD_W, LCD_H, x, y, x + KEY_W, y + KEY_H, UI_R_KEY, UI_COLOR_KEY);
            ui_rrect_frame(fb, LCD_W, LCD_H, x, y, x + KEY_W, y + KEY_H, UI_R_KEY,
                           UI_COLOR_BORDER);
            int tw = ui_text_width(lab, KB_KEY_SCALE);
            ui_text(fb, LCD_W, LCD_H, x + (KEY_W - tw) / 2,
                    y + (KEY_H - UI_FONT_H * KB_KEY_SCALE) / 2,
                    lab, KB_KEY_SCALE, UI_COLOR_FG, UI_COLOR_KEY);
        }
        y += KEY_H + KEY_VGAP;
    }

    int x = kb_bottom_x0();
    for (size_t i = 0; i < KB_BOTTOM_N; i++) {
        kb_draw_key(fb, x, y, kb_bottom[i].w, kb_bottom_label(i), kb_bottom[i].id);
        x += kb_bottom[i].w + KEY_GAP;
    }
}

/* Returns BTN_* id. If returning BTN_CHAR, sets *out_cp to the key codepoint. */
static int keyboard_hit(int x, int y, uint32_t *out_cp)
{
    if (out_cp != NULL) {
        *out_cp = 0;
    }
    if (y < CONTENT_Y1) {
        return BTN_NONE;
    }

    const uint32_t (*rows)[KB_ROW_MAX] = kb_rows();
    int ky = CONTENT_Y1 + KEY_VGAP;
    for (int r = 0; r < KB_ROWS_NUM; r++) {
        int n = 0;
        while (n < KB_ROW_MAX && rows[r][n] != 0) {
            n++;
        }
        int row_w = n * KEY_W + (n - 1) * KEY_GAP;
        int kx = (LCD_W - row_w) / 2;

        if (y >= ky && y < ky + KEY_H) {
            for (int k = 0; k < n; k++, kx += KEY_W + KEY_GAP) {
                if (x >= kx && x < kx + KEY_W) {
                    if (out_cp != NULL) {
                        *out_cp = rows[r][k];
                    }
                    return BTN_CHAR;
                }
            }
            return BTN_NONE;
        }
        ky += KEY_H + KEY_VGAP;
    }

    if (y >= ky && y < ky + KEY_H) {
        int x0 = kb_bottom_x0();
        for (size_t i = 0; i < KB_BOTTOM_N; i++) {
            if (x >= x0 && x < x0 + kb_bottom[i].w) {
                return kb_bottom[i].id;
            }
            x0 += kb_bottom[i].w + KEY_GAP;
        }
    }
    return BTN_NONE;
}

/* --- Screens ------------------------------------------------------------------ */

static void draw_screen(void);

/* live busy card: spinner + stage + ticking mm:ss, see implementation.
 * Used by both the web paths and the media paths - declared outside the
 * media guard on purpose. */
static struct {
    bool     on;          /* animator running                            */
    bool     quit;        /* animator stop request                       */
    bool     dead;        /* animator has exited                         */
    char     title[48];   /* what was tapped / searched                  */
    char     stage[48];   /* current phase ("проверяю файл")             */
    char     extra[64];   /* bytes / rate line ("1.2 из 2.9 МБ, 19 кБ/с")
                           * 64: worst-case snprintf fits without truncation */
    int64_t  t0;          /* busy window start, us                       */
    int64_t  stage_t0;    /* current stage start, us (for the rate)      */
    uint32_t acc_bytes;   /* bytes accumulated in the current stage      */
    uint32_t prev_done;   /* last FILMS_PROG_FILE done value             */
} s_busy;

static void busy_open(const char *title);
static void busy_stage(const char *txt);
static void busy_bytes(uint32_t done, uint32_t total);
static void busy_close(void);

#if CONFIG_EB_MEDIA_ENABLE

/* --- Media: internet radio + video (see media/media_player.h) --------------- */

static void draw_loading(uint16_t *fb, const char *what);

static const struct { const char *name; const char *url; } radio_stations[] = {
    { "Радио Маяк",     "http://icecast.vgtrk.cdnvideo.ru/mayakfm_mp3_128kbps" },
    { "Вести FM",       "http://icecast.vgtrk.cdnvideo.ru/vestifm_mp3_128kbps" },
    { "Европа Плюс",    "http://ep128.hostingradio.ru:8030/ep128" },
    { "Дорожное Радио", "http://dorognoe.hostingradio.ru:8000/dorognoe" },
    { "Культура FM",    "http://icecast.vgtrk.cdnvideo.ru/kulturafm_mp3_128kbps" },
    { "Радио России",   "http://icecast.vgtrk.cdnvideo.ru/radiorus_mp3_128kbps" },
};
#define RADIO_STATIONS_N (sizeof(radio_stations) / sizeof(radio_stations[0]))

/* Video presets. The SW H264 decoder (esp_h264 / tinyh264) plays only
 * Constrained Baseline streams: Main/High profiles are rejected at SPS
 * validation ("H264_DEC: profile_idc is error"). The clips in media/ are
 * Big Buck Bunny re-encoded to Constrained Baseline and served via
 * raw.githubusercontent.com (supports Range requests).
 * Decode speed reality (SW decode, P4 rev<3 @360MHz): 360p@30 lands at
 * ~5 fps (slideshow); 180p@20 is roughly realtime. Keep presets small.
 * See docs/ETHERNET_BROWSER.md, section "Медиа". */
static const struct { const char *name; const char *url; } video_presets[] = {
    { "BBB 180p (плавно)", "https://raw.githubusercontent.com/megavatt05/JC1060P470C-camera-photo-app/feature/ethernet-browser/media/bbb_180p_cb.mp4" },
    { "BBB 360p (четко)",  "https://raw.githubusercontent.com/megavatt05/JC1060P470C-camera-photo-app/feature/ethernet-browser/media/bbb_360p_cb.mp4" },
};
#define VIDEO_PRESETS_N (sizeof(video_presets) / sizeof(video_presets[0]))

/* HOME: three app launcher cards in the content zone */
static char wifi_pending_ssid[33]; /* SSID выбранной сети, ждём пароль */

static const ui_button_t home_apps[] = {
    { 20,  108, 320, 142, "РАДИО",     BTN_RADIO, 3 },
    { 352, 108, 320, 142, "ВИДЕО",     BTN_VIDEO, 3 },
    { 684, 108, 320, 142, "ФИЛЬМЫ",    BTN_FILMS, 3 },
#if CONFIG_EB_WIFI_ENABLE
    { 20,  255, 484, 72,  "Wi-Fi",     BTN_WIFI,  2 },
    { 520, 255, 484, 72,  "ТАЧ ТЕСТ",  BTN_TOUCH_TEST, 2 },
#else
    { 20,  255, 984, 72,  "ТАЧ ТЕСТ",  BTN_TOUCH_TEST, 2 },
#endif
};
#define HOME_APPS_N (sizeof(home_apps) / sizeof(home_apps[0]))

static bool home_apps_hit(int x, int y, int *out_id)
{
    for (size_t i = 0; i < HOME_APPS_N; i++) {
        if (ui_button_hit(&home_apps[i], x, y)) {
            *out_id = home_apps[i].id;
            return true;
        }
    }
    return false;
}

static const ui_icon_t *home_app_icon(size_t i)
{
    /* иконки карточек HOME: радио / видео / фильмы / Wi‑Fi / тач тест */
    static const ui_icon_t *const icons[] = {
        &ui_icon_music, &ui_icon_clap, &ui_icon_film,
#if CONFIG_EB_WIFI_ENABLE
        &ui_icon_globe,
#endif
        &ui_icon_refresh,
    };
    if (i >= sizeof(icons) / sizeof(icons[0])) {
        return &ui_icon_search;
    }
    return icons[i];
}

static const uint16_t home_app_color[] = {
    UI_COLOR_OK, UI_COLOR_WARN, UI_COLOR_ACCENT,
#if CONFIG_EB_WIFI_ENABLE
    UI_COLOR_ACCENT2,
#endif
    UI_COLOR_ACCENT,
};

/* Radio: station tile geometry, shared by draw and hit-test */

static void radio_st_btn(int i, ui_button_t *b)
{
    b->x = (i & 1) ? 516 : 12;
    b->y = 88 + (i >> 1) * 80;
    b->w = 496;
    b->h = 72;
    b->label = radio_stations[i].name;
    b->id = (int)(BTN_STATION + i);
    b->scale = 3;
}

static bool media_url_norm(const char *in, char *out, size_t outsz)
{
    while (*in == ' ') {
        in++;
    }
    if (*in == '\0') {
        return false;
    }
    if (strncasecmp(in, "http://", 7) != 0 && strncasecmp(in, "https://", 8) != 0) {
        snprintf(out, outsz, "http://%s", in);
    } else {
        snprintf(out, outsz, "%s", in);
    }
    return true;
}

/* ============================================================================
 * Асинхронный воркер радио и телеметрия задержек интерфейса (HCI / ISO 9241-110)
 * ============================================================================ */
static int s_radio_active_idx = -1;        /* Выбранная станция (Optimistic UI) */
static bool s_radio_is_connecting = false; /* Флаг ожидания подключения */
static int64_t s_radio_conn_t0 = 0;        /* Время старта запроса (таймаут Нильсена 10 с) */
static TaskHandle_t s_radio_worker_hd = NULL;
static QueueHandle_t s_radio_queue = NULL;

typedef struct {
    int station_idx;
    char url[256];
} radio_worker_msg_t;

static void radio_worker_task(void *arg)
{
    radio_worker_msg_t msg;
    while (1) {
        if (xQueueReceive(s_radio_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("radio_worker", "Старт станции [%d]: %s", msg.station_idx, msg.url);
            int64_t t0 = esp_timer_get_time();
            esp_err_t err = media_radio_start(msg.url);
            int64_t dur_ms = (esp_timer_get_time() - t0) / 1000;
            if (err == ESP_OK) {
                ESP_LOGI("radio_worker", "📻 Станция подключена за %lld мс", dur_ms);
            } else {
                ESP_LOGE("radio_worker", "❌ Ошибка подключения станции (%s) за %lld мс",
                         esp_err_to_name(err), dur_ms);
            }
        }
    }
}

static void radio_worker_init_once(void)
{
    if (s_radio_queue == NULL) {
        /* FreeRTOS требование: для xQueueOverwrite длина очереди обязана быть ровно 1! */
        s_radio_queue = xQueueCreate(1, sizeof(radio_worker_msg_t));
    }
    if (s_radio_worker_hd == NULL && s_radio_queue != NULL) {
        xTaskCreatePinnedToCore(radio_worker_task, "radio_worker", 12 * 1024,
                                NULL, 2, &s_radio_worker_hd, 1);
    }
}

static void radio_request_start(int station_idx, const char *url)
{
    s_radio_active_idx = station_idx;
    s_radio_is_connecting = true;
    s_radio_conn_t0 = esp_timer_get_time();
    radio_worker_init_once();
    radio_worker_msg_t msg = {
        .station_idx = station_idx,
    };
    strlcpy(msg.url, url, sizeof(msg.url));
    xQueueOverwrite(s_radio_queue, &msg);
}

static void draw_radio(uint16_t *fb)
{
    draw_status_bar(fb);
    ui_fill_rect(fb, LCD_W, LCD_H, 0, BAR_H, LCD_W, LCD_H, UI_COLOR_BG);

    ui_text(fb, LCD_W, LCD_H, 12, 44, "РАДИО", UI_SCALE_TITLE, UI_COLOR_ACCENT, UI_COLOR_BG);

    media_state_t mst = media_get_state();
    if (mst == MEDIA_STATE_PLAYING && s_radio_is_connecting) {
        s_radio_is_connecting = false;
    }

    char st[64];
    if (s_radio_is_connecting) {
        int64_t wait_s = (esp_timer_get_time() - s_radio_conn_t0) / 1000000;
        if (wait_s >= 10) {
            snprintf(st, sizeof(st), "СЕРВЕР НЕ ОТВЕЧАЕТ (>10с)");
        } else {
            snprintf(st, sizeof(st), "ПОДКЛЮЧЕНИЕ (%lld с)...", wait_s);
        }
    } else {
        snprintf(st, sizeof(st), "%s", media_state_str());
    }
    ui_text(fb, LCD_W, LCD_H, LCD_W - ui_text_width(st, UI_SCALE_TEXT) - 12,
            52, st, UI_SCALE_TEXT,
            s_radio_is_connecting ? UI_COLOR_ACCENT : UI_COLOR_DIM, UI_COLOR_BG);

    /* volume bar under the state text */
    ui_progress(fb, LCD_W, LCD_H, LCD_W - 172, 74, 160, 12,
                media_get_volume() / 100.0f, UI_COLOR_OK);

    for (size_t i = 0; i < RADIO_STATIONS_N; i++) {
        ui_button_t b;
        radio_st_btn((int)i, &b);
        bool is_selected = (s_radio_active_idx == (int)i) ||
                           (media_get_state() != MEDIA_STATE_IDLE &&
                            strcmp(media_get_url(), radio_stations[i].url) == 0);
        bool is_conn = (s_radio_is_connecting && s_radio_active_idx == (int)i);

        uint16_t bg_color = is_selected ? UI_COLOR_BG_PRESS : UI_COLOR_PANEL;
        uint16_t border_color = is_conn ? UI_COLOR_ACCENT : (is_selected ? UI_COLOR_OK : UI_COLOR_BORDER);
        uint16_t icon_color = is_conn ? UI_COLOR_ACCENT : (is_selected ? UI_COLOR_OK : UI_COLOR_DIM);
        uint16_t text_color = is_selected ? UI_COLOR_FG : UI_COLOR_DIM;

        ui_rrect(fb, LCD_W, LCD_H, b.x, b.y, b.x + b.w, b.y + b.h, UI_R_KEY, bg_color);
        ui_rrect_frame(fb, LCD_W, LCD_H, b.x, b.y, b.x + b.w, b.y + b.h, UI_R_KEY, border_color);
        ui_icon(&ui_icon_music, fb, LCD_W, LCD_H, b.x + 20, b.y + (b.h - 16) / 2, 1, icon_color);

        if (is_selected) {
            /* Выбранная станция: двухстрочный макет (название scale=2 + статус scale=1).
             * Исключает выход за границы (Text Overflow): гарантированный запас > 100 px */
            ui_text(fb, LCD_W, LCD_H, b.x + 56, b.y + 16, b.label, 2,
                    text_color, bg_color);
            if (is_conn) {
                ui_text(fb, LCD_W, LCD_H, b.x + 56, b.y + 44, "ПОДКЛЮЧЕНИЕ...", 1,
                        UI_COLOR_ACCENT, bg_color);
            } else {
                ui_text(fb, LCD_W, LCD_H, b.x + 56, b.y + 44, "В ЭФИРЕ", 1,
                        UI_COLOR_OK, bg_color);
            }
        } else {
            /* Неактивная станция: центрированное название (scale=3) */
            int tw = ui_text_width(b.label, 3);
            ui_text(fb, LCD_W, LCD_H, b.x + (b.w - tw) / 2 + 12,
                    b.y + (b.h - UI_FONT_H * 3) / 2, b.label, 3,
                    text_color, bg_color);
        }
    }

    static const struct { int w; int id; } ctrls[] = {
        { 180, BTN_VOLDN }, { 220, BTN_MPAUSE }, { 180, BTN_VOLUP }, { 220, BTN_MSTOP },
    };
    int x = (LCD_W - (180 + 220 + 180 + 220 + 3 * 12)) / 2;
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++) {
        const char *lab = "ПАУЗА";
        if (ctrls[i].id == BTN_VOLDN)  lab = "ТИШЕ-";
        if (ctrls[i].id == BTN_VOLUP)  lab = "ГРОМЧЕ+";
        if (ctrls[i].id == BTN_MPAUSE) lab = (media_get_state() == MEDIA_STATE_PAUSED) ? "ПУСК" : "ПАУЗА";
        if (ctrls[i].id == BTN_MSTOP)  lab = "СТОП";
        ui_button_t b = { x, 336, ctrls[i].w, 64, lab, ctrls[i].id, 3 };
        ui_button(fb, LCD_W, LCD_H, &b, false);
        x += ctrls[i].w + 12;
    }

    ui_button_t sd = { 200, 420, 280, 60, "SD-КАРТА", BTN_SD, 3 };
    ui_button(fb, LCD_W, LCD_H, &sd, false);
    ui_button_t back = { 544, 420, 280, 60, "В МЕНЮ", BTN_MBACK, 3 };
    ui_button(fb, LCD_W, LCD_H, &back, false);

    if (media_get_state() != MEDIA_STATE_IDLE && media_get_url()[0] != '\0') {
        char fitted[128];
        ui_text_fit(fitted, sizeof(fitted), media_get_url(), LCD_W - 24, 1);
        ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(fitted, 1)) / 2,
                500, fitted, 1, UI_COLOR_BORDER, UI_COLOR_BG);
    }
}

static int radio_hit(int x, int y)
{
    for (size_t i = 0; i < RADIO_STATIONS_N; i++) {
        ui_button_t b;
        radio_st_btn((int)i, &b);
        if (ui_button_hit(&b, x, y)) {
            return b.id;
        }
    }
    static const struct { int w; int id; } ctrls[] = {
        { 180, BTN_VOLDN }, { 220, BTN_MPAUSE }, { 180, BTN_VOLUP }, { 220, BTN_MSTOP },
    };
    int cx = (LCD_W - (180 + 220 + 180 + 220 + 3 * 12)) / 2;
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++) {
        if (x >= cx && x < cx + ctrls[i].w && y >= 336 && y < 336 + 64) {
            return ctrls[i].id;
        }
        cx += ctrls[i].w + 12;
    }
    ui_button_t sd = { 200, 420, 280, 60, "SD-КАРТА", BTN_SD, 3 };
    if (ui_button_hit(&sd, x, y)) {
        return BTN_SD;
    }
    ui_button_t back = { 544, 420, 280, 60, "В МЕНЮ", BTN_MBACK, 3 };
    if (ui_button_hit(&back, x, y)) {
        return BTN_MBACK;
    }
    return BTN_NONE;
}

static void video_preset_btn(int i, ui_button_t *b)
{
    b->x = 12;
    b->y = 92 + i * 84;
    b->w = LCD_W - 24;
    b->h = 76;
    b->label = video_presets[i].name;
    b->id = (int)(BTN_VIDPRESET + i);
    b->scale = 3;
}

static void draw_video(uint16_t *fb)
{
    draw_status_bar(fb);
    ui_fill_rect(fb, LCD_W, LCD_H, 0, BAR_H, LCD_W, LCD_H, UI_COLOR_BG);

    ui_text(fb, LCD_W, LCD_H, 12, 44, "ВИДЕО", UI_SCALE_TITLE, UI_COLOR_WARN, UI_COLOR_BG);

    for (size_t i = 0; i < VIDEO_PRESETS_N; i++) {
        ui_button_t b;
        video_preset_btn((int)i, &b);
        ui_rrect(fb, LCD_W, LCD_H, b.x, b.y, b.x + b.w, b.y + b.h, UI_R_KEY, UI_COLOR_PANEL);
        ui_rrect_frame(fb, LCD_W, LCD_H, b.x, b.y, b.x + b.w, b.y + b.h, UI_R_KEY,
                       UI_COLOR_BORDER);
        ui_circle(fb, LCD_W, LCD_H, b.x + 44, b.y + b.h / 2, 16, UI_COLOR_BG_PRESS);
        ui_icon(&ui_icon_play, fb, LCD_W, LCD_H, b.x + 40, b.y + (b.h - 16) / 2, 1,
                UI_COLOR_WARN);
        int tw = ui_text_width(b.label, 3);
        ui_text(fb, LCD_W, LCD_H, b.x + (b.w - tw) / 2 + 16,
                b.y + (b.h - UI_FONT_H * 3) / 2, b.label, 3, UI_COLOR_FG, UI_COLOR_PANEL);
    }

    ui_button_t url = { 168, 356, 320, 64, "СВОЙ URL", BTN_MURL, 3 };
    ui_button(fb, LCD_W, LCD_H, &url, false);
    ui_button_t sd = { 536, 356, 320, 64, "SD-КАРТА", BTN_SD, 3 };
    ui_button(fb, LCD_W, LCD_H, &sd, false);

    ui_button_t back = { 372, 436, 280, 56, "В МЕНЮ", BTN_MBACK, 3 };
    ui_button(fb, LCD_W, LCD_H, &back, false);

    const char *hint = "MP4 (H.264 + AAC) по HTTP/HTTPS или с SD-карты";
    ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(hint, 1)) / 2,
            508, hint, 1, UI_COLOR_BORDER, UI_COLOR_BG);
}

static int video_hit(int x, int y)
{
    for (size_t i = 0; i < VIDEO_PRESETS_N; i++) {
        ui_button_t b;
        video_preset_btn((int)i, &b);
        if (ui_button_hit(&b, x, y)) {
            return b.id;
        }
    }
    ui_button_t url = { 168, 356, 320, 64, "СВОЙ URL", BTN_MURL, 3 };
    if (ui_button_hit(&url, x, y)) {
        return BTN_MURL;
    }
    ui_button_t sd = { 536, 356, 320, 64, "SD-КАРТА", BTN_SD, 3 };
    if (ui_button_hit(&sd, x, y)) {
        return BTN_SD;
    }
    ui_button_t back = { 372, 436, 280, 56, "В МЕНЮ", BTN_MBACK, 3 };
    if (ui_button_hit(&back, x, y)) {
        return BTN_MBACK;
    }
    return BTN_NONE;
}

/* --- SD card file list ------------------------------------------------------- */

#define SD_ROWS_Y0      92      /* first file row                  */
#define SD_ROW_H        48
#define SD_ROW_STRIDE   52
#define SD_ROWS         7       /* visible rows                    */
#define SD_FILES_MAX    32      /* scanned entries (scroll for more) */
#define SD_NAME_MAX     96

static struct {
    char name[SD_NAME_MAX];
    bool video;                 /* video file (else audio)         */
} sd_files[SD_FILES_MAX];
static int  sd_files_n;
static int  sd_scroll;
static bool sd_mount_err;
static bool sd_from_video;      /* which screen opened the list    */

/* where ST_VIDEOP returns after the playback stops */
static browser_state_t videop_back = ST_VIDEO;
static char films_msg[96];      /* status line of the films results screen
                                 * (also used by video_start_fail) */

/* forward decl: sd_play (below) starts videos through it */
static void video_start_ui(const char *url);

static bool sd_ext_match(const char *name, const char *const *exts, size_t n)
{
    size_t len = strlen(name);
    for (size_t i = 0; i < n; i++) {
        size_t el = strlen(exts[i]);
        if (len > el && strcasecmp(name + len - el, exts[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Mount the card (lazy) and list media files in its root. The player
 * decodes what this build enables: MP3/AAC/WAV/M4A audio, MP4 video. */
static void sd_scan(void)
{
    sd_files_n = 0;
    sd_scroll = 0;
    sd_mount_err = false;

    if (sdcard_mount() != ESP_OK) {
        sd_mount_err = true;
        return;
    }
    DIR *d = opendir(sdcard_mp());
    if (d == NULL) {
        sd_mount_err = true;
        return;
    }
    static const char *const audio_ext[] = { ".mp3", ".wav", ".m4a", ".aac" };
    static const char *const video_ext[] = { ".mp4", ".m4v", ".mov" };
    struct dirent *e;
    while ((e = readdir(d)) != NULL && sd_files_n < SD_FILES_MAX) {
        const char *nm = e->d_name;
        if (nm[0] == '.' || strlen(nm) >= SD_NAME_MAX) {
            continue;
        }
        bool video = sd_ext_match(nm, video_ext, sizeof(video_ext) / sizeof(video_ext[0]));
        if (!video && !sd_ext_match(nm, audio_ext, sizeof(audio_ext) / sizeof(audio_ext[0]))) {
            continue;
        }
        strlcpy(sd_files[sd_files_n].name, nm, SD_NAME_MAX);
        sd_files[sd_files_n].video = video;
        sd_files_n++;
    }
    closedir(d);

    /* FAT lists in creation order - sort alphabetically for usability */
    for (int i = 1; i < sd_files_n; i++) {
        typeof(sd_files[0]) t = sd_files[i];
        int j = i - 1;
        while (j >= 0 && strcmp(sd_files[j].name, t.name) > 0) {
            sd_files[j + 1] = sd_files[j];
            j--;
        }
        sd_files[j + 1] = t;
    }
    ESP_LOGI(TAG, "SD: %d media file(s) in %s", sd_files_n, sdcard_mp());
}

/* Shared by draw and hit-test; label buffer is static on purpose: the
 * browser task is the only caller and label must outlive the call. */
static void sd_file_btn(int vis_row, ui_button_t *b)
{
    static char lab[SD_NAME_MAX + 8];
    const typeof(sd_files[0]) *f = &sd_files[sd_scroll + vis_row];
    snprintf(lab, sizeof(lab), "%s", f->name);
    b->x = 12;
    b->y = SD_ROWS_Y0 + vis_row * SD_ROW_STRIDE;
    b->w = LCD_W - 24;
    b->h = SD_ROW_H;
    b->label = lab;
    b->id = BTN_FILEROW;
    b->scale = 2;
}

static void draw_files(uint16_t *fb)
{
    draw_status_bar(fb);
    ui_fill_rect(fb, LCD_W, LCD_H, 0, BAR_H, LCD_W, LCD_H, UI_COLOR_BG);

    ui_text(fb, LCD_W, LCD_H, 12, 44, "ФАЙЛЫ SD", UI_SCALE_TITLE,
            UI_COLOR_ACCENT, UI_COLOR_BG);
    char st[48];
    snprintf(st, sizeof(st), "%s", media_state_str());
    ui_text(fb, LCD_W, LCD_H, LCD_W - ui_text_width(st, UI_SCALE_TEXT) - 12,
            52, st, UI_SCALE_TEXT, UI_COLOR_DIM, UI_COLOR_BG);

    if (sd_mount_err) {
        const char *msg = "карта не читается (FAT32?) - ОБНОВИТЬ повторит";
        ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(msg, UI_SCALE_TEXT)) / 2,
                260, msg, UI_SCALE_TEXT, UI_COLOR_ERR, UI_COLOR_BG);
    } else if (sd_files_n == 0) {
        const char *msg = "нет медиафайлов в корне карты";
        ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(msg, UI_SCALE_TEXT)) / 2,
                260, msg, UI_SCALE_TEXT, UI_COLOR_BORDER, UI_COLOR_BG);
    } else {
        int rows = sd_files_n - sd_scroll;
        if (rows > SD_ROWS) {
            rows = SD_ROWS;
        }
        for (int i = 0; i < rows; i++) {
            ui_button_t b;
            sd_file_btn(i, &b);
            const typeof(sd_files[0]) *f = &sd_files[sd_scroll + i];
            ui_rrect(fb, LCD_W, LCD_H, b.x, b.y, b.x + b.w, b.y + b.h, UI_R_KEY,
                     UI_COLOR_PANEL);
            ui_rrect_frame(fb, LCD_W, LCD_H, b.x, b.y, b.x + b.w, b.y + b.h, UI_R_KEY,
                           UI_COLOR_BORDER);
            ui_icon(f->video ? &ui_icon_clap : &ui_icon_music,
                    fb, LCD_W, LCD_H, b.x + 16, b.y + (b.h - 16) / 2, 1,
                    f->video ? UI_COLOR_WARN : UI_COLOR_OK);
            ui_text(fb, LCD_W, LCD_H, b.x + 44, b.y + (b.h - UI_FONT_H * 2) / 2,
                    b.label, 2, UI_COLOR_FG, UI_COLOR_PANEL);
        }
        int left = sd_files_n - sd_scroll - rows;
        if (left > 0) {
            char more[48];
            snprintf(more, sizeof(more), "листать: ВНИЗ (ещё %d)", left);
            ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(more, 1)) / 2,
                    536, more, 1, UI_COLOR_BORDER, UI_COLOR_BG);
        }
    }

    static const struct { int w; const char *lab; int id; } ctrls[] = {
        { 140, "ВВЕРХ",    BTN_FUP    }, { 200, "ОБНОВИТЬ", BTN_FREFR },
        { 140, "ВНИЗ",     BTN_FDOWN  }, { 200, "НАЗАД",    BTN_MBACK },
    };
    int x = (LCD_W - (140 + 200 + 140 + 200 + 3 * 12)) / 2;
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++) {
        ui_button_t b = { x, 466, ctrls[i].w, 60, ctrls[i].lab, ctrls[i].id, 2 };
        ui_button(fb, LCD_W, LCD_H, &b, false);
        x += ctrls[i].w + 12;
    }
}

static int files_hit(int x, int y)
{
    if (y >= SD_ROWS_Y0 && y < SD_ROWS_Y0 + SD_ROWS * SD_ROW_STRIDE - 4 &&
        x >= 12 && x < LCD_W - 12) {
        return BTN_FILEROW;
    }
    static const struct { int w; int id; } ctrls[] = {
        { 140, BTN_FUP }, { 200, BTN_FREFR }, { 140, BTN_FDOWN }, { 200, BTN_MBACK },
    };
    int cx = (LCD_W - (140 + 200 + 140 + 200 + 3 * 12)) / 2;
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++) {
        if (x >= cx && x < cx + ctrls[i].w && y >= 466 && y < 526) {
            return ctrls[i].id;
        }
        cx += ctrls[i].w + 12;
    }
    return BTN_NONE;
}

/* Play one file from the list (index is absolute, not scrolled) */
static void sd_play(int idx)
{
    if (idx < 0 || idx >= sd_files_n) {
        return;
    }
    char path[SD_NAME_MAX + 24];
    snprintf(path, sizeof(path), "%s/%s", sdcard_mp(), sd_files[idx].name);
    ESP_LOGI(TAG, "play from SD: %s", path);
    if (sd_files[idx].video) {
        videop_back = ST_FILES;
        video_start_ui(path);
    } else {
        media_radio_start(path);   /* audio keeps playing in background */
        draw_screen();
    }
}

static void draw_urlin(uint16_t *fb)
{
    draw_status_bar(fb);
    draw_url_row(fb);

    ui_fill_rect(fb, LCD_W, LCD_H, 0, CONTENT_Y0, LCD_W, CONTENT_Y1, UI_COLOR_BG);
    const char *target = br.urlin_video ? "адрес видео (MP4):" : "адрес потока (MP3/AAC):";
    ui_text(fb, LCD_W, LCD_H, 12, CONTENT_Y0 + 8, target, UI_SCALE_TEXT,
            UI_COLOR_DIM, UI_COLOR_BG);
    const char *hint = "GO внизу или справа - начать";
    ui_text(fb, LCD_W, LCD_H, LCD_W - ui_text_width(hint, 1) - 12, CONTENT_Y0 + 8,
            hint, 1, UI_COLOR_BORDER, UI_COLOR_BG);

    draw_keyboard(fb);
}

/* Start a video (preset, custom URL or a film from the catalog): the busy
 * card stays up through esp_player's whole PREPARING phase (HTTP open +
 * moov parse on a slow node is another silent minute+), then the panel is
 * handed over to the render on PLAYING. A tap cancels the wait. The caller
 * sets videop_back. */
static void video_start_fail(const char *msg)
{
    if (videop_back == ST_FILMSR) {
        strlcpy(films_msg, msg, sizeof(films_msg));
    }
    br.state = videop_back;
    draw_screen();
}

static void video_start_ui(const char *url)
{
    if (!s_busy.on) {           /* films path reuses the open busy card */
        busy_open("видео");
    }
    /* Тест скорости перед фильмом (Ethernet или Wi-Fi) — результат
     * в лог, в RAM-историю и на http://IP/speed */
    busy_stage("тест скорости...");
    speed_result_t spd;
    if (speed_test_run(&spd) == ESP_OK) {
        char line[64];
        snprintf(line, sizeof(line), "%.1f Мбит/с (%s)",
                 (double)spd.mbps, spd.iface_str);
        busy_stage(line);
        if (!speed_test_ok_for_smooth(&spd)) {
            ESP_LOGW(TAG, "скорость низкая (%.2f Мбит/с) — стрим может рваться",
                     (double)spd.mbps);
            busy_stage("медленно — продолжаю");
            vTaskDelay(pdMS_TO_TICKS(800));
        } else {
            vTaskDelay(pdMS_TO_TICKS(400));
        }
    } else {
        ESP_LOGW(TAG, "тест скорости не удался: %s", spd.note);
        busy_stage("сеть: без замера");
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    busy_stage("подключаюсь");

    esp_err_t rc = media_video_start(url);
    if (rc == ESP_OK) {
        busy_stage("готовлю плеер");
        /* esp_player prepares asynchronously (HTTP open, moov parse, track
         * info); CONNECTING ends with PLAYING, ERROR or FINISHED */
        bool cancel = false, pressed = false;
        while (media_get_state() == MEDIA_STATE_CONNECTING) {
            touch_point_t pts[TOUCH_MAX_POINTS];
            bool now = touch_poll(pts, TOUCH_MAX_POINTS) > 0;
            if (now) {
                pressed = true;
            } else if (pressed) {
                cancel = true;      /* press-and-release = tap = cancel */
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        /* swallow a still-held finger (max 3 s) so its release cannot
         * leak a stray tap into the ST_VIDEOP on-screen buttons */
        int64_t swallow_until = esp_timer_get_time() + 3000000LL;
        while (pressed && esp_timer_get_time() < swallow_until) {
            touch_point_t pts[TOUCH_MAX_POINTS];
            pressed = touch_poll(pts, TOUCH_MAX_POINTS) > 0;
            if (pressed) {
                vTaskDelay(pdMS_TO_TICKS(30));
            }
        }
        busy_close();   /* animator off BEFORE the render owns the panel */

        if (cancel) {
            media_stop();
            ESP_LOGI(TAG, "playback cancelled by tap");
            video_start_fail("отменено");
            return;
        }
        if (media_get_state() == MEDIA_STATE_PLAYING) {
            br.state = ST_VIDEOP;   /* render owns the panel from here */
            return;
        }
        media_state_t st = media_get_state();
        media_stop();
        ESP_LOGW(TAG, "playback did not start (%s)", media_state_str());
        if (st == MEDIA_STATE_FINISHED) {
            ESP_LOGE(TAG, "Видеопоток отвергнут декодером tinyH264: неподдерживаемый формат/профиль. "
                          "ESP32-P4 поддерживает только H.264 Baseline Profile (без CABAC/B-кадров). "
                          "Используйте tools/convert_movie.bat или ffmpeg -profile:v baseline");
            video_start_fail("неподдерживаемый H.264: нужен Baseline Profile (см. tools/convert_movie)");
        } else {
            video_start_fail("плеер: не удалось начать воспроизведение");
        }
        return;
    }

    busy_close();
    ESP_LOGW(TAG, "media_video_start failed");
    video_start_fail("плеер: не удалось запустить конвейер");
}



#if CONFIG_EB_WIFI_ENABLE
/* --- Экран Wi‑Fi менеджера ------------------------------------------------ */

static void draw_wifi(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, LCD_H, UI_COLOR_BG);
    draw_status_bar(fb);
    ui_text(fb, LCD_W, LCD_H, 16, 56, "Wi-Fi (C6)", 2, UI_COLOR_FG, UI_COLOR_BG);

    const char *st = wifi_mgr_status_str();
    ui_text(fb, LCD_W, LCD_H, 16, 96, st, 1, UI_COLOR_DIM, UI_COLOR_BG);

    char line[96];
    if (wifi_mgr_saved_ssid()[0]) {
        snprintf(line, sizeof(line), "NVS: %s", wifi_mgr_saved_ssid());
        ui_text(fb, LCD_W, LCD_H, 16, 120, line, 1, UI_COLOR_DIM, UI_COLOR_BG);
    }
    if (wifi_mgr_ip_str()[0]) {
        snprintf(line, sizeof(line), "IP: %s", wifi_mgr_ip_str());
        ui_text(fb, LCD_W, LCD_H, 16, 144, line, 1, UI_COLOR_ACCENT, UI_COLOR_BG);
    }

    ui_button_t bscan   = { 16,  180, 220, 56, "СКАН",     BTN_WIFI_SCAN,   2 };
    ui_button_t bconn   = { 250, 180, 260, 56, "ПОДКЛ NVS", BTN_WIFI_CONN,  2 };
    ui_button_t bportal = { 524, 180, 220, 56, "ПОРТАЛ",   BTN_WIFI_PORTAL, 2 };
    ui_button_t btest   = { 758, 180, 250, 56, "ТЕСТ NET", BTN_WIFI_TEST,  2 };
    ui_button(fb, LCD_W, LCD_H, &bscan, false);
    ui_button(fb, LCD_W, LCD_H, &bconn, false);
    ui_button(fb, LCD_W, LCD_H, &bportal, false);
    ui_button(fb, LCD_W, LCD_H, &btest, false);

    /* Шрифт сетей: scale 2 (1024×600 — scale 1 слишком мелкий) */
    ui_text(fb, LCD_W, LCD_H, 16, 248, "Сети (2.4 ГГц):", 2, UI_COLOR_FG, UI_COLOR_BG);
    int n = wifi_mgr_scan_count();
    int y = 288;
    for (int i = 0; i < n && i < 5; i++) {
        const wifi_mgr_ap_t *ap = wifi_mgr_scan_get(i);
        if (!ap) break;
        snprintf(line, sizeof(line), "%s  %ddBm%s",
                 ap->ssid, (int)ap->rssi, ap->is_open ? " open" : "");
        ui_button_t row = { 16, y, LCD_W - 32, 48, line, BTN_WIFI_ROW + i, 2 };
        ui_button(fb, LCD_W, LCD_H, &row, false);
        y += 52;
    }
    if (n == 0) {
        ui_text(fb, LCD_W, LCD_H, 16, 300, "Нажмите СКАН", 2, UI_COLOR_DIM, UI_COLOR_BG);
    }

    ui_button_t home = { 8, LCD_H - 60, 180, 56, "МЕНЮ", BTN_HOME, 0 };
    ui_button(fb, LCD_W, LCD_H, &home, false);
}


static void draw_wifi_pass(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, LCD_H, UI_COLOR_BG);
    draw_status_bar(fb);
    ui_text(fb, LCD_W, LCD_H, 16, 56, "Пароль Wi-Fi", 2, UI_COLOR_FG, UI_COLOR_BG);
    char line[80];
    snprintf(line, sizeof(line), "Сеть: %s", wifi_pending_ssid[0] ? wifi_pending_ssid : "?");
    ui_text(fb, LCD_W, LCD_H, 16, 96, line, 2, UI_COLOR_ACCENT, UI_COLOR_BG);

    /* поле пароля */
    ui_rrect(fb, LCD_W, LCD_H, 16, 140, LCD_W - 16, 200, 8, UI_COLOR_PANEL);
    const char *show = br.query[0] ? br.query : "введите пароль...";
    ui_text(fb, LCD_W, LCD_H, 28, 160, show, 2,
            br.query[0] ? UI_COLOR_FG : UI_COLOR_DIM, UI_COLOR_PANEL);

    ui_button_t bgo = { LCD_W - 200, 140, 180, 56, "GO", BTN_GO, 2 };
    ui_button(fb, LCD_W, LCD_H, &bgo, false);
    /* НАЗАД выше клавиатуры (CONTENT_Y1=368) */
    ui_button_t bback = { 16, 300, 180, 56, "НАЗАД", BTN_HOME, 0 };
    ui_button(fb, LCD_W, LCD_H, &bback, false);

    draw_keyboard(fb);
}

static void wifi_handle_tap(int x, int y)
{
    ui_button_t bscan   = { 16,  180, 220, 56, "СКАН",     BTN_WIFI_SCAN,   2 };
    ui_button_t bconn   = { 250, 180, 260, 56, "ПОДКЛ NVS", BTN_WIFI_CONN,  2 };
    ui_button_t bportal = { 524, 180, 220, 56, "ПОРТАЛ",   BTN_WIFI_PORTAL, 2 };
    ui_button_t btest   = { 758, 180, 250, 56, "ТЕСТ NET", BTN_WIFI_TEST,  2 };
    ui_button_t home = { 8, LCD_H - 60, 180, 56, "МЕНЮ", BTN_HOME, 0 };

    if (ui_button_hit(&home, x, y)) {
        br.state = ST_HOME;
        return;
    }
    if (ui_button_hit(&bscan, x, y)) {
        busy_open("Wi-Fi");
        busy_stage("скан...");
        wifi_mgr_scan();
        busy_close();
        return;
    }
    if (ui_button_hit(&bconn, x, y)) {
        const char *ssid = wifi_mgr_saved_ssid();
        busy_open("Wi-Fi");
        if (!ssid[0]) {
            busy_stage("нет SSID в NVS");
            vTaskDelay(pdMS_TO_TICKS(900));
        } else {
            busy_stage("подключение...");
            wifi_mgr_connect(ssid, NULL);
        }
        busy_close();
        return;
    }
    if (ui_button_hit(&bportal, x, y)) {
        busy_open("Портал");
        busy_stage("CamBrowser-Setup");
        wifi_mgr_start_portal();
        busy_stage("192.168.4.1");
        vTaskDelay(pdMS_TO_TICKS(1200));
        busy_close();
        return;
    }
    if (ui_button_hit(&btest, x, y)) {
        busy_open("Тест сети");
        busy_stage("generate_204...");
        wifi_mgr_test_t tr;
        wifi_mgr_test_internet(&tr);
        busy_stage(tr.note);
        vTaskDelay(pdMS_TO_TICKS(1500));
        busy_close();
        return;
    }
    /* y0 как в draw_wifi (288) */
    int n = wifi_mgr_scan_count();
    int y0 = 288;
    for (int i = 0; i < n && i < 5; i++) {
        ui_button_t row = { 16, y0 + i * 52, LCD_W - 32, 48, "", BTN_WIFI_ROW + i, 2 };
        if (ui_button_hit(&row, x, y)) {
            const wifi_mgr_ap_t *ap = wifi_mgr_scan_get(i);
            if (!ap) {
                return;
            }
            busy_open("Сеть");
            busy_stage(ap->ssid);
            if (ap->is_open) {
                busy_stage("открытая — connect");
                wifi_mgr_connect(ap->ssid, "");
                busy_close();
            } else {
                /* пароль на экранной клавиатуре платы */
                strlcpy(wifi_pending_ssid, ap->ssid, sizeof(wifi_pending_ssid));
                br.query[0] = '\0';
                br.kb_ru = false;
                busy_close();
                br.state = ST_WIFI_PASS;
                draw_screen();
            }
            return;
        }
    }
}

static void draw_wifi_diag(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, LCD_H, UI_COLOR_BG);

    wifi_mgr_diag_t diag;
    wifi_mgr_get_diag(&diag);

    bool connected = (diag.state == WIFI_MGR_CONNECTED && diag.ip[0] != '\0' && strcmp(diag.ip, "0.0.0.0") != 0);
    uint16_t status_col = connected ? UI_COLOR_OK :
                         (diag.state == WIFI_MGR_CONNECTING ? UI_COLOR_WARN : UI_COLOR_ERR);
    const char *status_title = connected ? "ПОДКЛЮЧЕНО" :
                              (diag.state == WIFI_MGR_CONNECTING ? "ПОДКЛЮЧЕНИЕ..." : "НЕТ СВЯЗИ");

    ui_icon(connected ? &ui_icon_wifi : &ui_icon_wifi_off,
            fb, LCD_W, LCD_H, 16, 12, 2, status_col);
    ui_text(fb, LCD_W, LCD_H, 56, 10, "ТЕРМИНАЛ ПОДКЛЮЧЕНИЯ WI-FI",
            UI_SCALE_TITLE, UI_COLOR_FG, UI_COLOR_BG);
    ui_text(fb, LCD_W, LCD_H, 56, 38, "ESP32-P4 + ESP32-C6 (SDIO esp-hosted v3.0.9)",
            1, UI_COLOR_DIM, UI_COLOR_BG);

    /* Бейдж статуса в правом верхнем углу */
    int st_w = 210, st_h = 38;
    ui_rrect(fb, LCD_W, LCD_H, LCD_W - st_w - 16, 10, LCD_W - 16, 10 + st_h, 6, UI_COLOR_PANEL);
    ui_rrect_frame(fb, LCD_W, LCD_H, LCD_W - st_w - 16, 10, LCD_W - 16, 10 + st_h, 6, status_col);
    int tw = ui_text_width(status_title, 2);
    ui_text(fb, LCD_W, LCD_H, LCD_W - st_w - 16 + (st_w - tw) / 2, 10 + (st_h - 16) / 2,
            status_title, 2, status_col, UI_COLOR_PANEL);

    /* === ЛЕВАЯ КОЛОНКА (x=16..440, y=56..516): Параметры сети === */
    int lx0 = 16, ly0 = 56, lx1 = 440, ly1 = 516;
    ui_card(fb, LCD_W, LCD_H, lx0, ly0, lx1, ly1, false);

    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 10, "ПАРАМЕТРЫ СОЕДИНЕНИЯ",
            1, UI_COLOR_ACCENT, UI_COLOR_PANEL);
    ui_line(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 24, lx1 - 16, ly0 + 24, UI_COLOR_BORDER);

    /* Блок SSID */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 34, "Сеть (SSID):", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_rrect(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 48, lx1 - 16, ly0 + 82, 6, UI_COLOR_KEY);
    ui_rrect_frame(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 48, lx1 - 16, ly0 + 82, 6, UI_COLOR_BORDER);
    ui_text(fb, LCD_W, LCD_H, lx0 + 26, ly0 + 56, diag.ssid, 2, UI_COLOR_FG, UI_COLOR_KEY);

    /* BSSID */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 94, "BSSID (MAC AP):", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 94, diag.bssid_str, 1, UI_COLOR_FG, UI_COLOR_PANEL);

    /* Канал */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 114, "Канал Wi-Fi:", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    char ch_buf[48];
    if (diag.channel > 0) {
        snprintf(ch_buf, sizeof(ch_buf), "%d (2.4 ГГц, 802.11ax/Wi-Fi 6)", diag.channel);
    } else {
        snprintf(ch_buf, sizeof(ch_buf), "—");
    }
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 114, ch_buf, 1, UI_COLOR_FG, UI_COLOR_PANEL);

    /* Шифрование */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 134, "Шифрование:", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 134, diag.auth_str, 1, UI_COLOR_FG, UI_COLOR_PANEL);

    /* RSSI */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 154, "Сигнал (RSSI):", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    char rssi_buf[32];
    if (diag.rssi != 0) {
        snprintf(rssi_buf, sizeof(rssi_buf), "%d dBm", (int)diag.rssi);
    } else {
        snprintf(rssi_buf, sizeof(rssi_buf), "—");
    }
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 154, rssi_buf, 1, UI_COLOR_FG, UI_COLOR_PANEL);

    float frac = 0.0f;
    if (diag.rssi < -95) {
        frac = 0.1f;
    } else if (diag.rssi >= -50) {
        frac = 1.0f;
    } else if (diag.rssi != 0) {
        frac = (float)(diag.rssi - (-95)) / 45.0f;
    }
    ui_progress(fb, LCD_W, LCD_H, lx0 + 220, ly0 + 155, 190, 10, frac,
                frac > 0.6f ? UI_COLOR_OK : (frac > 0.3f ? UI_COLOR_WARN : UI_COLOR_ERR));

    /* Разделитель IP блока */
    ui_line(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 178, lx1 - 16, ly0 + 178, UI_COLOR_BORDER);
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 188, "СЕТЕВАЯ АДРЕСАЦИЯ (DHCP)",
            1, UI_COLOR_ACCENT, UI_COLOR_PANEL);

    /* IP адрес */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 210, "IP адрес:", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 206, diag.ip, 2, UI_COLOR_OK, UI_COLOR_PANEL);

    /* Маска подсети */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 232, "Маска подсети:", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 232, diag.netmask, 1, UI_COLOR_FG, UI_COLOR_PANEL);

    /* Шлюз */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 252, "Основной шлюз:", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 252, diag.gw, 1, UI_COLOR_FG, UI_COLOR_PANEL);

    /* DNS */
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 272, "DNS сервер:", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 150, ly0 + 272, diag.dns, 1, UI_COLOR_FG, UI_COLOR_PANEL);

    /* Аппаратный стек */
    ui_line(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 296, lx1 - 16, ly0 + 296, UI_COLOR_BORDER);
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 306, "АППАРАТНЫЙ СТЕК SDIO",
            1, UI_COLOR_ACCENT, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 328, diag.c6_fw_str, 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 348, "Линк: SDIO 4-bit, esp-wifi-remote", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 368, "SW_AGGR: negotiated rx=15.8K tx=15.8K", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 388, "Режим: WIFI_PS_NONE (макс. скорость)", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 16, ly0 + 412, "Статус менеджера:", 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    ui_text(fb, LCD_W, LCD_H, lx0 + 160, ly0 + 412, diag.status_str, 1, UI_COLOR_ACCENT, UI_COLOR_PANEL);

    /* === ПРАВАЯ КОЛОНКА (x=454..1008, y=56..516): Окно терминала логов === */
    int rx0 = 454, ry0 = 56, rx1 = 1008, ry1 = 516;
    ui_rrect(fb, LCD_W, LCD_H, rx0, ry0, rx1, ry1, 8, 0x0000);
    ui_rrect_frame(fb, LCD_W, LCD_H, rx0, ry0, rx1, ry1, 8, UI_COLOR_BORDER);

    /* Хедер терминала */
    ui_fill_rect(fb, LCD_W, LCD_H, rx0 + 1, ry0 + 1, rx1 - 1, ry0 + 26, UI_COLOR_KEY);
    ui_line(fb, LCD_W, LCD_H, rx0, ry0 + 26, rx1, ry0 + 26, UI_COLOR_BORDER);
    ui_circle(fb, LCD_W, LCD_H, rx0 + 14, ry0 + 13, 4, UI_COLOR_ERR);
    ui_circle(fb, LCD_W, LCD_H, rx0 + 26, ry0 + 13, 4, UI_COLOR_WARN);
    ui_circle(fb, LCD_W, LCD_H, rx0 + 38, ry0 + 13, 4, UI_COLOR_OK);
    ui_text(fb, LCD_W, LCD_H, rx0 + 52, ry0 + 9, "терминал: /dev/esp_hosted/wlan0 [лог событий]",
            1, UI_COLOR_DIM, UI_COLOR_KEY);

    /* Строки логов терминала */
    int log_y = ry0 + 36;
    int max_vis_lines = 16;
    int start_i = 0;
    if (diag.log_count > max_vis_lines) {
        start_i = diag.log_count - max_vis_lines;
    }
    int last_x = rx0 + 14;
    int last_y = log_y;

    for (int i = start_i; i < diag.log_count; i++) {
        const char *line = diag.logs[i];
        uint16_t col = UI_COLOR_FG;
        if (strstr(line, "OK") || strstr(line, "есть IP") || strstr(line, "Связано") || strstr(line, "установлена")) {
            col = UI_COLOR_OK;
        } else if (strstr(line, "Обрыв") || strstr(line, "fail") || strstr(line, "ошибка")) {
            col = UI_COLOR_ERR;
        } else if (strstr(line, "повтор") || strstr(line, "скан") || strstr(line, "подключение")) {
            col = UI_COLOR_WARN;
        }

        char fitted[WIFI_MGR_LOG_LEN];
        ui_text_fit(fitted, sizeof(fitted), line, rx1 - rx0 - 28, 1);
        ui_text(fb, LCD_W, LCD_H, rx0 + 14, log_y, fitted, 1, col, 0x0000);
        last_x = rx0 + 14 + ui_text_width(fitted, 1);
        last_y = log_y;
        log_y += 24;
    }

    if (diag.log_count == 0) {
        ui_text(fb, LCD_W, LCD_H, rx0 + 14, log_y, "[00:00.0] Ожидание сетевых событий...",
                1, UI_COLOR_DIM, 0x0000);
        last_x = rx0 + 14 + ui_text_width("[00:00.0] Ожидание сетевых событий...", 1);
        last_y = log_y;
        log_y += 24;
    }

    /* Мигающий консольный курсор */
    if ((esp_timer_get_time() / 500000LL) & 1) {
        int cur_x = (last_x + 6 < rx1 - 20) ? (last_x + 4) : (rx0 + 14);
        int cur_y = (last_x + 6 < rx1 - 20) ? last_y : log_y;
        ui_fill_rect(fb, LCD_W, LCD_H, cur_x, cur_y, cur_x + 8, cur_y + 10, UI_COLOR_OK);
    }

    /* === НИЖНИЙ РЯД КНОПОК ДЕЙСТВИЙ (y=530..586, высота 56) === */
    ui_button_t bclose  = { 16,  530, 220, 56, "ЗАКРЫТЬ",        BTN_WIFI_DIAG_CLOSE,  2 };
    ui_button_t breconn = { 250, 530, 240, 56, "ПЕРЕПОДКЛЮЧИТЬ", BTN_WIFI_DIAG_RECONN, 2 };
    ui_button_t bscan   = { 504, 530, 240, 56, "СПИСОК СЕТЕЙ",   BTN_WIFI_DIAG_SCAN,   2 };
    ui_button_t btest   = { 758, 530, 250, 56, "ТЕСТ СЕТИ",      BTN_WIFI_DIAG_TEST,   2 };

    ui_button(fb, LCD_W, LCD_H, &bclose, false);
    ui_button(fb, LCD_W, LCD_H, &breconn, false);
    ui_button(fb, LCD_W, LCD_H, &bscan, false);
    ui_button(fb, LCD_W, LCD_H, &btest, false);
}

static void wifi_diag_handle_tap(int x, int y)
{
    ui_button_t bclose  = { 16,  530, 220, 56, "ЗАКРЫТЬ",        BTN_WIFI_DIAG_CLOSE,  2 };
    ui_button_t breconn = { 250, 530, 240, 56, "ПЕРЕПОДКЛЮЧИТЬ", BTN_WIFI_DIAG_RECONN, 2 };
    ui_button_t bscan   = { 504, 530, 240, 56, "СПИСОК СЕТЕЙ",   BTN_WIFI_DIAG_SCAN,   2 };
    ui_button_t btest   = { 758, 530, 250, 56, "ТЕСТ СЕТИ",      BTN_WIFI_DIAG_TEST,   2 };

    if (ui_button_hit(&bclose, x, y)) {
        br.state = (br.wifi_diag_back != ST_WIFI_DIAG) ? br.wifi_diag_back : ST_HOME;
        return;
    }
    if (ui_button_hit(&breconn, x, y)) {
        const char *ssid = wifi_mgr_saved_ssid();
        busy_open("Wi-Fi");
        if (!ssid[0]) {
            busy_stage("нет сети в памяти");
            vTaskDelay(pdMS_TO_TICKS(800));
        } else {
            busy_stage("подключение...");
            wifi_mgr_connect(ssid, NULL);
        }
        busy_close();
        return;
    }
    if (ui_button_hit(&bscan, x, y)) {
        br.state = ST_WIFI;
        return;
    }
    if (ui_button_hit(&btest, x, y)) {
        busy_open("Тест сети");
        busy_stage("generate_204 ping...");
        wifi_mgr_test_t tr;
        wifi_mgr_test_internet(&tr);
        busy_stage(tr.note);
        vTaskDelay(pdMS_TO_TICKS(1400));
        busy_close();
        return;
    }
}
#endif /* CONFIG_EB_WIFI_ENABLE */




/* GO pressed on the URL-input screen (or the keyboard GO): apply the URL */
static void urlin_apply(void)
{
    char url[128];
    if (!media_url_norm(br.query, url, sizeof(url))) {
        return;
    }
    if (br.urlin_video) {
        videop_back = ST_VIDEO;
        video_start_ui(url);
    } else {
        media_radio_start(url);
        br.state = ST_RADIO;
        draw_screen();
    }
}

/* --- Films: public-domain catalog from archive.org ------------------------- */

static film_item_t film_items[FILMS_MAX];
static int  films_n;
static int  films_scroll;

/* Search the catalog (empty query = "popular" list). Blocking, like
 * do_search(): a static "..." frame stays on the panel while we wait. */
static void do_films_search(void)
{
    /* live busy card: films_search fires FILMS_PROG_SEARCH through the
     * callback and the animator keeps the seconds ticking while we wait */
    busy_open(br.query[0] ? br.query : "популярное");
    busy_stage("ищу в каталоге");

    int n = 0;
    esp_err_t err = films_search(br.query, film_items, FILMS_MAX, &n);

    busy_close();

    films_n = n;
    films_scroll = 0;
    if (err != ESP_OK) {
        strlcpy(films_msg, "archive.org не отвечает", sizeof(films_msg));
    } else if (n == 0) {
        strlcpy(films_msg, "ничего не найдено (попробуйте EN)", sizeof(films_msg));
    } else {
        films_msg[0] = '\0';
    }
    br.state = ST_FILMSR;
    draw_screen();
}

/* Pick a result: resolve the mp4 derivative, verify the H.264 profile with
 * a couple of Range reads, only then hand the URL to the player. */
static void open_film(int idx)
{
    if (idx < 0 || idx >= films_n) {
        return;
    }
    /* live busy card with the film title; films_resolve/probe drive the
     * stage line + byte counter through films_prog_cb while this task is
     * blocked (a 3 MB moov at ~20 kB/s is minutes of waiting) */
    char what[40];
    strlcpy(what, film_items[idx].title, sizeof(what));
    size_t wl = strlen(what);
    while (wl > 0 && (((unsigned char)what[wl - 1]) & 0xC0) == 0x80) {
        what[--wl] = '\0';
    }
    busy_open(what);
    busy_stage("читаю метаданные");

    char url[FILMS_URL_MAX];
    films_play_err_t r = films_resolve(film_items[idx].ident, url, sizeof(url));
    uint16_t w = 0, h = 0;
    if (r == FILMS_PLAY_OK) {
        r = films_probe_url(url, &w, &h);
    }
    if (r != FILMS_PLAY_OK) {
        busy_close();
        if (w != 0 && h != 0) {
            snprintf(films_msg, sizeof(films_msg), "%ux%u: %s",
                     w, h, films_err_str(r));
        } else {
            strlcpy(films_msg, films_err_str(r), sizeof(films_msg));
        }
        ESP_LOGW(TAG, "film rejected: %s (%s)", film_items[idx].title, films_msg);
        br.state = ST_FILMSR;
        draw_screen();
        return;
    }
    ESP_LOGI(TAG, "film ok %ux%u: %s", w, h, url);
    busy_stage("запускаю плеер");
    videop_back = ST_FILMSR;
    video_start_ui(url);    /* busy card stays on: prepare phase + cancel */
}

static void draw_films(uint16_t *fb)
{
    draw_status_bar(fb);
    draw_url_row(fb);

    ui_fill_rect(fb, LCD_W, LCD_H, 0, CONTENT_Y0, LCD_W, CONTENT_Y1, UI_COLOR_BG);

    ui_icon(&ui_icon_film, fb, LCD_W, LCD_H, 12, CONTENT_Y0 + 6, 1, UI_COLOR_ACCENT);
    ui_text(fb, LCD_W, LCD_H, 34, CONTENT_Y0 + 5, "каталог кино (archive.org)",
            UI_SCALE_TEXT, UI_COLOR_DIM, UI_COLOR_BG);
    const char *hint = "пустой запрос = популярное";
    ui_text(fb, LCD_W, LCD_H, LCD_W - ui_text_width(hint, 1) - 12, CONTENT_Y0 + 8,
            hint, 1, UI_COLOR_BORDER, UI_COLOR_BG);

    draw_keyboard(fb);
}

static void draw_filmsr(uint16_t *fb)
{
    draw_status_bar(fb);
    draw_url_row(fb);
    ui_fill_rect(fb, LCD_W, LCD_H, 0, CONTENT_Y0, LCD_W, LCD_H, UI_COLOR_BG);

    int rows_max = (CONTENT_Y1_FULL - CONTENT_Y0) / ROW_H;

    if (films_n == 0) {
        /* centered error/empty card */
        int cw = 720, ch = 160;
        int cx = (LCD_W - cw) / 2, cy = CONTENT_Y0 + 40;
        ui_card(fb, LCD_W, LCD_H, cx, cy, cx + cw, cy + ch, false);
        ui_icon(&ui_icon_warn, fb, LCD_W, LCD_H, cx + 24, cy + 24, 1, UI_COLOR_WARN);
        char fitted[96];
        ui_text_fit(fitted, sizeof(fitted),
                    films_msg[0] ? films_msg : "ничего не найдено",
                    cw - 120, UI_SCALE_TEXT);
        ui_text(fb, LCD_W, LCD_H, cx + 56, cy + 26, fitted, UI_SCALE_TEXT,
                films_msg[0] ? UI_COLOR_WARN : UI_COLOR_FG, UI_COLOR_PANEL);
        const char *hint = "тап по экрану - вернуться к поиску";
        ui_text(fb, LCD_W, LCD_H, cx + (cw - ui_text_width(hint, 1)) / 2, cy + ch - 40,
                hint, 1, UI_COLOR_DIM, UI_COLOR_PANEL);
        return;
    }

    if (films_msg[0] != '\0') {
        char fitted[96];
        ui_text_fit(fitted, sizeof(fitted), films_msg, LCD_W - 100, 1);
        ui_text(fb, LCD_W, LCD_H, 12, CONTENT_Y0 + 2, fitted, 1,
                UI_COLOR_WARN, UI_COLOR_BG);
    }

    char cnt[32];
    snprintf(cnt, sizeof(cnt), "найдено: %d", films_n);
    ui_text(fb, LCD_W, LCD_H, LCD_W - ui_text_width(cnt, 1) - 16, CONTENT_Y0 + 2,
            cnt, 1, UI_COLOR_DIM, UI_COLOR_BG);

    int y0 = CONTENT_Y0 + 18;
    int rows = films_n - films_scroll;
    if (rows > rows_max - 1) {
        rows = rows_max - 1;
    }
    for (int i = 0; i < rows; i++) {
        int idx = films_scroll + i;
        int y = y0 + i * ROW_H;

        ui_rrect(fb, LCD_W, LCD_H, 8, y, LCD_W - 8, y + ROW_H - 6, UI_R_KEY,
                 UI_COLOR_PANEL);
        ui_rrect_frame(fb, LCD_W, LCD_H, 8, y, LCD_W - 8, y + ROW_H - 6, UI_R_KEY,
                       UI_COLOR_BORDER);

        char num[16];
        snprintf(num, sizeof(num), "%2d.", idx + 1);
        ui_text(fb, LCD_W, LCD_H, 20, y + (ROW_H - 6 - UI_FONT_H * 2) / 2, num,
                UI_SCALE_TEXT, UI_COLOR_ACCENT, UI_COLOR_PANEL);

        char fitted[FILM_TITLE_MAX];
        ui_text_fit(fitted, sizeof(fitted), film_items[idx].title,
                    LCD_W - 140, UI_SCALE_TEXT);
        ui_text(fb, LCD_W, LCD_H, 64, y + (ROW_H - 6 - UI_FONT_H * 2) / 2, fitted,
                UI_SCALE_TEXT, UI_COLOR_FG, UI_COLOR_PANEL);

        ui_icon(&ui_icon_play, fb, LCD_W, LCD_H, LCD_W - 44, y + (ROW_H - 6 - 16) / 2,
                1, UI_COLOR_ACCENT);
    }

    draw_scrollbar(fb, y0, CONTENT_Y1_FULL, films_n, films_scroll, rows_max - 1);
    draw_nav_bar(fb);
}

#endif /* CONFIG_EB_MEDIA_ENABLE */


static void draw_splash(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, LCD_H, UI_COLOR_BG);

    const char *title = "CamBrowser";
    ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(title, UI_SCALE_TITLE * 2)) / 2,
            180, title, UI_SCALE_TITLE * 2, UI_COLOR_FG, UI_COLOR_BG);

    /* accent underline */
    int tw = ui_text_width(title, UI_SCALE_TITLE * 2);
    ui_rrect(fb, LCD_W, LCD_H, (LCD_W - tw) / 2, 240, (LCD_W + tw) / 2, 248,
             4, UI_COLOR_ACCENT);

    const char *sub = "интернет-браузер на ESP32-P4";
    ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(sub, UI_SCALE_TEXT)) / 2,
            272, sub, UI_SCALE_TEXT, UI_COLOR_DIM, UI_COLOR_BG);

    /* spinner + status card */
    int phase = (int)((esp_timer_get_time() / 250000LL) & 7);
    ui_spinner(fb, LCD_W, LCD_H, LCD_W / 2, 350, 14, phase, UI_COLOR_ACCENT);

    char st[64];
    snprintf(st, sizeof(st), "сеть: %s", app_net_status_str());
    ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(st, UI_SCALE_TEXT)) / 2,
            392, st, UI_SCALE_TEXT, UI_COLOR_FG, UI_COLOR_BG);

    const char *hint = app_net_ready()
        ? "запуск..."
        : "тап или подождите — Wi-Fi в меню HOME";
    ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(hint, UI_SCALE_TEXT)) / 2,
            424, hint, UI_SCALE_TEXT, UI_COLOR_BORDER, UI_COLOR_BG);
}

static void draw_home(uint16_t *fb)
{
    draw_status_bar(fb);
    draw_url_row(fb);

    ui_fill_rect(fb, LCD_W, LCD_H, 0, CONTENT_Y0, LCD_W, CONTENT_Y1, UI_COLOR_BG);

#if CONFIG_EB_MEDIA_ENABLE
    for (size_t i = 0; i < HOME_APPS_N; i++) {
        const ui_button_t *b = &home_apps[i];
        ui_rrect(fb, LCD_W, LCD_H, b->x, b->y, b->x + b->w, b->y + b->h,
                 UI_R_CARD, UI_COLOR_PANEL);
        ui_rrect_frame(fb, LCD_W, LCD_H, b->x, b->y, b->x + b->w, b->y + b->h,
                       UI_R_CARD, UI_COLOR_BORDER);

        const ui_icon_t *ic = home_app_icon(i);
        uint16_t col = home_app_color[i % (sizeof(home_app_color) / sizeof(home_app_color[0]))];
        /* иконка на круге; для низкой карточки Wi‑Fi — компактнее */
        int icy = b->y + (b->h < 120 ? 28 : 44);
        int isc = (b->h < 120) ? 1 : 2;
        int ir = (b->h < 120) ? 16 : 24;
        ui_circle(fb, LCD_W, LCD_H, b->x + b->w / 2, icy, ir, UI_COLOR_BG_PRESS);
        if (ic) {
            ui_icon(ic, fb, LCD_W, LCD_H,
                    b->x + b->w / 2 - 8 * isc, icy - 8 * isc, isc, col);
        }
        int sc = (b->scale > 0) ? b->scale : 3;
        int tw = ui_text_width(b->label, sc);
        int ty = b->y + b->h - (sc * 8) - 12;
        ui_text(fb, LCD_W, LCD_H, b->x + (b->w - tw) / 2, ty, b->label, sc,
                UI_COLOR_FG, UI_COLOR_PANEL);
    }
    const char *hint = "введите запрос и нажмите GO";
    ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(hint, UI_SCALE_TEXT)) / 2,
            370, hint, UI_SCALE_TEXT, UI_COLOR_DIM, UI_COLOR_BG);
#else
    const char *hint = "type a search query, then GO";
    ui_text(fb, LCD_W, LCD_H, (LCD_W - ui_text_width(hint, UI_SCALE_TEXT)) / 2,
            (CONTENT_Y0 + CONTENT_Y1) / 2 - 8, hint, UI_SCALE_TEXT,
            UI_COLOR_DIM, UI_COLOR_BG);
#endif

    draw_keyboard(fb);
}

static void draw_results(uint16_t *fb)
{
    draw_status_bar(fb);
    draw_url_row(fb);
    ui_fill_rect(fb, LCD_W, LCD_H, 0, CONTENT_Y0, LCD_W, LCD_H, UI_COLOR_BG);

    if (br.serp_error || br.result_count == 0) {
        const char *msg = br.serp_error ? "поиск не удался" : "ничего не найдено";
        const char *hint = br.serp_error ? "антинбот или сеть - тап, чтобы вернуться"
                                         : "попробуйте другой запрос - тап, чтобы вернуться";
        int cw = 700, ch = 150;
        int cx = (LCD_W - cw) / 2, cy = CONTENT_Y0 + 40;
        ui_card(fb, LCD_W, LCD_H, cx, cy, cx + cw, cy + ch, false);
        ui_icon(&ui_icon_warn, fb, LCD_W, LCD_H, cx + 24, cy + 24, 1, UI_COLOR_WARN);
        ui_text(fb, LCD_W, LCD_H, cx + 56, cy + 26, msg, UI_SCALE_TEXT,
                UI_COLOR_WARN, UI_COLOR_PANEL);
        ui_text(fb, LCD_W, LCD_H, cx + 56, cy + 60, hint, 1, UI_COLOR_DIM, UI_COLOR_PANEL);
        const char *tap = "тап по экрану - назад";
        ui_text(fb, LCD_W, LCD_H, cx + (cw - ui_text_width(tap, 1)) / 2, cy + ch - 36,
                tap, 1, UI_COLOR_DIM, UI_COLOR_PANEL);
        return;
    }

    int rows_max = (CONTENT_Y1_FULL - CONTENT_Y0) / ROW_H;
    int first = br.page_scroll;
    int rows = br.result_count - first;
    if (rows > rows_max) {
        rows = rows_max;
    }
    int y0 = CONTENT_Y0 + 4;
    for (int i = 0; i < rows; i++) {
        int idx = first + i;
        int y = y0 + i * ROW_H;

        ui_rrect(fb, LCD_W, LCD_H, 8, y, LCD_W - 8, y + ROW_H - 6, UI_R_KEY,
                 UI_COLOR_PANEL);
        ui_rrect_frame(fb, LCD_W, LCD_H, 8, y, LCD_W - 8, y + ROW_H - 6, UI_R_KEY,
                       UI_COLOR_BORDER);

        char num[16]; /* >= 13: worst-case "%2d." is 11 (int) + '.' + NUL — GCC format-truncation */
        snprintf(num, sizeof(num), "%2d.", idx + 1);
        ui_text(fb, LCD_W, LCD_H, 20, y + (ROW_H - 6 - UI_FONT_H * 2) / 2, num,
                UI_SCALE_TEXT, UI_COLOR_ACCENT, UI_COLOR_PANEL);

        char fitted[WEB_RESULT_TITLE_MAX];
        ui_text_fit(fitted, sizeof(fitted), br.results[idx].title,
                    LCD_W - 110, UI_SCALE_TEXT);
        ui_text(fb, LCD_W, LCD_H, 64, y + (ROW_H - 6 - UI_FONT_H * 2) / 2, fitted,
                UI_SCALE_TEXT, UI_COLOR_FG, UI_COLOR_PANEL);

        ui_icon(&ui_icon_back, fb, LCD_W, LCD_H, LCD_W - 36, y + (ROW_H - 6 - 16) / 2,
                1, UI_COLOR_DIM);
    }

    draw_scrollbar(fb, y0, CONTENT_Y1_FULL, br.result_count, first, rows_max);
    draw_nav_bar(fb);
}

static void draw_page(uint16_t *fb)
{
    draw_status_bar(fb);
    draw_url_row(fb);
    ui_fill_rect(fb, LCD_W, LCD_H, 0, CONTENT_Y0, LCD_W, LCD_H, UI_COLOR_BG);

    if (br.page_text == NULL || br.line_count == 0) {
        const char *msg = br.page_text == NULL ? "страница не открылась" : "пустая страница";
        const char *hint = "тап по экрану - назад к результатам";
        int cw = 640, ch = 120;
        int cx = (LCD_W - cw) / 2, cy = CONTENT_Y0 + 60;
        ui_card(fb, LCD_W, LCD_H, cx, cy, cx + cw, cy + ch, false);
        ui_icon(&ui_icon_warn, fb, LCD_W, LCD_H, cx + 24, cy + 24, 1, UI_COLOR_WARN);
        ui_text(fb, LCD_W, LCD_H, cx + 56, cy + 26, msg, UI_SCALE_TEXT,
                UI_COLOR_WARN, UI_COLOR_PANEL);
        ui_text(fb, LCD_W, LCD_H, cx + 56, cy + 60, hint, 1, UI_COLOR_DIM, UI_COLOR_PANEL);
        return;
    }

    int rows = (CONTENT_Y1_FULL - CONTENT_Y0) / LINE_H;
    for (int i = 0; i < rows; i++) {
        int li = br.page_scroll + i;
        if (li >= br.line_count) {
            break;
        }
        char buf[128];
        int len = br.lines[li].len;
        if (len > (int)sizeof(buf) - 1) {
            len = (int)sizeof(buf) - 1;
        }
        memcpy(buf, br.lines[li].s, len);
        buf[len] = '\0';
        /* never cut a UTF-8 sequence at the buffer boundary */
        while (len > 0 && (((unsigned char)buf[len]) & 0xC0) == 0x80) {
            buf[--len] = '\0';
        }
        ui_text(fb, LCD_W, LCD_H, 12, CONTENT_Y0 + i * LINE_H, buf,
                UI_SCALE_TEXT, UI_COLOR_FG, UI_COLOR_BG);
    }

    /* page scroll indicator */
    draw_scrollbar(fb, CONTENT_Y0, CONTENT_Y1_FULL, br.line_count, br.page_scroll, rows);

    draw_nav_bar(fb);
}

static void draw_loading(uint16_t *fb, const char *what)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, LCD_H, UI_COLOR_BG);
    draw_status_bar(fb);

    static int dots = 0;
    dots = (dots + 1) % 4;

    int cw = 620, ch = 220;
    int cx = (LCD_W - cw) / 2, cy = (LCD_H - ch) / 2 + 16;
    ui_card(fb, LCD_W, LCD_H, cx, cy, cx + cw, cy + ch, false);

    int phase = (int)((esp_timer_get_time() / 250000LL) & 7);
    ui_spinner(fb, LCD_W, LCD_H, cx + cw / 2, cy + 52, 16, phase, UI_COLOR_ACCENT);

    char msg[96];
    snprintf(msg, sizeof(msg), "%s%s", what, "..." + (3 - dots));
    ui_text(fb, LCD_W, LCD_H, cx + (cw - ui_text_width(msg, UI_SCALE_TITLE)) / 2,
            cy + 96, msg, UI_SCALE_TITLE, UI_COLOR_FG, UI_COLOR_PANEL);

    const char *l1 = "идёт загрузка из сети";
    ui_text(fb, LCD_W, LCD_H, cx + (cw - ui_text_width(l1, 1)) / 2, cy + 140,
            l1, 1, UI_COLOR_DIM, UI_COLOR_PANEL);
    const char *l2 = "обычно 5-30 секунд, просто подождите";
    ui_text(fb, LCD_W, LCD_H, cx + (cw - ui_text_width(l2, 1)) / 2, cy + 160,
            l2, 1, UI_COLOR_BORDER, UI_COLOR_PANEL);
}

/* --- Live busy card -----------------------------------------------------------
 * Long phases (web search, page fetch, films metadata, the moov probe, and
 * the esp_player prepare) used to leave ONE static frame on the panel for
 * minutes - users read that as "the board hung". While the browser task is
 * blocked inside those calls, a small animator task (core 0, mostly asleep)
 * redraws a card every 250 ms: rotating spinner, current stage, and a
 * mm:ss counter that keeps ticking - visible proof that the board is alive
 * and it is the server that is slow. Stage text and byte counters arrive
 * via busy_stage()/busy_bytes() from the browser task (single writer, the
 * animator only reads - worst case one torn frame for 250 ms, cosmetic). */

/* s_busy itself is defined next to the busy_* declarations above
 * (video_start_ui reads s_busy.on before this point in the file). */

/* "64 кБ" / "2.9 МБ" - integer math, no %f (safe with any newlib config) */
static void busy_fmt_kb(char *out, size_t n, uint32_t b)
{
    if (b >= 1024u * 1024u) {
        uint32_t tenth = (uint32_t)(((uint64_t)b * 10u + (1024u * 1024u / 2u)) /
                                    (1024u * 1024u));
        /* uint32_t is 'unsigned long' on the riscv newlib toolchain: cast
         * explicitly so %lu is right both there and on the gcc host build */
        snprintf(out, n, "%lu.%lu МБ",
                 (unsigned long)(tenth / 10u), (unsigned long)(tenth % 10u));
    } else {
        snprintf(out, n, "%u кБ", (unsigned)((b + 1023u) / 1024u));
    }
}

static void draw_busy_frame(uint16_t *fb)
{
    ui_fill_rect(fb, LCD_W, LCD_H, 0, 0, LCD_W, LCD_H, UI_COLOR_BG);
    draw_status_bar(fb);

    const int cw = 680, ch = 260;
    int cx = (LCD_W - cw) / 2, cy = (LCD_H - ch) / 2 + 8;
    ui_card(fb, LCD_W, LCD_H, cx, cy, cx + cw, cy + ch, false);

    int phase = (int)((esp_timer_get_time() / 180000LL) & 7);
    ui_spinner(fb, LCD_W, LCD_H, cx + cw / 2, cy + 46, 15, phase, UI_COLOR_ACCENT);

    char fitted[48];
    ui_text_fit(fitted, sizeof(fitted),
                s_busy.title[0] ? s_busy.title : "ждём сеть", cw - 80,
                UI_SCALE_TEXT);
    ui_text(fb, LCD_W, LCD_H,
            cx + (cw - ui_text_width(fitted, UI_SCALE_TEXT)) / 2, cy + 78,
            fitted, UI_SCALE_TEXT, UI_COLOR_FG, UI_COLOR_PANEL);

    if (s_busy.stage[0] != '\0') {
        ui_text_fit(fitted, sizeof(fitted), s_busy.stage, cw - 80, UI_SCALE_TEXT);
        ui_text(fb, LCD_W, LCD_H,
                cx + (cw - ui_text_width(fitted, UI_SCALE_TEXT)) / 2, cy + 108,
                fitted, UI_SCALE_TEXT, UI_COLOR_ACCENT, UI_COLOR_PANEL);
    }

    /* the ticking clock: the whole point of this card */
    int64_t el = (esp_timer_get_time() - s_busy.t0) / 1000000LL;
    char line[24];
    snprintf(line, sizeof(line), "%d:%02d", (int)(el / 60), (int)(el % 60));
    ui_text(fb, LCD_W, LCD_H,
            cx + (cw - ui_text_width(line, UI_SCALE_TITLE)) / 2, cy + 146,
            line, UI_SCALE_TITLE, UI_COLOR_WARN, UI_COLOR_PANEL);

    if (s_busy.extra[0] != '\0') {
        ui_text(fb, LCD_W, LCD_H,
                cx + (cw - ui_text_width(s_busy.extra, UI_SCALE_TEXT)) / 2,
                cy + 186, s_busy.extra, UI_SCALE_TEXT, UI_COLOR_DIM,
                UI_COLOR_PANEL);
    }

#if CONFIG_EB_MEDIA_ENABLE
    /* live diagnostics under the progress line: link vs CPU at a glance
     * ("сеть 212 кБ/с  CPU0 40% CPU1 96%") - see pstats.h for the reading */
    pstats_snapshot_t sn;
    if (pstats_get(&sn)) {
        char diag[80];
        if (sn.cpu0 != PSTATS_CPU_NA) {
            snprintf(diag, sizeof(diag), "сеть %lu кБ/с   CPU0 %lu%%  CPU1 %lu%%",
                     (unsigned long)sn.net_kbps,
                     (unsigned long)sn.cpu0, (unsigned long)sn.cpu1);
        } else {
            snprintf(diag, sizeof(diag), "сеть %lu кБ/с",
                     (unsigned long)sn.net_kbps);
        }
        ui_text(fb, LCD_W, LCD_H,
                cx + (cw - ui_text_width(diag, 1)) / 2, cy + 214,
                diag, 1, UI_COLOR_BORDER, UI_COLOR_PANEL);
    }
#endif

    const char *hint = (strcmp(s_busy.stage, "готовлю плеер") == 0)
                       ? "тап по экрану - отменить"
                       : "плата работает - ждём ответ сервера";
    ui_text(fb, LCD_W, LCD_H,
            cx + (cw - ui_text_width(hint, 1)) / 2, cy + ch - 26,
            hint, 1, UI_COLOR_BORDER, UI_COLOR_PANEL);
}

#ifndef CAMOS_UI_PREVIEW
static void busy_animator(void *arg)
{
    (void)arg;
    while (!s_busy.quit) {
        void *fb = NULL;
        app_lcd_get_fb(br.fb_idx, &fb);
        if (fb != NULL) {
            draw_busy_frame((uint16_t *)fb);
            app_lcd_flush(br.fb_idx);
            br.fb_idx ^= 1;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    s_busy.dead = true;
    vTaskDelete(NULL);
}
#endif

static void busy_open(const char *title)
{
    strlcpy(s_busy.title, (title != NULL) ? title : "", sizeof(s_busy.title));
    s_busy.stage[0] = '\0';
    s_busy.extra[0] = '\0';
    s_busy.acc_bytes = 0;
    s_busy.prev_done = 0;
    s_busy.t0 = s_busy.stage_t0 = esp_timer_get_time();

    if (s_busy.on) {
        return;     /* video_start_ui reuses the film's busy window */
    }
    s_busy.on = true;
    s_busy.quit = false;
    s_busy.dead = false;
    /* paint one frame right away, BEFORE the animator exists: the card is
     * visible immediately and two tasks never paint the same window open */
    void *fb = NULL;
    app_lcd_get_fb(br.fb_idx, &fb);
    if (fb != NULL) {
        draw_busy_frame((uint16_t *)fb);
        app_lcd_flush(br.fb_idx);
        br.fb_idx ^= 1;
    }
#ifndef CAMOS_UI_PREVIEW
    if (xTaskCreatePinnedToCore(busy_animator, "busyui", 6144, NULL, 4,
                                NULL, 0) != pdPASS) {
        s_busy.on = false;  /* rare: the static frame above is the fallback */
    }
#endif
}

static void busy_stage(const char *txt)
{
    if (strncmp(s_busy.stage, txt, sizeof(s_busy.stage)) == 0) {
        return;     /* same stage: keep the rate accumulator running */
    }
    strlcpy(s_busy.stage, txt, sizeof(s_busy.stage));
    s_busy.extra[0] = '\0';
    s_busy.acc_bytes = 0;
    s_busy.prev_done = 0;
    s_busy.stage_t0 = esp_timer_get_time();
}

static void busy_bytes(uint32_t done, uint32_t total)
{
    /* bytes may restart (probe reads the head, then the moov tail):
     * accumulate everything that moved through the current stage */
    uint32_t add = (done >= s_busy.prev_done) ? done - s_busy.prev_done : done;
    s_busy.acc_bytes += add;
    s_busy.prev_done = done;

    int64_t dt = esp_timer_get_time() - s_busy.stage_t0;
    int kbps = 0;
    if (s_busy.acc_bytes > 1024 && dt > 500000) {
        kbps = (int)((uint64_t)s_busy.acc_bytes * 1000000ULL /
                     (uint64_t)dt / 1024ULL);
    }

    char got[16], tot[16];
    busy_fmt_kb(got, sizeof(got), done);
    if (total > 0 && total > done) {
        busy_fmt_kb(tot, sizeof(tot), total);
        snprintf(s_busy.extra, sizeof(s_busy.extra), "%s из %s%s%d кБ/с",
                 got, tot, kbps > 0 ? ", " : "", kbps);
    } else {
        snprintf(s_busy.extra, sizeof(s_busy.extra), "%s%s%d кБ/с",
                 got, kbps > 0 ? ", " : "", kbps);
    }
}

static void busy_close(void)
{
    if (!s_busy.on) {
        return;
    }
    s_busy.quit = true;
#ifndef CAMOS_UI_PREVIEW
    while (!s_busy.dead) {
        vTaskDelay(pdMS_TO_TICKS(20));  /* animator finishes its last frame */
    }
#endif
    s_busy.on = false;
}

/* films.c progress -> busy card */
static void films_prog_cb(void *ctx, films_prog_t st, uint32_t done, uint32_t total)
{
    (void)ctx;
    switch (st) {
    case FILMS_PROG_SEARCH:
        busy_stage("ищу в каталоге");
        break;
    case FILMS_PROG_METADATA:
        busy_stage("читаю метаданные");
        break;
    case FILMS_PROG_FILE:
        busy_stage("проверяю файл");    /* no-op when already in this stage */
        busy_bytes(done, total);
        break;
    default:
        break;
    }
}

static void draw_screen(void)
{
#if CONFIG_EB_MEDIA_ENABLE
    /* hard guard: while a video is playing the esp_video_render LCD backend
     * owns the DPI frame buffers - any UI write/flush here would corrupt
     * the playback (or crash on a torn double-buffer flip) */
    if (br.state == ST_VIDEOP && media_video_active()) {
        return;
    }
#endif

    void *fb = NULL;
    app_lcd_get_fb(br.fb_idx, &fb);
    if (fb == NULL) {
        return;
    }

    switch (br.state) {
    case ST_SPLASH:   draw_splash(fb);       break;
    case ST_HOME:     draw_home(fb);         break;
    case ST_BUSY:     draw_busy_frame(fb);   break;
    case ST_LOADING:  draw_loading(fb, "ищем");  break;
    case ST_RESULTS:  draw_results(fb);      break;
    case ST_PAGELOAD: draw_loading(fb, "открываем страницу");    break;
    case ST_PAGE:     draw_page(fb);         break;
    case ST_TOUCH_TEST: touch_calib_draw(fb); break;
#if CONFIG_EB_MEDIA_ENABLE
    case ST_RADIO:    draw_radio(fb);        break;
    case ST_VIDEO:    draw_video(fb);        break;
    case ST_URLIN:    draw_urlin(fb);        break;
    case ST_FILES:    draw_files(fb);        break;
    case ST_FILMS:    draw_films(fb);        break;
    case ST_WIFI:
#if CONFIG_EB_WIFI_ENABLE
        draw_wifi(fb);
        break;
    case ST_WIFI_PASS:
        draw_wifi_pass(fb);
        break;
    case ST_WIFI_DIAG:
        draw_wifi_diag(fb);
        break;
#endif
    case ST_FILMSR:   draw_filmsr(fb);       break;
    case ST_VIDEOP:   /* render owns the panel - nothing to draw */ break;
#endif
    }

    /* Визуальный маркер виртуального тапа (пульсирующий кружок пальца) */
    extern int s_sim_finger_x;
    extern int s_sim_finger_y;
    extern int64_t s_sim_finger_time;
    if (s_sim_finger_time > 0 && esp_timer_get_time() - s_sim_finger_time < 500000LL) {
        ui_circle(fb, LCD_W, LCD_H, s_sim_finger_x, s_sim_finger_y, 22, UI_COLOR_WARN);
        ui_circle_frame(fb, LCD_W, LCD_H, s_sim_finger_x, s_sim_finger_y, 28, UI_COLOR_OK);
        ui_circle_frame(fb, LCD_W, LCD_H, s_sim_finger_x, s_sim_finger_y, 30, UI_COLOR_FG);
    }

    app_lcd_flush(br.fb_idx);
    br.fb_idx ^= 1;
}

int s_sim_finger_x = -1;
int s_sim_finger_y = -1;
int64_t s_sim_finger_time = 0;

static void sim_user_tap(int x, int y, const char *action_desc)
{
    s_sim_finger_x = x;
    s_sim_finger_y = y;
    s_sim_finger_time = esp_timer_get_time();
    ESP_LOGI("USER_SIM", "👉 [ДЕЙСТВИЕ ПОЛЬЗОВАТЕЛЯ] %s -> Тап в (%d, %d)", action_desc, x, y);
    touch_inject(x, y);
}

/* --- Actions -------------------------------------------------------------------- */

static void do_search(void)
{
    if (br.query[0] == '\0') {
        return;
    }
    /* live busy card instead of the old static frame: the animator keeps
     * redrawing it (ticking seconds) while web_search blocks this task */
    busy_open("поиск в интернете");
    busy_stage("жду поисковик");

    char *body = NULL;
    size_t len = 0;
    esp_err_t err = web_search(br.query, br.engine_google, &body, &len);

    busy_close();

    br.result_count = 0;
    br.serp_error = true;
    if (err == ESP_OK && body != NULL) {
        int n = br.engine_google ?
                html_parse_serp_google(body, br.results, WEB_RESULTS_MAX) :
                html_parse_serp_ddg(body, br.results, WEB_RESULTS_MAX);
        if (n > 0) {
            br.result_count = n;
            br.serp_error = false;
        }
        free(body);
    } else {
        ESP_LOGW(TAG, "search failed: %s", esp_err_to_name(err));
    }

    br.state = ST_RESULTS;
    draw_screen();
}

static void open_result(int idx)
{
    if (idx < 0 || idx >= br.result_count) {
        return;
    }

    busy_open("страница");
    busy_stage("качаю html");

    char *body = NULL;
    size_t len = 0;
    esp_err_t err = web_get(br.results[idx].url, NULL, &body, &len, NULL);

    busy_close();

    page_free();
    if (err == ESP_OK && body != NULL) {
        char *text = heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM);
        if (text != NULL) {
            html_to_text(body, text, len + 1);
            br.page_text = text;
            page_wrap();
        }
        free(body);
    } else {
        ESP_LOGW(TAG, "page fetch failed: %s", esp_err_to_name(err));
    }

    br.state = ST_PAGE;
    draw_screen();
}

/* Append one key codepoint (ASCII or Cyrillic) as UTF-8 */
static void query_append_cp(uint32_t cp)
{
    char utf8[3];
    int n = kb_key_label(cp, utf8);
    size_t len = strlen(br.query);
    if (len + n < sizeof(br.query)) {
        memcpy(br.query + len, utf8, n);
        br.query[len + n] = '\0';
    }
}

/* --- Main loop ------------------------------------------------------------------- */

static const char *state_to_str(browser_state_t st)
{
    switch (st) {
    case ST_SPLASH:     return "SPLASH";
    case ST_HOME:       return "HOME";
    case ST_LOADING:    return "LOADING";
    case ST_RESULTS:    return "RESULTS";
    case ST_PAGELOAD:   return "PAGELOAD";
    case ST_PAGE:       return "PAGE";
    case ST_BUSY:       return "BUSY";
    case ST_TOUCH_TEST: return "TOUCH_TEST";
    case ST_WIFI_DIAG:  return "WIFI_DIAG";
#if CONFIG_EB_WIFI_ENABLE
    case ST_WIFI:       return "WIFI";
    case ST_WIFI_PASS:  return "WIFI_PASS";
#endif
#if CONFIG_EB_MEDIA_ENABLE
    case ST_RADIO:      return "РАДИО";
    case ST_VIDEO:      return "ВИДЕО";
    case ST_URLIN:      return "URL_INPUT";
    case ST_VIDEOP:     return "VIDEO_PLAY";
    case ST_FILES:      return "SD_FILES";
    case ST_FILMS:      return "FILMS";
    case ST_FILMSR:     return "FILMS_RESULTS";
#endif
    default:            return "UNKNOWN";
    }
}

static void browser_task(void *arg)
{
    /* Standalone boot: the LCD is already initialized, the browser owns the
     * screen from the first frame - no camera to stop, draw immediately */
    br.state = ST_SPLASH;
    draw_screen();

    touch_point_t pts[TOUCH_MAX_POINTS];
    bool prev_touch = false;
    int64_t splash_start = esp_timer_get_time();

    while (1) {
        int n = touch_poll(pts, TOUCH_MAX_POINTS);
        bool touch_now = (n > 0);
        int tap_x = (n > 0) ? pts[0].x : 0;
        int tap_y = (n > 0) ? pts[0].y : 0;
        bool tap = touch_now && !prev_touch;    /* press-and-release edge */

        int64_t t_touch_us = 0;
        if (tap) {
            t_touch_us = esp_timer_get_time();
            ESP_LOGI("UI_TOUCH", "👆 Касание: x=%d, y=%d [Экран: %s]",
                     tap_x, tap_y, state_to_str(br.state));
        }

        /* Автоматический пошаговый сценарий эмуляции действий пользователя прямо на экране */
        static int64_t s_demo_next_time = 0;
        static int s_demo_step = 0;
        static bool s_demo_enabled = true;

        /* Отключение демо при физическом касании экрана реальным пальцем */
        if (tap && (tap_x != s_sim_finger_x || tap_y != s_sim_finger_y)) {
            if (s_demo_enabled) {
                s_demo_enabled = false;
                ESP_LOGI("USER_SIM", "✋ Обнаружено физическое касание экрана - автоэмуляция отключена.");
            }
        }

        if (s_demo_enabled && br.state != ST_SPLASH) {
            int64_t now = esp_timer_get_time();
            if (s_demo_next_time == 0) {
                s_demo_next_time = now + 2500000LL; /* Старт через 2.5 сек после выхода из SPLASH */
            } else if (now >= s_demo_next_time) {
                switch (s_demo_step) {
                case 0:
                    sim_user_tap(50, 15, "Тап по плашке Wi-Fi (открытие терминала диагностики)");
                    s_demo_step = 1;
                    s_demo_next_time = now + 4000000LL; /* Показываем терминал 4 сек */
                    break;
                case 1:
                    if (br.state == ST_WIFI_DIAG) {
                        sim_user_tap(126, 558, "Тап по кнопке 'ЗАКРЫТЬ' в терминале Wi-Fi");
                        s_demo_step = 2;
                        s_demo_next_time = now + 2500000LL;
                    }
                    break;
                case 2:
                    if (br.state == ST_HOME) {
                        sim_user_tap(844, 180, "Тап по карточке 'ФИЛЬМЫ'");
                        s_demo_step = 3;
                        s_demo_next_time = now + 2500000LL;
                    }
                    break;
                case 3:
                    if (br.state == ST_FILMS) {
                        ESP_LOGI("USER_SIM", "👉 [ДЕЙСТВИЕ ПОЛЬЗОВАТЕЛЯ] Ввод буквы 'r' в строку поиска");
                        br.query[0] = 'r';
                        br.query[1] = '\0';
                        draw_screen();
                        sim_user_tap(974, 60, "Нажатие кнопки поиска 'GO' по запросу 'r'");
                        s_demo_step = 4;
                        s_demo_next_time = now + 5000000LL; /* Ожидание ответа каталога */
                    }
                    break;
                case 4:
                    if (br.state == ST_FILMSR && films_n > 0) {
                        sim_user_tap(500, 110, "Выбор первого фильма из списка результатов");
                        s_demo_step = 5;
                        s_demo_next_time = now + 7000000LL; /* Время для показа причины отказа */
                    }
                    break;
                case 5:
                    if (br.state == ST_FILMSR) {
                        sim_user_tap(80, 560, "Нажатие кнопки 'МЕНЮ' (возврат на HOME)");
                        s_demo_step = 6;
                        s_demo_next_time = now + 3000000LL;
                    }
                    break;
                default:
                    s_demo_step = 0;
                    s_demo_next_time = now + 8000000LL; /* Повтор сценария каждые 8 секунд в режиме ожидания */
                    ESP_LOGI("USER_SIM", "🔄 [ЦИКЛ] Сценарий эмуляции действий пользователя завершен, повтор через 8 сек.");
                    break;
                }
            }
        }

#if CONFIG_EB_WIFI_ENABLE
        /* Тап по статусной плашке Wi-Fi (левый верхний угол, y < BAR_H, x < 380) */
        if (tap && tap_y < BAR_H && tap_x < 380 &&
            br.state != ST_WIFI_DIAG && br.state != ST_TOUCH_TEST &&
            br.state != ST_SPLASH && br.state != ST_VIDEOP) {
            br.wifi_diag_back = br.state;
            br.state = ST_WIFI_DIAG;
            draw_screen();
            prev_touch = touch_now;
            vTaskDelay(pdMS_TO_TICKS(30));
            continue;
        }
#endif

        switch (br.state) {
        case ST_TOUCH_TEST:
            if (touch_calib_handle_input(pts, n, tap, tap_x, tap_y)) {
                br.state = ST_HOME;
                draw_screen();
            } else if (touch_now || prev_touch) {
                draw_screen();
            }
            break;

#if CONFIG_EB_WIFI_ENABLE
        case ST_WIFI_DIAG:
            if (tap) {
                wifi_diag_handle_tap(tap_x, tap_y);
                draw_screen();
            } else {
                static int64_t last_diag_update;
                int64_t now = esp_timer_get_time();
                if (now - last_diag_update > 500000LL) {
                    last_diag_update = now;
                    draw_screen();
                }
            }
            break;

        case ST_WIFI:
            if (tap) {
                wifi_handle_tap(tap_x, tap_y);
                draw_screen();
            }
            break;
        case ST_WIFI_PASS:
            /* экран пароля: клавиатура + НАЗАД + GO */
            if (tap) {
                ui_button_t bback = { 16, 300, 180, 56, "НАЗАД", BTN_HOME, 0 };
                ui_button_t bgo = { LCD_W - 200, 140, 180, 56, "GO", BTN_GO, 2 };
                if (ui_button_hit(&bback, tap_x, tap_y)) {
                    br.state = ST_WIFI;
                    draw_screen();
                } else if (ui_button_hit(&bgo, tap_x, tap_y) && wifi_pending_ssid[0]) {
                    busy_open("Wi-Fi");
                    busy_stage(wifi_pending_ssid);
                    wifi_mgr_connect(wifi_pending_ssid, br.query);
                    busy_close();
                    br.state = ST_WIFI;
                    draw_screen();
                } else {
                    uint32_t cp = 0;
                    int id = keyboard_hit(tap_x, tap_y, &cp);
                    if (id == BTN_CHAR) {
                        query_append_cp(cp);
                        draw_screen();
                    } else {
                        switch (id) {
                        case BTN_CASE:
                            br.kb_upper = !br.kb_upper;
                            draw_screen();
                            break;
                        case BTN_KBD:
                            br.kb_ru = !br.kb_ru;
                            draw_screen();
                            break;
                        case BTN_DEL: {
                            size_t len = strlen(br.query);
                            while (len > 0 &&
                                   (((unsigned char)br.query[len - 1]) & 0xC0) == 0x80) {
                                len--;
                            }
                            if (len > 0) {
                                br.query[len - 1] = '\0';
                            }
                            draw_screen();
                            break;
                        }
                        case BTN_SPC:
                            query_append_cp(' ');
                            draw_screen();
                            break;
                        case BTN_ENGINE: /* НАЗАД */
                            br.state = ST_WIFI;
                            draw_screen();
                            break;
                        case BTN_GO:
                            if (wifi_pending_ssid[0]) {
                                busy_open("Wi-Fi");
                                busy_stage(wifi_pending_ssid);
                                wifi_mgr_connect(wifi_pending_ssid, br.query);
                                busy_close();
                                br.state = ST_WIFI;
                                draw_screen();
                            }
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
            break;
#endif
        case ST_SPLASH:
            /* HOME и Wi‑Fi доступны БЕЗ Ethernet/IP — иначе нельзя открыть портал */
            if (tap || app_net_ready() ||
                (esp_timer_get_time() - splash_start > 1500000LL)) {
                br.state = ST_HOME;
                draw_screen();
            } else if (esp_timer_get_time() - splash_start > 400000LL) {
                /* периодически обновляем статус сети на splash */
                static int64_t last_redraw;
                int64_t now = esp_timer_get_time();
                if (now - last_redraw > 400000LL) {
                    last_redraw = now;
                    draw_screen();
                }
            }
            break;

        case ST_HOME:
            if (tap) {
                int app_id = BTN_NONE;
                if (urlrow_go_hit(tap_x, tap_y) && br.query[0] != '\0') {
                    do_search();
                }
#if CONFIG_EB_MEDIA_ENABLE
                else if (home_apps_hit(tap_x, tap_y, &app_id)) {
                    switch (app_id) {
#if CONFIG_EB_WIFI_ENABLE
                    case BTN_WIFI:
                        br.state = ST_WIFI;
                        draw_screen();
                        break;
#endif
                    case BTN_RADIO:
                        br.state = ST_RADIO;
                        draw_screen();
                        break;
                    case BTN_VIDEO:
                        br.state = ST_VIDEO;
                        draw_screen();
                        break;
                    case BTN_FILMS:
                        br.query[0] = '\0';
                        br.state = ST_FILMS;
                        draw_screen();
                        break;
                    case BTN_TOUCH_TEST:
                        touch_calib_reset();
                        br.state = ST_TOUCH_TEST;
                        draw_screen();
                        break;
                    default:
                        break;
                    }
                }
#endif
                else {
                    uint32_t cp = 0;
                    int id = keyboard_hit(tap_x, tap_y, &cp);
                    if (id == BTN_CHAR) {
                        query_append_cp(cp);
                        draw_screen();
                    } else {
                        switch (id) {
                        case BTN_CASE:
                            br.kb_upper = !br.kb_upper;
                            draw_screen();
                            break;
                        case BTN_KBD:
                            br.kb_ru = !br.kb_ru;
                            draw_screen();
                            break;
                        case BTN_DEL: {
                            /* strip one whole UTF-8 codepoint, not one byte */
                            size_t len = strlen(br.query);
                            while (len > 0 &&
                                   (((unsigned char)br.query[len - 1]) & 0xC0) == 0x80) {
                                len--;
                            }
                            if (len > 0) {
                                br.query[len - 1] = '\0';
                            }
                            draw_screen();
                            break;
                        }
                        case BTN_SPC:
                            query_append_cp(' ');
                            draw_screen();
                            break;
                        case BTN_ENGINE:
                            br.engine_google = !br.engine_google;
                            draw_screen();
                            break;
                        case BTN_GO:
                            do_search();
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
            break;

        case ST_RESULTS:
            if (tap) {
                int nav_id = 0;
                if (nav_hit(tap_x, tap_y, &nav_id)) {
                    if (nav_id == BTN_HOME) {
                        br.state = ST_HOME;
                        draw_screen();
                    } else if (nav_id == BTN_UP && br.page_scroll > 0) {
                        br.page_scroll--;
                        draw_screen();
                    } else if (nav_id == BTN_DOWN &&
                               br.page_scroll + (CONTENT_Y1_FULL - CONTENT_Y0) / ROW_H <
                               br.result_count) {
                        br.page_scroll++;
                        draw_screen();
                    }
                } else if (tap_y >= CONTENT_Y0 && tap_y < CONTENT_Y1_FULL) {
                    int row = (tap_y - CONTENT_Y0) / ROW_H;
                    int idx = br.page_scroll + row;
                    if (br.serp_error || idx >= br.result_count) {
                        br.state = ST_HOME;
                        draw_screen();
                    } else {
                        open_result(idx);
                    }
                }
            }
            break;

        case ST_PAGE:
            if (tap) {
                int nav_id = 0;
                if (nav_hit(tap_x, tap_y, &nav_id)) {
                    if (nav_id == BTN_HOME) {
                        page_free();
                        br.state = ST_HOME;
                        draw_screen();
                    } else if (nav_id == BTN_BACK) {
                        page_free();
                        br.state = ST_RESULTS;
                        draw_screen();
                    } else if (nav_id == BTN_UP && br.page_scroll > 0) {
                        br.page_scroll--;
                        draw_screen();
                    } else if (nav_id == BTN_DOWN &&
                               br.page_scroll + (CONTENT_Y1_FULL - CONTENT_Y0) / LINE_H <
                               br.line_count) {
                        br.page_scroll++;
                        draw_screen();
                    }
                } else if (tap_y >= CONTENT_Y0 && tap_y < CONTENT_Y1_FULL) {
                    /* page body tap = scroll down */
                    br.page_scroll += 4;
                    draw_screen();
                }
            }
            break;

#if CONFIG_EB_MEDIA_ENABLE
        case ST_RADIO:
            if (tap) {
                int id = radio_hit(tap_x, tap_y);
                if (id >= BTN_STATION && id < (int)(BTN_STATION + RADIO_STATIONS_N)) {
                    int clicked_idx = id - BTN_STATION;

                    /* Роберт Миллер (1968) / Якоб Нильсен: Пассивный отклик < 100 мс (Optimistic UI) */
                    s_radio_active_idx = clicked_idx;
                    s_radio_is_connecting = true;
                    s_radio_conn_t0 = esp_timer_get_time();

                    draw_screen();

                    /* W3C Event Timing / Google INP телеметрия задержки отклика */
                    int64_t inp_ms = (esp_timer_get_time() - t_touch_us) / 1000;
                    if (inp_ms > 100) {
                        ESP_LOGW("UI_PERF", "⚠️ INP превышен: %lld мс (лимит: 100 мс)", inp_ms);
                    } else {
                        ESP_LOGI("UI_PERF", "⚡ [INP] Тач -> карточка [%s]: %lld мс (ISO 9241-110 OK)",
                                 radio_stations[clicked_idx].name, inp_ms);
                    }

                    /* Асинхронный запуск в фоне через FreeRTOS очередь (Zero UI Wait) */
                    radio_request_start(clicked_idx, radio_stations[clicked_idx].url);
                } else {
                    switch (id) {
                    case BTN_MPAUSE:
                        if (media_get_state() == MEDIA_STATE_PAUSED) {
                            media_resume();
                        } else {
                            media_pause();
                        }
                        draw_screen();
                        break;
                    case BTN_MSTOP:
                        s_radio_active_idx = -1;
                        s_radio_is_connecting = false;
                        media_stop();
                        draw_screen();
                        break;
                    case BTN_VOLDN:
                        media_set_volume(media_get_volume() - 10);
                        draw_screen();
                        break;
                    case BTN_VOLUP:
                        media_set_volume(media_get_volume() + 10);
                        draw_screen();
                        break;
                    case BTN_MURL:
                        br.urlin_video = false;
                        br.query[0] = '\0';
                        br.state = ST_URLIN;
                        draw_screen();
                        break;
                    case BTN_SD:
                        sd_from_video = false;
                        sd_scan();
                        br.state = ST_FILES;
                        draw_screen();
                        break;
                    case BTN_MBACK:
                        br.state = ST_HOME;
                        draw_screen();
                        break;
                    default:
                        break;
                    }
                }
            }
            break;

        case ST_VIDEO:
            if (tap) {
                int id = video_hit(tap_x, tap_y);
                if (id >= BTN_VIDPRESET && id < (int)(BTN_VIDPRESET + VIDEO_PRESETS_N)) {
                    videop_back = ST_VIDEO;
                    video_start_ui(video_presets[id - BTN_VIDPRESET].url);
                } else {
                    switch (id) {
                    case BTN_MURL:
                        br.urlin_video = true;
                        br.query[0] = '\0';
                        br.state = ST_URLIN;
                        draw_screen();
                        break;
                    case BTN_SD:
                        sd_from_video = true;
                        sd_scan();
                        br.state = ST_FILES;
                        draw_screen();
                        break;
                    case BTN_MBACK:
                        br.state = ST_HOME;
                        draw_screen();
                        break;
                    default:
                        break;
                    }
                }
            }
            break;

        case ST_FILES:
            if (tap) {
                int id = files_hit(tap_x, tap_y);
                if (id == BTN_FILEROW) {
                    int row = (tap_y - SD_ROWS_Y0) / SD_ROW_STRIDE;
                    sd_play(sd_scroll + row);
                } else {
                    switch (id) {
                    case BTN_FREFR:
                        sdcard_reprobe();
                        sd_scan();
                        draw_screen();
                        break;
                    case BTN_FUP:
                        if (sd_scroll > 0) {
                            sd_scroll--;
                            draw_screen();
                        }
                        break;
                    case BTN_FDOWN:
                        if (sd_scroll + SD_ROWS < sd_files_n) {
                            sd_scroll++;
                            draw_screen();
                        }
                        break;
                    case BTN_MBACK:
                        br.state = sd_from_video ? ST_VIDEO : ST_RADIO;
                        draw_screen();
                        break;
                    default:
                        break;
                    }
                }
            }
            break;

        case ST_URLIN:
            if (tap) {
                if (urlrow_go_hit(tap_x, tap_y) && br.query[0] != '\0') {
                    urlin_apply();
                } else {
                    uint32_t cp = 0;
                    int id = keyboard_hit(tap_x, tap_y, &cp);
                    if (id == BTN_CHAR) {
                        query_append_cp(cp);
                        draw_screen();
                    } else {
                        switch (id) {
                        case BTN_ENGINE:    /* НАЗАД on this screen */
                            br.state = br.urlin_video ? ST_VIDEO : ST_RADIO;
                            draw_screen();
                            break;
                        case BTN_CASE:
                            br.kb_upper = !br.kb_upper;
                            draw_screen();
                            break;
                        case BTN_KBD:
                            br.kb_ru = !br.kb_ru;
                            draw_screen();
                            break;
                        case BTN_DEL: {
                            size_t len = strlen(br.query);
                            while (len > 0 &&
                                   (((unsigned char)br.query[len - 1]) & 0xC0) == 0x80) {
                                len--;
                            }
                            if (len > 0) {
                                br.query[len - 1] = '\0';
                            }
                            draw_screen();
                            break;
                        }
                        case BTN_SPC:
                            query_append_cp(' ');
                            draw_screen();
                            break;
                        case BTN_GO:
                            urlin_apply();
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
            break;

        case ST_VIDEOP:
            /* the render owns the panel: no UI drawing in this state */
            if (media_get_state() == MEDIA_STATE_FINISHED ||
                media_get_state() == MEDIA_STATE_ERROR ||
                media_video_active() == false) {
                media_stop();
                br.state = videop_back;
                draw_screen();
            } else if (tap) {
                media_stop();
                br.state = videop_back;
                draw_screen();
            }
            break;

        case ST_FILMS:
            if (tap) {
                uint32_t cp = 0;
                int id = urlrow_go_hit(tap_x, tap_y) ? BTN_GO
                                                     : keyboard_hit(tap_x, tap_y, &cp);
                if (id == BTN_CHAR) {
                    query_append_cp(cp);
                    draw_screen();
                } else {
                    switch (id) {
                    case BTN_GO:
                        do_films_search();   /* empty query = popular */
                        break;
                    case BTN_ENGINE:         /* НАЗАД on this screen */
                        br.state = ST_HOME;
                        draw_screen();
                        break;
                    case BTN_CASE:
                        br.kb_upper = !br.kb_upper;
                        draw_screen();
                        break;
                    case BTN_KBD:
                        br.kb_ru = !br.kb_ru;
                        draw_screen();
                        break;
                    case BTN_DEL: {
                        size_t len = strlen(br.query);
                        while (len > 0 &&
                               (((unsigned char)br.query[len - 1]) & 0xC0) == 0x80) {
                            len--;
                        }
                        if (len > 0) {
                            br.query[len - 1] = '\0';
                        }
                        draw_screen();
                        break;
                    }
                    case BTN_SPC:
                        query_append_cp(' ');
                        draw_screen();
                        break;
                    default:
                        break;
                    }
                }
            }
            break;

        case ST_FILMSR:
            if (tap) {
                int nav_id = 0;
                if (nav_hit(tap_x, tap_y, &nav_id)) {
                    if (nav_id == BTN_HOME) {
                        br.state = ST_HOME;
                        draw_screen();
                    } else if (nav_id == BTN_BACK) {
                        br.state = ST_FILMS;
                        draw_screen();
                    } else if (nav_id == BTN_UP && films_scroll > 0) {
                        films_scroll--;
                        draw_screen();
                    } else if (nav_id == BTN_DOWN &&
                               films_scroll + (CONTENT_Y1_FULL - CONTENT_Y0) / ROW_H <
                               films_n) {
                        films_scroll++;
                        draw_screen();
                    }
                } else if (tap_y >= CONTENT_Y0 && tap_y < CONTENT_Y1_FULL) {
                    int row = (tap_y - CONTENT_Y0) / ROW_H;
                    int idx = films_scroll + row;
                    if (films_n == 0 || idx >= films_n) {
                        br.state = ST_FILMS;    /* error frame: tap returns */
                        draw_screen();
                    } else {
                        open_film(idx);
                    }
                }
            }
            break;
#endif

        default:
            break;
        }

#if CONFIG_EB_MEDIA_ENABLE
        /* Якоб Нильсен: активная обратная связь при ожидании (>1 с)
         * Периодически перерисовываем экран радио для обновления секундного таймера
         * или при смене статуса плеера на PLAYING */
        static int s_radio_refresh_tick = 0;
        static media_state_t s_prev_radio_st = MEDIA_STATE_IDLE;
        if (br.state == ST_RADIO) {
            media_state_t cur_st = media_get_state();
            if (s_radio_is_connecting) {
                if (++s_radio_refresh_tick >= 16) { /* ~480 мс */
                    s_radio_refresh_tick = 0;
                    draw_screen();
                }
            }
            if (cur_st != s_prev_radio_st) {
                s_prev_radio_st = cur_st;
                if (cur_st == MEDIA_STATE_PLAYING) {
                    s_radio_is_connecting = false;
                }
                draw_screen();
            }
        }
#endif

        /* W3C Event Timing / Google INP: замер времени отклика на физическое касание */
        if (tap && t_touch_us > 0) {
            int64_t inp_ms = (esp_timer_get_time() - t_touch_us) / 1000;
            if (inp_ms > 100) {
                ESP_LOGW("UI_PERF", "⚠️ [INP] Время отклика экрана: %lld мс [Экран: %s] (>100 мс)",
                         inp_ms, state_to_str(br.state));
            } else {
                ESP_LOGI("UI_PERF", "⚡ [INP] Время отклика экрана: %lld мс [Экран: %s] (Good < 100 мс)",
                         inp_ms, state_to_str(br.state));
            }
        }

        prev_touch = touch_now;
        vTaskDelay(pdMS_TO_TICKS(br.state == ST_TOUCH_TEST ? 15 : 30));
    }
}

static void uart_cmd_task(void *arg)
{
    char buf[64];
    int idx = 0;
    ESP_LOGI("UART_HIL", "🤖 HIL-терминал готов: TAP <x> <y>, CLICK FILMS, CLICK WIFI, CLICK RADIO, CLICK HOME, STATUS");

    while (1) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (idx == 0) {
                continue;
            }
            buf[idx] = '\0';
            idx = 0;

            int tx = 0, ty = 0;
            if (sscanf(buf, "TAP %d %d", &tx, &ty) == 2) {
                ESP_LOGI("UART_HIL", "⚡ Команда HIL: TAP %d %d", tx, ty);
                touch_inject(tx, ty);
            } else if (strcmp(buf, "STATUS") == 0) {
                ESP_LOGI("UART_HIL", "📊 STATUS: screen=%s wifi_state=%s wifi_ready=%d ssid=\"%s\" ip=\"%s\"",
                         state_to_str(br.state), wifi_mgr_status_str(), (int)wifi_mgr_ready(),
                         wifi_mgr_saved_ssid(), wifi_mgr_ip_str());
            } else if (strcmp(buf, "CLICK FILMS") == 0) {
                ESP_LOGI("UART_HIL", "⚡ Команда HIL: Клик 'ФИЛЬМЫ'");
                touch_inject(844, 180);
            } else if (strcmp(buf, "CLICK WIFI") == 0) {
                ESP_LOGI("UART_HIL", "⚡ Команда HIL: Клик значок Wi-Fi");
                touch_inject(50, 15);
            } else if (strcmp(buf, "CLICK RADIO") == 0) {
                ESP_LOGI("UART_HIL", "⚡ Команда HIL: Клик 'РАДИО'");
                touch_inject(180, 180);
            } else if (strcmp(buf, "CLICK HOME") == 0) {
                ESP_LOGI("UART_HIL", "⚡ Команда HIL: Клик 'ДОМОЙ'");
                touch_inject(980, 16);
            } else if (strcmp(buf, "HELP") == 0) {
                ESP_LOGI("UART_HIL", "Команды: TAP <x> <y>, CLICK FILMS, CLICK WIFI, CLICK RADIO, CLICK HOME, STATUS");
            } else {
                ESP_LOGW("UART_HIL", "Неизвестная команда: '%s' (введите HELP)", buf);
            }
        } else if (idx < (int)sizeof(buf) - 1) {
            buf[idx++] = (char)c;
        }
    }
}

void browser_start(void)
{
    memset(&br, 0, sizeof(br));
    br.fb_idx = 1;  /* draw on the second fb, flush the first via app_lcd_flush */
#if CONFIG_EB_ENGINE_GOOGLE
    br.engine_google = true;
#endif
#if CONFIG_EB_MEDIA_ENABLE
    /* films.c stage/byte reports drive the live busy card */
    films_set_prog_cb(films_prog_cb, NULL);
    /* playback/net diagnostics (busy card line + video OSD); the sampler
     * self-attaches to the NIC once it is up, so start it right away -
     * probe/PREPARING phases need it too, before media_player_init runs */
    pstats_init();
    radio_worker_init_once();
#endif

    esp_err_t eth_err = app_net_start();
    if (eth_err != ESP_OK) {
        ESP_LOGE(TAG, "net start failed: %s", esp_err_to_name(eth_err));
    }
    esp_err_t tp_err = touch_init();
    if (tp_err != ESP_OK) {
        ESP_LOGW(TAG, "touch not found (%s) - browser will be view-only",
                 esp_err_to_name(tp_err));
    }

    if (xTaskCreatePinnedToCore(browser_task, "cambrowser", 16 * 1024, NULL,
                                3, NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "failed to create browser task");
    }

    /* Фоновый приемник UART HIL команд для автотестов */
    xTaskCreatePinnedToCore(uart_cmd_task, "uart_hil", 4096, NULL, 2, NULL, 0);
}


/* --- Host-only preview hook -------------------------------------------------
 * Compiled out of the firmware build (-UCAMOS_UI_PREVIEW is the default).
 * scripts/uihost uses it to render the actual browser drawing code into
 * BMP files so UI changes can be reviewed on a PC without hardware. */
#ifdef CAMOS_UI_PREVIEW
void ui_preview_set(int st, const char *q, int eng, int scroll,
                    const film_item_t *films, int nf, const char *msg,
                    int nres, const char *const *res_titles);
void ui_preview_set(int st, const char *q, int eng, int scroll,
                    const film_item_t *films, int nf, const char *msg,
                    int nres, const char *const *res_titles)
{
    memset(&br, 0, sizeof(br));
    br.fb_idx = 0;
    br.state = (browser_state_t)st;
    if (q != NULL) {
        strlcpy(br.query, q, sizeof(br.query));
    }
    br.engine_google = eng;
    br.page_scroll = scroll;
#if CONFIG_EB_MEDIA_ENABLE
    films_scroll = scroll;
    films_n = nf;
    films_msg[0] = '\0';
    if (msg != NULL) {
        strlcpy(films_msg, msg, sizeof(films_msg));
    }
    if (films != NULL) {
        for (int i = 0; i < nf && i < FILMS_MAX; i++) {
            film_items[i] = films[i];
        }
    }
#endif
    br.result_count = nres;
    if (res_titles != NULL) {
        for (int i = 0; i < nres && i < WEB_RESULTS_MAX; i++) {
            strlcpy(br.results[i].title, res_titles[i], WEB_RESULT_TITLE_MAX);
            strlcpy(br.results[i].url, "https://example.com/page", WEB_RESULT_URL_MAX);
        }
    }
    draw_screen();
}

/* Host-only: render ST_PAGE with the given plain text */
void ui_preview_page(const char *text)
{
    memset(&br, 0, sizeof(br));
    br.fb_idx = 0;
    br.page_text = strdup(text);
    page_wrap();
    br.state = ST_PAGE;
    draw_screen();
}

/* Host-only: render ST_FILES with the given names (video != 0 => video) */
void ui_preview_sd(const char *const *names, const int *video, int n, int err)
{
    memset(&br, 0, sizeof(br));
    br.fb_idx = 0;
    sd_files_n = 0;
    sd_mount_err = err;
    sd_scroll = 0;
    if (!err && names != NULL) {
        for (int i = 0; i < n && i < SD_FILES_MAX; i++) {
            strlcpy(sd_files[sd_files_n].name, names[i], SD_NAME_MAX);
            sd_files[sd_files_n].video = (video[i] != 0);
            sd_files_n++;
        }
    }
    br.state = ST_FILES;
    draw_screen();
}

/* Host-only: render the live busy card with the given stage/extra/elapsed */
void ui_preview_busy(const char *title, const char *stage,
                     const char *extra, int elapsed_s)
{
    memset(&br, 0, sizeof(br));
    br.fb_idx = 0;
    br.state = ST_BUSY;
    strlcpy(s_busy.title, (title != NULL) ? title : "", sizeof(s_busy.title));
    strlcpy(s_busy.stage, (stage != NULL) ? stage : "", sizeof(s_busy.stage));
    strlcpy(s_busy.extra, (extra != NULL) ? extra : "", sizeof(s_busy.extra));
    s_busy.t0 = esp_timer_get_time() - (int64_t)elapsed_s * 1000000LL;
    draw_screen();
}
#endif /* CAMOS_UI_PREVIEW */