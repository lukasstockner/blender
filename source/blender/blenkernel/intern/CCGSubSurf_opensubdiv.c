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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/CCGSubSurf_opensubdiv.c
 *  \ingroup bke
 */

#ifdef WITH_OPENSUBDIV

#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h" // for intptr_t support

#include "BLI_utildefines.h" /* for BLI_assert */
#include "BLI_math.h"

#include "CCGSubSurf.h"
#include "CCGSubSurf_intern.h"

#include "BKE_DerivedMesh.h"

#include "DNA_userdef_types.h"

#include "opensubdiv_capi.h"
#include "opensubdiv_converter_capi.h"

#include "GL/glew.h"

#define OSD_LOG if (false) printf

/* TODO(sergey): Move converters API to own file for clearity of code. */
static int conv_dm_get_num_faces(const OpenSubdiv_Converter *converter)
{
	DerivedMesh *dm = converter->user_data;
	return dm->getNumPolys(dm);
}

static int conv_dm_get_num_edges(const OpenSubdiv_Converter *converter)
{
	DerivedMesh *dm = converter->user_data;
	return dm->getNumEdges(dm);
}

static int conv_dm_get_num_verts(const OpenSubdiv_Converter *converter)
{
	DerivedMesh *dm = converter->user_data;
	return dm->getNumVerts(dm);
}

static int conv_dm_get_num_face_verts(const OpenSubdiv_Converter *converter,
                                      int face)
{
	DerivedMesh *dm = converter->user_data;
	const MPoly *mp = dm->getPolyArray(dm);
	const MPoly *mpoly = &mp[face];
	return mpoly->totloop;
}

static void conv_dm_get_face_verts(const OpenSubdiv_Converter *converter,
                                   int face,
                                   int *face_verts)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	const MPoly *mpoly = &mp[face];
	int loop;
	for(loop = 0; loop < mpoly->totloop; loop++) {
		face_verts[loop] = ml[mpoly->loopstart + loop].v;
	}
}

static void conv_dm_get_face_edges(const OpenSubdiv_Converter *converter,
                                   int face,
                                   int *face_edges)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	const MPoly *mpoly = &mp[face];
	int loop;
	for(loop = 0; loop < mpoly->totloop; loop++) {
		face_edges[loop] = ml[mpoly->loopstart + loop].e;
	}
}

static void conv_dm_get_edge_verts(const OpenSubdiv_Converter *converter,
                                   int edge,
                                   int *edge_verts)
{
	DerivedMesh *dm = converter->user_data;
	const MEdge *me = dm->getEdgeArray(dm);
	const MEdge *medge = &me[edge];
	edge_verts[0] = medge->v1;
	edge_verts[1] = medge->v2;
}

static int conv_dm_get_num_edge_faces(const OpenSubdiv_Converter *converter,
                                      int edge)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				++num;
				break;
			}
		}
	}
	return num;
}

static void conv_dm_get_edge_faces(const OpenSubdiv_Converter *converter,
                                   int edge,
                                   int *edge_faces)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->e == edge) {
				edge_faces[num++] = poly;
				break;
			}
		}
	}
}

static int conv_dm_get_num_vert_edges(const OpenSubdiv_Converter *converter,
                                      int vert)
{
	DerivedMesh *dm = converter->user_data;
	const MEdge *me = dm->getEdgeArray(dm);
	int num = 0, edge;
	for (edge = 0; edge < dm->getNumEdges(dm); edge++) {
		const MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			++num;
		}
	}
	return num;
}

static void conv_dm_get_vert_edges(const OpenSubdiv_Converter *converter,
                                   int vert,
                                   int *vert_edges)
{
	DerivedMesh *dm = converter->user_data;
	const MEdge *me = dm->getEdgeArray(dm);
	int num = 0, edge;
	for (edge = 0; edge < dm->getNumEdges(dm); edge++) {
		const MEdge *medge = &me[edge];
		if (medge->v1 == vert || medge->v2 == vert) {
			vert_edges[num++] = edge;
		}
	}
}

static int conv_dm_get_num_vert_faces(const OpenSubdiv_Converter *converter,
                                      int vert)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				++num;
				break;
			}
		}
	}
	return num;
}

