// =========================================================================
//  Мост между пультом, сгенерированным EEZ Studio (main/eez_ui/screens.c),
//  и движком Modbus (vfd_easydrive.c).
//
//  EEZ владеет разметкой и виджетами, прошивка — значениями: из tick он
//  трогает только get_var_*/set_var_*. Контракт описан в
//  Downloads/eez-winder-hmi/README.md, здесь держатся его правила:
//   - строковый геттер не возвращает NULL: генератор делает strcmp(new, cur);
//   - целые геттеры отдают тень последнего set_var_*, а не регистр: у
//     tick_screen_params нет проверки LV_STATE_EDITED, значение из привода
//     отвоевывало бы спинбокт у оператора;
//   - блокирующие vfd_read_regs()/vfd_write_reg() живут только в bus_worker,
//     LVGL-контекст ходит в шину неблокирующими vfd_cmd_*.
//
//  В спинбоктах лежат СЫРЫЕ регистры: частоты в 0.01 Гц, ramp в 0.1 с.
// =========================================================================

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "sdkconfig.h"

#include "ui.h"
#include "screens.h"
#include "vars.h"
#include "actions.h"
#include "eez_ui.h"
#include "vfd_easydrive.h"

static const char *TAG = "EEZ_UI";

#define PARAM_COUNT 7
#define JOG_PARAM   6      // P2.19 = частота толчка
#define STEP_HZ     100    // шаг "-"/"+" = 1.00 Гц при шкале слайдера 0.01 Гц
#define SP_MAX_RAW  (CONFIG_STANOK_MAX_FREQ_HZ * 100)

// Порядок обязан совпадать с порядком строк на экране ПАРАМЕТРЫ.
static const uint16_t PARAM_REG[PARAM_COUNT] = {
    0x0001, 0x0006, 0x0007, 0x0008, 0x000E, 0x000F, 0x0213,
};
// Знаков после точки и диапазон — как в WinderHMI.eez-project (gen_eez.py).
static const uint8_t PARAM_DEC[PARAM_COUNT] = { 0, 2, 2, 2, 1, 1, 2 };
static const int32_t PARAM_MIN[PARAM_COUNT] = { 0, 0, 0, 0, 1, 1, 0 };
static const int32_t PARAM_MAX[PARAM_COUNT] = { 8, 40000, 40000, 40000, 36000, 36000, 5000 };

static const char *const FAULT_NAME[20] = {
    "Перегрузка по току на разгоне",
    "Перегрузка по току на торможении",
    "Перегрузка по току на постоянной скорости",
    "Перенапряжение на разгоне",
    "Перенапряжение на торможении",
    "Перенапряжение на постоянной скорости",
    "Перенапряжение при останове",
    "Недостаточное напряжение в работе",
    "Пропал входной фазный",
    "Неисправность модуля",
    "Перегрев радиатора",
    "Перегрузка ЧП",
    "Перегрузка двигателя",
    "Авария по внешнему оборудованию",
    "Авария связи RS-485",
    "резерв",
    "Неверное измерение тока",
    "Нет связи клавиатура-плата",
    "Неисправность CPU",
    "Перегрев радиатора",
};

static const char *const FAULT_HINT[20] = {
    "увеличить время разгона, снизить инерцию нагрузки",
    "увеличить время торможения, возможен тормозной резистор",
    "проверить питание и броски нагрузки",
    "проверить питание, подхват по скорости",
    "увеличить время торможения, тормозной резистор",
    "проверить питание, снизить инерцию",
    "проверить напряжение сети",
    "проверить напряжение сети",
    "проверить входные клеммы и сеть",
    "проверить выход на КЗ/землю, продуть канал, вентилятор",
    "охладить помещение, проверить вентилятор и каналы",
    "уменьшить подхват момента, увеличить время разгона",
    "проверить нагрузку, напряжение сети, коэффициенты P9",
    "разомкнуть входной терминал аварии",
    "P8.01, кабель и терминалы 485, жив ли хост",
    "-",
    "сервис: датчик тока и цепь измерения",
    "переподключить шлейф клавиатуры",
    "сервис",
    "охладить помещение, проверить вентилятор и каналы",
};

