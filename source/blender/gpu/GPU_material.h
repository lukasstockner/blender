
#ifndef __GPU_MATERIAL__
#define __GPU_MATERIAL__

#include "GPU_node.h"

#define GPU_MAX_ATTRIB		32

struct Image;
struct ImageUser;
struct GPUMaterial;
typedef struct GPUMaterial GPUMaterial;

typedef struct GPUVertexAttribs {
	struct {
		int type;
		int glindex;
		char name[32];
	} layer[GPU_MAX_ATTRIB];

	int totlayer;
} GPUVertexAttribs;

GPUMaterial *GPU_material_construct_begin();
int GPU_material_construct_end(GPUMaterial *material);
void GPU_material_free(GPUMaterial *material);
void GPU_material_bind(GPUMaterial *material);
void GPU_material_unbind(GPUMaterial *material);

void GPU_material_vertex_attributes(GPUMaterial *material,
	GPUVertexAttribs *attrib);

GPUNode *GPU_mat_node_create(GPUMaterial *material, char *name,
	GPUNodeStack *in, GPUNodeStack *out);
void GPU_mat_node_uniform(GPUNode *node, GPUType type, void *ptr);
void GPU_mat_node_texture(GPUNode *node, GPUType type, int size, float *pixels);
void GPU_mat_node_image(GPUNode *node, GPUType type, struct Image *ima,
	struct ImageUser *iuser);
void GPU_mat_node_attribute(GPUNode *node, GPUType type, int laytype,
	char *name);
void GPU_mat_node_socket(GPUNode *node, GPUNodeStack *sock);
void GPU_mat_node_output(GPUNode *node, GPUType type, char *name,
	GPUNodeStack *out);

#endif /*__GPU_MATERIAL__*/

