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

__all__ = (
    "bake_action",
    )

import bpy


def frange(start, stop, step=1.0):
    while start < stop:
        yield start
        start += step


def ref_curve_eval(curve, frame_start, frame_stop, frame_step, x):
    fac = (x - frame_start) / frame_step
    idx = int(fac)
    fac = abs(fac - idx)
    if idx < 0:
        return curve[0]
    elif idx + 1 >= len(curve):
        return curve[-1]
    return (1.0 - fac) * curve[idx] + fac * curve[idx + 1]


def bezt_optimize(points, threshold, res, steps, org_ref_curve, frame_start, frame_stop, frame_step):
    """
    Try to optimize given pair of Bezier segments (triplet of contiguous control points).
    """
    # Trying to remove the center point and adjusting relevant handles of each end points.
    # If resulting curve gives error below threshold (i.e. average difference between y-values of original
    # and simplified curve is small enough), we keep it (i.e. remove its center point).

    from mathutils.geometry import interpolate_bezier
    from math import sqrt

    def correct_bezpart(points):
        # Same as in C code...
        h1 = points[0] - points[1]
        h2 = points[3] - points[2]
        d = points[3].x - points[0].x
        d1 = abs(h1.x)
        d2 = abs(h2.x)

        if d != 0.0 and d1 + d2 > d:
            fac = d / (d1 + d2)
            points[1] = points[0] - h1 * fac
            points[2] = points[3] - h2 * fac

    def bez_diff(ref_curve, cur_curve, res):
        # start and end values shall be the same!
        start_diff = end_diff = 0
        for i, (ref_v, cur_pt) in enumerate(zip(ref_curve[1:-1], cur_curve[1:-1])):
            # Note we give much higher importance (quadratic rate) to difference near matching end.
            start_fac = (i + 1) / res
            end_fac = 1.0 - start_fac
            start_diff += (cur_pt.y - ref_v) / (start_fac * start_fac)
            end_diff += (cur_pt.y - ref_v) / (end_fac * end_fac)
        return start_diff / (res - 2), end_diff / (res - 2)

    correct_bezpart(points)

    start_vec = points[1] - points[0]
    end_vec = points[2] - points[3]

    neg_slope = points[1].y < points[0].y if points[1].y != points[0].y else points[2].y < points[0].y if points[2].y != points[0].y else points[3].y < points[0].y

    cur_curve = interpolate_bezier(points[0], points[1], points[2], points[3], res)
    ref_curve = [ref_curve_eval(org_ref_curve, frame_start, frame_stop, frame_step, pt.x) for pt in cur_curve]

    start_diff, end_diff = bez_diff(ref_curve, cur_curve, res)
    prev_start_diff, prev_end_diff = start_diff, end_diff
    do_start = 0
    #~ print(points)
    #~ print(start_diff, end_diff)

    f = 1.0
    for i in range(steps):
        error = max(abs(start_diff), abs(end_diff))
        if error < threshold:
            return error

        prev_points = list(points)
        prev_start_vec, prev_end_vec = start_vec.copy(), end_vec.copy()

        if do_start > 0 or (do_start == 0 and abs(start_diff) > abs(end_diff)):
            do_start += 1
            if neg_slope:
                if start_diff > 0.0:
                    start_vec /= 1 + start_diff * f
                else:
                    start_vec *= 1 - start_diff * f
            else:
                if start_diff < 0.0:
                    start_vec /= 1 - start_diff * f
                else:
                    start_vec *= 1 + start_diff * f
            points[1] = points[0] + start_vec
        else:
            do_start -= 1
            if neg_slope:
                if end_diff > 0.0:
                    end_vec *= 1 + end_diff * f
                else:
                    end_vec /= 1 - end_diff * f
            else:
                if end_diff < 0.0:
                    end_vec *= 1 - end_diff * f
                else:
                    end_vec /= 1 + end_diff * f
            points[2] = points[3] + end_vec

        correct_bezpart(points)
        cur_curve = interpolate_bezier(points[0], points[1], points[2], points[3], res)
        ref_curve = [ref_curve_eval(org_ref_curve, frame_start, frame_stop, frame_step, pt.x) for pt in cur_curve]

        start_diff, end_diff = bez_diff(ref_curve, cur_curve, res)
        #~ print(points)
        #~ print(start_diff, end_diff, f, do_start, neg_slope)

        if ((do_start > 0 and abs(start_diff) > abs(prev_start_diff)) or
            (do_start < 0 and abs(end_diff) > abs(prev_end_diff))):
            #~ print("WRONG!!!", (start_diff, prev_start_diff) if do_start > 0 else (end_diff, prev_end_diff))
            points[:] = prev_points
            start_diff, end_diff = prev_start_diff, prev_end_diff
            start_vec, end_vec = prev_start_vec, prev_end_vec
            do_start *= -1
            if not (do_start % 2):
                f /= 2
        else:
            do_start = 0
            prev_start_diff, prev_end_diff = start_diff, end_diff

    return max(abs(start_diff), abs(end_diff))


