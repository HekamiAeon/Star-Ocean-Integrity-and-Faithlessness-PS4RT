#!/usr/bin/env python3
"""Prepare and start a native Linux PS4RT port from the current working template.

Future games require reverse engineering and source changes. Not a general PS4 emulator.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import shlex
import sys
import tempfile
from prepare import load_sfo, prepare
from link_modules import link
from content_profile import prepare as prepare_content

ROOT = Path(__file__).resolve().parent.parent


# Game language: Star Ocean reads the console's system language at every boot
# (sceSystemServiceParamGetInt, parameter 1), maps it to a row of its SystemLanguage.csv and mounts
# that row's text archive. The overlay's choice is therefore the emulated console language, a PS4
# system language ID (not a game-private index and not a trophy locale). Only languages verified
# for the EU build (CUSA03219) are offered, in menu order: its text exists in English and French.
# ID 1 reads the English text archive and ID 2 the French one; both keep the English voices.
# Every other ID falls back to the game's "Others" row (English); Japanese (0), Korean (9) and
# Chinese (10) name rows this build's SystemLanguage.csv does not have.
CONSOLE_LANGUAGES = (
    (1, 'English'),
    (2, 'French'),
)
DEFAULT_CONSOLE_LANGUAGE = 1  # English (US): what this port always reported before the option
MAX_SYSTEM_LANGUAGE = 30

# Internal resolutions saved by the in-game menu (gpu/shim/ps4rt_graphics.h Resolutions).
# 'native' is the game's own 1920x1080 and needs nothing. For the others the runtime makes the
# game itself render at that size (src/runtime_resolution.c). '540p' renders at 1024x576 as in
# the Code Fairy port (a height that is a multiple of 8).
RENDER_RESOLUTIONS = {
    '720p': '1280x720', '540p': '1024x576', '1440p': '2560x1440', '2160p': '3840x2160',
    '2560x1080': '2560x1080', '3440x1440': '3440x1440', '3840x1440': '3840x1440',
    '3840x1080': '3840x1080', '5120x1440': '5120x1440',
}


def _internal_resolution(ini: Path, env: dict) -> None:
    """Pass the saved internal resolution to the runtime (applied on restart).

    An explicit PS4RT_SO_RESOLUTION from the caller wins. A value this launcher took from the
    ini on an earlier launch (inherited through "Apply and restart") is not the caller's: it is
    dropped so the newly saved choice applies.
    """
    if env.pop('PS4RT_SO_RESOLUTION_FROM_INI', None):
        env.pop('PS4RT_SO_RESOLUTION', None)
    if 'PS4RT_SO_RESOLUTION' in env:
        return
    size = RENDER_RESOLUTIONS.get((_ini_value(ini, 'render_resolution') or '').strip().lower())
    if size:
        env['PS4RT_SO_RESOLUTION'] = size
        env['PS4RT_SO_RESOLUTION_FROM_INI'] = '1'


def _language_id(text) -> int | None:
    """A PS4 system language ID, or None for anything else."""
    try:
        value = int(str(text).strip())
    except ValueError:
        return None
    return value if 0 <= value <= MAX_SYSTEM_LANGUAGE else None


def _ini_value(path: Path, key: str) -> str | None:
    try:
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
    except OSError:
        return None
    found = None
    for line in lines:
        name, sep, value = line.partition('=')
        if sep and name.strip() == key:
            found = value.strip()
    return found


def _write_ini_value(path: Path, key: str, value: str, header: str = '') -> None:
    """Replace (or add) one key, keeping every other line, with an atomic file replacement."""
    try:
        lines = path.read_text(encoding='utf-8').splitlines()
    except FileNotFoundError:
        lines = [header] if header else []
    out, done = [], False
    for line in lines:
        if line.partition('=')[0].strip() == key and '=' in line:
            if not done:
                out.append(f'{key}={value}')
            done = True
        else:
            out.append(line)
    if not done:
        out.append(f'{key}={value}')
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix=f'.{path.name}.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as stream:
            stream.write('\n'.join(out) + '\n')
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    except BaseException:
        try:
            os.unlink(temp)
        except OSError:
            pass
        raise


def _console_language(user_dir: Path, ini: Path, env: dict) -> int:
    """Choose this launch's console language and tell the runtime and the overlay.

    The emulated console keeps its language in user/console.ini, like a PS4 system setting.
    ps4rt.ini only carries a pending overlay choice (language=-1 when there is none). A valid
    pending choice becomes the console language before the game starts and is cleared once it
    is stored. Without one, an explicit PS4RT_LANGUAGE from the caller wins for this launch only,
    then the stored console language, then English (US), the port's previous behaviour.
    """
    console = user_dir / 'console.ini'
    names = dict(CONSOLE_LANGUAGES)
    raw_env = env.pop('PS4RT_LANGUAGE', None)
    env.pop('PS4RT_LANGUAGE_CHOICES', None)
    explicit = _language_id(raw_env) if raw_env not in (None, '') else None
    if raw_env not in (None, '') and explicit is None:
        print(f'PS4RT: ignoring invalid PS4RT_LANGUAGE={raw_env!r}', flush=True)
    stored = _language_id(_ini_value(console, 'language') or '')
    raw_pending = _ini_value(ini, 'language')
    pending = None
    if raw_pending is not None:
        try:
            pending = int(raw_pending)
        except ValueError:
            pending = -2  # damaged: never applied, cleared below
    active = explicit if explicit is not None else stored
    if pending is not None and pending != -1:
        if pending in names:
            active = pending
            try:
                _write_ini_value(console, 'language', str(pending),
                                 '# Emulated console settings (PS4 system language ID)')
                stored = pending
                _write_ini_value(ini, 'language', '-1')
                print(f'PS4RT: game language set to {names[pending]}', flush=True)
            except OSError as error:
                # Use it for this launch; the pending choice stays and is retried next time.
                print(f'PS4RT: could not store the game language ({error}); '
                      f'using {names[pending]} for this launch only', flush=True)
        else:
            print(f'PS4RT: ignoring unsupported game language {raw_pending!r}', flush=True)
            try:
                _write_ini_value(ini, 'language', '-1')
            except OSError:
                pass
    if active is None:
        active = DEFAULT_CONSOLE_LANGUAGE
    choices = list(CONSOLE_LANGUAGES)
    if active not in names:  # a manual override: show it rather than a wrong selection
        choices.append((active, f'Other (system language {active})'))
    env['PS4RT_LANGUAGE'] = str(active)
    env['PS4RT_LANGUAGE_CHOICES'] = '|'.join(f'{i}={label}' for i, label in choices)
    return active


def _achievements(title_id: str, game: Path, user_dir: Path, env: dict) -> None:
    """Prepare the local trophy catalog (adapted from the Code Fairy port).

    The game still decides every unlock. Text, icons and unlock state live in
    the runtime user area; the installed game files are only read. A missing
    or damaged trophy archive must never prevent the game from starting.
    """
    from trophy_catalog import prepare_catalog, trophy_language
    env.pop('PS4RT_TROPHY_CATALOG', None)
    env['PS4RT_ACHIEVEMENTS_DIR'] = str(user_dir / 'achievements')
    try:
        language = int(env.get('PS4RT_LANGUAGE', '1'))
        language = trophy_language(title_id, user_dir, language)
        env['PS4RT_TROPHY_LANGUAGE'] = str(language)
        catalog = prepare_catalog(game, user_dir, language)
        if catalog is not None:
            env['PS4RT_TROPHY_CATALOG'] = str(catalog)
    except Exception as error:  # any metadata problem: play without achievements
        print(f'Achievements unavailable: {error}', flush=True)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path,
                        help='Your extracted plaintext CUSA03219 game directory')
    parser.add_argument('--data-dir', type=Path,
                        help='Optional private folder for this title, including saves and settings')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--upscaler', choices=('off', 'fsr3', 'fsr4'),
                        help='Override the saved graphics setting for this launch')
    parser.add_argument('--output-resolution', choices=('1080', '1440', '4k'))
    args = parser.parse_args(argv)

    # Keep the existing default layout: out/CUSA03219 contains game-derived
    # prepared files, game-folder.txt, graphics settings, saves and GPU caches.
    out = (args.data_dir.expanduser().resolve() if args.data_dir is not None
           else ROOT / 'out/CUSA03219')
    remembered = out / 'game-folder.txt'
    if args.game is not None:
        game = args.game.expanduser().resolve()
    else:
        if not remembered.is_file() or not remembered.read_text().strip():
            raise ValueError('Game folder not configured. First launch: '
                             'bash Play-Game.sh --game "/absolute/path/to/CUSA03219"\n'
                             'Refer to PLAYER_README.txt for details.')
        game = Path(remembered.read_text().strip()).expanduser().resolve()

    # Validate before preparing anything or replacing the remembered path.
    if not game.is_dir():
        raise ValueError(f'Game folder not found: {game}. '
                         'Supply --game "/absolute/path/to/CUSA03219"')
    sfo = game / 'sce_sys/param.sfo'
    if not sfo.is_file():
        raise ValueError(f'Missing sce_sys/param.sfo in game folder: {game}')
    title_id = str(load_sfo(sfo).get('TITLE_ID', ''))
    if title_id != 'CUSA03219':
        raise ValueError('This port requires CUSA03219; select that extracted game folder')
    runtime = ROOT / 'out/ps4rt'
    if not args.prepare_only and not runtime.is_file():
        parser.error('Build the runtime first: bash build.sh')
    prepare(game, out)
    link(game, out)
    prepare_content(game, out, 'full')
    # Only remember a successfully validated and prepared location.
    out.mkdir(parents=True, exist_ok=True)
    remembered.write_text(str(game) + '\n')
    if args.prepare_only:
        return 0
    user_dir = out / 'user'
    gpu_user = user_dir / 'gpu'
    gpu_user.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    # Do not leak compatibility overrides from the caller's shell into a new title.
    for key in ('PS4RT_TRANSITION_SNAPSHOT', 'PS4RT_BC6_FLOAT',
                'PS4RT_WQM_HELPERS', 'PS4RT_ASYNC_SHADERS',
                'PS4RT_OBJECT_MOTION', 'PS4RT_FSR1', 'PS4RT_PARTIAL_HEIGHT_VIEWS',
                'PS4RT_UPSCALER', 'PS4RT_OUTPUT_RES', 'PS4RT_RENDER_RES',
                'PS4RT_UPSCALE_PRESET', 'PS4RT_UPSCALE_BEFORE_CS',
                'PS4RT_UI_TRIGGER_VS', 'PS4RT_PRESET_FILE', 'PS4RT_RCAS',
                'PS4RT_FSR_SHARPNESS', 'PS4RT_JITTER'):
        env.pop(key, None)
    # Start with the known working runtime behavior; future ports adapt it in-source.
    # Do not force PS4RT_ASYNC_SHADERS here: the in-game menu writes its
    # choice to out/<title_id>/ps4rt.ini. New configs default to asynchronous compilation.
    env.update(PS4RT_BC6_FLOAT='unsigned', PS4RT_TRANSITION_SNAPSHOT='1',
               PS4RT_OBJECT_MOTION='0', PS4RT_FSR1='0',
               PS4RT_PARTIAL_HEIGHT_VIEWS='0')
    env['PS4RT_GPU_USER_DIR'] = str(gpu_user)
    env['PS4RT_CONFIG'] = str(out / 'ps4rt.ini')
    _internal_resolution(out / 'ps4rt.ini', env)
    # Before the trophy catalog: trophy text follows the console language.
    _console_language(user_dir, out / 'ps4rt.ini', env)
    _achievements(title_id, game, user_dir, env)
    # Restart through the player launcher, preserving the selected game and options.
    # The raw runtime cannot reconstruct --game once Python execs it.
    restart = [sys.executable, str(Path(__file__).resolve()), '--game', str(game)]
    if args.data_dir is not None:
        restart += ['--data-dir', str(out)]
    if args.upscaler is not None:
        restart += ['--upscaler', args.upscaler]
    if args.output_resolution is not None:
        restart += ['--output-resolution', args.output_resolution]
    env['PS4RT_RESTART_COMMAND'] = shlex.join(restart)
    # Load local game art with a stable absolute path, regardless of launch cwd.
    # A manually specified background overrides this default.
    if not env.get('PS4RT_OVERLAY_BACKGROUND'):
        overlay_art = ROOT / 'gpu/assets/star_ocean_overlay.png'
        if overlay_art.is_file():
            env['PS4RT_OVERLAY_BACKGROUND'] = str(overlay_art)
    if not env.get('PS4RT_MENU_SOUND_DIR'):
        sound_dir = ROOT / 'gpu/assets/sounds'
        if sound_dir.is_dir():
            env['PS4RT_MENU_SOUND_DIR'] = str(sound_dir)
    if args.upscaler is not None:
        env['PS4RT_UPSCALER'] = args.upscaler
    if args.output_resolution is not None:
        env['PS4RT_OUTPUT_RES'] = {
            '1080': '1920x1080', '1440': '2560x1440', '4k': '3840x2160'
        }[args.output_resolution]
    print(f'PS4RT native-port template: {title_id}', flush=True)
    command = [str(runtime), str(out / 'boot-linked.bin'), '--app0', str(game),
               '--user', str(user_dir), '--content-profile', str(out / 'content.bin'),
               '--timeout', '0']
    os.execve(str(runtime), command, env)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as error:
        sys.exit(f'PS4RT: {error}')
