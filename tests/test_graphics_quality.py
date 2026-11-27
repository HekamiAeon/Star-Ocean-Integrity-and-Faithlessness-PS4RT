"""Graphics quality: the menu's saved choice reaches the runtime on the next start.

The runtime limits the game's shadow maps by changing its code in memory (src/runtime_quality.c,
tested by test_runtime_quality.py) and the renderer reads the same value. The launcher only
passes the saved choice on. No game file is read or written.
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


class GraphicsQualityTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.ini = Path(temp.name) / "ps4rt.ini"

    def launch(self, saved=None, env=None):
        if saved is not None:
            self.ini.write_text(f"upscaler=off\ngraphics_quality={saved}\nvsync=1\n")
        env = dict(env or {})
        launch._graphics_quality(self.ini, env)
        return env

    def test_saved_choice_is_passed_to_the_runtime(self):
        for saved in ("high", "low"):
            self.assertEqual(self.launch(saved), {"PS4RT_SO_QUALITY": saved, "PS4RT_SO_QUALITY_FROM_INI": "1"})
        self.assertEqual(self.launch(" Low ")["PS4RT_SO_QUALITY"], "low")

    def test_max_unknown_or_missing_choice_leaves_the_game_alone(self):
        self.assertEqual(self.launch(), {})  # no settings file yet
        for saved in ("max", "", "medium", "2", "low ; rm", "ultra"):
            self.assertEqual(self.launch(saved), {}, saved)

    def test_restart_applies_the_newly_saved_choice(self):
        # "Apply and restart" hands the previous launch's environment to the new one.
        inherited = self.launch("low")
        self.assertEqual(self.launch("high", inherited)["PS4RT_SO_QUALITY"], "high")
        self.assertEqual(self.launch("max", inherited), {})

    def test_explicit_quality_from_the_caller_wins(self):
        self.assertEqual(self.launch("low", {"PS4RT_SO_QUALITY": "high"}), {"PS4RT_SO_QUALITY": "high"})

    def test_settings_file_is_not_changed(self):
        self.launch("low")
        before = self.ini.read_text()
        self.launch(env={"PS4RT_SO_QUALITY_FROM_INI": "1", "PS4RT_SO_QUALITY": "high"})
        self.assertEqual(self.ini.read_text(), before)

    def test_launcher_menu_and_runtime_agree_on_the_choices(self):
        graphics = (ROOT / "gpu/shim/ps4rt_graphics.h").read_text()
        keys = re.findall(r'"([^"]*)"', re.search(r"QualityKeys = \{([^}]*)\}", graphics).group(1))
        self.assertEqual(keys, ["max", "high", "low"])
        self.assertEqual(tuple(keys[1:]), launch.GRAPHICS_QUALITIES)
        runtime = (ROOT / "src/runtime_quality.c").read_text()
        for index, key in enumerate(keys):
            self.assertRegex(runtime, r'strcmp\(text, "%s"\)\) return %d;' % (key, index))


if __name__ == "__main__":
    unittest.main()
