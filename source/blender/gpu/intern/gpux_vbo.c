
#include "GPUx_vbo.h"
#include <stdlib.h>
#include <string.h>

/* VBOs are guaranteed for any GL >= 1.5
 * They can be turned off here (mostly for comparison). */
#define USE_VBO

/* VAOs are part of GL 3.0, and optionally available in 2.1 as an extension:
 * APPLE_vertex_array_object or ARB_vertex_array_object
 * the ARB version of VAOs *must* use VBOs for vertex data
 * so we should follow that restriction on all platforms. */
#ifdef USE_VBO
  #define USE_VAO

  #ifdef __linux__
    #define MESA_WORKAROUND
    /* For padded attributes (stride > size) Mesa likes the VBO to have some extra
     * space at the end, else it drops those attributes of our final vertex.
     * noticed this on Mesa 10.4.3 */
  #endif
#endif

#ifdef TRUST_NO_ONE
  #include <assert.h>
#endif /* TRUST_NO_ONE */

#ifdef PRINT
  #include <stdio.h>
#endif /* PRINT */

typedef unsigned char byte;

typedef struct {
	GLenum comp_type;
	unsigned comp_ct; /* 1 to 4 */
	unsigned sz; /* size in bytes, 1 to 16 */
	unsigned stride; /* natural stride in bytes, 1 to 16 */
	VertexFetchMode fetch_mode;
#ifdef GENERIC_ATTRIB
	char *name;
#else
	GLenum array;
#endif
	void *data;
	/* TODO: more storage options
	 * - single VBO for all attribs (sequential)
	 * - single VBO, attribs interleaved
	 * - distinguish between static & dynamic attribs, w/ separate storage */
#ifdef USE_VBO
	GLuint vbo_id;
#endif /* USE_VBO */
} Attrib;

static unsigned comp_sz(GLenum type)
{
	const GLubyte sizes[] = {1,1,2,2,4,4,4}; /* uint32 might result in smaller code? */
#ifdef TRUST_NO_ONE
	assert(type >= GL_BYTE && type <= GL_FLOAT);
#endif /* TRUST_NO_ONE */
	return sizes[type - GL_BYTE];
}

static unsigned attrib_sz(const Attrib *a)
{
	return a->comp_ct * comp_sz(a->comp_type);
}

static unsigned attrib_align(const Attrib *a)
{
	const unsigned c = comp_sz(a->comp_type);
	/* AMD HW can't fetch these well, so pad it out (other vendors too?) */
	if (a->comp_ct == 3 && c <= 2)
		return 4 * c;
	else
		return a->comp_ct * c;
}

struct VertexBuffer
{
	unsigned attrib_ct; /* 1 to 16 */
	unsigned vertex_ct;
	Attrib *attribs;
#ifdef USE_VAO
	GLuint vao_id;
#endif /* USE_VAO */
};

#ifdef PRINT
void GPUx_attrib_print(const VertexBuffer *buff, unsigned attrib_num)
{
	unsigned int comp_size, v;
	Attrib *a = buff->attribs + attrib_num;
	unsigned type_idx = a->comp_type - GL_BYTE;
	/* use GLSL names when they exist, or type_count for the others */
	const char *singular[] = {"byte","ubyte","short","ushort","int","uint","float"};
	const char *plural[] = {"byte_","ubyte_","short_","ushort_","ivec","uint_","vec"};
#ifdef GENERIC_ATTRIB
	const char *var_name = a->name ? a->name : "foo";
#else
	const char* var_name = "foo";
	switch (a->array) {
		case GL_VERTEX_ARRAY:
			var_name = "gl_Vertex";
			break;
		case GL_NORMAL_ARRAY:
			var_name = "gl_Normal";
			break;
		case GL_COLOR_ARRAY:
			var_name = "gl_Color";
			break;
		case GL_TEXTURE_COORD_ARRAY:
			var_name = "gl_MultiTexCoord0";
			break;
		default:
			;
	}
#endif
#ifdef TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(a->comp_type >= GL_BYTE && a->comp_type <= GL_FLOAT);
	assert(a->data != NULL); /* attribute must be specified */
#endif /* TRUST_NO_ONE */
	if (a->comp_ct == 1)
		printf("attrib %s %s = {\n", singular[type_idx], var_name);
	else
		printf("attrib %s%d %s = {\n", plural[type_idx], a->comp_ct, var_name);

	comp_size = comp_sz(a->comp_type);
	for (v = 0; v < buff->vertex_ct; ++v) {
		unsigned int offset;
		const void *data = (byte*)a->data + v * a->stride;
		for (offset = 0; offset < a->sz; ++offset) {
			if (offset % comp_size == 0)
				putchar(' ');
			printf("%02X", *(const byte*)data + offset);
		}
		putchar('\n');
	}
	puts("}");
}
#endif /* PRINT */

