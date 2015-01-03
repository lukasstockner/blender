# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Populate a template file (POT format currently) from Blender RNA/py/C data.
# XXX: This script is meant to be used from inside Blender!
#      You should not directly use this script, rather use update_msg.py!

import collections
import copy
import datetime
import os
import re
import sys

# XXX Relative import does not work here when used from Blender...
#~ from bl_previews_utils import utils

import bpy
from mathutils import Vector, Euler

##### Utils #####

def object_children_recursive(ob):
    for child in ob.children:
        yield child
        yield from object_children_recursive(child)

def object_merge_bbox(bbox, ob, ob_space):
    if ob.bound_box:
        ob_bbox = ob.bound_box
    else:
        ob_bbox = ((-ob.scale.x, -ob.scale.y, -ob.scale.z), (ob.scale.x, ob.scale.y, ob.scale.z))
    for v in ob.bound_box:
        v = ob_space.matrix_world.inverted() * ob.matrix_world * Vector(v)
        if bbox[0].x > v.x:
            bbox[0].x = v.x
        if bbox[0].y > v.y:
            bbox[0].y = v.y
        if bbox[0].z > v.z:
            bbox[0].z = v.z
        if bbox[1].x < v.x:
            bbox[1].x = v.x
        if bbox[1].y < v.y:
            bbox[1].y = v.y
        if bbox[1].z < v.z:
            bbox[1].z = v.z


def do_previews_bi(do_objects, do_groups):
    render_scene = bpy.data.scenes.new("TEMP_preview_render_scene")
    render_world = bpy.data.worlds.new("TEMP_preview_render_world")
    render_camera_data = bpy.data.cameras.new("TEMP_preview_render_camera")
    render_camera = bpy.data.objects.new("TEMP_preview_render_camera", render_camera_data)
    render_lamp_data = bpy.data.lamps.new("TEMP_preview_render_lamp", 'SPOT')
    render_lamp = bpy.data.objects.new("TEMP_preview_render_lamp", render_lamp_data)
    render_image = None

    objects_ignored = {render_camera, render_lamp}
    groups_ignored = {}

    render_world.use_sky_blend = True
    render_world.horizon_color = 0.9, 0.9, 0.9
    render_world.zenith_color = 0.5, 0.5, 0.5
    render_world.ambient_color = 0.1, 0.1, 0.1
    render_world.light_settings.use_environment_light = True
    render_world.light_settings.environment_energy = 1.0
    render_world.light_settings.environment_color = 'SKY_COLOR'
    render_scene.world = render_world

    render_camera.rotation_euler = Euler((1.1635528802871704, 0.0, 0.7853981852531433), 'XYZ')  # (66.67, 0.0, 45.0)
    render_scene.camera = render_camera
    render_scene.objects.link(render_camera)

    render_lamp.rotation_euler = Euler((0.7853981852531433, 0.0, 1.7453292608261108), 'XYZ')  # (45.0, 0.0, 100.0)
    render_lamp_data.falloff_type = 'CONSTANT'
    render_lamp_data.spot_size = 1.0471975803375244  # 60
    render_scene.objects.link(render_lamp)

    render_scene.render.resolution_x = 128
    render_scene.render.resolution_y = 128
    render_scene.render.resolution_percentage = 100
    render_scene.render.alpha_mode = 'TRANSPARENT'
    render_scene.render.filepath = '/tmp/TEMP_preview_render.png'  # XXX To be done properly!!!!

    prev_scene = bpy.context.screen.scene
    bpy.context.screen.scene = render_scene

    if do_objects:
        prev_shown = tuple(ob.hide_render for ob in bpy.data.objects)
        for ob in bpy.data.objects:
            if ob in objects_ignored:
                continue
            ob.hide_render = True
        for root in bpy.data.objects:
            if root in objects_ignored:
                continue
            if root.type not in {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT'}:
                continue
            bbox = (Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9)))
            objects = (root,) # + tuple(object_children_recursive(ob))
            for ob in objects:
                if ob.name not in render_scene.objects:
                    render_scene.objects.link(ob)
                ob.hide_render = False
                render_scene.update()
                object_merge_bbox(bbox, ob, render_camera)
            # Our bbox has been generated in camera local space, bring it back in world one
            bbox[0][:] = render_camera.matrix_world * bbox[0]
            bbox[1][:] = render_camera.matrix_world * bbox[1]
            cos = (
                bbox[0].x, bbox[0].y, bbox[0].z,
                bbox[0].x, bbox[0].y, bbox[1].z,
                bbox[0].x, bbox[1].y, bbox[0].z,
                bbox[0].x, bbox[1].y, bbox[1].z,
                bbox[1].x, bbox[0].y, bbox[0].z,
                bbox[1].x, bbox[0].y, bbox[1].z,
                bbox[1].x, bbox[1].y, bbox[0].z,
                bbox[1].x, bbox[1].y, bbox[1].z,
            )
            loc, ortho_scale = render_camera.camera_fit_coords(render_scene, cos)
            render_camera.location = loc
            loc, ortho_scale = render_lamp.camera_fit_coords(render_scene, cos)
            render_lamp.location = loc
            render_scene.update()

            bpy.ops.render.render(write_still=True)

            if render_image is None:
                render_image = bpy.data.images.load(render_scene.render.filepath)
            else:
                render_image.reload()
            pix = tuple(int(r * 255) + int(g * 255) * 256 + int(b * 255) * 256**2 + int(a * 255) * 256**3 for r, g, b, a in zip(*[iter(render_image.pixels)] * 4))  # XXX To be done properly!!!!!!
            root.preview.image_size = (128, 128)
            root.preview.image_pixels = pix

            for ob in objects:
                render_scene.objects.unlink(ob)
                ob.hide_render = True

        for ob, is_rendered in zip(bpy.data.objects, prev_shown):
            ob.hide_render = is_rendered

    if do_groups:
        for grp in bpy.data.groups:
            if grp in groups_ignored:
                continue
            bpy.ops.object.group_instance_add(group=grp.name)
            grp_ob = next((ob for ob in render_scene.objects if ob.dupli_group and ob.dupli_group.name == grp.name))
            bbox = (Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9)))
            render_scene.update()
            for ob in grp.objects:
                object_merge_bbox(bbox, ob, render_camera)
            # Our bbox has been generated in camera local space, bring it back in world one
            bbox[0][:] = render_camera.matrix_world * bbox[0]
            bbox[1][:] = render_camera.matrix_world * bbox[1]
            cos = (
                bbox[0].x, bbox[0].y, bbox[0].z,
                bbox[0].x, bbox[0].y, bbox[1].z,
                bbox[0].x, bbox[1].y, bbox[0].z,
                bbox[0].x, bbox[1].y, bbox[1].z,
                bbox[1].x, bbox[0].y, bbox[0].z,
                bbox[1].x, bbox[0].y, bbox[1].z,
                bbox[1].x, bbox[1].y, bbox[0].z,
                bbox[1].x, bbox[1].y, bbox[1].z,
            )
            loc, ortho_scale = render_camera.camera_fit_coords(render_scene, cos)
            render_camera.location = loc
            loc, ortho_scale = render_lamp.camera_fit_coords(render_scene, cos)
            render_lamp.location = loc
            render_scene.update()

            bpy.ops.render.render(write_still=True)

            if render_image is None:
                render_image = bpy.data.images.load(render_scene.render.filepath)
            else:
                render_image.reload()
            pix = tuple(int(r * 255) + int(g * 255) * 256 + int(b * 255) * 256**2 + int(a * 255) * 256**3 for r, g, b, a in zip(*[iter(render_image.pixels)] * 4))  # XXX To be done properly!!!!!!
            grp.preview.image_size = (128, 128)
            grp.preview.image_pixels = pix

            render_scene.objects.unlink(grp_ob)

    bpy.context.screen.scene = prev_scene
    render_scene.world = None
    render_scene.camera = None
    render_scene.objects.unlink(render_camera)
    render_scene.objects.unlink(render_lamp)
    bpy.data.scenes.remove(render_scene)
    bpy.data.worlds.remove(render_world)
    bpy.data.objects.remove(render_camera)
    bpy.data.cameras.remove(render_camera_data)
    bpy.data.objects.remove(render_lamp)
    bpy.data.lamps.remove(render_lamp_data)
    if render_image is not None:
        render_image.user_clear()
        bpy.data.images.remove(render_image)

    print("Saving %s..." % bpy.data.filepath)
    bpy.ops.wm.save_mainfile()


