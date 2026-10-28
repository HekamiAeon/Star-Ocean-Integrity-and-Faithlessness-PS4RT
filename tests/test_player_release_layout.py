"""Player archive is lean; matching source archive retains documentation."""
import importlib.util
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/package_star_ocean.py'
spec = importlib.util.spec_from_file_location('star_ocean_packager', SCRIPT)
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class PlayerReleaseLayoutTests(unittest.TestCase):
    def test_player_archive_excludes_docs_and_embedded_source(self):
        paths = {
            'run.sh', 'LICENSE', 'README.md',
            'docs/RUNNING_LINUX.md', 'docs/UPSTREAM_CREDITS.md',
            'docs/DISTRIBUTING_LINUX.md', 'src/runtime.c',
            'scripts/ps4rt_launch.py',
            'fsr4_shaders/LICENSE-FSR4-v07.txt',
            'gpu/assets/star_ocean_overlay.png',
            'gpu/third_party/imgui/LICENSE.txt',
        }
        source = {name: name.encode() for name in paths}
        modes = {name: 0o644 for name in paths}
        with patch.object(packager, 'host_elf', return_value=b'fake-ELF'):
            player = packager.runtime_entries(source, modes)
        self.assertIn('Play-Star-Ocean.sh', player)
        self.assertIn('PLAYER_README.txt', player)
        self.assertIn('LICENSE', player)
        self.assertIn('CREDITS.md', player)
        self.assertIn('gpu/third_party/imgui/LICENSE.txt', player)
        self.assertIn('fsr4_shaders/LICENSE-FSR4-v07.txt', player)
        self.assertNotIn('README.md', player)
        self.assertNotIn('src/runtime.c', player)
        self.assertTrue(all(not name.startswith(('docs/', 'source/'))
                            for name in player), sorted(player))
        self.assertIn(b'github.com/HekamiAeon/', player['PLAYER_README.txt'][0])
        with tempfile.TemporaryDirectory() as temp:
            dest = Path(temp) / 'player.tar.gz'
            packager.export(dest, 'test-player', 'abcdef0123456', 'linux-x86_64', player)
            with tarfile.open(dest) as archive:
                members = [m.name.partition('/')[2] for m in archive.getmembers()]
            self.assertTrue(all(not n.startswith(('docs/', 'source/'))
                                for n in members), members)
        # Documentation remains part of the separate committed source export.
        self.assertIn('docs/RUNNING_LINUX.md', source)
        self.assertIn('src/runtime.c', source)


if __name__ == '__main__':
    unittest.main()
