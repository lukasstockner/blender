#ifndef BLENDER_GL_BUFFER_ID
#define BLENDER_GL_BUFFER_ID

/* Manage GL buffer IDs in a thread-safe way
 * Use these instead of glGenBuffers & its friends
 * - alloc must be called from main thread
 * - free can be called from any thread
 * private for now since GPUx uses this internally
 * Mike Erwin, April 2015 */

#include "GPU_glew.h"

GLuint buffer_id_alloc();
void buffer_id_free(GLuint buffer_id);

GLuint vao_id_alloc();
void vao_id_free(GLuint vao_id);

#endif /* BLENDER_GL_BUFFER_ID */