def do_clear_previews(do_objects, do_groups):
    if do_objects:
        for ob in bpy.data.objects:
            ob.preview.image_size = (0, 0)

    if do_groups:
        for grp in bpy.data.groups:
            grp.preview.image_size = (0, 0)

    print("Saving %s..." % bpy.data.filepath)
    bpy.ops.wm.save_mainfile()


def main():
    try:
        import bpy
    except ImportError:
        print("This script must run from inside blender")
        return

    import sys
    back_argv = sys.argv
    # Get rid of Blender args!
    sys.argv = sys.argv[sys.argv.index("--") + 1:]

    import argparse
    parser = argparse.ArgumentParser(description="Use Blender to generate previews for currently open Blender file's items.")
    parser.add_argument('--clear', default=False, action="store_true", help="Clear previews instead of generating them.")
    parser.add_argument('--no_objects', default=True, action="store_false", help="Do not generate/clear previews for object IDs.")
    parser.add_argument('--no_groups', default=True, action="store_false", help="Do not generate/clear previews for group IDs.")
    args = parser.parse_args()

    if args.clear:
        do_clear_previews(do_objects=args.no_objects, do_groups=args.no_groups)
    else:
        do_previews_bi(do_objects=args.no_objects, do_groups=args.no_groups)

    sys.argv = back_argv


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    print(" *** Blend file {} *** \n".format(bpy.data.filepath))
    main()
    bpy.ops.wm.quit_blender()
