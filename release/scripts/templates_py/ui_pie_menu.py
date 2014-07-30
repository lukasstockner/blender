import bpy
from bpy.props import *
from bpy.types import Operator
from bpy.types import Menu

#expand an operator's macro in a pie menu

myitems = (('0','Hey Lady!', ''),('1','Spaaaaaaaceeeee!',''),('2','Wanna be awesome in space?',''), ('3','The fact sphere is always useful',''))


class TestPieOperator(bpy.types.Operator):
    """Tooltip"""
    bl_idname = "wm.test_pie_operator"
    bl_label = "Simple Pie Sticky Operator"

    test_type = EnumProperty(name='test_type', items = myitems, default='3')

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        print("The sphere core says: " + myitems[int(self.test_type)][1])
        return {'FINISHED'}


class VIEW3D_PIE_template(Menu):
    bl_label = "Test Pie"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator_enum("WM_OT_test_pie_operator", "test_type")


def register():
    bpy.utils.register_class(TestPieOperator)
    bpy.utils.register_class(VIEW3D_PIE_template)
 

def unregister():
    bpy.utils.unregister_class(TestPieOperator)
    bpy.utils.unregister_class(VIEW3D_PIE_template)


if __name__ == "__main__":
    register()
    
    bpy.ops.wm.call_pie_menu(name="VIEW3D_PIE_template")


