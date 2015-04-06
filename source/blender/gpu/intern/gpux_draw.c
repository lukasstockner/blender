
#include "GPUx_draw.h"
#include "gpux_element_private.h"

#ifdef TRUST_NO_ONE
  #include <assert.h>
#endif /* TRUST_NO_ONE */

#define REALLY_DRAW

/* generally useful utility function */
static unsigned chop_to_multiple(unsigned x, unsigned m)
{
	return x - x % m;
}

void GPUx_draw_points(const CommonDrawState *common_state, const PointDrawState *point_state, const VertexBuffer *vbo, const ElementList *el)
{
	GPUx_set_common_state(common_state);
	GPUx_set_point_state(point_state);

#ifdef TRUST_NO_ONE
	if (el) {
		assert(el->prim_type == GL_POINTS);
		assert(max_index(el) < GPUx_vertex_ct(vbo));
	}
#endif /* TRUST_NO_ONE */

#ifdef REALLY_DRAW
	GPUx_vertex_buffer_use_primed(vbo);

	if (el)
		glDrawRangeElements(GL_POINTS, min_index(el), max_index(el), el->prim_ct, el->index_type, el->indices);
	else
		glDrawArrays(GL_POINTS, 0, GPUx_vertex_ct(vbo));

	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}

void GPUx_draw_lines(const CommonDrawState *common_state, const LineDrawState *line_state, const VertexBuffer *vbo, const ElementList *el)
{
	GPUx_set_common_state(common_state);
	GPUx_set_line_state(line_state);

#ifdef TRUST_NO_ONE
	if (el) {
		assert(el->prim_type == GL_LINES);
		assert(max_index(el) < GPUx_vertex_ct(vbo));
	}
#endif /* TRUST_NO_ONE */

#ifdef REALLY_DRAW
	GPUx_vertex_buffer_use_primed(vbo);

	if (el)
		glDrawRangeElements(GL_LINES, min_index(el), max_index(el), el->prim_ct * 2, el->index_type, el->indices);
	else
		glDrawArrays(GL_LINES, 0, chop_to_multiple(GPUx_vertex_ct(vbo), 2));

	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}

void GPUx_draw_triangles(const CommonDrawState *common_state, const PolygonDrawState *polygon_state, const VertexBuffer *vbo, const ElementList *el)
{
	GPUx_set_common_state(common_state);
	GPUx_set_polygon_state(polygon_state);

#ifdef TRUST_NO_ONE
	if (el) {
		assert(el->prim_type == GL_TRIANGLES);
		assert(max_index(el) < GPUx_vertex_ct(vbo));
	}
#endif /* TRUST_NO_ONE */

#ifdef REALLY_DRAW
	GPUx_vertex_buffer_use_primed(vbo);

	if (el)
		glDrawRangeElements(GL_TRIANGLES, min_index(el), max_index(el), el->prim_ct * 3, el->index_type, el->indices);
	else
		glDrawArrays(GL_TRIANGLES, 0, chop_to_multiple(GPUx_vertex_ct(vbo), 3));

	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}

void GPUx_draw_primitives(const CommonDrawState *common_state, const void *primitive_state, const VertexBuffer *vbo, const ElementList *el)
{
	int vert_per_prim = 0;

#ifdef TRUST_NO_ONE
	assert(max_index(el) < GPUx_vertex_ct(vbo));
#endif /* TRUST_NO_ONE */

	switch (el->prim_type) {
		case GL_POINTS:
			GPUx_set_point_state((const PointDrawState*)primitive_state);
			vert_per_prim = 1;
			break;
		case GL_LINES:
			GPUx_set_line_state((const LineDrawState*)primitive_state);
			vert_per_prim = 2;
			break;
		case GL_TRIANGLES:
			GPUx_set_polygon_state((const PolygonDrawState*)primitive_state);
			vert_per_prim = 3;
			break;
		default:
#ifdef TRUST_NO_ONE
			assert(false);
#else
			return;
#endif /* TRUST_NO_ONE */
	}

	GPUx_set_common_state(common_state);

#ifdef REALLY_DRAW
	GPUx_vertex_buffer_use_primed(vbo);

	glDrawRangeElements(el->prim_type, min_index(el), max_index(el), el->prim_ct * vert_per_prim, el->index_type, el->indices);

	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}
