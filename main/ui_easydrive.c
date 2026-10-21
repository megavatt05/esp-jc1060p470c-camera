// =========================================================================
//  Обвязка сгенерированного SquareLine Studio интерфейса EASYDRIVE с движком
//  Modbus RTU (vfd_easydrive.c).
//
//  Разделение труда, которое здесь держится:
//   - задача LVGL (ui_tick, кнопки) НИКОГДА не трогает шину напрямую: только
//     неблокирующие vfd_cmd_* и чтение g_vfd;
//   - блокирующие vfd_read_regs()/vfd_write_reg() живут в задаче bus_worker,
//     как и требует заголовок vfd_easydrive.h;
//   - после любого обмена worker сам берёт lvgl_port_lock() и обновляет
//     виджеты — из своей задачи это обязательно, иначе гонка с портом LVGL.
//
//  Значения в spinbox — СЫРЫЕ регистры привода: частоты в 0.01 Гц, время
//  разгона/торможения в 0.1 с. Десятичную точку рисует формат spinbox.
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
#include "ui_easydrive.h"
#include "vfd_easydrive.h"

static const char *TAG = "EASYDRIVE_UI";

#define PARAM_COUNT 7
#define JOG_PARAM   6      // P2.19 = частота толчка
#define STEP_HZ     100    // шаг кнопок "-"/"+" = 1.00 Гц (шкала слайдера 0.01 Гц)

// Порядок обязан совпадать с порядком строк на экране ПАРАМЕТРЫ.
static const uint16_t PARAM_REG[PARAM_COUNT] = {
    0x0001, 0x0006, 0x0007, 0x0008, 0x000E, 0x000F, 0x0213,
};

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

static QueueHandle_t s_job_q;
static float s_jog_hz = 5.0f;
static int64_t s_fault_seen_us;
static uint8_t s_fault_latched;

// ---------------------------------------------------------------- виджеты
// Указатели SLS заполняет только внутри ui_*_screen_init(), поэтому таблицу
// инициализировать адресами нельзя — каждый раз читаем глобальную.

static lv_obj_t *spinbox_at(int i)
{
    switch (i) {
    case 0: return ui_Spn_p0;
    case 1: return ui_Spn_p1;
    case 2: return ui_Spn_p2;
    case 3: return ui_Spn_p3;
    case 4: return ui_Spn_p4;
    case 5: return ui_Spn_p5;
    default: return ui_Spn_p6;
    }
}

// Знаков после точки в каждой строке — второе число lv_spinbox_set_digit_format
// в ui/screens/ui_params.c; диапазоны те же, что заданы при экспорте.
static const uint8_t PARAM_DEC[PARAM_COUNT] = { 0, 2, 2, 2, 1, 1, 2 };
static const int32_t PARAM_MIN[PARAM_COUNT] = { 0, 0, 0, 0, 1, 1, 0 };
static const int32_t PARAM_MAX[PARAM_COUNT] = { 8, 40000, 40000, 40000, 36000, 36000, 5000 };

/* LVGL-спинбокс всегда дописывает ведущие нули до digit_count, и 0.1 с
 * выглядело как "0000.1" — оператор читает это как 0,0001. Разрез подгоняем
 * под фактическую длину числа. set_digit_format() заодно молча срезает
 * range_max до 10^digit_count, поэтому диапазон возвращаем сразу следом. */
static void spinbox_show(int i, int32_t raw)
{
    lv_obj_t *sb = spinbox_at(i);
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
}

static lv_obj_t *journal_label_at(int i)
{
    return i == 0 ? ui_Lbl_jrn0 : (i == 1 ? ui_Lbl_jrn1 : ui_Lbl_jrn2);
}

static lv_obj_t *snapshot_label_at(int i)
{
    switch (i) {
    case 0: return ui_Lbl_snap0;
    case 1: return ui_Lbl_snap1;
    case 2: return ui_Lbl_snap2;
    case 3: return ui_Lbl_snap3;
    case 4: return ui_Lbl_snap4;
    default: return ui_Lbl_snap5;
    }
}

static int32_t setpoint_01hz(void)
{
    int32_t v = lv_slider_get_value(ui_Sld_sp);
    const int32_t max_raw = CONFIG_STANOK_MAX_FREQ_HZ * 100;
    return v > max_raw ? max_raw : v;
}

