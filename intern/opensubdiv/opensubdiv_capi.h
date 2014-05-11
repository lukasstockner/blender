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

/* Types declaration. */
struct OpenSubdiv_CPUComputeController;
struct OpenSubdiv_CUDAComputeController;
struct OpenSubdiv_GLSLComputeController;
struct OpenSubdiv_EvaluatorDescr;
struct OpenSubdiv_GLMesh;

/* CPU Compute Controller functions */
struct OpenSubdiv_CPUComputeController *openSubdiv_createCPUComputeController(void);
void openSubdiv_deleteCPUComputeController(struct OpenSubdiv_CPUComputeController *controller);

/* GLSL Compute Controller functions */
//struct OpenSubdiv_GLSLComputeController *openSubdiv_createGLSLComputeController(void);
//void openSubdiv_deleteGLSLComputeController(struct OpenSubdiv_GLSLComputeController *controller);

/* CUDA Compute Controller functions. */
struct OpenSubdiv_CUDAComputeController *openSubdiv_createCUDAComputeController(void);
void openSubdiv_deleteCUDAComputeController(struct OpenSubdiv_CUDAComputeController *controller);

/* OpenSubdiv GL Mesh functions */
struct OpenSubdiv_GLMesh *openSubdiv_createOsdGLMeshFromEvaluator(
    struct OpenSubdiv_EvaluatorDescr *evaluator_descr,
    struct OpenSubdiv_CUDAComputeController *controller,
    int level);

void openSubdiv_deleteOsdGLMesh(struct OpenSubdiv_GLMesh *gl_mesh);
unsigned int openSubdiv_getOsdGLMeshPatchIndexBuffer(struct OpenSubdiv_GLMesh *gl_mesh);
unsigned int openSubdiv_bindOsdGLMeshVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshUpdateVertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh,
                                            const float *vertex_data,
                                            int start_vertex,
                                            int num_verts);
void openSubdiv_osdGLMeshUpdateVertexBufferFromDescr(struct OpenSubdiv_GLMesh *gl_mesh,
                                                     struct OpenSubdiv_MeshDescr *mesh_descr);
void openSubdiv_osdGLMeshRefine(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshSynchronize(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshDisplay(struct OpenSubdiv_GLMesh *gl_mesh);
void openSubdiv_osdGLMeshBindvertexBuffer(struct OpenSubdiv_GLMesh *gl_mesh);

#ifdef __cplusplus
}
#endif

#endif  /* __OPENSUBDIV_CAPI_H__ */