VertexBuffer *GPUx_vertex_buffer_create(unsigned a_ct, unsigned v_ct)
{
	VertexBuffer *buff = calloc(1, sizeof(VertexBuffer));
#ifdef TRUST_NO_ONE
	assert(a_ct >= 1 && a_ct <= 16);
#endif /* TRUST_NO_ONE */
	buff->attrib_ct = a_ct;
	buff->vertex_ct = v_ct;
	buff->attribs = calloc(a_ct, sizeof(Attrib));
	/* TODO: single allocation instead of 2 */
	return buff;
}

void GPUx_vertex_buffer_discard(VertexBuffer *buff)
{
	unsigned a_idx;
	for (a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
		Attrib *a = buff->attribs + a_idx;
#ifdef USE_VBO
		if (a->vbo_id)
			glDeleteBuffers(1, &a->vbo_id);
#endif /* USE_VBO */
#ifdef GENERIC_ATTRIB
		free(a->name);
#endif /* GENERIC_ATTRIB */
		free(a->data);
	}
#ifdef USE_VAO
	if (buff->vao_id)
		glDeleteVertexArrays(1, &buff->vao_id);
#endif /* USE_VAO */
	free(buff->attribs);
	free(buff);
}

static unsigned attrib_total_size(const VertexBuffer *buff, unsigned attrib_num)
{
	const Attrib *attrib = buff->attribs + attrib_num;
#ifdef TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
#endif /* TRUST_NO_ONE */

#ifdef MESA_WORKAROUND
	/* an over-estimate, with padding after each vertex */
	return buff->vertex_ct * attrib->stride;
#else
	/* just enough space for every vertex, with padding between but not after the last */
	return (buff->vertex_ct - 1) * attrib->stride + attrib->sz;
#endif /*  MESA_WORKAROUND */
}