static void conv_dm_get_vert_faces(const OpenSubdiv_Converter *converter,
                                   int vert,
                                   int *vert_faces)
{
	DerivedMesh *dm = converter->user_data;
	const MLoop *ml = dm->getLoopArray(dm);
	const MPoly *mp = dm->getPolyArray(dm);
	int num = 0, poly;
	for (poly = 0; poly < dm->getNumPolys(dm); poly++) {
		const MPoly *mpoly = &mp[poly];
		int loop;
		for (loop = 0; loop < mpoly->totloop; loop++) {
			const MLoop *mloop = &ml[mpoly->loopstart + loop];
			if (mloop->v == vert) {
				vert_faces[num++] = poly;
				break;
			}
		}
	}
}

static void converter_setup_from_derivedmesh(
        DerivedMesh *dm,
        OpenSubdiv_Converter *converter)
{
	converter->get_num_faces = conv_dm_get_num_faces;
	converter->get_num_edges = conv_dm_get_num_edges;
	converter->get_num_verts = conv_dm_get_num_verts;

	converter->get_num_face_verts = conv_dm_get_num_face_verts;
	converter->get_face_verts = conv_dm_get_face_verts;
	converter->get_face_edges = conv_dm_get_face_edges;

	converter->get_edge_verts = conv_dm_get_edge_verts;
	converter->get_num_edge_faces = conv_dm_get_num_edge_faces;
	converter->get_edge_faces = conv_dm_get_edge_faces;

	converter->get_num_vert_edges = conv_dm_get_num_vert_edges;
	converter->get_vert_edges = conv_dm_get_vert_edges;
	converter->get_num_vert_faces = conv_dm_get_num_vert_faces;
	converter->get_vert_faces = conv_dm_get_vert_faces;

	converter->user_data = dm;
}

static bool ccgSubSurf_checkDMTopologyChanged(DerivedMesh *dm, DerivedMesh *dm2)
{
	const int num_verts = dm->getNumVerts(dm);
	const int num_polys = dm->getNumPolys(dm);
	const MPoly *mpoly = dm->getPolyArray(dm);
	const MPoly *mpoly2 = dm2->getPolyArray(dm2);
	const MLoop *mloop = dm->getLoopArray(dm);
	const MLoop *mloop2 = dm2->getLoopArray(dm2);
	int poly_index;

	/* Quick tests based on the number of verts and facces. */
	if (num_verts != dm2->getNumVerts(dm2) ||
	    num_polys != dm2->getNumPolys(dm2))
	{
		return true;
	}

	/* Rather slow check for faces topology change. */
	for (poly_index = 0; poly_index < num_polys; poly_index++, mpoly++, mpoly2++) {
		int S;
		if (mpoly->totloop != mpoly2->totloop) {
			return true;
		}
		for (S = 0; S < mpoly->totloop; ++S) {
			if (mloop[mpoly->loopstart + S].v != mloop2[mpoly2->loopstart + S].v) {
				return true;
			}
		}
	}
	/* TODO(sergey): Check whether crease changed. */
	return false;
}

void ccgSubSurf_setDerivedMesh(CCGSubSurf *ss, DerivedMesh *dm)
{
	if (ss->dm != NULL) {
		if (ccgSubSurf_checkDMTopologyChanged(ss->dm, dm)) {
			ss->osd_topology_changed = true;
		}
		ss->dm->needsFree = 1;
		ss->dm->release(ss->dm);
	}
	ss->dm = dm;
}

static void ccgSubSurf__updateGLMeshCoords(CCGSubSurf *ss)
{
	/* TODO(sergey): This is rather a duplicated work to gather all
	 * the basis coordinates in an array. It also needed to update
	 * evaluator and we somehow should optimize this to positions
	 * are not being packed into an array at draw time.
	 */
	float (*positions)[3];
	int vertDataSize = ss->meshIFC.vertDataSize;
	int normalDataOffset = ss->normalDataOffset;
	int num_basis_verts = ss->vMap->numEntries;
	int i;

	BLI_assert(ss->meshIFC.numLayers == 3);

	positions = MEM_callocN(2 * sizeof(*positions) * num_basis_verts,
	                        "OpenSubdiv coarse points");
#pragma omp parallel for
	for (i = 0; i < ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			float *co = VERT_getCo(v, 0);
			float *no = VERT_getNo(v, 0);
			BLI_assert(v->osd_index < ss->vMap->numEntries);
			VertDataCopy(positions[v->osd_index * 2], co, ss);
			VertDataCopy(positions[v->osd_index * 2 + 1], no, ss);
		}
	}

	openSubdiv_osdGLMeshUpdateVertexBuffer(ss->osd_mesh,
	                                       (float *) positions,
	                                       0,
	                                       num_basis_verts);

	MEM_freeN(positions);
}

