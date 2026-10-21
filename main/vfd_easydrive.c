#include "vfd_easydrive.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "VFD";

/* Не выставленные Kconfig-значения дают рабочий профиль по умолчанию,
 * чтобы файл собирался и без main/Kconfig.projbuild. Для булевых опций
 * «нет в sdkconfig» = «выключено»: включение задаётся в sdkconfig.defaults. */
#ifndef CONFIG_STANOK_RS485_UART_NUM
#define CONFIG_STANOK_RS485_UART_NUM 1
#endif
#ifndef CONFIG_STANOK_RS485_TX_GPIO
#define CONFIG_STANOK_RS485_TX_GPIO 26   /* схема V1.0: GPIO26 = TX1 -> DI U8 */
#endif
#ifndef CONFIG_STANOK_RS485_RX_GPIO
#define CONFIG_STANOK_RS485_RX_GPIO 27   /* схема V1.0: GPIO27 = RX1 <- RO U8 */
#endif
#ifndef CONFIG_STANOK_RS485_DE_GPIO
#define CONFIG_STANOK_RS485_DE_GPIO -1
#endif
#ifndef CONFIG_STANOK_RS485_BAUD
#define CONFIG_STANOK_RS485_BAUD 9600
#endif
#ifndef CONFIG_STANOK_VFD_ADDR
#define CONFIG_STANOK_VFD_ADDR 1
#endif
#ifndef CONFIG_STANOK_POLL_MS
#define CONFIG_STANOK_POLL_MS 200
#endif
#ifndef CONFIG_STANOK_RESP_TIMEOUT_MS
#define CONFIG_STANOK_RESP_TIMEOUT_MS 120
#endif
#ifndef CONFIG_STANOK_LINK_TIMEOUT_MS
#define CONFIG_STANOK_LINK_TIMEOUT_MS 1000
#endif
#ifndef CONFIG_STANOK_MAX_FREQ_HZ
#define CONFIG_STANOK_MAX_FREQ_HZ 50
#endif
#ifndef CONFIG_STANOK_STOP_ON_BOOT
#define CONFIG_STANOK_STOP_ON_BOOT 1
#endif
#ifndef CONFIG_STANOK_RS485_AUTO_PINS
#define CONFIG_STANOK_RS485_AUTO_PINS 1
#endif
#ifndef CONFIG_STANOK_RS485_PIN_SCAN
#define CONFIG_STANOK_RS485_PIN_SCAN 1
#endif
#ifndef CONFIG_STANOK_TOUCH_INT_GPIO
#define CONFIG_STANOK_TOUCH_INT_GPIO 21
#endif
#ifndef CONFIG_STANOK_USE_IDM20
#define CONFIG_STANOK_USE_IDM20 0
#endif

#define VFD_UART_BUF      512
#define VFD_CMD_QUEUE_LEN 8
#define JOG_CHECK_MS      700   /* окно, за которое толчок обязан показать «работаю» */

typedef enum {
    OP_SET_FREQ,
    OP_RUN_FWD,
    OP_RUN_REV,
    OP_JOG_FWD,
    OP_JOG_REV,
    OP_STOP,
    OP_RESET_FAULT,
} vfd_op_t;

typedef struct {
    vfd_op_t op;
    float hz;
} vfd_cmd_msg_t;

volatile vfd_state_t g_vfd;

static QueueHandle_t  s_cmd_q;
static SemaphoreHandle_t s_state_mtx;
static SemaphoreHandle_t s_bus_mtx;   /* одна транзакция 485 целиком, между хозяевами шины */
static uint32_t s_err_streak;
static uint32_t s_retry_streak;
static uint32_t s_poll_phase;
static bool     s_boot_stop_done;
static uint16_t s_want_freq_reg;    /* ждём, что d-01 сойдётся с записанным */
static uint8_t  s_tx_active = VFD_UART_TX_GPIO;
static uint8_t  s_rx_active = VFD_UART_RX_GPIO;

/* Недогляд за толчком: слово 0x0013 из табл.4 имеет бит2..3 = 00 («run for
 * once»), привод после него останавливается сам, и второе касание той же
 * цифры края не даёт. Проверяем обратным чтением, был ли ход; не был —
 * пробуем слово толчка с бит2..3 = 11 («непрерывно»). */
static uint8_t  s_jog_stage;      /* 0 нет, 1 после «однократно», 2 после «непрерывно» */
static uint16_t s_jog_alt_word;
static bool     s_jog_saw_run;
static int64_t  s_jog_check_us;
static bool     s_probing;   /* true: кадры автоподбора не идут в статистику связи */
static int64_t  s_bus_idle_us;  /* момент, когда мы最后一次 перестали принимать линия */

static uint16_t clamp_hz_to_reg(float hz)
{
    if (hz < 0.0f) hz = 0.0f;
    if (hz > (float)CONFIG_STANOK_MAX_FREQ_HZ) hz = (float)CONFIG_STANOK_MAX_FREQ_HZ;
    return (uint16_t)(hz * 100.0f + 0.5f);
}

