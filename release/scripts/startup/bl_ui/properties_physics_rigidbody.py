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
from bpy.types import Panel
from bl_ui.properties_physics_common import effector_weights_ui


class PHYSICS_PT_rigidbody_panel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"


class PHYSICS_PT_rigid_body(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Rigid Body"

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                (not context.scene.render.use_game_engine))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbo = ob.rigid_body

        if rbo is not None:
            layout.prop(rbo, "type", text="Type")
            split = layout.split()
            col = split.column()
            if rbo.type == 'ACTIVE':
                col.prop(rbo, "enabled", text="Dynamic")
            col.prop(rbo, "kinematic", text="Animated")
            col = split.column()
            col.prop(rbo, "trigger", text="Trigger")
            col.prop(rbo, "ghost", text="Ghost")

            if rbo.type == 'ACTIVE':
                layout.prop(rbo, "mass")


class PHYSICS_PT_rigid_body_collisions(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Rigid Body Collisions"

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                (not context.scene.render.use_game_engine))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbo = ob.rigid_body

        layout.prop(rbo, "collision_shape", text="Shape")
        
        if rbo.collision_shape in {'MESH', 'CONVEX_HULL', 'APPROX'}:
            layout.prop(rbo, "mesh_source", text="Source")

        if rbo.collision_shape == 'MESH' and rbo.mesh_source == 'DEFORM':
            layout.prop(rbo, "use_deform", text="Deforming")

        split = layout.split()

        col = split.column()
        col.label(text="Surface Response:")
        col.prop(rbo, "friction")
        col.prop(rbo, "restitution", text="Bounciness")

        col = split.column()
        col.label(text="Sensitivity:")
        if rbo.collision_shape in {'MESH', 'CONE'}:
            col.prop(rbo, "collision_margin", text="Margin")
        else:
            col.prop(rbo, "use_margin")
            sub = col.column()
            sub.active = rbo.use_margin
            sub.prop(rbo, "collision_margin", text="Margin")

        layout.prop(rbo, "collision_groups")


class PHYSICS_PT_rigid_body_dynamics(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Rigid Body Dynamics"
    bl_default_closed = True

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                obj.rigid_body.type == 'ACTIVE' and
                (not context.scene.render.use_game_engine))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        rbo = ob.rigid_body

        split = layout.split()

        col = split.column()
        col.label(text="Activation:")
        col.prop(rbo, "use_start_deactivated")
        sub = col.column()
        sub.active = rbo.use_start_deactivated
        sub.prop(rbo, "activation_type")

        col = split.column()
        col.label(text="Deactivation:")
        col.prop(rbo, "use_deactivation")
        sub = col.column()
        sub.active = rbo.use_deactivation
        sub.prop(rbo, "deactivate_linear_velocity", text="Linear Vel")
        sub.prop(rbo, "deactivate_angular_velocity", text="Angular Vel")

        col = layout.column()
        col.label(text="Damping:")
        col.prop(rbo, "linear_damping", text="Translation")
        col.prop(rbo, "angular_damping", text="Rotation")

class PHYSICS_PT_rigid_body_field_weights(PHYSICS_PT_rigidbody_panel, Panel):
    bl_label = "Rigid Body Field Weights"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.rigid_body and
                obj.rigid_body.type == 'ACTIVE' and
                (not context.scene.render.use_game_engine))

    def draw(self, context):
        ob = context.object
        rbo = ob.rigid_body

        effector_weights_ui(self, context, rbo.effector_weights, 'RIGID_BODY')

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
