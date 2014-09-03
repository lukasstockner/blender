#
# Copyright 2011-2014 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License
#

# <pep8 compliant>

import bpy
import nodeitems_utils
from bpy.types import NodeTree, Node, NodeSocket
from bpy.props import EnumProperty, FloatProperty
from nodeitems_utils import NodeCategory, NodeItem


class CameraRaysTree(NodeTree):
    '''Camera rays node tree'''
    bl_idname = 'CameraRaysTreeType'
    bl_label = 'Camera Rays'
    bl_icon = 'CAMERA_DATA'


class CameraRaysTreeNode:
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'CameraRaysTreeType'


class PathAttributeNode(Node, CameraRaysTreeNode):
    '''Path attribute input node'''
    bl_idname = 'PathAttributeNodeType'
    bl_label = 'Attribute'

    def init(self, context):
        self.outputs.new('NodeSocketVector', "Raster")
        self.outputs.new('NodeSocketVector', "Lens")
        self.outputs.new('NodeSocketFloat', "Time")

    def draw_buttons(self, context, layout):
        pass


class CameraSamplePerspectiveNode(Node, CameraRaysTreeNode):
    '''Sample perspective camera ray'''
    bl_idname = 'CameraSamplePerspectiveNodeType'
    bl_label = 'Sample Perspective'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Raster")
        self.inputs.new('NodeSocketVector', "Lens")
        self.inputs.new('NodeSocketFloat', "Time")
        self.outputs.new('NodeSocketVector', "Ray Origin")
        self.outputs.new('NodeSocketVector', "Ray Direction")
        self.outputs.new('NodeSocketFloat', "Ray Length")

    def draw_buttons(self, context, layout):
        pass


class CameraRayOutputNode(Node, CameraRaysTreeNode):
    '''Camera ray output node'''
    bl_idname = 'CameraRayOutputNodeType'
    bl_label = 'Ray Output'

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Ray Origin")
        self.inputs.new('NodeSocketVector', "Ray Direction")
        self.inputs.new('NodeSocketFloat', "Ray Length")
        self.inputs.new('NodeSocketFloat', "Time")

    def draw_buttons(self, context, layout):
        pass


class PolynomialDistortionNode(Node, CameraRaysTreeNode):
    '''Ray distortion node type'''
    bl_idname = 'PolynomialDistortionNodeType'
    bl_label = 'Polynomial Distortion'

    mode = EnumProperty(name="Mode",
        description="Mode of the distortion",
        items=(('APPLY', 'Apply',
                "Apply the radial distortion on the input"),
               ('INVERT', "Invert",
                "Invert the radial distortion from the input")),
        default='INVERT')

    k1 = FloatProperty(name="K1",
                       description="First coefficient of third "
                                   "order polynomial radial distortion",
                       default=0.0)

    k2 = FloatProperty(name="K2",
                       description="Second coefficient of third "
                                   "order polynomial radial distortion",
                       default=0.0)

    k3 = FloatProperty(name="K3",
                       description="Third coefficient of third "
                                   "order polynomial radial distortion",
                       default=0.0)

    def init(self, context):
        self.inputs.new('NodeSocketVector', "Raster")
        self.outputs.new('NodeSocketVector', "Raster")

    def draw_buttons(self, context, layout):
        col = layout.column()
        col.prop(self, "mode", text="")

        col = layout.column(align=True)
        col.prop(self, "k1")
        col.prop(self, "k2")
        col.prop(self, "k3")


class CameraRaysNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'CameraRaysTreeType'

node_categories = [
    CameraRaysNodeCategory("INPUT", "Input", items=[
        NodeItem("PathAttributeNodeType"),
        ]),
    CameraRaysNodeCategory("OUTPUT", "Output", items=[
        NodeItem("CameraRayOutputNodeType"),
        ]),
    CameraRaysNodeCategory("SAMPLE", "Sample", items=[
        NodeItem("CameraSamplePerspectiveNodeType"),
        ]),
    CameraRaysNodeCategory("DISTORTION", "Distortion", items=[
        NodeItem("PolynomialDistortionNodeType"),
        ]),
    ]


def register():
    bpy.utils.register_class(CameraRaysTree)
    bpy.utils.register_class(PathAttributeNode)
    bpy.utils.register_class(CameraSamplePerspectiveNode)
    bpy.utils.register_class(CameraRayOutputNode)
    bpy.utils.register_class(PolynomialDistortionNode)

    nodeitems_utils.register_node_categories("CAMERA_NODES", node_categories)


def unregister():
    nodeitems_utils.unregister_node_categories("CAMERA_NODES")

    bpy.utils.unregister_class(CameraRaysTree)
    bpy.utils.unregister_class(PathAttributeNode)
    bpy.utils.unregister_class(CameraSamplePerspectiveNode)
    bpy.utils.unregister_class(CameraRayOutputNode)
    bpy.utils.unregister_class(PolynomialDistortionNode)
