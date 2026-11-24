"""Internal resolution: the menu's saved choice reaches the runtime on the next start.

The game itself renders at the chosen size once the runtime has changed its code in memory
(src/runtime_resolution.c, tested by test_runtime_resolution.py). The launcher only passes the
saved choice on as a size. No game file is read or written.
"""
from __future__ import annotations

from pathlib import Path
import re
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ps4rt_launch as launch


class InternalResolutionTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.ini = Path(temp.name) / "ps4rt.ini"

    def launch(self, saved=None, env=None):
        if saved is not None:
            self.ini.write_text(f"upscaler=off\nrender_resolution={saved}\nvsync=1\n")
        env = dict(env or {})
        launch._internal_resolution(self.ini, env)
        return env

    def test_saved_choice_is_passed_to_the_runtime_as_a_size(self):
        expected = {"720p": "1280x720", "540p": "1024x576", "1440p": "2560x1440", "2160p": "3840x2160",
                    "2560x1080": "2560x1080", "3440x1440": "3440x1440", "3840x1440": "3840x1440",
                    "3840x1080": "3840x1080", "5120x1440": "5120x1440"}
        self.assertEqual(launch.RENDER_RESOLUTIONS, expected)
        for saved, size in expected.items():
            env = self.launch(saved)
            self.assertEqual(env["PS4RT_SO_RESOLUTION"], size)
            self.assertEqual(env["PS4RT_SO_RESOLUTION_FROM_INI"], "1")
        self.assertEqual(self.launch(" 1440P ")["PS4RT_SO_RESOLUTION"], "2560x1440")

    def test_native_unknown_or_missing_choice_leaves_the_game_alone(self):
        self.assertEqual(self.launch(), {})  # no settings file yet
        for saved in ("native", "", "480p", "4K", "1280x720", "3440x1440 ; rm", "1920x1080"):
            self.assertEqual(self.launch(saved), {}, saved)

    def test_restart_applies_the_newly_saved_choice(self):
        # "Apply and restart" hands the previous launch's environment to the new one.
        inherited = self.launch("3440x1440")
        self.assertEqual(self.launch("5120x1440", inherited)["PS4RT_SO_RESOLUTION"], "5120x1440")
        self.assertEqual(self.launch("720p", inherited)["PS4RT_SO_RESOLUTION"], "1280x720")
        self.assertEqual(self.launch("native", inherited), {})

    def test_explicit_size_from_the_caller_wins(self):
        env = self.launch("3440x1440", {"PS4RT_SO_RESOLUTION": "2560x1080"})
        self.assertEqual(env, {"PS4RT_SO_RESOLUTION": "2560x1080"})

    def test_settings_file_is_not_changed(self):
        self.launch("5120x1440")
        before = self.ini.read_text()
        self.launch(env={"PS4RT_SO_RESOLUTION_FROM_INI": "1", "PS4RT_SO_RESOLUTION": "2560x1080"})
        self.assertEqual(self.ini.read_text(), before)

    def test_launcher_menu_and_runtime_agree_on_the_sizes(self):
        graphics = (ROOT / "gpu/shim/ps4rt_graphics.h").read_text()

        def array(name, pattern):
            return re.findall(pattern, re.search(name + r" = \{([^}]*)\}", graphics).group(1))

        keys = array("Resolutions", r'"([^"]+)"')
        widths = [int(n) for n in array("ResolutionWidths", r"\d+")]
        heights = [int(n) for n in array("ResolutionHeights", r"\d+")]
        self.assertEqual(keys[0], "native")
        self.assertEqual((widths[0], heights[0]), (1920, 1080))
        self.assertEqual(keys[1:], list(launch.RENDER_RESOLUTIONS))
        for key, width, height in zip(keys[1:], widths[1:], heights[1:]):
            self.assertEqual(launch.RENDER_RESOLUTIONS[key], f"{width}x{height}")
        # The runtime's own unit test applies each of these sizes.
        runtime_test = (ROOT / "tests/test_runtime_resolution.c").read_text()
        for size in launch.RENDER_RESOLUTIONS.values():
            self.assertIn(f'{{"{size}", ', runtime_test)

    def test_menu_row_and_restart_wiring(self):
        overlay = (ROOT / "gpu/shim/ps4rt_overlay.cpp").read_text()
        compact = overlay.split("void CompactGraphics() {", 1)[1].split("void ImageTab() {", 1)[0]
        self.assertIn('Row("Internal resolution");', compact)
        self.assertIn('ResolutionChoice("##value", s.render_resolution);', compact)
        self.assertIn("Restart required. Ultrawide resolutions are experimental", compact)
        # One list under aspect headings; the headings are not selectable.
        choice = overlay.split("void ResolutionChoice(", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("for (const auto& group : G::ResolutionMenuGroups)", choice)
        self.assertIn("ImGui::SeparatorText(group.label);", choice)
        self.assertIn("Store(target, G::ResolutionMenuOrder[i], true);", choice)
        # The applied size, then FSR outputs for the selected internal resolution (the Sakura Wars
        # port's layout); FSR is offered at every internal resolution.
        self.assertIn('InfoRow("Current Applied Resolution:", text);', compact)
        self.assertIn('ChoiceRow("FSR Upscale output", s.output_res, labels, values, count,', compact)
        self.assertIn("if (!Ps4RtSettings::OutputAllowed(iw, ih, o)) continue;", compact)
        self.assertLess(compact.index('Row("Internal resolution");'), compact.index('"Current Applied Resolution:"'))
        self.assertLess(compact.index('"Current Applied Resolution:"'), compact.index('"FSR Upscale output"'))
        self.assertLess(compact.index('"FSR Upscale output"'), compact.index('"Upscaling / anti-aliasing"'))
        self.assertNotIn("available at 1920 x 1080 only", compact)
        changes = overlay.split("int RestartChanges() {", 1)[1].split("}", 1)[0]
        self.assertIn("s.render_resolution != s.startup_render_resolution", changes)
        main = (ROOT / "scripts/ps4rt_launch.py").read_text().split("def main(", 1)[1]
        self.assertIn("_internal_resolution(out / 'ps4rt.ini', env)", main)

    def test_loader_applies_the_size_before_the_game_starts(self):
        loader = (ROOT / "src/loader.c").read_text()
        self.assertIn("runtime_resolution_apply(image, size);", loader)
        self.assertLess(loader.index("runtime_resolution_apply(image, size);"),
                        loader.index("isa_rewrite_apply(isa_table, image, stubs, isa_sets)"))


if __name__ == "__main__":
    unittest.main()
