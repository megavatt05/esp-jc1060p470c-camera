"""
Unit tests for CamBrowser Touchscreen Calibration and Hit-Testing.
Mirrors logic in main/camos/touch.c and main/browser/browser.c.
"""

import unittest

LCD_W = 1024
LCD_H = 600

class TouchCalibration:
    """Simulates the 1024x600 capacitive touch coordinate mapping and hit-testing."""

    def __init__(self, raw_min_x=0, raw_max_x=1023, raw_min_y=0, raw_max_y=599, invert_x=False, invert_y=False):
        self.raw_min_x = raw_min_x
        self.raw_max_x = raw_max_x
        self.raw_min_y = raw_min_y
        self.raw_max_y = raw_max_y
        self.invert_x = invert_x
        self.invert_y = invert_y

    def map_point(self, raw_x: int, raw_y: int) -> tuple[int, int]:
        # Clamp raw inputs
        clamped_x = max(self.raw_min_x, min(raw_x, self.raw_max_x))
        clamped_y = max(self.raw_min_y, min(raw_y, self.raw_max_y))

        # Scale to 0..1023 and 0..599
        span_x = max(1, self.raw_max_x - self.raw_min_x)
        span_y = max(1, self.raw_max_y - self.raw_min_y)

        screen_x = int((clamped_x - self.raw_min_x) * (LCD_W - 1) / span_x)
        screen_y = int((clamped_y - self.raw_min_y) * (LCD_H - 1) / span_y)

        if self.invert_x:
            screen_x = (LCD_W - 1) - screen_x
        if self.invert_y:
            screen_y = (LCD_H - 1) - screen_y

        return screen_x, screen_y

    @staticmethod
    def hit_test_button(screen_x: int, screen_y: int, btn_x: int, btn_y: int, btn_w: int, btn_h: int) -> bool:
        return (btn_x <= screen_x < btn_x + btn_w) and (btn_y <= screen_y < btn_y + btn_h)


class TestTouchCalibration(unittest.TestCase):

    def setUp(self):
        self.calib_direct = TouchCalibration(0, 1023, 0, 599)
        self.calib_offset = TouchCalibration(100, 900, 50, 550)

    def test_direct_corners(self):
        self.assertEqual(self.calib_direct.map_point(0, 0), (0, 0))
        self.assertEqual(self.calib_direct.map_point(1023, 599), (1023, 599))
        self.assertEqual(self.calib_direct.map_point(512, 300), (512, 300))

    def test_out_of_bounds_clamping(self):
        # Negative coords and overflow coords must be safely clamped
        self.assertEqual(self.calib_direct.map_point(-100, -50), (0, 0))
        self.assertEqual(self.calib_direct.map_point(2000, 1500), (1023, 599))

    def test_offset_scaling(self):
        # When touch controller reports raw range 100..900, it must map cleanly to 0..1023
        self.assertEqual(self.calib_offset.map_point(100, 50), (0, 0))
        self.assertEqual(self.calib_offset.map_point(900, 550), (1023, 599))
        self.assertEqual(self.calib_offset.map_point(500, 300), (511, 299))

    def test_button_hit_testing(self):
        # Search GO button: x=940, y=50, w=72, h=44
        btn = {"btn_x": 940, "btn_y": 50, "btn_w": 72, "btn_h": 44}

        # Inside button
        self.assertTrue(TouchCalibration.hit_test_button(945, 60, **btn))
        self.assertTrue(TouchCalibration.hit_test_button(940, 50, **btn))  # top-left edge

        # Outside button
        self.assertFalse(TouchCalibration.hit_test_button(939, 60, **btn))  # just left
        self.assertFalse(TouchCalibration.hit_test_button(945, 49, **btn))  # just above
        self.assertFalse(TouchCalibration.hit_test_button(1012, 60, **btn)) # past right edge (940+72=1012)
        self.assertFalse(TouchCalibration.hit_test_button(945, 94, **btn))  # past bottom edge (50+44=94)


if __name__ == "__main__":
    unittest.main()
