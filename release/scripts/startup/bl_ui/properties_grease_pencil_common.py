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


import bpy
from bpy.types import Menu


class GreasePencilDrawingToolsPanel():
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    # bl_region_type = 'TOOLS'
    bl_label = "Grease Pencil"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'

    @staticmethod
    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        col.label(text="Draw:")
        row = col.row(align=True)
        row.operator("gpencil.draw", text="Draw").mode = 'DRAW'
        row.operator("gpencil.draw", text="Erase").mode = 'ERASER'

        row = col.row(align=True)
        row.operator("gpencil.draw", text="Line").mode = 'DRAW_STRAIGHT'
        row.operator("gpencil.draw", text="Poly").mode = 'DRAW_POLY'


        row = col.row(align=True)
        row.prop(context.tool_settings, "use_grease_pencil_sessions", text="Continuous Drawing")

        gpd = context.gpencil_data
        if gpd:
            col.separator()

            col.label(text="Stroke Placement:")

            row = col.row(align=True)
            row.prop_enum(gpd, "draw_mode", 'VIEW')
            row.prop_enum(gpd, "draw_mode", 'CURSOR')

            if context.space_data.type == 'VIEW_3D':
                row = col.row(align=True)
                row.prop_enum(gpd, "draw_mode", 'SURFACE')
                row.prop_enum(gpd, "draw_mode", 'STROKE')

                row = col.row(align=False)
                row.active = gpd.draw_mode in ('SURFACE', 'STROKE')
                row.prop(gpd, "use_stroke_endpoints")

        if context.space_data.type == 'VIEW_3D':
            col.separator()
            col.separator()

            col.label(text="Measure:")
            col.operator("view3d.ruler")


class GreasePencilStrokeEditPanel():
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    bl_label = "Edit Strokes"
    bl_category = "Grease Pencil"
    bl_region_type = 'TOOLS'

    @staticmethod
    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        col.label(text="Select:")
        subcol = col.column(align=True)
        subcol.active = bool(context.editable_gpencil_strokes)
        subcol.operator("gpencil.select_all", text="Select All")
        subcol.operator("gpencil.select_circle")

        col.separator()

        col.label(text="Edit:")
        subcol = col.column(align=True)
        subcol.active = bool(context.editable_gpencil_strokes)
        subcol.operator("gpencil.strokes_duplicate", text="Duplicate")
        subcol.operator("transform.mirror", text="Mirror").gpencil_strokes = True

        col.separator()

        subcol = col.column(align=True)
        subcol.active = bool(context.editable_gpencil_strokes)
        subcol.operator("transform.translate").gpencil_strokes = True   # icon='MAN_TRANS'
        subcol.operator("transform.rotate").gpencil_strokes = True      # icon='MAN_ROT'
        subcol.operator("transform.resize", text="Scale").gpencil_strokes = True      # icon='MAN_SCALE'


###############################


class GPENCIL_PIE_tool_palette(Menu):
    """A pie menu for quick access to Grease Pencil tools"""
    bl_label = "Grease Pencil Tools"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()

        # W - Drawing Settings
        col = pie.column()
        col.operator("gpencil.draw", text="Draw", icon='GREASEPENCIL').mode = 'DRAW'
        col.operator("gpencil.draw", text="Straight Lines", icon='LINE_DATA').mode = 'DRAW_STRAIGHT'
        col.operator("gpencil.draw", text="Poly", icon='MESH_DATA').mode = 'DRAW_POLY'

        # E - Eraser
        # XXX: needs a dedicated icon...
        pie.operator("gpencil.draw", text="Eraser", icon='FORCE_CURVE').mode = 'ERASER'

        # Editing tools
        if context.editable_gpencil_strokes:
            # S - Select
            col = pie.column()
            col.operator("gpencil.select_all", text="Select All", icon='PARTICLE_POINT')
            col.operator("gpencil.select_circle", text="Circle Select", icon='META_EMPTY')
            #col.operator("gpencil.select", text="Stroke Under Mouse").entire_strokes = True

            # N - Move
            pie.operator("transform.translate", icon='MAN_TRANS').gpencil_strokes = True

            # NW - Rotate
            pie.operator("transform.rotate", icon='MAN_ROT').gpencil_strokes = True

            # NE - Scale
            pie.operator("transform.resize", text="Scale", icon='MAN_SCALE').gpencil_strokes = True

            # SW - Copy
            pie.operator("gpencil.strokes_duplicate", text="Copy...", icon='PARTICLE_PATH')

            # SE - Mirror?  (Best would be to do Settings here...)
            pie.operator("transform.mirror", text="Mirror", icon='MOD_MIRROR').gpencil_strokes = True