// ---------------------------------------------------------------- тени
static int32_t s_sp_raw;
static bool s_sp_touched;   // оператор уже двигал слайдер/кнопки -> уставку не подсаживать
static int32_t s_par[PARAM_COUNT];
static char s_msg[96] = "Не прочитано";
static char s_jrn[3][96];
static char s_snap[6][96];
static float s_jog_hz = 5.0f;
static int64_t s_fault_seen_us;
static uint8_t s_fault_latched;
static QueueHandle_t s_job_q;

// ---------------------------------------------------------------- задачи
typedef enum {
    JOB_PARAMS_READ,
    JOB_PARAMS_WRITE,
    JOB_JOURNAL_READ,
    JOB_COUNTERS_RESET,
} job_t;

static void set_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_msg, sizeof(s_msg), fmt, ap);
    va_end(ap);
}

static void post_job(job_t job)
{
    if (s_job_q && xQueueSend(s_job_q, &job, 0) != pdTRUE) {
        ESP_LOGW(TAG, "очередь шины занята, задание %d отброшено", job);
    }
}

// EEZ-спинбокт всегда дописывает ведущие нули до digit_count: 0.1 с выглядит
// как "0000.1", и оператор читает это как 0,0001. Разрез подгоняем под
// фактическую длину числа. set_digit_format() молча срезает range_max до
// 10^digit_count, поэтому диапазон возвращаем сразу следом.
//
// Значение толкаем в виджет именно здесь, а не полагаемся на тик EEZ. Спинбокт
// в LVGL 9 - потомок textarea, и иная его перерисовка (формат, диапазон)
// вызывает lv_textarea_set_text() -> LV_EVENT_VALUE_CHANGED -> set_var_par_*
// читает виджет обратно. Если бы тень s_par[] мы заполняли до этого вызова,
// обработчик затёр бы её старым нулём и тик стал бы писать 0 вместо числа:
// "Прочитано 7/7" при пустых полях. set_value() ставит число до read-back.
static void spinbox_fit(int i, int32_t raw)
{
    lv_obj_t *all[PARAM_COUNT] = { objects.spn_channel, objects.spn_fmax,
                                   objects.spn_fmin, objects.spn_fbase,
                                   objects.spn_accel, objects.spn_decel,
                                   objects.spn_jog };
    if (i < 0 || i >= PARAM_COUNT) return;
    lv_obj_t *sb = all[i];
    if (!sb) return;
    const uint8_t dec = PARAM_DEC[i];
    char digits[16];
    snprintf(digits, sizeof(digits), "%d", (int)(raw < 0 ? -raw : raw));
    int32_t int_digits = (int32_t)strlen(digits) - (int32_t)dec;
    if (int_digits < 1) int_digits = 1;
    const uint32_t want = (uint32_t)(int_digits + (int32_t)dec);
    if (lv_spinbox_get_digit_count(sb) != want) lv_spinbox_set_digit_format(sb, want, dec);
    lv_spinbox_set_range(sb, PARAM_MIN[i], PARAM_MAX[i]);
    lv_spinbox_set_value(sb, raw);
    s_par[i] = lv_spinbox_get_value(sb);
}

