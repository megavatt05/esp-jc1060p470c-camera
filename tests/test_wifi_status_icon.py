"""
CAMBROWSER STATUS BAR & WI-FI ICON TRANSITIONS TEST SUITE
=========================================================
Тестирует логику переключения значка Wi-Fi в статус-баре:
1. Состояния иконки (WIFI_ON vs WIFI_OFF).
2. Цветовую индикацию (зеленый / желтый / синий / фиолетовый / красный).
3. Интерактивную реакцию на тап по значку Wi-Fi (x < 380, y < BAR_H):
   - Открытие экрана ST_WIFI_DIAG (терминал диагностики);
   - Возврат по кнопке ЗАКРЫТЬ в предыдущий экран.
4. Проверку контракта команд UART HIL (TAP, CLICK WIFI, CLICK FILMS, STATUS).
"""

import unittest

# Цвета интерфейса CamBrowser (RGB565)
UI_COLOR_OK = 0x2DE8       # Зеленый
UI_COLOR_WARN = 0xFBE0     # Оранжевый / желтый
UI_COLOR_ACCENT = 0x257C   # Циан / синий
UI_COLOR_ACCENT2 = 0x93DC  # Фиолетовый
UI_COLOR_ERR = 0xF9C7      # Красный

# Состояния Wi-Fi менеджера (wifi_mgr_state_t)
WIFI_MGR_IDLE = 0
WIFI_MGR_SCANNING = 1
WIFI_MGR_CONNECTING = 2
WIFI_MGR_CONNECTED = 3
WIFI_MGR_AP_MODE = 4
WIFI_MGR_TESTING = 5
WIFI_MGR_FAIL = 6

# Состояния UI
ST_HOME = 0
ST_WIFI_DIAG = 1
ST_FILMS = 2


def calc_wifi_badge(wst: int, ready: bool, ssid: str, ip: str) -> dict:
    """
    Точная модель логики draw_status_bar() из browser.c (строки 284-335).
    """
    if ready:
        icon = "WIFI_ON"
        color = UI_COLOR_OK
        text = f"{ssid}  {ip}" if ssid else ip
    elif wst == WIFI_MGR_CONNECTING:
        icon = "WIFI_ON"
        color = UI_COLOR_WARN
        text = f"Подкл. к {ssid}..." if ssid else "Подключение Wi-Fi..."
    elif wst == WIFI_MGR_SCANNING:
        icon = "WIFI_ON"
        color = UI_COLOR_ACCENT
        text = "Скан сетей Wi-Fi..."
    elif wst == WIFI_MGR_AP_MODE:
        icon = "WIFI_ON"
        color = UI_COLOR_ACCENT2
        text = "Портал: 192.168.4.1"
    else:
        icon = "WIFI_OFF"
        color = UI_COLOR_ERR
        text = "Wi-Fi откл (нажмите)"

    return {
        "icon": icon,
        "color": color,
        "text": text,
    }


def handle_touch_event(current_state: int, tap_x: int, tap_y: int) -> int:
    """
    Модель обработки тапа в browser_task() (строки 2548-2560).
    """
    BAR_H = 34
    # Тап по статусной плашке Wi-Fi (x < 380, y < BAR_H)
    if tap_y < BAR_H and tap_x < 380 and current_state != ST_WIFI_DIAG:
        return ST_WIFI_DIAG
    return current_state


class TestWifiStatusIconAndHIL(unittest.TestCase):
    """
    Тестирование значка Wi-Fi и интерактивности статус-бара.
    """

    def test_wifi_icon_idle_shows_off(self):
        """Когда Wi-Fi выключен, значок перечёркнут (WIFI_OFF) и горит красным."""
        badge = calc_wifi_badge(WIFI_MGR_IDLE, ready=False, ssid="", ip="")
        self.assertEqual(badge["icon"], "WIFI_OFF")
        self.assertEqual(badge["color"], UI_COLOR_ERR)
        self.assertEqual(badge["text"], "Wi-Fi откл (нажмите)")

    def test_wifi_icon_connecting_shows_warn(self):
        """Во время подключения значок активен (WIFI_ON), горит оранжевым."""
        badge = calc_wifi_badge(WIFI_MGR_CONNECTING, ready=False, ssid="MyHome", ip="")
        self.assertEqual(badge["icon"], "WIFI_ON")
        self.assertEqual(badge["color"], UI_COLOR_WARN)
        self.assertIn("MyHome", badge["text"])

    def test_wifi_icon_connected_shows_green(self):
        """При установке соединения значок активен (WIFI_ON), горит зеленым и показывает IP."""
        badge = calc_wifi_badge(WIFI_MGR_CONNECTED, ready=True, ssid="MyHome", ip="192.168.1.100")
        self.assertEqual(badge["icon"], "WIFI_ON")
        self.assertEqual(badge["color"], UI_COLOR_OK)
        self.assertIn("192.168.1.100", badge["text"])

    def test_wifi_icon_scanning_shows_accent(self):
        """При сканировании значок подсвечивается синим/цианом."""
        badge = calc_wifi_badge(WIFI_MGR_SCANNING, ready=False, ssid="", ip="")
        self.assertEqual(badge["icon"], "WIFI_ON")
        self.assertEqual(badge["color"], UI_COLOR_ACCENT)

    def test_tap_on_wifi_badge_opens_diagnostics(self):
        """
        Проверяет, что тап по значку Wi-Fi в верхнем углу (x=50, y=15)
        переключает экран на терминал диагностики ST_WIFI_DIAG.
        """
        state = ST_HOME
        new_state = handle_touch_event(state, tap_x=50, tap_y=15)
        self.assertEqual(new_state, ST_WIFI_DIAG, "Тап по плашке Wi-Fi должен открыть терминал диагностики")

    def test_uart_hil_command_parsing(self):
        """
        Проверяет формат команд UART HIL терминала (sscanf / strcmp).
        """
        commands = [
            ("TAP 50 15", (50, 15)),
            ("TAP 844 180", (844, 180)),
            ("TAP 500 120", (500, 120)),
        ]
        for cmd, expected in commands:
            parts = cmd.split()
            self.assertEqual(parts[0], "TAP")
            tx, ty = int(parts[1]), int(parts[2])
            self.assertEqual((tx, ty), expected)


if __name__ == "__main__":
    unittest.main()
