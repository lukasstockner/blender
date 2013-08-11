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
from bpy.types import MenuBar, Menu
from bpy.app.translations import contexts as i18n_contexts

# *********** UTILITIES ***********

class ShowHideMenu():
    bl_label = "Show/Hide"
    _operator_name = ""

    def draw(self, context):
        layout = self.layout

        layout.operator("%s.reveal" % self._operator_name, text="Show Hidden")
        layout.operator("%s.hide" % self._operator_name, text="Hide Selected").unselected = False
        layout.operator("%s.hide" % self._operator_name, text="Hide Unselected").unselected = True

class VIEW3D_MT_menubar_particle_hide(ShowHideMenu, Menu):
    _operator_name = "particle"

class VIEW3D_MT_menubar_pose_hide(ShowHideMenu, Menu):
    _operator_name = "pose"

class VIEW3D_MT_menubar_mesh_hide(ShowHideMenu, Menu):
    _operator_name = "mesh"

class VIEW3D_MT_menubar_curve_hide(ShowHideMenu, Menu):
    _operator_name = "curve"

class VIEW3D_MT_menubar_obj_hide(Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout
        layout.operator("object.hide_view_clear", text="Show Hidden")
        layout.operator("object.hide_view_set", text="Hide Selected").unselected = False
        layout.operator("object.hide_view_set", text="Hide Unselected").unselected = True

class VIEW3D_MT_menubar_meta_hide(Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout

        layout.operator("mball.reveal_metaelems", text="Show Hidden")
        layout.operator("mball.hide_metaelems", text="Hide Selected").unselected = False
        layout.operator("mball.hide_metaelems", text="Hide Unselected").unselected = True


class VIEW3D_MT_menubar_general_history(Menu):
    bl_label = "History"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")
        layout.separator()
        layout.operator("screen.redo_last")
        layout.operator("screen.repeat_last")
        layout.operator("screen.repeat_history", text="History...")


class VIEW3D_MT_menubar_general_snap(Menu):
    bl_label = "Snap"

    def draw(self, context):
        layout = self.layout
        layout.operator("view3d.snap_selected_to_grid", text="Selection to Grid")
        layout.operator("view3d.snap_selected_to_cursor", text="Selection to Cursor")
        layout.separator()
        layout.operator("view3d.snap_cursor_to_selected", text="Cursor to Selected")
        layout.operator("view3d.snap_cursor_to_center", text="Cursor to Center")
        layout.operator("view3d.snap_cursor_to_grid", text="Cursor to Grid")
        layout.operator("view3d.snap_cursor_to_active", text="Cursor to Active")


def general_mirror(context, layout):
    layout.operator("transform.mirror", text="Mirror Interactive")
    layout.operator_context = 'INVOKE_REGION_WIN'
    props = layout.operator("transform.mirror", text="Mirror X (global)")
    props.constraint_axis = (True, False, False)
    props.constraint_orientation = 'GLOBAL'
    props = layout.operator("transform.mirror", text="Mirror Y (global)")
    props.constraint_axis = (False, True, False)
    props.constraint_orientation = 'GLOBAL'
    props = layout.operator("transform.mirror", text="Mirror Z (global)")
    props.constraint_axis = (False, False, True)
    props.constraint_orientation = 'GLOBAL'

    if context.edit_object:
        props = layout.operator("transform.mirror", text="Mirror X (local)")
        props.constraint_axis = (True, False, False)
        props.constraint_orientation = 'LOCAL'
        props = layout.operator("transform.mirror", text="Mirror Y (local)")
        props.constraint_axis = (False, True, False)
        props.constraint_orientation = 'LOCAL'
        props = layout.operator("transform.mirror", text="Mirror Z (local)")
        props.constraint_axis = (False, False, True)
        props.constraint_orientation = 'LOCAL'

        layout.operator("object.vertex_group_mirror")



class VIEW3D_MT_menubar_general_animation(Menu):
    bl_label = "Animation"

    def draw(self, context): 
        layout = self.layout 

        layout.operator("anim.keyframe_insert_menu")
        layout.operator("anim.keyframe_delete_v3d")
        layout.operator("anim.keyframe_clear_v3d", text="Clear Keyframes...")
        layout.operator("anim.keying_set_active_set", text="Change Keying Set...")
        layout.separator()
        if context.mode == 'POSE':
            layout.operator("pose.paths_calculate", text="Calculate Motion Paths")
            layout.operator("pose.paths_clear", text="Clear Motion Paths")
        else:
            layout.operator("object.paths_calculate", text="Calculate Motion Paths")
            layout.operator("object.paths_clear", text="Clear Motion Paths")
        layout.separator()
        layout.operator("nla.bake", text="Bake Action...")


# ********** Select menus, suffix from context.mode **********

class VIEW3D_MT_select_object(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("object.select_all").action = 'TOGGLE'
        layout.operator("object.select_all", text="Inverse").action = 'INVERT'
        layout.operator("object.select_random", text="Random")
        layout.operator("object.select_mirror", text="Mirror")
        layout.operator("object.select_by_layer", text="Select All by Layer")
        layout.operator_menu_enum("object.select_by_type", "type", text="Select All by Type...")
        layout.operator("object.select_camera", text="Select Camera")

        layout.separator()

        layout.operator_menu_enum("object.select_grouped", "type", text="Grouped")
        layout.operator_menu_enum("object.select_linked", "type", text="Linked")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_pose(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("pose.select_all").action = 'TOGGLE'
        layout.operator("pose.select_all", text="Inverse").action = 'INVERT'
        layout.operator("pose.select_flip_active", text="Flip Active")
        layout.operator("pose.select_constraint_target", text="Constraint Target")
        layout.operator("pose.select_linked", text="Linked")

        layout.separator()

        layout.operator("pose.select_hierarchy", text="Parent").direction = 'PARENT'
        layout.operator("pose.select_hierarchy", text="Child").direction = 'CHILD'

        layout.separator()

        props = layout.operator("pose.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("pose.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'

        layout.separator()

        layout.operator_menu_enum("pose.select_grouped", "type", text="Grouped")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_particle(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")

        layout.separator()

        layout.operator("particle.select_all").action = 'TOGGLE'
        layout.operator("particle.select_linked")
        layout.operator("particle.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("particle.select_more")
        layout.operator("particle.select_less")

        layout.separator()

        layout.operator("particle.select_roots", text="Roots")
        layout.operator("particle.select_tips", text="Tips")


class VIEW3D_MT_select_edit_mesh(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        # primitive
        layout.operator("mesh.select_all").action = 'TOGGLE'
        layout.operator("mesh.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        # numeric
        layout.operator("mesh.select_random", text="Random")
        layout.operator("mesh.select_nth")

        layout.separator()

        # geometric
        layout.operator("mesh.edges_select_sharp", text="Sharp Edges")
        layout.operator("mesh.faces_select_linked_flat", text="Linked Flat Faces")

        layout.separator()

        # topology
        layout.operator("mesh.select_loose", text="Loose Geometry")
        if context.scene.tool_settings.mesh_select_mode[2] is False:
            layout.operator("mesh.select_non_manifold", text="Non Manifold")
        layout.operator("mesh.select_interior_faces", text="Interior Faces")
        layout.operator("mesh.select_face_by_sides")

        layout.separator()

        # other ...
        layout.operator_menu_enum("mesh.select_similar", "type", text="Similar")
        layout.operator("mesh.select_ungrouped", text="Ungrouped Verts")

        layout.separator()

        layout.operator("mesh.select_less", text="Less")
        layout.operator("mesh.select_more", text="More")

        layout.separator()

        layout.operator("mesh.select_mirror", text="Mirror")
        layout.operator("mesh.select_axis", text="Side of Active")

        layout.operator("mesh.select_linked", text="Linked")
        layout.operator("mesh.shortest_path_select", text="Shortest Path")
        layout.operator("mesh.loop_multi_select", text="Edge Loop").ring = False
        layout.operator("mesh.loop_multi_select", text="Edge Ring").ring = True

        layout.separator()

        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_select_edit_curve(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_all").action = 'TOGGLE'
        layout.operator("curve.select_all", text="Inverse").action = 'INVERT'
        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")

        layout.separator()

        layout.operator("curve.de_select_first")
        layout.operator("curve.de_select_last")
        layout.operator("curve.select_next")
        layout.operator("curve.select_previous")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_surface(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("curve.select_all").action = 'TOGGLE'
        layout.operator("curve.select_all", text="Inverse").action = 'INVERT'
        layout.operator("curve.select_random")
        layout.operator("curve.select_nth")
        layout.operator("curve.select_linked", text="Select Linked")

        layout.separator()

        layout.operator("curve.select_row")

        layout.separator()

        layout.operator("curve.select_more")
        layout.operator("curve.select_less")


class VIEW3D_MT_select_edit_metaball(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("mball.select_all").action = 'TOGGLE'
        layout.operator("mball.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("mball.select_random_metaelems")


class VIEW3D_MT_select_edit_lattice(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("lattice.select_all").action = 'TOGGLE'
        layout.operator("lattice.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("lattice.select_ungrouped", text="Ungrouped Verts")


class VIEW3D_MT_select_edit_armature(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("armature.select_all").action = 'TOGGLE'
        layout.operator("armature.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("armature.select_hierarchy", text="Parent").direction = 'PARENT'
        layout.operator("armature.select_hierarchy", text="Child").direction = 'CHILD'

        layout.separator()

        props = layout.operator("armature.select_hierarchy", text="Extend Parent")
        props.extend = True
        props.direction = 'PARENT'

        props = layout.operator("armature.select_hierarchy", text="Extend Child")
        props.extend = True
        props.direction = 'CHILD'

        layout.operator_menu_enum("armature.select_similar", "type", text="Similar")
        layout.operator("object.select_pattern", text="Select Pattern...")


class VIEW3D_MT_select_paint_mask(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.face_select_all").action = 'TOGGLE'
        layout.operator("paint.face_select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("paint.face_select_linked", text="Linked")


class VIEW3D_MT_select_paint_mask_vertex(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.select_border")
        layout.operator("view3d.select_circle")

        layout.separator()

        layout.operator("paint.vert_select_all").action = 'TOGGLE'
        layout.operator("paint.vert_select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("paint.vert_select_ungrouped", text="Ungrouped Verts")


# ***********OBJECT MODE ***********

class VIEW3D_MT_menubar_obj_transform(Menu):
    bl_label = "Transform"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")
        layout.separator()
        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("transform.transform", text="Align to Transform Orientation").mode = 'ALIGN'  # XXX see alignmenu() in edit.c of b2.4x to get this working
        layout.separator()
        layout.operator("object.randomize_transform")
        layout.operator("object.align")
        layout.separator()
        layout.operator("object.anim_transforms_to_deltas")
        # Mirroring
        layout.separator()
        general_mirror(context, layout)
        # Clearing
        layout.separator()
        layout.operator("object.location_clear", text="Clear Location")
        layout.operator("object.rotation_clear", text="Clear Rotation")
        layout.operator("object.scale_clear", text="Clear Scale")
        layout.operator("object.origin_clear", text="Clear Origin")
        # Apply
        layout.separator()
        props = layout.operator("object.transform_apply", text="Apply Location", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = True, False, False
        props = layout.operator("object.transform_apply", text="Apply Rotation", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, True, False
        props = layout.operator("object.transform_apply", text="Apply Scale", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, False, True
        props = layout.operator("object.transform_apply", text="Apply Rotation & Scale", text_ctxt=i18n_contexts.default)
        props.location, props.rotation, props.scale = False, True, True
        layout.operator("object.visual_transform_apply", text="Apply Visual Transform", text_ctxt=i18n_contexts.default)
        # Texture Space
        layout.separator()
        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True
        layout.separator()
        layout.operator("object.duplicates_make_real")


class VIEW3D_MT_menubar_obj_add(Menu):
    bl_label = "Add & Delete"

    def draw(self, context):
        layout = self.layout

        # note, don't use 'EXEC_SCREEN' or operators wont get the 'v3d' context.

        # Note: was EXEC_AREA, but this context does not have the 'rv3d', which prevents
        #       "align_view" to work on first call (see [#32719]).
        layout.operator_context = 'EXEC_REGION_WIN'

        layout.menu("INFO_MT_mesh_add", icon='OUTLINER_OB_MESH')
        layout.menu("INFO_MT_curve_add", icon='OUTLINER_OB_CURVE')
        layout.menu("INFO_MT_surface_add", icon='OUTLINER_OB_SURFACE')
        layout.operator_menu_enum("object.metaball_add", "type", text="Metaball", icon='OUTLINER_OB_META')
        layout.operator("object.text_add", text="Text", icon='OUTLINER_OB_FONT')
        layout.separator()
        layout.menu("INFO_MT_armature_add", icon='OUTLINER_OB_ARMATURE')
        layout.operator("object.add", text="Lattice", icon='OUTLINER_OB_LATTICE').type = 'LATTICE'
        layout.operator_menu_enum("object.empty_add", "type", text="Empty", icon='OUTLINER_OB_EMPTY')
        layout.separator()
        layout.operator("object.speaker_add", text="Speaker", icon='OUTLINER_OB_SPEAKER')
        layout.separator()
        layout.operator("object.camera_add", text="Camera", icon='OUTLINER_OB_CAMERA')
        layout.operator_menu_enum("object.lamp_add", "type", text="Lamp", icon='OUTLINER_OB_LAMP')
        layout.separator()
        layout.operator_menu_enum("object.effector_add", "type", text="Force Field", icon='OUTLINER_OB_EMPTY')
        layout.separator()

        if len(bpy.data.groups) > 10:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("object.group_instance_add", text="Group Instance...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_menu_enum("object.group_instance_add", "group", text="Group Instance", icon='OUTLINER_OB_EMPTY')

        layout.separator()

        layout.operator("object.duplicate_move")
        layout.operator("object.duplicate_move_linked")
        layout.operator("object.join")

        layout.separator()

        layout.operator("object.delete")


class VIEW3D_MT_menubar_obj_edit(Menu):
    bl_label = "Edit"
    
    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.origin_set", text="Geometry to Origin").type = 'GEOMETRY_ORIGIN'
        layout.operator("object.origin_set", text="Origin to Geometry").type = 'ORIGIN_GEOMETRY'
        layout.operator("object.origin_set", text="Origin to 3D Cursor").type = 'ORIGIN_CURSOR'
        layout.operator("object.origin_set", text="Origin to Center of Mass").type = 'ORIGIN_CENTER_OF_MASS'
        layout.separator()
        layout.separator()
        layout.operator("object.shade_smooth")
        layout.operator("object.shade_flat")
        layout.separator()
        layout.operator_enum("object.convert", "target")
        layout.separator()
        layout.operator("object.move_to_layer", text="Move to Layer...")


class VIEW3D_MT_menubar_obj_rigidbody(Menu):
    bl_label = "Rigid Body"
    
    def draw(self, context):
        layout = self.layout

        layout.operator("rigidbody.objects_add", text="Add Active").type = 'ACTIVE'
        layout.operator("rigidbody.objects_add", text="Add Passive").type = 'PASSIVE'
        layout.operator("rigidbody.objects_remove", text="Remove")
        layout.separator()
        layout.operator("rigidbody.shape_change", text="Change Shape")
        layout.operator("rigidbody.mass_calculate", text="Calculate Mass")
        layout.operator("rigidbody.object_settings_copy", text="Copy from Active")
        layout.operator("rigidbody.bake_to_keyframes", text="Bake To Keyframes")
        layout.separator()
        layout.operator("rigidbody.connect", text="Connect")


class VIEW3D_MT_menubar_obj_game(Menu):
    bl_label = "Game"

    def draw(self, context):
        layout = self.layout
        layout.operator("object.logic_bricks_copy", text="Copy Logic Bricks")
        layout.operator("object.game_physics_copy", text="Copy Physics Properties")
        layout.separator()
        layout.operator("object.game_property_copy", text="Replace Properties").operation = 'REPLACE'
        layout.operator("object.game_property_copy", text="Merge Properties").operation = 'MERGE'
        layout.operator_menu_enum("object.game_property_copy", "property", text="Copy Properties...")
        layout.separator()
        layout.operator("object.game_property_clear")


class VIEW3D_MT_menubar_obj_group(Menu):
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout
        layout.operator("group.create")
        layout.operator("group.objects_remove")
        layout.operator("group.objects_remove_all")
        layout.separator()
        layout.operator("group.objects_add_active")
        layout.operator("group.objects_remove_active")


class VIEW3D_MT_menubar_obj_parent(Menu):
    bl_label = "Parent"

    def draw(self, context):
        layout = self.layout
        layout.operator_enum("object.parent_set", "type")
        layout.separator()
        layout.operator_enum("object.parent_clear", "type")


class VIEW3D_MT_menubar_obj_constraints(Menu):
    bl_label = "Constraints"

    def draw(self, context):
        layout = self.layout
        layout.label("Constraints")
        layout.operator("object.constraint_add_with_targets")
        layout.operator("object.constraints_copy")
        layout.operator("object.constraints_clear")
        layout.label("Tracking")
        layout.operator_enum("object.track_set", "type")
        layout.operator_enum("object.track_clear", "type")
        

class VIEW3D_MT_menubar_obj_links(Menu):
    bl_label = "Links"

    def draw(self, context):
        layout = self.layout
        layout.operator("object.proxy_make", text="Make Proxy...")
        
        layout.label("Make Local:")
        layout.operator_enum("object.make_local", "type")
        
        layout.label("Make Single User")
        props = layout.operator("object.make_single_user", text="Object")
        props.object = True
        props = layout.operator("object.make_single_user", text="Object & Data")
        props.object = props.obdata = True
        props = layout.operator("object.make_single_user", text="Object & Data & Materials+Tex")
        props.object = props.obdata = props.material = props.texture = True
        props = layout.operator("object.make_single_user", text="Materials+Tex")
        props.material = props.texture = True
        props = layout.operator("object.make_single_user", text="Object Animation")
        props.animation = True
        
        layout.label("Make Links:")
        operator_context_default = layout.operator_context
        if len(bpy.data.scenes) > 10:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("object.make_links_scene", text="Objects to Scene...", icon='OUTLINER_OB_EMPTY')
        else:
            layout.operator_context = 'EXEC_REGION_WIN'
            layout.operator_menu_enum("object.make_links_scene", "scene", text="Objects to Scene...")
        layout.operator_context = operator_context_default
        layout.operator_enum("object.make_links_data", "type")  # inline

        layout.operator("object.join_uvs")  # stupid place to add this!
        layout.operator("object.make_dupli_face")


class VIEW3D_MB_menubar_objectmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "objectmode"
    
    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_select_%s" % mode_string.lower())
        row.menu("VIEW3D_MT_menubar_obj_add")
        row.menu("VIEW3D_MT_menubar_obj_transform")
        row.menu("VIEW3D_MT_menubar_obj_edit")
        
        row.menu("VIEW3D_MT_menubar_obj_parent")
        row.menu("VIEW3D_MT_menubar_obj_group")

        row.menu("VIEW3D_MT_menubar_obj_links")
        row.menu("VIEW3D_MT_menubar_general_animation")
        row.menu("VIEW3D_MT_menubar_obj_constraints")
        row.menu("VIEW3D_MT_menubar_obj_rigidbody")
        row.menu("VIEW3D_MT_menubar_obj_game")

        row.menu("VIEW3D_MT_menubar_general_history")
        row.menu("VIEW3D_MT_menubar_general_snap")
        row.menu("VIEW3D_MT_menubar_obj_hide")        


# ***********EDIT MODE ***********

class VIEW3D_MT_menubar_mesh_transform(Menu):
    bl_label = "Transform"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")
        layout.separator()
        layout.operator("transform.shrink_fatten")
        layout.operator("transform.push_pull")
        layout.operator("transform.shear")
        layout.operator("transform.warp")
        layout.operator("transform.tosphere")
        layout.separator()
        general_mirror(context, layout)
        #layout.separator()
        #layout.operator("transform.edge_slide")
        #layout.operator("transform.vert_slide")
        layout.separator()
        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True

class VIEW3D_MT_menubar_mesh_add(Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout
        
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_plane_add", icon='MESH_PLANE', text="Plane")
        layout.operator("mesh.primitive_cube_add", icon='MESH_CUBE', text="Cube")
        layout.operator("mesh.primitive_circle_add", icon='MESH_CIRCLE', text="Circle")
        layout.operator("mesh.primitive_uv_sphere_add", icon='MESH_UVSPHERE', text="UV Sphere")
        layout.operator("mesh.primitive_ico_sphere_add", icon='MESH_ICOSPHERE', text="Icosphere")
        layout.operator("mesh.primitive_cylinder_add", icon='MESH_CYLINDER', text="Cylinder")
        layout.operator("mesh.primitive_cone_add", icon='MESH_CONE', text="Cone")
        layout.separator()
        layout.operator("mesh.primitive_grid_add", icon='MESH_GRID', text="Grid")
        layout.operator("mesh.primitive_monkey_add", icon='MESH_MONKEY', text="Monkey")
        layout.operator("mesh.primitive_torus_add", icon='MESH_TORUS', text="Torus")

        layout.separator()

        layout.operator("mesh.duplicate_move")


class VIEW3D_MT_menubar_mesh_delete(Menu):
    bl_label = "Delete"

    def draw(self, context):
        layout = self.layout
        layout.operator_enum("mesh.delete", "type")
        layout.separator()
        layout.operator("mesh.dissolve_verts")
        layout.operator("mesh.dissolve_edges")
        layout.operator("mesh.dissolve_faces")
        layout.separator()
        layout.operator("mesh.dissolve_limited")
        layout.separator()
        layout.operator("mesh.edge_collapse")
        layout.operator("mesh.delete_edgeloop")


class VIEW3D_MT_menubar_mesh_mesh(Menu):
    bl_label = "Mesh"

    _extrude_funcs = {
        'VERT': lambda layout: layout.operator("mesh.extrude_vertices_move", text="Extrude Vertices Only"),
        'EDGE': lambda layout: layout.operator("mesh.extrude_edges_move", text="Extrude Edges Only"),
        'FACE': lambda layout: layout.operator("mesh.extrude_faces_move", text="Extrude Individual Faces"),
        'REGION': lambda layout: layout.operator("view3d.edit_mesh_extrude_move_normal", text="Extrude Region"),
    }

    @staticmethod
    def extrude_options(context):
        mesh = context.object.data
        select_mode = context.tool_settings.mesh_select_mode

        menu = []
        if mesh.total_face_sel:
            menu += ['REGION', 'FACE']
        if mesh.total_edge_sel and (select_mode[0] or select_mode[1]):
            menu += ['EDGE']
        if mesh.total_vert_sel and select_mode[0]:
            menu += ['VERT']

        # should never get here
        return menu

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        
        layout.operator("view3d.edit_mesh_extrude_move_normal")
        layout.operator("view3d.edit_mesh_extrude_individual_move")
        layout.separator()
        for menu_id in self.extrude_options(context):
            self._extrude_funcs[menu_id](layout)
        layout.separator()
        props = layout.operator("mesh.knife_tool", text="Knife Cut")
        props.use_occlude_geometry = True
        props.only_selected = False
        props = layout.operator("mesh.knife_tool", text="Knife Cut (only selected)")
        props.use_occlude_geometry = False
        props.only_selected = True
        layout.operator("mesh.knife_project")
        layout.separator()
        layout.operator("mesh.loopcut_slide")
        layout.operator("mesh.subdivide")
        layout.separator()
        layout.operator("mesh.spin")
        layout.operator("mesh.screw")
        layout.separator()
        layout.operator("mesh.remove_doubles")
        layout.separator()
        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")
        layout.separator()
        layout.operator("mesh.normals_make_consistent", text="Recalculate Normals (outside)").inside = False
        layout.operator("mesh.normals_make_consistent", text="Recalculate Normals (inside)").inside = True
        layout.operator("mesh.flip_normals")
        layout.separator()
        layout.operator("mesh.symmetrize")
        layout.operator("mesh.symmetry_snap")
        layout.operator_menu_enum("mesh.sort_elements", "type", text="Sort Elements...")
        
        


class VIEW3D_MT_vertex_group(Menu):
    bl_label = "Vertex Groups"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.vertex_group_assign", text="Assign to New Group").new = True

        ob = context.active_object
        if ob.mode == 'EDIT' or (ob.mode == 'WEIGHT_PAINT' and ob.type == 'MESH' and ob.data.use_paint_mask_vertex):
            if ob.vertex_groups.active:
                layout.separator()
                layout.operator("object.vertex_group_assign", text="Assign to Active Group").new = False
                layout.operator("object.vertex_group_remove_from", text="Remove from Active Group").use_all_groups = False
                layout.operator("object.vertex_group_remove_from", text="Remove from All").use_all_groups = True
                layout.separator()

        if ob.vertex_groups.active:
            layout.operator_menu_enum("object.vertex_group_set_active", "group", text="Set Active Group")
            layout.operator("object.vertex_group_remove", text="Remove Active Group").all = False
            layout.operator("object.vertex_group_remove", text="Remove All Groups").all = True


class VIEW3D_MT_hook(Menu):
    bl_label = "Hooks"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.hook_add_newob")
        layout.operator("object.hook_add_selob").use_bone = False
        layout.operator("object.hook_add_selob", text="Hook to Selected Object Bone").use_bone = True

        if [mod.type == 'HOOK' for mod in context.active_object.modifiers]:
            layout.separator()
            layout.operator_menu_enum("object.hook_assign", "modifier")
            layout.operator_menu_enum("object.hook_remove", "modifier")
            layout.separator()
            layout.operator_menu_enum("object.hook_select", "modifier")
            layout.operator_menu_enum("object.hook_reset", "modifier")
            layout.operator_menu_enum("object.hook_recenter", "modifier")


class VIEW3D_MT_menubar_mesh_vertices(Menu):
    bl_label = "Vertices"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator_enum("mesh.merge", "type")
        layout.separator()
        layout.operator("mesh.merge", text="Merge at Center").type = 'CENTER'
        layout.operator("mesh.merge", text="Merge at Cursor").type = 'CURSOR'
        layout.operator("mesh.merge", text="Merge at First").type = 'FIRST'
        layout.operator("mesh.merge", text="Merge at Last").type = 'LAST'
        layout.operator("mesh.merge", text="Collapse").type = 'COLLAPSE'
        layout.separator()
        layout.operator("mesh.rip_move")
        layout.operator("mesh.rip_move_fill")
        layout.operator("mesh.split")
        layout.separator()
        layout.operator("mesh.separate", text="Separate by Selected").type = 'SELECTED'
        layout.operator("mesh.separate", text="Separate by Material").type = 'MATERIAL'
        layout.operator("mesh.separate", text="Separate by Loose Parts").type = 'LOOSE'
        layout.separator()
        layout.operator("mesh.vert_connect")
        layout.operator("transform.vert_slide")
        layout.separator()
        layout.operator("mesh.bevel").vertex_only = True
        layout.operator("mesh.vertices_smooth")
        layout.operator("mesh.remove_doubles")
        layout.operator("mesh.blend_from_shape")
        layout.operator("object.vertex_group_blend")
        layout.operator("mesh.shape_propagate_to_all")
        layout.separator()
        layout.menu("VIEW3D_MT_vertex_group")
        layout.menu("VIEW3D_MT_hook")


class VIEW3D_MT_menubar_mesh_edges(Menu):
    bl_label = "Edges"

    def draw(self, context):
        layout = self.layout
        with_freestyle = bpy.app.build_options.freestyle

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.edge_face_add")
        layout.separator()
        layout.operator("mesh.subdivide")
        layout.operator("mesh.unsubdivide")
        layout.separator()
        layout.operator("transform.edge_crease")
        layout.operator("transform.edge_bevelweight")
        layout.separator()
        layout.operator("mesh.mark_seam").clear = False
        layout.operator("mesh.mark_seam", text="Clear Seam").clear = True
        layout.separator()
        layout.operator("mesh.mark_sharp").clear = False
        layout.operator("mesh.mark_sharp", text="Clear Sharp").clear = True
        layout.separator()

        if with_freestyle:
            layout.operator("mesh.mark_freestyle_edge").clear = False
            layout.operator("mesh.mark_freestyle_edge", text="Clear Freestyle Edge").clear = True

        layout.separator()

        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").use_ccw = False
        layout.operator("mesh.edge_rotate", text="Rotate Edge CCW").use_ccw = True

        layout.separator()

        layout.operator("mesh.bevel").vertex_only = False
        layout.operator("mesh.edge_split")
        layout.operator("mesh.bridge_edge_loops")

        layout.separator()

        layout.operator("transform.edge_slide")
        layout.operator("mesh.loop_multi_select", text="Edge Loop").ring = False
        layout.operator("mesh.loop_multi_select", text="Edge Ring").ring = True
        layout.operator("mesh.loop_to_region")
        layout.operator("mesh.region_to_loop")


class VIEW3D_MT_menubar_mesh_faces(Menu):
    bl_label = "Faces"

    def draw(self, context):
        layout = self.layout
        with_freestyle = bpy.app.build_options.freestyle
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.edge_face_add")
        layout.separator()
        layout.operator("mesh.flip_normals")
        layout.separator()
        layout.operator("mesh.fill")
        layout.operator("mesh.fill_grid")
        layout.operator("mesh.beautify_fill")
        layout.operator("mesh.inset")
        layout.operator("mesh.bevel").vertex_only = False
        layout.operator("mesh.solidify")
        layout.operator("mesh.wireframe")
        layout.separator()
        if with_freestyle:
            layout.operator("mesh.mark_freestyle_face").clear = False
            layout.operator("mesh.mark_freestyle_face", text="Clear Freestyle Face").clear = True
        layout.separator()
        layout.operator("mesh.poke")
        layout.operator("mesh.quads_convert_to_tris")
        layout.operator("mesh.tris_convert_to_quads")
        layout.separator()
        layout.operator("mesh.faces_shade_smooth")
        layout.operator("mesh.faces_shade_flat")
        layout.separator()
        layout.operator("mesh.edge_rotate", text="Rotate Edge CW").use_ccw = False
        layout.separator()
        layout.operator("mesh.uvs_rotate")
        layout.operator("mesh.uvs_reverse")
        layout.operator("mesh.colors_rotate")
        layout.operator("mesh.colors_reverse")
        

class VIEW3D_MT_menubar_mesh_UV(Menu):
    bl_context = "mesh_edit"
    bl_label = "UV"

    def draw(self, context):
        layout = self.layout
        layout.label("UV:")
        layout.operator("uv.unwrap")
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("uv.smart_project")
        layout.operator("uv.lightmap_pack")
        layout.operator("uv.follow_active_quads")
        layout.separator()
        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("uv.cube_project")
        layout.operator("uv.cylinder_project")
        layout.operator("uv.sphere_project")
        layout.separator()
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("uv.project_from_view").scale_to_bounds = False
        layout.operator("uv.project_from_view", text="Project from View (Bounds)").scale_to_bounds = True
        layout.separator()
        layout.operator("uv.reset")


class VIEW3D_MB_menubar_editmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "mesh_edit"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_select_%s" % mode_string.lower())
        row.menu("VIEW3D_MT_menubar_mesh_add")
        row.menu("VIEW3D_MT_menubar_mesh_delete")
        row.menu("VIEW3D_MT_menubar_mesh_transform")
        row.menu("VIEW3D_MT_menubar_mesh_mesh")
        row.menu("VIEW3D_MT_menubar_mesh_vertices")
        row.menu("VIEW3D_MT_menubar_mesh_edges")
        row.menu("VIEW3D_MT_menubar_mesh_faces")
        row.menu("VIEW3D_MT_menubar_mesh_UV")
        row.menu("VIEW3D_MT_menubar_general_history")
        row.menu("VIEW3D_MT_menubar_general_snap")
        row.menu("VIEW3D_MT_menubar_mesh_hide")
                

# ***********CURVE EDIT MODE ***********

# TODO: also used for surface_edit mode
class VIEW3D_MT_menubar_curve_transform(Menu):
    bl_label = "Transform"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")
        layout.separator()
        # layout.operator("transform.shrink_fatten") # This doesn't work on curves does it?
        layout.operator("transform.push_pull")
        layout.operator("transform.shear")
        layout.operator("transform.warp")
        layout.operator("transform.tosphere")
        layout.separator()
        general_mirror(context, layout)
        layout.separator()
        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True


class VIEW3D_MT_menubar_curve_add(Menu):
    bl_label = "Add & Delete"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("curve.vertex_add", text="Add vertex")
        layout.operator("curve.make_segment")
        layout.operator("curve.duplicate_move", text="Duplicate")
        layout.operator("curve.delete", text="Delete...")


class VIEW3D_MT_menubar_curve_curve(Menu):
    bl_label = "Curve"

    def draw(self, context):
        layout = self.layout
        layout.operator("curve.spline_type_set", text="Set Type to Poly").type = 'POLY'
        layout.operator("curve.spline_type_set", text="Set Type to Bezier").type = 'BEZIER'
        layout.operator("curve.spline_type_set", text="Set Type to NURBS").type = 'NURBS'
        layout.separator()
        layout.operator("curve.cyclic_toggle")
        layout.operator("curve.switch_direction")
        layout.separator()
        layout.operator("transform.transform", text="Scale Feather").mode = 'CURVE_SHRINKFATTEN'
        layout.operator("curve.radius_set")


class VIEW3D_MT_menubar_curve_points(Menu):
    bl_label = "Control Points"

    def draw(self, context):
        layout = self.layout
        edit_object = context.edit_object

        layout.operator("curve.extrude_move", text="Extrude")
        
        if edit_object.type == 'CURVE':
            layout.separator()
            layout.operator("transform.tilt")
            layout.operator("curve.tilt_clear")
            layout.separator()
            layout.operator("curve.handle_type_set", text="Set Handle to Auto").type = 'AUTOMATIC'
            layout.operator("curve.handle_type_set", text="Set Handle to Vector").type = 'VECTOR'
            layout.operator("curve.handle_type_set", text="Set Handle to Align").type = 'ALIGNED'
            layout.operator("curve.handle_type_set", text="Set Handle to Free").type = 'FREE_ALIGN'
            # layout.operator_menu_enum("curve.handle_type_set", "type")
            layout.separator()
            layout.operator("curve.spline_weight_set")
            layout.operator("curve.radius_set")
            
        layout.separator()
        layout.operator("curve.separate")
        layout.separator()
        layout.menu("VIEW3D_MT_hook")


class VIEW3D_MT_menubar_curve_segments(Menu):
    bl_label = "Segments"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("curve.subdivide")
        layout.separator()
        layout.operator("curve.smooth")
        layout.operator("curve.smooth_radius")
    

def nurbs_draw(self, context):
    layout = self.layout

    view = context.space_data
    mode_string = context.mode
    edit_object = context.edit_object
    obj = context.active_object
    toolsettings = context.tool_settings

    row = layout.row(align=True)
    row.menu("VIEW3D_MT_select_%s" % mode_string.lower())
    row.menu("VIEW3D_MT_menubar_curve_add")
    row.menu("VIEW3D_MT_menubar_curve_transform")
    #row.menu("VIEW3D_MT_menubar_curve_edit")
    # Edit
    row.menu("VIEW3D_MT_menubar_curve_curve")
    row.menu("VIEW3D_MT_menubar_curve_points")
    row.menu("VIEW3D_MT_menubar_curve_segments")
    # General
    row.menu("VIEW3D_MT_menubar_general_history")
    row.menu("VIEW3D_MT_menubar_general_snap")
    row.menu("VIEW3D_MT_menubar_curve_hide")
    

class VIEW3D_MB_menubar_curveeditmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = 'curve_edit'
    draw = nurbs_draw


class VIEW3D_MB_menubar_surfaceeditmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "surface_edit"
    draw = nurbs_draw


# *********** METABALL ***********


class VIEW3D_MT_menubar_meta_add(Menu):
    bl_label = "Add & Delete"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("object.metaball_add", text="Add Ball").type = 'BALL'
        layout.operator("object.metaball_add", text="Add Capsule").type = 'CAPSULE'
        layout.operator("object.metaball_add", text="Add Plane").type = 'PLANE'
        layout.operator("object.metaball_add", text="Add Ellipsoid").type = 'ELLIPSOID'
        layout.operator("object.metaball_add", text="Add Cube").type = 'CUBE'
        layout.separator()
        layout.operator("mball.duplicate_metaelems")
        layout.separator()
        layout.operator("mball.delete_metaelems", text="Delete...")


class VIEW3D_MT_menubar_meta_transform(Menu):
    bl_label = "Transform"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")
        layout.separator()
        layout.operator("transform.push_pull")
        layout.operator("transform.shear")
        layout.operator("transform.warp")
        layout.operator("transform.tosphere")
        layout.separator()
        general_mirror(context, layout)
        layout.separator()
        layout.operator("transform.translate", text="Move Texture Space").texture_space = True
        layout.operator("transform.resize", text="Scale Texture Space").texture_space = True


class VIEW3D_MB_menubar_metaeditmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "mball_edit"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_select_%s" % mode_string.lower())
        row.menu("VIEW3D_MT_menubar_meta_add")
        row.menu("VIEW3D_MT_menubar_meta_transform")
        # General
        row.menu("VIEW3D_MT_menubar_general_history")
        row.menu("VIEW3D_MT_menubar_general_snap")
        row.menu("VIEW3D_MT_menubar_meta_hide")



# *********** METABALL ***********

class VIEW3D_MT_menubar_text_edit(Menu):
    bl_label = "Edit"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("font.text_copy", text="Copy")
        layout.operator("font.text_cut", text="Cut")
        layout.operator("font.text_paste", text="Paste")
        layout.separator()
        layout.operator("font.case_set", text="To Upper").case = 'UPPER'
        layout.operator("font.case_set", text="To Lower").case = 'LOWER'
        layout.separator()
        layout.operator("font.insert_lorem")
        layout.operator("font.file_paste")


class VIEW3D_MT_menubar_text_style(Menu):
    bl_label = "Style"
    
    def draw(self, context):
        layout = self.layout

        layout.operator("font.style_toggle", text="Toggle Bold").style = 'BOLD'
        layout.operator("font.style_toggle", text="Toggle Italic").style = 'ITALIC'
        layout.operator("font.style_toggle", text="Toggle Underline").style = 'UNDERLINE'


class VIEW3D_MB_menubar_texteditmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "text_edit"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_menubar_text_edit")
        row.menu("VIEW3D_MT_menubar_text_style")
        row.menu("VIEW3D_MT_edit_text_chars")


# *********** ARMATURE ***********

class VIEW3D_MT_menubar_armature_add(Menu):
    bl_label = "Add & Delete"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("armature.bone_primitive_add")
        layout.operator("armature.duplicate_move")
        layout.separator()
        layout.operator("armature.delete")
        


class VIEW3D_MT_menubar_armature_transform(Menu):
    bl_label = "Transform"
    
    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")
        layout.separator()
        layout.operator("transform.push_pull")
        layout.operator("transform.shear")
        layout.operator("transform.warp")
        layout.operator("transform.tosphere")
        layout.separator()
        general_mirror(context, layout)
        layout.separator()
        
        obj = context.object
        if (obj.type == 'ARMATURE' and obj.mode in {'EDIT', 'POSE'} and
            obj.data.draw_type in {'BBONE', 'ENVELOPE'}
            ):

            layout.operator("transform.transform", text="Scale Envelope/BBone").mode = 'BONE_SIZE'

        if context.edit_object and context.edit_object.type == 'ARMATURE':
            layout.operator("armature.align")

        layout.separator()
        layout.operator("transform.transform", text="Set Bone Roll").mode = 'BONE_ROLL'
        layout.separator()
        layout.operator("armature.calculate_roll", text="Recalculate Roll (X Axis)").type = 'X'
        layout.operator("armature.calculate_roll", text="Recalculate Roll (Y Axis)").type = 'Y'
        layout.operator("armature.calculate_roll", text="Recalculate Roll (Z Axis)").type = 'Z'
        layout.separator()
        layout.operator("armature.calculate_roll", text="Recalculate Roll (Active Bone)").type = 'ACTIVE'
        layout.operator("armature.calculate_roll", text="Recalculate Roll (View Axis)").type = 'VIEW'
        layout.operator("armature.calculate_roll", text="Recalculate Roll (Cursor)").type = 'CURSOR'


class VIEW3D_MT_menubar_armature_bones(Menu):
    bl_label = "Bones"
    
    def draw(self, context):
        layout = self.layout
        
        layout.operator("armature.extrude_move")
        layout.operator("armature.extrude_forked")
        layout.separator()
        layout.operator("armature.fill")
        layout.operator("armature.subdivide", text="Subdivide")
        layout.separator()
        layout.operator("armature.merge").type = 'WITHIN_CHAIN'
        layout.operator("armature.separate")
        layout.separator()
        layout.operator("armature.switch_direction")
        layout.separator()
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("armature.armature_layers")
        layout.operator("armature.bone_layers")
        layout.separator()
        layout.menu("VIEW3D_MT_bone_options_toggle", text="Bone Settings")


class VIEW3D_MT_menubar_armature_names(Menu):
    bl_label = "Names"
    
    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'
        layout.operator("armature.flip_names")
        layout.separator()
        layout.operator("armature.autoside_names", text="AutoName Left/Right").type = 'XAXIS'
        layout.operator("armature.autoside_names", text="AutoName Front/Back").type = 'YAXIS'
        layout.operator("armature.autoside_names", text="AutoName Top/Bottom").type = 'ZAXIS'
        
        
class VIEW3D_MT_menubar_armature_parent(Menu):
    bl_label = "Parent"

    def draw(self, context):
        layout = self.layout

        layout.operator("armature.parent_set", text="Set Parent (Connected)").type = 'CONNECTED'
        layout.operator("armature.parent_set", text="Set Parent (Keep Offset)").type = 'OFFSET'
        layout.separator()
        layout.operator("armature.parent_clear", text="Clear Parent")


class VIEW3D_MB_menubar_armatureeditmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "armature_edit"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_select_%s" % mode_string.lower())
        row.menu("VIEW3D_MT_menubar_armature_add")
        row.menu("VIEW3D_MT_menubar_armature_transform")
        row.menu("VIEW3D_MT_menubar_armature_bones")
        row.menu("VIEW3D_MT_menubar_armature_names")
        row.menu("VIEW3D_MT_menubar_armature_parent")

        row.menu("VIEW3D_MT_menubar_general_history")
        row.menu("VIEW3D_MT_menubar_general_snap")
        row.menu("VIEW3D_MT_menubar_armature_hide")


# *********** POSE MODE ***********

class VIEW3D_MT_menubar_pose_transform(Menu):
    bl_label = "Transform"
    
    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")
        layout.separator()
        layout.operator("transform.push_pull")
        layout.operator("transform.shear")
        layout.operator("transform.warp")
        layout.operator("transform.tosphere")
        layout.separator()
        general_mirror(context, layout)
        layout.separator()
        
        obj = context.objectmodect
        if (obj.type == 'ARMATURE' and obj.mode in {'EDIT', 'POSE'} and
            obj.data.draw_type in {'BBONE', 'ENVELOPE'}
            ):

            layout.operator("transform.transform", text="Scale Envelope/BBone").mode = 'BONE_SIZE'

        if context.edit_object and context.edit_object.type == 'ARMATURE':
            layout.operator("armature.align")

        layout.separator()
        layout.operator("pose.loc_clear", text="Clear Location")
        layout.operator("pose.rot_clear", text="Clear Rotation")
        layout.operator("pose.scale_clear", text="Clear Scale")
        layout.operator("pose.transforms_clear", text="Clear All")
        layout.operator("pose.user_transforms_clear", text="Reset Unkeyed")
        layout.separator()
        layout.operator("pose.armature_apply")
        layout.operator("pose.visual_transform_apply")
        layout.separator()
        layout.operator("pose.quaternions_flip")        


class VIEW3D_MT_menubar_pose_pose(Menu):
    bl_label = "Pose"
    
    def draw(self, context):
        layout = self.layout
        
        layout.operator("pose.copy")
        layout.operator("pose.paste")
        layout.operator("pose.paste", text="Paste X-Flipped Pose").flipped = True
        layout.separator()
        layout.operator("pose.push")
        layout.operator("pose.relax")
        layout.operator("pose.breakdown")
        layout.separator()
        layout.operator("pose.propagate").mode = 'WHILE_HELD'
        layout.operator("pose.propagate", text="Propagate to Next Keyframe").mode = 'NEXT_KEY'
        layout.operator("pose.propagate", text="Propagate to Last Keyframe (Make Cyclic)").mode = 'LAST_KEY'
        layout.operator("pose.propagate", text="Propagate to Selected Markers").mode = 'SELECTED_MARKERS'
        layout.separator()
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("pose.armature_layers", text="Change Armature Layers...")
        layout.operator("pose.bone_layers", text="Change Bone Layers...")
        layout.separator()
        layout.menu("VIEW3D_MT_bone_options_toggle", text="Bone Settings")


class VIEW3D_MT_menubar_pose_library(Menu):
    bl_label = "Library"
    
    def draw(self, context):
        layout = self.layout

        layout.operator("poselib.browse_interactive", text="Browse Poses...")
        layout.separator()
        layout.operator("poselib.pose_add", text="Add Pose...")
        layout.separator()
        layout.operator("poselib.pose_rename", text="Rename Pose...")
        layout.operator("poselib.pose_remove", text="Remove Pose...")


class VIEW3D_MT_menubar_pose_constraints(Menu):
    bl_label = "Constraints"
    
    def draw(self, context):
        layout = self.layout

        layout.operator("pose.ik_add")
        layout.operator("pose.ik_clear")
        layout.separator()
        layout.operator("pose.constraint_add_with_targets", text="Add (With Targets)...")
        layout.operator("pose.constraints_copy")
        layout.operator("pose.constraints_clear")


class VIEW3D_MB_menubar_poseeditmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "posemode"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_select_%s" % mode_string.lower())
        row.menu("VIEW3D_MT_menubar_pose_transform")
        row.menu("VIEW3D_MT_menubar_pose_pose")
        row.menu("VIEW3D_MT_menubar_pose_library")
        row.menu("VIEW3D_MT_menubar_general_animation")
        row.menu("VIEW3D_MT_menubar_armature_names")
        row.menu("VIEW3D_MT_object_parent")
        row.menu("VIEW3D_MT_pose_group")
        row.menu("VIEW3D_MT_menubar_pose_constraints")
        row.menu("VIEW3D_MT_menubar_general_history")
        row.menu("VIEW3D_MT_menubar_general_snap")
        row.menu("VIEW3D_MT_menubar_pose_hide")


# *********** LATTICE ***********

class VIEW3D_MT_menubar_lattice_transform(Menu):
    bl_label = "Transform"
    
    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")
        layout.separator()
        layout.operator("transform.push_pull")
        layout.operator("transform.shear")
        layout.operator("transform.warp")
        layout.operator("transform.tosphere")
        layout.separator()
        general_mirror(context, layout)


class VIEW3D_MT_menubar_lattice_lattice(Menu):
    bl_label = "Lattice"
    
    def draw(self, context):
        layout = self.layout
        layout.operator("lattice.make_regular")
        layout.separator()
        layout.operator("lattice.flip", text="Flip U (X) Axis").axis = 'U'
        layout.operator("lattice.flip", text="Flip V (Y) Axis").axis = 'V'
        layout.operator("lattice.flip", text="Flip W (Z) Axis").axis = 'W'
        

class VIEW3D_MB_menubar_latticeeditmode(MenuBar):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'MENU_BAR'
    bl_context = "lattice_edit"

    def draw(self, context):
        layout = self.layout

        view = context.space_data
        mode_string = context.mode
        edit_object = context.edit_object
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_select_%s" % mode_string.lower())
        row.menu("VIEW3D_MT_menubar_lattice_transform")
        row.menu("VIEW3D_MT_menubar_lattice_lattice")
        row.menu("VIEW3D_MT_menubar_general_history")
        row.menu("VIEW3D_MT_menubar_general_snap")


# *********** REGISTER ***********

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
