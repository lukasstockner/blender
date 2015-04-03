#ifndef BLENDER_GL_ELEMENT_LIST_PRIVATE
#define BLENDER_GL_ELEMENT_LIST_PRIVATE

#include "GPUx_element.h"
#include <stdbool.h>

/* track min & max observed index (for glDrawRangeElements) */
#define TRACK_INDEX_RANGE

struct ElementList {
	unsigned prim_ct; 
	GLenum prim_type; /* GL_POINTS, GL_LINES, GL_TRIANGLES */
	GLenum index_type; /* GL_UNSIGNED_BYTE, _SHORT (ES), also _INT (full GL) */
	unsigned max_allowed_index;
#ifdef TRACK_INDEX_RANGE
	unsigned min_observed_index;
	unsigned max_observed_index;
#endif /* TRACK_INDEX_RANGE */
	void *indices; /* array of index_type */
};

unsigned min_index(const ElementList*);
unsigned max_index(const ElementList*);

#endif /* BLENDER_GL_ELEMENT_LIST_PRIVATE */
