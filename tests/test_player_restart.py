"""Guard Sakura-style player restart wiring and confirmation."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PlayerRestartTests(unittest.TestCase):
    def test_confirmed_button_in_compact_game_menu(self):
        overlay = (ROOT / "gpu/shim/ps4rt_overlay.cpp").read_text()
        compact = overlay.split("void Menu() {", 1)[1].split("constexpr int TabCount", 1)[0]
        self.assertIn('ImGui::Button("Exit Game")', compact)
        self.assertIn('ImGui::Button("Restart Game")', compact)
        self.assertIn('ImGui::BeginPopupModal("Restart game?"', compact)
        self.assertIn('Unsaved game progress will be lost.', compact)
        self.assertIn("Ps4RtSettings::Save();\n                dirty = false;\n                runtime_restart();", compact)
        self.assertNotIn('Button("Install DLC', compact)
        self.assertIn('!ImGui::IsPopupOpen("Restart game?")', compact)

    def test_escape_cancels_confirmation(self):
        overlay = (ROOT / "gpu/shim/ps4rt_overlay.cpp").read_text()
        events = overlay.split("bool HandleEvent(", 1)[1]
        self.assertIn('ImGui::IsPopupOpen("Restart game?")', events)

    def test_launcher_restarts_selected_game(self):
        launcher = (ROOT / "scripts/ps4rt_launch.py").read_text()
        self.assertIn("'--game', str(game)", launcher)
        self.assertIn("env['PS4RT_RESTART_COMMAND'] = shlex.join(restart)", launcher)
        self.assertIn("restart += ['--upscaler', args.upscaler]", launcher)

    def test_gpu_descriptors_are_closed_at_exec(self):
        source = (ROOT / "src/loader.c").read_text()
        self.assertIn("SYS_close_range, 3u, ~0u, 4u", source)


if __name__ == "__main__":
    unittest.main()