bool ccgSubSurf_prepareGLMesh(CCGSubSurf *ss, bool use_osd_glsl)
{
	int compute_type;

	/* Happens for meshes without faces. */
	//if (UNLIKELY(ss->osd_evaluator == NULL)) {
	//	return false;
	//}

	switch (U.opensubdiv_compute_type) {
#define CHECK_COMPUTE_TYPE(type) \
		case USER_OPENSUBDIV_COMPUTE_ ## type: \
			compute_type = OPENSUBDIV_EVALUATOR_ ## type; \
			break;
		CHECK_COMPUTE_TYPE(CPU)
		CHECK_COMPUTE_TYPE(OPENMP)
		CHECK_COMPUTE_TYPE(OPENCL)
		CHECK_COMPUTE_TYPE(CUDA)
		CHECK_COMPUTE_TYPE(GLSL_TRANSFORM_FEEDBACK)
		CHECK_COMPUTE_TYPE(GLSL_COMPUTE)
#undef CHECK_COMPUTE_TYPE
	}

	if (ss->osd_vao == 0) {
		glGenVertexArrays(1, &ss->osd_vao);
	}

	if (ss->osd_mesh_invalid) {
		openSubdiv_deleteOsdGLMesh(ss->osd_mesh);
		ss->osd_mesh = NULL;
		ss->osd_mesh_invalid = false;
	}

	if (ss->osd_mesh == NULL) {
		OpenSubdiv_Converter converter;
		OpenSubdiv_TopologyRefinerDescr *topology_refiner;
		converter_setup_from_derivedmesh(ss->dm, &converter);
		topology_refiner = openSubdiv_createTopologyRefinerDescr(&converter);
		ss->osd_mesh = openSubdiv_createOsdGLMeshFromTopologyRefiner(
		        topology_refiner,
		        compute_type,
		        ss->subdivLevels,
		        OPENSUBDIV_SCHEME_CATMARK,  /* TODO(sergey): Deprecated argument. */
		        ss->osd_subsurf_uv);

		if (UNLIKELY(ss->osd_mesh == NULL)) {
			/* Most likely compute device is not available. */
			return false;
		}

		ccgSubSurf__updateGLMeshCoords(ss);

		openSubdiv_osdGLMeshRefine(ss->osd_mesh);
		openSubdiv_osdGLMeshSynchronize(ss->osd_mesh);

		glBindVertexArray(ss->osd_vao);
		glBindBuffer(GL_ARRAY_BUFFER,
		             openSubdiv_getOsdGLMeshVertexBuffer(ss->osd_mesh));

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		                      sizeof(GLfloat) * 6, 0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
		                      sizeof(GLfloat) * 6, (float*)12);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	else if (ss->osd_coords_invalid) {
		ccgSubSurf__updateGLMeshCoords(ss);
		openSubdiv_osdGLMeshRefine(ss->osd_mesh);
		openSubdiv_osdGLMeshSynchronize(ss->osd_mesh);
		ss->osd_coords_invalid = false;
	}

	openSubdiv_osdGLMeshDisplayPrepare(use_osd_glsl, ss->osd_uv_index);

	return true;
}

void ccgSubSurf_drawGLMesh(CCGSubSurf *ss, bool fill_quads,
                           int start_partition, int num_partitions)
{
	if (LIKELY(ss->osd_mesh != NULL)) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
		             openSubdiv_getOsdGLMeshPatchIndexBuffer(ss->osd_mesh));

		openSubdiv_osdGLMeshBindVertexBuffer(ss->osd_mesh);
		glBindVertexArray(ss->osd_vao);
		openSubdiv_osdGLMeshDisplay(ss->osd_mesh, fill_quads,
		                            start_partition, num_partitions);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

void ccgSubSurf_setSkipGrids(CCGSubSurf *ss, bool skip_grids)
{
	ss->skip_grids = skip_grids;
}

