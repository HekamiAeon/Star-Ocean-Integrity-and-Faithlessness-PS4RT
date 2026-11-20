"""Player archive is lean; source archives leave the project docs out."""
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools/package_star_ocean.py'
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
        # The player README and launchers are tracked source files.
        for name in ('PLAYER_README.txt', 'Play-Game.sh', 'Play-Star-Ocean.sh'):
            source['packaging/linux/' + name] = (
                SCRIPT.parents[1] / 'packaging/linux' / name).read_bytes()
        modes = {name: 0o644 for name in source}
        with patch.object(packager, 'host_elf', return_value=b'fake-ELF'):
            player = packager.runtime_entries(source, modes)
        self.assertIn('Play-Game.sh', player)
        self.assertIn('Play-Star-Ocean.sh', player)
        self.assertEqual(player['Play-Game.sh'][1], 0o755)
        self.assertEqual(player['Play-Star-Ocean.sh'][1], 0o755)
        self.assertIn(b'Play-Game.sh', player['Play-Star-Ocean.sh'][0])
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
        self.assertIn(b'bash Play-Game.sh --game', player['PLAYER_README.txt'][0])
        self.assertIn(b'bash Play-Game.sh\n', player['PLAYER_README.txt'][0])
        with tempfile.TemporaryDirectory() as temp:
            dest = Path(temp) / 'player.tar.gz'
            packager.export(dest, 'test-player', 'abcdef0123456', 'linux-x86_64', player)
            with tarfile.open(dest) as archive:
                members = [m.name.partition('/')[2] for m in archive.getmembers()]
            self.assertTrue(all(not n.startswith(('docs/', 'source/'))
                                for n in members), members)

    def test_source_requires_credits_and_license_but_not_the_readme(self):
        self.assertIn('LICENSE', packager.REQUIRED_RELEASE_FILES)
        self.assertIn('docs/UPSTREAM_CREDITS.md', packager.REQUIRED_RELEASE_FILES)
        self.assertNotIn('README.md', packager.REQUIRED_RELEASE_FILES)


def git(*args):
    return subprocess.run(['git', '-C', str(ROOT), *args], capture_output=True, check=True).stdout


def export_ignored(paths):
    """Whether this checkout's .gitattributes leaves each path out of `git archive`."""
    fields = git('check-attr', '-z', 'export-ignore', '--', *paths).split(b'\0')
    return {fields[i].decode(): fields[i + 2] == b'set' for i in range(0, len(fields) - 2, 3)}


def tracked(*paths):
    return [p.decode() for p in git('ls-files', '-z', '--', *paths).split(b'\0') if p]


# Project documentation: kept out of source archives, except the credits and docs/licenses/.
PROJECT_DOCS = ('README.md', 'docs', 'packaging/linux/README.md')


def project_doc(path):
    return path in PROJECT_DOCS or path.startswith('docs/')


@unittest.skipUnless(shutil.which('git') and (ROOT / '.git').exists(), 'needs a Git checkout')
class SourceArchiveDocsPolicyTests(unittest.TestCase):
    """Project docs stay out of GitHub's generated archives and the package_star_ocean.py source
    export (both use git archive), so they can change after a release is built."""

    def test_player_docs_are_left_out_of_source_archives(self):
        docs = [p for p in tracked(*PROJECT_DOCS)
                if p != 'docs/UPSTREAM_CREDITS.md' and not p.startswith('docs/licenses/')]
        self.assertIn('README.md', docs)
        self.assertIn('packaging/linux/README.md', docs)
        for path, ignored in export_ignored(docs).items():
            self.assertTrue(ignored, path)

    def test_credits_licenses_and_notices_stay_in_source_archives(self):
        notice = re.compile(r'(^|/)(license|notice|copying|readme)[^/]*$', re.IGNORECASE)
        keep = ['docs/UPSTREAM_CREDITS.md', 'docs/licenses', 'docs/licenses/GPL-3.0.txt',
                'docs/licenses/nested/NOTICE.txt',
                *(p for p in tracked() if notice.search(p) and not project_doc(p))]
        self.assertIn('LICENSE', keep)
        self.assertIn('gpu/third_party/half/README.txt', keep)
        for path, ignored in export_ignored(keep).items():
            self.assertFalse(ignored, path)

    def test_required_source_is_never_left_out(self):
        for path, ignored in export_ignored(sorted(packager.REQUIRED_RELEASE_FILES)).items():
            self.assertFalse(ignored, path)


if __name__ == '__main__':
    unittest.main()
