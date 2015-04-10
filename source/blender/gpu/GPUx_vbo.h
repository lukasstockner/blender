#ifndef BLENDER_GL_VERTEX_BUFFER
#define BLENDER_GL_VERTEX_BUFFER

/* vertex buffer support
 * Mike Erwin, Nov 2014 */

#include "GPU_glew.h"
/* ^-- for GLenum (and if you're including this file, you're probably calling OpenGL anyway) */
#include <stdbool.h>

//#define GENERIC_ATTRIB
#define TRUST_NO_ONE
//#define PRINT

struct VertexBuffer; /* forward declaration */
typedef struct VertexBuffer VertexBuffer;

typedef enum {
	KEEP_FLOAT,
	KEEP_INT, /* requires EXT_gpu_shader4 */
	NORMALIZE_INT_TO_FLOAT, /* 127 (ubyte) -> 0.5 (and so on for other int types) */
	CONVERT_INT_TO_FLOAT /* 127 (any int type) -> 127.0 */
} VertexFetchMode;

VertexBuffer* GPUx_vertex_buffer_create(unsigned attrib_ct, unsigned GPUx_vertex_ct);
void GPUx_vertex_buffer_discard(VertexBuffer*);

unsigned GPUx_vertex_ct(const VertexBuffer*);

#ifdef PRINT
void GPUx_attrib_print(const VertexBuffer*, unsigned attrib_num);
#endif /* PRINT */

void GPUx_specify_attrib(VertexBuffer*, unsigned attrib_num,
#ifdef GENERIC_ATTRIB
                    const char *name, /* use any legal GLSL identifier */
#else
                    GLenum attrib_array, /* use GL_VERTEX_ARRAY, GL_NORMAL_ARRAY, etc. */
#endif
                    GLenum comp_type, unsigned comp_ct, VertexFetchMode);

/* set value of single attribute of single vertex
 * incoming data must be of same type & size for this attribute */
void GPUx_set_attrib(VertexBuffer*, unsigned attrib_num, unsigned vertex_num, const void *data);
/* convenience functions for specific type and size
 * can add more like this if it's useful */
void GPUx_set_attrib_2f(VertexBuffer*, unsigned attrib_num, unsigned vertex_num, float x, float y);
void GPUx_set_attrib_3f(VertexBuffer*, unsigned attrib_num, unsigned vertex_num, float x, float y, float z);

/* bulk attribute filling routines (all vertices)
 * incoming data must be of same type & size for this attribute
 * must be tightly packed in memory, no padding */
void GPUx_fill_attrib(VertexBuffer*, unsigned attrib_num, const void *data);
/* this version can have padding between attributes */
void GPUx_fill_attrib_stride(VertexBuffer*, unsigned attrib_num, const void *data, unsigned stride);

/* call before drawing to make this vertex buffer part of current OpenGL state */
void GPUx_vertex_buffer_use(VertexBuffer*);
/* call after drawing */
void GPUx_vertex_buffer_done_using(const VertexBuffer*);

/* alternative to vertex_buffer_use:
 * prime does all the setup (create VBOs, send to GPU, etc.) so use_primed doesn't have to */
void GPUx_vertex_buffer_prime(VertexBuffer*);
void GPUx_vertex_buffer_use_primed(const VertexBuffer*);
/* prime, use_primed, done_using, use_primed, done_using ...
 * use, done_using, use, done_using ... (first use auto-primes)
 * 'use' modifies VAO and VBO IDs on first run, so is non-const (no 'mutable' in C)
 * this was primary motivation for splitting into two functions */

#endif /* BLENDER_GL_VERTEX_BUFFER */
