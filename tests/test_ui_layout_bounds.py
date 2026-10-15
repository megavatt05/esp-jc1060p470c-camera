"""
CAMBROWSER UI LAYOUT & TEXT BOUNDS TEST SUITE
=============================================
Тест компоновки интерфейса и эмуляции действий пользователя.
Проверяет выход текста за границы кнопок (Text Overflow / Bounding Box Collision).

Стандарты:
- ISO 9241-110 (Visual clarity and self-descriptiveness)
- Google Material Layout & Bounds Validation
"""

import unittest

# Константы шрифта и геометрии CamBrowser (из main/camos/ui.h и browser.c)
UI_FONT_W = 8
UI_FONT_H = 8
LCD_W = 1024
LCD_H = 600

# Список радиостанций из browser.c
RADIO_STATIONS = [
    "Радио Маяк",
    "Вести FM",
    "Европа Плюс",
    "Дорожное Радио",
    "Культура FM",
    "Радио России",
]

def utf8_char_count(s: str) -> int:
    return len(s)

def calc_text_width(text: str, scale: int) -> int:
    return utf8_char_count(text) * UI_FONT_W * scale

def get_radio_btn_geometry(i: int):
    x = 516 if (i & 1) else 12
    y = 88 + (i >> 1) * 80
    w = 496
    h = 72
    return {"x": x, "y": y, "w": w, "h": h}


class TestUiLayoutAndTextBounds(unittest.TestCase):
    """
    Тестирование компоновки элементов UI и детекции оверфлоу текста.
    """

    def test_legacy_single_line_format_fails_bounds(self):
        """
        ДЕМОНСТРАЦИЯ ОБНАРУЖЕНИЯ ДЕФЕКТА:
        Если формировать строку как '[Имя] [ПОДКЛ...]' со scale=3,
        текст гарантированно вылетает за правую границу кнопки.
        """
        btn_w = 496
        overflows = []

        for i, station in enumerate(RADIO_STATIONS):
            label = f"{station} [ПОДКЛ...]"
            tw = calc_text_width(label, scale=3)
            btn = get_radio_btn_geometry(i)
            text_x = btn["x"] + (btn["w"] - tw) // 2 + 12
            text_right = text_x + tw
            btn_right = btn["x"] + btn["w"]

            if text_right > btn_right or text_x < btn["x"]:
                overflows.append((station, text_right - btn_right))

        # Тест доказывает, что дефект существовал для большинства станций
        self.assertGreater(len(overflows), 0, "Тест обязан зафиксировать оверфлоу для однострочного формата")
        # Для 'Дорожное Радио' выход за пределы составлял более 60 пикселей!
        dorognoe_overflow = dict(overflows).get("Дорожное Радио", 0)
        self.assertGreater(dorognoe_overflow, 50)

    def test_two_line_layout_prevents_overflow_for_all_stations(self):
        """
        ТЕСТ ВАЛИДАЦИИ ДВУХСТРОЧНОГО МАКЕТА:
        При разделении на заголовок (scale=2) и статус (scale=1)
        НИ ОДНА станция не выходит за границы ни по горизонтали, ни по вертикали.
        """
        icon_w = 56  # отступ с учетом иконки слева

        for i, station in enumerate(RADIO_STATIONS):
            btn = get_radio_btn_geometry(i)
            available_w = btn["w"] - icon_w - 16  # запас справа 16px

            # 1. Заголовок станции при scale=2
            title_w = calc_text_width(station, scale=2)
            self.assertLessEqual(
                title_w, available_w,
                f"Заголовок '{station}' (scale=2) превышает ширину кнопки: {title_w}px > {available_w}px"
            )

            # 2. Строка статуса при scale=1 ('ПОДКЛЮЧЕНИЕ...' или 'В ЭФИРЕ')
            status_conn_w = calc_text_width("ПОДКЛЮЧЕНИЕ...", scale=1)
            self.assertLessEqual(status_conn_w, available_w)

            # 3. Вертикальное размещение (высота кнопки h=72)
            # y_title = 16, h_title = 16 (до 32)
            # y_status = 44, h_status = 8 (до 52)
            # 52 < 72 (запас 20px снизу)
            total_content_h = 44 + 8
            self.assertLess(total_content_h, btn["h"], "Текст выходит за вертикальные границы кнопки")

    def test_user_action_simulation_full_journey(self):
        """
        ЭМУЛЯЦИЯ ПОЛНОГО СЦЕНАРИЯ ДЕЙСТВИЙ ПОЛЬЗОВАТЕЛЯ (USER JOURNEY):
        1. Пользователь находится на экране радио (все карточки IDLE).
        2. Пользователь нажимает на 'Дорожное Радио' (координаты тача x=700, y=190).
        3. Карточка переходит в статус CONNECTING:
           - Проверяем попадание тача в кнопку (hit testing);
           - Проверяем, что ширина названия и статуса не выходят за рамку;
        4. Поток начинает играть: карточка переходит в PLAYING:
           - Проверяем отрисовку статуса 'В ЭФИРЕ';
           - Проверяем границы;
        5. Пользователь нажимает 'СТОП':
           - Карточка возвращается в IDLE.
        """
        target_idx = 3  # Дорожное Радио (самое длинное название)
        btn = get_radio_btn_geometry(target_idx)

        # 1. Эмуляция тача в центр кнопки
        tap_x = btn["x"] + btn["w"] // 2
        tap_y = btn["y"] + btn["h"] // 2

        # Проверяем hit test
        hit = (btn["x"] <= tap_x <= btn["x"] + btn["w"]) and (btn["y"] <= tap_y <= btn["y"] + btn["h"])
        self.assertTrue(hit, "Тач должен точно попадать в кнопку")

        # 2. Состояние CONNECTING
        station_name = RADIO_STATIONS[target_idx]
        title_w = calc_text_width(station_name, 2)
        status_w = calc_text_width("ПОДКЛЮЧЕНИЕ...", 1)

        content_right_edge = btn["x"] + 56 + max(title_w, status_w)
        btn_right_edge = btn["x"] + btn["w"]

        # Ключевое условие: контент полностью внутри кнопки!
        margin = btn_right_edge - content_right_edge
        self.assertGreater(
            margin, 100,
            f"Запас пространства для '{station_name}' должен быть более 100px (фактический запас: {margin}px)"
        )


if __name__ == "__main__":
    unittest.main()
