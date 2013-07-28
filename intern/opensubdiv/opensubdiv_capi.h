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
struct OpenSubdiv_HbrCatmarkSubdivision;
struct OpenSubdiv_HbrMesh;
struct OpenSubdiv_EvaluationDescr;

/* HBR Catmark functions. */
struct OpenSubdiv_HbrCatmarkSubdivision *openSubdiv_createHbrCatmarkSubdivision(void);
void openSubdiv_deleteHbrCatmarkSubdivision(struct OpenSubdiv_HbrCatmarkSubdivision *sundivision);

/* HBR mesh functions. */
struct OpenSubdiv_HbrMesh *openSubdiv_createCatmarkHbrMesh(struct OpenSubdiv_HbrCatmarkSubdivision *subdivision);
void openSubdiv_deleteHbrMesh(struct OpenSubdiv_HbrMesh *mesh);
void openSubdiv_createHbrMeshVertex(struct OpenSubdiv_HbrMesh *mesh, int id);
void openSubdiv_createHbrMeshTriFace(struct OpenSubdiv_HbrMesh *mesh, int v0, int v1, int v2);
void openSubdiv_createHbrMeshQuadFace(struct OpenSubdiv_HbrMesh *mesh, int v0, int v1, int v2, int v3);
void openSubdiv_createHbrMeshFace(struct OpenSubdiv_HbrMesh *mesh, int num_vertices, int *indices);
void openSubdiv_finishHbrMesh(struct OpenSubdiv_HbrMesh *mesh);

/* Mesh descriptor functions. */
struct OpenSubdiv_MeshDescr *openSubdiv_createMeshDescr(void);
void openSubdiv_deleteMeshDescr(struct OpenSubdiv_MeshDescr *mesh_descr);
void openSubdiv_createMeshDescrVertex(struct OpenSubdiv_MeshDescr *mesh_descr, const float coord[3]);
void openSubdiv_createMeshDescrTriFace(struct OpenSubdiv_MeshDescr *mesh_descr, int v0, int v1, int v2);
void openSubdiv_createMeshDescrQuadFace(struct OpenSubdiv_MeshDescr *mesh_descr, int v0, int v1, int v2, int v3);
void openSubdiv_createMeshDescrFace(struct OpenSubdiv_MeshDescr *mesh_descr, int num_vertices, int *indices);
void openSubdiv_finishMeshDescr(struct OpenSubdiv_MeshDescr *mesh_descr);

/* Evaluation functions. */
struct OpenSubdiv_EvaluationDescr *openSubdiv_createEvaluationDescr(struct OpenSubdiv_MeshDescr *mesh_descr);
void openSubdiv_deleteEvaluationDescr(struct OpenSubdiv_EvaluationDescr *evaluation_descr);
void openSubdiv_evaluateDescr(struct OpenSubdiv_EvaluationDescr *evaluation_descr,
                              int face_id, float u, float v,
                              float P[3], float dPdu[3], float dPdv[3]);

#ifdef __cplusplus
}
#endif

#endif /* __OPENSUBDIV_CAPI_H__ */