static void bus_worker(void *arg)
{
    (void)arg;
    job_t job;

    for (;;) {
        if (xQueueReceive(s_job_q, &job, portMAX_DELAY) != pdTRUE) continue;

        switch (job) {
        case JOB_PARAMS_READ: {
            uint16_t raw[PARAM_COUNT];
            int ok = 0;
            for (int i = 0; i < PARAM_COUNT; i++) {
                raw[i] = 0;
                if (vfd_read_regs(PARAM_REG[i], &raw[i], 1)) ok++;
            }
            if (ok > 0) s_jog_hz = (float)raw[JOG_PARAM] / 100.0f;
            // P0.14/P0.15 хранят 0.1 доли ВЫБРАННОЙ ЕДИНИЦЫ, а единицу задаёт
            // P3.09 (0 = секунды, 1 = минуты, завод — минуты). Без подписи
            // "10.4" на панели читается как 10.4 с, хотя привод понимает 10.4 мин.
            uint16_t time_unit = 0;
            bool unit_ok = vfd_read_regs(0x0309, &time_unit, 1);
            const char *unit_txt = !unit_ok ? "?" : (time_unit ? "мин" : "с");
            ESP_LOGI(TAG, "raw из привода: P0.01=%u P0.06=%u(0.01Гц) P0.07=%u(0.01Гц) "
                          "P0.08=%u(0.01Гц) P0.14=%u P0.15=%u (0.1 %s, P3.09=%u) P2.19=%u(0.01Гц)",
                     raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], unit_txt,
                     time_unit, raw[6]);
            if (lvgl_port_lock(0)) {
                for (int i = 0; i < PARAM_COUNT; i++) {
                    // Таймаут по P2.19 неотличим от честного нуля: толчок не
                    // трогаем, чтобы не записать в привод 0 вместо его значения.
                    if (raw[i] || i != JOG_PARAM) spinbox_fit(i, raw[i]);
                }
                ESP_LOGI(TAG, "тени после отрисовки: %d %d %d %d %d %d %d",
                         (int)s_par[0], (int)s_par[1], (int)s_par[2], (int)s_par[3],
                         (int)s_par[4], (int)s_par[5], (int)s_par[6]);
                if (ok == PARAM_COUNT)
                    set_msg("Прочитано %d/%d · разгон/торможение в %s", ok, PARAM_COUNT, unit_txt);
                else
                    set_msg("Отвечено %d из %d · время в %s", ok, PARAM_COUNT, unit_txt);
                lvgl_port_unlock();
            }
            break;
        }

        case JOB_PARAMS_WRITE: {
            uint16_t want[PARAM_COUNT];
            // Тени уже то, что набрал оператор: set_var_par_* вызывается из
            // встроенного обработчика EEZ прямо в контексте LVGL.
            for (int i = 0; i < PARAM_COUNT; i++) want[i] = (uint16_t)s_par[i];
            // Эхо записи подтверждает только приём кадра, поэтому каждую
            // величину читаем обратно.
            int confirmed = 0;
            for (int i = 0; i < PARAM_COUNT; i++) {
                uint16_t back = 0xFFFF;
                if (vfd_write_reg(PARAM_REG[i], want[i]) &&
                    vfd_read_regs(PARAM_REG[i], &back, 1) && back == want[i]) {
                    confirmed++;
                    s_par[i] = back;
                }
            }
            if (lvgl_port_lock(0)) {
                set_msg("Подтверждено чтением %d/%d", confirmed, PARAM_COUNT);
                lv_obj_set_style_text_color(objects.lbl_p_msg,
                    lv_palette_main(confirmed == PARAM_COUNT ? LV_PALETTE_GREEN
                                                             : LV_PALETTE_ORANGE), 0);
                lvgl_port_unlock();
            }
            break;
        }

        case JOB_JOURNAL_READ: {
            uint16_t code[3] = {0}, snap[6] = {0};
            bool codes_ok = vfd_read_regs(0x0D13, code, 3);
            bool snap_ok = vfd_read_regs(0x0D16, snap, 6);
            for (int i = 0; i < 3; i++) {
                uint8_t c = (uint8_t)(code[i] & 0x7F);
                if (!codes_ok) {
                    snprintf(s_jrn[i], sizeof(s_jrn[i]), "d-%d   нет связи", 19 + i);
                } else if (c == 0 || c >= 20) {
                    snprintf(s_jrn[i], sizeof(s_jrn[i]), "d-%d   пусто", 19 + i);
                } else {
                    snprintf(s_jrn[i], sizeof(s_jrn[i]), "d-%d  Er%02d  %s",
                             19 + i, c, FAULT_NAME[c]);
                }
            }
            if (!snap_ok) {
                for (int i = 0; i < 6; i++) {
                    snprintf(s_snap[i], sizeof(s_snap[i]), "d-%d  нет связи", 22 + i);
                }
            } else {
                // d-22..d-27 повторяют d-00/d-01/d-02/d-03/d-07/d-16
                snprintf(s_snap[0], sizeof(s_snap[0]), "d-22 частота   %.2f Гц", snap[0] / 100.0);
                snprintf(s_snap[1], sizeof(s_snap[1]), "d-23 уставка   %.2f Гц", snap[1] / 100.0);
                snprintf(s_snap[2], sizeof(s_snap[2]), "d-24 ток       %.2f А", snap[2] / 100.0);
                snprintf(s_snap[3], sizeof(s_snap[3]), "d-25 U выхода  %d В", snap[3]);
                snprintf(s_snap[4], sizeof(s_snap[4]), "d-26 звено DC  %d В", snap[4]);
                snprintf(s_snap[5], sizeof(s_snap[5]), "d-27 IGBT      %.1f C", snap[5] / 10.0);
            }
            break;
        }

        case JOB_COUNTERS_RESET:
            g_vfd.polls_ok = 0;
            g_vfd.timeouts = 0;
            g_vfd.crc_err = 0;
            g_vfd.retries = 0;
            g_vfd.cmd_ok = 0;
            g_vfd.cmd_err = 0;
            g_vfd.echo_bytes = 0;
            break;
        }
    }
}

