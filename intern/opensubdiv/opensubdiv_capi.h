/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENSUBDIV_CAPI_H__
#define __OPENSUBDIV_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

// Types declaration.
struct OpenSubdiv_EvaluatorDescr;
struct OpenSubdiv_GLMesh;

#ifdef __cplusplus
struct OpenSubdiv_GLMeshDescr;
typedef struct OpenSubdiv_GLMesh {
	int controller_type;
	OpenSubdiv_GLMeshDescr *descriptor;
} OpenSubdiv_GLMesh;
#endif

// Keep this a bitmask os it's possible to pass vailable
// controllers to Blender.
enum {
	OPENSUBDIV_CONTROLLER_CPU                      = (1 << 0),
	OPENSUBDIV_CONTROLLER_OPENMP                   = (1 << 1),
	OPENSUBDIV_CONTROLLER_OPENCL                   = (1 << 2),
	OPENSUBDIV_CONTROLLER_CUDA                     = (1 << 3),
	OPENSUBDIV_CONTROLLER_GLSL_TRANSFORM_FEEDBACK  = (1 << 4),
	OPENSUBDIV_CONTROLLER_GLSL_COMPUTE             = (1 << 5),
};

struct OpenSubdiv_GLMesh *openSubdiv_createOsdGLMeshFromEvaluator(
    struct OpenSubdiv_EvaluatorDescr *evaluator_descr,
    int controller_type,
    int level);

void openSubdiv_deleteOsdGLMesh(struct OpenSubdiv_GLMesh *gl_mesh);
unsigned int openSubdiv_getOsdGLMeshPatchIndexBuffer(struct OpenSubdiv_GLMesh *gl_mesh);
unsigned int openSubdiv_bindOsdGLMeshVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshUpdateVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh,
                                            const float *vertex_data,
                                            int start_vertex,
                                            int num_verts);
void openSubdiv_osdGLMeshRefine(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshSynchronize(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshDisplay(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshBindvertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh);

int openSubdiv_getAvailableControllers(void);

void openSubdiv_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif  /* __OPENSUBDIV_CAPI_H__ */