BLI_INLINE void ccgSubSurf__mapGridToFace(int S, float grid_u, float grid_v,
                                          float *face_u, float *face_v)
{
	float u, v;

	/* - Each grid covers half of the face along the edges.
	 * - Grid's (0, 0) starts from the middle of the face.
	 */
	u = 0.5f - 0.5f * grid_u;
	v = 0.5f - 0.5f * grid_v;

	if (S == 0) {
		*face_u = v;
		*face_v = u;
	}
	else if (S == 1) {
		*face_u = 1.0f - u;
		*face_v = v;
	}
	else if (S == 2) {
		*face_u = 1.0f - v;
		*face_v = 1.0f - u;
	}
	else {
		*face_u = u;
		*face_v = 1.0f - v;
	}
}

BLI_INLINE void ccgSubSurf__mapEdgeToFace(int S,
                                          int edge_segment,
                                          bool inverse_edge,
                                          int edgeSize,
                                          float *face_u, float *face_v)
{
	int t = inverse_edge ? edgeSize - edge_segment - 1 : edge_segment;
	if (S == 0) {
		*face_u = (float) t / (edgeSize - 1);
		*face_v = 0.0f;
	}
	else if (S == 1) {
		*face_u = 1.0f;
		*face_v = (float) t / (edgeSize - 1);
	}
	else if (S == 2) {
		*face_u = 1.0f - (float) t / (edgeSize - 1);
		*face_v = 1.0f;
	}
	else {
		*face_u = 0.0f;
		*face_v = 1.0f - (float) t / (edgeSize - 1);
	}
}

void ccgSubSurf_setUVCoordsFromDM(CCGSubSurf *ss,
                                  DerivedMesh *dm,
                                  bool subdivide_uvs)
{
	CustomData *loop_data = &dm->loopData;
	int /*layer,*/ num_layer = CustomData_number_of_layers(loop_data, CD_MLOOPUV);
	bool mpoly_allocated;
	MPoly *mpoly;

	ss->osd_uv_index = CustomData_get_active_layer(&dm->loopData,
	                                               CD_MLOOPUV);

	if (subdivide_uvs != ss->osd_subsurf_uv) {
		ss->osd_uvs_invalid = true;
	}

	if (num_layer == 0 || !ss->osd_uvs_invalid) {
		return;
	}

	ss->osd_uvs_invalid = false;
	ss->osd_subsurf_uv = subdivide_uvs;
	if (ss->osd_mesh) {
		ss->osd_mesh_invalid = true;
	}

	mpoly = DM_get_poly_array(dm, &mpoly_allocated);

	/* TODO(sergey): Need proper port. */
#if 0
	openSubdiv_evaluatorFVDataClear(ss->osd_evaluator);

	for (layer = 0; layer < num_layer; ++layer) {
		openSubdiv_evaluatorFVNamePush(ss->osd_evaluator, "u");
		openSubdiv_evaluatorFVNamePush(ss->osd_evaluator, "v");
	}

	{
		int i;
		for (i = 0; i < ss->fMap->curSize; ++i) {
			CCGFace *face = (CCGFace *) ss->fMap->buckets[i];
			for (; face; face = face->next) {
				int index = GET_INT_FROM_POINTER(ccgSubSurf_getFaceFaceHandle(face));
				MPoly *mp = &mpoly[index];
				int S;
				for (S = 0; S < face->numVerts; ++S) {
					for (layer = 0; layer < num_layer; ++layer) {
						MLoopUV *mloopuv = CustomData_get_layer_n(loop_data,
						                                          CD_MLOOPUV,
						                                          layer);

						MLoopUV *loopuv = &mloopuv[mp->loopstart + S];
						openSubdiv_evaluatorFVDataPush(ss->osd_evaluator,
						                               loopuv->uv[0]);
						openSubdiv_evaluatorFVDataPush(ss->osd_evaluator,
						                               loopuv->uv[1]);
					}
				}
			}
		}
	}
#endif

	if (mpoly_allocated) {
		MEM_freeN(mpoly);
	}
}

static bool check_topology_changed(CCGSubSurf *ss)
{
	if (ss->osd_compute != U.opensubdiv_compute_type) {
		return true;
	}

	return ss->osd_topology_changed;
}

static bool opensubdiv_createEvaluator(CCGSubSurf *ss)
{
	OpenSubdiv_Converter converter;
	OpenSubdiv_TopologyRefinerDescr *topology_refiner;
	converter_setup_from_derivedmesh(ss->dm, &converter);
	topology_refiner = openSubdiv_createTopologyRefinerDescr(&converter);
	ss->osd_compute = U.opensubdiv_compute_type;
	ss->osd_evaluator =
	        openSubdiv_createEvaluatorDescr(topology_refiner,
	                                        ss->subdivLevels);
	return ss->osd_evaluator != NULL;
}