// ---------------------------------------------------------------- тексты
static char *snr(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return buf;
}

static const char *drive_state_text(void)
{
    if (g_vfd.link == VFD_LINK_DEAD) return "ПРИВОД НЕ ОТВЕЧАЕТ";
    if (g_vfd.link_stale) return "НЕТ СВЯЗИ";
    if (g_vfd.is_fault) return "АВАРИЯ";
    if (g_vfd.is_running) {
        if (g_vfd.status_word & VFD_ST_DECELERATING) return "ТОРМОЖЕНИЕ";
        if (g_vfd.status_word & VFD_ST_ACCELERATING) return "РАЗГОН";
        return (g_vfd.status_word & VFD_ST_DIR_REV) ? "РАБОТА НАЗАД" : "РАБОТА ВПЕРЁД";
    }
    return "ОСТАНОВЛЕН";
}

static uint32_t drive_state_color(void)
{
    if (g_vfd.link == VFD_LINK_DEAD) return 0x9E9E9E;
    if (g_vfd.link_stale || g_vfd.is_fault) return 0xE53935;
    if (g_vfd.is_running) return 0x43A047;
    return 0xFFB300;
}

static const char *fault_reason_text(void)
{
    switch (g_vfd.dbg_reason) {
    case 1: return "нет ответа";
    case 2: return "чужой адрес";
    case 3: return "ошибка CRC";
    case 4: return "функция/длина";
    default: return "обмен штатный";
    }
}

// ---------------------------------------------------------------- геттеры
int32_t get_var_sp_raw(void) { return s_sp_raw; }
void set_var_sp_raw(int32_t value)
{
    if (value < 0) value = 0;
    if (value > SP_MAX_RAW) value = SP_MAX_RAW;
    s_sp_raw = value;
    s_sp_touched = true;
}

#define PAR_ACCESSORS(NAME, SLOT)                       \
    int32_t get_var_##NAME(void) { return s_par[SLOT]; } \
    void set_var_##NAME(int32_t value) { s_par[SLOT] = value; }

PAR_ACCESSORS(par_channel, 0)
PAR_ACCESSORS(par_fmax, 1)
PAR_ACCESSORS(par_fmin, 2)
PAR_ACCESSORS(par_fbase, 3)
PAR_ACCESSORS(par_accel, 4)
PAR_ACCESSORS(par_decel, 5)
PAR_ACCESSORS(par_jog, 6)

const char *get_var_freq_txt(void)
{
    static char b[32];
    return snr(b, sizeof(b), "%.2f", (double)g_vfd.out_freq_hz);
}

