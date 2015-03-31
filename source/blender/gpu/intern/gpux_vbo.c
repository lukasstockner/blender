
#include "GPUx_vbo.h"
#include <stdlib.h>
#include <string.h>

/* VBOs are guaranteed for any GL >= 1.5
 * They can be turned off here (mostly for comparison). */
#define USE_VBO true

/* VAOs are part of GL 3.0, and optionally available in 2.1 as an extension:
 * APPLE_vertex_array_object or ARB_vertex_array_object
 * the ARB version of VAOs *must* use VBOs for vertex data
 * so we should follow that restriction on all platforms. */
#define USE_VAO (USE_VBO && true)

#if TRUST_NO_ONE
  #include <assert.h>
#endif /* TRUST_NO_ONE */

#if PRINT
  #include <stdio.h>
#endif /* PRINT */

typedef unsigned char byte;

typedef struct {
	GLenum comp_type;
	unsigned comp_ct; /* 1 to 4 */
	unsigned sz; /* size in bytes, 1 to 16 */
	unsigned stride; /* natural stride in bytes, 1 to 16 */
	VertexFetchMode fetch_mode;
#if GENERIC_ATTRIB
	char *name;
#else
	GLenum array;
#endif
	void *data;
	/* TODO: more storage options
	 * - single VBO for all attribs (sequential)
	 * - single VBO, attribs interleaved
	 * - distinguish between static & dynamic attribs, w/ separate storage */
#if USE_VBO
	GLuint vbo_id;
#endif /* USE_VBO */
} Attrib;

static unsigned comp_sz(GLenum type)
{
	const GLubyte sizes[] = {1,1,2,2,4,4,4}; /* uint32 might result in smaller code? */
#if TRUST_NO_ONE
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
	/* I know AMD HW can't fetch these well, so pad it out */
	if (a->comp_ct == 3 && a->sz <= 2)
		return 4 * a->sz;
	else
		return a->comp_ct * a->sz;
}

struct VertexBuffer
{
	unsigned attrib_ct; /* 1 to 16 */
	unsigned vertex_ct;
	Attrib *attribs;
#if USE_VAO
	GLuint vao_id;
#endif /* USE_VAO */
};

#if PRINT
static void attrib_name_print(const Attrib *a)
{
#if TRUST_NO_ONE
	assert(a->comp_type >= GL_BYTE && a->comp_type <= GL_FLOAT);
#endif /* TRUST_NO_ONE */
}

static void attrib_data_print(const VertexBuffer *buff, unsigned attrib_num)
{
}

void attrib_print(const VertexBuffer *buff, unsigned attrib_num)
{
	Attrib *a = buff->attribs + attrib_num;
#if TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(a->comp_type >= GL_BYTE && a->comp_type <= GL_FLOAT);
	assert(a->data != NULL); /* attribute must be specified */
#endif /* TRUST_NO_ONE */
	/* use GLSL names when they exist, or type_count for the others */
	const char *singular[] = {"byte","ubyte","short","ushort","int","uint","float"};
	const char *plural[] = {"byte_","ubyte_","short_","ushort_","ivec","uint_","vec"};
#if GENERIC_ATTRIB
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
	unsigned type_idx = a->comp_type - GL_BYTE;
	if (a->comp_ct == 1)
		printf("attrib %s %s = {\n", singular[type_idx], var_name);
	else
		printf("attrib %s%d %s = {\n", plural[type_idx], a->comp_ct, var_name);

	unsigned comp_size = comp_sz(a->comp_type);
	for (unsigned v = 0; v < buff->vertex_ct; ++v) {
		const void *data = (byte*)a->data + v * a->stride;
		for (unsigned offset = 0; offset < a->sz; ++offset) {
			if (offset % comp_size == 0)
				putchar(' ');
			printf("%02X", *(const byte*)data + offset);
		}
		putchar('\n');
	}
	puts("}");
}
#endif /* PRINT */