/* ======================= CRC-16/MODBUS ======================= */
static uint16_t modbus_crc16(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

/* ======================= Обмен одним кадром =======================
 * Строгий half-duplex: перед отправкой очищаем приём, после отправки
 * ждём завершения передачи и выдерживаем T3.5. Ожидаем РОВНО ожидаемую
 * длину: у привода нет исключений, поэтому лишние или недостающие байты
 * — это мусор в линии, а не «непонятный ответ». */
/* Снимок последнего неудачного обмена — чтобы по логу отличить «привод молчит»
 * от «привод отвечает, но не то». */
static void dbg_frame(const uint8_t *req, uint8_t reason, const uint8_t *resp, size_t got, size_t exp)
{
    if (s_probing) return;
    g_vfd.dbg_reason = reason;
    g_vfd.dbg_func = req[1];
    g_vfd.dbg_got = (uint16_t)got;
    g_vfd.dbg_exp = (uint16_t)exp;
    size_t n = got < 8 ? got : 8;
    char tmp[sizeof(g_vfd.dbg_frame)] = { 0 };
    size_t o = 0;
    for (size_t i = 0; i < n; i++)
        o += snprintf(tmp + o, sizeof(tmp) - o, "%02X ", resp[i]);
    for (size_t i = 0; i < sizeof(tmp); i++) g_vfd.dbg_frame[i] = tmp[i];
}

static bool bus_xfer_locked(const uint8_t *req, size_t req_len, uint8_t *resp, size_t resp_len)
{
    /* Пауза ПОСЛЕ ответа ведомого. Привод переключает направление приёма и
     * пересчитывает параметры примерно столько же, сколько идёт сам кадр;
     * если следующий запрос отправить сразу за его ответом, первые биты
     * теряются и привод молчит весь таймаут. Это и была причина «нет связи»
     * при полностью живой линии. */
    if (s_bus_idle_us) {
        int64_t since = esp_timer_get_time() - s_bus_idle_us;
        if (since < VFD_TURNAROUND_US)
            vTaskDelay(pdMS_TO_TICKS((uint32_t)(VFD_TURNAROUND_US - since + 999) / 1000));
    }

    uart_flush_input(VFD_UART_NUM);
    uart_write_bytes(VFD_UART_NUM, (const char *)req, req_len);
    if (uart_wait_tx_done(VFD_UART_NUM, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGW(TAG, "TX не завершился за 100 мс");
    }
    /* Полудуплекс честно: всё, что пришло ДОКОНЦА нашей же передачи, — это
     * возврат нашего кадра приёмником (авто-направление на U8 может и так,
     * и этак). Ответ ведомого физически не мог быться раньше, чем через
     * 20..40 мс, поэтому эти байты выбрасываем и ведём счёт. Без этого эхо
     * 06H/10H неотличимо от ответа привода: кадр записи побайтово совпадает
     * со своим эхом, включая CRC. */
    {
        size_t pending = 0;
        uart_get_buffered_data_len(VFD_UART_NUM, &pending);
        if (pending) {
            if ((uint16_t)pending > g_vfd.echo_bytes) g_vfd.echo_bytes = (uint16_t)pending;
            uart_flush_input(VFD_UART_NUM);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(VFD_FRAME_GAP_MS));

    size_t got = 0;
    int64_t deadline = esp_timer_get_time() + (int64_t)CONFIG_STANOK_RESP_TIMEOUT_MS * 1000;
    while ((int64_t)got < (int64_t)resp_len && esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(VFD_UART_NUM, resp + got, resp_len - got, pdMS_TO_TICKS(10));
        if (n > 0) got += (size_t)n;
    }
    s_bus_idle_us = esp_timer_get_time();

    if (got != resp_len) {
        dbg_frame(req, 1, resp, got, resp_len);
        if (!s_probing) {
            g_vfd.timeouts++;
            if (got) g_vfd.crc_err++;   /* пришли огрызки — наводки в линии */
        }
        return false;
    }
    if (resp[0] != VFD_MODBUS_ADDR) {
        dbg_frame(req, 2, resp, got, resp_len);
        if (!s_probing) g_vfd.crc_err++;
        return false;
    }
    uint16_t crc = modbus_crc16(resp, resp_len - 2);
    if (resp[resp_len - 2] != (uint8_t)crc || resp[resp_len - 1] != (uint8_t)(crc >> 8)) {
        dbg_frame(req, 3, resp, got, resp_len);
        if (!s_probing) g_vfd.crc_err++;
        return false;
    }
    /* Ответ на 03H: функция та же и байт количества обязан совпадать.
     * Ответ на 06H/10H: эхо первых байтов запроса. */
    if (req[1] == 0x03) {
        if (resp[1] != 0x03 || resp[2] != (uint8_t)(resp_len - 5)) {
            dbg_frame(req, 4, resp, got, resp_len);
            if (!s_probing) g_vfd.crc_err++;
            return false;
        }
    } else if (resp[1] != req[1] || resp[2] != req[2] || resp[3] != req[3]) {
        dbg_frame(req, 4, resp, got, resp_len);
        if (!s_probing) g_vfd.crc_err++;
        return false;
    }
    return true;
}

/* Кадр на полудуплексной 485 — неделимая сделка: чужой запрос, вставший в
 * паузу между нашим TX и ответом, приводит к огрызкам и CRC. Хозяев у шины
 * два (vfd_task с опросом и bus_worker с экранами ПАРАМЕТРЫ/ЖУРНАЛ), поэтому
 * захват здесь, в единственной точке выхода в линию. Худший держатель ~240 мс
 * (15 + 100 TX + 5 + 120 таймаут), ждём вдесятеро больше: на старте UI читает
 * таблицу параллельно с разведкой карты регистров.
 * Отказ ждать — это НЕ ошибка привода: счётчики связи не трогаем. */
static bool bus_xfer(const uint8_t *req, size_t req_len, uint8_t *resp, size_t resp_len)
{
    if (!xSemaphoreTake(s_bus_mtx, pdMS_TO_TICKS(2500))) {
        ESP_LOGW(TAG, "шина занята >1 с, кадр пропущен");
        return false;
    }
    bool ok = bus_xfer_locked(req, req_len, resp, resp_len);
    xSemaphoreGive(s_bus_mtx);
    return ok;
}

static bool bus_xfer_retry(const uint8_t *req, size_t req_len, uint8_t *resp, size_t resp_len)
{
    for (int attempt = 0; attempt < VFD_RETRY_MAX; attempt++) {
        if (attempt) {
            g_vfd.retries++;
            s_retry_streak++;
            vTaskDelay(pdMS_TO_TICKS(VFD_RETRY_BACKOFF_MS));
        }
        if (bus_xfer(req, req_len, resp, resp_len)) {
            if (attempt == 0) s_retry_streak = 0;
            return true;
        }
    }
    return false;
}

/* ======================= Чтение / запись ======================= */
bool vfd_read_regs(uint16_t reg, uint16_t *out, uint8_t count)
{
    if (count == 0 || count > VFD_MAX_BLOCK_REG) return false;
    uint8_t req[8] = { VFD_MODBUS_ADDR, 0x03, reg >> 8, reg & 0xFF, 0, count };
    uint16_t c = modbus_crc16(req, 6);
    req[6] = c; req[7] = c >> 8;

    uint8_t resp[5 + 2 * VFD_MAX_BLOCK_REG];
    size_t rlen = 5 + 2 * (size_t)count;
    int64_t t0 = esp_timer_get_time();
    /* 8, а не 6: CRC уже дописана в req, и привод принимает только полный
     * кадр. С длиной 6 уходил кадр без двух байтов CRC — привод на него
     * молчал, и связь выглядела мёртвой при полностью живой линии. */
    if (!bus_xfer_retry(req, 8, resp, rlen)) {
        s_err_streak++;
        return false;
    }
    for (uint8_t i = 0; i < count; i++)
        out[i] = (uint16_t)((resp[3 + 2 * i] << 8) | resp[4 + 2 * i]);

    g_vfd.polls_ok++;
    s_err_streak = 0;
    g_vfd.last_ok_us = esp_timer_get_time();
    uint32_t ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
    g_vfd.latency_ms = (uint16_t)((g_vfd.latency_ms * 3 + ms) / 4);
    return true;
}

static bool write_single(uint16_t reg, uint16_t value)
{
    uint8_t req[8] = { VFD_MODBUS_ADDR, 0x06, reg >> 8, reg & 0xFF, value >> 8, value & 0xFF };
    uint16_t c = modbus_crc16(req, 6);
    req[6] = c; req[7] = c >> 8;

    uint8_t resp[8];
    if (!bus_xfer_retry(req, 8, resp, 8)) { g_vfd.cmd_err++; s_err_streak++; return false; }
    if (memcmp(resp, req, 8) != 0) { g_vfd.cmd_err++; g_vfd.crc_err++; return false; }
    g_vfd.cmd_ok++;
    g_vfd.last_ok_us = esp_timer_get_time();
    g_vfd.polls_ok++;
    s_err_streak = 0;
    return true;
}

/* 10H: команда + заданная частота одним кадром (мануал: 10H может менять
 * частоту одновременно с командой). Ответ — эхо первых 6 байтов + CRC. */
static bool write_pair(uint16_t reg, uint16_t v0, uint16_t v1)
{
    uint8_t req[13] = { VFD_MODBUS_ADDR, 0x10, reg >> 8, reg & 0xFF, 0, 2, 4,
                        v0 >> 8, v0 & 0xFF, v1 >> 8, v1 & 0xFF };
    uint16_t c = modbus_crc16(req, 11);
    req[11] = c; req[12] = c >> 8;

    uint8_t resp[8];
    if (!bus_xfer_retry(req, 13, resp, 8)) { g_vfd.cmd_err++; s_err_streak++; return false; }
    if (memcmp(resp, req, 6) != 0) { g_vfd.cmd_err++; g_vfd.crc_err++; return false; }
    g_vfd.cmd_ok++;
    g_vfd.last_ok_us = esp_timer_get_time();
    g_vfd.polls_ok++;
    s_err_streak = 0;
    return true;
}

bool vfd_write_reg(uint16_t reg, uint16_t value) { return write_single(reg, value); }

/* ======================= Автоподбор пары TX/RX =======================
 * По схеме V1.0 линия 485 — это UART1, TX=GPIO26, RX=GPIO27, встроенный
 * MAX485 (U8) с автоматическим направлением, так что ноги по умолчанию
 * заведомо верные. Перебор остаётся страховкой на другой ревизии платы или
 * на «я перепаял/подключил внешним модулем»: кандидатов проверяем НАСТОЯЩИМ
 * Modbus-кадром — молчание значит «не та нога», а ответ с нашим адресом и
 * живой CRC значит «линия найдена». Подбираем только пару TX/RX: выхода на
 * чужом пине DE мы боимся сильнее, чем неподобранного направления, поэтому
 * DE всегда берётся из menuconfig и перебором не трогается. */
#if CONFIG_STANOK_RS485_AUTO_PINS

static bool pin_is_off_limits(int gpio)
{
    static const int denied[] = {
        1, 34, 35, 36, 37, 38,                     /* strapping + кнопка BOOT */
        0,                                        /* сброс LCD (GPIO0 -> R47) */
        7, 8,                                     /* I2C периферии: тач + ES8311 */
        9, 10, 11, 12, 13, 48,                    /* I2S аудио + PA */
        22, 23,                                   /* тач RST, подсветка LCD */
        31, 50, 51, 52,                           /* Ethernet RMII */
        39, 40, 41, 42, 43, 44,                   /* microSD SDMMC slot 0 */
        14, 15, 16, 17, 18, 19, 54,               /* ESP-Hosted: SDIO slot 1 + RST */
    };
    if (!GPIO_IS_VALID_GPIO((gpio_num_t)gpio)) return true;
    for (unsigned i = 0; i < sizeof(denied) / sizeof(denied[0]); i++)
        if (denied[i] == gpio) return true;
    if ((gpio) == CONFIG_STANOK_TOUCH_INT_GPIO) return true;
#if CONFIG_STANOK_USE_IDM20
    if ((gpio) == CONFIG_STANOK_IDM20_A_GPIO || (gpio) == CONFIG_STANOK_IDM20_B_GPIO) return true;
#endif
    return false;
}

/* Один честный кадр чтения d-00: ответ пришёл — пара подходящая. */
static bool probe_pair(uint8_t tx, uint8_t rx)
{
    if (uart_set_pin(VFD_UART_NUM, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
        return false;
    uint8_t req[8] = { VFD_MODBUS_ADDR, 0x03, VFD_REG_D00 >> 8, VFD_REG_D00 & 0xFF, 0, 1 };
    uint16_t c = modbus_crc16(req, 6);
    req[6] = c; req[7] = c >> 8;
    uint8_t resp[7];
    s_probing = true;
    bool ok = bus_xfer(req, 8, resp, 7);
    s_probing = false;
    return ok;
}

static bool probe_bus_pins(uint8_t *tx_out, uint8_t *rx_out)
{
    static const uint8_t named[][2] = {
        { 26, 27 },               /* схема V1.0: TX1=GPIO26, RX1=GPIO27 */
        { 20, 21 }, { 21, 20 },   /* гребёнка 485 по разметке платы */
        {  2,  3 }, {  3,  2 },   /* README этого проекта */
        {  4,  5 }, {  5,  4 },   /* свободны, пока ИДМ-20 выключен */
    };
    s_probing = true;
    uart_flush_input(VFD_UART_NUM);
    for (unsigned i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
        if (pin_is_off_limits(named[i][0]) || pin_is_off_limits(named[i][1])) continue;
        if (probe_pair(named[i][0], named[i][1])) {
            s_probing = false;
            *tx_out = named[i][0]; *rx_out = named[i][1];
            return true;
        }
    }
#if CONFIG_STANOK_RS485_PIN_SCAN
    /* Полный перебор: ~400 кадров по ~40 мс = около 16 с, раз за жизнь связи. */
    for (int tx = 0; tx <= 48; tx++) {
        if (pin_is_off_limits(tx)) continue;
        for (int rx = 0; rx <= 48; rx++) {
            if (rx == tx || pin_is_off_limits(rx)) continue;
            esp_task_wdt_reset();
            if (probe_pair((uint8_t)tx, (uint8_t)rx)) {
                s_probing = false;
                *tx_out = (uint8_t)tx; *rx_out = (uint8_t)rx;
                return true;
            }
        }
    }
#endif
    s_probing = false;
    return false;
}
#endif /* CONFIG_STANOK_RS485_AUTO_PINS */

/* ======================= Очередь команд ======================= */
static void cmd_post(vfd_op_t op, float hz, bool drops_motion)
{
    vfd_cmd_msg_t msg = { .op = op, .hz = hz };
    if (drops_motion) {
        /* Стоп важнее всего: выкидываем из очереди неисполненные команды хода,
         * иначе после всплеска наводок станок поедет по устаревшему намерению. */
        vfd_cmd_msg_t stash[VFD_CMD_QUEUE_LEN];
        int n = 0;
        vfd_cmd_msg_t m;
        while (xQueueReceive(s_cmd_q, &m, 0) == pdTRUE) {
            bool motion = (m.op == OP_RUN_FWD || m.op == OP_RUN_REV ||
                           m.op == OP_JOG_FWD || m.op == OP_JOG_REV);
            if (!motion) stash[n++] = m;
        }
        for (int i = 0; i < n; i++) xQueueSend(s_cmd_q, &stash[i], 0);
    }
    if (xQueueSend(s_cmd_q, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "очередь команд полна — команда отброшена");
        g_vfd.cmd_err++;
    }
}

void vfd_cmd_set_frequency(float hz) { cmd_post(OP_SET_FREQ, hz, false); }
void vfd_cmd_run_fwd(float hz)       { cmd_post(OP_RUN_FWD, hz, false); }
void vfd_cmd_run_rev(float hz)       { cmd_post(OP_RUN_REV, hz, false); }
void vfd_cmd_jog(bool reverse, float hz) { cmd_post(reverse ? OP_JOG_REV : OP_JOG_FWD, hz, false); }
void vfd_cmd_stop(void)              { cmd_post(OP_STOP, 0.0f, true); }
void vfd_cmd_reset_fault(void)       { cmd_post(OP_RESET_FAULT, 0.0f, false); }

/* ======================= Исполнение команды ======================= */
/* Обратное чтение 0x2000. Эхо записи ничего не доказывает: привод на плохой
 * кадр молчит, а на хороший отвечает побайтовой копией запроса, CRC включая.
 * Число, которое привод сам отдаёт по 03H, — отдельное, уже независимое. */
static void verify_cmd(uint16_t expect)
{
    uint16_t v = 0;
    bool ok = vfd_read_regs(VFD_REG_CMD, &v, 1);
    xSemaphoreTake(s_state_mtx, portMAX_DELAY);
    g_vfd.cmd_readback    = v;
    g_vfd.cmd_readback_ok = ok && v == expect;
    xSemaphoreGive(s_state_mtx);
    ESP_LOGI(TAG, "  чтение 0x2000 после записи 0x%04X: %s", expect,
             !ok ? "привод не отдаёт" :
             (v == expect ? "лежит то же слово" : "ЛЕЖИТ ДРУГОЕ"));
}

/* Досмотр толчка: привод сам скажет, сдвинулся ли вал. Слово из табл.4
 * (0x0013) имеет бит2..3 = 00 «run for once», т.е. толчок с самостопом,
 * и повторная запись того же числа края не даёт. Вариант с бит2..3 = 11
 * («непрерывно») держит вал до слова СТОП, поэтому кнопка с ним живая. */
static void jog_followup(void)
{
    if (!s_jog_stage || esp_timer_get_time() < s_jog_check_us) return;

    if (s_jog_saw_run || g_vfd.out_freq_hz > 0.2f) {
        ESP_LOGI(TAG, "толчок подтверждён словом 0x%04X: d-00 = %.2f Гц, статус 0x%04X | "
                      "частоту толчка задаёт P2.19 привода, а не уставка панели",
                 g_vfd.last_cmd, g_vfd.out_freq_hz, g_vfd.status_word);
        s_jog_stage = 0;
        s_jog_alt_word = 0;
        return;
    }
    if (s_jog_stage == 1 && s_jog_alt_word) {
        uint16_t alt = s_jog_alt_word;
        ESP_LOGW(TAG, "слово 0x%04X («однократно») вал не сдвинуло — пробую 0x%04X («непрерывно»)",
                 g_vfd.last_cmd, alt);
        write_single(VFD_REG_CMD, VFD_CMD_STOP);
        write_single(VFD_REG_CMD, alt);
        s_jog_stage = 2;
        s_jog_saw_run = false;
        s_jog_check_us = esp_timer_get_time() + (int64_t)JOG_CHECK_MS * 1000;
        return;
    }
    ESP_LOGE(TAG, "ни 0x0013, ни 0x001F толчка не дали: d-00 = %.2f Гц, статус 0x%04X. "
                  "Смотрите P2.19 (при 0.00 толчка нет) и P0.04 = 2",
             g_vfd.out_freq_hz, g_vfd.status_word);
    s_jog_stage = 0;
    s_jog_alt_word = 0;
}

static void exec_cmd(const vfd_cmd_msg_t *m)
{
    uint16_t reg = clamp_hz_to_reg(m->hz);
    uint16_t cmd = 0;
    bool ok;

    switch (m->op) {
    case OP_SET_FREQ:
        ok = write_single(VFD_REG_FREQ_SET, reg);
        cmd = 0xFFFF;                      /* не команда пуска, подтверждения хода нет */
        s_want_freq_reg = reg;
        break;
    case OP_RUN_FWD:
    case OP_RUN_REV: {
        cmd = (m->op == OP_RUN_FWD) ? VFD_CMD_FWD_RUN : VFD_CMD_REV_RUN;
        uint16_t hz_reg = reg;
        /* 0 = «не менять частоту»: берём последнюю записанную уставку */
        if (hz_reg == 0) hz_reg = clamp_hz_to_reg(g_vfd.set_freq_hz);
        ok = write_pair(VFD_REG_CMD, cmd, hz_reg);
        s_want_freq_reg = hz_reg;
        break;
    }
    case OP_JOG_FWD:
    case OP_JOG_REV:
        cmd = (m->op == OP_JOG_FWD) ? VFD_CMD_FWD_JOG : VFD_CMD_REV_JOG;
        /* Всегда через СТОП: без фронта бит0..1 01->11 привод толчка не
         * видит, а слово в 0x2000 после толчка остаётся прежним. */
        write_single(VFD_REG_CMD, VFD_CMD_STOP);
        ok = write_single(VFD_REG_CMD, cmd);
        s_jog_alt_word = (m->op == OP_JOG_FWD) ? VFD_CMD_FWD_JOG_HOLD : VFD_CMD_REV_JOG_HOLD;
        s_jog_stage    = ok ? 1 : 0;
        s_jog_saw_run  = false;
        s_jog_check_us = esp_timer_get_time() + (int64_t)JOG_CHECK_MS * 1000;
        break;
    case OP_STOP:
        cmd = VFD_CMD_STOP;
        ok = write_single(VFD_REG_CMD, cmd);
        /* Оператор сказал «стоп» — никаких автодосмотров толчка после этого. */
        s_jog_stage = 0;
        s_jog_alt_word = 0;
        break;
    case OP_RESET_FAULT:
        cmd = VFD_CMD_FAULT_RESET;
        ok = write_single(VFD_REG_CMD, cmd);
        break;
    default:
        ok = false;
    }

    g_vfd.last_cmd = cmd;
    g_vfd.last_cmd_acked = ok;
    g_vfd.last_cmd_confirmed = false;      /* подтвердится чтением состояния */
    ESP_LOGI(TAG, "команда op=%d cmd=0x%04X hz=%.2f -> %s",
             m->op, cmd, m->hz, ok ? "принята приводом" : "НЕ ДОШЛА");
    if (ok && cmd != 0xFFFF) verify_cmd(cmd);
}

/* ======================= Опрос телеметрии ======================= */
static void poll_telemetry(void)
{
    uint16_t d[5];
    bool alive = false;
    bool d00_ok = false;

    /* Каждый цикл: частота/уставка/ток/напряжение/обороты + слово состояния.
     * Это то, по чему принимается решение об аварийном останове. */
    if (vfd_read_regs(VFD_REG_D00, d, 5)) {
        xSemaphoreTake(s_state_mtx, portMAX_DELAY);
        g_vfd.out_freq_hz   = d[0] / 100.0f;
        g_vfd.set_freq_hz   = d[1] / 100.0f;
        g_vfd.out_cur_a     = d[2] / 100.0f;
        g_vfd.out_volt_v    = d[3];
        g_vfd.rpm           = (int16_t)d[4];
        alive = true;
        d00_ok = true;
        xSemaphoreGive(s_state_mtx);
    }

    uint16_t st;
    if (vfd_read_regs(VFD_REG_STATUS, &st, 1)) {
        xSemaphoreTake(s_state_mtx, portMAX_DELAY);
        g_vfd.status_word = st;
        g_vfd.is_running  = (st & VFD_ST_RUNNING) != 0;
        g_vfd.is_fault    = (st & VFD_ST_FAULT) != 0;
        g_vfd.fault_code  = g_vfd.is_fault ? VFD_ST_FAULT_NUM(st) : 0;
        if (s_jog_stage && g_vfd.is_running) s_jog_saw_run = true;
        /* Подтверждение последней команды: верим не эху записи, а тому,
         * что привод сам показал в слове состояния. */
        if (g_vfd.last_cmd_acked && g_vfd.last_cmd != 0xFFFF) {
            switch (g_vfd.last_cmd) {
            case VFD_CMD_STOP:        g_vfd.last_cmd_confirmed = !g_vfd.is_running; break;
            case VFD_CMD_FAULT_RESET: g_vfd.last_cmd_confirmed = !g_vfd.is_fault;   break;
            default:                  g_vfd.last_cmd_confirmed = g_vfd.is_running;  break;
            }
        }
        alive = true;
        xSemaphoreGive(s_state_mtx);
    }

    /* Уставка частоты подтверждается по d-01: так видно, что привод реально
     * принял число, а не срезал его пределами P0.06/P0.07. */
    if (d00_ok && s_want_freq_reg) {
        if (abs((int)d[1] - (int)s_want_freq_reg) <= 1) {
            xSemaphoreTake(s_state_mtx, portMAX_DELAY);
            g_vfd.last_cmd_confirmed = true;
            xSemaphoreGive(s_state_mtx);
            s_want_freq_reg = 0;
        } else if (g_vfd.last_cmd_acked) {
            ESP_LOGW(TAG, "уставка не сошлась: записали %d, привод показывает %d",
                     s_want_freq_reg, d[1]);
        }
    }

    /* Раз в 5 циклов (~1 с): медленные величины — звено, сеть, температуры. */
    if (alive && (s_poll_phase % 5) == 0) {
        uint16_t b[3];
        if (vfd_read_regs(VFD_REG_D06, b, 3)) {
            xSemaphoreTake(s_state_mtx, portMAX_DELAY);
            g_vfd.bus_volt_v   = b[1];
            g_vfd.input_volt_v = b[2];
            xSemaphoreGive(s_state_mtx);
        }
        uint16_t t[2];
        if (vfd_read_regs(VFD_REG_D15, t, 2)) {
            xSemaphoreTake(s_state_mtx, portMAX_DELAY);
            g_vfd.igbt_temp_c = t[1] / 10.0f;
            xSemaphoreGive(s_state_mtx);
        }
    }
    s_poll_phase++;

    xSemaphoreTake(s_state_mtx, portMAX_DELAY);
    int64_t age = (esp_timer_get_time() - g_vfd.last_ok_us) / 1000;
    g_vfd.link_stale = (g_vfd.last_ok_us == 0) || (age > CONFIG_STANOK_LINK_TIMEOUT_MS);
    if (!alive && g_vfd.link_stale) g_vfd.link = VFD_LINK_DEAD;
    else if (s_retry_streak > 3)    g_vfd.link = VFD_LINK_DEGRADED;
    else                             g_vfd.link = VFD_LINK_OK;
    xSemaphoreGive(s_state_mtx);
}

bool vfd_link_is_down(void)
{
    if (g_vfd.last_ok_us == 0) return true;
    return (esp_timer_get_time() - g_vfd.last_ok_us) / 1000 > CONFIG_STANOK_LINK_TIMEOUT_MS;
}

/* ======================= Разведка карты регистров =======================
 * Этот привод на битый запрос молчит, а не отвечает исключением, поэтому
 * «что именно он согласен отдавать по FC03» можно узнать только спросив.
 * Делаем это один раз после нахождения линии и пишем результат в лог:
 * по нему видно, каким блоком надо опрашивать телеметрию. */
static void probe_reg_map(void)
{
    static const uint16_t probes[][2] = {
        { VFD_REG_D00,    1 }, { VFD_REG_D00, 2 }, { VFD_REG_D00, 3 },
        { VFD_REG_D00,    5 }, { VFD_REG_STATUS, 1 }, { VFD_REG_CMD, 1 },
        { VFD_REG_D06,    1 }, { VFD_REG_D06, 3 }, { VFD_REG_D15, 2 },
    };
    ESP_LOGW(TAG, "разведка FC03: что привод отвечает по блокам");
    for (unsigned i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        uint16_t reg = probes[i][0], cnt = probes[i][1];
        uint8_t req[8] = { VFD_MODBUS_ADDR, 0x03, reg >> 8, reg & 0xFF, 0, (uint8_t)cnt };
        uint16_t c = modbus_crc16(req, 6);
        req[6] = c; req[7] = c >> 8;
        uint8_t resp[5 + 2 * VFD_MAX_BLOCK_REG];
        int64_t t0 = esp_timer_get_time();
        bool ok = bus_xfer(req, 8, resp, 5 + 2 * cnt);
        uint32_t ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
        uint16_t val = ok ? (uint16_t)((resp[3] << 8) | resp[4]) : 0;
        static const char *const why[] = {
            "-", "нет ответа", "чужой адрес", "CRC", "функция/длина"
        };
        char frame[sizeof(g_vfd.dbg_frame)];
        for (size_t i = 0; i < sizeof(frame); i++) frame[i] = g_vfd.dbg_frame[i];
        if (ok) ESP_LOGI(TAG, "  0x%04X x%u: ЕСТЬ, %u мс, первое 0x%04X", reg, cnt, ms, val);
        else    ESP_LOGI(TAG, "  0x%04X x%u: НЕТ (%s), %u мс, ждали %u пришли %u: [%s]",
                         reg, cnt, why[g_vfd.dbg_reason & 7], ms,
                         g_vfd.dbg_exp, g_vfd.dbg_got, frame);
        esp_task_wdt_reset();
    }

    /* Параметры, без которых непонятно, почему команда уходит, а вал стоит.
     * Адрес = группа.номер: P2.19 -> 0x0213 (таблица 1 мануала CV3100/MINI). */
    static const struct { uint16_t addr; const char *name; } params[] = {
        { 0x0004, "P0.04 источник команды" },
        { 0x0007, "P0.07 нижний предел, 0.01 Гц" },
        { 0x0017, "P0.23 клавиша REV/JOG" },
        { 0x0213, "P2.19 частота толчка, 0.01 Гц" },
        { 0x0214, "P2.20 разгон толчка, 0.1 с" },
        { 0x0215, "P2.21 торможение толчка, 0.1 с" },
        { 0x0802, "P8.02 таймаут связи" },
        /* Разгон/торможение (P0.14/P0.15) измеряются в 0.1 ДОЛИ ВЫБРАННОЙ ЕДИНИЦЫ,
         * а единицу выбирает P3.09: 0 = секунды, 1 = минуты (завод — минуты).
         * Без этого числа 104 на панели значит то 10.4 с, то 10.4 мин. */
        { 0x0309, "P3.09 единица времени разгона (0=с, 1=мин), шаг 0.1" },
    };
    for (unsigned i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
        uint16_t v;
        esp_task_wdt_reset();
        if (vfd_read_regs(params[i].addr, &v, 1)) {
            ESP_LOGI(TAG, "  %s = %u (0x%04X)", params[i].name, v, v);
        } else {
            ESP_LOGI(TAG, "  %s: привести нельзя", params[i].name);
        }
    }

    /* Толчок едет на P2.19, а не на уставку панели: при P2.19 = 0 кнопка
     * ЗАПРАВКА выглядит сломанной, хотя кадры уходят исправно. */
    uint16_t jog_hz_x100;
    if (vfd_read_regs(0x0213, &jog_hz_x100, 1)) {
        if (jog_hz_x100 == 0)
            ESP_LOGE(TAG, "  P2.19 = 0.00 Гц: толчка не будет ни на каком слове — "
                          "задайте частоту заправки (0x0213, 0.01 Гц)");
        else
            ESP_LOGI(TAG, "  заправка пойдёт на %.2f Гц — это P2.19, её меняет панель привода",
                     jog_hz_x100 / 100.0f);
    }

    /* ======================= 06H и 10H по-честному =======================
     * Эхо записи доказать ничего не может: привод на побитую команду молчит,
     * а на верную возвращает копию запроса вплоть до CRC. Поэтому пишем то,
     * что движение начать не может — СТОП и уже установленную частоту, —
     * и проверяем обратным чтением. Это единственный способ отличить
     * «кадр дошёл» от «наш же кадр вернулся в приёмник». */
    {
        uint16_t f = 0, r = 0, r2[2] = { 0, 0 };
        bool got_f = vfd_read_regs(VFD_REG_FREQ_SET, &f, 1);
        ESP_LOGW(TAG, "разведка 06H/10H: пишем СТОП и текущую уставку (вал не тронется)");

        bool ok6 = write_single(VFD_REG_CMD, VFD_CMD_STOP);
        bool rb6 = vfd_read_regs(VFD_REG_CMD, &r, 1);
        ESP_LOGI(TAG, "  06H 0x2000=СТОП: эхо %s, обратно прочитано 0x%04X -> %s",
                 ok6 ? "есть" : "НЕТ", rb6 ? r : 0,
                 (ok6 && rb6 && r == VFD_CMD_STOP) ? "ПРИНЯТО" : "НЕ ПОДТВЕРЖДЕНО");

        if (got_f) {
            bool ok10 = write_pair(VFD_REG_CMD, VFD_CMD_STOP, f);
            bool rb10 = vfd_read_regs(VFD_REG_CMD, r2, 2);
            ESP_LOGI(TAG, "  10H 0x2000=СТОП + 0x2001=%u: эхо %s, обратно 0x%04X/0x%04X -> %s",
                     f, ok10 ? "есть" : "НЕТ", r2[0], r2[1],
                     (ok10 && rb10 && r2[0] == VFD_CMD_STOP && r2[1] == f)
                         ? "ПРИНЯТО" : "НЕ ПОДТВЕРЖДЕНО");
        } else {
            ESP_LOGW(TAG, "  10H не проверяем: 0x2001 не читается, писать вслепую нельзя");
        }
        uint16_t ech = g_vfd.echo_bytes;
        if (ech)
            ESP_LOGW(TAG, "  внимание: линия возвращает нам %u байт нашей же передачи — "
                          "эхо отброшено, в статистике его нет", ech);
        else
            ESP_LOGI(TAG, "  эха в линии нет: приёмник на время нашей передачи закрыт");
    }
}

/* ======================= Задача-владелец шины =======================
 * Кадров здесь не пересечь: второй хозяин (bus_worker из UI) ждёт мьютекс
 * внутри bus_xfer. Следит за собой через esp_task_wdt: завис — значит сброс,
 * а привод без хостовых кадров уйдёт в Er14 по собственному таймауту P8.02. */
static void vfd_task(void *arg)
{
    static const esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms    = 3 * CONFIG_STANOK_POLL_MS + 8000,
        .trigger_panic = true,
    };
    /* Сначала пробуем перенастроить уже поднятый TWDT (его поднимает LVGL
     * порт — это норма), и только если его нет — инициализируем сами.
     * esp_task_wdt_init() поверх работающего watchdog печатает E-строку,
     * а «псевдо-ошибка» в каждом старте приучает не замечать настоящие. */
    esp_err_t werr = esp_task_wdt_reconfigure(&wdt_cfg);
    if (werr == ESP_ERR_INVALID_STATE) werr = esp_task_wdt_init(&wdt_cfg);
    ESP_ERROR_CHECK(werr);
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    bool pins_locked = (VFD_UART_DE_GPIO >= 0) || !CONFIG_STANOK_RS485_AUTO_PINS;
    bool map_probed = false;
    int64_t probe_after_us = 0;
    uint32_t down_ms = 0;

    while (1) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());

#if CONFIG_STANOK_RS485_AUTO_PINS
        /* Ни одного честного ответа с самого старта — возможно, перепутаны
         * ноги UART. Ищем пару настоящими кадрами, но не чаще раза в 30 с,
         * чтобы не гонять перебор по кругу на оборванной линии. */
        if (!pins_locked && g_vfd.polls_ok == 0 &&
            esp_timer_get_time() >= probe_after_us) {
            uint8_t tx, rx;
            ESP_LOGW(TAG, "привод молчит: перебор пар TX/RX для линии 485");
            if (probe_bus_pins(&tx, &rx)) {
                s_tx_active = tx; s_rx_active = rx;
                g_vfd.uart_tx_gpio = tx;
                g_vfd.uart_rx_gpio = rx;
                pins_locked = true;
                ESP_LOGI(TAG, "линия 485 найдена: TX=GPIO%d RX=GPIO%d", tx, rx);
            } else {
                ESP_LOGE(TAG, "ни одна пара TX/RX не ответила. Проверьте wiring: A/B, "
                              "адаптер 3.3 В, P8.01=9600 8E1, адрес %d", VFD_MODBUS_ADDR);
                probe_after_us = esp_timer_get_time() + 30 * 1000 * 1000;
                uart_set_pin(VFD_UART_NUM, VFD_UART_TX_GPIO, VFD_UART_RX_GPIO,
                             UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            }
        }
#endif

        /* Пара ног известна (или задана руками) — один раз выясняем, какими
         * блоками этот привод вообще разговаривает. */
        if (pins_locked && !map_probed) {
            map_probed = true;
            probe_reg_map();
        }

        /* 1. Сначала чужие команды: оператор важнее телеметрии. */
        vfd_cmd_msg_t msg;
        int handled = 0;
        while (xQueueReceive(s_cmd_q, &msg, 0) == pdTRUE && handled < 4) {
            exec_cmd(&msg);
            handled++;
        }

        /* 2. Затем опрос. */
        poll_telemetry();
        jog_followup();

        /* 3. При потере связи останавливаем сами и не даём ходу из очереди.
         *    Если же умер сам ESP, станок остановит привод по P8.02 (Er14). */
        if (vfd_link_is_down()) {
            down_ms += CONFIG_STANOK_POLL_MS;
            if (down_ms >= 1500) {
                down_ms = 0;
                static const char *const why[] = {
                    "-", "нет ответа", "чужой адрес", "CRC", "функция/длина"
                };
                char frame[sizeof(g_vfd.dbg_frame)];
                for (size_t i = 0; i < sizeof(frame); i++) frame[i] = g_vfd.dbg_frame[i];
                ESP_LOGE(TAG, "связь с ПЧ потеряна — принудительный СТОП | "
                              "опросов OK %lu, таймаутов %lu, битых %lu, ретраев %lu | "
                              "последний сбой: FC%02X %s (ждали %u, пришли %u: [%s])",
                         (unsigned long)g_vfd.polls_ok, (unsigned long)g_vfd.timeouts,
                         (unsigned long)g_vfd.crc_err, (unsigned long)g_vfd.retries,
                         g_vfd.dbg_func, why[g_vfd.dbg_reason & 7],
                         g_vfd.dbg_exp, g_vfd.dbg_got, frame);
                vfd_cmd_stop();
            }
        } else {
            down_ms = 0;
#if CONFIG_STANOK_STOP_ON_BOOT
            /* После рестарта панели привод мог остаться в RUN (например, если
             * панель упала по watchdog во время намотки). Гасим сразу. */
            if (!s_boot_stop_done) {
                s_boot_stop_done = true;
                if (g_vfd.is_running) {
                    ESP_LOGW(TAG, "привод найден запущенным при старте панели — СТОП");
                    vfd_cmd_stop();
                }
            }
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_STANOK_POLL_MS));
    }
}

/* ======================= Инициализация ======================= */
esp_err_t vfd_easydrive_init(void)
{
    s_state_mtx = xSemaphoreCreateMutex();
    s_bus_mtx = xSemaphoreCreateMutex();
    s_cmd_q = xQueueCreate(VFD_CMD_QUEUE_LEN, sizeof(vfd_cmd_msg_t));
    if (!s_cmd_q || !s_state_mtx || !s_bus_mtx) return ESP_ERR_NO_MEM;

    uart_config_t uc = {
        .baud_rate  = VFD_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        /* Привод настроен на 8E1 (P8.01 = 0x13). С UART_PARITY_DISABLE он
         * не отвечает вообще — это измерено, а не догадка. */
        .parity     = UART_PARITY_EVEN,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_param_config(VFD_UART_NUM, &uc), TAG, "uart_param_config");
    ESP_RETURN_ON_ERROR(uart_set_pin(VFD_UART_NUM, VFD_UART_TX_GPIO, VFD_UART_RX_GPIO,
                                     (VFD_UART_DE_GPIO >= 0) ? VFD_UART_DE_GPIO : UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE), TAG, "uart_set_pin");
    if (VFD_UART_DE_GPIO >= 0) {
        /* Аппаратное направление передачи: DE активен на время TX,
         * приём включается до того, как последние биты уйдут в линию. */
        ESP_RETURN_ON_ERROR(uart_set_mode(VFD_UART_NUM, UART_MODE_RS485_HALF_DUPLEX), TAG, "uart_set_mode");
        ESP_LOGI(TAG, "RS-485: DE/RE на GPIO%d, полудуплекс аппаратно", VFD_UART_DE_GPIO);
    } else {
        ESP_LOGI(TAG, "RS-485: направление автоматическое (DE не используется)");
    }
    ESP_RETURN_ON_ERROR(uart_driver_install(VFD_UART_NUM, VFD_UART_BUF, VFD_UART_BUF, 0, NULL, 0),
                        TAG, "uart_driver_install");

    xTaskCreate(vfd_task, "vfd_bus", 4096, NULL, 6, NULL);

    g_vfd.uart_tx_gpio = VFD_UART_TX_GPIO;
    g_vfd.uart_rx_gpio = VFD_UART_RX_GPIO;

    ESP_LOGI(TAG, "ЧП Mini-S: UART%d TX=GPIO%d RX=GPIO%d DE=GPIO%d, %d 8E1, адрес %d, опрос %d мс",
             VFD_UART_NUM, VFD_UART_TX_GPIO, VFD_UART_RX_GPIO, VFD_UART_DE_GPIO,
             VFD_BAUD_RATE, VFD_MODBUS_ADDR, CONFIG_STANOK_POLL_MS);
#if CONFIG_STANOK_RS485_AUTO_PINS
    if (VFD_UART_DE_GPIO < 0)
        ESP_LOGI(TAG, "автоподбор пары TX/RX включён: при молчании привода ноги переберут");
#endif
    return ESP_OK;
}
