"""
Unit tests for CamBrowser HTML & Search Engine (SERP) Parsing Logic.
Mirrors the C implementation in main/browser/html_text.c and web_client.c.
"""

import unittest
import urllib.parse
import re

class HtmlSerpParser:
    """Python reference model matching main/browser/html_text.c logic."""

    @staticmethod
    def decode_entity(text: str) -> str:
        entities = {
            "&quot;": '"',
            "&amp;": "&",
            "&lt;": "<",
            "&gt;": ">",
            "&#39;": "'",
            "&nbsp;": " ",
            "&#x27;": "'",
            "&#x2F;": "/"
        }
        for ent, ch in entities.items():
            text = text.replace(ent, ch)
        return text

    @staticmethod
    def strip_blocks(html: str) -> str:
        blocks = ["script", "style", "svg", "noscript"]
        result = html
        for b in blocks:
            pattern = re.compile(rf"<{b}[^>]*>.*?</{b}>", re.IGNORECASE | re.DOTALL)
            result = pattern.sub("", result)
        return result

    @staticmethod
    def resolve_href(href: str) -> str:
        if "duckduckgo.com/y.js" in href:
            return ""  # Ad link filtered out
        if "uddg=" in href:
            idx = href.find("uddg=") + 5
            amp = href.find("&", idx)
            enc = href[idx:amp] if amp != -1 else href[idx:]
            return urllib.parse.unquote(enc)
        if href.startswith("//"):
            return "https:" + href
        return href

    @classmethod
    def parse_serp_ddg(cls, html: str, max_results: int = 10) -> list:
        html = cls.strip_blocks(html)
        results = []
        pattern = re.compile(
            r'<a[^>]*class="[^"]*result__a[^"]*"[^>]*href="([^"]+)"[^>]*>(.*?)</a>',
            re.IGNORECASE | re.DOTALL
        )
        for match in pattern.finditer(html):
            if len(results) >= max_results:
                break
            raw_href, raw_text = match.group(1), match.group(2)
            url = cls.resolve_href(raw_href)
            if not url or "/search?" in url or "accounts.google.com" in url:
                continue
            clean_title = re.sub(r'<[^>]+>', '', raw_text)
            clean_title = cls.decode_entity(clean_title).strip()
            if clean_title and url:
                results.append({"url": url, "title": clean_title})
        return results

    @classmethod
    def html_to_text(cls, html: str) -> str:
        cleaned = cls.strip_blocks(html)
        body_match = re.search(r'<body[^>]*>(.*)', cleaned, re.IGNORECASE | re.DOTALL)
        if body_match:
            cleaned = body_match.group(1)
        # Block elements to newline
        cleaned = re.sub(r'<(p|br|div|h[1-6]|li|tr)[^>]*>', '\n', cleaned, flags=re.IGNORECASE)
        # Strip all other tags
        text = re.sub(r'<[^>]+>', ' ', cleaned)
        text = cls.decode_entity(text)
        # Collapse whitespace
        lines = [re.sub(r'[ \t]+', ' ', line).strip() for line in text.split('\n')]
        return '\n'.join(line for line in lines if line)


class TestHtmlSerpParser(unittest.TestCase):

    def setUp(self):
        self.parser = HtmlSerpParser()

    def test_url_decode_percent_escapes(self):
        raw = "https%3A%2F%2Fstackoverflow.com%2Fquestions%2F27125672%2Fwhat-is-r"
        expected = "https://stackoverflow.com/questions/27125672/what-is-r"
        self.assertEqual(self.parser.resolve_href(f"//duckduckgo.com/l/?uddg={raw}&rut=123"), expected)

    def test_url_decode_cyrillic_utf8(self):
        query = "поиск в интернете"
        encoded = urllib.parse.quote(query)
        self.assertEqual(encoded, "%D0%BF%D0%BE%D0%B8%D1%81%D0%BA%20%D0%B2%20%D0%B8%D0%BD%D1%82%D0%B5%D1%80%D0%BD%D0%B5%D1%82%D0%B5")
        decoded = urllib.parse.unquote(encoded)
        self.assertEqual(decoded, query)

    def test_ddg_ad_links_are_filtered(self):
        ad_href = "//duckduckgo.com/y.js?ad_id=12345"
        self.assertEqual(self.parser.resolve_href(ad_href), "")

    def test_strip_blocks_removes_scripts_and_styles(self):
        snippet = "<html><head><script>alert(1);</script><style>body{color:red;}</style></head><body>Hello</body></html>"
        cleaned = self.parser.strip_blocks(snippet)
        self.assertNotIn("alert(1)", cleaned)
        self.assertNotIn("color:red", cleaned)
        self.assertIn("Hello", cleaned)

    def test_parse_serp_ddg_real_markup(self):
        sample_html = """
        <div class="results">
            <h2 class="result__title">
                <a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fhabr.com%2Fru%2Fpost%2F123%2F&amp;rut=abc">
                    Что такое <b>ESP32-P4</b> &amp; как его готовить
                </a>
            </h2>
            <div class="result__snippet">Новый двухъядерный чип RISC-V на 400 МГц...</div>
            <h2 class="result__title">
                <a class="result__a" href="//duckduckgo.com/y.js?ad=1">Рекламная ссылка</a>
            </h2>
            <h2 class="result__title">
                <a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=https%3A%2F%2Fgithub.com%2Fespressif%2Fesp-idf&amp;rut=xyz">
                    GitHub - espressif/esp-idf
                </a>
            </h2>
        </div>
        """
        results = self.parser.parse_serp_ddg(sample_html, max_results=5)
        self.assertEqual(len(results), 2)
        self.assertEqual(results[0]["url"], "https://habr.com/ru/post/123/")
        self.assertEqual(results[0]["title"], "Что такое ESP32-P4 & как его готовить")
        self.assertEqual(results[1]["url"], "https://github.com/espressif/esp-idf")
        self.assertEqual(results[1]["title"], "GitHub - espressif/esp-idf")

    def test_html_to_text_formatting(self):
        html_page = """
        <html>
        <body>
            <h1>Заголовок H1</h1>
            <p>Первый параграф текста &quot;в кавычках&quot;.</p>
            <div>Второй блок &lt;div&gt;.</div>
        </body>
        </html>
        """
        text = self.parser.html_to_text(html_page)
        lines = text.split('\n')
        self.assertIn("Заголовок H1", lines)
        self.assertIn('Первый параграф текста "в кавычках".', lines)
        self.assertIn('Второй блок <div>.', lines)


if __name__ == "__main__":
    unittest.main()
