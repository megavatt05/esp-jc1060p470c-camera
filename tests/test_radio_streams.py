"""
Unit tests for CamBrowser Radio Streaming & Icecast Protocol Handler.
Mirrors stream validation logic in main/media/media_player.c.
"""

import unittest

class RadioStreamValidator:
    """Validates Icecast HTTP streaming response headers and MIME types."""

    VALID_AUDIO_TYPES = {
        "audio/mpeg",
        "audio/mp3",
        "audio/aac",
        "audio/aacp",
        "audio/x-aac",
        "application/ogg"
    }

    @classmethod
    def validate_stream_response(cls, status_code: int, headers: dict) -> tuple[bool, str]:
        if status_code != 200:
            return False, f"HTTP Error {status_code}: сервер вещания вернул ошибку"

        content_type = headers.get("content-type", "").lower()
        # strip charset or parameters if any
        base_type = content_type.split(";")[0].strip()

        if base_type not in cls.VALID_AUDIO_TYPES:
            return False, f"Неподдерживаемый MIME-тип: '{base_type}' (ожидается audio/mpeg или audio/aac)"

        return True, "Поток валиден и готов к декодированию"


class TestRadioStreams(unittest.TestCase):

    def test_valid_icecast_mp3_response(self):
        headers = {
            "content-type": "audio/mpeg",
            "server": "Icecast 2.4.0",
            "icy-name": "Europa Plus 128k",
            "icy-br": "128"
        }
        ok, msg = RadioStreamValidator.validate_stream_response(200, headers)
        self.assertTrue(ok)
        self.assertIn("готов к декодированию", msg)

    def test_server_error_rejected(self):
        headers = {"content-type": "text/html"}
        ok, msg = RadioStreamValidator.validate_stream_response(500, headers)
        self.assertFalse(ok)
        self.assertIn("HTTP Error 500", msg)

    def test_non_audio_mime_rejected(self):
        # When stream URL redirects to a web page or captchas
        headers = {"content-type": "text/html; charset=UTF-8"}
        ok, msg = RadioStreamValidator.validate_stream_response(200, headers)
        self.assertFalse(ok)
        self.assertIn("Неподдерживаемый MIME-тип", msg)

    def test_aac_stream_accepted(self):
        headers = {"content-type": "audio/aacp"}
        ok, msg = RadioStreamValidator.validate_stream_response(200, headers)
        self.assertTrue(ok)


if __name__ == "__main__":
    unittest.main()