void GPUx_specify_attrib(VertexBuffer *buff, unsigned attrib_num,
#ifdef GENERIC_ATTRIB
                    const char *name,
#else
                    GLenum attrib_array,
#endif
                    GLenum comp_type, unsigned comp_ct, VertexFetchMode fetch_mode)
{
	Attrib *attrib;
#ifdef TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(comp_type >= GL_BYTE && comp_type <= GL_FLOAT);
	assert(comp_ct >= 1 && comp_ct <= 4);

	if (comp_type == GL_FLOAT)
		assert(fetch_mode == KEEP_FLOAT);
	else
		assert(fetch_mode != KEEP_FLOAT);

  #ifndef GENERIC_ATTRIB
	/* classic (non-generic) attributes each have their quirks
	 * handle below */
	switch (attrib_array) {
		case GL_VERTEX_ARRAY:
			assert(comp_type == GL_FLOAT || comp_type == GL_SHORT || comp_type == GL_INT);
			if (comp_type != GL_FLOAT)
				assert(fetch_mode == CONVERT_INT_TO_FLOAT);
			assert(comp_ct >= 2);
			break;
		case GL_NORMAL_ARRAY:
			assert(comp_type == GL_FLOAT || comp_type == GL_BYTE || comp_type == GL_SHORT || comp_type == GL_INT);
			if (comp_type != GL_FLOAT)
				assert(fetch_mode == NORMALIZE_INT_TO_FLOAT);
			assert(comp_ct == 3);
			break;
		case GL_COLOR_ARRAY:
			/* any comp_type allowed */
			if (comp_type != GL_FLOAT)
				assert(fetch_mode == NORMALIZE_INT_TO_FLOAT);
			assert(comp_ct >= 3);
			break;
		case GL_TEXTURE_COORD_ARRAY:
			assert(comp_type == GL_FLOAT || comp_type == GL_SHORT || comp_type == GL_INT);
			if (comp_type != GL_FLOAT)
				assert(fetch_mode == CONVERT_INT_TO_FLOAT);
			break;
		/* not supporting these:
		 * GL_INDEX_ARRAY
		 * GL_SECONDARY_COLOR_ARRAY
		 * GL_EDGE_FLAG_ARRAY
		 * GL_FOG_COORD_ARRAY
		 */
		default:
			assert(false); /* bad or unsupported array */
	}
	assert(fetch_mode != KEEP_INT); /* glVertexPointer and friends have no int variants */
	/* TODO: allow only one of each type of array (scan other attribs) */
  #endif
#endif /* TRUST_NO_ONE */
	attrib = buff->attribs + attrib_num;
#ifdef GENERIC_ATTRIB
	attrib->name = strdup(name);
#else
	attrib->array = attrib_array;
#endif /* GENERIC_ATTRIB */
	attrib->comp_type = comp_type;
	attrib->comp_ct = comp_ct;
	attrib->sz = attrib_sz(attrib);
	attrib->stride = attrib_align(attrib);
	attrib->fetch_mode = fetch_mode;
	attrib->data = malloc(attrib_total_size(buff, attrib_num));
#ifdef PRINT
	GPUx_attrib_print(buff, attrib_num);
#endif /* PRINT */
}

void GPUx_set_attrib(VertexBuffer *buff, unsigned attrib_num, unsigned vertex_num, const void *data)
{
	Attrib *attrib = buff->attribs + attrib_num;
#ifdef TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(vertex_num < buff->vertex_ct);
	assert(attrib->data != NULL); /* attribute must be specified */
#endif /* TRUST_NO_ONE */
	memcpy((byte*)attrib->data + vertex_num * attrib->stride, data, attrib->sz);
}

void GPUx_set_attrib_2f(VertexBuffer *buff, unsigned attrib_num, unsigned vertex_num, float x, float y)
{
	const GLfloat data[] = { x, y };
#ifdef TRUST_NO_ONE
	Attrib *attrib = buff->attribs + attrib_num;
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 2);
#endif /* TRUST_NO_ONE */
	GPUx_set_attrib(buff, attrib_num, vertex_num, data);
}

void GPUx_set_attrib_3f(VertexBuffer *buff, unsigned attrib_num, unsigned vertex_num, float x, float y, float z)
{
	const GLfloat data[] = { x, y, z };
#ifdef TRUST_NO_ONE
	Attrib *attrib = buff->attribs + attrib_num;
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 3);
#endif /* TRUST_NO_ONE */
	GPUx_set_attrib(buff, attrib_num, vertex_num, data);
}

void GPUx_fill_attrib(VertexBuffer *buff, unsigned attrib_num, const void *data)
	{
	Attrib *attrib = buff->attribs + attrib_num;
#ifdef TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(attrib->data != NULL); /* attribute must be specified */
#endif /* TRUST_NO_ONE */
	if (attrib->sz == attrib->stride) {
		/* tightly packed, so we can copy it all at once */
		memcpy(attrib->data, data, buff->vertex_ct * attrib->sz);
	}
	else {
		unsigned v;
		/* not tightly packed, so we must copy it per vertex
		 * (this is begging for vector (SSE) coding) */
		for (v = 0; v < buff->vertex_ct; ++v)
			memcpy((byte*)attrib->data + v * attrib->stride, (byte*)data + v * attrib->sz, attrib->sz);
	}
}