VertexBuffer *vertex_buffer_create(unsigned a_ct, unsigned v_ct)
{
#if TRUST_NO_ONE
	assert(a_ct >= 1 && a_ct <= 16);
#endif /* TRUST_NO_ONE */
	VertexBuffer *buff = calloc(1, sizeof(VertexBuffer));
	buff->attrib_ct = a_ct;
	buff->vertex_ct = v_ct;
	buff->attribs = calloc(a_ct, sizeof(Attrib));
	/* TODO: single allocation instead of 2 */
	return buff;
}

void vertex_buffer_discard(VertexBuffer *buff)
{
	for (unsigned a_idx = 0; a_idx < buff->attrib_ct; ++a_idx)
	{
		/* whatever needs doing */
		Attrib *a = buff->attribs + a_idx;
#if USE_VBO
		if (a->vbo_id)
			glDeleteBuffers(1, &a->vbo_id);
#endif /* USE_VBO */
#if GENERIC_ATTRIB
		free(a->name);
#endif /* GENERIC_ATTRIB */
		free(a->data);
	}
#if USE_VAO
	if (buff->vao_id)
		glDeleteVertexArrays(1, &buff->vao_id);
#endif /* USE_VAO */
	free(buff->attribs);
	free(buff);
}

static unsigned attrib_total_size(const VertexBuffer *buff, unsigned attrib_num)
{
#if TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
#endif /* TRUST_NO_ONE */
	const Attrib *attrib = buff->attribs + attrib_num;
	/* just enough space for every vertex, with padding between but not after the last */
	return (buff->vertex_ct - 1) * attrib->stride + attrib->sz;
}

