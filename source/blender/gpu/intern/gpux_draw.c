
#include "GPUx_draw.h"
#include "gpux_element_private.h"

#include <stdlib.h>

#ifdef TRUST_NO_ONE
  #include <assert.h>
#endif /* TRUST_NO_ONE */

#define REALLY_DRAW

/* generally useful utility function */
static unsigned chop_to_multiple(unsigned x, unsigned m)
{
	return x - x % m;
}

void GPUx_draw_points(const VertexBuffer *vbo, const ElementList *el, const PointDrawState *point_state, const CommonDrawState *common_state)
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

	if (el) {
		GPUx_element_list_use_primed(el);
		glDrawRangeElements(GL_POINTS, min_index(el), max_index(el), el->prim_ct, el->index_type, index_ptr(el));
		GPUx_element_list_done_using(el);
	}
	else
		glDrawArrays(GL_POINTS, 0, GPUx_vertex_ct(vbo));

	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}

void GPUx_draw_lines(const VertexBuffer *vbo, const ElementList *el, const LineDrawState *line_state, const CommonDrawState *common_state)
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

	if (el) {
		GPUx_element_list_use_primed(el);
		glDrawRangeElements(GL_LINES, min_index(el), max_index(el), el->prim_ct * 2, el->index_type, index_ptr(el));
		GPUx_element_list_done_using(el);
	}
	else
		glDrawArrays(GL_LINES, 0, chop_to_multiple(GPUx_vertex_ct(vbo), 2));

	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}

void GPUx_draw_triangles(const VertexBuffer *vbo, const ElementList *el, const PolygonDrawState *polygon_state, const CommonDrawState *common_state)
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

	if (el) {
		GPUx_element_list_use_primed(el);
		glDrawRangeElements(GL_TRIANGLES, min_index(el), max_index(el), el->prim_ct * 3, el->index_type, index_ptr(el));
		GPUx_element_list_done_using(el);
	}
	else
		glDrawArrays(GL_TRIANGLES, 0, chop_to_multiple(GPUx_vertex_ct(vbo), 3));

	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}

void GPUx_draw_primitives(const VertexBuffer *vbo, const ElementList *el, const void *primitive_state, const CommonDrawState *common_state)
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
	GPUx_element_list_use_primed(el);

	glDrawRangeElements(el->prim_type, min_index(el), max_index(el), el->prim_ct * vert_per_prim, el->index_type, index_ptr(el));

	GPUx_element_list_done_using(el);
	GPUx_vertex_buffer_done_using(vbo);
#endif /* REALLY_DRAW */
}

GPUxBatch *GPUx_batch_create()
{
	GPUxBatch *batch = calloc(1, sizeof(GPUxBatch));
	batch->prim_type = GL_NONE;
	batch->state = default_state;
	return batch;
}

void GPUx_batch_discard(GPUxBatch *batch)
{
	GPUx_vertex_buffer_discard(batch->buff);
	if (batch->elem)
		GPUx_element_list_discard(batch->elem);
	free(batch);
}

unsigned GPUx_batch_size(const GPUxBatch *batch)
{
	unsigned sz = GPUx_vertex_buffer_size(batch->buff);
	if (batch->elem)
		sz += GPUx_element_list_size(batch->elem);
	return sz;
}

void GPUx_draw_batch(const GPUxBatch *batch)
{
	int vert_per_prim = 0;

#ifdef TRUST_NO_ONE
	if (batch->elem) {
		assert(batch->elem->prim_type == batch->prim_type);
		assert(max_index(batch->elem) < GPUx_vertex_ct(batch->buff));
	}
#endif /* TRUST_NO_ONE */

	switch (batch->prim_type) {
		case GL_POINTS:
			GPUx_set_point_state(&batch->state.point);
			vert_per_prim = 1;
			break;
		case GL_LINES:
			GPUx_set_line_state(&batch->state.line);
			vert_per_prim = 2;
			break;
		case GL_TRIANGLES:
			GPUx_set_polygon_state(&batch->state.polygon);
			vert_per_prim = 3;
			break;
		default:
#ifdef TRUST_NO_ONE
			assert(false);
#else
			return;
#endif /* TRUST_NO_ONE */
	}

	GPUx_set_common_state(&batch->state.common);

#ifdef REALLY_DRAW
	GPUx_vertex_buffer_use_primed(batch->buff);

	if (batch->elem) {
		GPUx_element_list_use_primed(batch->elem);
		glDrawRangeElements(batch->prim_type, min_index(batch->elem), max_index(batch->elem),
		                    batch->elem->prim_ct * vert_per_prim, batch->elem->index_type, index_ptr(batch->elem));
		GPUx_element_list_done_using(batch->elem);
	}
	else
		glDrawArrays(batch->prim_type, 0, chop_to_multiple(GPUx_vertex_ct(batch->buff), vert_per_prim));

	GPUx_vertex_buffer_done_using(batch->buff);
#endif /* REALLY_DRAW */
}
