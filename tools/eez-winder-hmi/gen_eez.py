"""Генератор WinderHMI.eez-project — панель намотчика PON в формате EEZ Studio 0.29.

Проект повторяет геометрию, палитру и состав экранов панели EASYDRIVE
(gen_ui.py для SquareLine Studio 1.6.2), но отдаёт интерфейс прошивке не
глобалами ui_*, а слоем нативных аксессоров get_var_*()/set_var_*() и
именованных действий action_*(). Причина: глобалы SquareLine живут в одном
файле-экспорте и перезаписываются каждым экспортом, а аксессоры — это
контракт, который прошивка реализует сама (см. README.md).

Схемы виджетов взяты дословно из официальных примеров EEZ (refs/*.eez-project),
а не придуманы: LVGL-9.0, sm9 (LVGL 9.x), noflow (нативный интерфейс без flow),
spinbox. Проверка результата — headless-сборка:

    "<EEZ Studio.exe>" --build-project "$PWD/WinderHMI.eez-project"
    # абсолютный путь к проекту обязателен: относительный EEZ не резолвит

objID считаются из имени (uuid5), поэтому повторный прогон даёт побайтово
тот же файл: diff в git показывает только реальные правки.
"""
import base64
import hashlib
import json
import os
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "WinderHMI.eez-project")
TTF = os.path.join(HERE, "assets", "fonts", "roboto_regular.ttf")

W, H = 1024, 600

# ---------------------------------------------------------------- палитра
# Те же кортежи, что в gen_ui.py: панель должна выглядеть как уже прошитая.
BG = (16, 21, 28)
PANEL = (26, 34, 48)
TOPBAR = (22, 32, 46)
FIELD = (38, 48, 66)
BTN = (45, 58, 78)
BTN_NAV = (30, 41, 59)
GREEN = (22, 163, 74)
RED = (220, 38, 38)
FAULT_BG = (40, 22, 26)
AMBER = (245, 158, 11)
BLUE = (37, 99, 235)
WHITE = (255, 255, 255)
DIM = (154, 167, 184)


def hx(rgb):
    return "#%06x" % (rgb[0] << 16 | rgb[1] << 8 | rgb[2])


def oid(key):
    return str(uuid.uuid5(uuid.NAMESPACE_DNS, "winder-hmi:" + key))


# ---------------------------------------------------------------- шрифты
# Имена — ровно те символы, которые EEZ объявит в fonts.h: ui_font_<имя>
# (EEZ приводит имя к нижнему регистру, поэтому и здесь нижний).
FONTS = [("body16", 16), ("label20", 20), ("head26", 26), ("big56", 56)]

# Диапазоны EEZ пишются в десятичных кодовых точках через запятую.
# 32-127 ASCII + 176 «градус» + русский блок 1040-1103 + Ё(1025) и ё(1105).
# Ё/ё вне 1040..1103 — без них «Ёлка» превращается в пустые клетки.
RANGES = "32-127,176,1025,1040-1103,1105"


def font_entry(name, size):
    blob = base64.b64encode(open(TTF, "rb").read()).decode()
    return {
        "objID": oid("font:" + name),
        "name": name,
        "renderingEngine": "LVGL",
        "source": {
            "objID": oid("fontsrc:" + name),
            "filePath": "assets\\fonts\\roboto_regular.ttf",
            "size": size,
        },
        "embeddedFontFile": blob,
        "bpp": 4,
        "threshold": 128,
        "height": 0,
        "ascent": 0,
        "descent": 0,
        "glyphs": [],
        "lvglRanges": RANGES,
        "lvglSymbols": "",
        "lvglBinFile": "",
    }


