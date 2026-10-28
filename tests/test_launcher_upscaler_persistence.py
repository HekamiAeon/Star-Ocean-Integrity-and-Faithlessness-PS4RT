"""Guard persisted FSR upscaling against unconditional launch overrides."""
import ast
from pathlib import Path
import unittest

LAUNCHER = Path(__file__).resolve().parents[1] / "scripts/ps4rt_launch.py"


class UpscalerPersistenceTests(unittest.TestCase):
    def test_saved_upscaler_is_not_forced_off(self):
        source = LAUNCHER.read_text(encoding="utf-8")
        ast.parse(source, filename=str(LAUNCHER))
        self.assertNotIn("PS4RT_UPSCALER='off'", source)
        self.assertIn("if args.upscaler is not None:", source)
        self.assertIn("env['PS4RT_UPSCALER'] = args.upscaler", source)
        self.assertIn("env['PS4RT_CONFIG'] = str(out / 'ps4rt.ini')", source)


if __name__ == "__main__":
    unittest.main()
