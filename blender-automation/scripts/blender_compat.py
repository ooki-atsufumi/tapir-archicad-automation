"""Small compatibility layer so the same scripts run on Blender 4.2 LTS ... 5.x.

Blender renamed a few identifiers between 4.x and 5.x (EEVEE engine id, video
container names, etc.). Every other script imports this module instead of
hard-coding those strings.
"""
import bpy


def blender_version():
    return bpy.app.version  # (major, minor, patch)


def set_render_engine(engine):
    """engine: 'EEVEE' | 'CYCLES' | 'WORKBENCH'. Picks the id valid in this Blender."""
    scene = bpy.context.scene
    candidates = {
        'EEVEE': ['BLENDER_EEVEE_NEXT', 'BLENDER_EEVEE'],
        'CYCLES': ['CYCLES'],
        'WORKBENCH': ['BLENDER_WORKBENCH'],
    }[engine.upper()]
    valid = {item.identifier for item in scene.render.bl_rna.properties['engine'].enum_items}
    for cand in candidates:
        if cand in valid:
            scene.render.engine = cand
            return cand
    raise RuntimeError(f'No render engine among {candidates} is available in Blender {blender_version()}')


def set_mp4_output(scene, filepath, fps=30):
    """Configure H.264 / MP4 output (FFmpeg is bundled with every official Blender build)."""
    scene.render.fps = fps
    scene.render.image_settings.file_format = 'FFMPEG'
    scene.render.ffmpeg.format = 'MPEG4'
    scene.render.ffmpeg.codec = 'H264'
    scene.render.ffmpeg.constant_rate_factor = 'HIGH'
    scene.render.ffmpeg.ffmpeg_preset = 'GOOD'
    scene.render.ffmpeg.gopsize = fps
    scene.render.ffmpeg.audio_codec = 'NONE'
    scene.render.filepath = filepath


def enable_addon_if_needed(module_names):
    """Try to enable the first available add-on/extension from a list of module names.

    Extensions installed from extensions.blender.org live under
    'bl_ext.blender_org.<name>' or 'bl_ext.user_default.<name>'; legacy add-ons use
    their bare module name. Returns the module name that was enabled or None.
    """
    import addon_utils
    installed = {mod.__name__ for mod in addon_utils.modules()}
    for name in module_names:
        for full in (name, f'bl_ext.blender_org.{name}', f'bl_ext.user_default.{name}'):
            if full in installed:
                try:
                    addon_utils.enable(full, default_set=True, persistent=True)
                    return full
                except Exception as exc:  # noqa: BLE001
                    print(f'[compat] could not enable {full}: {exc}')
    return None


def clear_scene():
    """Start from an empty scene while keeping user preferences (enabled add-ons, GPU)."""
    bpy.ops.wm.read_homefile(use_empty=True)


def iter_fcurves(action):
    """Yield every F-curve of an action on both legacy (<=4.3) and slotted (>=4.4) actions."""
    layers = getattr(action, 'layers', None)
    if layers:
        for layer in layers:
            for strip in layer.strips:
                for bag in getattr(strip, 'channelbags', []):
                    yield from bag.fcurves
        return
    yield from action.fcurves
