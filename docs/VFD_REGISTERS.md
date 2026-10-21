# Карта регистров частотника Easydrive Mini-S (CV3100) по Modbus RTU

Полное адресное пространство преобразователя, с которым работает эта панель: мониторинг
`d-00..d-28`, слово состояния `0x0E00`, командное слово `0x2000` и ВСЕ параметры
`P0..PA` (глава 5 и главы 9 таблицы 1..4 мануала `cv3100_series.pdf`).
Последняя колонка параметров — что из них реально трогает прошивка панели.

Правило адреса (табл. 1): параметр `P`g`.`nn` -> `0xGGnn`, мониторинг d-`nn` -> `0x0Dnn`.
Старший байт в кадре идёт первым, CRC — младший байт вперёд.

| Что | Формат адреса | Примеры |
|---|---|---|
| Параметр P`g`.`nn` | `GGnnH` — группа, номер | P0.01 → `0001H`, P0.14 → `000EH`, P1.03 → `0103H`, P2.19 → `0213H`, P8.02 → `0802H`, PA.00 → `0A00H` |
| Мониторинг d-`nn` | `0DnnH` | d-01 → `0D01H`, d-14 → `0D0EH`, d-28 → `0D1CH` |
| Слово состояния | `0E00H` | см. ниже |
| Команда | `2000H` | см. ниже |
| Заданная частота связи | `2001H` | 0.01 Гц, пишется и по 06H, и по 10H |
| Направление | `2002H` | читается, обычно не нужен |

## Мониторинг `0x0D00..0x0D1C` (гл. 5, стр. 5-17/5-18)

| Регистр | Адрес | Что | Единица | Панель |
|---|---|---|---|---|
| d-00 | 0D00H | выходная частота | 0.01 Гц | FC03 0x0D00 x5, опрос 200 мс |
| d-01 | 0D01H | заданная частота | 0.01 Гц | то же чтение x5 |
| d-02 | 0D02H | ток выхода | 0.01 А | то же чтение x5 |
| d-03 | 0D03H | напряжение выхода | 1 В | то же чтение x5 |
| d-04 | 0D04H | обороты двигателя | об/мин (знак — направление) | то же чтение x5 |
| d-05 / d-06 | 0D04H/0D06H | линейная скорость факт / уставка | м/с | FC03 0x0D06 x1/x3 в разведке линии |
| d-07 | 0D07H | напряжение звена постоянного тока | 1 В |  |
| d-08 | 0D08H | напряжение входа | 1 В |  |
| d-09 / d-10 | 0D09H/0D0AH | PID задан / обратная связь | — |  |
| d-11 / d-12 | 0D0BH/0D0CH | AI1 (В) / AI2 (мА) | — |  |
| d-13 | 0D0DH | частота импульсного входа | кГц |  |
| d-14 | 0D0EH | состояние дискретных входов | битовое |  |
| d-15 / d-16 | 0D0FH/0D10H | радиатор / IGBT | °C (0.1) | FC03 0x0D0F x2, опрос t радиатора; FC03 0x0D0F x2, IGBT 0.1 C |
| d-17 / d-18 | 0D11H/0D12H | счётчик текущий / уставкa | — | FC03 0x0D11 x1, задел под ИДМ-20 |
| d-19..d-21 | 0D13H..0D15H | три последних кода аварии | — | ОБНОВИТЬ ЖУРНАЛ: 0x0D13 x3 |
| d-22..d-27 | 0D16H..0D1BH | параметры в момент последней аварии (частота, уставка, ток, напряжение, звено, IGBT) | как у d-00.. | ОБНОВИТЬ ЖУРНАЛ: 0x0D16 x6 |
| d-28 | 0D1CH | версия прошивки | — |  |

Авария хранится в d-19..d-21 и в старшем байте `0E00H`; `Er10` и `Er19` — один и тот
же перегрев радиатора (в таблице мануала записаны как `Er10 (Er19)`).

## Слово состояния `0x0E00` (табл. 2)

Младший байт — состояние, старший — авария.

| Бит | 0 | 1 |
|---|---|---|
| 0 | напряжение ненормальное | напряжение в норме |
| 1 | — | резерв |
| 2 | порядок фаз обратный | прямой |
| 3 | команда назад | команда вперёд |
| 4 | **стой** | **работаю** |
| 5 | — | канал задания частоты = 1 (цифровое задание) |
| 6 | — | разгон |
| 7 | — | торможение |
| 8..14 | — | номер аварии (Er**NN**) |
| 15 | — | есть авария |

В покое на стенде читается `0x0001`. Пример из мануала: `0x9321` = бит15 + номер
`0x13` = 19 → `Er19` (перегрев), младший байт `0x21` = напряжение в норме + канал 1.

## Командное слово `0x2000` (табл. 3 + табл. 4)

| Биты | Значения |
|---|---|
| 0..1 | `00` нет функции · `01` СТОП · `10` ПУСК · `11` **ТОЛЧОК (JOG)** |
| 2..3 | `00` **однократно (run for once)** · `11` непрерывно |
| 4..5 | `00` нет функции · `01` вперёд · `10` назад · `11` сброс аварии |
| 6..15 | резерв |

Слова из табл. 4 (официальные): `0001H` стоп · `001EH` ход вперёд · `002EH` ход назад ·
`0013H` толчок вперёд · `0023H` толчок назад · `0031H` сброс аварии.

Проверка разрядов: `0x1E` = `01 11 10` = вперёд + непрерывно + пуск; `0x01` = `00 00 01`
= стоп. Отсюда же **«толчок, который держится»**: `0x1F` = `01 11 11` (вперёд,
непрерывно, JOG) и `0x2F` = `10 11 11` (назад). Слово `0x0013` по табл. 3 имеет
бит2..3 = `00` = «run for once»: привод сам делает разгон-выдержку-торможение
(P2.20/P2.19/P2.21) и останавливается, а число в `0x2000` при этом не меняется —
повторная запись той же цифры не даёт фронта и толчка не будет.

Частоту толчка задаёт **P2.19** (`0x0213`), а не `0x2001` (гл. 4, стр. 4-2: «runs at jog
frequency, see P2.19~P2.21»). `0x2001` влияет на обычный ход.

`10H` умеет менять частоту одновременно с командой (примечание к табл. 3 и пример на
стр. 9-19: `2000H=001EH` + `2001H=1088H` одним кадром). Ответ на `10H` — только первые
6 байт запроса + их собственный CRC, а не эхо всего кадра.

## Правило разделения на целую и дробную часть

Вес слова задаёт не экран и не здратый смысл, а колонка «единица» мануала.
Регистр всегда целочисленный 16-бит; дробь — это фиксированная запятая:

| Единица в мануале | Сырое слово | sep_pos (знаков после точки) | что рисовать |
|---|---|---|---|
| `1 Гц`, `1 В`, `об/мин`, код аварии, выбор 0..N | 7000 | 0 | `7000` (никакой точки) |
| `0.1 с`, `0.1 %`, `0.1 °C` | 104 | 1 | `10.4` |
| `0.01 Гц`, `0.01 А` | 7000 | 2 | `70.00` |

В LVGL-спинбокте это `lv_spinbox_set_digit_format(int_digits + sep_pos, sep_pos)`;
`sep_pos = 0` означает «точки нет», а не «одна цифра после точки».
Обратное преобразование при записи: `raw = (uint16_t)(значение × вес + 0.5)`.

Две вещи, которые по одному числу не выводятся:

- **Единица времени разгона/торможения выбирается параметром.** Шаг остаётся
  `0.1`, но от чего — решает `P3.09` (`0309H`): 0 = секунды, 1 = минуты,
  заводское значение — **минуты**. Сырое 104 = 10.4 с только при P3.09=0,
  иначе это 10.4 мин. На стенде MINI-S-2S0007M fw V2.03 замерено 2026-10-21:
  **P3.09 = 0**, значит 10.4 с и 20.0 с. Тот же шаг относится к P0.14/P0.15,
  P2.20/P2.21 и P2.22..P2.27.
- **Частоты ниже `P0.07` не бывает.** `P0.07` (нижний предел) поднимает малые
  уставки: на стенде он 10.00 Гц, поэтому «5 Гц» превращаются в 10.00 — это не
  потеря кадра.

## Параметры `P0..PA` (глава 5, стр. 5-1..5-16)

### P0 — базовые (Basic)

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P0.00 | 0000H | Control mode 0: Sensorless vector control 1: V/F control 1 1 | × |  |
| P0.01 | 0001H | Frequency input channel selection 0: Panel potentiometer setting 1: Keypad digit setting by ▲/▼ or digital encoder 2: UP/DOWN terminal digit setting 3: Communication setting 4: AI1 analog voltage signal setting(0..10V) 5: AI2 analog current signal setting(0..20mA) 6: Terminal pulse(0..10KHZ) 7: Compounding setting (AI1+AI2) 8: External terminal selection 1 0 | ○ | читает и пишет (экран ПАРАМЕТРЫ) |
| P0.02 | 0002H | Digital frequency control LED single unit: memory during power off 0: storage 1: not storage LED decimal unit: holding during stop status 0: holding 1: not holding LED hundred & thousand unit: Reserved (Note: Only valid to P0.01=1,2, 3) 1 00 | ○ |  |
| P0.03 | 0003H | Frequency setting 0.00..Upper limit frequency 0.01Hz 50.00Hz | ○ |  |
| P0.04 | 0004H | Running command selection 0: Keypad control available 1: External terminal control available 2: Serial port control available 1 0 | ○ | читает в разведке на старте |
| P0.05 | 0005H | Run direction setting 0: Forward 1: Reverse 2: Reverse prevent 1 0 | ○ |  |
| P0.06 | 0006H | Upper limit frequency ｛P0.07｝..400.00Hz 0.01Hz 50.00Hz | × | читает и пишет (экран ПАРАМЕТРЫ) |
| P0.07 | 0007H | Lower limit frequency 0.00..【P0.06】 0.01Hz 00.00Hz | × | читает и пишет (экран ПАРАМЕТРЫ) |
| P0.08 | 0008H | Basic running frequency 1.00..Upper limit frequency 0.01Hz 50.00Hz | × | читает и пишет (экран ПАРАМЕТРЫ) |
| P0.09 | 0009H | Max.output voltage 200..500V 100..250V 1 380V 220V | × |  |
| P0.10 | 000AH | Model choose 0: G type(Constant torque load) 1: FP type(Fan, pump load) 1 0 | × |  |
| P0.11 | 000BH | Torsion rising selection 0: Manual 1: Automatic 1 0 | × |  |
| P0.12 | 000CH | Torsion rising setting 0.0..30.0％ Note: available when F0.11=0 0.1 | ○ |  |
| P0.13 | 000DH | Rotation deviation frequency compensation 0.0..150.0％ 0.1 0.0 | ○ |  |
| P0.14 | 000EH | Acceleration time 1 0.1..3600S Accel&Decel time unit reference P3.09) 0.1 | ○ | читает и пишет (экран ПАРАМЕТРЫ) |
| P0.15 | 000FH | Deceleration time 1 0.1 | ○ | читает и пишет (экран ПАРАМЕТРЫ) |
| P0.16 | 0010H | V/F curve setting 0: constant torsion curve 1: decreasing torsion curve 1 (1.7 hypo-power) 2: decreasing torsion curve 2 (2.0 hype-power) 3: Customer self-defined V/F curve mode(determined by P0.17..P0.22) 1 0 | × |  |
| P0.17 | 0011H | V/F freq. value F1 0.00..frequency value F2 0.01Hz 12.50Hz | × |  |
| P0.18 | 0012H | V/F volt. value V1 0.0..voltage value V2 0.1% 25.0％ | × |  |
| P0.19 | 0013H | V/F freq. value F2 Freq. value F1..Freq. value F3 0.01Hz 25.00Hz | × |  |
| P0.20 | 0014H | V/F volt. value V2 Volt. value V1..Volt. value V3 0.1% 50.0％ | × |  |
| P0.21 | 0015H | V/F freq. value F3 Freq. value F2..basic running frequency 0.01Hz 37.50Hz | × |  |
| P0.22 | 0016H | V/F volt. Value V3 Voltage value V2..100.0％ 0.1% 75.0％ | × |  |
| P0.23 | 0017H | REV/JOG function selection 0: REV 1: JOG 1 1 | ○ | читает в разведке на старте |

### P1 — двигатель и вектор

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P1.00 | 0100H | Motorrated voltage 380V: 200..500V 220V: 100..250V 1V 380V 220V | ○ |  |
| P1.01 | 0101H | Motor rated current 0.1..600.0A 0.1A | ○ |  |
| P1.02 | 0102H | Motor rated rev. 300..3000RPM 1RPM | × |  |
| P1.03 | 0103H | Motor rated freq. 1.00..400.00Hz 0.01Hz 50.00Hz | × |  |
| P1.04 | 0104H | Motor no-loading current 0.1..500.0A 0.1A | ○ |  |
| P1.05 | 0105H | Motor stator resistance 0.001..10.000Ω 0.001 | × |  |
| P1.06 | 0106H | Motor rotor resistance 0.001..10.000Ω 0.001 | × |  |
| P1.07 | 0107H | Motor inductance 0.01..600.00mH 0.01mH | × |  |
| P1.08 | 0108H | Motor mutual inductance 0.01..600.00mH 0.01mH | × |  |
| P1.09 | — | Reserved | ? |  |
| P1.10 | 010AH | Rotation deviation compensation 0.50-2.00 0.01 1.00 | ○ |  |
| P1.11 | 010BH | Pre-excitation of-motor selection 0: conditionally available 1: available all along 1 0 | × |  |
| P1.12 | 010CH | Pre-excitation continuing time 0.1..10S 0.1 0.2S | × |  |
| P1.13 | 010DH | Motor auto tuning 0: no action 1: statictuning (available P0.00=0) 1 0 | × |  |
| P1.14 | 010EH | Auto speed regulation (ASR) proportion (P) gain 0.01..5.00 0.01 1.00 | ○ |  |
| P1.15 | 010FH | ASR integration (I) time constant 0.01..10.00S 0.01s 2.00S | ○ |  |

### P2 — вспомогательные

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P2.00 | 0200H | Starting mode 0: start with starting freq. 1: reserved 1 0 | × |  |
| P2.01 | 0201H | Starting freq. 0.00..10.00Hz 0.01Hz 1.00Hz | ○ |  |
| P2.02 | 0202H | Starting freq. duration 0.0..10.0s 0.1s 0.0s | × |  |
| P2.03 | 0203H | DC braking current level at starting 0.0..100.0％ 0.1% 0.0％ | ○ |  |
| P2.04 | 0204H | DC braking time at starting 0.1..30.0s (0: immovability of DC braking) 0.1s 0.0s | × |  |
| P2.05 | 0205H | Accel.&Decel .way 0: Beeline 1: S-curve 1 0 | ○ |  |
| P2.06 | 0206H | S-curve starting duration proportion 10.0..40.0％ 0.1% 20.0％ | × |  |
| P2.07 | 0207H | S-curve ascend/descend duration proportion 10.0..80.0％ 0.1% 60.0％ | × |  |
| P2.08 | 0208H | AVR function 0: inhibitive 1: allowable 1 1 | × |  |
| P2.09 | 0209H | Automatic energy saving operation 0: inhibitive 1: allowable 1 0 | × |  |
| P2.10 | 020AH | Forward andreverser Rotation dead-zone interval 0.1..10.0s 0.1s 0.0s | × |  |
| P2.11 | 020BH | Stop mode 0: ramp to stop 1: coast to stop 1 0 | × |  |
| P2.12 | 020CH | Starting frequency for DC braking during stop 0.00..20.00Hz 0.01Hz 0.00Hz | ○ |  |
| P2.13 | 020DH | DC braking current during stop 0.0..100.0％ 0.1% 0.0％ | ○ |  |
| P2.14 | 020EH | DC braking time during STOP 0.1..30.0s (0: No action during DC Braking) 0.1s | × |  |
| P2.15 | 020FH | Power-off restart setting 0: inhibitive 1: normal start 2: reserved 1 1 | × |  |
| P2.16 | 0210H | Restart waiting time after power-off 0.0..20.0s 0.1s 0.5s | × |  |
| P2.17 | 0211H | Failure self-reset times 0..10 1 0 | × |  |
| P2.18 | 0212H | Failure self-reset interval 0.5..25.0s 0.1s 3.0s | × |  |
| P2.19 | 0213H | Jog running freq. 0.00..50.00Hz 0.01Hz 10.00Hz | ○ | читает и пишет (экран ПАРАМЕТРЫ) |
| P2.20 | 0214H | Jog accel. time 0.1..3600srefer to P3.09 0.1s | ○ | читает в разведке на старте |
| P2.21 | 0215H | Jog decel. time 0.1s | ○ | читает в разведке на старте |
| P2.22 | 0216H | Accel. time 2 0.1..3600srefer to P3.09 0.1s | ○ |  |
| P2.23 | 0217H | Decel. Time 2 0.1s | ○ |  |
| P2.24 | 0218H | Accel. time 3 0.1..3600srefer to P3.09 0.1s | ○ |  |
| P2.25 | 0219H | Decel. Time 3 0.1s | ○ |  |
| P2.26 | 021AH | Accel. time 3 0.1..3600srefer to P3.09 0.1s | ○ |  |
| P2.27 | 021BH | Decel. Time 3 0.1s | ○ |  |
| P2.28 | 021CH | The 1st step freq. 0.00..Upper limit freq. 0.01Hz 5.00Hz | ○ |  |
| P2.29 | 021DH | The 2nd step freq. 0.00..Upper limit freq. 0.01Hz 10.00Hz | ○ |  |
| P2.30 | 021EH | The 3rd step freq. 0.00..Upper limit freq. 0.01Hz 15.00Hz | ○ |  |
| P2.31 | 021FH | The 4th step freq. 0.00..Upper limit freq. 0.01Hz 20.00Hz | ○ |  |
| P2.32 | 0220H | The 5th step freq. 0.00..Upper limit freq. 0.01Hz 25.00Hz | ○ |  |
| P2.33 | 0221H | The 6th step freq. 0.00..Upper limit freq. 0.01Hz 30.00Hz | ○ |  |
| P2.34 | 0222H | The 7th step freq. 0.00..Upper limit freq. 0.01Hz 40.00Hz | ○ |  |
| P2.35 | — | Reserved | ? |  |
| P2.36 | 0224H | Skip freq.1 0.00..Upper limit freq. 0.01Hz 0.00Hz | ○ |  |
| P2.37 | 0225H | Skip freq. band 1 0.0..10.0Hz 0.01Hz 0.00Hz | ○ |  |
| P2.38 | 0226H | Skip freq.2 0.00..Upper limit freq. 0.01Hz 0.00Hz | ○ |  |
| P2.39 | 0227H | Skip freq. band 2 0.0..10.0Hz 0.01Hz 0.00Hz | ○ |  |
| P2.40 | 0228H | Skip freq.3 0.00..Upper limit freq. 0.01Hz 0.00Hz | ○ |  |
| P2.41 | 0229H | Skip freq. band 3 0.0..10.0Hz 0.01Hz 0.00Hz | ○ |  |
| P2.42 | 022AH | Carrier freq. selection 1.0..12.0KHz 0.1KHz | ○ |  |
| P2.43 | 022BH | Carrier control method 0: fixed carrier 1: automatic adjustment 1 1 | ○ |  |

### P3 — интерфейс оператора

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P3.00 | 0300H | LCD language selection 0: Chinese 1: English 1 0 | ○ |  |
| P3.01 | 0301H | Parameter initialization 0: disabled 1: revert to factory setting 2: eliminate fault record 1 0 | × |  |
| P3.02 | 0302H | Parameter read-in protection 0: allowed to modify all parameters (some of which can not be changed during inverter running) 1: only allow to change the frequency 2: forbidden to modify all parameters 1 0 | ○ |  |
| P3.03 | 0303H | Factory password 0..9999 1 0 | ○ |  |
| P3.04 | 0304H | Monitoring parameter 1 selection 0..18 1 0 | ○ |  |
| P3.05 | 0305H | Monitoring parameter 2 selection 0..18 1 1 | ○ |  |
| P3.06 | 0306H | Linear velocity quotiety 0.01..100.0 0.01 1.00 | ○ |  |
| P3.07 | 0307H | Close loop display quotiety 0.01..100.0 0.01 1.00 | ○ |  |
| P3.08 | 0308H | Software edition 0..99.99 0.01 | × |  |
| P3.09 | 0309H | Accel. & Decel. time unit 0: Second 1: Minute 1 0 | ○ | читает в разведке на старте |

### P4 — дискретные входы/выходы

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P4.00 | 0400H | Input terminal DI1 function 0: Leave control terminal unused 1: Multi-step speed definition 1 2: Multi-step speed definition 2 3: Multi-step speed definition 3 4: Accel. & Decel. 1 5: Accel. & Decel. 2 6: Freq. channel selection 1 7: Freq. channel selection 2 8: Freq. channel selection 3 9: Forward jog control 10: Reverse jog control 11: Inverter running forbidden command 12: Freq. increasing command 13: Freq. decreasing command 14: Peripheral failure input 15: Three-line operation control 16: DC braking command 17: Counter reset signal input 18: Counter trigger signal input (Available to DI6) 19: Exterior pulse input (Available to DI6) 20: Exterior reset signal input 21: UP/DOWN terminal frequency reset 22: PID operation available 23: Programmable multi-step speed running available 24: Wobbling frequency operation available selection 25: Wobbling freq. status reset 26: Exterior stop command 27: Free stop control 28: Reserve 29: Command channel input switched to terminal 30: Freq. input channel switched to AI2 31: Timer start 32: Timer reset 1 0 | × |  |
| P4.01 | 0401H | Input terminal DI2 function 1 0 | × |  |
| P4.02 | 0402H | Input terminal DI3 function 1 0 | × |  |
| P4.03 | 0403H | Input terminal DI4 function 1 0 | × |  |
| P4.04 | 0404H | Input terminal DI5 function 1 0 | × |  |
| P4.05 | 0405H | Input terminal DI6 function 1 0 | × |  |
| P4.06 | 0406H | FWD/REV terminal control method 0: Two-line control mode 1 1: Two-line control mode 2 2: Three-line control mode 1 3: Three-line control mode 2 (reserved) 1 0 | × |  |
| P4.07 | 0407H | Open collector terminal DO1 output 0: Inverter running indication 1: Freq./speed arrival signal (FAR) 2: Freq./speed detecting signal (FDT) 3: Inverter zero speed running indication 4: Peripheral failure stop 5: Output freq. arriving at the upper limit 6: Output freq. arriving at the lower limit 7: Programmable multi-step speed circular running finished 8: Inverter overload alarming signal 9: Inverter starts to run in train 10:Counter checkout signal output 11:Counter reset signal output 12: Inverter failure 13:less-voltage stopping 14: Wobbling frequency fluctuationlimit 15: Programmable multi-step speed running finished. 16: Reserved 17: Timing to output 1 0 | ○ |  |
| P4.08 | 0408H | Open collector terminal DO2 output 1 1 | ○ |  |
| P4.09 | 0409H | Programmabl e relay output 1 12 | ○ |  |
| P4.10 | 040AH | FDT level setting 0.00Hz..Upper limit frequency 0.01Hz 10.00Hz | ○ |  |
| P4.11 | 040BH | FDT lag value 0.0..30.00Hz 0.1Hz 1.00Hz | ○ |  |
| P4.12 | 040CH | Freq. arrival FAR checkout width 0.00Hz..15.0Hz 0.01Hz 5.00Hz | ○ |  |
| P4.13 | 040DH | Overload pre-alarm checkout level 20..120％ 1% 100％ | ○ |  |
| P4.14 | 040EH | Overload pre-alarm delayed time 0.0..15.0s 0.1s 1.0s | × |  |
| P4.15 | 040FH | Counter reset value 【P4.16】..60000 1 1 | × |  |
| P4.16 | 0410H | Counter checkout value 0..【P4.15】 1 1 | × |  |

### P5 — аналог. входы/выходы

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P5.00 | 0500H | AI1 input voltage of lower limit 0.0..【P5.01】 0.1V 0.0V | ○ |  |
| P5.01 | 0501H | AI 1input voltage of upper limit 【P5.00】..10.0V 0.1V 10.0V | ○ |  |
| P5.02 | 0502H | AI2 input current of lower limit 0.0..【P5.03】 0.1mA 0.0mA | ○ |  |
| P5.03 | 0503H | AI2 input current of upper limit 【P5.02】..20.0mA 0.1mA 20.0mA | ○ |  |
| P5.04 | 0504H | Pulse input frequency of lower limit 0.0..【P5.05】 0.1KHz 0.0KHz | ○ |  |
| P5.05 | 0505H | Pulse input frequency of upper limit 【P5.04】..20.0kHz 0.1KHz 10.0KHz | ○ |  |
| P5.06 | 0506H | Setting frequency to minimum input 0.0Hz..upper frequency 0.01Hz 0.00Hz | ○ |  |
| P5.07 | 0507H | Setting frequency to maximum input 0.0Hz..upper frequency 0.01Hz 50.00Hz | ○ |  |
| P5.08 | 0508H | Delayed time to analog input signal 0.1..5.0s 0.1s 0.5s | ○ |  |
| P5.09 | 0509H | AO1 multi-function al analog output terminal function selection 0: Output frequency 1: Setting frequency 2: Output current 3: Motor rev. 4: Output voltage 5: Bus voltage 6: PIDration 7: PID feedback 1 0 | ○ |  |
| P5.10 | 050AH | DO3 multi-function al pulse output terminal function selection 1 2 | ○ |  |
| P5.11 | 050BH | AO1 gain setting 20..200% 1% 100％ | ○ |  |
| P5.12 | 050CH | Reserved | ? |  |
| P5.13 | 050DH | DO3 gain setting 20..200%(Rating 10KHZ) 1% 100％ | ○ |  |
| P5.14 | 050EH | Reserved | ? |  |
| P5.15 | 050FH | Compounding specified channel setting LED single digit:Operand 1 0: Keypadpotentiometer; 1: Digit setting 2: reserved 3: Communication setting 4: AI1 5: AI2 6: Terminal pulse. LED tens digit: Operand 2 0: Keypadpotentiometer 1: Digit setting 2: reserved 3: Communication setting 4: AI1 5: AI2 6: Terminal pulse LED Hundreds digit: Operand 3 0: Keypadpotentiometer 1: Digit setting 2: reserved 3: Communication setting 4: AI1 5: AI2 6: Terminal pulse LED Thousands digit: Reserved 1 000 | × |  |
| P5.16 | 0510H | Compounding specified arithmetic setting LED single digit: Arithmetic 1 0: Addition 1: Subtraction 2: Absolute value(Subtraction) 3: Choose Maximum. 4: Choose Minimum. LED tens digit: Arithmetic 2 0: Addition 1: Subtraction 2: Absolute value(Subtraction) 3: Choose Maximum. 4: Choose Minimum. 5: Operand 3 not be concerned with operation LED Hundreds & thousands digit: Reserved 1 00 | ○ |  |

### P6 — PID

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P6.00 | 0600H | PID action setting LED single digit: Function setting 0: Close 1: Open LED tens digit: PIDavailability selection 0: Automatic availability 1: Manual availability by defining of multi-function terminal LED Hundreds & thousands digit: Reserved 1 00 | × |  |
| P6.01 | 0601H | PID specified channel selection 0: Keypad potentiometer 1: Digit setting 2: Reserved 3: Reserved 4: AI1 provision 5: AI2 provision 6: Terminal pulse 7: AI1+AI2 8: AI1-AI2 9: MIN｛AI1,AI2｝ 10:MAX｛AI1,AI2｝ 1 1 | × |  |
| P6.02 | 0602H | PID feedback channel selection 1 4 | × |  |
| P6.03 | 0603H | Specified digital quantity setting 0.00..10.00V 0.01V 0.00V | ○ |  |
| P6.04 | 0604H | Gain of feedback channel 0.01..10.00 0.01 1.00 | ○ |  |
| P6.05 | 0605H | Polarity of feedback channel 0: Positive 1 0 | × |  |
| P6.06 | 0606H | Proportion gain P 0.01..10.00 0.01 1.00 | ○ |  |
| P6.07 | 0607H | Integral time Ti 0.1..200.0s 0.1s 1.0s | ○ |  |
| P6.08 | 0608H | Differential coefficient time Td 0.0: No 0.1..10.0s 0.1s 0.0s | ○ |  |
| P6.09 | 0609H | Sampling time T 0.00: Automatic 0.01..10.00s 0.01s 0.00s | ○ |  |
| P6.10 | 060AH | Deviation margin 0.0..20.0％ 0.1% 0.0％ | ○ |  |
| P6.11 | 060BH | Closed-loo p presetting frequency 0.0..Upper limit frequency 0.01Hz 0.00Hz | ○ |  |
| P6.12 | 060CH | Duration of presetting frequency 0.0..6000.0s 0.1s 0.0s | × |  |
| P6.13 | 060DH | Sleeping threshold 0.0..10.00V 0.01 10.00V | ○ |  |
| P6.14 | 060EH | Waking threshold 0.0..10.00V 0.01 0.00V | ○ |  |
| P6.15 | 060FH | Sleep/Wake waiting time 0.1..600.00S 0.1 300.0S | ○ |  |

### P7 — программируемый ход

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P7.00 | 0700H | Programma ble running control ( includes simple PLC and wobbling frequency running) LED single digit: Running mode selection 0: No action 1: Single circulation(Simple PLC) 2: Continuous circulation(Simple PLC) 3: Holding ultimate value after single circulation(Simple PLC) 4: Wobbling frequency running LED tens digit: Programmable multi-step speed (PLC)runningavailability selection 0: Automatic availability 1: Manual availability by defining multi-function terminal LED Hundreds digit: Wobbling frequency running availability mode 0: Automatic availability 1: Manual availability by defining of multi-function terminal. LED thousands digit: Reserved 1 000 | × |  |
| P7.01 | 0701H | Stage 1 running time 0.0..6000.0s 0.1s 10.0s | ○ |  |
| P7.02 | 0702H | Stage 2 running time 0.0..6000.0s 0.1s 10.0s | ○ |  |
| P7.03 | 0703H | Stage 3 running time 0.0..6000.0s 0.1s 10.0s | ○ |  |
| P7.04 | 0704H | Stage 4 running time 0.0..6000.0s 0.1s 10.0s | ○ |  |
| P7.05 | 0705H | Stage 5 running time 0.0..6000.0s 0.1s 10.0s | ○ |  |
| P7.06 | 0706H | Stage 6 running time 0.0..6000.0s 0.1s 10.0s | ○ |  |
| P7.07 | 0707H | Stage 7 running time 0.0..6000.0s 0.1s 10.0s | ○ |  |
| P7.08 | — | Reserved | ? |  |
| P7.09 | 0709H | Multi-step speed running direction 1 LED single digit: Stage 1 direction 0: Forward 1: Reverse LED tens digit: Stage 2 direction 0: Forward 1: Reverse LED hundreds digit: Stage 3 direction 0: Forward 1: Reverse LED thousands digit: Stage 4 direction 0: Forward 1: Reverse 1 0000 | ○ |  |
| P7.10 | 070AH | Multi-step speed running direction 2 LED single digit: Stage 5 direction 0: Forward 1: Reverse LED tens digit: Stage 6 direction 0: Forward 1: Reverse LED hundreds digit: Stage 7 direction 0: Forward 1: Reverse LED thousands digit: Reserved 1 000 | ○ |  |
| P7.11 | 070BH | Wobbling frequency running mode LED single digit: Reserved LED tens digit: Wobbling range control 0: Fixed 1: Variable LED hundreds digit: Selection of wobbling frequency start up mode after power off 0: Start up according to memory before stop 1: Restart LED thousands digit: Selection of Wobbling frequency status storage after power off 0: Storing after power off 1: Not storing 1 000 | × |  |
| P7.12 | 070CH | Wobbling pre-setting frequency 0.00Hz..Upper limit freq. 0.01Hz 10.00Hz | ○ |  |
| P7.13 | 070DH | Wobbling presetting frequency waiting time 0.0..3600.0s 0.1s 0.0s | × |  |
| P7.14 | 070EH | Wobbling frequency width 0.0..50.0％ 0.1% 10.0％ | ○ |  |
| P7.15 | 070FH | Skip frequency 0.0..50.0％(relate to wobbling frequency range) 0.1% 10.0％ | ○ |  |
| P7.16 | 0710H | Wobbling freq. period 0.1..3600.0s 0.1s 10.0s | ○ |  |
| P7.17 | 0711H | Triangle wave risetime 0.0..100.0％(Wobbling period) 0.1% 50.0％ | ○ |  |
| P7.18 | 0712H | Norm of central wobbling frequency 0.00Hz..Upper limit feq. 0.01Hz 10.00Hz | ○ |  |

### P8 — связь

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P8.00 | 0800H | Local communica tion address 0: Host station 1..31: Ancillary station 1 1 | × |  |
| P8.01 | 0801H | Communic ation scheme LED single digit: Baud rate selection 1: 2400BPS 2: 4800BPS 3: 9600BPS 4: 19200BPS 5: 38400BPS LED tens digit: Data form 0: No checkout 1: Even checkout 2: Odd checkout LED hundreds digit: Reserved LED thousands digit: 0: return information when modify data 1: no return information when modify data 1 013 | × |  |
| P8.02 | 0802H | Communic ating overtime checkout time 0.0..100.0s 0.1s 10.0s | × | читает в разведке на старте |
| P8.03 | 0803H | Response delay 0..1000ms 1ms 5ms | × |  |
| P8.04 | 0804H | Linkage proportion setting 0.01..10.00 0.01 1.00 | ○ |  |

### P9 — защиты

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| P9.00 | 0900H | Motor overload stall prevention factor 30％..110％ 1% 105% | ○ |  |
| P9.01 | 0901H | Low voltage prevention level 360..480V(380V machine type) 1V 400V | ○ |  |
| P9.02 | 0902H | Over voltage prevention level 660..760V(380v machine type) 1V 700V | ○ |  |
| P9.03 | 0903H | Over current stall prevention level 80％..220％ 1% 180％ | ○ |  |

### PA — расширенные

| Параметр | Адрес | Определение, диапазон, единица, завод | Менять | Панель |
|---|---|---|---|---|
| PA.00 | 0A00H | Zero frequency running threshold 0.00..50.00Hz 0.01Hz 0.00Hz | ○ |  |
| PA.01 | 0A01H | Zero frequency hysteresis 0.00..50.00Hz 0.01Hz 0.00Hz | ○ |  |
| PA.02 | 0A02H | Energy consuming brakinginiti al voltage 600..750V 1V 700V | ○ |  |
| PA.03 | 0A03H | Energy consuming braking action percentage 10..100％ 1% 50％ | ○ |  |
| PA.04 | 0A04H | Input phase-failur e protection & Cooling fan control 0: Enable & Fan automatic control 1: Enable & Fan working during power on 2: Disable & Fan automatic control 3: Disable & Fan working during power on 1 0 | ○ |  |
| PA.05 | 0A05H | UP/DOWN terminal modifying speed 0.01Hz..100.0Hz/S 0.01Hz/S 1.00Hz/ S | ○ |  |
| PA.06 | 0A06H | Over modulation enabling 0: Forbidden 1: Allowable 1 0 | × |  |
| PA.07 | 0A07H | Timer value seting 0..65536S 1S 0 | × |  |
| PA.08 | 0A08H | The Rang of AO1 output 0: 0..10V/0..20mA 1: 2..10V/4..20mA 1 0 | × |  |
| PA.09 | — | Reserved | ? |  |
| PA.10 | — | Reserved | ? |  |
| PA.11 | — | Reserved | ? |  |
| PA.12 | — | Reserved | ? |  |

Всего параметров в таблице: 185, из них 9 помечены Reserved (адреса `—` или незанятые — привод на них молчит). Права: `○` — менять на ходу, `×` — только на останове, `√` — можно на ходу.

## Замерено на этом приводе (плата JC1060P470C + MINI-S-2S0007M, fw V2.03)

- Линия: **9600 8E1** (P8.01 = `013`), адрес 1. С 8N1 привод не отвечает вообще.
- **Привод не шлёт Modbus-исключение никогда.** Плохой кадр, чужой адрес, битая CRC —
  молчание. «Нет ответа» = единственный канал ошибок.
- `03H` по табл. 9-4 («не более 5 адресов в кадре») и по замерам: блоки 1..5 работают,
  с 6-го регистра начинается мусор и CRC не сходится.
- Латентность кадра 34..60 мс, разрешение линии на T3.5 ≈ 10 мс по мануалу; реальный
  разворот шины (от конца ответа до следующего запроса) нужен ~60..80 мс, иначе привод
  молчит весь таймаут при живой линии.
- Самоэхо в линии отсутствует: приёмник MAX485 закрыт на время нашей
  передачи (`uart_get_buffered_data_len()` после `uart_wait_tx_done()` = 0).
- Ответ `06H` побайтово равен запросу (включая CRC) — сам по себе ничего не доказывает.
  Единственное честное подтверждение записи: `0x2000`/`0x2001` прочитались обратно по `03H`.
- Проверенные значения в покое: `0x0E00 = 0x0001`, d-01 = `0x1388` (50.00 Гц),
  P0.04 = 2, P0.01 = 3, P0.07 = 10.00 Гц (нижний предел поднимает малые уставки),
  P2.19 = 10.00 Гц, P2.20 = P2.21 = 50 (0.1 с, масштабируются к базовой 50 Гц),
  P0.06 = 70.00 Гц, P0.08 = 50.00 Гц, P0.14 = 10.4, P0.15 = 20.0 при **P3.09 = 0
  (секунды)** — панель обязана подписывать единицу по чтению P3.09, а не по мануалу.