# ---------------------------------------------------------------- виджеты
# Флаги — из примеров EEZ, чтобы генератор не выдал ни одного лишнего
# add_flag/remove_flag против дефолтов LVGL.
FLAGS = {
    "screen": "CLICKABLE|PRESS_LOCK|CLICK_FOCUSABLE|GESTURE_BUBBLE|SNAPPABLE|SCROLLABLE|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER",
    "panel": "CLICKABLE|CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLLABLE|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_WITH_ARROW|SNAPPABLE",
    "button": "PRESS_LOCK|CLICK_FOCUSABLE|GESTURE_BUBBLE|SNAPPABLE|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_ON_FOCUS|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER",
    "label": "PRESS_LOCK|CLICK_FOCUSABLE|GESTURE_BUBBLE|SNAPPABLE|SCROLLABLE|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER",
    "slider": "CLICKABLE|CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_WITH_ARROW|SNAPPABLE",
    "spinbox": "CLICK_FOCUSABLE|GESTURE_BUBBLE|PRESS_LOCK|SCROLLABLE|SCROLL_CHAIN_HOR|SCROLL_CHAIN_VER|SCROLL_ELASTIC|SCROLL_MOMENTUM|SCROLL_ON_FOCUS|SCROLL_WITH_ARROW|SNAPPABLE",
}

TYPES = {
    "screen": "LVGLScreenWidget",
    "panel": "LVGLPanelWidget",
    "button": "LVGLButtonWidget",
    "label": "LVGLLabelWidget",
    "slider": "LVGLSliderWidget",
    "spinbox": "LVGLSpinboxWidget",
}


def base(kind, key, x, y, w, h, style, clickable):
    return {
        "objID": oid(key),
        "type": TYPES[kind],
        "left": x,
        "top": y,
        "width": w,
        "height": h,
        "customInputs": [],
        "customOutputs": [],
        "style": {
            "objID": oid(key + ":style"),
            "useStyle": "default",
            "conditionalStyles": [],
            "childStyles": [],
        },
        "timeline": [],
        "eventHandlers": [],
        "identifier": key.split("/")[-1],
        "leftUnit": "px",
        "topUnit": "px",
        "widthUnit": "px",
        "heightUnit": "px",
        "widgetFlags": FLAGS[kind],
        "hiddenFlag": False,
        "hiddenFlagType": "literal",
        "clickableFlag": clickable,
        "clickableFlagType": "literal",
        "checkedStateType": "literal",
        "disabledStateType": "literal",
        "states": "",
        "localStyles": {"objID": oid(key + ":local"), "definition": style},
        "groupIndex": 0,
    }


def bg(rgb, radius):
    d = {"bg_color": hx(rgb), "bg_opa": 255, "border_width": 0,
         "radius": radius, "pad_top": 0, "pad_bottom": 0,
         "pad_left": 0, "pad_right": 0}
    return {"MAIN": {"DEFAULT": d}}


def txt(color, font, center=False):
    d = {"text_color": hx(color), "text_font": font}
    if center:
        d["align"] = "CENTER"
    return {"MAIN": {"DEFAULT": d}}


def panel(key, x, y, w, h, kids, color=PANEL, radius=10):
    n = base("panel", "panel/" + key, x, y, w, h, bg(color, radius), False)
    n["children"] = kids
    return n


def label(key, x, y, text, color=WHITE, font="body16", center=False,
          variable=None):
    n = base("label", "label/" + key, x, y, 0, 0, txt(color, font, center),
             False)
    # size "content" = LV_SIZE_CONTENT, как в SquareLine-версии панели
    n["widthUnit"] = "content"
    n["heightUnit"] = "content"
    n["text"] = variable if variable else text
    n["textType"] = "expression" if variable else "literal"
    n["longMode"] = "WRAP"
    n["recolor"] = False
    return n


def button(key, x, y, w, h, caption, action, color=BTN, fg=WHITE,
           font="label20", radius=12):
    n = base("button", "button/" + key, x, y, w, h, bg(color, radius), True)
    lab = label(key + "_lbl", 0, 0, caption, fg, font, center=True)
    n["children"] = [lab]
    if action:
        n["eventHandlers"] = [{
            "objID": oid("event/" + key),
            "eventName": "CLICKED",
            "handlerType": "action",
            "action": action,
        }]
    return n


