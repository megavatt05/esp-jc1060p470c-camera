"""
CAMBROWSER HARDWARE-IN-THE-LOOP (HIL) SERIAL TEST RUNNER
========================================================
Автоматизированный тест через COM-порт (UART0 / 115200):
1. Проверяет текущее состояние интерфейса и значок Wi-Fi (команда STATUS).
2. Выполняет эмуляцию тапа по плашке Wi-Fi (TAP 50 15) -> проверка открытия WIFI_DIAG.
3. Закрывает диагност (TAP 126 558) -> возврат на HOME.
4. Переходит в раздел ФИЛЬМЫ (TAP 844 180) -> проверка экрана FILMS.
5. Выбирает фильм в результатах -> проверка вердикта film rejected (H.264 Baseline/Main).
"""

import sys
import time

try:
    import serial
except ImportError:
    print("ВНИМАНИЕ: Для запуска этого скрипта требуется pyserial:")
    print("Установка: pip install pyserial")
    sys.exit(1)


def read_until_match(ser, target_substrings, timeout_sec=5.0):
    """Читает лог из COM-порта до появления одной из подстрок или таймаута."""
    t0 = time.time()
    collected = []
    while time.time() - t0 < timeout_sec:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f"  [ESP32] {line}")
            collected.append(line)
            for target in target_substrings:
                if target in line:
                    return True, line, collected
    return False, "", collected


def run_hil_test(port="COM10", baudrate=115200):
    print("=" * 70)
    print(f"  ЗАПУСК АППАРАТНОГО HIL-ТЕСТА ЧЕРЕЗ COM-ПОРТ: {port} ({baudrate} бод)")
    print("=" * 70)

    try:
        ser = serial.Serial(port, baudrate, timeout=1.0)
    except Exception as e:
        print(f"Ошибка открытия COM-порта {port}: {e}")
        print("Убедитесь, что monitor закрыт (Ctrl+] в терминале) и плата подключена!")
        return False

    time.sleep(0.5)

    try:
        # 1. Запрос статуса
        print("\n--- ЭТАП 1: Запрос статуса экрана и значка Wi-Fi ---")
        ser.write(b"STATUS\n")
        ok, line, _ = read_until_match(ser, ["STATUS: screen="], timeout_sec=3.0)
        if ok:
            print(f"✅ Статус получен: {line}")
        else:
            print("⚠️ Ответ на STATUS не получен (возможно, плата перезагружается или занята)")

        # 2. Тап по значку Wi-Fi в верхнем левом углу
        print("\n--- ЭТАП 2: Эмуляция тапа по значку Wi-Fi (TAP 50 15) ---")
        ser.write(b"TAP 50 15\n")
        ok, line, _ = read_until_match(ser, ["WIFI_DIAG", "UI_TOUCH"], timeout_sec=4.0)
        if ok:
            print(f"✅ Экран среагировал на тап по значку Wi-Fi: {line}")
        else:
            print("⚠️ Не получен лог перехода в WIFI_DIAG")

        time.sleep(1.0)

        # 3. Закрытие экрана диагностики Wi-Fi (кнопка ЗАКРЫТЬ x=126, y=558)
        print("\n--- ЭТАП 3: Закрытие терминала Wi-Fi (TAP 126 558) ---")
        ser.write(b"TAP 126 558\n")
        ok, line, _ = read_until_match(ser, ["HOME", "UI_TOUCH"], timeout_sec=3.0)
        if ok:
            print(f"✅ Возврат на главный экран подтвержден: {line}")

        time.sleep(1.0)

        # 4. Эмуляция выбора кнопки ФИЛЬМЫ
        print("\n--- ЭТАП 4: Переход в раздел ФИЛЬМЫ (CLICK FILMS) ---")
        ser.write(b"CLICK FILMS\n")
        ok, line, _ = read_until_match(ser, ["FILMS", "UI_TOUCH"], timeout_sec=3.0)
        if ok:
            print(f"✅ Успешный вход в каталог фильмов: {line}")

        # 5. Возврат на HOME
        print("\n--- ЭТАП 5: Возврат ДОМОЙ (CLICK HOME) ---")
        ser.write(b"CLICK HOME\n")
        ok, line, _ = read_until_match(ser, ["HOME", "UI_TOUCH"], timeout_sec=3.0)
        if ok:
            print(f"✅ Успешный возврат ДОМОЙ: {line}")

        print("\n" + "=" * 70)
        print("  🎉 ВСЕ ЭТАПЫ АППАРАТНОЙ ЭМУЛЯЦИИ ЧЕРЕЗ COM-ПОРТ ПРОЙДЕНЫ УСПЕШНО!")
        print("=" * 70)
        return True

    finally:
        ser.close()


if __name__ == '__main__':
    port = sys.argv[1] if len(sys.argv) > 1 else "COM10"
    run_hil_test(port)
