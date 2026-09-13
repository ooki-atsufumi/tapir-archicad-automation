"""Camera walkthrough: builds a smooth path from waypoints and drives a camera along it.

Waypoints are given in Archicad project coordinates (metres, same origin as the IFC).
The camera is placed at eye height above each waypoint and looks along the path.
"""
import math

import bpy
from mathutils import Vector  # noqa: F401

import blender_compat as compat


def make_path(name, waypoints, eye_height=1.6, smooth=True):
    curve = bpy.data.curves.new(name, 'CURVE')
    curve.dimensions = '3D'
    curve.use_path = True
    spline = curve.splines.new('NURBS' if smooth else 'POLY')
    spline.points.add(len(waypoints) - 1)
    for pt, wp in zip(spline.points, waypoints):
        z = wp[2] if len(wp) > 2 else 0.0
        pt.co = (wp[0], wp[1], z + eye_height, 1.0)
    if smooth:
        spline.order_u = min(4, len(waypoints))
        spline.use_endpoint_u = True
    obj = bpy.data.objects.new(name, curve)
    obj['eye_height'] = eye_height
    obj['floor_z'] = min((wp[2] if len(wp) > 2 else 0.0) for wp in waypoints)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def path_length(path_obj, samples=200):
    """Approximate world-space length of the first spline by sampling the evaluated curve."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = path_obj.evaluated_get(depsgraph)
    mesh = eval_obj.to_mesh()
    try:
        verts = [eval_obj.matrix_world @ v.co for v in mesh.vertices]
    finally:
        eval_obj.to_mesh_clear()
    if len(verts) < 2:
        return 0.0
    return sum((verts[i + 1] - verts[i]).length for i in range(len(verts) - 1))


def add_follow_path(obj, path_obj, frame_start, frame_end, look_ahead=0.0):
    con = obj.constraints.new('FOLLOW_PATH')
    con.target = path_obj
    con.use_fixed_location = True
    con.use_curve_follow = False
    con.offset_factor = min(1.0, look_ahead)
    con.keyframe_insert('offset_factor', frame=frame_start)
    con.offset_factor = 1.0
    con.keyframe_insert('offset_factor', frame=frame_end)
    # Linear interpolation -> constant walking speed
    if obj.animation_data and obj.animation_data.action:
        for fc in compat.iter_fcurves(obj.animation_data.action):
            if 'offset_factor' in fc.data_path:
                for kp in fc.keyframe_points:
                    kp.interpolation = 'LINEAR'
    return con


def create_camera_walk(waypoints, eye_height=1.6, walk_speed=1.2, fps=30,
                       focal_length_mm=24.0, look_ahead_m=2.0, name='WalkCam',
                       sway=True):
    """Creates path + camera + look-at target. Returns (camera, path, frame_end)."""
    scene = bpy.context.scene
    scene.render.fps = fps
    path = make_path(f'{name}_Path', waypoints, eye_height)

    length = max(path_length(path), 0.01)
    duration_s = length / walk_speed
    frame_end = int(math.ceil(duration_s * fps)) + 1
    scene.frame_start = 1
    scene.frame_end = frame_end

    cam_data = bpy.data.cameras.new(name)
    cam_data.lens = focal_length_mm
    cam_data.sensor_width = 36.0
    cam_data.clip_start = 0.05
    cam_data.clip_end = 500.0
    cam = bpy.data.objects.new(name, cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam

    target = bpy.data.objects.new(f'{name}_Target', None)
    target.empty_display_size = 0.2
    scene.collection.objects.link(target)

    add_follow_path(cam, path, 1, frame_end)
    ahead = min(0.95, look_ahead_m / length)
    add_follow_path(target, path, 1, frame_end, look_ahead=ahead)

    track = cam.constraints.new('TRACK_TO')
    track.target = target
    track.track_axis = 'TRACK_NEGATIVE_Z'
    track.up_axis = 'UP_Y'

    if sway:
        _add_head_bob(cam, frame_end, fps, walk_speed)

    print(f'[walkthrough] path {length:.1f} m, {duration_s:.1f} s, {frame_end} frames @ {fps} fps')
    return cam, path, frame_end


def _add_head_bob(cam, frame_end, fps, walk_speed, amplitude=0.012):
    """Subtle vertical bob synced to step frequency (about 1.8 steps/s at 1.2 m/s)."""
    steps_per_s = 1.5 * walk_speed
    period = fps / steps_per_s
    if cam.animation_data is None:
        cam.animation_data_create()
    # delta_location keeps the constraint-driven location untouched
    frame = 1
    while frame <= frame_end:
        cam.delta_location.z = 0.0
        cam.keyframe_insert('delta_location', index=2, frame=frame)
        cam.delta_location.z = amplitude
        cam.keyframe_insert('delta_location', index=2, frame=frame + period / 2)
        frame += period
    for fc in compat.iter_fcurves(cam.animation_data.action):
        if fc.data_path == 'delta_location':
            for kp in fc.keyframe_points:
                kp.interpolation = 'SINE'
                kp.easing = 'EASE_IN_OUT'
    cam.delta_location.z = 0.0


def add_dof(cam, focus_target=None, fstop=4.0):
    cam.data.dof.use_dof = True
    cam.data.dof.aperture_fstop = fstop
    if focus_target is not None:
        cam.data.dof.focus_object = focus_target
