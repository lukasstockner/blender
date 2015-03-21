
#include "GPUx_state.h"
#include "GPUx_vbo.h"

#include <string.h> /* memset */

#if TRUST_NO_ONE
  #include <assert.h>
#endif /* TRUST_NO_ONE */

static const CommonDrawState default_common = { false, true, true, false };
static const PointDrawState default_point = { false, 1.0f };
static const LineDrawState default_line = { false, 1.0f, 0 };
static const PolygonDrawState default_polygon = { true, false, MATERIAL_NONE, 0 };

/* TODO: these should be replicated once per GL context
 * ^-- more of a MUSTDO */
static CommonDrawState current_common;
static PointDrawState current_point;
static LineDrawState current_line;
static PolygonDrawState current_polygon;

void init_draw_state()
{
	current_common = default_common;
	current_point = default_point;
	current_line = default_line;
	current_polygon = default_polygon;
}

void set_common_state(const CommonDrawState *state)
{
	if (state->blend != current_common.blend) {
		if (state->blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
		current_common.blend = state->blend;
	}

	if (state->depth_test != current_common.depth_test) {
		if (state->depth_test)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		current_common.depth_test = state->depth_test;
	}

	if (state->depth_write != current_common.depth_write) {
		if (state->depth_write)
			glDepthMask(1);
		else
			glDepthMask(0);
		current_common.depth_write = state->depth_write;
	}

	if (state->lighting != current_common.lighting) {
		if (state->lighting)
			glEnable(GL_LIGHTING);
		else
			glDisable(GL_LIGHTING);
		current_common.lighting = state->lighting;
	}
}

void set_point_state(const PointDrawState *state)
{
	if (state->smooth != current_point.smooth) {
		if (state->smooth)
			glEnable(GL_POINT_SMOOTH);
		else
			glDisable(GL_POINT_SMOOTH);
		current_point.smooth = state->smooth;
	}

	if (state->size != current_point.size) {
		glPointSize(state->size);
		current_point.size = state->size;
	}
}

void set_line_state(const LineDrawState *state)
{
	if (state->smooth != current_line.smooth) {
		if (state->smooth)
			glEnable(GL_LINE_SMOOTH);
		else
			glDisable(GL_LINE_SMOOTH);
		current_line.smooth = state->smooth;
	}

	if (state->width != current_line.width) {
		glLineWidth(state->width);
		current_line.width = state->width;
	}

	if (state->stipple != current_line.stipple) {
		if (state->stipple) {
			glEnable(GL_LINE_STIPPLE);
			/* line stipple is 16-bit pattern */
			const GLushort pattern = 0x4E72; /* or 0xAAAA */
			glLineStipple(state->stipple, pattern);
		}
		else
			glDisable(GL_LINE_STIPPLE);
		current_line.stipple = state->stipple;
	}
}

static GLenum faces_to_cull(const PolygonDrawState *state)
{
	/* https://www.opengl.org/wiki/Face_Culling */

	if (!state->draw_front && !state->draw_back)
		return GL_FRONT_AND_BACK;
	else if (!state->draw_front)
		return GL_FRONT;
	else if (!state->draw_back)
		return GL_BACK;
	else
		return GL_NONE; /* no culling */
}

void set_polygon_state(const PolygonDrawState *state)
{
	const GLenum cull = faces_to_cull(state);
	const GLenum curr_cull = faces_to_cull(&current_polygon);
	if (cull != curr_cull) {
		if (cull == GL_NONE)
			glDisable(GL_CULL_FACE);
		else {
			if (curr_cull == GL_NONE)
				glEnable(GL_CULL_FACE);
			glCullFace(cull);
		}
		current_polygon.draw_front = state->draw_front;
		current_polygon.draw_back = state->draw_back;
	}

	if (state->material_id != current_polygon.material_id) {
		/* TODO: whatever needed to make material active */
		current_polygon.material_id = state->material_id;
	}

	if (state->stipple != current_polygon.stipple) {
		if (state->stipple) {
			glEnable(GL_POLYGON_STIPPLE);
			static bool pattern_set = false;
			if (!pattern_set) {
				/* polygon stipple is 32x32-bit pattern */
				GLubyte pattern[128];
				memset(pattern, 0xAA, sizeof(pattern));
				glPolygonStipple(pattern);
				pattern_set = true;
			}
		}
		else
			glDisable(GL_LINE_STIPPLE);
		current_polygon.stipple = state->stipple;
	}
}
