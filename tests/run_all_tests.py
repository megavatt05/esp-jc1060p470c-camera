"""
Master Test Runner for CamBrowser (JC1060P470C) Unit Test Suite.
Executes all unit tests and generates metrics.
"""

import unittest
import time
import sys
import json

from test_html_parser import TestHtmlSerpParser
from test_mp4_h264_probe import TestMp4H264Probe
from test_touch_calib import TestTouchCalibration
from test_radio_streams import TestRadioStreams
from test_freertos_contracts import TestFreeRTOSApiContracts
from test_ui_layout_bounds import TestUiLayoutAndTextBounds
from test_films_search_flow import TestFilmsSearchAndPlaybackFlow
from test_wifi_status_icon import TestWifiStatusIconAndHIL

def run_suite():
    suite = unittest.TestSuite()
    loader = unittest.TestLoader()

    modules = [
        ("HTML & SERP Parser (Поиск & Web)", TestHtmlSerpParser),
        ("MP4 & H.264 Video Probe (Видео)", TestMp4H264Probe),
        ("Touchscreen Calibration (Тач & UI)", TestTouchCalibration),
        ("Radio Streams Validator (Аудиопотоки)", TestRadioStreams),
        ("FreeRTOS & RTOS API Contracts (Ядро & Очереди)", TestFreeRTOSApiContracts),
        ("UI Layout Bounds & User Simulation (Границы текста & Эмуляция)", TestUiLayoutAndTextBounds),
        ("Films Catalog & Playback Flow (Поиск 'r' & Валидация фильмов)", TestFilmsSearchAndPlaybackFlow),
        ("Wi-Fi Status Icon & HIL Contracts (Значок Wi-Fi & Терминал HIL)", TestWifiStatusIconAndHIL),
    ]

    for name, cls in modules:
        suite.addTests(loader.loadTestsFromTestCase(cls))

    start_time = time.perf_counter()
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    elapsed = time.perf_counter() - start_time

    summary = {
        "total_tests": result.testsRun,
        "passed": result.testsRun - len(result.failures) - len(result.errors),
        "failed": len(result.failures),
        "errors": len(result.errors),
        "success_rate": round(100.0 * (result.testsRun - len(result.failures) - len(result.errors)) / max(1, result.testsRun), 1),
        "elapsed_seconds": round(elapsed, 4),
        "modules_tested": len(modules)
    }

    print("\n" + "=" * 60)
    print("      CAMBROWSER UNIT TEST SUITE - EXECUTION SUMMARY")
    print("=" * 60)
    print(f" Модулей протестировано : {summary['modules_tested']}")
    print(f" Всего тестов выполнено : {summary['total_tests']}")
    print(f" Успешно (PASSED)       : {summary['passed']}")
    print(f" Провалено (FAILED)     : {summary['failed']}")
    print(f" Ошибок (ERRORS)        : {summary['errors']}")
    print(f" Успешность (Pass Rate) : {summary['success_rate']}%")
    print(f" Время выполнения       : {summary['elapsed_seconds']} сек")
    print("=" * 60)

    with open("tests/test_results.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)

    return 0 if result.wasSuccessful() else 1

if __name__ == "__main__":
    sys.exit(run_suite())
