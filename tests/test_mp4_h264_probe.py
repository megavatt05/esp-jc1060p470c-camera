"""
Unit tests for CamBrowser MP4 Container & H.264 Profile Validator.
Mirrors the C probe logic in main/browser/films.c and esp_h264 / tinyh264 limitations.
"""

import unittest
import struct

class Mp4H264Probe:
    """Reference implementation of films.c MP4 box parser and H.264 validator."""

    PROFILES = {
        66: "Baseline",
        77: "Main",
        88: "Extended",
        100: "High",
        110: "High 10",
        122: "High 4:2:2",
        244: "High 4:4:4 Predictive"
    }

    @classmethod
    def parse_avcc(cls, payload: bytes):
        """
        avcC atom structure:
        byte 0: configurationVersion (1)
        byte 1: AVCProfileIndication (66 = Baseline, 77 = Main, 100 = High)
        byte 2: profile_compatibility (bit 6 = constraint_set1_flag)
        byte 3: AVCLevelIndication
        """
        if len(payload) < 4:
            return None
        ver, profile, compat, level = struct.unpack(">BBBB", payload[:4])
        profile_name = cls.PROFILES.get(profile, f"Unknown ({profile})")
        has_constraint_set1 = bool(compat & 0x40)

        # Baseline (66) or Main (77) with constraint_set1_flag acts as Constrained Baseline
        is_constrained_baseline = (profile == 66) or (profile == 77 and has_constraint_set1)

        return {
            "version": ver,
            "profile_idc": profile,
            "profile_name": profile_name,
            "compat": compat,
            "level": level,
            "is_constrained_baseline": is_constrained_baseline,
        }

    @classmethod
    def validate_stream_compatibility(cls, avcc_info: dict, has_ctts_box: bool) -> tuple[bool, str]:
        """
        Validates if ESP32-P4's tinyH264 decoder can play this stream.
        P4 tinyH264 requires:
        1. Constrained Baseline Profile (profile_idc == 66 or Main with constraint_set1).
        2. No B-frames (absence of 'ctts' box in video track).
        """
        if not avcc_info:
            return False, "Не найден атом avcC с параметрами H.264"

        if has_ctts_box:
            return False, "Обнаружены B-кадры (атом ctts). tinyH264 не поддерживает B-frames."

        if not avcc_info["is_constrained_baseline"]:
            return False, (
                f"Неподдерживаемый профиль: {avcc_info['profile_name']} (IDC {avcc_info['profile_idc']}). "
                f"tinyH264 поддерживает только Constrained Baseline Profile."
            )

        return True, "Совместимо с ESP32-P4 tinyH264"


class TestMp4H264Probe(unittest.TestCase):

    def test_baseline_profile_accepted(self):
        # Profile 66 (Baseline), compat = 0x00, level = 21 (2.1)
        avcc_payload = bytes([0x01, 66, 0x00, 21, 0xFF, 0xE1])
        info = Mp4H264Probe.parse_avcc(avcc_payload)
        self.assertIsNotNone(info)
        self.assertEqual(info["profile_name"], "Baseline")
        self.assertTrue(info["is_constrained_baseline"])

        compatible, msg = Mp4H264Probe.validate_stream_compatibility(info, has_ctts_box=False)
        self.assertTrue(compatible)
        self.assertIn("Совместимо", msg)

    def test_high_profile_rejected(self):
        # Profile 100 (High Profile), compat = 0x00, level = 41 (4.1)
        avcc_payload = bytes([0x01, 100, 0x00, 41, 0xFF, 0xE1])
        info = Mp4H264Probe.parse_avcc(avcc_payload)
        self.assertIsNotNone(info)
        self.assertEqual(info["profile_name"], "High")
        self.assertFalse(info["is_constrained_baseline"])

        compatible, msg = Mp4H264Probe.validate_stream_compatibility(info, has_ctts_box=False)
        self.assertFalse(compatible)
        self.assertIn("Неподдерживаемый профиль", msg)

    def test_main_profile_without_constraints_rejected(self):
        # Profile 77 (Main), no constraint_set1
        avcc_payload = bytes([0x01, 77, 0x00, 31, 0xFF, 0xE1])
        info = Mp4H264Probe.parse_avcc(avcc_payload)
        self.assertFalse(info["is_constrained_baseline"])

        compatible, msg = Mp4H264Probe.validate_stream_compatibility(info, has_ctts_box=False)
        self.assertFalse(compatible)

    def test_main_profile_with_constraint_set1_accepted(self):
        # Profile 77 (Main) + constraint_set1_flag (0x40 in compat byte)
        avcc_payload = bytes([0x01, 77, 0x40, 31, 0xFF, 0xE1])
        info = Mp4H264Probe.parse_avcc(avcc_payload)
        self.assertTrue(info["is_constrained_baseline"])

        compatible, msg = Mp4H264Probe.validate_stream_compatibility(info, has_ctts_box=False)
        self.assertTrue(compatible)

    def test_b_frames_ctts_rejected_even_for_baseline(self):
        # Baseline profile, but container has B-frame composition offset table (ctts)
        avcc_payload = bytes([0x01, 66, 0x00, 21, 0xFF, 0xE1])
        info = Mp4H264Probe.parse_avcc(avcc_payload)

        compatible, msg = Mp4H264Probe.validate_stream_compatibility(info, has_ctts_box=True)
        self.assertFalse(compatible)
        self.assertIn("Обнаружены B-кадры", msg)


if __name__ == "__main__":
    unittest.main()