void specify_attrib(VertexBuffer *buff, unsigned attrib_num,
#if GENERIC_ATTRIB
                    const char *name,
#else
                    GLenum attrib_array,
#endif
                    GLenum comp_type, unsigned comp_ct, VertexFetchMode fetch_mode)
{
#if TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(comp_type >= GL_BYTE && comp_type <= GL_FLOAT);
	assert(comp_ct >= 1 && comp_ct <= 4);
	if (comp_type == GL_FLOAT)
		assert(fetch_mode == KEEP_FLOAT);
	else
		assert(fetch_mode != KEEP_FLOAT);
  #if !GENERIC_ATTRIB
	/* classic (non-generic) attributes each have their quirks
	 * handle below */
	switch (attrib_array) {
		case GL_VERTEX_ARRAY:
			assert(comp_type == GL_FLOAT || comp_type == GL_SHORT || comp_type == GL_INT);
			if (comp_type != GL_FLOAT)
				assert(fetch_mode == CONVERT_INT_TO_FLOAT);
			assert(comp_count >= 2);
			break;
		case GL_NORMAL_ARRAY:
			assert(comp_type == GL_FLOAT || comp_type == GL_BYTE || comp_type == GL_SHORT || comp_type == GL_INT);
			if (comp_type != GL_FLOAT)
				assert(fetch_mode == NORMALIZE_INT_TO_FLOAT);
			assert(comp_count == 3);
			break;
		case GL_COLOR_ARRAY:
			/* any comp_type allowed */
			if (comp_type != GL_FLOAT)
				assert(fetch_mode == NORMALIZE_INT_TO_FLOAT);
			assert(comp_count >= 3);
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
	Attrib *attrib = buff->attribs + attrib_num;
#if GENERIC_ATTRIB
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
#if PRINT
	attrib_print(buff, attrib_num);
#endif /* PRINT */
}

void set_attrib(VertexBuffer *buff, unsigned attrib_num, unsigned vertex_num, const void *data)
{
	Attrib *attrib = buff->attribs + attrib_num;
#if TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(vertex_num < buff->vertex_ct);
	assert(attrib->data != NULL); /* attribute must be specified */
#endif /* TRUST_NO_ONE */
	memcpy((byte*)attrib->data + vertex_num * attrib->stride, data, attrib->sz);
}

void set_attrib_3f(VertexBuffer *buff, unsigned attrib_num, unsigned vertex_num, float x, float y, float z)
{
	Attrib *attrib = buff->attribs + attrib_num;
#if TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(attrib->data != NULL); /* attribute must be specified */
	assert(attrib->comp_type == GL_FLOAT);
	assert(attrib->comp_ct == 3);
#endif /* TRUST_NO_ONE */
	const GLfloat data[] = { x, y, z };
	set_attrib(buff, attrib_num, vertex_num, data);
}

void fill_attrib(VertexBuffer *buff, unsigned attrib_num, const void *data)
	{
	Attrib *attrib = buff->attribs + attrib_num;
#if TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(attrib->data != NULL); /* attribute must be specified */
#endif /* TRUST_NO_ONE */
	if (attrib->sz == attrib->stride) {
		/* tightly packed, so we can copy it all at once */
		memcpy(attrib->data, data, buff->vertex_ct * attrib->sz);
	}
	else {
		/* not tightly packed, so we must copy it per vertex
		 * (this is begging for vector (SSE) coding) */
		for (unsigned v = 0; v < buff->vertex_ct; ++v)
			memcpy((byte*)attrib->data + v * attrib->stride, (byte*)data + v * attrib->sz, attrib->sz);
	}
}

void fill_attrib_stride(VertexBuffer *buff, unsigned attrib_num, const void *data, unsigned stride)
{
	Attrib *attrib = buff->attribs + attrib_num;
#if TRUST_NO_ONE
	assert(attrib_num < buff->attrib_ct);
	assert(attrib->data != NULL); /* attribute must be specified */
	assert(stride >= attrib->sz); /* no overlapping attributes (legal but weird) */
#endif /* TRUST_NO_ONE */
	if (stride == attrib->stride) {
		/* natural stride used, so we can copy it all at once */
		memcpy(attrib->data, data, buff->vertex_ct * attrib->sz);
	}
	else {
		/* we must copy it per vertex */
		for (unsigned v = 0; v < buff->vertex_ct; ++v)
			memcpy((byte*)attrib->data + v * attrib->stride, (byte*)data + v * stride, attrib->sz);
	}
}

void vertex_buffer_use(VertexBuffer *buff)
{
#if USE_VAO
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

	for (unsigned a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
		Attrib *a = buff->attribs + a_idx;
#if GENERIC_ATTRIB
		glEnableVertexAttribArray(a_idx);
  #if USE_VBO
		if (a->vbo_id)
			glBindBuffer(a->vbo_id, GL_ARRAY_BUFFER);
		else {
			glGenBuffers(1, &a->vbo_id);
			glBindBuffer(a->vbo_id, GL_ARRAY_BUFFER);
			/* fill with delicious data & send to GPU the first time only */
			glBufferData(GL_ARRAY_BUFFER, attrib_total_size(buff, a_idx), a->data, GL_STATIC_DRAW);
		}

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
  #else /* client vertex array */
		switch (a->fetch_mode) {
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_FALSE, a->stride, a->data);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_TRUE, a->stride, a->data);
				break;
			case KEEP_INT:
				glVertexAttribIPointerEXT(a_idx, a->comp_ct, a->comp_type, a->stride, a->data);
		}
  #endif /* USE_VBO */
#else /* classic (non-generic) attributes */
		glEnableClientState(a->array);
  #if USE_VBO
		if (a->vbo_id)
			glBindBuffer(a->vbo_id, GL_ARRAY_BUFFER);
		else {
			glGenBuffers(1, &a->vbo_id);
			glBindBuffer(a->vbo_id, GL_ARRAY_BUFFER);
			/* fill with delicious data & send to GPU the first time only */
			glBufferData(GL_ARRAY_BUFFER, attrib_total_size(buff, a_idx), a->data, GL_STATIC_DRAW);
		}
		const void *data = 0;
  #else /* client vertex array */
		const void *data = a->data;
  #endif /* USE_VBO */
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

#if USE_VBO
	glBindBuffer(0, GL_ARRAY_BUFFER);
#endif /* USE_VBO */
}

