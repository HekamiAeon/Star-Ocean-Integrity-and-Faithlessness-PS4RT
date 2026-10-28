"""Regression guard for the default-off, explicit-opt-in draw pipeline."""
from pathlib import Path
import unittest

RASTERIZER = (Path(__file__).resolve().parents[1] /
              "gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp")


class DrawPipelineDefaultTests(unittest.TestCase):
    def test_default_disabled_with_explicit_override(self):
        source = RASTERIZER.read_text(encoding="utf-8")
        signature = "bool Rasterizer::DrawPipeWanted() {"
        self.assertEqual(source.count(signature), 1)
        method = source.split(signature, 1)[1].split("\n}", 1)[0]
        self.assertIn('std::getenv("PS4RT_DRAW_PIPE")', method)
        self.assertIn("return env[0] == '1';", method)
        self.assertTrue(method.rstrip().endswith("return false;"))
        self.assertNotIn("Ps4RtThreads::Available()", method)


if __name__ == "__main__":
    unittest.main()
