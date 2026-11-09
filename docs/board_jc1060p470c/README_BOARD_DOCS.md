# Документация на плату GUITION JC1060P470C (ESP32-P4)

В этой папке собрана полная заводская документация производителя Guition, даташиты на микросхемы и периферию платы, принципиальные схемы и наглядная распиновка разъёмов.

---

## Содержимое папки

| Файл | Описание документа |
| :--- | :--- |
| [**`JC1060P470C_V1.0_Schematic.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_V1.0_Schematic.pdf) | **Принципиальная электрическая схема платы (V1.0)**. Разводка всех GPIO, цепей питания LDO, схемы драйверов экрана, тача, Ethernet, аудио и разъема JP1 Expand IO. |
| [**`JC1060P470C_JP1_Pin_Definitions.png`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_JP1_Pin_Definitions.png) | **Цветная графическая раскладка гребёнки Expand IO (JP1)** с цветовой маркировкой (Power, GPIO, C6, GND, NC). |
| [**`JC1060P470C_Specification_EN.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_Specification_EN.pdf) | **Официальная техническая спецификация** платы на английском языке (параметры процессора ESP32-P4, 32 МБ PSRAM, MIPI-DSI, Wi-Fi 6 на ESP32-C6, питание, интерфейсы). |
| [**`JC1060P470C_Getting_Started_Manual.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_Getting_Started_Manual.pdf) | **Руководство пользователя производителя (Getting Started)**. Подключение, режимы прошивки, первоначальная настройка и тестирование. |
| [**`JC1060P470C_Structure_Diagram.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_Structure_Diagram.pdf) | **Чертёж и механические габариты** платы и дисплея (посадочные отверстия, расстояния между разъемами, толщина). |
| [**`JC1060M070N_I_7inch_IPS_Datasheet.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060M070N_I_7inch_IPS_Datasheet.pdf) | **Даташит на 7-дюймовый IPS экран** 1024×600 (контроллер JD9165, MIPI-DSI 2-lane, тайминги, распиновка шлейфа). |
| [**`ESP32_P4_Chip_Datasheet_CN.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/ESP32_P4_Chip_Datasheet_CN.pdf) | **Даташит на микроконтроллер ESP32-P4** от Espressif (периферия, PCNT, DMA2D, PPA, тайминги). |
| [**`OV02C10_Camera_Datasheet.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/OV02C10_Camera_Datasheet.pdf) | **Даташит на бортовую камеру OmniVision OV02C10** (MIPI-CSI). |
| [**`JC1060P470C_Ethernet_Spravochnik.pdf`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_Ethernet_Spravochnik.pdf) | **Справочник по аппаратному Ethernet** (RMII PHY, GPIO, настройка IP-стека). |
| [**`JC1060P470C_Audio_Hardware.md`**](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_Audio_Hardware.md) / [`.pdf`](file:///C:/Users/megav/Downloads/esp-jc1060p470c-camera/docs/board_jc1060p470c/JC1060P470C_Audio_Hardware.pdf) | **Аппаратный справочник по звуку**: кодек ES8311, усилитель динамика NS4150 (GPIO11), микрофон, схемы включения. |

---

## Быстрая шпаргалка по гребёнке Expand IO (JP1)

```
       Левый ряд (Нечётные)          Правый ряд (Чётные)
             [ 3V3 ]  (Пин 1)  |  (Пин 2)  [ 5V ]
             [ 3V3 ]  (Пин 3)  |  (Пин 4)  [ 5V ]
             [ GND ]  (Пин 5)  |  (Пин 6)  [ GND ]   <-- ЗЕМЛЯ ДАТЧИКОВ
ИДМ-20 Фаза A [ GPIO1 ](Пин 7) |  (Пин 8)  [ NC ]
ИДМ-20 Фаза B [ GPIO2 ](Пин 9) |  (Пин 10) [ GPIO47 ]
             [ GPIO3 ](Пин 11) |  (Пин 12) [ GPIO46 ]
             [ GPIO4 ](Пин 13) |  (Пин 14) [ GPIO45 ]
             [ GPIO5 ](Пин 15) |  (Пин 16) [ GND ]
             [ GPIO20](Пин 17) |  (Пин 18) [ 3V3 ]
             [ GPIO32](Пин 19) |  (Пин 20) [ C6_U0RXD ]
             [ GPIO33](Пин 21) |  (Пин 22) [ C6_U0TXD ]
             [I2C_SDA](Пин 23) |  (Пин 24) [ C6_IO9 ]
             [I2C_SCL](Пин 25) |  (Пин 26) [ C6_CHIP_PU ]
```