static bool opensubdiv_ensureEvaluator(CCGSubSurf *ss)
{
	if (ss->osd_evaluator != NULL) {
		if (check_topology_changed(ss)) {
			/* If topology changes then we are to re-create evaluator
			 * from the very scratch.
			 */
			openSubdiv_deleteEvaluatorDescr(ss->osd_evaluator);
			ss->osd_evaluator = NULL;

			/* We would also need to re-create gl mesh from sratch
			 * if the topology changes.
			 * Here we only tag for free, actual free should happen
			 * from the main thread.
			 */
			if (ss->osd_mesh) {
				ss->osd_mesh_invalid = true;
			}

			ss->osd_uvs_invalid = true;
		}
	}
	if (ss->osd_evaluator == NULL) {
		int num_basis_verts = ss->vMap->numEntries;
		OSD_LOG("Allocating new evaluator, %d verts\n", num_basis_verts);
		opensubdiv_createEvaluator(ss);
	} else {
		OSD_LOG("Re-using old evaluator\n");
	}
	return ss->osd_evaluator != NULL;
}

static void opensubdiv_updateCoarsePositions(CCGSubSurf *ss)
{
	float (*positions)[3];
	int vertDataSize = ss->meshIFC.vertDataSize;
	int num_basis_verts = ss->vMap->numEntries;
	int i;

	/* TODO(sergey): Avoid allocation on every update. We could either update
	 * coordinates in chunks of 1K vertices (which will only use stack memory)
	 * or do some callback magic for OSD evaluator can invoke it and fill in
	 * buffer directly.
	 */
	if (ss->meshIFC.numLayers == 3) {
		/* If all the components are to be initialized, no need to memset the
		 * new memory block.
		 */
		positions = MEM_mallocN(3 * sizeof(float) * num_basis_verts,
		                        "OpenSubdiv coarse points");
	}
	else {
		/* Calloc in order to have z component initialized to 0 for Uvs */
		positions = MEM_callocN(3 * sizeof(float) * num_basis_verts,
		                        "OpenSubdiv coarse points");
	}
#pragma omp parallel for
	for (i = 0; i < ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			float *co = VERT_getCo(v, 0);
			BLI_assert(v->osd_index < ss->vMap->numEntries);
			VertDataCopy(positions[v->osd_index], co, ss);
			OSD_LOG("Point %d has value %f %f %f\n",
			        v->osd_index,
			        positions[v->osd_index][0],
			        positions[v->osd_index][1],
			        positions[v->osd_index][2]);
		}
	}

	openSubdiv_setEvaluatorCoarsePositions(ss->osd_evaluator,
	                                       (float*)positions,
	                                       0,
	                                       num_basis_verts);

	MEM_freeN(positions);
}

static void opensubdiv_updateCoarseNormals(CCGSubSurf *ss)
{
	int i;
	int normalDataOffset = ss->normalDataOffset;
	int vertDataSize = ss->meshIFC.vertDataSize;

	if (ss->meshIFC.numLayers != 3) {
		return;
	}

#pragma omp parallel for
	for (i = 0; i < ss->vMap->curSize; ++i) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			float *no = VERT_getNo(v, 0);
			zero_v3(no);
		}
	}

#pragma omp parallel for
	for (i = 0; i < ss->fMap->curSize; ++i) {
		CCGFace *f = (CCGFace *) ss->fMap->buckets[i];
		for (; f; f = f->next) {
			int S;
			float face_no[3] = {0.0f, 0.0f, 0.0f};
			CCGVert *v_prev = FACE_getVerts(f)[f->numVerts - 1];
			float *co_prev = VERT_getCo(v_prev, 0);
			for (S = 0; S < f->numVerts; S++) {
				CCGVert *v_curr = FACE_getVerts(f)[S];
				float *co_curr = VERT_getCo(v_curr, 0);
				add_newell_cross_v3_v3v3(face_no, co_prev, co_curr);
				co_prev = co_curr;
			}
			if (UNLIKELY(normalize_v3(face_no) == 0.0f)) {
				face_no[2] = 1.0f; /* other axis set to 0.0 */
			}
#pragma omp critical
			{
				for (S = 0; S < f->numVerts; S++) {
					CCGVert *v = FACE_getVerts(f)[S];
					float *no = VERT_getNo(v, 0);
					add_v3_v3(no, face_no);
				}
			}
		}
	}

