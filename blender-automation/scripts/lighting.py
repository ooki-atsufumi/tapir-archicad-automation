"""Lighting & world setup for interior/exterior architectural walkthroughs."""
import math
import os

import bpy


def setup_world(hdri_path=None, sun_elevation_deg=45.0, sun_azimuth_deg=135.0,
                strength=1.0, sky_turbidity=3.0):
    scene = bpy.context.scene
    world = scene.world or bpy.data.worlds.new('World')
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputWorld')
    bg = nt.nodes.new('ShaderNodeBackground')
    bg.inputs['Strength'].default_value = strength
    nt.links.new(bg.outputs['Background'], out.inputs['Surface'])

    if hdri_path and os.path.isfile(hdri_path):
        env = nt.nodes.new('ShaderNodeTexEnvironment')
        env.image = bpy.data.images.load(hdri_path)
        nt.links.new(env.outputs['Color'], bg.inputs['Color'])
        print(f'[lighting] HDRI {os.path.basename(hdri_path)}')
    else:
        sky = nt.nodes.new('ShaderNodeTexSky')
        sky.sky_type = 'NISHITA'
        sky.sun_elevation = math.radians(sun_elevation_deg)
        sky.sun_rotation = math.radians(sun_azimuth_deg)
        sky.air_density = 1.0
        sky.dust_density = sky_turbidity / 3.0
        nt.links.new(sky.outputs['Color'], bg.inputs['Color'])
        print('[lighting] procedural Nishita sky')
    return world


def add_sun(elevation_deg=45.0, azimuth_deg=135.0, strength=4.0, angle_deg=0.53):
    sun_data = bpy.data.lights.new('Sun', 'SUN')
    sun_data.energy = strength
    sun_data.angle = math.radians(angle_deg)
    sun = bpy.data.objects.new('Sun', sun_data)
    bpy.context.scene.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(90.0 - elevation_deg), 0.0, math.radians(azimuth_deg + 180.0))
    return sun


def add_interior_fill(bounds_lo, bounds_hi, ceiling_offset=0.3, power_w=300.0, grid=(2, 2)):
    """Area lights just below the ceiling so windows are not the only light source."""
    lights = []
    size_x = (bounds_hi.x - bounds_lo.x) / grid[0]
    size_y = (bounds_hi.y - bounds_lo.y) / grid[1]
    z = bounds_hi.z - ceiling_offset
    for i in range(grid[0]):
        for j in range(grid[1]):
            data = bpy.data.lights.new(f'Fill_{i}_{j}', 'AREA')
            data.energy = power_w
            data.shape = 'RECTANGLE'
            data.size = size_x * 0.6
            data.size_y = size_y * 0.6
            obj = bpy.data.objects.new(data.name, data)
            obj.location = (bounds_lo.x + size_x * (i + 0.5), bounds_lo.y + size_y * (j + 0.5), z)
            bpy.context.scene.collection.objects.link(obj)
            lights.append(obj)
    return lights


def set_color_management(look='AgX - Medium High Contrast', exposure=0.0):
    scene = bpy.context.scene
    try:
        scene.view_settings.view_transform = 'AgX'
        scene.view_settings.look = look
    except TypeError:
        scene.view_settings.view_transform = 'Filmic'
    scene.view_settings.exposure = exposure
