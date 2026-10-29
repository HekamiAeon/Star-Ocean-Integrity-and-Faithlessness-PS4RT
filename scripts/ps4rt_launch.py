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
from prepare import load_sfo, prepare
from link_modules import link
from content_profile import prepare as prepare_content

ROOT = Path(__file__).resolve().parent.parent
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
    # choice to out/<title_id>/ps4rt.ini. New configs default to Sync.
    env.update(PS4RT_BC6_FLOAT='unsigned', PS4RT_TRANSITION_SNAPSHOT='1',
               PS4RT_OBJECT_MOTION='0', PS4RT_FSR1='0',
               PS4RT_PARTIAL_HEIGHT_VIEWS='0')
    env['PS4RT_GPU_USER_DIR'] = str(gpu_user)
    env['PS4RT_CONFIG'] = str(out / 'ps4rt.ini')
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
