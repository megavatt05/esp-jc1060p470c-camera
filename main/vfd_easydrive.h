#pragma once
// =========================================================================
//  Управление частотным преобразователем Easydrive / INVT MINI-S-2S0007M
//  по Modbus RTU через RS-485.
//
//  Карта регистров — из официального мануала cv3100_series.pdf (глава 9) и
//  проверена на живом приводе 2026-10-20. Измеренные особенности привода,
//  которые определили форму этого API:
//
//   1) 9600 8E1 (P8.01 = 0x13). Чётность — часть кадра; с 8N1 привод молчит.
//   2) FC 03H отдаёт не больше 6 регистров: с 7-го начинается мусор внутреннего
//      буфера привода и не сходится CRC. Опрос разбит на блоки 5/3/2/1.
//   3) Привод никогда не шлёт Modbus-исключение. Неверный адрес, неверная
//      функция, кадр с битой CRC — молчание. Значит «нет ответа» и есть
//      единственный канал сообщений об ошибке; отличить «не то понял» от
//      «не ответил» нельзя, поэтому каждое критичное действие проверяется
//      обратным чтением.
//   4) Запись на несуществующий адрес (06H @ 0x0FFF) возвращается эхом как
//      успешная. Успешное эхо НЕ доказывает запись параметра.
//   5) Латентность кадра 29..39 мс — это время линии 9600 8E1, не помеха;
//      0 ошибок на 480 кадрах. Владелец шины один: задача vfd_task.
//
//  Физический слой по схеме платы JC1060P470C V1.0 (5-Schematic, U8):
//  на плате стоит встроенный MAX485 — GPIO26 = TX1 -> R75 -> DI, GPIO27 =
//  RX1 <- R70 <- RO, а /RE и /DE соединены в цепь EN и через R76 (0R)
//  висят на TX1_1, то есть направление переключает автомат на 74LVC1G132
//  и Q3 (8550). Пина DE в программном управлении нет и быть не должно;
//  аппаратного RTS у UART1 на P4 тоже нет — и не нужен. Гребёнка J5 дублирует
//  те же TX1/RX1 + 5V/GND для внешнего адаптера.
//  Раньше в этом проекте сбросом LCD числился GPIO27 — по той же схеме сброс
//  панели сидит на GPIO0, а GPIO27 занят приёмником 485.
// =========================================================================

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Линия RS-485 ---- */
#define VFD_UART_NUM          CONFIG_STANOK_RS485_UART_NUM
#define VFD_UART_TX_GPIO      CONFIG_STANOK_RS485_TX_GPIO
#define VFD_UART_RX_GPIO      CONFIG_STANOK_RS485_RX_GPIO
#define VFD_UART_DE_GPIO      CONFIG_STANOK_RS485_DE_GPIO   /* -1 = авто-направление в адаптере */
#define VFD_BAUD_RATE         CONFIG_STANOK_RS485_BAUD
#define VFD_MODBUS_ADDR       CONFIG_STANOK_VFD_ADDR

#define VFD_MAX_BLOCK_REG     5      /* измеренный безопасный предел чтения */
#define VFD_FRAME_GAP_MS      5      /* T3.5 при 9600 = 4 мс */
#define VFD_TURNAROUND_US     (CONFIG_STANOK_RS485_TURNAROUND_MS * 1000)
#define VFD_RETRY_MAX         3
#define VFD_RETRY_BACKOFF_MS  40

/* ---- Командное слово 0x2000 (мануал, таблицы 3 и 4) ----
 * bit0..1  01 стоп | 10 пуск | 11 JOG
 * bit2..3  00 «run for once» (однократно) | 11 непрерывно
 * bit4..5  01 вперёд | 10 назад | 11 сброс аварии */
#define VFD_CMD_STOP          0x0001u
#define VFD_CMD_FWD_RUN       0x001Eu
#define VFD_CMD_REV_RUN       0x002Eu
#define VFD_CMD_FWD_JOG       0x0013u   /* табл.4: бит2..3=00, толчок однократно */
#define VFD_CMD_REV_JOG       0x0023u
#define VFD_CMD_FWD_JOG_HOLD  0x001Fu   /* тот же толчок, но бит2..3=11 «непрерывно» */
#define VFD_CMD_REV_JOG_HOLD  0x002Fu
#define VFD_CMD_FAULT_RESET   0x0031u

/* ---- Карта адресов ---- */
#define VFD_REG_CMD           0x2000u   /* команда */
#define VFD_REG_FREQ_SET      0x2001u   /* заданная частота, 0.01 Гц */
#define VFD_REG_DIR           0x2002u   /* направление */
#define VFD_REG_D00           0x0D00u   /* d-00 выходная частота, 0.01 Гц */
#define VFD_REG_D06           0x0D06u   /* d-06 .. d-08 */
#define VFD_REG_D15           0x0D0Fu   /* d-15 радиатор, d-16 IGBT */
#define VFD_REG_D17           0x0D11u   /* d-17 счётчик (пригодится для ИДМ-20) */
#define VFD_REG_STATUS        0x0E00u   /* слово состояния, старший байт = номер Er */