// ---------------------------------------------------------------- задачи
typedef enum {
    JOB_PARAMS_READ,
    JOB_PARAMS_WRITE,
    JOB_JOURNAL_READ,
    JOB_COUNTERS_RESET,
} job_t;

static void set_msg(lv_obj_t *lbl, const char *fmt, ...)
{
    if (!lbl) return;
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    lv_label_set_text(lbl, buf);
}

static void post_job(job_t job)
{
    if (s_job_q && xQueueSend(s_job_q, &job, 0) != pdTRUE) {
        ESP_LOGW(TAG, "очередь шины занята, задание %d отброшено", job);
    }
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
            ESP_LOGI(TAG, "raw из привода: P0.01=%u P0.06=%u(0.01Гц) P0.07=%u(0.01Гц) "
                          "P0.08=%u(0.01Гц) P0.14=%u(0.1с) P0.15=%u(0.1с) P2.19=%u(0.01Гц)",
                     raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6]);
            if (lvgl_port_lock(0)) {
                for (int i = 0; i < PARAM_COUNT; i++) {
                    if (raw[i] || i != JOG_PARAM) spinbox_show(i, raw[i]);
                }
                if (ok == PARAM_COUNT) {
                    set_msg(ui_Lbl_p_msg, "Прочитано %d/%d", ok, PARAM_COUNT);
                } else {
                    set_msg(ui_Lbl_p_msg, "Отвечено %d из %d, остальное - таймаут", ok, PARAM_COUNT);
                }
                lvgl_port_unlock();
            }
            break;
        }

        case JOB_PARAMS_WRITE: {
            uint16_t want[PARAM_COUNT];
            if (lvgl_port_lock(0)) {
                for (int i = 0; i < PARAM_COUNT; i++) want[i] = (uint16_t)lv_spinbox_get_value(spinbox_at(i));
                lvgl_port_unlock();
            } else {
                break;
            }
            // Эхо записи про подтверждает только приём кадра (см. шапку
            // vfd_easydrive.h), поэтому каждую величину читаем обратно.
            int confirmed = 0;
            for (int i = 0; i < PARAM_COUNT; i++) {
                uint16_t back = 0xFFFF;
                if (vfd_write_reg(PARAM_REG[i], want[i]) &&
                    vfd_read_regs(PARAM_REG[i], &back, 1) && back == want[i]) {
                    confirmed++;
                }
            }
            if (lvgl_port_lock(0)) {
                set_msg(ui_Lbl_p_msg, "Подтверждено чтением %d/%d", confirmed, PARAM_COUNT);
                lv_obj_set_style_text_color(ui_Lbl_p_msg,
                    lv_palette_main(confirmed == PARAM_COUNT ? LV_PALETTE_GREEN : LV_PALETTE_ORANGE), 0);
                lvgl_port_unlock();
            }
            break;
        }

        case JOB_JOURNAL_READ: {
            uint16_t code[3] = {0}, snap[6] = {0};
            bool codes_ok = vfd_read_regs(0x0D13, code, 3);
            bool snap_ok = vfd_read_regs(0x0D16, snap, 6);
            if (lvgl_port_lock(0)) {
                for (int i = 0; i < 3; i++) {
                    uint8_t c = (uint8_t)(code[i] & 0x7F);
                    if (!codes_ok) {
                        lv_label_set_text_fmt(journal_label_at(i), "d-%d   нет связи", 19 + i);
                    } else if (c == 0 || c >= 20) {
                        lv_label_set_text_fmt(journal_label_at(i), "d-%d   пусто", 19 + i);
                    } else {
                        lv_label_set_text_fmt(journal_label_at(i), "d-%d  Er%02d  %s",
                                              19 + i, c, FAULT_NAME[c]);
                    }
                }
                if (!snap_ok) {
                    for (int i = 0; i < 6; i++) {
                        lv_label_set_text_fmt(snapshot_label_at(i), "d-%d  нет связи", 22 + i);
                    }
                } else {
                    // d-22..d-27 повторяют d-00/d-01/d-02/d-03/d-07/d-16
                    lv_label_set_text_fmt(snapshot_label_at(0), "d-22 частота   %.2f Гц", snap[0] / 100.0);
                    lv_label_set_text_fmt(snapshot_label_at(1), "d-23 уставка   %.2f Гц", snap[1] / 100.0);
                    lv_label_set_text_fmt(snapshot_label_at(2), "d-24 ток       %.2f А", snap[2] / 100.0);
                    lv_label_set_text_fmt(snapshot_label_at(3), "d-25 U выхода  %d В", snap[3]);
                    lv_label_set_text_fmt(snapshot_label_at(4), "d-26 звено DC  %d В", snap[4]);
                    lv_label_set_text_fmt(snapshot_label_at(5), "d-27 IGBT      %.1f C", snap[5] / 10.0);
                }
                lvgl_port_unlock();
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

// ---------------------------------------------------------------- экраны
static void paint(lv_obj_t *lbl, uint32_t col)
{
    if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(col), 0);
}

static void refresh_main(void)
{
    lv_label_set_text_fmt(ui_Lbl_freq, "%.2f", (double)g_vfd.out_freq_hz);
    lv_label_set_text_fmt(ui_Lbl_cur, "%.2f", (double)g_vfd.out_cur_a);
    lv_label_set_text_fmt(ui_Lbl_volt, "%.0f", (double)g_vfd.out_volt_v);
    lv_label_set_text_fmt(ui_Lbl_rpm, "%ld", (long)g_vfd.rpm);
    lv_label_set_text_fmt(ui_Lbl_sp_val, "%.2f Гц", setpoint_01hz() / 100.0);

    const char *state;
    uint32_t col;
    if (g_vfd.link == VFD_LINK_DEAD) {
        state = "ПРИВОД НЕ ОТВЕЧАЕТ";
        col = 0x9E9E9E;
    } else if (g_vfd.link_stale) {
        state = "НЕТ СВЯЗИ";
        col = 0xE53935;
    } else if (g_vfd.is_fault) {
        state = "АВАРИЯ";
        col = 0xE53935;
    } else if (g_vfd.is_running) {
        col = 0x43A047;
        if (g_vfd.status_word & VFD_ST_DECELERATING) state = "ТОРМОЖЕНИЕ";
        else if (g_vfd.status_word & VFD_ST_ACCELERATING) state = "РАЗГОН";
        else state = (g_vfd.status_word & VFD_ST_DIR_REV) ? "РАБОТА НАЗАД" : "РАБОТА ВПЕРЁД";
    } else {
        state = "ОСТАНОВЛЕН";
        col = 0xFFB300;
    }
    if (g_vfd.is_fault && !g_vfd.link_stale && g_vfd.link != VFD_LINK_DEAD) {
        lv_label_set_text_fmt(ui_Lbl_state, "АВАРИЯ Er%02u", g_vfd.fault_code);
    } else {
        lv_label_set_text(ui_Lbl_state, state);
    }
    paint(ui_Lbl_state, col);

    switch (g_vfd.link) {
    case VFD_LINK_OK:
        lv_label_set_text_fmt(ui_Lbl_link, "СВЯЗЬ OK  %u мс", g_vfd.latency_ms);
        paint(ui_Lbl_link, 0x43A047);
        break;
    case VFD_LINK_DEGRADED:
        lv_label_set_text_fmt(ui_Lbl_link, "СВЯЗЬ С РЕТРАЯМИ  crc %lu", (unsigned long)g_vfd.crc_err);
        paint(ui_Lbl_link, 0xFFB300);
        break;
    default:
        lv_label_set_text(ui_Lbl_link, "СВЯЗЬ: НЕТ ОТВЕТОВ");
        paint(ui_Lbl_link, 0xE53935);
        break;
    }

    // Активная авария: имя и подсказка из таблицы, время - локальный отсчёт
    if (g_vfd.is_fault && g_vfd.fault_code != s_fault_latched) {
        s_fault_latched = g_vfd.fault_code;
        s_fault_seen_us = esp_timer_get_time();
    } else if (!g_vfd.is_fault) {
        s_fault_latched = 0;
    }
    if (g_vfd.is_fault) {
        uint8_t c = g_vfd.fault_code;
        lv_label_set_text_fmt(ui_Lbl_acode, "Er%02u", c);
        lv_label_set_text(ui_Lbl_aname, c < 20 ? FAULT_NAME[c] : "неизвестный код");
        lv_label_set_text_fmt(ui_Lbl_adesc, "%s", c < 20 ? FAULT_HINT[c] : "-");
        lv_label_set_text_fmt(ui_Lbl_atime, "идёт %.1f с", (esp_timer_get_time() - s_fault_seen_us) / 1e6);
    } else {
        lv_label_set_text(ui_Lbl_acode, "Er--");
        lv_label_set_text(ui_Lbl_aname, "Нет активной аварии");
        lv_label_set_text(ui_Lbl_adesc, "d-19..d-21 читаются кнопкой ниже");
        lv_label_set_text(ui_Lbl_atime, "");
    }
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

static void refresh_conn(void)
{
    lv_label_set_text(ui_Lbl_c_state, g_vfd.link == VFD_LINK_OK ? "СВЯЗЬ УСТАНОВЛЕНА" :
                      (g_vfd.link == VFD_LINK_DEGRADED ? "СВЯЗЬ С ОШИБКАМИ" : "СВЯЗЬ НЕТ"));
    paint(ui_Lbl_c_state, g_vfd.link == VFD_LINK_OK ? 0x43A047 :
          (g_vfd.link == VFD_LINK_DEGRADED ? 0xFFB300 : 0xE53935));

    lv_label_set_text_fmt(ui_Lbl_c_cfg, "UART%d  TX=GPIO%d RX=GPIO%d  DE авто",
                          CONFIG_STANOK_RS485_UART_NUM, g_vfd.uart_tx_gpio, g_vfd.uart_rx_gpio);
    lv_label_set_text_fmt(ui_Lbl_c_cfg2, "%d 8E1  адрес %d  опрос %d мс  таймаут %d мс",
                          CONFIG_STANOK_RS485_BAUD, CONFIG_STANOK_VFD_ADDR,
                          CONFIG_STANOK_POLL_MS, CONFIG_STANOK_RESP_TIMEOUT_MS);

    lv_label_set_text_fmt(ui_Lbl_c_cnt, "опр %lu  ретр %lu  тайм %lu  crc %lu",
                          (unsigned long)g_vfd.polls_ok, (unsigned long)g_vfd.retries,
                          (unsigned long)g_vfd.timeouts, (unsigned long)g_vfd.crc_err);
    lv_label_set_text_fmt(ui_Lbl_c_cnt2, "команд %lu  ошибок %lu  латент %u мс  эхо %u Б",
                          (unsigned long)g_vfd.cmd_ok, (unsigned long)g_vfd.cmd_err,
                          g_vfd.latency_ms, g_vfd.echo_bytes);

    lv_label_set_text_fmt(ui_Lbl_c_cmd, "0x2000 = %04X  %s", g_vfd.last_cmd,
                          g_vfd.last_cmd_acked ? (g_vfd.last_cmd_confirmed ? "подтверждено" : "эхо без подтверждения") : "без ответа");
    lv_label_set_text_fmt(ui_Lbl_c_cmd2, "чтение обратно: 0x%04X  %s", g_vfd.cmd_readback,
                          g_vfd.cmd_readback_ok ? "совпало" : "НЕ совпало");

    lv_label_set_text_fmt(ui_Lbl_c_err, "%s  FC%02X  принято %u / ждали %u",
                          fault_reason_text(), g_vfd.dbg_func, g_vfd.dbg_got, g_vfd.dbg_exp);
    lv_label_set_text_fmt(ui_Lbl_c_frame, "кадр: %s", g_vfd.dbg_frame[0] ? g_vfd.dbg_frame : "-");
}

static void ui_tick(lv_timer_t *t)
{
    (void)t;
    lv_obj_t *scr = lv_screen_active();
    if (scr == ui_main) refresh_main();
    else if (scr == ui_alarm) refresh_main();   // тот же блок аварии/связи
    else if (scr == ui_conn) refresh_conn();
}

// ---------------------------------------------------------------- события
static void on_start(lv_event_t *e)
{
    (void)e;
    vfd_cmd_run_fwd(setpoint_01hz() / 100.0f);
}

static void on_reverse(lv_event_t *e)
{
    (void)e;
    vfd_cmd_run_rev(setpoint_01hz() / 100.0f);
}

static void on_stop(lv_event_t *e)
{
    (void)e;
    vfd_cmd_stop();
}

static void on_jog(lv_event_t *e)
{
    (void)e;
    vfd_cmd_jog(false, s_jog_hz);
}

static void on_fault_reset(lv_event_t *e)
{
    (void)e;
    vfd_cmd_reset_fault();
}

static void on_setpoint(lv_event_t *e)
{
    // VALUE_CHANGED приходит и во время перетаскивания: кадр шлём один раз на отпускании
    if (lv_event_get_code(e) != LV_EVENT_RELEASED) return;
    vfd_cmd_set_frequency(setpoint_01hz() / 100.0f);
}

static void on_setpoint_step(lv_event_t *e)
{
    int32_t step = (int32_t)(intptr_t)lv_event_get_user_data(e);
    int32_t v = lv_slider_get_value(ui_Sld_sp) + step;
    const int32_t max_raw = CONFIG_STANOK_MAX_FREQ_HZ * 100;
    if (v < 0) v = 0;
    if (v > max_raw) v = max_raw;
    lv_slider_set_value(ui_Sld_sp, v, LV_ANIM_OFF);
    vfd_cmd_set_frequency(v / 100.0f);
}

static void on_params_read(lv_event_t *e)
{
    (void)e;
    set_msg(ui_Lbl_p_msg, "Читаю %d регистра...", PARAM_COUNT);
    post_job(JOB_PARAMS_READ);
}

static void on_params_write(lv_event_t *e)
{
    (void)e;
    set_msg(ui_Lbl_p_msg, "Пишу и читаю обратно...");
    post_job(JOB_PARAMS_WRITE);
}

static void on_journal_read(lv_event_t *e)
{
    (void)e;
    post_job(JOB_JOURNAL_READ);
}

static void on_counters_reset(lv_event_t *e)
{
    (void)e;
    post_job(JOB_COUNTERS_RESET);
}

static void bind(lv_obj_t *obj, lv_event_cb_t cb, lv_event_code_t code, void *ud)
{
    if (obj) lv_obj_add_event_cb(obj, cb, code, ud);
}

// ---------------------------------------------------------------- старт
void easydrive_ui_init(void)
{
    ui_init();

    s_job_q = xQueueCreate(4, sizeof(job_t));
    if (s_job_q == NULL || xTaskCreate(bus_worker, "ui_bus", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "не удалось создать задачу обслуживания шины");
        return;
    }

    bind(ui_Btn_start, on_start, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_stop, on_stop, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_rev, on_reverse, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_jog, on_jog, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_fault_reset, on_fault_reset, LV_EVENT_CLICKED, NULL);

    bind(ui_Sld_sp, on_setpoint, LV_EVENT_RELEASED, NULL);
    bind(ui_Btn_sp_dn, on_setpoint_step, LV_EVENT_CLICKED, (void *)(intptr_t)-STEP_HZ);
    bind(ui_Btn_sp_up, on_setpoint_step, LV_EVENT_CLICKED, (void *)(intptr_t)STEP_HZ);

    bind(ui_Btn_p_read, on_params_read, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_p_write, on_params_write, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_a_read, on_journal_read, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_a_reset, on_fault_reset, LV_EVENT_CLICKED, NULL);
    bind(ui_Btn_c_reset, on_counters_reset, LV_EVENT_CLICKED, NULL);

    lv_timer_create(ui_tick, 100, NULL);
    // До первого чтения строки таблицы показывают нули, а спинбокс рисует их
    // как "0000.1" / "000.00". Ведущие нули срезаем сразу, иначе размерность
    // читается наоборот ещё до того, как привод успел ответить.
    for (int i = 0; i < PARAM_COUNT; i++) spinbox_show(i, lv_spinbox_get_value(spinbox_at(i)));
    // Таблица ПАРАМЕТРЫ не должна выглядеть нулевой до первого нажатия, а в логе
    // сразу видно, какая размерность реально лежит в приводе.
    job_t first = JOB_PARAMS_READ;
    xQueueSend(s_job_q, &first, 0);
    ESP_LOGI(TAG, "панель EASYDRIVE: %d экранов, параметры 0x%04X..0x%04X",
             PARAM_COUNT, PARAM_REG[0], PARAM_REG[PARAM_COUNT - 1]);
}
