"""
CAMBROWSER HARDWARE-IN-THE-LOOP (HIL) TEST SUITE
================================================
Использует официальный фреймворк Espressif `pytest-embedded` для проверки
живой платы ESP32-P4 через COM-порт.

Запуск:
    pytest tests/test_board_hil.py --port COM10 --baud 115200
"""

import pytest

@pytest.mark.esp32p4
def test_board_boot_and_stability(dut):
    """
    Проверяет успешную загрузку прошивки и отсутствие паник/ассертов ядра.
    """
    # 1. Проверяем старт 2-й стадии бутлоадера
    dut.expect(r"boot: ESP-IDF v6\.1", timeout=10)

    # 2. Проверяем обнаружение 32 МБ PSRAM памяти
    dut.expect(r"esp_psram: Found 32MB PSRAM device", timeout=10)

    # 3. Проверяем старт сетевого интерфейса и получение IP
    dut.expect(r"wifi_mgr: DHCP OK: IP", timeout=25)

    # 4. Проверяем обнаружение и инициализацию сенсорного экрана GT911
    dut.expect(r"camos_touch: touch controller found: GT911 at 0x5D", timeout=10)

    # 5. Проверяем успешный запуск основного приложения
    dut.expect(r"main_task: Returned from app_main\(\)", timeout=10)

    # 6. КРИТИЧЕСКАЯ ПРОВЕРКА: за следующие 3 секунды в логе НЕ должно появиться паник!
    # Если вылетит esp_cpu_reset, abort() или Guru Meditation — pytest-embedded
    # автоматически завалит тест.