def simplify_fcurve(fcurve, frame_start, frame_stop, threshold):
    """
    This function simplifies given fcurve, removing some existing control points and adjusting the others' handles.
    Note that it does not remove non-aligned (or auto) points, nor any using something else than Bezier interpolation.

    :arg frame_start: First frame to simplify.
    :type frame_start: int
    :arg frame_stop: Last frame to simplify (excluded).
    :type frame_stop: int
    :arg threshold: Precision of simplification
       (the smaller the more precise, never zero, typically 0.1 gives best results).
    :type threshold: float

    :return: The number of deleted keyframes.
    :rtype: int
    """

    # * We make several passes on the curve, removing each time at most (n - 1) / 2 of its control points.
    # * End points are never removed.
    # * Points which do not have aligned handles are never removed, neither are points using non-Bezier interpolation.
    # * Each set of contiguous, aligned/auto points define a single curve segment.
    # * At each pass, for each segment, we check a set of triplets, and try to optimize it.

    SIMPLIFIED_TYPES_AUTO = {'AUTO', 'AUTO_CLAMPED'}
    SIMPLIFIED_TYPES = {'ALIGNED'} | SIMPLIFIED_TYPES_AUTO
    SIMPLIFIED_INTERPOLATION = {'BEZIER'}

    frame_step = max(0.001, threshold / 10.0)
    res = min(1000, int(1 / threshold * 10))
    steps = min(100, int(1 / threshold * 5))

    ref_curve = [fcurve.evaluate(x) for x in frange(frame_start, frame_stop, frame_step)]

    curves = [[[], False]]
    for pt in fcurve.keyframe_points:
        if pt.co.x < frame_start:
            continue
        if pt.co.x >= frame_stop:
            break
        if pt.interpolation not in SIMPLIFIED_INTERPOLATION:
            # 'Break' point.
            if len(curves[-1][0]) > 2:
                curves.append([[], False])
            else:  # Current curve segment is too short to be simplifiable, simply ignore it!
                curves[-1][0][:] = []
            #~ print("breaking")
            continue
        if pt.handle_left_type not in SIMPLIFIED_TYPES or pt.handle_right_type not in SIMPLIFIED_TYPES:
            # 'Break' point.
            if len(curves[-1][0]) > 1:
                curves[-1][0].append([[pt.handle_left, pt.co, pt.handle_right], False, pt])
                curves.append([[], False])
            else:  # Current curve segment is too short to be simplifiable, simply ignore it!
                curves[-1][0][:] = []
            #~ print("breaking")
        curves[-1][0].append([[pt.handle_left, pt.co, pt.handle_right], False, pt])

    if not curves[-1][0]:
        del curves[-1]  # Cleanup.

    if not curves:
        return 0

    del_keyframes = []
    step_simplified = True
    while step_simplified:
        step_simplified = False
        for crv in curves:
            if crv[1]:
                continue  # that whole segment of curve is considered impossible to simplify further.

            curve = crv[0]
            curve_len = len(curve)
            new_curve1 = curve[0:1]
            del_keyframes1 = []
            simplified1 = 0
            tot_error1 = 0.0
            if curve_len <= 2:
                continue

            for i in range(0, curve_len - 2, 2):
                if curve[i + 1][1]:
                    # Center knot of this triplet is locked (marked as not removable), skip.
                    new_curve1 += curve[i + 1:i + 3]
                points = [curve[i][0][1].copy(), curve[i][0][2].copy(), curve[i + 2][0][0].copy(), curve[i + 2][0][1].copy()]
                error = bezt_optimize(points, threshold, res, steps, ref_curve, frame_start, frame_stop, frame_step)
                #~ print(error)
                if (error < threshold):
                    del_keyframes1.append(curve[i + 1][2])
                    tot_error1 += error
                    # Center points of knots do not change - ever!
                    new_curve1[-1][0][2] = points[1]
                    new_curve1.append(curve[i + 2])
                    new_curve1[-1][0][0] = points[2]
                    simplified1 += 1
                else:
                    new_curve1 += curve[i + 1:i + 3]  # Mere copy of org curve...
                    #~ new_curve1[-2][1] = True  # Lock that center knot from now on.
            step_simplified = step_simplified or (simplified1 > 0)

            if curve_len > 3:
                # If we have four or more control points, we also have to check the other possible set of triplets...
                new_curve2 = curve[0:1]
                del_keyframes2 = []
                simplified2 = 0
                tot_error2 = 0.0

                for i in range(1, curve_len - 2, 2):
                    if curve[i + 1][1]:
                        # Center knot of this triplet is locked (marked as not removable), skip.
                        new_curve2 += curve[i + 1:i + 3]
                    points = [curve[i][0][1].copy(), curve[i][0][2].copy(), curve[i + 2][0][0].copy(), curve[i + 2][0][1].copy()]
                    error = bezt_optimize(points, threshold, res, steps, ref_curve, frame_start, frame_stop, frame_step)
                    #~ print(error)
                    if (error < threshold):
                        del_keyframes2.append(curve[i + 1][2])
                        tot_error2 += error
                        # Center points of knots do not change - ever!
                        new_curve2[-1][0][2] = points[1]
                        new_curve2.append(curve[i + 2])
                        new_curve2[-1][0][0] = points[2]
                        simplified2 += 1
                    else:
                        new_curve2 += curve[i + 1:i + 3]  # Mere copy of org curve...
                        #~ new_curve2[-2][1] = True  # Lock that center knot from now on.

                if (simplified2 > simplified1) or (simplified2 and ((tot_error2 < tot_error1) or not simplified1)):
                    new_curve1 = new_curve2
                    del_keyframes1 = del_keyframes2
                    step_simplified = step_simplified or (simplified2 > 0)

            if (len(new_curve1) < curve_len):
                curve[:] = new_curve1
                del_keyframes += del_keyframes1
            else:
                crv[1] = True  # That segment of curve cannot be simplified further.

    ret = len(del_keyframes)
    if not del_keyframes:
        return ret

    # Now! Update our fcurve.

    # 'Flatten' our curve segments into a single curve again.
    curve = []
    for c, _ in curves:
        if len(c) >= 2:
            if curve and curve[-1][2] == c[0][2]:
                curve[-1][0][2] = c[0][2]
                curve += c[1:]
            else:
                curve += c

    # Update handles of kept, modified keyframes.
    for bezt, _, pt in c:
        # Tag 'auto' handles as 'aligned'.
        if pt.handle_left_type in SIMPLIFIED_TYPES_AUTO:
            pt.handle_left_type = 'ALIGNED'
        if pt.handle_right_type in SIMPLIFIED_TYPES_AUTO:
            pt.handle_right_type = 'ALIGNED'
        pt.handle_left, pt.co, pt.handle_right = bezt

    # Remove deleted keyframes - WARNING must be the last thing done! Otherwise, other points become invalid...
    for pt in sorted(del_keyframes, key=lambda pt: pt.co.x, reverse=True):
        fcurve.keyframe_points.remove(pt, fast=True)

    fcurve.update()
    return ret