#pragma omp parallel for
	for (i = 0; i < ss->vMap->curSize; ++i) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			float *no = VERT_getNo(v, 0);
			normalize_v3(no);
		}
	}
}

static void opensubdiv_evaluateQuadFaceGrids(CCGSubSurf *ss,
                                             CCGFace *face,
                                             const int osd_face_index)
{
	int normalDataOffset = ss->normalDataOffset;
	int subdivLevels = ss->subdivLevels;
	int gridSize = ccg_gridsize(subdivLevels);
	int edgeSize = ccg_edgesize(subdivLevels);
	int vertDataSize = ss->meshIFC.vertDataSize;
	int S;
	bool do_normals = ss->meshIFC.numLayers == 3;

#pragma omp parallel for
	for (S = 0; S < face->numVerts; S++) {
		int x, y, k;
		CCGEdge *edge = NULL;
		bool inverse_edge;

		for (x = 0; x < gridSize; x++) {
			for (y = 0; y < gridSize; y++) {
				float *co = FACE_getIFCo(face, subdivLevels, S, x, y);
				float *no = FACE_getIFNo(face, subdivLevels, S, x, y);
				float grid_u = (float) x / (gridSize - 1),
				      grid_v = (float) y / (gridSize - 1);
				float face_u, face_v;
				float P[3], dPdu[3], dPdv[3];

				ccgSubSurf__mapGridToFace(S, grid_u, grid_v, &face_u, &face_v);

				/* TODO(sergey): Need proper port. */
				openSubdiv_evaluateLimit(ss->osd_evaluator, osd_face_index,
				                         face_u, face_v,
				                         P,
				                         do_normals ? dPdu : NULL,
				                         do_normals ? dPdv : NULL);

				OSD_LOG("face=%d, corner=%d, grid_u=%f, grid_v=%f, face_u=%f, face_v=%f, P=(%f, %f, %f)\n",
				        osd_face_index, S, grid_u, grid_v, face_u, face_v, P[0], P[1], P[2]);

				VertDataCopy(co, P, ss);
				if (do_normals) {
					cross_v3_v3v3(no, dPdu, dPdv);
					normalize_v3(no);
				}

				if (x == gridSize - 1 && y == gridSize - 1) {
					float *vert_co = VERT_getCo(FACE_getVerts(face)[S], subdivLevels);
					VertDataCopy(vert_co, co, ss);
					if (do_normals) {
						float *vert_no = VERT_getNo(FACE_getVerts(face)[S], subdivLevels);
						VertDataCopy(vert_no, no, ss);
					}
				}
				if (S == 0 && x == 0 && y == 0) {
					float *center_co = (float *)FACE_getCenterData(face);
					VertDataCopy(center_co, co, ss);
					if (do_normals) {
						float *center_no = (float *)((byte *)FACE_getCenterData(face) + normalDataOffset);
						VertDataCopy(center_no, no, ss);
					}
				}
			}
		}

		for (x = 0; x < gridSize; x++) {
			VertDataCopy(FACE_getIECo(face, subdivLevels, S, x),
			             FACE_getIFCo(face, subdivLevels, S, x, 0), ss);
			if (do_normals){
				VertDataCopy(FACE_getIENo(face, subdivLevels, S, x),
				             FACE_getIFNo(face, subdivLevels, S, x, 0), ss);
			}
		}

		for (k = 0; k < face->numVerts; k++) {
			CCGEdge *current_edge = FACE_getEdges(face)[k];
			CCGVert **face_verts = FACE_getVerts(face);
			if (current_edge->v0 == face_verts[S] &&
			    current_edge->v1 == face_verts[(S + 1) % face->numVerts])
			{
				edge = current_edge;
				inverse_edge = false;
				break;
			}
			if (current_edge->v1 == face_verts[S] &&
			    current_edge->v0 == face_verts[(S + 1) % face->numVerts])
			{
				edge = current_edge;
				inverse_edge = true;
				break;
			}
		}

		BLI_assert(edge != NULL);

		for (x = 0; x < edgeSize; x++) {
			float u = 0, v = 0;
			float *co = EDGE_getCo(edge, subdivLevels, x);
			float *no = EDGE_getNo(edge, subdivLevels, x);
			float P[3], dPdu[3], dPdv[3];
			ccgSubSurf__mapEdgeToFace(S, x,
			                          inverse_edge,
			                          edgeSize,
			                          &u, &v);

			/* TODO(sergey): Ideally we will re-use grid here, but for now
			 * let's just re-evaluate for simplicity.
			 */
			/* TODO(sergey): Need proper port. */
			openSubdiv_evaluateLimit(ss->osd_evaluator, osd_face_index, u, v, P, dPdu, dPdv);
			VertDataCopy(co, P, ss);
			if (do_normals) {
				cross_v3_v3v3(no, dPdu, dPdv);
				normalize_v3(no);
			}
		}
	}
}

