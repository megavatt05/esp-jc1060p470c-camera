# Панель HMI, сгенерированная EEZ Studio

Сюда попадает UI станка намотки, описанный в `~/Downloads/eez-winder-hmi`
(`WinderHMI.eez-project`, собирается скриптом `gen_eez.py`). Движок Modbus RTU
и вся логика привода — в `main/vfd_easydrive.c`; этот каталог только рисует.

Экраны: `ГЛАВНАЯ` → `ПАРАМЕТРЫ` → `АВАРИИ` → `СВЯЗЬ` (SCREEN_ID_MAIN..CONN).
Старый интерфейс SquareLine Studio лежит рядом в `main/ui` + `main/ui_easydrive.c`
и в сборку не входит: оба набора экспортируют `ui_init()/ui_tick()`, поэтому
одновременно их быть не может. Для отката — вернуть `SLS_UI_SRCS` в
`main/CMakeLists.txt` и вызвать `easydrive_ui_init()`.

## Состав каталога

| Файл | Откуда |
|---|---|
| `screens.c/h`, `vars.h`, `actions.h`, `fonts.h`, `styles.c/h`, `images.c/h`, `ui.c/h` | EEZ Studio, ни строки не править руками |
| `eez_glue.c`, `eez_ui.h` | рука: тени переменных, `get_var_*`/`set_var_*`/`action_*`, задача шины |
| `fonts/ui_font_*.c` | копия шрифтов SquareLine с нижним регистром имён (см. ловушку 3) |

## Как пересобрать после правки макета

```bash
cd ~/Downloads/eez-winder-hmi && python gen_eez.py
"C:/Users/megav/AppData/Local/Programs/eezstudio/EEZ Studio.exe" \
    --build-project "C:/Users/megav/Downloads/eez-winder-hmi/WinderHMI.eez-project"
cp generated/ui/{ui.h,ui.c,screens.h,screens.c,vars.h,actions.h,fonts.h,styles.h,styles.c,images.h,images.c} \
    ~/Downloads/esp-jc1060p470c-camera/main/eez_ui/
```

После этого `eez_glue.c` обязан закрыть все объявленные в `vars.h`/`actions.h`
имена — линker скажет точнее, чем любой список.

## Контракт glue-слоя (три правила, на которых всё держится)

1. **Геттеры строк никогда не возвращают NULL.** Генератор сравнивает
   `strcmp(new_val, cur_val)`; NULL = вылет. Каждый геттер пишет в свой
   `static char b[N]` и возвращает его.
2. **Геттеры чисел обязаны отдавать тень, а не «спросить у привода».** Тик
   сравнивает `get_var_par_x()` с `lv_spinbox_get_value()` и трогает виджет
   только при расхождении; опрос шины вне LVGL-задачи запрещён.
3. **Правка оператора приходит сама**: на слайдер и спинбокты EEZ навешивает
   встроенный обработчик `LV_EVENT_VALUE_CHANGED`, который вызывает
   `set_var_*`. Дублировать это действием в проекте нельзя — будет два вызова.

## Ловушки, уже оплаченные

1. **Спинбокт затирает тень нулём.** `lv_spinbox` в LVGL 9 — потомок textarea, и
   любая его перерисовка (`set_digit_format`, `set_range`) идёт через
   `lv_textarea_set_text()`, а та рассылает `LV_EVENT_VALUE_CHANGED`. Встроенный
   обработчик EEZ читает виджет обратно и зовёт `set_var_par_*`. Если тень
   заполнена *до* подгонки формата, обратно прилетает старый ноль — и экран
   показывает «Прочитано 7/7» при пустых полях. Порядок в `spinbox_fit()`:
   формат → диапазон → `lv_spinbox_set_value(raw)` → тень берётся из виджета.
   У слайдера этой гонки нет: `lv_bar_set_value()` событий не рассылает.
2. **Ведущие нули.** Спинбокт дописывает нули до `digit_count`, поэтому 0.1 с
   выглядит как «0000.1», а оператор читает 0,0001. `spinbox_fit()` режет
   `digit_count` по фактической длине числа.
3. **`set_digit_format()` молча обрезает `range_max` до `10^digit_count-1`.**
   Поэтому сразу следом обязана идти `lv_spinbox_set_range()`.
4. **Имена шрифтов.** EEZ пишет их в нижний регистр (`ui_font_body16`); в
   прошивке уже был `ui_font_Body16.c` — на NTFS это одна и та же файловая запись,
   потому копии лежат в `eez_ui/fonts/` с переименованными символами.
5. **`lvglInclude` в проекте = `lvgl.h`**, не `lvgl/lvgl.h`: иначе скомпонентный
   путь include в IDF 6.1 не-resolve'ится.