const char *get_var_cur_txt(void)
{
    static char b[32];
    return snr(b, sizeof(b), "%.2f", (double)g_vfd.out_cur_a);
}

const char *get_var_volt_txt(void)
{
    static char b[32];
    return snr(b, sizeof(b), "%.0f", (double)g_vfd.out_volt_v);
}

const char *get_var_rpm_txt(void)
{
    static char b[32];
    return snr(b, sizeof(b), "%ld", (long)g_vfd.rpm);
}

const char *get_var_sp_txt(void)
{
    static char b[32];
    return snr(b, sizeof(b), "%.2f Гц", s_sp_raw / 100.0);
}

const char *get_var_state_txt(void)
{
    static char b[48];
    if (g_vfd.is_fault && !g_vfd.link_stale && g_vfd.link != VFD_LINK_DEAD) {
        return snr(b, sizeof(b), "АВАРИЯ Er%02u", g_vfd.fault_code);
    }
    return snr(b, sizeof(b), "%s", drive_state_text());
}

const char *get_var_link_txt(void)
{
    static char b[64];
    switch (g_vfd.link) {
    case VFD_LINK_OK:
        return snr(b, sizeof(b), "СВЯЗЬ OK  %u мс", g_vfd.latency_ms);
    case VFD_LINK_DEGRADED:
        return snr(b, sizeof(b), "СВЯЗЬ С РЕТРАЯМИ  crc %lu", (unsigned long)g_vfd.crc_err);
    default:
        return snr(b, sizeof(b), "СВЯЗЬ: НЕТ ОТВЕТОВ");
    }
}

const char *get_var_p_msg_txt(void) { return s_msg; }

const char *get_var_fault_code_txt(void)
{
    static char b[16];
    return snr(b, sizeof(b), g_vfd.is_fault ? "Er%02u" : "Er--", g_vfd.fault_code);
}

const char *get_var_fault_name_txt(void)
{
    static char b[96];
    if (!g_vfd.is_fault) return snr(b, sizeof(b), "Нет активной аварии");
    return snr(b, sizeof(b), "%s",
               g_vfd.fault_code < 20 ? FAULT_NAME[g_vfd.fault_code] : "неизвестный код");
}

const char *get_var_fault_desc_txt(void)
{
    static char b[128];
    if (!g_vfd.is_fault) return snr(b, sizeof(b), "d-19..d-21 читаются кнопкой ниже");
    return snr(b, sizeof(b), "%s",
               g_vfd.fault_code < 20 ? FAULT_HINT[g_vfd.fault_code] : "-");
}

const char *get_var_fault_time_txt(void)
{
    static char b[48];
    if (g_vfd.is_fault) {
        if (g_vfd.fault_code != s_fault_latched) {
            s_fault_latched = g_vfd.fault_code;
            s_fault_seen_us = esp_timer_get_time();
        }
        return snr(b, sizeof(b), "идёт %.1f с",
                   (esp_timer_get_time() - s_fault_seen_us) / 1e6);
    }
    s_fault_latched = 0;
    return snr(b, sizeof(b), "");
}

const char *get_var_jrn0_txt(void) { return s_jrn[0]; }
const char *get_var_jrn1_txt(void) { return s_jrn[1]; }
const char *get_var_jrn2_txt(void) { return s_jrn[2]; }
const char *get_var_snap0_txt(void) { return s_snap[0]; }
const char *get_var_snap1_txt(void) { return s_snap[1]; }
const char *get_var_snap2_txt(void) { return s_snap[2]; }
const char *get_var_snap3_txt(void) { return s_snap[3]; }
const char *get_var_snap4_txt(void) { return s_snap[4]; }
const char *get_var_snap5_txt(void) { return s_snap[5]; }

const char *get_var_c_state_txt(void)
{
    static char b[48];
    return snr(b, sizeof(b), g_vfd.link == VFD_LINK_OK ? "СВЯЗЬ УСТАНОВЛЕНА" :
                             (g_vfd.link == VFD_LINK_DEGRADED ? "СВЯЗЬ С ОШИБКАМИ"
                                                              : "СВЯЗЬ НЕТ"));
}