void GPUx_fill_attrib_stride(VertexBuffer *buff, unsigned attrib_num, const void *data, unsigned stride)
{
	Attrib *attrib = buff->attribs + attrib_num;
#ifdef TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(attrib->data != NULL); /* attribute must be specified */
	assert(stride >= attrib->sz); /* no overlapping attributes (legal but weird) */
#endif /* TRUST_NO_ONE */
	if (stride == attrib->stride) {
		/* natural stride used, so we can copy it all at once */
		memcpy(attrib->data, data, buff->vertex_ct * attrib->sz);
	}
	else {
		unsigned v;
		/* we must copy it per vertex */
		for (v = 0; v < buff->vertex_ct; ++v)
			memcpy((byte*)attrib->data + v * attrib->stride, (byte*)data + v * stride, attrib->sz);
	}
}

void GPUx_vertex_buffer_use(VertexBuffer *buff)
{
	unsigned a_idx;
	const void *data;
#ifdef USE_VAO
	if (buff->vao_id) {
		/* simply bind & exit */
		glBindVertexArray(buff->vao_id);
		return;
	}
	else {
		glGenVertexArrays(1, &buff->vao_id);
		glBindVertexArray(buff->vao_id);
	}
#endif /* USE_VAO */

	for (a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
		Attrib *a = buff->attribs + a_idx;

#ifdef GENERIC_ATTRIB
		glEnableVertexAttribArray(a_idx);
#else
		glEnableClientState(a->array);
#endif /* GENERIC_ATTRIB */

#ifdef USE_VBO
		if (a->vbo_id)
			glBindBuffer(GL_ARRAY_BUFFER, a->vbo_id);
		else {
			glGenBuffers(1, &a->vbo_id);
			glBindBuffer(GL_ARRAY_BUFFER, a->vbo_id);
			/* fill with delicious data & send to GPU the first time only */
			glBufferData(GL_ARRAY_BUFFER, attrib_total_size(buff, a_idx), a->data, GL_STATIC_DRAW);
		}

		data = 0;
#else /* client vertex array */
		data = a->data;
#endif /* USE_VBO */

#ifdef GENERIC_ATTRIB
		switch (a->fetch_mode) {
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_FALSE, a->stride, data);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_TRUE, a->stride, data);
				break;
			case KEEP_INT:
				glVertexAttribIPointerEXT(a_idx, a->comp_ct, a->comp_type, a->stride, data);
		}
#else /* classic (non-generic) attributes */
		switch (a->array) {
			case GL_VERTEX_ARRAY:
				glVertexPointer(a->comp_ct, a->comp_type, a->stride, data);
				break;
			case GL_NORMAL_ARRAY:
				glNormalPointer(a->comp_type, a->stride, data);
				break;
			case GL_COLOR_ARRAY:
				glColorPointer(a->comp_ct, a->comp_type, a->stride, data);
				break;
			case GL_TEXTURE_COORD_ARRAY:
				glTexCoordPointer(a->comp_ct, a->comp_type, a->stride, data);
				/* TODO: transition to glMultiTexCoordPointer? */
				break;
			default:
				;
		}
#endif /* GENERIC_ATTRIB */
	}

#ifdef USE_VBO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif /* USE_VBO */
}