static void opensubdiv_evaluateNGonFaceGrids(CCGSubSurf *ss,
                                             CCGFace *face,
                                             const int osd_face_index)
{
	CCGVert **all_verts = FACE_getVerts(face);
	int normalDataOffset = ss->normalDataOffset;
	int subdivLevels = ss->subdivLevels;
	int gridSize = ccg_gridsize(subdivLevels);
	int edgeSize = ccg_edgesize(subdivLevels);
	int vertDataSize = ss->meshIFC.vertDataSize;
	int S;
	bool do_normals = ss->meshIFC.numLayers == 3;

	/* Note about handling non-quad faces.
	 *
	 * In order to deal with non-quad faces we need to split them
	 * into a quads in the following way:
	 *
	 *                                                     |
	 *                                                (vert_next)
	 *                                                     |
	 *                                                     |
	 *                                                     |
	 *                  (face_center) ------------------- (v2)
	 *                         | (o)-------------------->  |
	 *                         |  |                     v  |
	 *                         |  |                        |
	 *                         |  |                        |
	 *                         |  |                        |
	 *                         |  |                   y ^  |
	 *                         |  |                     |  |
	 *                         |  v  u             x    |  |
	 *                         |                   <---(o) |
	 * ---- (vert_prev) ---- (v1)  --------------------  (vert)
	 *
	 * This is how grids are expected to be stored and it's how
	 * OpenSubdiv deals with non-quad faces using ptex face indices.
	 * We only need to convert ptex (x, y) to grid (u, v) by some
	 * simple flips and evaluate the ptex face.
	 */

	/* Evaluate face grids. */
#pragma omp parallel for
	for (S = 0; S < face->numVerts; S++) {
		int x, y;
		for (x = 0; x < gridSize; x++) {
			for (y = 0; y < gridSize; y++) {
				float *co = FACE_getIFCo(face, subdivLevels, S, x, y);
				float *no = FACE_getIFNo(face, subdivLevels, S, x, y);
				float u = 1.0f - (float) y / (gridSize - 1),
				      v = 1.0f - (float) x / (gridSize - 1);
				float P[3], dPdu[3], dPdv[3];

				/* TODO(sergey): Need proper port. */
				openSubdiv_evaluateLimit(ss->osd_evaluator, osd_face_index + S, u, v, P, dPdu, dPdv);

				OSD_LOG("face=%d, corner=%d, u=%f, v=%f, P=(%f, %f, %f)\n",
				        osd_face_index + S, S, u, v, P[0], P[1], P[2]);

				VertDataCopy(co, P, ss);
				if (do_normals) {
					cross_v3_v3v3(no, dPdu, dPdv);
					normalize_v3(no);
				}

				/* TODO(sergey): De-dpuplicate with the quad case. */
				if (x == gridSize - 1 && y == gridSize - 1) {
					float *vert_co = VERT_getCo(FACE_getVerts(face)[S], subdivLevels);
					VertDataCopy(vert_co, co, ss);
					if (do_normals) {
						float *vert_no = VERT_getNo(FACE_getVerts(face)[S], subdivLevels);
						VertDataCopy(vert_no, no, ss);
					}
				}
				if (S == 0 && x == 0 && y == 0) {
					float *center_co = (float *)FACE_getCenterData(face);
					VertDataCopy(center_co, co, ss);
					if (do_normals) {
						float *center_no = (float *)((byte *)FACE_getCenterData(face) + normalDataOffset);
						VertDataCopy(center_no, no, ss);
					}
				}
			}
		}
		for (x = 0; x < gridSize; x++) {
			VertDataCopy(FACE_getIECo(face, subdivLevels, S, x),
			             FACE_getIFCo(face, subdivLevels, S, x, 0), ss);
			if (do_normals) {
				VertDataCopy(FACE_getIENo(face, subdivLevels, S, x),
				             FACE_getIFNo(face, subdivLevels, S, x, 0), ss);
			}
		}
	}

	/* Evaluate edges. */
	for (S = 0; S < face->numVerts; S++) {
		CCGEdge *edge = FACE_getEdges(face)[S];
		int x, S0, S1;
		bool flip;

		for (x = 0; x < face->numVerts; ++x) {
			if (all_verts[x] == edge->v0) {
				S0 = x;
			}
			else if (all_verts[x] == edge->v1) {
				S1 = x;
			}
		}
		if (S == face->numVerts - 1) {
			flip = S0 > S1;
		}
		else {
			flip = S0 < S1;
		}

		for (x = 0; x <= edgeSize / 2; x++) {
			float *edge_co = EDGE_getCo(edge, subdivLevels, x);
			float *edge_no = EDGE_getNo(edge, subdivLevels, x);
			float *face_edge_co;
			float *face_edge_no;
			if (flip) {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S0, gridSize - 1, gridSize - 1 - x);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S0, gridSize - 1, gridSize - 1 - x);
			}
			else {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S0, gridSize - 1 - x, gridSize - 1);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S0, gridSize - 1 - x, gridSize - 1);
			}
			VertDataCopy(edge_co, face_edge_co, ss);
			if (do_normals) {
				VertDataCopy(edge_no, face_edge_no, ss);
			}
		}
		for (x = edgeSize / 2 + 1; x < edgeSize; x++) {
			float *edge_co = EDGE_getCo(edge, subdivLevels, x);
			float *edge_no = EDGE_getNo(edge, subdivLevels, x);
			float *face_edge_co;
			float *face_edge_no;
			if (flip) {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S1, x - edgeSize / 2, gridSize - 1);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S1, x - edgeSize / 2, gridSize - 1);
			}
			else {
				face_edge_co = FACE_getIFCo(face, subdivLevels, S1, gridSize - 1, x - edgeSize / 2);
				face_edge_no = FACE_getIFNo(face, subdivLevels, S1, gridSize - 1, x - edgeSize / 2);
			}
			VertDataCopy(edge_co, face_edge_co, ss);
			if (do_normals) {
				VertDataCopy(edge_no, face_edge_no, ss);
			}
		}
	}
}

