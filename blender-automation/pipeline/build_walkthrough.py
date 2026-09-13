"""Runs INSIDE Blender. Builds the whole walkthrough scene from a JSON config and
optionally renders it.

    blender -b -P pipeline/build_walkthrough.py -- --config pipeline/config.example.json [--render] [--save out.blend]

Claude Code can also call the individual functions through blender-mcp
(execute_blender_code) for interactive iteration; see README.md.
"""
import argparse
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'scripts'))

import blender_compat as compat  # noqa: E402
import import_ifc  # noqa: E402
import walkthrough  # noqa: E402
import lighting  # noqa: E402
import render_settings  # noqa: E402


def parse_args():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', required=True)
    parser.add_argument('--render', action='store_true', help='render the animation after building')
    parser.add_argument('--still', type=int, default=None, help='render only this frame as PNG (quick check)')
    parser.add_argument('--save', default=None, help='save the built scene as .blend')
    parser.add_argument('--preset', default=None, help='override render preset from config')
    return parser.parse_args(argv)


def resolve(base_dir, path):
    if path is None:
        return None
    return path if os.path.isabs(path) else os.path.normpath(os.path.join(base_dir, path))


def build(cfg, base_dir):
    compat.clear_scene()
    print(f'[pipeline] Blender {".".join(map(str, compat.blender_version()))}')

    model = cfg['model']
    objects = import_ifc.import_model(resolve(base_dir, model['path']))
    lo, hi = import_ifc.scene_bounds(objects)
    print(f'[pipeline] model bounds {tuple(round(v, 2) for v in lo)} .. {tuple(round(v, 2) for v in hi)}')

    cam_cfg = cfg['camera']
    cam, path, frame_end = walkthrough.create_camera_walk(
        waypoints=cam_cfg['waypoints'],
        eye_height=cam_cfg.get('eye_height_m', 1.6),
        walk_speed=cam_cfg.get('walk_speed_m_s', 1.2),
        fps=cfg.get('fps', 30),
        focal_length_mm=cam_cfg.get('focal_length_mm', 24.0),
        look_ahead_m=cam_cfg.get('look_ahead_m', 2.0),
        sway=cam_cfg.get('head_bob', True),
    )
    if cam_cfg.get('depth_of_field', False):
        walkthrough.add_dof(cam, fstop=cam_cfg.get('fstop', 4.0))

    light = cfg.get('lighting', {})
    lighting.setup_world(hdri_path=resolve(base_dir, light.get('hdri')),
                         sun_elevation_deg=light.get('sun_elevation_deg', 45),
                         sun_azimuth_deg=light.get('sun_azimuth_deg', 135),
                         strength=light.get('world_strength', 1.0))
    if light.get('sun', True):
        lighting.add_sun(light.get('sun_elevation_deg', 45), light.get('sun_azimuth_deg', 135),
                         light.get('sun_strength', 4.0))
    if light.get('interior_fill', True):
        lighting.add_interior_fill(lo, hi, power_w=light.get('fill_power_w', 300.0))
    lighting.set_color_management(exposure=light.get('exposure', 0.0))

    avatar_cfg = cfg.get('avatar')
    if avatar_cfg and avatar_cfg.get('enabled', True) and avatar_cfg.get('path'):
        import avatar
        avatar.place_avatar(resolve(base_dir, avatar_cfg['path']), path, frame_end,
                            walkthrough.path_length(path),
                            height_m=avatar_cfg.get('height_m', 1.70),
                            ahead_of_camera_m=avatar_cfg.get('ahead_of_camera_m', 2.5))

    out = cfg.get('output', {})
    render_settings.apply_preset(out.get('preset', 'preview'),
                                 resolve(base_dir, out.get('video', 'renders/walkthrough.mp4')),
                                 fps=cfg.get('fps', 30),
                                 overrides=out.get('overrides'))
    return frame_end


def main():
    args = parse_args()
    cfg_path = os.path.abspath(args.config)
    with open(cfg_path, encoding='utf-8') as fh:
        cfg = json.load(fh)
    if args.preset:
        cfg.setdefault('output', {})['preset'] = args.preset
    base_dir = os.path.dirname(cfg_path)

    frame_end = build(cfg, base_dir)
    os.makedirs(os.path.dirname(bpy.context.scene.render.filepath) or '.', exist_ok=True)

    if args.save:
        save_path = resolve(base_dir, args.save)
        os.makedirs(os.path.dirname(save_path) or '.', exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=save_path)
        print(f'[pipeline] saved {save_path}')

    if args.still is not None:
        still_path = os.path.splitext(bpy.context.scene.render.filepath)[0] + f'_f{args.still:04d}.png'
        render_settings.render_still(min(max(1, args.still), frame_end), still_path)
        print(f'[pipeline] still written {still_path}')
    if args.render:
        render_settings.render_animation()
        print(f'[pipeline] video written {bpy.context.scene.render.filepath}')


if __name__ == '__main__':
    main()
