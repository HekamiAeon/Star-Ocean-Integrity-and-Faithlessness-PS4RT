"""Regression tests for the standalone Star Ocean player launcher contract."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('star_ocean_launcher', ROOT / 'scripts/ps4rt_launch.py')
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class PlayerLauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'player'
        (self.root / 'out').mkdir(parents=True)
        (self.root / 'out/ps4rt').touch()
        self.one = self.make_game('first')
        self.two = self.make_game('second')

    def make_game(self, label):
        game = Path(self.temp.name) / label / 'CUSA03219'
        (game / 'sce_sys').mkdir(parents=True)
        (game / 'sce_sys/param.sfo').write_bytes(b'test fixture')
        return game

    def call(self, *args, title_id='CUSA03219'):
        with (patch.object(launcher, 'ROOT', self.root),
              patch.object(launcher, 'load_sfo', return_value={'TITLE_ID': title_id}),
              patch.object(launcher, 'prepare') as prepare,
              patch.object(launcher, 'link') as link,
              patch.object(launcher, 'prepare_content') as content,
              patch.object(launcher.os, 'execve') as execute):
            result = launcher.main(list(args))
            return result, prepare, link, content, execute

    def test_unconfigured_explains_first_launch_without_picker(self):
        with self.assertRaisesRegex(ValueError, 'Play-Game.sh --game') as error:
            self.call()
        self.assertIn('PLAYER_README.txt', str(error.exception))
        self.assertFalse((self.root / 'out/CUSA03219/game-folder.txt').exists())

    def test_first_explicit_then_remembered(self):
        self.call('--game', str(self.one))
        saved = self.root / 'out/CUSA03219/game-folder.txt'
        self.assertEqual(saved.read_text().strip(), str(self.one))
        _, prepare, _, _, execute = self.call()
        self.assertEqual(prepare.call_args.args[0], self.one)
        self.assertEqual(execute.call_args.args[1][2], '--app0')
        self.assertEqual(execute.call_args.args[1][3], str(self.one))

    def test_explicit_override_updates_memory(self):
        self.call('--game', str(self.one))
        self.call('--game', str(self.two))
        self.assertEqual((self.root / 'out/CUSA03219/game-folder.txt').read_text().strip(), str(self.two))

    def test_invalid_explicit_does_not_destroy_previous_memory(self):
        self.call('--game', str(self.one))
        with self.assertRaisesRegex(ValueError, 'Game folder not found'):
            self.call('--game', str(Path(self.temp.name) / 'missing'))
        self.assertEqual((self.root / 'out/CUSA03219/game-folder.txt').read_text().strip(), str(self.one))

    def test_stale_remembered_location_is_actionable(self):
        state = self.root / 'out/CUSA03219/game-folder.txt'
        state.parent.mkdir(parents=True, exist_ok=True)
        state.write_text(str(Path(self.temp.name) / 'moved') + '\n')
        with self.assertRaisesRegex(ValueError, 'Supply --game'):
            self.call()

    def test_wrong_game_rejected(self):
        with self.assertRaisesRegex(ValueError, 'requires CUSA03219'):
            self.call('--game', str(self.one), title_id='CUSA99999')

    def test_prepare_only_remembers_without_running(self):
        result, _, _, _, execute = self.call('--game', str(self.one), '--prepare-only')
        self.assertEqual(result, 0)
        execute.assert_not_called()
        self.assertEqual((self.root / 'out/CUSA03219/game-folder.txt').read_text().strip(), str(self.one))

    def test_custom_data_dir_and_restart_keep_options(self):
        private = Path(self.temp.name) / 'private'
        _, _, _, _, execute = self.call('--game', str(self.one),
                                        '--data-dir', str(private),
                                        '--upscaler', 'fsr4',
                                        '--output-resolution', '4k')
        self.assertEqual((private / 'game-folder.txt').read_text().strip(), str(self.one))
        env = execute.call_args.args[2]
        self.assertEqual(env['PS4RT_CONFIG'], str(private / 'ps4rt.ini'))
        self.assertIn('--data-dir', env['PS4RT_RESTART_COMMAND'])
        self.assertIn(str(private), env['PS4RT_RESTART_COMMAND'])
        self.assertIn('--upscaler fsr4', env['PS4RT_RESTART_COMMAND'])
        self.assertIn('--output-resolution 4k', env['PS4RT_RESTART_COMMAND'])
        self.call('--data-dir', str(private))


if __name__ == '__main__':
    unittest.main()

