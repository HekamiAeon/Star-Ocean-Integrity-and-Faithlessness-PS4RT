"""Static guard: FSR 4 reads the game's tonemapped frame as display-range input, like FSR 3.

The scaled path (RunScaled, used by the compact menu) feeds the upscalers the game's tonemapped
8-bit frame; FSR 3 and DLSS already treat it as display-range. FSR 4 read it as HDR with auto
exposure and re-exposed an already exposed image (Sakura Wars 87c6ed6). The native-AA path
(Run) keeps HDR input for linear scene colour.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
RENDERER = ROOT / "gpu/shadps4/video_core/renderer_vulkan"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    depth, i = 0, source.index("{", start)
    for j in range(i, len(source)):
        depth += {"{": 1, "}": -1}.get(source[j], 0)
        if depth == 0:
            return source[i:j + 1]
    raise AssertionError(signature)


def fsr4_range(body: str) -> str:
    calls = re.findall(r"RecordFsr4\(cmdbuf,.*?frame_ms, (true|false)\)", body, re.S)
    if len(calls) != 1:
        raise AssertionError(calls)
    return calls[0]


class Fsr4InputRangeTests(unittest.TestCase):
    def setUp(self):
        self.upscaler = (RENDERER / "vk_temporal_upscaler.cpp").read_text(encoding="utf-8")
        self.fsr4 = (RENDERER / "vk_fsr4.cpp").read_text(encoding="utf-8")

    def test_tonemapped_frame_is_display_range(self):
        body = function_body(self.upscaler, "void TemporalUpscaler::RunScaled()")
        self.assertIn("EnsureResources(w, h, ow, oh, false)", body)  # FSR 3: display-range
        self.assertEqual(fsr4_range(body), "false")

    def test_native_aa_keeps_hdr_scene_colour(self):
        body = function_body(self.upscaler, "void TemporalUpscaler::Run()")
        self.assertIn("EnsureResources(w, h, ow, oh, true)", body)
        self.assertEqual(fsr4_range(body), "true")

    def test_context_follows_the_input_range(self):
        self.assertIn("desc.flags = hdr ? FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | "
                      "FFX_UPSCALE_ENABLE_AUTO_EXPOSURE : 0;", self.fsr4)
        self.assertIn("f.hdr != hdr_input", self.fsr4)  # a range change rebuilds the context
        self.assertIn("d.enableAutoExposure = f.auto_exposure && f.hdr;", self.fsr4)


if __name__ == "__main__":
    unittest.main()
