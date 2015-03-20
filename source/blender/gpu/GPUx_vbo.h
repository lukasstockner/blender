#ifndef BLENDER_GL_VERTEX_BUFFER
#define BLENDER_GL_VERTEX_BUFFER

// vertex buffer support
// Mike Erwin, Nov 2014

#include "GPU_glew.h"
// ^-- for GLenum (and if you're including this file, you're probably calling OpenGL anyway)
#include <stdbool.h>

#define TRUST_NO_ONE true
#define PRINT false

struct VertexBuffer; // forward declaration
typedef struct VertexBuffer VertexBuffer;

typedef enum
	{
	KEEP_FLOAT,
	KEEP_INT, // requires EXT_gpu_shader4
	NORMALIZE_INT_TO_FLOAT, // 127 (ubyte) -> 0.5 (and so on for other int types)
	CONVERT_INT_TO_FLOAT // 127 (any int type) -> 127.0
	} VertexFetchMode;

VertexBuffer* vertex_buffer_create(unsigned attrib_ct, unsigned vertex_ct);
void vertex_buffer_discard(VertexBuffer*);

// how many vertices?
unsigned vertex_ct(const VertexBuffer*);

#if PRINT
void attrib_print(const VertexBuffer*, unsigned attrib_num);
#endif // PRINT

void specify_attrib(VertexBuffer*, unsigned attrib_num, const char* name, GLenum comp_type, unsigned comp_ct, VertexFetchMode);

// set value of single attribute of single vertex
// incoming data must be of same type & size for this attribute
void set_attrib(VertexBuffer*, unsigned attrib_num, unsigned vertex_num, const void* data);
// convenience function for specific type and size
// can add more like this if it's useful
void set_attrib_3f(VertexBuffer*, unsigned attrib_num, unsigned vertex_num, float x, float y, float z);

// bulk attribute filling routines (all vertices)
// incoming data must be of same type & size for this attribute
// must be tightly packed in memory, no padding
void fill_attrib(VertexBuffer*, unsigned attrib_num, const void* data);
// this version can have padding between attributes
void fill_attrib_stride(VertexBuffer*, unsigned attrib_num, const void* data, unsigned stride);

// call before drawing to make this vertex buffer part of current OpenGL state
void vertex_buffer_use(VertexBuffer*);
// call after drawing
void vertex_buffer_done_using(const VertexBuffer*);

// alternative to vertex_buffer_use:
// prime does all the setup (create VBOs, send to GPU, etc.) so use_primed doesn't have to
void vertex_buffer_prime(VertexBuffer*);
void vertex_buffer_use_primed(const VertexBuffer*);
// prime, use_primed, done_using, use_primed, done_using ...
// use, done_using, use, done_using ... (first use auto-primes)
// 'use' modifies VAO and VBO IDs on first run, so is non-const (no 'mutable' in C)
// this was primary motivation for splitting into two functions

#endif // BLENDER_GL_VERTEX_BUFFER