void vertex_buffer_prime(VertexBuffer *buff)
{
#if USE_VAO
  #if TRUST_NO_ONE
	assert(buff->vao_id == 0);
  #endif /* TRUST_NO_ONE */

	glGenVertexArrays(1, &buff->vao_id);
	glBindVertexArray(buff->vao_id);
#endif /* USE_VAO */

#if USE_VBO
	for (unsigned a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
		Attrib *a = buff->attribs + a_idx;

  #if TRUST_NO_ONE
		assert(a->vbo_id == 0);
  #endif /* TRUST_NO_ONE */

		glGenBuffers(1, &a->vbo_id);
		glBindBuffer(a->vbo_id, GL_ARRAY_BUFFER);
		/* fill with delicious data & send to GPU the first time only */
		glBufferData(GL_ARRAY_BUFFER, attrib_total_size(buff, a_idx), a->data, GL_STATIC_DRAW);

  #if USE_VAO
    #if GENERIC_ATTRIB
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

	glBindBuffer(0, GL_ARRAY_BUFFER);
#endif /* USE_VBO */

#if USE_VAO
	glBindVertexArray(0);
#endif /* USE_VAO */
}

void vertex_buffer_use_primed(const VertexBuffer *buff)
{
#if USE_VAO
  #if TRUST_NO_ONE
	assert(buff->vao_id);
  #endif /* TRUST_NO_ONE */

	/* simply bind & exit */
	glBindVertexArray(buff->vao_id);
#else
	for (unsigned a_idx = 0; a_idx < buff->attrib_ct; ++a_idx) {
		Attrib *a = buff->attribs + a_idx;
  #if TRUST_NO_ONE
		assert(a->vbo_id);
  #endif /* TRUST_NO_ONE */
  #if GENERIC_ATTRIB
		glEnableVertexAttribArray(a_idx);
    #if USE_VBO
		glBindBuffer(a->vbo_id, GL_ARRAY_BUFFER);

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
    #else /* client vertex array */
		switch (a->fetch_mode) {
			case KEEP_FLOAT:
			case CONVERT_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_FALSE, a->stride, a->data);
				break;
			case NORMALIZE_INT_TO_FLOAT:
				glVertexAttribPointer(a_idx, a->comp_ct, a->comp_type, GL_TRUE, a->stride, a->data);
				break;
			case KEEP_INT:
				glVertexAttribIPointerEXT(a_idx, a->comp_ct, a->comp_type, a->stride, a->data);
		}
    #endif /* USE_VBO */
  #else /* classic (non-generic) attributes */
		glEnableClientState(a->array);
    #if USE_VBO
		glBindBuffer(a->vbo_id, GL_ARRAY_BUFFER);
		const void *data = 0;
    #else /* client vertex array */
		const void *data = a->data;
    #endif /* USE_VBO */
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

  #if USE_VBO
	glBindBuffer(0, GL_ARRAY_BUFFER);
  #endif /* USE_VBO */
#endif /* USE_VAO */
}

void vertex_buffer_done_using(const VertexBuffer *buff)
{
#if USE_VAO
	glBindVertexArray(0);
#else
	for (unsigned a_idx = 0; a_idx < buff->attrib_ct; ++a_idx)
  #if GENERIC_ATTRIB
		glDisableVertexAttribArray(a_idx);
  #else
		glDisableClientState(buff->attrib[a_idx].array);
  #endif /* GENERIC_ATTRIB */
#endif /* USE_VAO */
}

unsigned vertex_ct(const VertexBuffer *buff)
{
	return buff->vertex_ct;
}
