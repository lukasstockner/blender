# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

if "bpy" in locals():
    import imp
    imp.reload(preview_render)
else:
    import bpy
    from bpy.types import (Operator,
                          )
    from bpy.props import (BoolProperty,
                           CollectionProperty,
                           EnumProperty,
                           FloatProperty,
                           FloatVectorProperty,
                           IntProperty,
                           PointerProperty,
                           StringProperty,
                           )
    from bl_previews_utils import bl_previews_render as preview_render


import os
import subprocess


class SYSTEM_OT_generate_previews(Operator):
    """Generate selected .blend file's object previews"""
    bl_idname = "system.blend_generate_previews"
    bl_label = "Generate Blend Files Previews"
    bl_options = {'REGISTER'}

    # -----------
    # File props.
    files = CollectionProperty(type=bpy.types.OperatorFileListElement, options={'HIDDEN', 'SKIP_SAVE'})

    directory = StringProperty(maxlen=1024, subtype='FILE_PATH', options={'HIDDEN', 'SKIP_SAVE'})

    # Show only images/videos, and directories!
    filter_blender = BoolProperty(default=True, options={'HIDDEN', 'SKIP_SAVE'})
    filter_folder = BoolProperty(default=True, options={'HIDDEN', 'SKIP_SAVE'})

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        context.window_manager.progress_begin(0, len(self.files))
        context.window_manager.progress_update(0)
        for i, fn in enumerate(self.files):
            blen_path = os.path.join(self.directory, fn.name)
            cmmd = (
                bpy.app.binary_path,
                #~ "--background",
                "--factory-startup",
                blen_path,
                "--python",
                os.path.join(os.path.dirname(preview_render.__file__), "bl_previews_render.py"),
                "--",
                "bl_previews_render.py",  # arg parser expects first arg to be prog name!
            )
            # Not working (UI is not refreshed...).
            #self.report({'INFO'}, "Extracting messages, this will take some time...")
            if subprocess.call(cmmd):
                self.report({'ERROR'}, "Previews generation process failed for file '%s'!" % blend_path)
                context.window_manager.progress_end()
                return {'CANCELLED'}
            context.window_manager.progress_update(i + 1)
        context.window_manager.progress_end()

        return {'FINISHED'}

class SYSTEM_OT_clear_previews(Operator):
    """Clear selected .blend file's object previews"""
    bl_idname = "system.blend_clear_previews"
    bl_label = "Clear Blend Files Previews"
    bl_options = {'REGISTER'}

    # -----------
    # File props.
    files = CollectionProperty(type=bpy.types.OperatorFileListElement, options={'HIDDEN', 'SKIP_SAVE'})

    directory = StringProperty(maxlen=1024, subtype='FILE_PATH', options={'HIDDEN', 'SKIP_SAVE'})

    # Show only images/videos, and directories!
    filter_blender = BoolProperty(default=True, options={'HIDDEN', 'SKIP_SAVE'})
    filter_folder = BoolProperty(default=True, options={'HIDDEN', 'SKIP_SAVE'})

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        context.window_manager.progress_begin(0, len(self.files))
        context.window_manager.progress_update(0)
        for i, fn in enumerate(self.files):
            blen_path = os.path.join(self.directory, fn.name)
            cmmd = (
                bpy.app.binary_path,
                #~ "--background",
                "--factory-startup",
                blen_path,
                "--python",
                os.path.join(os.path.dirname(preview_render.__file__), "bl_previews_render.py"),
                "--",
                "bl_previews_render.py",  # arg parser expects first arg to be prog name!
                "--clear",
            )
            # Not working (UI is not refreshed...).
            #self.report({'INFO'}, "Extracting messages, this will take some time...")
            if subprocess.call(cmmd):
                self.report({'ERROR'}, "Previews clearing process failed for file '%s'!" % blend_path)
                context.window_manager.progress_end()
                return {'CANCELLED'}
            context.window_manager.progress_update(i + 1)
        context.window_manager.progress_end()

        return {'FINISHED'}
