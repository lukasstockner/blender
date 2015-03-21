
#include "GPUx_element_private.h"
#include <stdlib.h>

#if TRACK_INDEX_RANGE
void track_index_range(ElementList* el, unsigned v)
	{
	if (v < el->min_observed_index)
		el->min_observed_index = v;
	if (v > el->max_observed_index) // would say "else if" but the first time...
		el->max_observed_index = v;
	}
#endif // TRACK_INDEX_RANGE

unsigned min_index(const ElementList* el)
	{
#if TRACK_INDEX_RANGE
	return el->min_observed_index;
#else
	return 0;
#endif // TRACK_INDEX_RANGE
	}

unsigned max_index(const ElementList* el)
	{
#if TRACK_INDEX_RANGE
	return el->max_observed_index;
#else
	return el->max_allowed_index;
#endif // TRACK_INDEX_RANGE
	}

ElementList* create_element_list(GLenum prim_type, unsigned prim_ct, unsigned max_index)
	{
	ElementList* el;
	unsigned index_size, prim_vertex_ct;

	if (prim_type == GL_POINTS)
		prim_vertex_ct = 1;
	else if (prim_type == GL_LINES)
		prim_vertex_ct = 2;
	else if (prim_type == GL_TRIANGLES)
		prim_vertex_ct = 3;
	else {
#if TRUST_NO_ONE
		assert(false);
#endif // TRUST_NO_ONE
		return NULL;
		}

	el = calloc(1, sizeof(ElementList));

	el->prim_type = prim_type;
	el->prim_ct = prim_ct;
	el->max_allowed_index = max_index;

	if (max_index <= 255)
		{
		el->index_type = GL_UNSIGNED_BYTE;
		index_size = sizeof(GLubyte);
		}
	else if (max_index <= 65535)
		{
		el->index_type = GL_UNSIGNED_SHORT;
		index_size = sizeof(GLushort);
		}
	else
		{
		el->index_type = GL_UNSIGNED_INT;
		index_size = sizeof(GLuint);
		}

#if TRACK_INDEX_RANGE
	el->min_observed_index = 0xFFFFFFFF;
//	el->min_observed_index = (unsigned) -1;
	el->max_observed_index = 0;
#endif // TRACK_INDEX_RANGE

	el->indices = calloc(prim_ct * prim_vertex_ct, index_size);
	// TODO: use only one calloc, not two

	return el;
	}

void discard_element_list(ElementList* el)
	{
	free(el->indices);
	free(el);
	}

void set_point_vertex(ElementList* el, unsigned prim_idx, unsigned v1)
	{
	const unsigned offset = prim_idx;
#if TRUST_NO_ONE
	assert(el->prim_type == GL_POINTS);
	assert(prim_idx < el->prim_ct); // prim out of range
	assert(v1 <= el->max_allowed_index); // index out of range
#endif // TRUST_NO_ONE
#if TRACK_INDEX_RANGE
	track_index_range(el, v1);
#endif // TRACK_INDEX_RANGE
	switch (el->index_type)
		{
		case GL_UNSIGNED_BYTE:
			{
			GLubyte* indices = el->indices;
			indices[offset] = v1;
			break;
			}
		case GL_UNSIGNED_SHORT:
			{
			GLushort* indices = el->indices;
			indices[offset] = v1;
			break;
			}
		case GL_UNSIGNED_INT:
			{
			GLuint* indices = el->indices;
			indices[offset] = v1;
			break;
			}
		}
	}

void set_line_vertices(ElementList* el, unsigned prim_idx, unsigned v1, unsigned v2)
	{
	const unsigned offset = prim_idx * 2;
#if TRUST_NO_ONE
	assert(el->prim_type == GL_LINES);
	assert(prim_idx < el->prim_ct); // prim out of range
	assert(v1 <= el->max_allowed_index && v2 <= el->max_allowed_index); // index out of range
	assert(v1 != v2); // degenerate line
#endif // TRUST_NO_ONE
#if TRACK_INDEX_RANGE
	track_index_range(el, v1);
	track_index_range(el, v2);
#endif // TRACK_INDEX_RANGE
	switch (el->index_type)
		{
		case GL_UNSIGNED_BYTE:
			{
			GLubyte* indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			break;
			}
		case GL_UNSIGNED_SHORT:
			{
			GLushort* indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			break;
			}
		case GL_UNSIGNED_INT:
			{
			GLuint* indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			break;
			}
		}
	}

void set_triangle_vertices(ElementList* el, unsigned prim_idx, unsigned v1, unsigned v2, unsigned v3)
	{
	const unsigned offset = prim_idx * 3;
#if TRUST_NO_ONE
	assert(el->prim_type == GL_TRIANGLES);
	assert(prim_idx < el->prim_ct); // prim out of range
	assert(v1 <= el->max_allowed_index && v2 <= el->max_allowed_index && v3 <= el->max_allowed_index); // index out of range
	assert(v1 != v2 && v2 != v3 && v3 != v1); // degenerate triangle
#endif // TRUST_NO_ONE
#if TRACK_INDEX_RANGE
	track_index_range(el, v1);
	track_index_range(el, v2);
	track_index_range(el, v3);
#endif // TRACK_INDEX_RANGE
	switch (el->index_type)
		{
		case GL_UNSIGNED_BYTE:
			{
			GLubyte* indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			indices[offset + 2] = v3;
			break;
			}
		case GL_UNSIGNED_SHORT:
			{
			GLushort* indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			indices[offset + 2] = v3;
			break;
			}
		case GL_UNSIGNED_INT:
			{
			GLuint* indices = el->indices;
			indices[offset] = v1;
			indices[offset + 1] = v2;
			indices[offset + 2] = v3;
			break;
			}
		}
	}

void optimize(ElementList* el)
	{
	// TODO: apply Forsyth's vertex cache algorithm
	// ...
	
	// http://hacksoflife.blogspot.com/2010/01/to-strip-or-not-to-strip.html
	// http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html <-- excellent
	// http://home.comcast.net/%7Etom_forsyth/blog.wiki.html#%5B%5BRegular%20mesh%20vertex%20cache%20ordering%5D%5D

	// Another opportunity: lines & triangles can have their verts rotated
	// could use this for de-dup and cache optimization.
	// line ab = ba
	// triangle abc = bca = cab

	// TODO: (optional) rearrange vertex attrib buffer to improve mem locality
	}