/* ---- Биты слова состояния 0x0E00 (мануал, таблица 2) ---- */
#define VFD_ST_VOLT_OK        (1u << 0)
#define VFD_ST_PHASE_REV      (1u << 2)
#define VFD_ST_DIR_REV        (1u << 3)
#define VFD_ST_RUNNING        (1u << 4)
#define VFD_ST_DIGITAL_SET    (1u << 5)
#define VFD_ST_ACCELERATING   (1u << 6)
#define VFD_ST_DECELERATING   (1u << 7)
#define VFD_ST_FAULT          (1u << 15)
#define VFD_ST_FAULT_NUM(s)   (((s) >> 8) & 0x7Fu)

typedef enum {
    VFD_LINK_DEAD = 0,     /* ни одного успешного обмена с момента старта */
    VFD_LINK_OK,           /* опрос идёт штатно */
    VFD_LINK_DEGRADED,     /* ответы есть, но с ретраями: линия деградирует */
} vfd_link_t;

typedef struct {
    /* Телеметрия — прочитана из привода, а не посчитана локально */
    float    out_freq_hz;        /* d-00 */
    float    set_freq_hz;        /* d-01 */
    float    out_cur_a;          /* d-02 */
    float    out_volt_v;         /* d-03 */
    int32_t  rpm;                /* d-04 */
    float    bus_volt_v;         /* d-07 */
    float    input_volt_v;       /* d-08 */
    float    igbt_temp_c;        /* d-16 */
    uint16_t status_word;        /* 0x0E00 */
    bool     is_running;         /* по bit4 статуса, НЕ по факту отправки команды */
    bool     is_fault;
    uint8_t  fault_code;         /* 0 = нет, иначе Er00..Er19 */

    /* Состояние связи: по нему панель решает, можно ли доверять цифрам */
    vfd_link_t link;
    bool     link_stale;         /* старше CONFIG_STANOK_LINK_TIMEOUT_MS */
    uint32_t polls_ok;
    uint32_t timeouts;           /* истечения таймаута */
    uint32_t crc_err;            /* битые кадры — прямой индикатор наводок */
    uint32_t retries;
    uint32_t cmd_ok;
    uint32_t cmd_err;
    uint16_t latency_ms;         /* скользящее среднее по обменам */
    int64_t  last_ok_us;         /* время последнего хорошего ответа */
    uint16_t last_cmd;           /* последнее командное слово */
    bool     last_cmd_acked;     /* привод ответил эхом */
    bool     last_cmd_confirmed; /* и подтвердил его чтением состояния */
    uint16_t cmd_readback;       /* что реально лежит в 0x2000 после записи */
    bool     cmd_readback_ok;    /* чтение 0x2000 совпало с записанным */
    uint16_t echo_bytes;         /* максимум байт, принятых во время НАШЕЙ
                                  * передачи: ненулевой = линия 485 возвращает
                                  * нам собственный кадр, эхо не является
                                  * ответом привода */
    uint8_t  uart_tx_gpio;       /* реально работающая пара ног: её подбирает
                                 * probe_bus_pins(), если в menuconfig ошибка */
    uint8_t  uart_rx_gpio;

    /* Разбор последнего неудачного обмена: по этим полям видно, молчит ли
     * привод (got=0) или отвечает чем-то непонятным (got>0, reason=3/4). */
    uint8_t  dbg_reason;         /* 1 нет ответа, 2 чужой адрес, 3 CRC, 4 функция/длина */
    uint8_t  dbg_func;           /* функция запроса, на котором всё и сломалось */
    uint16_t dbg_got;            /* байт принято */
    uint16_t dbg_exp;            /* байт ожидалось */
    char     dbg_frame[32];      /* первые байты ответа в hex */
} vfd_state_t;

extern volatile vfd_state_t g_vfd;

/** Инициализация UART и запуск задачи-владельца шины. */
esp_err_t vfd_easydrive_init(void);

/** true, если связь потеряна дольше CONFIG_STANOK_LINK_TIMEOUT_MS. */
bool vfd_link_is_down(void);

/* ---- Неблокирующие команды: безопасны из задачи LVGL по касанию ----
 * Ставятся в очередь владельцу шины. Команда «стоп» вытесняет из очереди
 * более ранние команды движения, чтобы после всплеска наводок станок не
 * исполнил устаревшее намерение оператора. */
void vfd_cmd_set_frequency(float hz);            /* только 0x2001 */
void vfd_cmd_run_fwd(float hz);                  /* 10H: 0x2000=0x001E + 0x2001 */
void vfd_cmd_run_rev(float hz);                  /* 10H: 0x2000=0x002E + 0x2001 */
void vfd_cmd_jog(bool reverse, float hz);        /* 0x0013 / 0x0023 */
void vfd_cmd_stop(void);                         /* 0x0001, рамп-стоп */
void vfd_cmd_reset_fault(void);                  /* 0x0031 */

/* ---- Прямые блокирующие обращения: только из служебных задач, не из UI ---- */
bool vfd_read_regs(uint16_t reg, uint16_t *out, uint8_t count);
bool vfd_write_reg(uint16_t reg, uint16_t value);

#ifdef __cplusplus
}
#endif
