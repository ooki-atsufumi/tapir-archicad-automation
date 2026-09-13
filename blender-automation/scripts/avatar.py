"""Avatar (digital double) handling.

Expects a rigged, animated character file with a walk cycle in place
(e.g. FBX from Mixamo "Walking" with "In Place" checked, or an MPFB2 character with a
walk cycle BVH retargeted). The avatar follows the same path as the camera, keeps its
feet on the floor, and the walk cycle is looped for the whole shot.
"""
import os

import bpy

import blender_compat as compat


def import_avatar(path):
    before = set(bpy.data.objects)
    ext = os.path.splitext(path)[1].lower()
    if ext == '.fbx':
        bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=True,
                                 ignore_leaf_bones=True, use_anim=True)
    elif ext in ('.glb', '.gltf'):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == '.blend':
        with bpy.data.libraries.load(path, link=False) as (src, dst):
            dst.objects = [n for n in src.objects]
        for obj in dst.objects:
            if obj is not None:
                bpy.context.scene.collection.objects.link(obj)
    else:
        raise RuntimeError(f'Unsupported avatar format: {ext}')
    new = [o for o in bpy.data.objects if o not in before]
    armature = next((o for o in new if o.type == 'ARMATURE'), None)
    if armature is None:
        raise RuntimeError('Avatar file contains no armature (needs a rigged character)')
    return armature, new


def fit_height(armature, children, target_height_m):
    """Scale the whole character so its bounding box height matches the person."""
    from mathutils import Vector
    lo, hi = float('inf'), float('-inf')
    for obj in children:
        if obj.type != 'MESH':
            continue
        for corner in obj.bound_box:
            z = (obj.matrix_world @ Vector(corner)).z
            lo, hi = min(lo, z), max(hi, z)
    height = hi - lo
    if height <= 0:
        return 1.0
    factor = target_height_m / height
    armature.scale *= factor
    bpy.context.view_layer.update()
    print(f'[avatar] scaled x{factor:.3f} to {target_height_m:.2f} m')
    return factor


def loop_walk_cycle(armature, frame_end):
    """Repeat the imported action for the whole shot using a Cycles F-modifier."""
    ad = armature.animation_data
    if ad is None or ad.action is None:
        print('[avatar] no action found, avatar will slide without animation')
        return
    for fc in compat.iter_fcurves(ad.action):
        if not any(m.type == 'CYCLES' for m in fc.modifiers):
            mod = fc.modifiers.new('CYCLES')
            mod.mode_before = 'REPEAT'
            mod.mode_after = 'REPEAT'
    print(f'[avatar] looped action "{ad.action.name}" until frame {frame_end}')


def follow_path(armature, path_obj, frame_start, frame_end, lead_m=0.0, path_length_m=1.0):
    """Make the character walk along the path, facing the walking direction."""
    from walkthrough import add_follow_path
    con = add_follow_path(armature, path_obj, frame_start, frame_end,
                          look_ahead=min(0.95, lead_m / max(path_length_m, 0.01)))
    con.use_curve_follow = True
    con.forward_axis = 'TRACK_NEGATIVE_Y'  # Mixamo / MPFB characters face -Y in Blender
    con.up_axis = 'UP_Z'
    return con


def place_avatar(avatar_path, path_obj, frame_end, path_length_m, height_m=1.70,
                 ahead_of_camera_m=2.5):
    armature, objects = import_avatar(avatar_path)
    fit_height(armature, objects, height_m)
    # The path runs at eye height; shift the character down so its feet touch the floor.
    armature.delta_location.z = -float(path_obj.get('eye_height', 1.6))
    follow_path(armature, path_obj, 1, frame_end, ahead_of_camera_m, path_length_m)
    loop_walk_cycle(armature, frame_end)
    return armature

