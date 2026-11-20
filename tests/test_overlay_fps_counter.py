"""Static guard for the compact menu's FPS counter checkbox (as in the Sakura Wars and Valkyria
Revolution ports): the same show_fps setting as the full menu's Overlay tab."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / "gpu/shim/ps4rt_overlay.cpp"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    depth, i = 0, source.index("{", start)
    for j in range(i, len(source)):
        depth += {"{": 1, "}": -1}.get(source[j], 0)
        if depth == 0:
            return source[i:j + 1]
    raise AssertionError(signature)


class OverlayFpsCounterTests(unittest.TestCase):
    def setUp(self):
        self.source = OVERLAY.read_text(encoding="utf-8")

    def test_compact_graphics_has_the_checkbox_after_display_mode(self):
        body = function_body(self.source, "void CompactGraphics()")
        row = body.index('CheckRow("FPS counter", s.show_fps,')
        self.assertLess(body.index('"Display mode"'), row)
        self.assertLess(row, body.index('"Upscaling / anti-aliasing"'))

    def test_full_menu_keeps_its_overlay_toggle(self):
        body = function_body(self.source, "void OverlayTab()")
        self.assertIn('CheckRow("Show overlay", s.show_fps,', body)


if __name__ == "__main__":
    unittest.main()
