# Электронная библиотека: Стандарты UI, архитектура HMI и профилирование задержек

Данная директория (`books/` / `книги/`) содержит нормативную, научную и практическую литературу, регламентирующую время отклика интерфейсов, эргономику человеко-машинного взаимодействия (HCI), архитектурные паттерны FreeRTOS и методику профилирования задержек встраиваемых систем.

---

## Каталог литературы и материалов

### 1. Фундаментальные труды по восприятию времени и эргономике
* **[01_Robert_Miller_1968_Response_Time_RU.md](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/01_Robert_Miller_1968_Response_Time_RU.md)**
  - *Автор:* Robert B. Miller (IBM Corporation, 1968).
  - *Название:* «Время отклика в диалоговых транзакциях между человеком и ЭВМ» (AFIPS Fall Joint Computer Conference).
  - *О чем:* Классический труд, заложивший основы HCI. Концепция психологического «замыкания» (Closure), 17 типов диалоговых транзакций и обоснование лимитов 0.1 с, 1.0 с, 15 с.

* **[02_Jakob_Nielsen_Response_Times_and_Limits_RU.md](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/02_Jakob_Nielsen_Response_Times_and_Limits_RU.md)**
  - *Автор:* Jakob Nielsen (Nielsen Norman Group).
  - *Название:* «Время отклика: 3 ключевых предела юзабилити и масштабы времени (Powers of 10)».
  - *О чем:* Биологическое обоснование ограничений восприятия человека (сенсорная память, кратковременная память, когнитивный фокус). Правила пассивной и активной обратной связи.

* **[03_ISO_9241_110_Dialogue_Principles_RU.md](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/03_ISO_9241_110_Dialogue_Principles_RU.md)**
  - *Организация:* International Organization for Standardization (ISO).
  - *Название:* «ISO 9241-110:2020: Эргономика взаимодействия человек-система — Часть 110: Принципы диалога».
  - *О чем:* 7 базовых принципов эргономики интерфейсов, критерии соответствия ожиданиям, регламент времени реакции на сенсорный ввод (< 150 мс) и обязательность индикации промежуточных состояний.

---

### 2. Спецификации измерения задержек и стандарты надежности ПО
* **[04_UI_Latency_Measurement_and_Logging_Specs_RU.md](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/04_UI_Latency_Measurement_and_Logging_Specs_RU.md)**
  - *Организации:* W3C, Google, MISRA, ISO 26262.
  - *Название:* «Стандарты и спецификации логирования задержек пользовательского интерфейса».
  - *О чем:* Разложение задержки на фазы (Input Delay, Processing Duration, Presentation Delay). Метрика Google INP (Interaction to Next Paint) и RAIL Model. Требования WCET (Worst-Case Execution Time) для автомобильных и ответственных систем.

* **[05_Realtime_Embedded_UI_and_FreeRTOS_Architecture_RU.md](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/05_Realtime_Embedded_UI_and_FreeRTOS_Architecture_RU.md)**
  - *Инженерное руководство:* Практическая архитектура для ESP32-P4 / FreeRTOS / ESP-IDF.
  - *О чем:* Разделение приоритетов задач, изоляция UI-потока от сетевого ввода-вывода (Producer-Consumer), реализация паттерна *Optimistic UI*, защита от дребезга тачей и замер латентности через `esp_timer_get_time()`.

---

### 3. Официальные скачанные спецификации W3C (Первоисточники)
Директория `specs/`:
* **[W3C_Event_Timing_API.html](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/specs/W3C_Event_Timing_API.html)** — Официальная спецификация консорциума W3C по измерению задержек взаимодействия пользователя с экраном (Event Timing API).
* **[W3C_User_Timing_Level_2.html](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/specs/W3C_User_Timing_Level_2.html)** — Официальный стандарт расстановки меток производительности (`mark`, `measure`).
* **[W3C_Performance_Timeline_Level_2.html](file:///c:/Users/megav/Downloads/JC1060P470C-camera-photo-app/books/specs/W3C_Performance_Timeline_Level_2.html)** — Спецификация шкалы времени и метрик профилирования.
