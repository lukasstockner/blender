#ifndef BLENDER_GL_DRAW_PRIMITIVES
#define BLENDER_GL_DRAW_PRIMITIVES

/* Stateless draw functions for lists of primitives.
 * Mike Erwin, Dec 2014 */

#include "GPUx_state.h"
#include "GPUx_vbo.h"
#include "GPUx_element.h"

/* pass ElementList = NULL to draw all vertices from VertexBuffer in order
 * pass NULL to either state arg to use defaults */
void GPUx_draw_points(const VertexBuffer*, const ElementList*, const PointDrawState*, const CommonDrawState*);
void GPUx_draw_lines(const VertexBuffer*, const ElementList*, const LineDrawState*, const CommonDrawState*);
void GPUx_draw_triangles(const VertexBuffer*, const ElementList*, const PolygonDrawState*, const CommonDrawState*);

/* generic version uses ElementList's primitive type */
void GPUx_draw_primitives(const VertexBuffer*, const ElementList*, const void *primitive_state, const CommonDrawState*);

#endif /* BLENDER_GL_DRAW_PRIMITIVES */