const char *get_var_c_cfg1_txt(void)
{
    static char b[96];
    return snr(b, sizeof(b), "UART%d  TX=GPIO%d RX=GPIO%d  DE авто",
               CONFIG_STANOK_RS485_UART_NUM, g_vfd.uart_tx_gpio, g_vfd.uart_rx_gpio);
}

const char *get_var_c_cfg2_txt(void)
{
    static char b[96];
    return snr(b, sizeof(b), "%d 8E1  адрес %d  опрос %d мс  таймаут %d мс",
               CONFIG_STANOK_RS485_BAUD, CONFIG_STANOK_VFD_ADDR,
               CONFIG_STANOK_POLL_MS, CONFIG_STANOK_RESP_TIMEOUT_MS);
}

const char *get_var_c_cnt1_txt(void)
{
    static char b[96];
    return snr(b, sizeof(b), "опр %lu  ретр %lu  тайм %lu  crc %lu",
               (unsigned long)g_vfd.polls_ok, (unsigned long)g_vfd.retries,
               (unsigned long)g_vfd.timeouts, (unsigned long)g_vfd.crc_err);
}

const char *get_var_c_cnt2_txt(void)
{
    static char b[96];
    return snr(b, sizeof(b), "команд %lu  ошибок %lu  латент %u мс  эхо %u Б",
               (unsigned long)g_vfd.cmd_ok, (unsigned long)g_vfd.cmd_err,
               g_vfd.latency_ms, g_vfd.echo_bytes);
}

const char *get_var_c_cmd1_txt(void)
{
    static char b[96];
    return snr(b, sizeof(b), "0x2000 = %04X  %s", g_vfd.last_cmd,
               g_vfd.last_cmd_acked
                   ? (g_vfd.last_cmd_confirmed ? "подтверждено" : "эхо без подтверждения")
                   : "без ответа");
}

const char *get_var_c_cmd2_txt(void)
{
    static char b[96];
    return snr(b, sizeof(b), "чтение обратно: 0x%04X  %s", g_vfd.cmd_readback,
               g_vfd.cmd_readback_ok ? "совпало" : "НЕ совпало");
}

const char *get_var_c_err_txt(void)
{
    static char b[96];
    return snr(b, sizeof(b), "%s  FC%02X  принято %u / ждали %u",
               fault_reason_text(), g_vfd.dbg_func, g_vfd.dbg_got, g_vfd.dbg_exp);
}

const char *get_var_c_frame_txt(void)
{
    static char b[128];
    return snr(b, sizeof(b), "кадр: %s", g_vfd.dbg_frame[0] ? g_vfd.dbg_frame : "-");
}

// ---------------------------------------------------------------- действия
void action_nav_main(lv_event_t *e)  { (void)e; loadScreen(SCREEN_ID_MAIN); }
void action_nav_params(lv_event_t *e){ (void)e; loadScreen(SCREEN_ID_PARAMS); }
void action_nav_alarm(lv_event_t *e) { (void)e; loadScreen(SCREEN_ID_ALARM); }
void action_nav_conn(lv_event_t *e)  { (void)e; loadScreen(SCREEN_ID_CONN); }

void action_btn_start(lv_event_t *e)
{
    (void)e;
    vfd_cmd_run_fwd(s_sp_raw / 100.0f);
}

void action_btn_stop(lv_event_t *e)
{
    (void)e;
    vfd_cmd_stop();
}

void action_btn_rev(lv_event_t *e)
{
    (void)e;
    vfd_cmd_run_rev(s_sp_raw / 100.0f);
}

void action_btn_jog(lv_event_t *e)
{
    (void)e;
    vfd_cmd_jog(false, s_jog_hz);
}

void action_btn_fault_reset(lv_event_t *e)
{
    (void)e;
    vfd_cmd_reset_fault();
}

void action_fault_reset(lv_event_t *e)
{
    (void)e;
    vfd_cmd_reset_fault();
}

