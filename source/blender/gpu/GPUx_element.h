#ifndef BLENDER_GL_ELEMENT_LIST
#define BLENDER_GL_ELEMENT_LIST

/* Element lists specify which vertices to use when drawing point,
 * line, or triangle primiitives. They don't care how the per-vertex
 * data (attributes) are laid out, only *which* vertices are used.
 * Mike Erwin, Dec 2014 */

#include "GPU_glew.h"
/* ^-- for GLenum (and if you're including this file, you're probably calling OpenGL anyway) */

struct ElementList; /* forward declaration */
typedef struct ElementList ElementList;

ElementList *create_element_list(GLenum prim_type, unsigned prim_ct, unsigned max_index);
void discard_element_list(ElementList*);

void set_point_vertex(ElementList*, unsigned prim_idx, unsigned v1);
void set_line_vertices(ElementList*, unsigned prim_idx, unsigned v1, unsigned v2);
void set_triangle_vertices(ElementList*, unsigned prim_idx, unsigned v1, unsigned v2, unsigned v3);

void optimize(ElementList*); /* optionally call this after setting all vertex indices */

#endif /* BLENDER_GL_ELEMENT_LIST */
