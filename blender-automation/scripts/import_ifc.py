"""Import the Archicad IFC (or a fallback mesh export) into the current Blender scene.

Preferred path: Bonsai (formerly BlenderBIM, free, GPL) -> native IFC geometry with
materials/colours and IFC properties kept.
Fallback: glTF / FBX / OBJ exported from Archicad (File > Save As) when Bonsai is not
installed.
"""
import os

import bpy

import blender_compat as compat

BONSAI_MODULES = ['bonsai', 'blenderbim']


def _import_with_bonsai(path):
    enabled = compat.enable_addon_if_needed(BONSAI_MODULES)
    if enabled is None:
        return False
    print(f'[import_ifc] using {enabled}')
    ops = bpy.ops.bim
    if hasattr(ops, 'load_project'):
        # Bonsai >= 0.8: loads geometry + IFC data. should_start_fresh_session keeps
        # whatever is already in the scene (camera, avatar) intact.
        try:
            ops.load_project(filepath=path, should_start_fresh_session=False)
        except TypeError:
            ops.load_project(filepath=path)
        return True
    if hasattr(bpy.ops, 'import_ifc') and hasattr(bpy.ops.import_ifc, 'bim'):
        bpy.ops.import_ifc.bim(filepath=path)  # legacy BlenderBIM
        return True
    return False


def _import_mesh_fallback(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in ('.glb', '.gltf'):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == '.fbx':
        bpy.ops.import_scene.fbx(filepath=path, use_custom_normals=True)
    elif ext == '.obj':
        bpy.ops.wm.obj_import(filepath=path)
    elif ext == '.usd' or ext == '.usdc' or ext == '.usda' or ext == '.usdz':
        bpy.ops.wm.usd_import(filepath=path)
    else:
        raise RuntimeError(f'Unsupported model format: {ext}')


def import_model(path, collection_name='BIM'):
    path = os.path.abspath(path)
    if not os.path.isfile(path):
        raise FileNotFoundError(path)

    before = set(bpy.data.objects)
    ext = os.path.splitext(path)[1].lower()
    if ext in ('.ifc', '.ifczip', '.ifcxml'):
        if not _import_with_bonsai(path):
            raise RuntimeError(
                'Bonsai extension not found. Install it from Edit > Preferences > Get Extensions '
                '(search "Bonsai"), or export glTF/FBX from Archicad and pass that file instead.')
    else:
        _import_mesh_fallback(path)

    new_objects = [o for o in bpy.data.objects if o not in before]
    coll = bpy.data.collections.get(collection_name) or bpy.data.collections.new(collection_name)
    if coll.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(coll)
    for obj in new_objects:
        for c in list(obj.users_collection):
            if c is not coll:
                c.objects.unlink(obj)
        if obj.name not in coll.objects:
            coll.objects.link(obj)
    print(f'[import_ifc] imported {len(new_objects)} objects into "{collection_name}"')
    return new_objects


def scene_bounds(objects):
    """Axis aligned bounds (min, max) of the given mesh objects in world space."""
    from mathutils import Vector
    lo = Vector((float('inf'),) * 3)
    hi = Vector((float('-inf'),) * 3)
    for obj in objects:
        if obj.type != 'MESH':
            continue
        for corner in obj.bound_box:
            p = obj.matrix_world @ Vector(corner)
            lo = Vector(map(min, lo, p))
            hi = Vector(map(max, hi, p))
    return lo, hi
