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

/* handle normals/shading in various ways */
typedef enum {
	NORMAL_DRAW_NONE, /* draw uniform surface (unlit, uncolored) for depth/picking */
	NORMAL_DRAW_SMOOTH, /* use vertex normals for smooth appearance */
	NORMAL_DRAW_FLAT, /* use poly normals for faceted appearance */
	NORMAL_DRAW_LOOP, /* use loop normals for most faithful rendering */
} NormalDrawMode;

typedef struct GPUxBatch {
	GLenum prim_type; /* GL_POINTS, GL_LINES, GL_TRIANGLES (must match elem->prim_type) */
	int draw_type; /* OB_WIRE, OB_SOLID, OB_MATERIAL */
	NormalDrawMode normal_draw_mode;
	DrawState state;
	VertexBuffer *buff; /* TODO: rename "verts" */
	ElementList *elem;
} GPUxBatch;

GPUxBatch *GPUx_batch_create(void);
void GPUx_batch_discard(GPUxBatch*);

unsigned GPUx_batch_size(const GPUxBatch*); /* total, in bytes */

void GPUx_draw_batch(const GPUxBatch*);

#endif /* BLENDER_GL_DRAW_PRIMITIVES */
