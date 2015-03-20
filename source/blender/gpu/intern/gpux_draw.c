
#include "GPUx_draw.h"
#include "GPUx_element_private.h"

#if TRUST_NO_ONE
  #include <assert.h>
#endif // TRUST_NO_ONE

#define REALLY_DRAW true

// generally useful utility function
static unsigned chop_to_multiple(unsigned x, unsigned m)
	{
	return x - x % m;
	}

void draw_points(const CommonDrawState* common_state, const PointDrawState* point_state, const VertexBuffer* vbo, const ElementList* el)
	{
	set_common_state(common_state);
	set_point_state(point_state);

#if TRUST_NO_ONE
	if (el)
		{
		assert(el->prim_type == GL_POINTS);
		assert(el->max_allowed_index < vertex_ct(vbo));
		}
#endif // TRUST_NO_ONE

#if REALLY_DRAW
	vertex_buffer_use_primed(vbo);

	if (el)
		glDrawRangeElements(GL_POINTS, min_index(el), max_index(el), el->prim_ct, el->index_type, el->indices);
	else
		glDrawArrays(GL_POINTS, 0, vertex_ct(vbo));

	vertex_buffer_done_using(vbo);
#endif // REALLY_DRAW
	}

void draw_lines(const CommonDrawState* common_state, const LineDrawState* line_state, const VertexBuffer* vbo, const ElementList* el)
	{
	set_common_state(common_state);
	set_line_state(line_state);

#if TRUST_NO_ONE
	if (el)
		{
		assert(el->prim_type == GL_LINES);
		assert(el->max_allowed_index < vertex_ct(vbo));
		}
#endif // TRUST_NO_ONE

#if REALLY_DRAW
	vertex_buffer_use_primed(vbo);

	if (el)
		glDrawRangeElements(GL_LINES, min_index(el), max_index(el), el->prim_ct * 2, el->index_type, el->indices);
	else
		glDrawArrays(GL_LINES, 0, chop_to_multiple(vertex_ct(vbo), 2));

	vertex_buffer_done_using(vbo);
#endif // REALLY_DRAW
	}

void draw_triangles(const CommonDrawState* common_state, const PolygonDrawState* polygon_state, const VertexBuffer* vbo, const ElementList* el)
	{
	set_common_state(common_state);
	set_polygon_state(polygon_state);

#if TRUST_NO_ONE
	if (el)
		{
		assert(el->prim_type == GL_TRIANGLES);
		assert(el->max_allowed_index < vertex_ct(vbo));
		}
#endif // TRUST_NO_ONE

#if REALLY_DRAW
	vertex_buffer_use_primed(vbo);

	if (el)
		glDrawRangeElements(GL_TRIANGLES, min_index(el), max_index(el), el->prim_ct * 3, el->index_type, el->indices);
	else
		glDrawArrays(GL_TRIANGLES, 0, chop_to_multiple(vertex_ct(vbo), 3));

	vertex_buffer_done_using(vbo);
#endif // REALLY_DRAW
	}

void draw_primitives(const CommonDrawState* common_state, const void* primitive_state, const VertexBuffer* vbo, const ElementList* el)
	{
#if TRUST_NO_ONE
	assert(el->max_allowed_index < vertex_ct(vbo));
#endif // TRUST_NO_ONE

	int vert_per_prim = 0;

	switch (el->prim_type)
		{
		case GL_POINTS:
			set_point_state((const PointDrawState*)primitive_state);
			vert_per_prim = 1;
			break;
		case GL_LINES:
			set_line_state((const LineDrawState*)primitive_state);
			vert_per_prim = 2;
			break;
		case GL_TRIANGLES:
			set_polygon_state((const PolygonDrawState*)primitive_state);
			vert_per_prim = 3;
			break;
		default:
#if TRUST_NO_ONE
			assert(false);
#else
			return;
#endif // TRUST_NO_ONE
		}

	set_common_state(common_state);

#if REALLY_DRAW
	vertex_buffer_use_primed(vbo);

	glDrawRangeElements(el->prim_type, min_index(el), max_index(el), el->prim_ct * vert_per_prim, el->index_type, el->indices);

	vertex_buffer_done_using(vbo);
#endif // REALLY_DRAW
	}