def slider(key, x, y, w, h, lo, hi, var):
    n = base("slider", "slider/" + key, x, y, w, h,
             {"MAIN": {"DEFAULT": {"bg_color": hx(FIELD), "bg_opa": 255,
                                   "radius": h // 2, "border_width": 0}},
              "INDICATOR": {"DEFAULT": {"bg_color": hx(BLUE), "bg_opa": 255,
                                        "radius": h // 2}},
              "KNOB": {"DEFAULT": {"bg_color": hx(WHITE), "bg_opa": 255,
                                   "radius": h // 2, "border_width": 0}}},
             True)
    n["min"] = lo
    n["minType"] = "literal"
    n["max"] = hi
    n["maxType"] = "literal"
    n["mode"] = "NORMAL"
    n["value"] = var
    n["valueType"] = "expression"
    n["valueLeft"] = 0
    n["valueLeftType"] = "literal"
    return n


def spinbox(key, x, y, w, h, lo, hi, digits, frac, var):
    n = base("spinbox", "spinbox/" + key, x, y, w, h,
             {"MAIN": {"DEFAULT": {"bg_color": hx(FIELD), "bg_opa": 255,
                                   "radius": 6, "border_width": 0,
                                   "text_color": hx(WHITE),
                                   "text_font": "label20",
                                   "text_align": "LEFT"}}}, True)
    n["digitCount"] = digits
    n["separatorPosition"] = frac
    n["min"] = lo
    n["max"] = hi
    n["rollover"] = False
    n["step"] = 1
    n["stepType"] = "literal"
    n["value"] = var
    n["valueType"] = "expression"
    return n


# ---------------------------------------------------------------- экраны
NAV = [("main", "ГЛАВНАЯ"), ("params", "ПАРАМЕТРЫ"), ("alarm", "АВАРИИ"),
       ("conn", "ЛИНИЯ")]


def navbar(which):
    btns = [button("btn_nav_%s_%s" % (k, which), 8 + i * 254, 12, 246, 60, cap,
                   "nav_" + k, BLUE if k == which else BTN_NAV)
            for i, (k, cap) in enumerate(NAV)]
    return panel("nav_" + which, 0, 516, W, 84, btns, TOPBAR, 0)


def topbar(title, which, right=None):
    kids = [label("lbl_title_" + which, 16, 12, title, WHITE, "head26")]
    if right:
        kids.append(label("lbl_top_right_" + which, 800, 18, right, DIM,
                          "body16"))
    return panel("top_" + which, 0, 0, W, 56, kids, TOPBAR, 0)


# --- ГЛАВНАЯ ---------------------------------------------------------------
main_meas = [
    label("lbl_meas_cap", 20, 12, "ЧАСТОТА ВЫХОДНАЯ", DIM, "body16"),
    label("lbl_freq", 20, 40, None, WHITE, "big56", variable="freq_txt"),
    label("lbl_freq_unit", 250, 80, "Гц", DIM, "label20"),
]
main_mon = [label("lbl_state_cap", 20, 96, "СОСТОЯНИЕ ПРИВОДА (0x0E00)", DIM,
                  "body16"),
            label("lbl_state", 20, 124, None, AMBER, "head26",
                  variable="state_txt")]
for nm, var, cap, x in [("lbl_cur", "cur_txt", "ТОК, А", 20),
                        ("lbl_volt", "volt_txt", "НАПРЯЖЕНИЕ, В", 170),
                        ("lbl_rpm", "rpm_txt", "ОБОРОТЫ, об/мин", 320)]:
    main_mon.append(label(nm + "_cap", x, 16, cap, DIM, "body16"))
    main_mon.append(label(nm, x, 44, None, WHITE, "label20", variable=var))

main_sp = [
    label("lbl_sp_cap", 20, 10,
          "УСТАВКА ЧАСТОТЫ, Гц (регистр 0x2001, 0.01 Гц)", DIM, "body16"),
    slider("sld_sp", 20, 56, 620, 32, 0, 5000, "sp_raw"),
    button("btn_sp_dn", 660, 42, 60, 60, "-", "sp_dn", BTN_NAV,
           WHITE, "head26"),
    label("lbl_sp_val", 740, 56, None, WHITE, "head26", variable="sp_txt"),
    button("btn_sp_up", 890, 42, 60, 60, "+", "sp_up", BTN_NAV,
           WHITE, "head26"),
]

main = [topbar("EASYDRIVE - НАМОТКА PON", "main"),
        panel("link", 600, 0, 424, 56,
              [label("lbl_link", 0, 0, None, DIM, "body16", center=True,
                     variable="link_txt")], TOPBAR, 0),
        panel("meas", 16, 68, 480, 196, main_meas),
        panel("mon", 512, 68, 496, 196, main_mon),
        panel("sp", 16, 276, 992, 116, main_sp)]
for i, (key, cap, col, fg) in enumerate([
        ("btn_start", "ПУСК", GREEN, WHITE),
        ("btn_stop", "СТОП", RED, WHITE),
        ("btn_jog", "ТОЛЧОК", BLUE, WHITE),
        ("btn_rev", "РЕВЕРС", BTN, WHITE),
        ("btn_fault_reset", "СБРОС", AMBER, BG)]):
    main.append(button(key, 16 + i * 200, 404, 190, 96, cap, key,
                       col, fg, "head26"))
main.append(navbar("main"))

# --- ПАРАМЕТРЫ -------------------------------------------------------------
# Строго те же семь параметров и те же масштабы, что в gen_ui.py и в
# ui_easydrive.c: адрес = группа.номер, частоты в 0.01 Гц, ramp в 0.1 с.
PARAMS = [("channel", "P0.01", "Канал задания (3 = Modbus)", 0, 8, 1, 0),
          ("fmax", "P0.06", "Верхняя частота, Гц", 0, 40000, 5, 2),
          ("fmin", "P0.07", "Нижняя частота, Гц", 0, 40000, 5, 2),
          ("fbase", "P0.08", "Базовая частота, Гц", 0, 40000, 5, 2),
          ("accel", "P0.14", "Время разгона, ед. по P3.09", 1, 36000, 5, 1),
          ("decel", "P0.15", "Время торможения, ед. P3.09", 1, 36000, 5, 1),
          ("jog", "P2.19", "Частота толчка JOG, Гц", 0, 5000, 4, 2)]

params = [topbar("ПАРАМЕТРЫ ПРЕОБРАЗОВАТЕЛЯ", "params",
                 "АДРЕС 1  9600 8E1"),
          button("btn_p_read", 16, 72, 240, 48, "ЧИТАТЬ", "par_read",
                 BTN_NAV, WHITE, "label20", 8),
          button("btn_p_write", 272, 72, 240, 48, "ЗАПИСАТЬ",
                 "par_write", GREEN, WHITE, "label20", 8),
          label("lbl_p_msg", 540, 86, None, DIM, "body16",
                variable="p_msg_txt")]
for i, (key, code, nm, lo, hi, digits, frac) in enumerate(PARAMS):
    y = 136 + i * 50
    params.append(label("lbl_p%d" % i, 20, y + 12, "%s  %s" % (code, nm),
                        DIM, "label20"))
    params.append(spinbox("spn_" + key, 760, y, 240, 44, lo, hi, digits, frac,
                          "par_" + key))
params.append(navbar("params"))

# --- АВАРИИ ----------------------------------------------------------------
alarm = [topbar("АВАРИИ И ЖУРНАЛ ПРИВОДА", "alarm"),
         panel("cur", 16, 68, 992, 120, [
             label("lbl_acode", 20, 16, None, RED, "head26",
                   variable="fault_code_txt"),
             label("lbl_aname", 180, 22, None, WHITE, "label20",
                   variable="fault_name_txt"),
             label("lbl_adesc", 180, 58, None, DIM, "body16",
                   variable="fault_desc_txt"),
             label("lbl_atime", 20, 86, None, DIM, "body16",
                   variable="fault_time_txt")], FAULT_BG, 10),
         button("btn_a_read", 16, 200, 260, 44, "ОБНОВИТЬ ЖУРНАЛ",
                "journal_read", BTN_NAV, WHITE, "body16", 8),
         button("btn_a_reset", 292, 200, 260, 44, "СБРОС АВАРИИ",
                "fault_reset", AMBER, BG, "body16", 8),
         panel("jrn", 16, 256, 992, 248,
               [label("lbl_jrn_cap", 20, 10, "ПОСЛЕДНИЕ АВАРИИ ПРИВОДА", DIM,
                      "body16")] +
               [label("lbl_jrn%d" % i, 20, 36 + i * 22, None, WHITE, "body16",
                      variable="jrn%d_txt" % i) for i in range(3)] +
               [label("lbl_snap_cap", 520, 10,
                      "ПАРАМЕТРЫ ПРИ ПОСЛЕДНЕЙ АВАРИИ", DIM, "body16")] +
               [label("lbl_snap%d" % i, 520, 36 + i * 22, None, WHITE,
                      "body16", variable="snap%d_txt" % i)
                for i in range(6)]),
         navbar("alarm")]

# --- ЛИНИЯ -----------------------------------------------------------------
# Как и в SquareLine-версии: ни одного перевода строки в тексте виджета —
# каждая строка отдельный label, иначе генератор вставит \n в set_text().
conn = [topbar("ЛИНИЯ RS-485 / MODBUS RTU", "conn"),
        panel("line", 16, 68, 992, 380,
              [label("lbl_c_state", 20, 16, None, RED, "head26",
                     variable="c_state_txt"),
               label("lbl_c_cfg_cap", 20, 68, "НАСТРОЙКА ДРАЙВЕРА", DIM,
                     "body16"),
               label("lbl_c_cfg", 20, 92, None, WHITE, "body16",
                     variable="c_cfg1_txt"),
               label("lbl_c_cfg2", 20, 112, None, WHITE, "body16",
                     variable="c_cfg2_txt"),
               label("lbl_c_cnt_cap", 20, 150, "СЧЁТЧИКИ ОБМЕНА", DIM,
                     "body16"),
               label("lbl_c_cnt", 20, 174, None, WHITE, "body16",
                     variable="c_cnt1_txt"),
               label("lbl_c_cnt2", 20, 194, None, WHITE, "body16",
                     variable="c_cnt2_txt"),
               label("lbl_c_cmd_cap", 520, 68, "ПОСЛЕДНЯЯ КОМАНДА", DIM,
                     "body16"),
               label("lbl_c_cmd", 520, 92, None, WHITE, "body16",
                     variable="c_cmd1_txt"),
               label("lbl_c_cmd2", 520, 112, None, WHITE, "body16",
                     variable="c_cmd2_txt"),
               label("lbl_c_err_cap", 520, 150, "ПОСЛЕДНИЙ СБОЙ ОБМЕНА", DIM,
                     "body16"),
               label("lbl_c_err", 520, 174, None, WHITE, "body16",
                     variable="c_err_txt"),
               label("lbl_c_frame", 520, 194, None, DIM, "body16",
                     variable="c_frame_txt"),
               label("lbl_c_note", 20, 250,
                     "Параметры линии задаются в menuconfig и в прошивке не "
                     "меняются.", DIM, "body16")]),
        button("btn_c_reset", 16, 460, 300, 44, "СБРОС СЧЁТЧИКОВ",
               "counters_reset", BTN_NAV, WHITE, "body16", 8),
        navbar("conn")]


def page(name, kids, idx):
    screen = base("screen", "screen/" + name, 0, 0, W, H, bg(BG, 0), True)
    screen["children"] = kids
    return {
        "objID": oid("page:" + name),
        "components": [screen],
        "connectionLines": [],
        "localVariables": [],
        "userProperties": [],
        "name": name,
        "left": idx * 1100,
        "top": 0,
        "width": W,
        "height": H,
        "createAtStart": True,
        "deleteOnScreenUnload": False,
    }


PAGES = [("main", main), ("params", params), ("alarm", alarm), ("conn", conn)]

# ---------------------------------------------------------------- переменные
# integer — то, что крутят руками (слайдер и спинбоксы), в сырых единицах
# регистра. string — всё, что только показывают: формат числа остаётся в
# прошивке, а не в движке выражений EEZ.
VARS = [("sp_raw", "integer", "0")]
VARS += [("par_" + k, "integer", "0") for k, *_ in PARAMS]
VARS += [(v, "string", '""') for v in [
    "freq_txt", "state_txt", "cur_txt", "volt_txt", "rpm_txt", "sp_txt",
    "link_txt", "p_msg_txt",
    "fault_code_txt", "fault_name_txt", "fault_desc_txt", "fault_time_txt",
    "jrn0_txt", "jrn1_txt", "jrn2_txt",
    "snap0_txt", "snap1_txt", "snap2_txt", "snap3_txt", "snap4_txt",
    "snap5_txt",
    "c_state_txt", "c_cfg1_txt", "c_cfg2_txt", "c_cnt1_txt", "c_cnt2_txt",
    "c_cmd1_txt", "c_cmd2_txt", "c_err_txt", "c_frame_txt",
]]


def var_entry(name, type_, default):
    return {"objID": oid("var:" + name), "name": name, "type": type_,
            "defaultValue": default, "persistent": False, "native": False}


# ---------------------------------------------------------------- действия
# VALUE_CHANGED у слайдера и спинбоктов не дублируем действием: EEZ уже
# навешивает на них встроенный обработчик, который вызывает set_var_sp_raw /
# set_var_par_*; firmware ловит правку именно там.
ACTIONS = ["nav_main", "nav_params", "nav_alarm",
           "nav_conn", "btn_start", "btn_stop",
           "btn_jog", "btn_rev", "btn_fault_reset",
           "sp_up", "sp_dn",
           "par_read", "par_write",
           "journal_read", "fault_reset",
           "counters_reset"]

# ---------------------------------------------------------------- сборка
def build_file(name, template):
    return {"objID": oid("buildfile:" + name), "fileName": name,
            "template": template.replace("\n", "\r\n")}


# Только те файлы, чей код генерирует EEZ. vars.c и actions.c в списке нет
# нарочно: их тела принадлежат прошивке (get_var_* поверх g_vfd и action_*
# поверх очереди команд), а EEZ переписывает файлы из своего списка целиком.
FILES = [
    ("screens.h", """#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

//${eez-studio LVGL_INCLUDE}

#ifdef __cplusplus
extern "C" {
#endif

//${eez-studio LVGL_SCREENS_DECL}
//${eez-studio LVGL_SCREENS_DECL_EXT}

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/"""),
    ("screens.c", """#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

//${eez-studio LVGL_SCREENS_DEF}
//${eez-studio LVGL_SCREENS_DEF_EXT}"""),
    ("actions.h", """#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

//${eez-studio LVGL_INCLUDE}

#ifdef __cplusplus
extern "C" {
#endif

//${eez-studio LVGL_ACTIONS_DECL}

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/"""),
    ("vars.h", """#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

//${eez-studio FLOW_ENUMS}

// Flow global variables

//${eez-studio FLOW_GLOBAL_VARIABLES_ENUM}

// Native global variables

//${eez-studio LVGL_VARS_DECL}

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/"""),
    ("images.h", """#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

//${eez-studio LVGL_INCLUDE}

#ifdef __cplusplus
extern "C" {
#endif

//${eez-studio LVGL_IMAGES_DECL}

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/"""),
    ("images.c", """#include "images.h"

//${eez-studio LVGL_IMAGES_DEF}"""),
    ("fonts.h", """#ifndef EEZ_LVGL_UI_FONTS_H
#define EEZ_LVGL_UI_FONTS_H

//${eez-studio LVGL_INCLUDE}

#ifdef __cplusplus
extern "C" {
#endif

//${eez-studio LVGL_FONTS_DECL}

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_FONTS_H*/"""),
    ("styles.h", """#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

//${eez-studio LVGL_INCLUDE}

#ifdef __cplusplus
extern "C" {
#endif

//${eez-studio LVGL_STYLES_DECL}

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/"""),
    ("styles.c", """#include "styles.h"
#include "images.h"
#include "fonts.h"

//${eez-studio LVGL_STYLES_DEF}"""),
    ("ui.h", """#ifndef EEZ_LVGL_UI_GUI_H
#define EEZ_LVGL_UI_GUI_H

//${eez-studio LVGL_INCLUDE}

#include "screens.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init();
void ui_tick();

void loadScreen(enum ScreensEnum screenId);

#ifdef __cplusplus
}
#endif

#endif // EEZ_LVGL_UI_GUI_H"""),
    ("ui.c", """#include "ui.h"
#include "images.h"

static int16_t currentScreen = -1;

static lv_obj_t *getLvglObjectFromIndex(int32_t index) {
    if (index == -1) {
        return 0;
    }
    return ((lv_obj_t **)&objects)[index];
}

void loadScreen(enum ScreensEnum screenId) {
    currentScreen = (int16_t)(screenId - 1);
    lv_obj_t *screen = getLvglObjectFromIndex(currentScreen);
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_FADE_ON, 150, 0, false);
}

void ui_init() {
    create_screens();
    loadScreen(SCREEN_ID_MAIN);
}

void ui_tick() {
    tick_screen(currentScreen);
}"""),
]


def project():
    return {
        "lvglStyles": {"objID": oid("lvglStyles"), "styles": [],
                       "defaultStyles": {}},
        "objID": oid("project"),
        "settings": {
            "objID": oid("settings"),
            "general": {
                "objID": oid("general"),
                "projectVersion": "v3",
                "projectType": "lvgl",
                "lvglVersion": "9.2.2",
                "extensions": [],
                "imports": [],
                "flowSupport": False,
                "displayWidth": W,
                "displayHeight": H,
                "displayBorderRadius": 0,
                "colorBpp": "16",
                "bitmapColorFormat": "RGB",
                "description": "Панель станка намотки PON: JC1060P470C (ESP32-P4), "
                               "MIPI-DSI 1024x600, LVGL 9, частотник Easydrive по RS-485.",
                "image": "",
                "keywords": "esp32p4,lvgl9,modbus,pon",
                "resourceFiles": [],
            },
            "build": {
                "objID": oid("build"),
                "configurations": [{"objID": oid("config:Default"),
                                    "name": "Default"}],
                "files": [build_file(n, t) for n, t in FILES],
                "destinationFolder": "generated\\ui",
                "separateFolderForImagesAndFonts": True,
                "imageExportMode": "source",
                "fontExportMode": "source",
                "fileSystemPath": "",
                "lvglInclude": "lvgl.h",
                "screensLifetimeSupport": False,
                "generateSourceCodeForEezFramework": False,
                "compressFlowDefinition": False,
                "executionQueueSize": 1000,
                "expressionEvaluatorStackSize": 20,
            },
        },
        "variables": {
            "objID": oid("variables"),
            "globalVariables": [var_entry(*v) for v in VARS],
            "structures": [],
            "enums": [],
        },
        "actions": [{"objID": oid("action:" + a), "components": [],
                     "connectionLines": [], "localVariables": [],
                     "name": a} for a in ACTIONS],
        "userPages": [page(n, kids, i) for i, (n, kids) in enumerate(PAGES)],
        "userWidgets": [],
        "fonts": [font_entry(n, s) for n, s in FONTS],
        "readme": {"objID": oid("readme"), "readmeFile": "README.md"},
        "bitmaps": [],
        "colors": [],
        "themes": [{"objID": oid("theme:Default"), "name": "Default",
                    "colors": []}],
    }


if __name__ == "__main__":
    text = json.dumps(project(), ensure_ascii=False, indent=2)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    n = 0
    for _, kids in PAGES:
        def count(lst):
            return sum(1 + count(c.get("children") or []) for c in lst)
        n += count(kids)
    print("экранов %d, виджетов %d, переменных %d, действий %d, файл %d КБ"
          % (len(PAGES), n, len(VARS), len(ACTIONS), len(text) // 1024))
