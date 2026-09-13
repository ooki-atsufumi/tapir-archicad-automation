"""One-shot driver run OUTSIDE Blender (plain Python 3):

    python pipeline/run_pipeline.py --config pipeline/config.example.json --export-ifc --render

Steps
 1. (optional) ask the running Archicad to export IFC via the Tapir Add-On
 2. launch Blender in background mode with build_walkthrough.py
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

BLENDER_CANDIDATES = [
    os.environ.get('BLENDER_PATH'),
    shutil.which('blender'),
    r'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe',
    r'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe',
    r'C:\Program Files\Blender Foundation\Blender 5.0\blender.exe',
    r'C:\Program Files\Blender Foundation\Blender 4.5\blender.exe',
    '/Applications/Blender.app/Contents/MacOS/Blender',
]


def find_blender():
    for cand in BLENDER_CANDIDATES:
        if cand and os.path.isfile(cand):
            return cand
    raise SystemExit('Blender executable not found. Set BLENDER_PATH or add blender to PATH.')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True)
    parser.add_argument('--export-ifc', action='store_true', help='export IFC from running Archicad first')
    parser.add_argument('--render', action='store_true')
    parser.add_argument('--still', type=int, default=None)
    parser.add_argument('--save', default=None)
    parser.add_argument('--preset', default=None)
    parser.add_argument('--dry-run', action='store_true', help='print the commands only')
    parser.add_argument('--archicad-port', type=int, default=19723)
    args = parser.parse_args()

    cfg_path = os.path.abspath(args.config)
    with open(cfg_path, encoding='utf-8') as fh:
        cfg = json.load(fh)
    base_dir = os.path.dirname(cfg_path)
    model_path = cfg['model']['path']
    if not os.path.isabs(model_path):
        model_path = os.path.normpath(os.path.join(base_dir, model_path))

    if args.export_ifc:
        cmd = [sys.executable, os.path.join(ROOT, 'archicad', 'export_ifc.py'),
               '--out', model_path, '--port', str(args.archicad_port)]
        print('>', ' '.join(cmd))
        if not args.dry_run:
            subprocess.run(cmd, check=True)

    blender = 'blender' if args.dry_run else find_blender()
    cmd = [blender, '-b', '-P', os.path.join(HERE, 'build_walkthrough.py'), '--', '--config', cfg_path]
    if args.render:
        cmd.append('--render')
    if args.still is not None:
        cmd += ['--still', str(args.still)]
    if args.save:
        cmd += ['--save', args.save]
    if args.preset:
        cmd += ['--preset', args.preset]
    print('>', ' '.join(cmd))
    if not args.dry_run:
        subprocess.run(cmd, check=True)


if __name__ == '__main__':
    main()