static void opensubdiv_evaluateGrids(CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->fMap->curSize; i++) {
		CCGFace *face = (CCGFace *) ss->fMap->buckets[i];
		for (; face; face = face->next) {
			if (face->numVerts == 4) {
				/* For quads we do special magic with converting face coords
				 * into corner coords and interpolating grids from it.
				 */
				opensubdiv_evaluateQuadFaceGrids(ss, face, face->osd_index);
			}
			else {
				/* NGons and tris are split into separate osd faces which
				 * evaluates onto grids directly.
				 */
				opensubdiv_evaluateNGonFaceGrids(ss, face, face->osd_index);
			}
		}
	}
}

void ccgSubSurf__sync_opensubdiv(CCGSubSurf *ss)
{
	BLI_assert(ss->meshIFC.numLayers == 2 || ss->meshIFC.numLayers == 3);

	/* TODO(sergey): Apparently it's not supported by OpenSubdiv. */
	if (ss->fMap->numEntries == 0) {
		return;
	}

	ss->osd_coords_invalid = true;

	/* Make sure OSD evaluator is up-to-date. */
	if (ss->skip_grids == false) {
		if (opensubdiv_ensureEvaluator(ss)) {
			/* Update coarse points in the OpenSubdiv evaluator. */
			opensubdiv_updateCoarsePositions(ss);

			/* Evaluate opensubdiv mesh into the CCG grids. */
			opensubdiv_evaluateGrids(ss);
		}
		else {
			BLI_assert(!"OpenSubdiv initializetion failed, should not happen.");
		}
	}
	else {
		BLI_assert(ss->meshIFC.numLayers == 3);
		/* TODO(sergey): De-duplicate with the case of evalautor. */
		if (check_topology_changed(ss)) {
			if (ss->osd_mesh) {
				ss->osd_mesh_invalid = true;
			}
			ss->osd_uvs_invalid = true;
			ss->osd_topology_changed = false;
			ss->osd_compute = U.opensubdiv_compute_type;
		}
		opensubdiv_updateCoarseNormals(ss);
	}

#ifdef DUMP_RESULT_GRIDS
	ccgSubSurf__dumpCoords(ss);
#endif
}

#undef OSD_LOG

#endif  /* WITH_OPENSUBDIV */