static void sp_step(int32_t step)
{
    int32_t v = s_sp_raw + step;
    if (v < 0) v = 0;
    if (v > SP_MAX_RAW) v = SP_MAX_RAW;
    s_sp_raw = v;
    s_sp_touched = true;
    vfd_cmd_set_frequency(v / 100.0f);
}

void action_sp_up(lv_event_t *e) { (void)e; sp_step(STEP_HZ); }
void action_sp_dn(lv_event_t *e) { (void)e; sp_step(-STEP_HZ); }

void action_par_read(lv_event_t *e)
{
    (void)e;
    set_msg("Читаю %d регистра...", PARAM_COUNT);
    post_job(JOB_PARAMS_READ);
}

void action_par_write(lv_event_t *e)
{
    (void)e;
    set_msg("Пишу и читаю обратно...");
    post_job(JOB_PARAMS_WRITE);
}

void action_journal_read(lv_event_t *e)
{
    (void)e;
    post_job(JOB_JOURNAL_READ);
}

void action_counters_reset(lv_event_t *e)
{
    (void)e;
    post_job(JOB_COUNTERS_RESET);
}

// ---------------------------------------------------------------- обвязка
// VALUE_CHANGED генератор отдаёт в set_var_sp_raw на каждое движение
// перетаскивания, а кадр 0x2001 нужен один раз на отпускании.
static void on_sp_released(lv_event_t *e)
{
    (void)e;
    vfd_cmd_set_frequency(s_sp_raw / 100.0f);
}

static void paint_colors(void)
{
    lv_obj_set_style_text_color(objects.lbl_state, lv_color_hex(drive_state_color()), 0);
    lv_obj_set_style_text_color(objects.lbl_link, lv_color_hex(
        g_vfd.link == VFD_LINK_OK ? 0x43A047 :
        (g_vfd.link == VFD_LINK_DEGRADED ? 0xFFB300 : 0xE53935)), 0);
    lv_obj_set_style_text_color(objects.lbl_c_state, lv_color_hex(
        g_vfd.link == VFD_LINK_OK ? 0x43A047 :
        (g_vfd.link == VFD_LINK_DEGRADED ? 0xFFB300 : 0xE53935)), 0);
}

static void on_ui_timer(lv_timer_t *t)
{
    (void)t;
    // Слайдер стартует с нуля, а привод помнит свою уставку: без этого переноса
    // ГЛАВНАЯ показывает 0.00 Гц там, где в 0x2001 лежит 28.06.
    if (!s_sp_touched && g_vfd.link == VFD_LINK_OK) {
        int32_t raw = (int32_t)(g_vfd.set_freq_hz * 100.0 + 0.5);
        if (raw >= 0 && raw <= SP_MAX_RAW) {
            s_sp_raw = raw;
            s_sp_touched = true;
        }
    }
    ui_tick();
    paint_colors();
}

void eez_ui_init(void)
{
    ui_init();

    for (int i = 0; i < 3; i++) snprintf(s_jrn[i], sizeof(s_jrn[i]), "d-%d  -", 19 + i);
    for (int i = 0; i < 6; i++) snprintf(s_snap[i], sizeof(s_snap[i]), "d-%d  -", 22 + i);

    s_job_q = xQueueCreate(4, sizeof(job_t));
    if (s_job_q == NULL ||
        xTaskCreate(bus_worker, "ui_bus", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "не удалось создать задачу обслуживания шины");
        return;
    }

    lv_obj_add_event_cb(objects.sld_sp, on_sp_released, LV_EVENT_RELEASED, NULL);
    lv_timer_create(on_ui_timer, 100, NULL);

    // До первого чтения строки показывают прочерки, а спинбокт - "0000.1".
    for (int i = 0; i < PARAM_COUNT; i++) spinbox_fit(i, s_par[i]);
    post_job(JOB_PARAMS_READ);

    ESP_LOGI(TAG, "панель EEZ: 4 экрана, параметры 0x%04X..0x%04X, уставка %d шагов",
             PARAM_REG[0], PARAM_REG[PARAM_COUNT - 1], SP_MAX_RAW);
}
