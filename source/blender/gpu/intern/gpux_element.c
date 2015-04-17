
#include "gpux_element_private.h"
#include "MEM_guardedalloc.h"

/* private functions */

#ifdef TRACK_INDEX_RANGE
static void track_index_range(ElementList *el, unsigned v)
{
	if (v < el->min_observed_index)
		el->min_observed_index = v;
	if (v > el->max_observed_index) /* would say "else if" but the first time... */
		el->max_observed_index = v;
}
#endif /* TRACK_INDEX_RANGE */

unsigned min_index(const ElementList *el)
{
#ifdef TRACK_INDEX_RANGE
	return el->min_observed_index;
#else
	return 0;
#endif /* TRACK_INDEX_RANGE */
}

unsigned max_index(const ElementList *el)
{
#ifdef TRACK_INDEX_RANGE
	return el->max_observed_index;
#else
	return el->max_allowed_index;
#endif /* TRACK_INDEX_RANGE */
}

const void *index_ptr(const ElementList *el)
{
#ifdef USE_ELEM_VBO
	if (el->vbo_id) /* primed, data lives in buffer object */
		return (const void*)0;
	else /* data lives in client memory */
		return el->indices;
#else
	return el->indices;
#endif /* USE_ELEM_VBO */
}

/* public functions */

ElementList *GPUx_element_list_create(GLenum prim_type, unsigned prim_ct, unsigned max_index)
{
	ElementList *el;
	unsigned index_size, prim_vertex_ct;

	if (prim_type == GL_POINTS)
		prim_vertex_ct = 1;
	else if (prim_type == GL_LINES)
		prim_vertex_ct = 2;
	else if (prim_type == GL_TRIANGLES)
		prim_vertex_ct = 3;
	else {
#ifdef TRUST_NO_ONE
		BLI_assert(false);
#endif /* TRUST_NO_ONE */
		return NULL;
	}

	el = MEM_callocN(sizeof(ElementList), "ElementList");

	el->prim_type = prim_type;
	el->prim_ct = prim_ct;
	el->max_allowed_index = max_index;

	if (max_index <= 255) {
		el->index_type = GL_UNSIGNED_BYTE;
		index_size = sizeof(GLubyte);
	}
	else if (max_index <= 65535) {
		el->index_type = GL_UNSIGNED_SHORT;
		index_size = sizeof(GLushort);
	}
	else {
		el->index_type = GL_UNSIGNED_INT;
		index_size = sizeof(GLuint);
	}

#ifdef TRACK_INDEX_RANGE
	el->min_observed_index = max_index + 1; /* any valid index will be < this */
	el->max_observed_index = 0;
#endif /* TRACK_INDEX_RANGE */

	el->indices = MEM_callocN(prim_ct * prim_vertex_ct * index_size, "ElementList.indices");
	/* TODO: use only one calloc, not two */

	return el;
}

void GPUx_element_list_discard(ElementList *el)
{
#ifdef USE_ELEM_VBO
	if (el->vbo_id)
		glDeleteBuffers(1, &el->vbo_id);
#endif /* USE_ELEM_VBO */

	MEM_freeN(el->indices);
	MEM_freeN(el);
}

void GPUx_set_point_vertex(ElementList *el, unsigned prim_idx, unsigned v1)
{
	const unsigned offset = prim_idx;
#ifdef TRUST_NO_ONE
	BLI_assert(el->prim_type == GL_POINTS);
	BLI_assert(prim_idx < el->prim_ct); /* prim out of range */
	BLI_assert(v1 <= el->max_allowed_index); /* index out of range */
#endif /* TRUST_NO_ONE */
#ifdef TRACK_INDEX_RANGE
	track_index_range(el, v1);
#endif /* TRACK_INDEX_RANGE */
	switch (el->index_type) {
		case GL_UNSIGNED_BYTE:
		{
			GLubyte *indices = el->indices;
			indices[offset] = v1;
			break;
		}
		case GL_UNSIGNED_SHORT:
		{
			GLushort *indices = el->indices;
			indices[offset] = v1;
			break;
		}
		case GL_UNSIGNED_INT:
		{
			GLuint *indices = el->indices;
			indices[offset] = v1;
			break;
		}
	}
}

