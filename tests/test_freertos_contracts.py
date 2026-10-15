"""
CAMBROWSER STATIC & ARCHITECTURAL CONTRACT TEST SUITE
=====================================================
Проверка системных контрактов FreeRTOS и правил использования API Espressif.
Ловит ошибки конфигурации очередей, стеков задач и недопустимые вызовы
до этапа прошивки микроконтроллера.
"""

import os
import re
import unittest

class TestFreeRTOSApiContracts(unittest.TestCase):
    """
    Проверяет Си-исходники проекта на соответствие контрактам ядра FreeRTOS.
    """

    @classmethod
    def setUpClass(cls):
        cls.proj_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        cls.browser_c_path = os.path.join(cls.proj_root, "main", "browser", "browser.c")
        with open(cls.browser_c_path, "r", encoding="utf-8") as f:
            cls.browser_c_src = f.read()

    def test_xqueue_overwrite_requires_length_one(self):
        """
        ПРАВИЛО FreeRTOS: xQueueOverwrite() допустим ТОЛЬКО для очередей длиной 1.
        В queue.c срабатывает configASSERT(pxQueue->uxLength == 1).
        Если очередь создана длиной > 1, это немедленный крах чипа!
        """
        # 1. Проверяем, вызывается ли xQueueOverwrite в browser.c
        has_overwrite = "xQueueOverwrite" in self.browser_c_src
        self.assertTrue(has_overwrite, "browser.c должен использовать xQueueOverwrite для атомарных команд")

        # 2. Ищем создание очереди s_radio_queue: xQueueCreate(len, ...)
        match = re.search(r's_radio_queue\s*=\s*xQueueCreate\s*\(\s*(\d+)\s*,', self.browser_c_src)
        self.assertIsNotNone(match, "Не найдено создание очереди s_radio_queue = xQueueCreate(...)")

        queue_len = int(match.group(1))
        # Жесткая проверка: длина ОБЯЗАНА быть ровно 1!
        self.assertEqual(
            queue_len, 1,
            f"КРИТИЧЕСКИЙ СБОЙ FreeRTOS: s_radio_queue создана с длиной {queue_len}! "
            f"Для xQueueOverwrite длина ОБЯЗАНА быть ровно 1, иначе сработает configASSERT и чип перезагрузится."
        )

    def test_worker_task_stack_is_sufficient(self):
        """
        Проверяет, что фоновые воркеры имеют достаточный стек (не менее 8 КБ)
        для предотвращения переполнения стека (Stack Overflow).
        """
        # Ищем xTaskCreatePinnedToCore для radio_worker
        match = re.search(r'xTaskCreatePinnedToCore\s*\(\s*radio_worker_task\s*,[^,]+,\s*(\d+(?:\s*\*\s*\d+)?)\s*,', self.browser_c_src)
        self.assertIsNotNone(match, "Не найдено создание задачи radio_worker_task")

        stack_expr = match.group(1)
        stack_bytes = eval(stack_expr)
        self.assertGreaterEqual(
            stack_bytes, 8192,
            f"Стек задачи radio_worker_task ({stack_bytes} байт) слишком мал! "
            f"Сетевой стек и плеер требуют минимум 8192 байт."
        )

    def test_radio_worker_initialized_at_boot(self):
        """
        Проверяет, что radio_worker_init_once() вызывается при старте браузера (browser_start),
        а не только лениво при первом таче (устранение гонок и задержек).
        """
        match = re.search(r'void\s+browser_start\s*\(void\).*?radio_worker_init_once\s*\(\s*\);',
                          self.browser_c_src, re.DOTALL)
        self.assertIsNotNone(
            match,
            "radio_worker_init_once() должен вызываться в browser_start() для пред-инициализации структур"
        )

    def test_all_state_enums_covered_in_telemetry(self):
        """
        Проверяет, что функция телеметрии state_to_str() покрывает все состояния browser_state_t.
        """
        # Ищем определение enum browser_state_t
        enum_match = re.search(r'typedef\s+enum\s*\{([^}]+)\}\s*browser_state_t;', self.browser_c_src)
        self.assertIsNotNone(enum_match, "Не найден enum browser_state_t")

        states = re.findall(r'(ST_[A-Z0-9_]+)', enum_match.group(1))
        self.assertTrue(len(states) > 0, "Список состояний не должен быть пустым")

        # Ищем switch в state_to_str
        func_match = re.search(r'state_to_str\s*\([^)]*\)\s*\{([^}]+)\}', self.browser_c_src)
        self.assertIsNotNone(func_match, "Не найдена функция state_to_str")

        covered_cases = re.findall(r'case\s+(ST_[A-Z0-9_]+):', func_match.group(1))
        # Проверяем ключевые состояния экрана
        key_states = ["ST_SPLASH", "ST_HOME", "ST_RADIO", "ST_VIDEO", "ST_TOUCH_TEST"]
        for st in key_states:
            self.assertIn(st, covered_cases, f"Состояние {st} должно присутствовать в state_to_str()")


if __name__ == "__main__":
    unittest.main()
