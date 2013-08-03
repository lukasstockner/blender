/*
 * Copyright 2012, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 *		Sergey Sharybin
 */

#include "COM_PlaneTrackDeformNode.h"
#include "COM_ExecutionSystem.h"

#include "COM_PlaneTrackMaskOperation.h"
#include "COM_PlaneTrackWarpImageOperation.h"
#include "COM_PlaneTrackWarpMaskOperation.h"

#include "COM_DownsampleOperation.h"

extern "C" {
	#include "BKE_node.h"
	#include "BKE_movieclip.h"
	#include "BKE_tracking.h"
}

static int getLengthInPixels(float v1[2], float v2[2], int width, int height)
{
	float dx = fabsf(v2[0] - v1[0]) * width;
	float dy = fabsf(v2[1] - v1[1]) * height;

	return sqrtf(dx * dx + dy * dy);
}

PlaneTrackDeformNode::PlaneTrackDeformNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

bool PlaneTrackDeformNode::getDownsampleDimensions(MovieClip *movieClip, NodePlaneTrackDeformData *data, int frame_number,
                                                   float *downsample_width_r, float *downsample_height_r)
{
	bool do_downsample = false;

	*downsample_width_r = 0.0f;
	*downsample_height_r = 0.0f;

	if (movieClip) {
		MovieTracking *tracking = &movieClip->tracking;
		MovieTrackingObject *object = BKE_tracking_object_get_named(tracking, data->tracking_object);
		if (object) {
			MovieTrackingPlaneTrack *plane_track =
				BKE_tracking_plane_track_get_named(tracking, object, data->plane_track_name);

			if (plane_track) {
				MovieClipUser user = {0};
				int width, height;
				int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(movieClip, frame_number);
				MovieTrackingPlaneMarker *plane_marker =
					BKE_tracking_plane_marker_get(plane_track, clip_framenr);

				BKE_movieclip_user_set_frame(&user, frame_number);
				BKE_movieclip_get_size(movieClip, &user, &width, &height);

				*downsample_width_r = max_ff(getLengthInPixels(plane_marker->corners[0], plane_marker->corners[1], width, height),
				                             getLengthInPixels(plane_marker->corners[2], plane_marker->corners[3], width, height));
				*downsample_height_r = max_ff(getLengthInPixels(plane_marker->corners[1], plane_marker->corners[2], width, height),
				                              getLengthInPixels(plane_marker->corners[0], plane_marker->corners[3], width, height));

				/* Only do downsample if both dimensions are smaller */
				do_downsample = *downsample_width_r < width && *downsample_height_r < height;
			}
		}
	}

	return do_downsample;
}

void PlaneTrackDeformNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *input_image = this->getInputSocket(0);
	InputSocket *input_mask = this->getInputSocket(1);

	OutputSocket *output_warped_image = this->getOutputSocket(0);
	OutputSocket *output_warped_mask = this->getOutputSocket(1);
	OutputSocket *output_plane = this->getOutputSocket(2);

	bNode *editorNode = this->getbNode();
	MovieClip *clip = (MovieClip *) editorNode->id;

	NodePlaneTrackDeformData *data = (NodePlaneTrackDeformData *) editorNode->storage;

	int frame_number = context->getFramenumber();

	if (output_warped_image->isConnected()) {
		PlaneTrackWarpImageOperation *warp_image_operation = new PlaneTrackWarpImageOperation();

		warp_image_operation->setMovieClip(clip);
		warp_image_operation->setTrackingObject(data->tracking_object);
		warp_image_operation->setPlaneTrackName(data->plane_track_name);
		warp_image_operation->setFramenumber(frame_number);

		float downsample_width, downsample_height;
		if (getDownsampleDimensions(clip, data, frame_number, &downsample_width, &downsample_height)) {
			DownsampleOperation *downsample_operation = new DownsampleOperation();
			downsample_operation->setNewWidth(downsample_width);
			downsample_operation->setNewHeight(downsample_height);
			downsample_operation->setKeepAspect(true);

			input_image->relinkConnections(downsample_operation->getInputSocket(0), 0, graph);
			addLink(graph, downsample_operation->getOutputSocket(), warp_image_operation->getInputSocket(0));
			graph->addOperation(downsample_operation);
		}
		else {
			input_image->relinkConnections(warp_image_operation->getInputSocket(0), 0, graph);
		}

		output_warped_image->relinkConnections(warp_image_operation->getOutputSocket());

		graph->addOperation(warp_image_operation);
	}

	if (output_warped_mask->isConnected()) {
		PlaneTrackWarpMaskOperation *warp_mask_operation = new PlaneTrackWarpMaskOperation();

		warp_mask_operation->setMovieClip(clip);
		warp_mask_operation->setTrackingObject(data->tracking_object);
		warp_mask_operation->setPlaneTrackName(data->plane_track_name);
		warp_mask_operation->setFramenumber(frame_number);

		input_mask->relinkConnections(warp_mask_operation->getInputSocket(0), 1, graph);
		output_warped_mask->relinkConnections(warp_mask_operation->getOutputSocket());
		graph->addOperation(warp_mask_operation);
	}

	if (output_plane->isConnected()) {
		PlaneTrackMaskOperation *plane_mask_operation = new PlaneTrackMaskOperation();

		plane_mask_operation->setMovieClip(clip);
		plane_mask_operation->setTrackingObject(data->tracking_object);
		plane_mask_operation->setPlaneTrackName(data->plane_track_name);
		plane_mask_operation->setFramenumber(frame_number);

		output_plane->relinkConnections(plane_mask_operation->getOutputSocket());

		graph->addOperation(plane_mask_operation);
	}
}