void GPUx_set_line_vertices(ElementList *el, unsigned prim_idx, unsigned v1, unsigned v2)
{
	const unsigned offset = prim_idx * 2;
#ifdef TRUST_NO_ONE
	BLI_assert(el->prim_type == GL_LINES);
	BLI_assert(prim_idx < el->prim_ct); /* prim out of range */
	BLI_assert(v1 <= el->max_allowed_index && v2 <= el->max_allowed_index); /* index out of range */
	BLI_assert(v1 != v2); /* degenerate line */
#endif /* TRUST_NO_ONE */
#ifdef TRACK_INDEX_RANGE
	track_index_range(el, v1);
	track_index_range(el, v2);
#endif /* TRACK_INDEX_RANGE */
	switch (el->index_type) {
		case GL_UNSIGNED_BYTE:
		{
			GLubyte *indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			break;
		}
		case GL_UNSIGNED_SHORT:
		{
			GLushort *indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			break;
		}
		case GL_UNSIGNED_INT:
		{
			GLuint *indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			break;
		}
	}
}

void GPUx_set_triangle_vertices(ElementList *el, unsigned prim_idx, unsigned v1, unsigned v2, unsigned v3)
{
	const unsigned offset = prim_idx * 3;
#ifdef TRUST_NO_ONE
	BLI_assert(el->prim_type == GL_TRIANGLES);
	BLI_assert(prim_idx < el->prim_ct); /* prim out of range */
	BLI_assert(v1 <= el->max_allowed_index && v2 <= el->max_allowed_index && v3 <= el->max_allowed_index); /* index out of range */
	BLI_assert(v1 != v2 && v2 != v3 && v3 != v1); /* degenerate triangle */
#endif /* TRUST_NO_ONE */
#ifdef TRACK_INDEX_RANGE
	track_index_range(el, v1);
	track_index_range(el, v2);
	track_index_range(el, v3);
#endif /* TRACK_INDEX_RANGE */
	switch (el->index_type) {
		case GL_UNSIGNED_BYTE:
		{
			GLubyte *indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			indices[offset + 2] = v3;
			break;
		}
		case GL_UNSIGNED_SHORT:
		{
			GLushort *indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			indices[offset + 2] = v3;
			break;
		}
		case GL_UNSIGNED_INT:
		{
			GLuint *indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			indices[offset + 2] = v3;
			break;
		}
	}
}

void GPUx_optimize(ElementList *el)
{
	(void)el;

	/* TODO: apply Forsyth's vertex cache algorithm */
	
	/* http://hacksoflife.blogspot.com/2010/01/to-strip-or-not-to-strip.html
	 * http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html <-- excellent
	 * http://home.comcast.net/%7Etom_forsyth/blog.wiki.html#%5B%5BRegular%20mesh%20vertex%20cache%20ordering%5D%5D */

	/* Another opportunity: lines & triangles can have their verts rotated
	 * could use this for de-dup and cache optimization.
	 * line ab = ba
	 * triangle abc = bca = cab */

	/* TODO: (optional) rearrange vertex attrib buffer to improve mem locality */
}

void GPUx_element_list_prime(ElementList *el)
{
#ifdef USE_ELEM_VBO
	int prim_vertex_ct = 0, index_size = 0, total_size;

#ifdef TRUST_NO_ONE
	BLI_assert(el->vbo_id == 0);
  #endif /* TRUST_NO_ONE */

	if (el->prim_type == GL_POINTS)
		prim_vertex_ct = 1;
	else if (el->prim_type == GL_LINES)
		prim_vertex_ct = 2;
	else if (el->prim_type == GL_TRIANGLES)
		prim_vertex_ct = 3;

	if (el->index_type == GL_UNSIGNED_BYTE)
		index_size = sizeof(GLubyte);
	else if (el->index_type == GL_UNSIGNED_SHORT)
		index_size = sizeof(GLushort);
	else if (el->index_type == GL_UNSIGNED_INT)
		index_size = sizeof(GLuint);

	total_size = prim_vertex_ct * el->prim_ct * index_size;

	glGenBuffers(1, &el->vbo_id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, el->vbo_id);
	/* fill with delicious data & send to GPU the first time only */
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, total_size, el->indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#else
	(void)el;
#endif /* USE_ELEM_VBO */
}

void GPUx_element_list_use_primed(const ElementList *el)
{
#ifdef USE_ELEM_VBO
  #ifdef TRUST_NO_ONE
	BLI_assert(el->vbo_id);
  #endif /* TRUST_NO_ONE */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, el->vbo_id);
#else
	(void)el;
#endif /* USE_ELEM_VBO */
}

void GPUx_element_list_done_using(const ElementList *el)
{
	(void)el;
#ifdef USE_ELEM_VBO
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif /* USE_ELEM_VBO */
}