void GPUx_vertex_buffer_prime(VertexBuffer *buff)
{
	unsigned a_idx;
#ifdef USE_VAO
  #ifdef TRUST_NO_ONE
	assert(buff->vao_id == 0);
  #endif /* TRUST_NO_ONE */

	glGenVertexArrays(1, &buff->vao_id);
	glBindVertexArray(buff->vao_id);
#endif /* USE_VAO */

	(void)a_idx; /* avoid unused warnings */
	(void)buff;

#ifdef USE_VBO
	for (a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
		Attrib *a = buff->attribs + a_idx;

  #ifdef USE_VAO
    #ifdef GENERIC_ATTRIB
		glEnableVertexAttribArray(a_idx);
    #else
		glEnableClientState(a->array);
    #endif /* GENERIC_ATTRIB */
  #endif /* USE_VAO */

  #ifdef TRUST_NO_ONE
		assert(a->vbo_id == 0);
  #endif /* TRUST_NO_ONE */

		glGenBuffers(1, &a->vbo_id);
		glBindBuffer(GL_ARRAY_BUFFER, a->vbo_id);
		/* fill with delicious data & send to GPU the first time only */
		glBufferData(GL_ARRAY_BUFFER, attrib_total_size(buff, a_idx), a->data, GL_STATIC_DRAW);

  #ifdef USE_VAO
    #ifdef GENERIC_ATTRIB
		switch (a->fetch_mode) {
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_FALSE, a->stride, 0);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_TRUE, a->stride, 0);
				break;
			case KEEP_INT:
				glVertexAttribIPointerEXT(a_idx, a->comp_ct, a->comp_type, a->stride, 0);
		}
    #else /* classic (non-generic) attributes */
		switch (a->array) {
			case GL_VERTEX_ARRAY:
				glVertexPointer(a->comp_ct, a->comp_type, a->stride, 0);
				break;
			case GL_NORMAL_ARRAY:
				glNormalPointer(a->comp_type, a->stride, 0);
				break;
			case GL_COLOR_ARRAY:
				glColorPointer(a->comp_ct, a->comp_type, a->stride, 0);
				break;
			case GL_TEXTURE_COORD_ARRAY:
				glTexCoordPointer(a->comp_ct, a->comp_type, a->stride, 0);
				break;
			default:
				;
		}
    #endif /* GENERIC_ATTRIB */
  #endif /* USE_VAO */
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif /* USE_VBO */

#ifdef USE_VAO
	glBindVertexArray(0);
#endif /* USE_VAO */
}

void GPUx_vertex_buffer_use_primed(const VertexBuffer *buff)
{
#ifdef USE_VAO
  #ifdef TRUST_NO_ONE
	assert(buff->vao_id);
  #endif /* TRUST_NO_ONE */

	/* simply bind & exit */
	glBindVertexArray(buff->vao_id);
#else
	unsigned int a_idx;

	for (a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
		Attrib *a = buff->attribs + a_idx;
		const void *data;

  #ifdef GENERIC_ATTRIB
		glEnableVertexAttribArray(a_idx);
  #else
		glEnableClientState(a->array);
  #endif /* GENERIC_ATTRIB */

  #ifdef USE_VBO
    #ifdef TRUST_NO_ONE
		assert(a->vbo_id);
    #endif /* TRUST_NO_ONE */
		glBindBuffer(GL_ARRAY_BUFFER, a->vbo_id);

		data = 0;
  #else /* client vertex array */
		data = a->data;
  #endif /* USE_VBO */

  #ifdef GENERIC_ATTRIB
		switch (a->fetch_mode) {
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_FALSE, a->stride, data);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_TRUE, a->stride, data);
				break;
			case KEEP_INT:
				glVertexAttribIPointerEXT(a_idx, a->comp_ct, a->comp_type, a->stride, data);
		}
  #else /* classic (non-generic) attributes */
		switch (a->array) {
			case GL_VERTEX_ARRAY:
				glVertexPointer(a->comp_ct, a->comp_type, a->stride, data);
				break;
			case GL_NORMAL_ARRAY:
				glNormalPointer(a->comp_type, a->stride, data);
				break;
			case GL_COLOR_ARRAY:
				glColorPointer(a->comp_ct, a->comp_type, a->stride, data);
				break;
			case GL_TEXTURE_COORD_ARRAY:
				glTexCoordPointer(a->comp_ct, a->comp_type, a->stride, data);
				break;
			default:
				;
		}
  #endif /* GENERIC_ATTRIB */
	}

  #ifdef USE_VBO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
  #endif /* USE_VBO */
#endif /* USE_VAO */
}

void GPUx_vertex_buffer_done_using(const VertexBuffer *buff)
{
#ifdef USE_VAO
	(void)buff;
	glBindVertexArray(0);
#else
	unsigned int a_idx;

	for (a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
  #ifdef GENERIC_ATTRIB
		glDisableVertexAttribArray(a_idx);
  #else
		glDisableClientState(buff->attribs[a_idx].array);
  #endif /* GENERIC_ATTRIB */
	}
#endif /* USE_VAO */
}

unsigned GPUx_vertex_ct(const VertexBuffer *buff)
{
	return buff->vertex_ct;
}
