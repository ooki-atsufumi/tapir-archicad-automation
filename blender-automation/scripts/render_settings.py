"""Render presets. 'preview' for quick checks, 'final' for delivery."""
import bpy

import blender_compat as compat


PRESETS = {
    # Fast EEVEE draft: 1080p, low samples -> seconds per frame on a laptop GPU.
    'preview': dict(engine='EEVEE', resolution=(1280, 720), percentage=100, samples=16),
    # Real-time quality with ray tracing (EEVEE Next), good for most client videos.
    'eevee_hq': dict(engine='EEVEE', resolution=(1920, 1080), percentage=100, samples=64),
    # Path tracing. Photoreal, slow (minutes/frame on CPU, ~10-40 s/frame on a modern GPU).
    'final': dict(engine='CYCLES', resolution=(1920, 1080), percentage=100, samples=256),
}


def apply_preset(name, output_path, fps=30, overrides=None):
    cfg = dict(PRESETS[name])
    cfg.update(overrides or {})
    scene = bpy.context.scene
    engine_id = compat.set_render_engine(cfg['engine'])
    scene.render.resolution_x, scene.render.resolution_y = cfg['resolution']
    scene.render.resolution_percentage = cfg['percentage']
    scene.render.use_motion_blur = cfg.get('motion_blur', True)
    scene.render.film_transparent = False

    if engine_id.startswith('BLENDER_EEVEE'):
        ev = scene.eevee
        ev.taa_render_samples = cfg['samples']
        for attr, value in (('use_raytracing', True), ('use_shadows', True),
                            ('use_volumetric_shadows', False), ('use_gtao', True),
                            ('use_bloom', False)):
            if hasattr(ev, attr):
                setattr(ev, attr, value)
        try:
            ev.ray_tracing_options.resolution_scale = '2'
        except (AttributeError, TypeError):
            pass
    elif engine_id == 'CYCLES':
        cy = scene.cycles
        cy.samples = cfg['samples']
        cy.use_denoising = True
        cy.use_adaptive_sampling = True
        cy.adaptive_threshold = 0.02
        cy.max_bounces = 8
        cy.caustics_reflective = False
        cy.caustics_refractive = False
        _prefer_gpu()

    compat.set_mp4_output(scene, output_path, fps)
    print(f'[render] preset={name} engine={engine_id} {cfg["resolution"]} samples={cfg["samples"]} -> {output_path}')


def _prefer_gpu():
    prefs = bpy.context.preferences.addons.get('cycles')
    if prefs is None:
        return
    cprefs = prefs.preferences
    for backend in ('OPTIX', 'CUDA', 'HIP', 'ONEAPI', 'METAL'):
        try:
            cprefs.compute_device_type = backend
        except TypeError:
            continue
        try:
            cprefs.get_devices()
        except Exception:  # noqa: BLE001
            pass
        devices = [d for d in cprefs.devices if d.type != 'CPU']
        if devices:
            for d in cprefs.devices:
                d.use = True
            bpy.context.scene.cycles.device = 'GPU'
            print(f'[render] Cycles on GPU via {backend}: {[d.name for d in devices]}')
            return
    bpy.context.scene.cycles.device = 'CPU'
    print('[render] Cycles on CPU (no GPU backend found)')


def render_animation(frame_step=1):
    scene = bpy.context.scene
    scene.frame_step = frame_step
    bpy.ops.render.render(animation=True, write_still=False)


def render_still(frame, path):
    scene = bpy.context.scene
    scene.frame_set(frame)
    prev_fmt = scene.render.image_settings.file_format
    prev_path = scene.render.filepath
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    scene.render.image_settings.file_format = prev_fmt
    scene.render.filepath = prev_path
