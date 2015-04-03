
#include "GPUx_state.h"
#include "GPUx_vbo.h"

#include <string.h> /* memset */

#ifdef TRUST_NO_ONE
  #include <assert.h>
#endif /* TRUST_NO_ONE */

const DrawState default_state = {
	.common = { false, true, true, false },
	.point = { false, 1.0f },
	.line = { false, 1.0f, 0 },
	.polygon = { true, false, MATERIAL_NONE, 0 }
};

static DrawState current;
static bool polygon_stipple_pattern_set = false;
/* TODO: these should be replicated once per GL context
 * ^-- more of a MUSTDO */

void GPUx_reset_draw_state()
{
	current = default_state;
#if 0 /* TODO: make default state play nice with UI drawing code */
	force_state_update();
#endif
}

void GPUx_set_common_state(const CommonDrawState *state)
{
	if (state->blend != current.common.blend) {
		if (state->blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
		current.common.blend = state->blend;
	}

	if (state->depth_test != current.common.depth_test) {
		if (state->depth_test)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		current.common.depth_test = state->depth_test;
	}

	if (state->depth_write != current.common.depth_write) {
		if (state->depth_write)
			glDepthMask(1);
		else
			glDepthMask(0);
		current.common.depth_write = state->depth_write;
	}

	if (state->lighting != current.common.lighting) {
		if (state->lighting)
			glEnable(GL_LIGHTING);
		else
			glDisable(GL_LIGHTING);
		current.common.lighting = state->lighting;
	}
}

void GPUx_set_point_state(const PointDrawState *state)
{
	if (state->smooth != current.point.smooth) {
		if (state->smooth)
			glEnable(GL_POINT_SMOOTH);
		else
			glDisable(GL_POINT_SMOOTH);
		current.point.smooth = state->smooth;
	}

	if (state->size != current.point.size) {
		glPointSize(state->size);
		current.point.size = state->size;
	}
}

void GPUx_set_line_state(const LineDrawState *state)
{
	if (state->smooth != current.line.smooth) {
		if (state->smooth)
			glEnable(GL_LINE_SMOOTH);
		else
			glDisable(GL_LINE_SMOOTH);
		current.line.smooth = state->smooth;
	}

	if (state->width != current.line.width) {
		glLineWidth(state->width);
		current.line.width = state->width;
	}

	if (state->stipple != current.line.stipple) {
		if (state->stipple) {
			const GLushort pattern = 0x4E72; /* or 0xAAAA */
			glEnable(GL_LINE_STIPPLE);
			/* line stipple is 16-bit pattern */
			glLineStipple(state->stipple, pattern);
		}
		else
			glDisable(GL_LINE_STIPPLE);
		current.line.stipple = state->stipple;
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

void GPUx_set_polygon_state(const PolygonDrawState *state)
{
	const GLenum cull = faces_to_cull(state);
	const GLenum curr_cull = faces_to_cull(&current.polygon);
	if (cull != curr_cull) {
		if (cull == GL_NONE)
			glDisable(GL_CULL_FACE);
		else {
			if (curr_cull == GL_NONE)
				glEnable(GL_CULL_FACE);
			glCullFace(cull);
		}
		current.polygon.draw_front = state->draw_front;
		current.polygon.draw_back = state->draw_back;
	}

	if (state->material_id != current.polygon.material_id) {
		/* TODO: whatever needed to make material active */
		current.polygon.material_id = state->material_id;
	}

	if (state->stipple != current.polygon.stipple) {
		if (state->stipple) {
			glEnable(GL_POLYGON_STIPPLE);
			if (!polygon_stipple_pattern_set) {
				/* polygon stipple is 32x32-bit pattern */
				GLubyte pattern[128];
				memset(pattern, 0xAA, sizeof(pattern));
				glPolygonStipple(pattern);
				polygon_stipple_pattern_set = true;
			}
		}
		else
			glDisable(GL_LINE_STIPPLE);
		current.polygon.stipple = state->stipple;
	}
}

void GPUx_force_state_update()
{
	const GLenum cull = faces_to_cull(&current.polygon);
	/* TODO: factor some of this stuff out, share with set_*_state functions? */

	/* common state */
	if (current.common.blend)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);

	if (current.common.depth_test)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	if (current.common.depth_write)
		glDepthMask(1);
	else
		glDepthMask(0);

	if (current.common.lighting)
		glEnable(GL_LIGHTING);
	else
		glDisable(GL_LIGHTING);

	/* point state */
	if (current.point.smooth)
		glEnable(GL_POINT_SMOOTH);
	else
		glDisable(GL_POINT_SMOOTH);

	glPointSize(current.point.size);

	/* line state */
	if (current.line.smooth)
		glEnable(GL_LINE_SMOOTH);
	else
		glDisable(GL_LINE_SMOOTH);

	glLineWidth(current.line.width);

	if (current.line.stipple) {
		const GLushort pattern = 0x4E72; /* or 0xAAAA */
		glEnable(GL_LINE_STIPPLE);
		/* line stipple is 16-bit pattern */
		glLineStipple(current.line.stipple, pattern);
	}
	else
		glDisable(GL_LINE_STIPPLE);

	/* polygon state */
	if (cull == GL_NONE)
		glDisable(GL_CULL_FACE);
	else {
		glEnable(GL_CULL_FACE);
		glCullFace(cull);
	}

	/* TODO: whatever needed to make material active */

	if (current.polygon.stipple) {
		GLubyte pattern[128];
		glEnable(GL_POLYGON_STIPPLE);
		/* polygon stipple is 32x32-bit pattern */
		memset(pattern, 0xAA, sizeof(pattern));
		glPolygonStipple(pattern);
		polygon_stipple_pattern_set = true;
	}
	else
		glDisable(GL_LINE_STIPPLE);
}
