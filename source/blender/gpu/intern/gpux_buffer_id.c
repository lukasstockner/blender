#include "gpux_buffer_id.h"
#include "BLI_utildefines.h"
#include "BLI_threads.h"

//#define ORPHAN_DEBUG

#ifdef ORPHAN_DEBUG
  #include <stdio.h>
#endif

#define ORPHANED_BUFFER_MAX 48
static GLuint orphaned_buffer_ids[ORPHANED_BUFFER_MAX];
static unsigned orphaned_buffer_ct = 0;

#define ORPHANED_VAO_MAX 16
static GLuint orphaned_vao_ids[ORPHANED_VAO_MAX];
static unsigned orphaned_vao_ct = 0;

static ThreadMutex orphan_mutex = BLI_MUTEX_INITIALIZER;

GLuint buffer_id_alloc()
{
	GLuint new_buffer_id;

	BLI_assert(BLI_thread_is_main());

	/* delete orphaned IDs */
	BLI_mutex_lock(&orphan_mutex);
	if (orphaned_buffer_ct) {
#ifdef ORPHAN_DEBUG
		printf("deleting %d orphaned VBO%s\n", orphaned_buffer_ct, orphaned_buffer_ct == 1 ? "" : "s");
#endif
		glDeleteBuffers(orphaned_buffer_ct, orphaned_buffer_ids);
		orphaned_buffer_ct = 0;
	}
	BLI_mutex_unlock(&orphan_mutex);

	glGenBuffers(1, &new_buffer_id);
	return new_buffer_id;
}

void buffer_id_free(GLuint buffer_id)
{
	if (BLI_thread_is_main()) {
		glDeleteBuffers(1, &buffer_id);
	}
	else {
		/* add this ID to the orphaned list */
		BLI_mutex_lock(&orphan_mutex);
		BLI_assert(orphaned_buffer_ct < ORPHANED_BUFFER_MAX); /* increase MAX if needed */
#ifdef ORPHAN_DEBUG
		printf("orphaning VBO %d\n", buffer_id);
#endif
		orphaned_buffer_ids[orphaned_buffer_ct++] = buffer_id;
		BLI_mutex_unlock(&orphan_mutex);
	}
}

GLuint vao_id_alloc()
{
	GLuint new_vao_id;

	BLI_assert(BLI_thread_is_main());

	/* delete orphaned IDs */
	BLI_mutex_lock(&orphan_mutex);
	if (orphaned_vao_ct) {
#ifdef ORPHAN_DEBUG
		printf("deleting %d orphaned VAO%s\n", orphaned_vao_ct, orphaned_vao_ct == 1 ? "" : "s");
#endif
		glDeleteVertexArrays(orphaned_vao_ct, orphaned_vao_ids);
		orphaned_vao_ct = 0;
	}
	BLI_mutex_unlock(&orphan_mutex);

	glGenVertexArrays(1, &new_vao_id);
	return new_vao_id;
}

void vao_id_free(GLuint vao_id)
{
	if (BLI_thread_is_main()) {
		glDeleteVertexArrays(1, &vao_id);
	}
	else {
		/* add this ID to the orphaned list */
		BLI_mutex_lock(&orphan_mutex);
		BLI_assert(orphaned_vao_ct < ORPHANED_VAO_MAX); /* increase MAX if needed */
#ifdef ORPHAN_DEBUG
		printf("orphaning VAO %d\n", vao_id);
#endif
		orphaned_vao_ids[orphaned_vao_ct++] = vao_id;
		BLI_mutex_unlock(&orphan_mutex);
	}
}