# XXX visual keying is actually always considered as True in this code...
def bake_action(frame_start,
                frame_end,
                frame_step=1,
                only_selected=False,
                do_pose=True,
                do_object=True,
                do_visual_keying=True,
                do_constraint_clear=False,
                do_parents_clear=False,
                do_clean=False,
                action=None,
                clean_threshold=0.0,
                ):

    """
    Return an image from the file path with options to search multiple paths
    and return a placeholder if its not found.

    :arg frame_start: First frame to bake.
    :type frame_start: int
    :arg frame_end: Last frame to bake.
    :type frame_end: int
    :arg frame_step: Frame step.
    :type frame_step: int
    :arg only_selected: Only bake selected data.
    :type only_selected: bool
    :arg do_pose: Bake pose channels.
    :type do_pose: bool
    :arg do_object: Bake objects.
    :type do_object: bool
    :arg do_visual_keying: Use the final transformations for baking ('visual keying')
    :type do_visual_keying: bool
    :arg do_constraint_clear: Remove constraints after baking.
    :type do_constraint_clear: bool
    :arg do_parents_clear: Unparent after baking objects.
    :type do_parents_clear: bool
    :arg do_clean: Remove redundant keyframes after baking.
    :type do_clean: bool
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` or None
    :arg clean_threshold: How much approximation do we accept while simplifying fcurves.
    :type clean_threshold: float

    :return: an action or None
    :rtype: :class:`bpy.types.Action`
    """

    # -------------------------------------------------------------------------
    # Helper Functions and vars

    def pose_frame_info(obj):
        matrix = {}
        for name, pbone in obj.pose.bones.items():
            if do_visual_keying:
                # Get the final transform of the bone in its own local space...
                matrix[name] = obj.convert_space(pbone, pbone.matrix, 'POSE', 'LOCAL')
            else:
                matrix[name] = pbone.matrix_basis.copy()
        return matrix

    if do_parents_clear:
        if do_visual_keying:
            def obj_frame_info(obj):
                return obj.matrix_world.copy()
        else:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_basis
                if parent:
                    return parent.matrix_world * matrix
                else:
                    return matrix.copy()
    else:
        if do_visual_keying:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_world
                if parent:
                    return parent.matrix_world.inverted_safe() * matrix
                else:
                    return matrix.copy()
        else:
            def obj_frame_info(obj):
                return obj.matrix_basis.copy()

    # -------------------------------------------------------------------------
    # Setup the Context

    # TODO, pass data rather then grabbing from the context!
    scene = bpy.context.scene
    obj = bpy.context.object
    frame_back = scene.frame_current

    if obj.pose is None:
        do_pose = False

    if not (do_pose or do_object):
        return None

    pose_info = []
    obj_info = []

    options = {'INSERTKEY_NEEDED'}

    frame_range = range(frame_start, frame_end + 1, frame_step)

    # -------------------------------------------------------------------------
    # Collect transformations

    for f in frame_range:
        scene.frame_set(f)
        scene.update()
        if do_pose:
            pose_info.append(pose_frame_info(obj))
        if do_object:
            obj_info.append(obj_frame_info(obj))

    # -------------------------------------------------------------------------
    # Create action

    # in case animation data hasn't been created
    atd = obj.animation_data_create()
    if action is None:
        action = bpy.data.actions.new("Action")
    atd.action = action

    # -------------------------------------------------------------------------
    # Apply transformations to action

    # pose
    if do_pose:
        for name, pbone in obj.pose.bones.items():
            if only_selected and not pbone.bone.select:
                continue

            if do_constraint_clear:
                while pbone.constraints:
                    pbone.constraints.remove(pbone.constraints[0])

            # create compatible eulers
            euler_prev = None

            for (f, matrix) in zip(frame_range, pose_info):
                pbone.matrix_basis = matrix[name].copy()

                pbone.keyframe_insert("location", -1, f, name, options)

                rotation_mode = pbone.rotation_mode
                if rotation_mode == 'QUATERNION':
                    pbone.keyframe_insert("rotation_quaternion", -1, f, name, options)
                elif rotation_mode == 'AXIS_ANGLE':
                    pbone.keyframe_insert("rotation_axis_angle", -1, f, name, options)
                else:  # euler, XYZ, ZXY etc
                    if euler_prev is not None:
                        euler = pbone.rotation_euler.copy()
                        euler.make_compatible(euler_prev)
                        pbone.rotation_euler = euler
                        euler_prev = euler
                        del euler
                    else:
                        euler_prev = pbone.rotation_euler.copy()
                    pbone.keyframe_insert("rotation_euler", -1, f, name, options)

                pbone.keyframe_insert("scale", -1, f, name, options)

    # object. TODO. multiple objects
    if do_object:
        if do_constraint_clear:
            while obj.constraints:
                obj.constraints.remove(obj.constraints[0])

        # create compatible eulers
        euler_prev = None

        for (f, matrix) in zip(frame_range, obj_info):
            name = "Action Bake"  # XXX: placeholder
            obj.matrix_basis = matrix

            obj.keyframe_insert("location", -1, f, name, options)

            rotation_mode = obj.rotation_mode
            if rotation_mode == 'QUATERNION':
                obj.keyframe_insert("rotation_quaternion", -1, f, name, options)
            elif rotation_mode == 'AXIS_ANGLE':
                obj.keyframe_insert("rotation_axis_angle", -1, f, name, options)
            else:  # euler, XYZ, ZXY etc
                if euler_prev is not None:
                    euler = obj.rotation_euler.copy()
                    euler.make_compatible(euler_prev)
                    obj.rotation_euler = euler
                    euler_prev = euler
                    del euler
                else:
                    euler_prev = obj.rotation_euler.copy()
                obj.keyframe_insert("rotation_euler", -1, f, name, options)

            obj.keyframe_insert("scale", -1, f, name, options)

        if do_parents_clear:
            obj.parent = None

    # -------------------------------------------------------------------------
    # Clean

    if do_clean:
        for fcu in action.fcurves:
            keyframe_points = fcu.keyframe_points
            i = 1
            while i < len(fcu.keyframe_points) - 1:
                val_prev = keyframe_points[i - 1].co[1]
                val_next = keyframe_points[i + 1].co[1]
                val = keyframe_points[i].co[1]

                if abs(val - val_prev) + abs(val - val_next) < 0.0001:
                    keyframe_points.remove(keyframe_points[i])
                else:
                    i += 1
            if clean_threshold != 0.0:
                simplify_fcurve(fcu, keyframe_points[0].co.x, keyframe_points[-1].co.x + 1, clean_threshold)

    scene.frame_set(frame_back)

    return action
