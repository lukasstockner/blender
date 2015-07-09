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

/** \file blender/blenkernel/intern/CCGSubSurf.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h" // for intptr_t support

#include "BLI_utildefines.h" /* for BLI_assert */
#include "BLI_math.h"

#include "BKE_ccg.h"
#include "CCGSubSurf.h"
#include "BKE_subsurf.h"

#include "BKE_DerivedMesh.h"
#include "BKE_subsurf.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"

#ifdef WITH_OPENSUBDIV
#  include "opensubdiv_capi.h"
//#  include <opensubdiv/osdutil/evaluator_capi.h>
#endif

#include "GL/glew.h"

/* Define this to see dump of the grids after the subsurf applied. */
#undef DUMP_RESULT_GRIDS

/* used for normalize_v3 in BLI_math_vector
 * float.h's FLT_EPSILON causes trouble with subsurf normals - campbell */
#define EPSILON (1.0e-35f)

/* With this limit a single triangle becomes over 3 million faces */
#define CCGSUBSURF_LEVEL_MAX 11

/***/

typedef unsigned char byte;

/***/

static int kHashSizes[] = {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459
};

typedef struct _EHEntry EHEntry;
struct _EHEntry {
	EHEntry *next;
	void *key;
};
typedef struct _EHash {
	EHEntry **buckets;
	int numEntries, curSize, curSizeIdx;

	CCGAllocatorIFC allocatorIFC;
	CCGAllocatorHDL allocator;
} EHash;

#define EHASH_alloc(eh, nb)     ((eh)->allocatorIFC.alloc((eh)->allocator, nb))
#define EHASH_free(eh, ptr)     ((eh)->allocatorIFC.free((eh)->allocator, ptr))

#define EHASH_hash(eh, item)    (((uintptr_t) (item)) % ((unsigned int) (eh)->curSize))

static void ccgSubSurf__sync(CCGSubSurf *ss);
static int _edge_isBoundary(const CCGEdge *e);

static EHash *_ehash_new(int estimatedNumEntries, CCGAllocatorIFC *allocatorIFC, CCGAllocatorHDL allocator)
{
	EHash *eh = allocatorIFC->alloc(allocator, sizeof(*eh));
	eh->allocatorIFC = *allocatorIFC;
	eh->allocator = allocator;
	eh->numEntries = 0;
	eh->curSizeIdx = 0;
	while (kHashSizes[eh->curSizeIdx] < estimatedNumEntries)
		eh->curSizeIdx++;
	eh->curSize = kHashSizes[eh->curSizeIdx];
	eh->buckets = EHASH_alloc(eh, eh->curSize * sizeof(*eh->buckets));
	memset(eh->buckets, 0, eh->curSize * sizeof(*eh->buckets));

	return eh;
}
typedef void (*EHEntryFreeFP)(EHEntry *, void *);
static void _ehash_free(EHash *eh, EHEntryFreeFP freeEntry, void *userData)
{
	int numBuckets = eh->curSize;

	while (numBuckets--) {
		EHEntry *entry = eh->buckets[numBuckets];

		while (entry) {
			EHEntry *next = entry->next;

			freeEntry(entry, userData);

			entry = next;
		}
	}

	EHASH_free(eh, eh->buckets);
	EHASH_free(eh, eh);
}

static void _ehash_insert(EHash *eh, EHEntry *entry)
{
	int numBuckets = eh->curSize;
	int hash = EHASH_hash(eh, entry->key);
	entry->next = eh->buckets[hash];
	eh->buckets[hash] = entry;
	eh->numEntries++;

	if (UNLIKELY(eh->numEntries > (numBuckets * 3))) {
		EHEntry **oldBuckets = eh->buckets;
		eh->curSize = kHashSizes[++eh->curSizeIdx];
		
		eh->buckets = EHASH_alloc(eh, eh->curSize * sizeof(*eh->buckets));
		memset(eh->buckets, 0, eh->curSize * sizeof(*eh->buckets));

		while (numBuckets--) {
			for (entry = oldBuckets[numBuckets]; entry; ) {
				EHEntry *next = entry->next;
				
				hash = EHASH_hash(eh, entry->key);
				entry->next = eh->buckets[hash];
				eh->buckets[hash] = entry;
				
				entry = next;
			}
		}

		EHASH_free(eh, oldBuckets);
	}
}

static void *_ehash_lookupWithPrev(EHash *eh, void *key, void ***prevp_r)
{
	int hash = EHASH_hash(eh, key);
	void **prevp = (void **) &eh->buckets[hash];
	EHEntry *entry;
	
	for (; (entry = *prevp); prevp = (void **) &entry->next) {
		if (entry->key == key) {
			*prevp_r = (void **) prevp;
			return entry;
		}
	}
	
	return NULL;
}

static void *_ehash_lookup(EHash *eh, void *key)
{
	int hash = EHASH_hash(eh, key);
	EHEntry *entry;
	
	for (entry = eh->buckets[hash]; entry; entry = entry->next)
		if (entry->key == key)
			break;
	
	return entry;
}

/**/

static void _ehashIterator_init(EHash *eh, EHashIterator *ehi)
{
	/* fill all members */
	ehi->eh = eh;
	ehi->curBucket = -1;
	ehi->curEntry = NULL;

	while (!ehi->curEntry) {
		ehi->curBucket++;
		if (ehi->curBucket == ehi->eh->curSize)
			break;
		ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
	}
}

static void *_ehashIterator_getCurrent(EHashIterator *ehi)
{
	return ehi->curEntry;
}

static void _ehashIterator_next(EHashIterator *ehi)
{
	if (ehi->curEntry) {
		ehi->curEntry = ehi->curEntry->next;
		while (!ehi->curEntry) {
			ehi->curBucket++;
			if (ehi->curBucket == ehi->eh->curSize)
				break;
			ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
		}
	}
}
static int _ehashIterator_isStopped(EHashIterator *ehi)
{
	return !ehi->curEntry;
}

/***/

static void *_stdAllocator_alloc(CCGAllocatorHDL UNUSED(a), int numBytes)
{
	return MEM_mallocN(numBytes, "CCG standard alloc");
}
static void *_stdAllocator_realloc(CCGAllocatorHDL UNUSED(a), void *ptr, int newSize, int UNUSED(oldSize))
{
	return MEM_reallocN(ptr, newSize);
}
static void _stdAllocator_free(CCGAllocatorHDL UNUSED(a), void *ptr)
{
	MEM_freeN(ptr);
}

static CCGAllocatorIFC *_getStandardAllocatorIFC(void)
{
	static CCGAllocatorIFC ifc;

	ifc.alloc = _stdAllocator_alloc;
	ifc.realloc = _stdAllocator_realloc;
	ifc.free = _stdAllocator_free;
	ifc.release = NULL;

	return &ifc;
}

/***/

BLI_INLINE int ccg_gridsize(int level)
{
	BLI_assert(level > 0);
	BLI_assert(level <= CCGSUBSURF_LEVEL_MAX + 1);

	return (1 << (level - 1)) + 1;
}

int BKE_ccg_gridsize(int level)
{
	return ccg_gridsize(level);
}

int BKE_ccg_factor(int low_level, int high_level)
{
	BLI_assert(low_level > 0 && high_level > 0);
	BLI_assert(low_level <= high_level);

	return 1 << (high_level - low_level);
}

BLI_INLINE int ccg_edgesize(int level)
{
	BLI_assert(level > 0);
	BLI_assert(level <= CCGSUBSURF_LEVEL_MAX + 1);
	
	return 1 + (1 << level);
}

BLI_INLINE int ccg_spacing(int high_level, int low_level)
{
	BLI_assert(high_level > 0 && low_level > 0);
	BLI_assert(high_level >= low_level);
	BLI_assert((high_level - low_level) <= CCGSUBSURF_LEVEL_MAX);

	return 1 << (high_level - low_level);
}

BLI_INLINE int ccg_edgebase(int level)
{
	BLI_assert(level > 0);
	BLI_assert(level <= CCGSUBSURF_LEVEL_MAX + 1);

	return level + (1 << level) - 1;
}

/***/

#define NormZero(av)     { float *_a = (float *) av; _a[0] = _a[1] = _a[2] = 0.0f; } (void)0
#define NormCopy(av, bv) { float *_a = (float *) av, *_b = (float *) bv; _a[0]  = _b[0]; _a[1]  = _b[1]; _a[2]  = _b[2]; } (void)0
#define NormAdd(av, bv)  { float *_a = (float *) av, *_b = (float *) bv; _a[0] += _b[0]; _a[1] += _b[1]; _a[2] += _b[2]; } (void)0

BLI_INLINE void Normalize(float no[3])
{
	const float length = sqrtf(no[0] * no[0] + no[1] * no[1] + no[2] * no[2]);

	if (length > EPSILON) {
		const float length_inv = 1.0f / length;

		no[0] *= length_inv;
		no[1] *= length_inv;
		no[2] *= length_inv;
	}
	else {
		NormZero(no);
	}
}

/***/

enum {
	Vert_eEffected =    (1 << 0),
	Vert_eChanged =     (1 << 1),
	Vert_eSeam =        (1 << 2)
} /*VertFlags*/;
enum {
	Edge_eEffected =    (1 << 0)
} /*CCGEdgeFlags*/;
enum {
	Face_eEffected =    (1 << 0)
} /*FaceFlags*/;

struct CCGVert {
	CCGVert     *next;  /* EHData.next */
	CCGVertHDL vHDL;    /* EHData.key */

	short numEdges, numFaces, flags;
	int osd_index;  /* Index of the vertex in the map, used by OSD. */

	CCGEdge **edges;
	CCGFace **faces;
//	byte *levelData;
//	byte *userData;
};

BLI_INLINE byte *VERT_getLevelData(CCGVert *v)
{
	return (byte *)(&(v)[1]);
}

struct CCGEdge {
	CCGEdge     *next;  /* EHData.next */
	CCGEdgeHDL eHDL;    /* EHData.key */

	short numFaces, flags;
	float crease;

	CCGVert *v0, *v1;
	CCGFace **faces;

//	byte *levelData;
//	byte *userData;
};

BLI_INLINE byte *EDGE_getLevelData(CCGEdge *e)
{
	return (byte *)(&(e)[1]);
}

struct CCGFace {
	CCGFace     *next;  /* EHData.next */
	CCGFaceHDL fHDL;    /* EHData.key */

	short numVerts, flags;
	int osd_index;

//	CCGVert **verts;
//	CCGEdge **edges;
//	byte *centerData;
//	byte **gridData;
//	byte *userData;
};

BLI_INLINE CCGVert **FACE_getVerts(CCGFace *f)
{
	return (CCGVert **)(&f[1]);
}

BLI_INLINE CCGEdge **FACE_getEdges(CCGFace *f)
{
	return (CCGEdge **)(&(FACE_getVerts(f)[f->numVerts]));
}

BLI_INLINE byte *FACE_getCenterData(CCGFace *f)
{
	return (byte *)(&(FACE_getEdges(f)[(f)->numVerts]));
}

typedef enum {
	eSyncState_None = 0,
	eSyncState_Vert,
	eSyncState_Edge,
	eSyncState_Face,
	eSyncState_Partial
} SyncState;

struct CCGSubSurf {
	EHash *vMap;    /* map of CCGVertHDL -> Vert */
	EHash *eMap;    /* map of CCGEdgeHDL -> Edge */
	EHash *fMap;    /* map of CCGFaceHDL -> Face */

	CCGMeshIFC meshIFC;
	
	CCGAllocatorIFC allocatorIFC;
	CCGAllocatorHDL allocator;

	int subdivLevels;
	int numGrids;
	int allowEdgeCreation;
	float defaultCreaseValue;
	void *defaultEdgeUserData;

	void *q, *r;
		
	/* data for calc vert normals */
	int calcVertNormals;
	int normalDataOffset;

	/* data for paint masks */
	int allocMask;
	int maskDataOffset;

	/* data for age'ing (to debug sync) */
	int currentAge;
	int useAgeCounts;
	int vertUserAgeOffset;
	int edgeUserAgeOffset;
	int faceUserAgeOffset;

	/* data used during syncing */
	SyncState syncState;

	EHash *oldVMap, *oldEMap, *oldFMap;
	int lenTempArrays;
	CCGVert **tempVerts;
	CCGEdge **tempEdges;

#ifdef WITH_OPENSUBDIV
	struct OpenSubdiv_EvaluatorDescr *osd_evaluator;
	struct OpenSubdiv_GLMesh *osd_mesh;
	bool osd_mesh_invalid, osd_coords_invalid;
	unsigned int osd_vao;
	bool skip_grids;
	short osd_compute;

	bool osd_uvs_invalid;
	bool osd_subsurf_uv;
	int osd_uv_index;
	int osd_next_face_index;

	DerivedMesh *dm;
#endif
};

#define CCGSUBSURF_alloc(ss, nb)            ((ss)->allocatorIFC.alloc((ss)->allocator, nb))
#define CCGSUBSURF_realloc(ss, ptr, nb, ob) ((ss)->allocatorIFC.realloc((ss)->allocator, ptr, nb, ob))
#define CCGSUBSURF_free(ss, ptr)            ((ss)->allocatorIFC.free((ss)->allocator, ptr))

/***/

static int VertDataEqual(const float a[], const float b[], const CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->meshIFC.numLayers; i++) {
		if (a[i] != b[i])
			return 0;
	}
	return 1;
}

static void VertDataZero(float v[], const CCGSubSurf *ss)
{
	memset(v, 0, sizeof(float) * ss->meshIFC.numLayers);
}

static void VertDataCopy(float dst[], const float src[], const CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->meshIFC.numLayers; i++)
		dst[i] = src[i];
}

static void VertDataAdd(float a[], const float b[], const CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->meshIFC.numLayers; i++)
		a[i] += b[i];
}

static void VertDataSub(float a[], const float b[], const CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->meshIFC.numLayers; i++)
		a[i] -= b[i];
}

static void VertDataMulN(float v[], float f, const CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->meshIFC.numLayers; i++)
		v[i] *= f;
}

static void VertDataAvg4(float v[],
                         const float a[], const float b[],
                         const float c[], const float d[],
                         const CCGSubSurf *ss)
{
	int i;
	for (i = 0; i < ss->meshIFC.numLayers; i++)
		v[i] = (a[i] + b[i] + c[i] + d[i]) * 0.25f;
}

/***/

static CCGVert *_vert_new(CCGVertHDL vHDL, CCGSubSurf *ss)
{
	int num_vert_data = ss->subdivLevels + 1;
	CCGVert *v = CCGSUBSURF_alloc(ss,
	                              sizeof(CCGVert) +
	                              ss->meshIFC.vertDataSize * num_vert_data +
	                              ss->meshIFC.vertUserSize);
	byte *userData;

	v->vHDL = vHDL;
	v->edges = NULL;
	v->faces = NULL;
	v->numEdges = v->numFaces = 0;
	v->flags = 0;

	userData = ccgSubSurf_getVertUserData(ss, v);
	memset(userData, 0, ss->meshIFC.vertUserSize);
	if (ss->useAgeCounts) *((int *) &userData[ss->vertUserAgeOffset]) = ss->currentAge;

	return v;
}
static void _vert_remEdge(CCGVert *v, CCGEdge *e)
{
	int i;
	for (i = 0; i < v->numEdges; i++) {
		if (v->edges[i] == e) {
			v->edges[i] = v->edges[--v->numEdges];
			break;
		}
	}
}
static void _vert_remFace(CCGVert *v, CCGFace *f)
{
	int i;
	for (i = 0; i < v->numFaces; i++) {
		if (v->faces[i] == f) {
			v->faces[i] = v->faces[--v->numFaces];
			break;
		}
	}
}
static void _vert_addEdge(CCGVert *v, CCGEdge *e, CCGSubSurf *ss)
{
	v->edges = CCGSUBSURF_realloc(ss, v->edges, (v->numEdges + 1) * sizeof(*v->edges), v->numEdges * sizeof(*v->edges));
	v->edges[v->numEdges++] = e;
}
static void _vert_addFace(CCGVert *v, CCGFace *f, CCGSubSurf *ss)
{
	v->faces = CCGSUBSURF_realloc(ss, v->faces, (v->numFaces + 1) * sizeof(*v->faces), v->numFaces * sizeof(*v->faces));
	v->faces[v->numFaces++] = f;
}
static CCGEdge *_vert_findEdgeTo(const CCGVert *v, const CCGVert *vQ)
{
	int i;
	for (i = 0; i < v->numEdges; i++) {
		CCGEdge *e = v->edges[v->numEdges - 1 - i]; // XXX, note reverse
		if ((e->v0 == v && e->v1 == vQ) ||
		    (e->v1 == v && e->v0 == vQ))
		{
			return e;
		}
	}
	return NULL;
}
static int _vert_isBoundary(const CCGVert *v)
{
	int i;
	for (i = 0; i < v->numEdges; i++)
		if (_edge_isBoundary(v->edges[i]))
			return 1;
	return 0;
}

static void *_vert_getCo(CCGVert *v, int lvl, int dataSize)
{
	return &VERT_getLevelData(v)[lvl * dataSize];
}
static float *_vert_getNo(CCGVert *v, int lvl, int dataSize, int normalDataOffset)
{
	return (float *) &VERT_getLevelData(v)[lvl * dataSize + normalDataOffset];
}

static void _vert_free(CCGVert *v, CCGSubSurf *ss)
{
	if (v->edges) {
		CCGSUBSURF_free(ss, v->edges);
	}

	if (v->faces) {
		CCGSUBSURF_free(ss, v->faces);
	}

	CCGSUBSURF_free(ss, v);
}

static int VERT_seam(const CCGVert *v)
{
	return ((v->flags & Vert_eSeam) != 0);
}

/***/

static CCGEdge *_edge_new(CCGEdgeHDL eHDL, CCGVert *v0, CCGVert *v1, float crease, CCGSubSurf *ss)
{
	int num_edge_data = ccg_edgebase(ss->subdivLevels + 1);
	CCGEdge *e = CCGSUBSURF_alloc(ss,
	                              sizeof(CCGEdge) +
	                              ss->meshIFC.vertDataSize * num_edge_data +
	                              ss->meshIFC.edgeUserSize);
	byte *userData;

	e->eHDL = eHDL;
	e->v0 = v0;
	e->v1 = v1;
	e->crease = crease;
	e->faces = NULL;
	e->numFaces = 0;
	e->flags = 0;
	_vert_addEdge(v0, e, ss);
	_vert_addEdge(v1, e, ss);

	userData = ccgSubSurf_getEdgeUserData(ss, e);
	memset(userData, 0, ss->meshIFC.edgeUserSize);
	if (ss->useAgeCounts) *((int *) &userData[ss->edgeUserAgeOffset]) = ss->currentAge;

	return e;
}
static void _edge_remFace(CCGEdge *e, CCGFace *f)
{
	int i;
	for (i = 0; i < e->numFaces; i++) {
		if (e->faces[i] == f) {
			e->faces[i] = e->faces[--e->numFaces];
			break;
		}
	}
}
static void _edge_addFace(CCGEdge *e, CCGFace *f, CCGSubSurf *ss)
{
	e->faces = CCGSUBSURF_realloc(ss, e->faces, (e->numFaces + 1) * sizeof(*e->faces), e->numFaces * sizeof(*e->faces));
	e->faces[e->numFaces++] = f;
}
static int _edge_isBoundary(const CCGEdge *e)
{
	return e->numFaces < 2;
}

#ifndef WITH_OPENSUBDIV
static CCGVert *_edge_getOtherVert(CCGEdge *e, CCGVert *vQ)
{
	if (vQ == e->v0) {
		return e->v1;
	}
	else {
		return e->v0;
	}
}
#endif

static void *_edge_getCo(CCGEdge *e, int lvl, int x, int dataSize)
{
	int levelBase = ccg_edgebase(lvl);
	return &EDGE_getLevelData(e)[dataSize * (levelBase + x)];
}
static float *_edge_getNo(CCGEdge *e, int lvl, int x, int dataSize, int normalDataOffset)
{
	int levelBase = ccg_edgebase(lvl);
	return (float *) &EDGE_getLevelData(e)[dataSize * (levelBase + x) + normalDataOffset];
}
static void *_edge_getCoVert(CCGEdge *e, CCGVert *v, int lvl, int x, int dataSize)
{
	int levelBase = ccg_edgebase(lvl);
	if (v == e->v0) {
		return &EDGE_getLevelData(e)[dataSize * (levelBase + x)];
	}
	else {
		return &EDGE_getLevelData(e)[dataSize * (levelBase + (1 << lvl) - x)];
	}
}

static void _edge_free(CCGEdge *e, CCGSubSurf *ss)
{
	if (e->faces) {
		CCGSUBSURF_free(ss, e->faces);
	}

	CCGSUBSURF_free(ss, e);
}
static void _edge_unlinkMarkAndFree(CCGEdge *e, CCGSubSurf *ss)
{
	_vert_remEdge(e->v0, e);
	_vert_remEdge(e->v1, e);
	e->v0->flags |= Vert_eEffected;
	e->v1->flags |= Vert_eEffected;
	_edge_free(e, ss);
}

static float EDGE_getSharpness(CCGEdge *e, int lvl)
{
	if (!lvl)
		return e->crease;
	else if (!e->crease)
		return 0.0f;
	else if (e->crease - lvl < 0.0f)
		return 0.0f;
	else
		return e->crease - lvl;
}

static CCGFace *_face_new(CCGFaceHDL fHDL, CCGVert **verts, CCGEdge **edges, int numVerts, CCGSubSurf *ss)
{
	int maxGridSize = ccg_gridsize(ss->subdivLevels);
	int num_face_data = (numVerts * maxGridSize +
	                     numVerts * maxGridSize * maxGridSize + 1);
	CCGFace *f = CCGSUBSURF_alloc(ss,
	                              sizeof(CCGFace) +
	                              sizeof(CCGVert *) * numVerts +
	                              sizeof(CCGEdge *) * numVerts +
	                              ss->meshIFC.vertDataSize * num_face_data +
	                              ss->meshIFC.faceUserSize);
	byte *userData;
	int i;

	f->numVerts = numVerts;
	f->fHDL = fHDL;
	f->flags = 0;

	for (i = 0; i < numVerts; i++) {
		FACE_getVerts(f)[i] = verts[i];
		FACE_getEdges(f)[i] = edges[i];
		_vert_addFace(verts[i], f, ss);
		_edge_addFace(edges[i], f, ss);
	}

	userData = ccgSubSurf_getFaceUserData(ss, f);
	memset(userData, 0, ss->meshIFC.faceUserSize);
	if (ss->useAgeCounts) *((int *) &userData[ss->faceUserAgeOffset]) = ss->currentAge;

	return f;
}

BLI_INLINE void *_face_getIECo(CCGFace *f, int lvl, int S, int x, int levels, int dataSize)
{
	int maxGridSize = ccg_gridsize(levels);
	int spacing = ccg_spacing(levels, lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize * (1 + S * (maxGridSize + maxGridSize * maxGridSize));
	return &gridBase[dataSize * x * spacing];
}
BLI_INLINE void *_face_getIENo(CCGFace *f, int lvl, int S, int x, int levels, int dataSize, int normalDataOffset)
{
	int maxGridSize = ccg_gridsize(levels);
	int spacing = ccg_spacing(levels, lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize * (1 + S * (maxGridSize + maxGridSize * maxGridSize));
	return &gridBase[dataSize * x * spacing + normalDataOffset];
}
BLI_INLINE void *_face_getIFCo(CCGFace *f, int lvl, int S, int x, int y, int levels, int dataSize)
{
	int maxGridSize = ccg_gridsize(levels);
	int spacing = ccg_spacing(levels, lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize * (1 + S * (maxGridSize + maxGridSize * maxGridSize));
	return &gridBase[dataSize * (maxGridSize + (y * maxGridSize + x) * spacing)];
}
BLI_INLINE float *_face_getIFNo(CCGFace *f, int lvl, int S, int x, int y, int levels, int dataSize, int normalDataOffset)
{
	int maxGridSize = ccg_gridsize(levels);
	int spacing = ccg_spacing(levels, lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize * (1 + S * (maxGridSize + maxGridSize * maxGridSize));
	return (float *) &gridBase[dataSize * (maxGridSize + (y * maxGridSize + x) * spacing) + normalDataOffset];
}
BLI_INLINE int _face_getVertIndex(CCGFace *f, CCGVert *v)
{
	int i;
	for (i = 0; i < f->numVerts; i++)
		if (FACE_getVerts(f)[i] == v)
			return i;
	return -1;
}
BLI_INLINE int _face_getEdgeIndex(CCGFace *f, CCGEdge *e)
{
	int i;
	for (i = 0; i < f->numVerts; i++)
		if (FACE_getEdges(f)[i] == e)
			return i;
	return -1;
}
BLI_INLINE void *_face_getIFCoEdge(CCGFace *f, CCGEdge *e, int f_ed_idx, int lvl, int eX, int eY, int levels, int dataSize)
{
	int maxGridSize = ccg_gridsize(levels);
	int spacing = ccg_spacing(levels, lvl);
	int x, y, cx, cy;

	BLI_assert(f_ed_idx == _face_getEdgeIndex(f, e));

	eX = eX * spacing;
	eY = eY * spacing;
	if (e->v0 != FACE_getVerts(f)[f_ed_idx]) {
		eX = (maxGridSize * 2 - 1) - 1 - eX;
	}
	y = maxGridSize - 1 - eX;
	x = maxGridSize - 1 - eY;
	if (x < 0) {
		f_ed_idx = (f_ed_idx + f->numVerts - 1) % f->numVerts;
		cx = y;
		cy = -x;
	}
	else if (y < 0) {
		f_ed_idx = (f_ed_idx + 1) % f->numVerts;
		cx = -y;
		cy = x;
	}
	else {
		cx = x;
		cy = y;
	}
	return _face_getIFCo(f, levels, f_ed_idx, cx, cy, levels, dataSize);
}
static float *_face_getIFNoEdge(CCGFace *f, CCGEdge *e, int f_ed_idx, int lvl, int eX, int eY, int levels, int dataSize, int normalDataOffset)
{
	return (float *) ((byte *) _face_getIFCoEdge(f, e, f_ed_idx, lvl, eX, eY, levels, dataSize) + normalDataOffset);
}
static void _face_calcIFNo(CCGFace *f, int lvl, int S, int x, int y, float no[3], int levels, int dataSize)
{
	float *a = _face_getIFCo(f, lvl, S, x + 0, y + 0, levels, dataSize);
	float *b = _face_getIFCo(f, lvl, S, x + 1, y + 0, levels, dataSize);
	float *c = _face_getIFCo(f, lvl, S, x + 1, y + 1, levels, dataSize);
	float *d = _face_getIFCo(f, lvl, S, x + 0, y + 1, levels, dataSize);
	float a_cX = c[0] - a[0], a_cY = c[1] - a[1], a_cZ = c[2] - a[2];
	float b_dX = d[0] - b[0], b_dY = d[1] - b[1], b_dZ = d[2] - b[2];

	no[0] = b_dY * a_cZ - b_dZ * a_cY;
	no[1] = b_dZ * a_cX - b_dX * a_cZ;
	no[2] = b_dX * a_cY - b_dY * a_cX;

	Normalize(no);
}

static void _face_free(CCGFace *f, CCGSubSurf *ss)
{
	CCGSUBSURF_free(ss, f);
}
static void _face_unlinkMarkAndFree(CCGFace *f, CCGSubSurf *ss)
{
	int j;
	for (j = 0; j < f->numVerts; j++) {
		_vert_remFace(FACE_getVerts(f)[j], f);
		_edge_remFace(FACE_getEdges(f)[j], f);
		FACE_getVerts(f)[j]->flags |= Vert_eEffected;
	}
	_face_free(f, ss);
}

/***/

CCGSubSurf *ccgSubSurf_new(CCGMeshIFC *ifc, int subdivLevels, CCGAllocatorIFC *allocatorIFC, CCGAllocatorHDL allocator)
{
	if (!allocatorIFC) {
		allocatorIFC = _getStandardAllocatorIFC();
		allocator = NULL;
	}

	if (subdivLevels < 1) {
		return NULL;
	}
	else {
		CCGSubSurf *ss = allocatorIFC->alloc(allocator, sizeof(*ss));

		ss->allocatorIFC = *allocatorIFC;
		ss->allocator = allocator;

		ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);

		ss->meshIFC = *ifc;
		
		ss->subdivLevels = subdivLevels;
		ss->numGrids = 0;
		ss->allowEdgeCreation = 0;
		ss->defaultCreaseValue = 0;
		ss->defaultEdgeUserData = NULL;

		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;

		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;

		ss->allocMask = 0;

		ss->q = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);
		ss->r = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);

		ss->currentAge = 0;

		ss->syncState = eSyncState_None;

		ss->oldVMap = ss->oldEMap = ss->oldFMap = NULL;
		ss->lenTempArrays = 0;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;

#ifdef WITH_OPENSUBDIV
		ss->osd_evaluator = NULL;
		ss->osd_mesh = NULL;
		ss->osd_mesh_invalid = false;
		ss->osd_coords_invalid = false;
		ss->osd_vao = 0;
		ss->skip_grids = false;
		ss->osd_compute = 0;
		ss->osd_uvs_invalid = true;
		ss->osd_subsurf_uv = 0;
		ss->osd_uv_index = -1;
		ss->osd_next_face_index = 0;
		ss->dm = NULL;
#endif

		return ss;
	}
}

void ccgSubSurf_free(CCGSubSurf *ss)
{
	CCGAllocatorIFC allocatorIFC = ss->allocatorIFC;
	CCGAllocatorHDL allocator = ss->allocator;
#ifdef WITH_OPENSUBDIV
	if (ss->osd_evaluator != NULL) {
		/* TODO(sergey): Need proper port. */
		openSubdiv_deleteEvaluatorDescr(ss->osd_evaluator);
	}
	if (ss->osd_mesh != NULL) {
		/* TODO(sergey): Make sure free happens form the main thread! */
		openSubdiv_deleteOsdGLMesh(ss->osd_mesh);
	}
	if (ss->osd_vao != 0) {
		glDeleteVertexArrays(1, &ss->osd_vao);
	}
	if (ss->dm != NULL) {
		ss->dm->needsFree = 1;
		ss->dm->release(ss->dm);
	}
#endif

	if (ss->syncState) {
		_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_free, ss);
		_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_free, ss);
		_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);

		MEM_freeN(ss->tempVerts);
		MEM_freeN(ss->tempEdges);
	}

	CCGSUBSURF_free(ss, ss->r);
	CCGSUBSURF_free(ss, ss->q);
	if (ss->defaultEdgeUserData) CCGSUBSURF_free(ss, ss->defaultEdgeUserData);

	_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
	_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
	_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);

	CCGSUBSURF_free(ss, ss);

	if (allocatorIFC.release) {
		allocatorIFC.release(allocator);
	}
}

CCGError ccgSubSurf_setAllowEdgeCreation(CCGSubSurf *ss, int allowEdgeCreation, float defaultCreaseValue, void *defaultUserData)
{
	if (ss->defaultEdgeUserData) {
		CCGSUBSURF_free(ss, ss->defaultEdgeUserData);
	}

	ss->allowEdgeCreation = !!allowEdgeCreation;
	ss->defaultCreaseValue = defaultCreaseValue;
	ss->defaultEdgeUserData = CCGSUBSURF_alloc(ss, ss->meshIFC.edgeUserSize);

	if (defaultUserData) {
		memcpy(ss->defaultEdgeUserData, defaultUserData, ss->meshIFC.edgeUserSize);
	}
	else {
		memset(ss->defaultEdgeUserData, 0, ss->meshIFC.edgeUserSize);
	}

	return eCCGError_None;
}
void ccgSubSurf_getAllowEdgeCreation(CCGSubSurf *ss, int *allowEdgeCreation_r, float *defaultCreaseValue_r, void *defaultUserData_r)
{
	if (allowEdgeCreation_r) *allowEdgeCreation_r = ss->allowEdgeCreation;
	if (ss->allowEdgeCreation) {
		if (defaultCreaseValue_r) *defaultCreaseValue_r = ss->defaultCreaseValue;
		if (defaultUserData_r) memcpy(defaultUserData_r, ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
	}
}

CCGError ccgSubSurf_setSubdivisionLevels(CCGSubSurf *ss, int subdivisionLevels)
{
	if (subdivisionLevels <= 0) {
		return eCCGError_InvalidValue;
	}
	else if (subdivisionLevels != ss->subdivLevels) {
		ss->numGrids = 0;
		ss->subdivLevels = subdivisionLevels;
		_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);
		_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
		_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
		ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	}

	return eCCGError_None;
}

void ccgSubSurf_getUseAgeCounts(CCGSubSurf *ss, int *useAgeCounts_r, int *vertUserOffset_r, int *edgeUserOffset_r, int *faceUserOffset_r)
{
	*useAgeCounts_r = ss->useAgeCounts;

	if (vertUserOffset_r) *vertUserOffset_r = ss->vertUserAgeOffset;
	if (edgeUserOffset_r) *edgeUserOffset_r = ss->edgeUserAgeOffset;
	if (faceUserOffset_r) *faceUserOffset_r = ss->faceUserAgeOffset;
}

CCGError ccgSubSurf_setUseAgeCounts(CCGSubSurf *ss, int useAgeCounts, int vertUserOffset, int edgeUserOffset, int faceUserOffset)
{
	if (useAgeCounts) {
		if ((vertUserOffset + 4 > ss->meshIFC.vertUserSize) ||
		    (edgeUserOffset + 4 > ss->meshIFC.edgeUserSize) ||
		    (faceUserOffset + 4 > ss->meshIFC.faceUserSize))
		{
			return eCCGError_InvalidValue;
		}
		else {
			ss->useAgeCounts = 1;
			ss->vertUserAgeOffset = vertUserOffset;
			ss->edgeUserAgeOffset = edgeUserOffset;
			ss->faceUserAgeOffset = faceUserOffset;
		}
	}
	else {
		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_setCalcVertexNormals(CCGSubSurf *ss, int useVertNormals, int normalDataOffset)
{
	if (useVertNormals) {
		if (normalDataOffset < 0 || normalDataOffset + 12 > ss->meshIFC.vertDataSize) {
			return eCCGError_InvalidValue;
		}
		else {
			ss->calcVertNormals = 1;
			ss->normalDataOffset = normalDataOffset;
		}
	}
	else {
		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;
	}

	return eCCGError_None;
}

void ccgSubSurf_setAllocMask(CCGSubSurf *ss, int allocMask, int maskOffset)
{
	ss->allocMask = allocMask;
	ss->maskDataOffset = maskOffset;
}

void ccgSubSurf_setNumLayers(CCGSubSurf *ss, int numLayers)
{
	ss->meshIFC.numLayers = numLayers;
}

/***/

CCGError ccgSubSurf_initFullSync(CCGSubSurf *ss)
{
	if (ss->syncState != eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->oldVMap = ss->vMap; 
	ss->oldEMap = ss->eMap; 
	ss->oldFMap = ss->fMap;

	ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);

	ss->numGrids = 0;

	ss->lenTempArrays = 12;
	ss->tempVerts = MEM_mallocN(sizeof(*ss->tempVerts) * ss->lenTempArrays, "CCGSubsurf tempVerts");
	ss->tempEdges = MEM_mallocN(sizeof(*ss->tempEdges) * ss->lenTempArrays, "CCGSubsurf tempEdges");

	ss->syncState = eSyncState_Vert;
	ss->osd_next_face_index = 0;

	return eCCGError_None;
}

CCGError ccgSubSurf_initPartialSync(CCGSubSurf *ss)
{
	if (ss->syncState != eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->syncState = eSyncState_Partial;

	return eCCGError_None;
}

CCGError ccgSubSurf_syncVertDel(CCGSubSurf *ss, CCGVertHDL vHDL)
{
	if (ss->syncState != eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	}
	else {
		void **prevp;
		CCGVert *v = _ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);

		if (!v || v->numFaces || v->numEdges) {
			return eCCGError_InvalidValue;
		}
		else {
			*prevp = v->next;
			_vert_free(v, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncEdgeDel(CCGSubSurf *ss, CCGEdgeHDL eHDL)
{
	if (ss->syncState != eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	}
	else {
		void **prevp;
		CCGEdge *e = _ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);

		if (!e || e->numFaces) {
			return eCCGError_InvalidValue;
		}
		else {
			*prevp = e->next;
			_edge_unlinkMarkAndFree(e, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncFaceDel(CCGSubSurf *ss, CCGFaceHDL fHDL)
{
	if (ss->syncState != eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	}
	else {
		void **prevp;
		CCGFace *f = _ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		if (!f) {
			return eCCGError_InvalidValue;
		}
		else {
			*prevp = f->next;
			_face_unlinkMarkAndFree(f, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncVert(CCGSubSurf *ss, CCGVertHDL vHDL, const void *vertData, int seam, CCGVert **v_r)
{
	void **prevp;
	CCGVert *v = NULL;
	short seamflag = (seam) ? Vert_eSeam : 0;
	
	if (ss->syncState == eSyncState_Partial) {
		v = _ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			_ehash_insert(ss->vMap, (EHEntry *) v);
			v->flags = Vert_eEffected | seamflag;
		}
		else if (!VertDataEqual(vertData, _vert_getCo(v, 0, ss->meshIFC.vertDataSize), ss) ||
		         ((v->flags & Vert_eSeam) != seamflag))
		{
			int i, j;

			VertDataCopy(_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			v->flags = Vert_eEffected | seamflag;

			for (i = 0; i < v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
			for (i = 0; i < v->numFaces; i++) {
				CCGFace *f = v->faces[i];
				for (j = 0; j < f->numVerts; j++) {
					FACE_getVerts(f)[j]->flags |= Vert_eEffected;
				}
			}
		}
	}
	else {
		if (ss->syncState != eSyncState_Vert) {
			return eCCGError_InvalidSyncState;
		}

		v = _ehash_lookupWithPrev(ss->oldVMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			_ehash_insert(ss->vMap, (EHEntry *) v);
			v->flags = Vert_eEffected | seamflag;
		}
		else if (!VertDataEqual(vertData, _vert_getCo(v, 0, ss->meshIFC.vertDataSize), ss) ||
		         ((v->flags & Vert_eSeam) != seamflag))
		{
			*prevp = v->next;
			_ehash_insert(ss->vMap, (EHEntry *) v);
			VertDataCopy(_vert_getCo(v, 0, ss->meshIFC.vertDataSize), vertData, ss);
			v->flags = Vert_eEffected | Vert_eChanged | seamflag;
		}
		else {
			*prevp = v->next;
			_ehash_insert(ss->vMap, (EHEntry *) v);
			v->flags = 0;
		}
#ifdef WITH_OPENSUBDIV
		v->osd_index = ss->vMap->numEntries - 1;
#endif
	}

	if (v_r) *v_r = v;
	return eCCGError_None;
}

CCGError ccgSubSurf_syncEdge(CCGSubSurf *ss, CCGEdgeHDL eHDL, CCGVertHDL e_vHDL0, CCGVertHDL e_vHDL1, float crease, CCGEdge **e_r)
{
	void **prevp;
	CCGEdge *e = NULL, *eNew;

	if (ss->syncState == eSyncState_Partial) {
		e = _ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);
		if (!e || e->v0->vHDL != e_vHDL0 || e->v1->vHDL != e_vHDL1 || crease != e->crease) {
			CCGVert *v0 = _ehash_lookup(ss->vMap, e_vHDL0);
			CCGVert *v1 = _ehash_lookup(ss->vMap, e_vHDL1);

			eNew = _edge_new(eHDL, v0, v1, crease, ss);

			if (e) {
				*prevp = eNew;
				eNew->next = e->next;

				_edge_unlinkMarkAndFree(e, ss);
			}
			else {
				_ehash_insert(ss->eMap, (EHEntry *) eNew);
			}

			eNew->v0->flags |= Vert_eEffected;
			eNew->v1->flags |= Vert_eEffected;
		}
	}
	else {
		if (ss->syncState == eSyncState_Vert) {
			ss->syncState = eSyncState_Edge;
		}
		else if (ss->syncState != eSyncState_Edge) {
			return eCCGError_InvalidSyncState;
		}

		e = _ehash_lookupWithPrev(ss->oldEMap, eHDL, &prevp);
		if (!e || e->v0->vHDL != e_vHDL0 || e->v1->vHDL != e_vHDL1 || e->crease != crease) {
			CCGVert *v0 = _ehash_lookup(ss->vMap, e_vHDL0);
			CCGVert *v1 = _ehash_lookup(ss->vMap, e_vHDL1);
			e = _edge_new(eHDL, v0, v1, crease, ss);
			_ehash_insert(ss->eMap, (EHEntry *) e);
			e->v0->flags |= Vert_eEffected;
			e->v1->flags |= Vert_eEffected;
		}
		else {
			*prevp = e->next;
			_ehash_insert(ss->eMap, (EHEntry *) e);
			e->flags = 0;
			if ((e->v0->flags | e->v1->flags) & Vert_eChanged) {
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
		}
	}

	if (e_r) *e_r = e;
	return eCCGError_None;
}

CCGError ccgSubSurf_syncFace(CCGSubSurf *ss, CCGFaceHDL fHDL, int numVerts, CCGVertHDL *vHDLs, CCGFace **f_r)
{
	void **prevp;
	CCGFace *f = NULL, *fNew;
	int j, k, topologyChanged = 0;

	if (UNLIKELY(numVerts > ss->lenTempArrays)) {
		ss->lenTempArrays = (numVerts < ss->lenTempArrays * 2) ? ss->lenTempArrays * 2 : numVerts;
		ss->tempVerts = MEM_reallocN(ss->tempVerts, sizeof(*ss->tempVerts) * ss->lenTempArrays);
		ss->tempEdges = MEM_reallocN(ss->tempEdges, sizeof(*ss->tempEdges) * ss->lenTempArrays);
	}

	if (ss->syncState == eSyncState_Partial) {
		f = _ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		for (k = 0; k < numVerts; k++) {
			ss->tempVerts[k] = _ehash_lookup(ss->vMap, vHDLs[k]);
		}
		for (k = 0; k < numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k + 1) % numVerts]);
		}

		if (f) {
			if (f->numVerts != numVerts ||
			    memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts) * numVerts) ||
			    memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges) * numVerts))
			{
				topologyChanged = 1;
			}
		}

		if (!f || topologyChanged) {
			fNew = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);

			if (f) {
				ss->numGrids += numVerts - f->numVerts;

				*prevp = fNew;
				fNew->next = f->next;

				_face_unlinkMarkAndFree(f, ss);
			}
			else {
				ss->numGrids += numVerts;
				_ehash_insert(ss->fMap, (EHEntry *) fNew);
			}

			for (k = 0; k < numVerts; k++)
				FACE_getVerts(fNew)[k]->flags |= Vert_eEffected;
		}
	}
	else {
		if (ss->syncState == eSyncState_Vert || ss->syncState == eSyncState_Edge) {
			ss->syncState = eSyncState_Face;
		}
		else if (ss->syncState != eSyncState_Face) {
			return eCCGError_InvalidSyncState;
		}

		f = _ehash_lookupWithPrev(ss->oldFMap, fHDL, &prevp);

		for (k = 0; k < numVerts; k++) {
			ss->tempVerts[k] = _ehash_lookup(ss->vMap, vHDLs[k]);

			if (!ss->tempVerts[k])
				return eCCGError_InvalidValue;
		}
		for (k = 0; k < numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k + 1) % numVerts]);

			if (!ss->tempEdges[k]) {
				if (ss->allowEdgeCreation) {
					CCGEdge *e = ss->tempEdges[k] = _edge_new((CCGEdgeHDL) - 1, ss->tempVerts[k], ss->tempVerts[(k + 1) % numVerts], ss->defaultCreaseValue, ss);
					_ehash_insert(ss->eMap, (EHEntry *) e);
					e->v0->flags |= Vert_eEffected;
					e->v1->flags |= Vert_eEffected;
					if (ss->meshIFC.edgeUserSize) {
						memcpy(ccgSubSurf_getEdgeUserData(ss, e), ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
					}
				}
				else {
					return eCCGError_InvalidValue;
				}
			}
		}

		if (f) {
			if (f->numVerts != numVerts ||
			    memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts) * numVerts) ||
			    memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges) * numVerts))
			{
				topologyChanged = 1;
			}
		}

		if (!f || topologyChanged) {
			f = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);
			_ehash_insert(ss->fMap, (EHEntry *) f);
			ss->numGrids += numVerts;

			for (k = 0; k < numVerts; k++)
				FACE_getVerts(f)[k]->flags |= Vert_eEffected;
		}
		else {
			*prevp = f->next;
			_ehash_insert(ss->fMap, (EHEntry *) f);
			f->flags = 0;
			ss->numGrids += f->numVerts;

			for (j = 0; j < f->numVerts; j++) {
				if (FACE_getVerts(f)[j]->flags & Vert_eChanged) {
					for (k = 0; k < f->numVerts; k++)
						FACE_getVerts(f)[k]->flags |= Vert_eEffected;
					break;
				}
			}
		}
#ifdef WITH_OPENSUBDIV
		f->osd_index = ss->osd_next_face_index;
		if (numVerts == 4) {
			ss->osd_next_face_index++;
		}
		else {
			ss->osd_next_face_index += numVerts;
		}
#endif
	}

	if (f_r) *f_r = f;
	return eCCGError_None;
}

CCGError ccgSubSurf_processSync(CCGSubSurf *ss)
{
	if (ss->syncState == eSyncState_Partial) {
		ss->syncState = eSyncState_None;

		ccgSubSurf__sync(ss);
	}
	else if (ss->syncState) {
		_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_unlinkMarkAndFree, ss);
		_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_unlinkMarkAndFree, ss);
		_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);
		MEM_freeN(ss->tempEdges);
		MEM_freeN(ss->tempVerts);

		ss->lenTempArrays = 0;

		ss->oldFMap = ss->oldEMap = ss->oldVMap = NULL;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;

		ss->syncState = eSyncState_None;

		ccgSubSurf__sync(ss);
	}
	else {
		return eCCGError_InvalidSyncState;
	}

	return eCCGError_None;
}

#define VERT_getCo(v, lvl)                  _vert_getCo(v, lvl, vertDataSize)
#define VERT_getNo(v, lvl)                  _vert_getNo(v, lvl, vertDataSize, normalDataOffset)
#define EDGE_getCo(e, lvl, x)               _edge_getCo(e, lvl, x, vertDataSize)
#define EDGE_getNo(e, lvl, x)               _edge_getNo(e, lvl, x, vertDataSize, normalDataOffset)
#define FACE_getIFNo(f, lvl, S, x, y)       _face_getIFNo(f, lvl, S, x, y, subdivLevels, vertDataSize, normalDataOffset)
#define FACE_calcIFNo(f, lvl, S, x, y, no)  _face_calcIFNo(f, lvl, S, x, y, no, subdivLevels, vertDataSize)
#define FACE_getIENo(f, lvl, S, x)          _face_getIENo(f, lvl, S, x, subdivLevels, vertDataSize, normalDataOffset)
#define FACE_getIECo(f, lvl, S, x)          _face_getIECo(f, lvl, S, x, subdivLevels, vertDataSize)
#define FACE_getIFCo(f, lvl, S, x, y)       _face_getIFCo(f, lvl, S, x, y, subdivLevels, vertDataSize)

static void ccgSubSurf__calcVertNormals(CCGSubSurf *ss,
                                        CCGVert **effectedV, CCGEdge **effectedE, CCGFace **effectedF,
                                        int numEffectedV, int numEffectedE, int numEffectedF)
{
	int i, ptrIdx;
	int subdivLevels = ss->subdivLevels;
	int lvl = ss->subdivLevels;
	int edgeSize = ccg_edgesize(lvl);
	int gridSize = ccg_gridsize(lvl);
	int normalDataOffset = ss->normalDataOffset;
	int vertDataSize = ss->meshIFC.vertDataSize;

//#pragma omp parallel for private(ptrIdx) if (numEffectedF * edgeSize * edgeSize * 4 >= CCG_OMP_LIMIT)
	for (ptrIdx = 0; ptrIdx < numEffectedF; ptrIdx++) {
		CCGFace *f = (CCGFace *) effectedF[ptrIdx];
		int S, x, y;
		float no[3];

		for (S = 0; S < f->numVerts; S++) {
			for (y = 0; y < gridSize - 1; y++) {
				for (x = 0; x < gridSize - 1; x++) {
					NormZero(FACE_getIFNo(f, lvl, S, x, y));
				}
			}

			if (FACE_getEdges(f)[(S - 1 + f->numVerts) % f->numVerts]->flags & Edge_eEffected) {
				for (x = 0; x < gridSize - 1; x++) {
					NormZero(FACE_getIFNo(f, lvl, S, x, gridSize - 1));
				}
			}
			if (FACE_getEdges(f)[S]->flags & Edge_eEffected) {
				for (y = 0; y < gridSize - 1; y++) {
					NormZero(FACE_getIFNo(f, lvl, S, gridSize - 1, y));
				}
			}
			if (FACE_getVerts(f)[S]->flags & Vert_eEffected) {
				NormZero(FACE_getIFNo(f, lvl, S, gridSize - 1, gridSize - 1));
			}
		}

		for (S = 0; S < f->numVerts; S++) {
			int yLimit = !(FACE_getEdges(f)[(S - 1 + f->numVerts) % f->numVerts]->flags & Edge_eEffected);
			int xLimit = !(FACE_getEdges(f)[S]->flags & Edge_eEffected);
			int yLimitNext = xLimit;
			int xLimitPrev = yLimit;
			
			for (y = 0; y < gridSize - 1; y++) {
				for (x = 0; x < gridSize - 1; x++) {
					int xPlusOk = (!xLimit || x < gridSize - 2);
					int yPlusOk = (!yLimit || y < gridSize - 2);

					FACE_calcIFNo(f, lvl, S, x, y, no);

					NormAdd(FACE_getIFNo(f, lvl, S, x + 0, y + 0), no);
					if (xPlusOk)
						NormAdd(FACE_getIFNo(f, lvl, S, x + 1, y + 0), no);
					if (yPlusOk)
						NormAdd(FACE_getIFNo(f, lvl, S, x + 0, y + 1), no);
					if (xPlusOk && yPlusOk) {
						if (x < gridSize - 2 || y < gridSize - 2 || FACE_getVerts(f)[S]->flags & Vert_eEffected) {
							NormAdd(FACE_getIFNo(f, lvl, S, x + 1, y + 1), no);
						}
					}

					if (x == 0 && y == 0) {
						int K;

						if (!yLimitNext || 1 < gridSize - 1)
							NormAdd(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, 1), no);
						if (!xLimitPrev || 1 < gridSize - 1)
							NormAdd(FACE_getIFNo(f, lvl, (S - 1 + f->numVerts) % f->numVerts, 1, 0), no);

						for (K = 0; K < f->numVerts; K++) {
							if (K != S) {
								NormAdd(FACE_getIFNo(f, lvl, K, 0, 0), no);
							}
						}
					}
					else if (y == 0) {
						NormAdd(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, x), no);
						if (!yLimitNext || x < gridSize - 2)
							NormAdd(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, x + 1), no);
					}
					else if (x == 0) {
						NormAdd(FACE_getIFNo(f, lvl, (S - 1 + f->numVerts) % f->numVerts, y, 0), no);
						if (!xLimitPrev || y < gridSize - 2)
							NormAdd(FACE_getIFNo(f, lvl, (S - 1 + f->numVerts) % f->numVerts, y + 1, 0), no);
					}
				}
			}
		}
	}
	/* XXX can I reduce the number of normalisations here? */
	for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
		CCGVert *v = (CCGVert *) effectedV[ptrIdx];
		float *no = VERT_getNo(v, lvl);

		NormZero(no);

		for (i = 0; i < v->numFaces; i++) {
			CCGFace *f = v->faces[i];
			NormAdd(no, FACE_getIFNo(f, lvl, _face_getVertIndex(f, v), gridSize - 1, gridSize - 1));
		}

		if (UNLIKELY(v->numFaces == 0)) {
			NormCopy(no, VERT_getCo(v, lvl));
		}

		Normalize(no);

		for (i = 0; i < v->numFaces; i++) {
			CCGFace *f = v->faces[i];
			NormCopy(FACE_getIFNo(f, lvl, _face_getVertIndex(f, v), gridSize - 1, gridSize - 1), no);
		}
	}
	for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge *) effectedE[ptrIdx];

		if (e->numFaces) {
			CCGFace *fLast = e->faces[e->numFaces - 1];
			int x;

			for (i = 0; i < e->numFaces - 1; i++) {
				CCGFace *f = e->faces[i];
				const int f_ed_idx = _face_getEdgeIndex(f, e);
				const int f_ed_idx_last = _face_getEdgeIndex(fLast, e);

				for (x = 1; x < edgeSize - 1; x++) {
					NormAdd(_face_getIFNoEdge(fLast, e, f_ed_idx_last, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
					        _face_getIFNoEdge(f, e, f_ed_idx, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
				}
			}

			for (i = 0; i < e->numFaces - 1; i++) {
				CCGFace *f = e->faces[i];
				const int f_ed_idx = _face_getEdgeIndex(f, e);
				const int f_ed_idx_last = _face_getEdgeIndex(fLast, e);

				for (x = 1; x < edgeSize - 1; x++) {
					NormCopy(_face_getIFNoEdge(f, e, f_ed_idx, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
					         _face_getIFNoEdge(fLast, e, f_ed_idx_last, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
				}
			}
		}
	}

//#pragma omp parallel for private(ptrIdx) if (numEffectedF * edgeSize * edgeSize * 4 >= CCG_OMP_LIMIT)
	for (ptrIdx = 0; ptrIdx < numEffectedF; ptrIdx++) {
		CCGFace *f = (CCGFace *) effectedF[ptrIdx];
		int S, x, y;

		for (S = 0; S < f->numVerts; S++) {
			NormCopy(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, gridSize - 1),
			         FACE_getIFNo(f, lvl, S, gridSize - 1, 0));
		}

		for (S = 0; S < f->numVerts; S++) {
			for (y = 0; y < gridSize; y++) {
				for (x = 0; x < gridSize; x++) {
					float *no = FACE_getIFNo(f, lvl, S, x, y);
					Normalize(no);
				}
			}

			VertDataCopy((float *)((byte *)FACE_getCenterData(f) + normalDataOffset),
			             FACE_getIFNo(f, lvl, S, 0, 0), ss);

			for (x = 1; x < gridSize - 1; x++)
				NormCopy(FACE_getIENo(f, lvl, S, x),
				         FACE_getIFNo(f, lvl, S, x, 0));
		}
	}

	for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge *) effectedE[ptrIdx];

		if (e->numFaces) {
			CCGFace *f = e->faces[0];
			int x;
			const int f_ed_idx = _face_getEdgeIndex(f, e);

			for (x = 0; x < edgeSize; x++)
				NormCopy(EDGE_getNo(e, lvl, x),
				         _face_getIFNoEdge(f, e, f_ed_idx, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
		}
		else {
			/* set to zero here otherwise the normals are uninitialized memory
			 * render: tests/animation/knight.blend with valgrind.
			 * we could be more clever and interpolate vertex normals but these are
			 * most likely not used so just zero out. */
			int x;

			for (x = 0; x < edgeSize; x++) {
				float *no = EDGE_getNo(e, lvl, x);
				NormCopy(no, EDGE_getCo(e, lvl, x));
				Normalize(no);
			}
		}
	}
}

static void ccgSubSurf__calcSubdivLevel(CCGSubSurf *ss,
                                        CCGVert **effectedV, CCGEdge **effectedE, CCGFace **effectedF,
                                        int numEffectedV, int numEffectedE, int numEffectedF, int curLvl)
{
	int subdivLevels = ss->subdivLevels;
	int edgeSize = ccg_edgesize(curLvl);
	int gridSize = ccg_gridsize(curLvl);
	int nextLvl = curLvl + 1;
	int ptrIdx, cornerIdx, i;
	int vertDataSize = ss->meshIFC.vertDataSize;
	float *q = ss->q, *r = ss->r;

//#pragma omp parallel for private(ptrIdx) if (numEffectedF * edgeSize * edgeSize * 4 >= CCG_OMP_LIMIT)
	for (ptrIdx = 0; ptrIdx < numEffectedF; ptrIdx++) {
		CCGFace *f = (CCGFace *) effectedF[ptrIdx];
		int S, x, y;

		/* interior face midpoints
		 * - old interior face points
		 */
		for (S = 0; S < f->numVerts; S++) {
			for (y = 0; y < gridSize - 1; y++) {
				for (x = 0; x < gridSize - 1; x++) {
					int fx = 1 + 2 * x;
					int fy = 1 + 2 * y;
					const float *co0 = FACE_getIFCo(f, curLvl, S, x + 0, y + 0);
					const float *co1 = FACE_getIFCo(f, curLvl, S, x + 1, y + 0);
					const float *co2 = FACE_getIFCo(f, curLvl, S, x + 1, y + 1);
					const float *co3 = FACE_getIFCo(f, curLvl, S, x + 0, y + 1);
					float *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

					VertDataAvg4(co, co0, co1, co2, co3, ss);
				}
			}
		}

		/* interior edge midpoints
		 * - old interior edge points
		 * - new interior face midpoints
		 */
		for (S = 0; S < f->numVerts; S++) {
			for (x = 0; x < gridSize - 1; x++) {
				int fx = x * 2 + 1;
				const float *co0 = FACE_getIECo(f, curLvl, S, x + 0);
				const float *co1 = FACE_getIECo(f, curLvl, S, x + 1);
				const float *co2 = FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx);
				const float *co3 = FACE_getIFCo(f, nextLvl, S, fx, 1);
				float *co  = FACE_getIECo(f, nextLvl, S, fx);
				
				VertDataAvg4(co, co0, co1, co2, co3, ss);
			}

			/* interior face interior edge midpoints
			 * - old interior face points
			 * - new interior face midpoints
			 */

			/* vertical */
			for (x = 1; x < gridSize - 1; x++) {
				for (y = 0; y < gridSize - 1; y++) {
					int fx = x * 2;
					int fy = y * 2 + 1;
					const float *co0 = FACE_getIFCo(f, curLvl, S, x, y + 0);
					const float *co1 = FACE_getIFCo(f, curLvl, S, x, y + 1);
					const float *co2 = FACE_getIFCo(f, nextLvl, S, fx - 1, fy);
					const float *co3 = FACE_getIFCo(f, nextLvl, S, fx + 1, fy);
					float *co  = FACE_getIFCo(f, nextLvl, S, fx, fy);

					VertDataAvg4(co, co0, co1, co2, co3, ss);
				}
			}

			/* horizontal */
			for (y = 1; y < gridSize - 1; y++) {
				for (x = 0; x < gridSize - 1; x++) {
					int fx = x * 2 + 1;
					int fy = y * 2;
					const float *co0 = FACE_getIFCo(f, curLvl, S, x + 0, y);
					const float *co1 = FACE_getIFCo(f, curLvl, S, x + 1, y);
					const float *co2 = FACE_getIFCo(f, nextLvl, S, fx, fy - 1);
					const float *co3 = FACE_getIFCo(f, nextLvl, S, fx, fy + 1);
					float *co  = FACE_getIFCo(f, nextLvl, S, fx, fy);

					VertDataAvg4(co, co0, co1, co2, co3, ss);
				}
			}
		}
	}

	/* exterior edge midpoints
	 * - old exterior edge points
	 * - new interior face midpoints
	 */
	for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge *) effectedE[ptrIdx];
		float sharpness = EDGE_getSharpness(e, curLvl);
		int x, j;

		if (_edge_isBoundary(e) || sharpness > 1.0f) {
			for (x = 0; x < edgeSize - 1; x++) {
				int fx = x * 2 + 1;
				const float *co0 = EDGE_getCo(e, curLvl, x + 0);
				const float *co1 = EDGE_getCo(e, curLvl, x + 1);
				float *co  = EDGE_getCo(e, nextLvl, fx);

				VertDataCopy(co, co0, ss);
				VertDataAdd(co, co1, ss);
				VertDataMulN(co, 0.5f, ss);
			}
		}
		else {
			for (x = 0; x < edgeSize - 1; x++) {
				int fx = x * 2 + 1;
				const float *co0 = EDGE_getCo(e, curLvl, x + 0);
				const float *co1 = EDGE_getCo(e, curLvl, x + 1);
				float *co  = EDGE_getCo(e, nextLvl, fx);
				int numFaces = 0;

				VertDataCopy(q, co0, ss);
				VertDataAdd(q, co1, ss);

				for (j = 0; j < e->numFaces; j++) {
					CCGFace *f = e->faces[j];
					const int f_ed_idx = _face_getEdgeIndex(f, e);
					VertDataAdd(q, _face_getIFCoEdge(f, e, f_ed_idx, nextLvl, fx, 1, subdivLevels, vertDataSize), ss);
					numFaces++;
				}

				VertDataMulN(q, 1.0f / (2.0f + numFaces), ss);

				VertDataCopy(r, co0, ss);
				VertDataAdd(r, co1, ss);
				VertDataMulN(r, 0.5f, ss);

				VertDataCopy(co, q, ss);
				VertDataSub(r, q, ss);
				VertDataMulN(r, sharpness, ss);
				VertDataAdd(co, r, ss);
			}
		}
	}

	/* exterior vertex shift
	 * - old vertex points (shifting)
	 * - old exterior edge points
	 * - new interior face midpoints
	 */
	for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
		CCGVert *v = (CCGVert *) effectedV[ptrIdx];
		const float *co = VERT_getCo(v, curLvl);
		float *nCo = VERT_getCo(v, nextLvl);
		int sharpCount = 0, allSharp = 1;
		float avgSharpness = 0.0;
		int j, seam = VERT_seam(v), seamEdges = 0;

		for (j = 0; j < v->numEdges; j++) {
			CCGEdge *e = v->edges[j];
			float sharpness = EDGE_getSharpness(e, curLvl);

			if (seam && _edge_isBoundary(e))
				seamEdges++;

			if (sharpness != 0.0f) {
				sharpCount++;
				avgSharpness += sharpness;
			}
			else {
				allSharp = 0;
			}
		}

		if (sharpCount) {
			avgSharpness /= sharpCount;
			if (avgSharpness > 1.0f) {
				avgSharpness = 1.0f;
			}
		}

		if (seamEdges < 2 || seamEdges != v->numEdges)
			seam = 0;

		if (!v->numEdges || ss->meshIFC.simpleSubdiv) {
			VertDataCopy(nCo, co, ss);
		}
		else if (_vert_isBoundary(v)) {
			int numBoundary = 0;

			VertDataZero(r, ss);
			for (j = 0; j < v->numEdges; j++) {
				CCGEdge *e = v->edges[j];
				if (_edge_isBoundary(e)) {
					VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
					numBoundary++;
				}
			}

			VertDataCopy(nCo, co, ss);
			VertDataMulN(nCo, 0.75f, ss);
			VertDataMulN(r, 0.25f / numBoundary, ss);
			VertDataAdd(nCo, r, ss);
		}
		else {
			int cornerIdx = (1 + (1 << (curLvl))) - 2;
			int numEdges = 0, numFaces = 0;

			VertDataZero(q, ss);
			for (j = 0; j < v->numFaces; j++) {
				CCGFace *f = v->faces[j];
				VertDataAdd(q, FACE_getIFCo(f, nextLvl, _face_getVertIndex(f, v), cornerIdx, cornerIdx), ss);
				numFaces++;
			}
			VertDataMulN(q, 1.0f / numFaces, ss);
			VertDataZero(r, ss);
			for (j = 0; j < v->numEdges; j++) {
				CCGEdge *e = v->edges[j];
				VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
				numEdges++;
			}
			VertDataMulN(r, 1.0f / numEdges, ss);

			VertDataCopy(nCo, co, ss);
			VertDataMulN(nCo, numEdges - 2.0f, ss);
			VertDataAdd(nCo, q, ss);
			VertDataAdd(nCo, r, ss);
			VertDataMulN(nCo, 1.0f / numEdges, ss);
		}

		if ((sharpCount > 1 && v->numFaces) || seam) {
			VertDataZero(q, ss);

			if (seam) {
				avgSharpness = 1.0f;
				sharpCount = seamEdges;
				allSharp = 1;
			}

			for (j = 0; j < v->numEdges; j++) {
				CCGEdge *e = v->edges[j];
				float sharpness = EDGE_getSharpness(e, curLvl);

				if (seam) {
					if (_edge_isBoundary(e))
						VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
				}
				else if (sharpness != 0.0f) {
					VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
				}
			}

			VertDataMulN(q, (float) 1 / sharpCount, ss);

			if (sharpCount != 2 || allSharp) {
				/* q = q + (co - q) * avgSharpness */
				VertDataCopy(r, co, ss);
				VertDataSub(r, q, ss);
				VertDataMulN(r, avgSharpness, ss);
				VertDataAdd(q, r, ss);
			}

			/* r = co * 0.75 + q * 0.25 */
			VertDataCopy(r, co, ss);
			VertDataMulN(r, 0.75f, ss);
			VertDataMulN(q, 0.25f, ss);
			VertDataAdd(r, q, ss);

			/* nCo = nCo + (r - nCo) * avgSharpness */
			VertDataSub(r, nCo, ss);
			VertDataMulN(r, avgSharpness, ss);
			VertDataAdd(nCo, r, ss);
		}
	}

	/* exterior edge interior shift
	 * - old exterior edge midpoints (shifting)
	 * - old exterior edge midpoints
	 * - new interior face midpoints
	 */
	for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge *) effectedE[ptrIdx];
		float sharpness = EDGE_getSharpness(e, curLvl);
		int sharpCount = 0;
		float avgSharpness = 0.0;
		int x, j;

		if (sharpness != 0.0f) {
			sharpCount = 2;
			avgSharpness += sharpness;

			if (avgSharpness > 1.0f) {
				avgSharpness = 1.0f;
			}
		}
		else {
			sharpCount = 0;
			avgSharpness = 0;
		}

		if (_edge_isBoundary(e)) {
			for (x = 1; x < edgeSize - 1; x++) {
				int fx = x * 2;
				const float *co = EDGE_getCo(e, curLvl, x);
				float *nCo = EDGE_getCo(e, nextLvl, fx);

				/* Average previous level's endpoints */
				VertDataCopy(r, EDGE_getCo(e, curLvl, x - 1), ss);
				VertDataAdd(r, EDGE_getCo(e, curLvl, x + 1), ss);
				VertDataMulN(r, 0.5f, ss);

				/* nCo = nCo * 0.75 + r * 0.25 */
				VertDataCopy(nCo, co, ss);
				VertDataMulN(nCo, 0.75f, ss);
				VertDataMulN(r, 0.25f, ss);
				VertDataAdd(nCo, r, ss);
			}
		}
		else {
			for (x = 1; x < edgeSize - 1; x++) {
				int fx = x * 2;
				const float *co = EDGE_getCo(e, curLvl, x);
				float *nCo = EDGE_getCo(e, nextLvl, fx);
				int numFaces = 0;

				VertDataZero(q, ss);
				VertDataZero(r, ss);
				VertDataAdd(r, EDGE_getCo(e, curLvl, x - 1), ss);
				VertDataAdd(r, EDGE_getCo(e, curLvl, x + 1), ss);
				for (j = 0; j < e->numFaces; j++) {
					CCGFace *f = e->faces[j];
					int f_ed_idx = _face_getEdgeIndex(f, e);
					VertDataAdd(q, _face_getIFCoEdge(f, e, f_ed_idx, nextLvl, fx - 1, 1, subdivLevels, vertDataSize), ss);
					VertDataAdd(q, _face_getIFCoEdge(f, e, f_ed_idx, nextLvl, fx + 1, 1, subdivLevels, vertDataSize), ss);

					VertDataAdd(r, _face_getIFCoEdge(f, e, f_ed_idx, curLvl, x, 1, subdivLevels, vertDataSize), ss);
					numFaces++;
				}
				VertDataMulN(q, 1.0f / (numFaces * 2.0f), ss);
				VertDataMulN(r, 1.0f / (2.0f + numFaces), ss);

				VertDataCopy(nCo, co, ss);
				VertDataMulN(nCo, (float) numFaces, ss);
				VertDataAdd(nCo, q, ss);
				VertDataAdd(nCo, r, ss);
				VertDataMulN(nCo, 1.0f / (2 + numFaces), ss);

				if (sharpCount == 2) {
					VertDataCopy(q, co, ss);
					VertDataMulN(q, 6.0f, ss);
					VertDataAdd(q, EDGE_getCo(e, curLvl, x - 1), ss);
					VertDataAdd(q, EDGE_getCo(e, curLvl, x + 1), ss);
					VertDataMulN(q, 1 / 8.0f, ss);

					VertDataSub(q, nCo, ss);
					VertDataMulN(q, avgSharpness, ss);
					VertDataAdd(nCo, q, ss);
				}
			}
		}
	}

//#pragma omp parallel private(ptrIdx) if (numEffectedF * edgeSize * edgeSize * 4 >= CCG_OMP_LIMIT)
	{
		float *q, *r;

//#pragma omp critical
		{
			q = MEM_mallocN(ss->meshIFC.vertDataSize, "CCGSubsurf q");
			r = MEM_mallocN(ss->meshIFC.vertDataSize, "CCGSubsurf r");
		}

//#pragma omp for schedule(static)
		for (ptrIdx = 0; ptrIdx < numEffectedF; ptrIdx++) {
			CCGFace *f = (CCGFace *) effectedF[ptrIdx];
			int S, x, y;

			/* interior center point shift
			 * - old face center point (shifting)
			 * - old interior edge points
			 * - new interior face midpoints
			 */
			VertDataZero(q, ss);
			for (S = 0; S < f->numVerts; S++) {
				VertDataAdd(q, FACE_getIFCo(f, nextLvl, S, 1, 1), ss);
			}
			VertDataMulN(q, 1.0f / f->numVerts, ss);
			VertDataZero(r, ss);
			for (S = 0; S < f->numVerts; S++) {
				VertDataAdd(r, FACE_getIECo(f, curLvl, S, 1), ss);
			}
			VertDataMulN(r, 1.0f / f->numVerts, ss);

			VertDataMulN((float *)FACE_getCenterData(f), f->numVerts - 2.0f, ss);
			VertDataAdd((float *)FACE_getCenterData(f), q, ss);
			VertDataAdd((float *)FACE_getCenterData(f), r, ss);
			VertDataMulN((float *)FACE_getCenterData(f), 1.0f / f->numVerts, ss);

			for (S = 0; S < f->numVerts; S++) {
				/* interior face shift
				 * - old interior face point (shifting)
				 * - new interior edge midpoints
				 * - new interior face midpoints
				 */
				for (x = 1; x < gridSize - 1; x++) {
					for (y = 1; y < gridSize - 1; y++) {
						int fx = x * 2;
						int fy = y * 2;
						const float *co = FACE_getIFCo(f, curLvl, S, x, y);
						float *nCo = FACE_getIFCo(f, nextLvl, S, fx, fy);
						
						VertDataAvg4(q,
						             FACE_getIFCo(f, nextLvl, S, fx - 1, fy - 1),
						             FACE_getIFCo(f, nextLvl, S, fx + 1, fy - 1),
						             FACE_getIFCo(f, nextLvl, S, fx + 1, fy + 1),
						             FACE_getIFCo(f, nextLvl, S, fx - 1, fy + 1),
						             ss);

						VertDataAvg4(r,
						             FACE_getIFCo(f, nextLvl, S, fx - 1, fy + 0),
						             FACE_getIFCo(f, nextLvl, S, fx + 1, fy + 0),
						             FACE_getIFCo(f, nextLvl, S, fx + 0, fy - 1),
						             FACE_getIFCo(f, nextLvl, S, fx + 0, fy + 1),
						             ss);

						VertDataCopy(nCo, co, ss);
						VertDataSub(nCo, q, ss);
						VertDataMulN(nCo, 0.25f, ss);
						VertDataAdd(nCo, r, ss);
					}
				}

				/* interior edge interior shift
				 * - old interior edge point (shifting)
				 * - new interior edge midpoints
				 * - new interior face midpoints
				 */
				for (x = 1; x < gridSize - 1; x++) {
					int fx = x * 2;
					const float *co = FACE_getIECo(f, curLvl, S, x);
					float *nCo = FACE_getIECo(f, nextLvl, S, fx);
					
					VertDataAvg4(q,
					             FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx - 1),
					             FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx + 1),
					             FACE_getIFCo(f, nextLvl, S, fx + 1, +1),
					             FACE_getIFCo(f, nextLvl, S, fx - 1, +1), ss);

					VertDataAvg4(r,
					             FACE_getIECo(f, nextLvl, S, fx - 1),
					             FACE_getIECo(f, nextLvl, S, fx + 1),
					             FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx),
					             FACE_getIFCo(f, nextLvl, S, fx, 1),
					             ss);

					VertDataCopy(nCo, co, ss);
					VertDataSub(nCo, q, ss);
					VertDataMulN(nCo, 0.25f, ss);
					VertDataAdd(nCo, r, ss);
				}
			}
		}

//#pragma omp critical
		{
			MEM_freeN(q);
			MEM_freeN(r);
		}
	}

	/* copy down */
	edgeSize = ccg_edgesize(nextLvl);
	gridSize = ccg_gridsize(nextLvl);
	cornerIdx = gridSize - 1;

//#pragma omp parallel for private(i) if (numEffectedF * edgeSize * edgeSize * 4 >= CCG_OMP_LIMIT)
	for (i = 0; i < numEffectedE; i++) {
		CCGEdge *e = effectedE[i];
		VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl), ss);
		VertDataCopy(EDGE_getCo(e, nextLvl, edgeSize - 1), VERT_getCo(e->v1, nextLvl), ss);
	}

//#pragma omp parallel for private(i) if (numEffectedF * edgeSize * edgeSize * 4 >= CCG_OMP_LIMIT)
	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];
		int S, x;

		for (S = 0; S < f->numVerts; S++) {
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[(S + f->numVerts - 1) % f->numVerts];

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], nextLvl), ss);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, cornerIdx), EDGE_getCo(FACE_getEdges(f)[S], nextLvl, cornerIdx), ss);
			for (x = 1; x < gridSize - 1; x++) {
				float *co = FACE_getIECo(f, nextLvl, S, x);
				VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, 0), co, ss);
				VertDataCopy(FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 0, x), co, ss);
			}
			for (x = 0; x < gridSize - 1; x++) {
				int eI = gridSize - 1 - x;
				VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, eI, vertDataSize), ss);
				VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, eI, vertDataSize), ss);
			}
		}
	}
}

#ifdef DUMP_RESULT_GRIDS
static void ccgSubSurf__dumpCoords(CCGSubSurf *ss)
{
	int vertDataSize = ss->meshIFC.vertDataSize;
	int subdivLevels = ss->subdivLevels;
	int gridSize = ccg_gridsize(subdivLevels);
	int edgeSize = ccg_edgesize(subdivLevels);
	int i, index, S;

	for (i = 0, index = 0; i < ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next, index++) {
			float *co = VERT_getCo(v, subdivLevels);
			printf("vertex index=%d, co=(%f, %f, %f)\n",
			       index, co[0], co[1], co[2]);
		}
	}

	for (i = 0, index = 0; i < ss->eMap->curSize; i++) {
		CCGEdge *e = (CCGEdge *) ss->eMap->buckets[i];
		for (; e; e = e->next, index++) {
			int x;
			float *co = VERT_getCo(e->v0, subdivLevels);
			printf("edge index=%d, start_co=(%f, %f, %f)\n",
			       index, co[0], co[1], co[2]);
			for (x = 0; x < edgeSize; x++) {
				float *co = EDGE_getCo(e, subdivLevels, x);
				printf("edge index=%d, seg=%d, co=(%f, %f, %f)\n",
				       index, x, co[0], co[1], co[2]);
			}
			co = VERT_getCo(e->v1, subdivLevels);
			printf("edge index=%d, end_co=(%f, %f, %f)\n",
			       index, co[0], co[1], co[2]);
		}
	}

	for (i = 0, index = 0; i < ss->fMap->curSize; i++) {
		CCGFace *f = (CCGFace *) ss->fMap->buckets[i];
		for (; f; f = f->next, index++) {
			for (S = 0; S < f->numVerts; S++) {
				CCGVert *v = FACE_getVerts(f)[S];
				float *co = VERT_getCo(v, subdivLevels);
				printf("face index=%d, vertex=%d, coord=(%f, %f, %f)\n",
				       index, S, co[0], co[1], co[2]);
			}
		}
	}

	for (i = 0, index = 0; i < ss->fMap->curSize; i++) {
		CCGFace *f = (CCGFace *) ss->fMap->buckets[i];
		for (; f; f = f->next, index++) {
			for (S = 0; S < f->numVerts; S++) {
				CCGEdge *e = FACE_getEdges(f)[S];
				float *co1 = VERT_getCo(e->v0, subdivLevels);
				float *co2 = VERT_getCo(e->v1, subdivLevels);
				printf("face index=%d, edge=%d, coord1=(%f, %f, %f), coord2=(%f, %f, %f)\n",
				       index, S, co1[0], co1[1], co1[2], co2[0], co2[1], co2[2]);
			}
		}
	}

	for (i = 0, index = 0; i < ss->fMap->curSize; i++) {
		CCGFace *f = (CCGFace *) ss->fMap->buckets[i];
		for (; f; f = f->next, index++) {
			for (S = 0; S < f->numVerts; S++) {
				int x, y;
				for (x = 0; x < gridSize; x++) {
					for (y = 0; y < gridSize; y++) {
						float *co = FACE_getIFCo(f, subdivLevels, S, x, y);
						printf("face index=%d. corner=%d, x=%d, y=%d, coord=(%f, %f, %f)\n",
						        index, S, x, y, co[0], co[1], co[2]);
					}
				}
				for (x = 0; x < gridSize; x++) {
					float *co = FACE_getIECo(f, subdivLevels, S, x);
					printf("face index=%d. cornder=%d, ie_index=%d, coord=(%f, %f, %f)\n",
					       index, S, x, co[0], co[1], co[2]);
				}
			}
		}
	}
}
#endif  /* DUMP_RESULT_GRIDS */

#ifdef WITH_OPENSUBDIV

#  define OSD_LOG if (false) printf

void ccgSubSurf_setDerivedMesh(CCGSubSurf *ss, DerivedMesh *dm)
{
	if (ss->dm != NULL) {
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
//#pragma omp parallel for
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
		int scheme = ss->meshIFC.simpleSubdiv
		             ? OPENSUBDIV_SCHEME_BILINEAR
		             : OPENSUBDIV_SCHEME_CATMARK;

		ss->osd_mesh = openSubdiv_createOsdGLMeshFromEvaluator(
			//ss->osd_evaluator,
			ss->dm,
			compute_type,
			ss->subdivLevels,
			scheme,
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
	int num_vertices,
	    refinement_level,
	    /*num_indices,*/
	    num_nverts;
	int *indices, *nverts;
	int i, index, osd_face_index;
	const float *float_args = NULL;
	/*        openSubdiv_evaluatorGetFloatTagArgs(ss->osd_evaluator); */

	/* If compute type changes, need to re-create GL Mesh.
	 * For now let's do evaluator as well, will optimize
	 * later.
	 */
	if (ss->osd_compute != U.opensubdiv_compute_type) {
		return true;
	}

	BLI_assert(ss->osd_evaluator != NULL);

	return false;

	/* Get the topology from existing evaluator. */
	/* TODO(sergey): Need proper port. */
	/*
	openSubdiv_getEvaluatorTopology(ss->osd_evaluator,
	                                &num_vertices,
	                                &refinement_level,
	                                &num_indices,
	                                &indices,
	                                &num_nverts,
	                                &nverts);
	*/

	/* Quick tests based on the number of subdiv level, verts and facces. */
	if (refinement_level != ss->subdivLevels ||
	    num_vertices != ss->vMap->numEntries ||
	    num_nverts != ss->fMap->numEntries)
	{
		return true;
	}

	/* Rather slow check for faces topology change. */
	for (i = 0, osd_face_index = 0, index = 0;
	     i < ss->fMap->curSize;
	     i++)
	{
		CCGFace *face = (CCGFace *) ss->fMap->buckets[i];
		for (; face; face = face->next, ++osd_face_index) {
			int S;

			if (face->numVerts != nverts[osd_face_index]) {
				return true;
			}

			for (S = 0; S < face->numVerts; ++S) {
				if (FACE_getVerts(face)[S]->osd_index != indices[index++]) {
					return true;
				}
			}
		}
	}

	/* For now we consider crease changes as a topology changes. */
	/* TODO(sergey): Currently optimized for creases only, if more
	 * tags are added this will break.
	 */
	for (i = 0, index = 0; i < ss->eMap->curSize; i++) {
		CCGEdge *e = (CCGEdge *) ss->eMap->buckets[i];
		for (; e; e = e->next, index++) {
			if (e->crease != float_args[index]) {
				return true;
			}
		}
	}

	return false;
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
		ss->osd_evaluator =
		        openSubdiv_createEvaluatorDescr(ss->dm,
		                                        ss->subdivLevels);
	} else {
		OSD_LOG("Re-using old evaluator\n");
	}
	return true;
}

static void opensubdiv_updateCoarsePositions(CCGSubSurf *ss)
{
	float (*positions)[3];
	int vertDataSize = ss->meshIFC.vertDataSize;
	int num_basis_verts = ss->vMap->numEntries;
	int i;

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
//#pragma omp parallel for
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

	/* TODO(sergey): Need proper port. */
	//openSubdiv_setEvaluatorCoarsePositions(ss->osd_evaluator,
	//                                       (float *) positions,
	//                                       num_basis_verts);

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

//#pragma omp parallel for
	for (i = 0; i < ss->vMap->curSize; ++i) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			float *no = VERT_getNo(v, 0);
			zero_v3(no);
		}
	}

//#pragma omp parallel for
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
//#pragma omp critical
			{
				for (S = 0; S < f->numVerts; S++) {
					CCGVert *v = FACE_getVerts(f)[S];
					float *no = VERT_getNo(v, 0);
					add_v3_v3(no, face_no);
				}
			}
		}
	}

//#pragma omp parallel for
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

//#pragma omp parallel for
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
					cross_v3_v3v3(no, dPdv, dPdu);
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
				cross_v3_v3v3(no, dPdv, dPdu);
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
//#pragma omp parallel for
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
					cross_v3_v3v3(no, dPdv, dPdu);
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

static void ccgSubSurf__sync(CCGSubSurf *ss)
{
	BLI_assert(ss->meshIFC.numLayers == 2 || ss->meshIFC.numLayers == 3);

	/* TODO(sergey): Apparently it's not supported by OpenSubdiv. */
	if (ss->fMap->numEntries == 0) {
		return;
	}

	ss->osd_coords_invalid = true;


	/* Make sure OSD evaluator is up-to-date. */
	if (opensubdiv_ensureEvaluator(ss)) {
		if (ss->skip_grids == false) {
			/* Update coarse points in the OpenSubdiv evaluator. */
			opensubdiv_updateCoarsePositions(ss);

			/* Evaluate opensubdiv mesh into the CCG grids. */
			opensubdiv_evaluateGrids(ss);
		}
		else {
			BLI_assert(ss->meshIFC.numLayers == 3);
			opensubdiv_updateCoarseNormals(ss);
		}
	}
	else {
		BLI_assert(!"OpenSubdiv initializetion failed, should not happen.");
	}

#ifdef DUMP_RESULT_GRIDS
	ccgSubSurf__dumpCoords(ss);
#endif
}

#  undef OSD_LOG
#else  /* WITH_OPENSUBDIV */
static void ccgSubSurf__sync(CCGSubSurf *ss)
{
	CCGVert **effectedV;
	CCGEdge **effectedE;
	CCGFace **effectedF;
	int numEffectedV, numEffectedE, numEffectedF;
	int subdivLevels = ss->subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize;
	int i, j, ptrIdx, S;
	int curLvl, nextLvl;
	void *q = ss->q, *r = ss->r;

	effectedV = MEM_mallocN(sizeof(*effectedV) * ss->vMap->numEntries, "CCGSubsurf effectedV");
	effectedE = MEM_mallocN(sizeof(*effectedE) * ss->eMap->numEntries, "CCGSubsurf effectedE");
	effectedF = MEM_mallocN(sizeof(*effectedF) * ss->fMap->numEntries, "CCGSubsurf effectedF");
	numEffectedV = numEffectedE = numEffectedF = 0;
	for (i = 0; i < ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			if (v->flags & Vert_eEffected) {
				effectedV[numEffectedV++] = v;

				for (j = 0; j < v->numEdges; j++) {
					CCGEdge *e = v->edges[j];
					if (!(e->flags & Edge_eEffected)) {
						effectedE[numEffectedE++] = e;
						e->flags |= Edge_eEffected;
					}
				}

				for (j = 0; j < v->numFaces; j++) {
					CCGFace *f = v->faces[j];
					if (!(f->flags & Face_eEffected)) {
						effectedF[numEffectedF++] = f;
						f->flags |= Face_eEffected;
					}
				}
			}
		}
	}

	curLvl = 0;
	nextLvl = curLvl + 1;

	for (ptrIdx = 0; ptrIdx < numEffectedF; ptrIdx++) {
		CCGFace *f = effectedF[ptrIdx];
		void *co = FACE_getCenterData(f);
		VertDataZero(co, ss);
		for (i = 0; i < f->numVerts; i++) {
			VertDataAdd(co, VERT_getCo(FACE_getVerts(f)[i], curLvl), ss);
		}
		VertDataMulN(co, 1.0f / f->numVerts, ss);

		f->flags = 0;
	}
	for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
		CCGEdge *e = effectedE[ptrIdx];
		void *co = EDGE_getCo(e, nextLvl, 1);
		float sharpness = EDGE_getSharpness(e, curLvl);

		if (_edge_isBoundary(e) || sharpness >= 1.0f) {
			VertDataCopy(co, VERT_getCo(e->v0, curLvl), ss);
			VertDataAdd(co, VERT_getCo(e->v1, curLvl), ss);
			VertDataMulN(co, 0.5f, ss);
		}
		else {
			int numFaces = 0;
			VertDataCopy(q, VERT_getCo(e->v0, curLvl), ss);
			VertDataAdd(q, VERT_getCo(e->v1, curLvl), ss);
			for (i = 0; i < e->numFaces; i++) {
				CCGFace *f = e->faces[i];
				VertDataAdd(q, (float *)FACE_getCenterData(f), ss);
				numFaces++;
			}
			VertDataMulN(q, 1.0f / (2.0f + numFaces), ss);

			VertDataCopy(r, VERT_getCo(e->v0, curLvl), ss);
			VertDataAdd(r, VERT_getCo(e->v1, curLvl), ss);
			VertDataMulN(r, 0.5f, ss);

			VertDataCopy(co, q, ss);
			VertDataSub(r, q, ss);
			VertDataMulN(r, sharpness, ss);
			VertDataAdd(co, r, ss);
		}

		/* edge flags cleared later */
	}
	for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
		CCGVert *v = effectedV[ptrIdx];
		void *co = VERT_getCo(v, curLvl);
		void *nCo = VERT_getCo(v, nextLvl);
		int sharpCount = 0, allSharp = 1;
		float avgSharpness = 0.0;
		int seam = VERT_seam(v), seamEdges = 0;

		for (i = 0; i < v->numEdges; i++) {
			CCGEdge *e = v->edges[i];
			float sharpness = EDGE_getSharpness(e, curLvl);

			if (seam && _edge_isBoundary(e))
				seamEdges++;

			if (sharpness != 0.0f) {
				sharpCount++;
				avgSharpness += sharpness;
			}
			else {
				allSharp = 0;
			}
		}

		if (sharpCount) {
			avgSharpness /= sharpCount;
			if (avgSharpness > 1.0f) {
				avgSharpness = 1.0f;
			}
		}

		if (seamEdges < 2 || seamEdges != v->numEdges)
			seam = 0;

		if (!v->numEdges || ss->meshIFC.simpleSubdiv) {
			VertDataCopy(nCo, co, ss);
		}
		else if (_vert_isBoundary(v)) {
			int numBoundary = 0;

			VertDataZero(r, ss);
			for (i = 0; i < v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				if (_edge_isBoundary(e)) {
					VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl), ss);
					numBoundary++;
				}
			}
			VertDataCopy(nCo, co, ss);
			VertDataMulN(nCo, 0.75f, ss);
			VertDataMulN(r, 0.25f / numBoundary, ss);
			VertDataAdd(nCo, r, ss);
		}
		else {
			int numEdges = 0, numFaces = 0;

			VertDataZero(q, ss);
			for (i = 0; i < v->numFaces; i++) {
				CCGFace *f = v->faces[i];
				VertDataAdd(q, (float *)FACE_getCenterData(f), ss);
				numFaces++;
			}
			VertDataMulN(q, 1.0f / numFaces, ss);
			VertDataZero(r, ss);
			for (i = 0; i < v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl), ss);
				numEdges++;
			}
			VertDataMulN(r, 1.0f / numEdges, ss);

			VertDataCopy(nCo, co, ss);
			VertDataMulN(nCo, numEdges - 2.0f, ss);
			VertDataAdd(nCo, q, ss);
			VertDataAdd(nCo, r, ss);
			VertDataMulN(nCo, 1.0f / numEdges, ss);
		}

		if (sharpCount > 1 || seam) {
			VertDataZero(q, ss);

			if (seam) {
				avgSharpness = 1.0f;
				sharpCount = seamEdges;
				allSharp = 1;
			}

			for (i = 0; i < v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				float sharpness = EDGE_getSharpness(e, curLvl);

				if (seam) {
					if (_edge_isBoundary(e)) {
						CCGVert *oV = _edge_getOtherVert(e, v);
						VertDataAdd(q, VERT_getCo(oV, curLvl), ss);
					}
				}
				else if (sharpness != 0.0f) {
					CCGVert *oV = _edge_getOtherVert(e, v);
					VertDataAdd(q, VERT_getCo(oV, curLvl), ss);
				}
			}

			VertDataMulN(q, (float) 1 / sharpCount, ss);

			if (sharpCount != 2 || allSharp) {
				/* q = q + (co - q) * avgSharpness */
				VertDataCopy(r, co, ss);
				VertDataSub(r, q, ss);
				VertDataMulN(r, avgSharpness, ss);
				VertDataAdd(q, r, ss);
			}

			/* r = co * 0.75 + q * 0.25 */
			VertDataCopy(r, co, ss);
			VertDataMulN(r, 0.75f, ss);
			VertDataMulN(q, 0.25f, ss);
			VertDataAdd(r, q, ss);

			/* nCo = nCo + (r - nCo) * avgSharpness */
			VertDataSub(r, nCo, ss);
			VertDataMulN(r, avgSharpness, ss);
			VertDataAdd(nCo, r, ss);
		}

		/* vert flags cleared later */
	}

	if (ss->useAgeCounts) {
		for (i = 0; i < numEffectedV; i++) {
			CCGVert *v = effectedV[i];
			byte *userData = ccgSubSurf_getVertUserData(ss, v);
			*((int *) &userData[ss->vertUserAgeOffset]) = ss->currentAge;
		}

		for (i = 0; i < numEffectedE; i++) {
			CCGEdge *e = effectedE[i];
			byte *userData = ccgSubSurf_getEdgeUserData(ss, e);
			*((int *) &userData[ss->edgeUserAgeOffset]) = ss->currentAge;
		}

		for (i = 0; i < numEffectedF; i++) {
			CCGFace *f = effectedF[i];
			byte *userData = ccgSubSurf_getFaceUserData(ss, f);
			*((int *) &userData[ss->faceUserAgeOffset]) = ss->currentAge;
		}
	}

	for (i = 0; i < numEffectedE; i++) {
		CCGEdge *e = effectedE[i];
		VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl), ss);
		VertDataCopy(EDGE_getCo(e, nextLvl, 2), VERT_getCo(e->v1, nextLvl), ss);
	}
	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];
		for (S = 0; S < f->numVerts; S++) {
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[(S + f->numVerts - 1) % f->numVerts];

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 1, 1), VERT_getCo(FACE_getVerts(f)[S], nextLvl), ss);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 1), EDGE_getCo(FACE_getEdges(f)[S], nextLvl, 1), ss);

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 1, 0), _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize), ss);
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 1), _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize), ss);
		}
	}

	for (curLvl = 1; curLvl < subdivLevels; curLvl++)
		ccgSubSurf__calcSubdivLevel(ss,
		                            effectedV, effectedE, effectedF,
		                            numEffectedV, numEffectedE, numEffectedF, curLvl);

	if (ss->calcVertNormals)
		ccgSubSurf__calcVertNormals(ss,
		                            effectedV, effectedE, effectedF,
		                            numEffectedV, numEffectedE, numEffectedF);

	for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
		CCGVert *v = effectedV[ptrIdx];
		v->flags = 0;
	}
	for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
		CCGEdge *e = effectedE[ptrIdx];
		e->flags = 0;
	}

	MEM_freeN(effectedF);
	MEM_freeN(effectedE);
	MEM_freeN(effectedV);

#ifdef DUMP_RESULT_GRIDS
	ccgSubSurf__dumpCoords(ss);
#endif
}
#endif  /* WITH_OPENSUBDIV */

static void ccgSubSurf__allFaces(CCGSubSurf *ss, CCGFace ***faces, int *numFaces, int *freeFaces)
{
	CCGFace **array;
	int i, num;

	if (*faces == NULL) {
		array = MEM_mallocN(sizeof(*array) * ss->fMap->numEntries, "CCGSubsurf allFaces");
		num = 0;
		for (i = 0; i < ss->fMap->curSize; i++) {
			CCGFace *f = (CCGFace *) ss->fMap->buckets[i];

			for (; f; f = f->next)
				array[num++] = f;
		}

		*faces = array;
		*numFaces = num;
		*freeFaces = 1;
	}
	else {
		*freeFaces = 0;
	}
}

static void ccgSubSurf__effectedFaceNeighbours(CCGSubSurf *ss, CCGFace **faces, int numFaces, CCGVert ***verts, int *numVerts, CCGEdge ***edges, int *numEdges)
{
	CCGVert **arrayV;
	CCGEdge **arrayE;
	int numV, numE, i, j;

	arrayV = MEM_mallocN(sizeof(*arrayV) * ss->vMap->numEntries, "CCGSubsurf arrayV");
	arrayE = MEM_mallocN(sizeof(*arrayE) * ss->eMap->numEntries, "CCGSubsurf arrayV");
	numV = numE = 0;

	for (i = 0; i < numFaces; i++) {
		CCGFace *f = faces[i];
		f->flags |= Face_eEffected;
	}

	for (i = 0; i < ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert *) ss->vMap->buckets[i];

		for (; v; v = v->next) {
			for (j = 0; j < v->numFaces; j++)
				if (!(v->faces[j]->flags & Face_eEffected))
					break;
			
			if (j == v->numFaces) {
				arrayV[numV++] = v;
				v->flags |= Vert_eEffected;
			}
		}
	}

	for (i = 0; i < ss->eMap->curSize; i++) {
		CCGEdge *e = (CCGEdge *) ss->eMap->buckets[i];

		for (; e; e = e->next) {
			for (j = 0; j < e->numFaces; j++)
				if (!(e->faces[j]->flags & Face_eEffected))
					break;
			
			if (j == e->numFaces) {
				e->flags |= Edge_eEffected;
				arrayE[numE++] = e;
			}
		}
	}

	*verts = arrayV;
	*numVerts = numV;
	*edges = arrayE;
	*numEdges = numE;
}

/* copy face grid coordinates to other places */
CCGError ccgSubSurf_updateFromFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	int i, S, x, gridSize, cornerIdx, subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize, freeF;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl) ? lvl : subdivLevels;
	gridSize = ccg_gridsize(lvl);
	cornerIdx = gridSize - 1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);

	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		for (S = 0; S < f->numVerts; S++) {
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[(S + f->numVerts - 1) % f->numVerts];

			VertDataCopy((float *)FACE_getCenterData(f), FACE_getIFCo(f, lvl, S, 0, 0), ss);
			VertDataCopy(VERT_getCo(FACE_getVerts(f)[S], lvl), FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), ss);

			for (x = 0; x < gridSize; x++)
				VertDataCopy(FACE_getIECo(f, lvl, S, x), FACE_getIFCo(f, lvl, S, x, 0), ss);

			for (x = 0; x < gridSize; x++) {
				int eI = gridSize - 1 - x;
				VertDataCopy(_edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, cornerIdx, x), ss);
				VertDataCopy(_edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, x, cornerIdx), ss);
			}
		}
	}

	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* copy other places to face grid coordinates */
CCGError ccgSubSurf_updateToFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	int i, S, x, gridSize, cornerIdx, subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize, freeF;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl) ? lvl : subdivLevels;
	gridSize = ccg_gridsize(lvl);
	cornerIdx = gridSize - 1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);

	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		for (S = 0; S < f->numVerts; S++) {
			int prevS = (S + f->numVerts - 1) % f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			for (x = 0; x < gridSize; x++) {
				int eI = gridSize - 1 - x;
				VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
			}

			for (x = 1; x < gridSize - 1; x++) {
				VertDataCopy(FACE_getIFCo(f, lvl, S, 0, x), FACE_getIECo(f, lvl, prevS, x), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, 0), FACE_getIECo(f, lvl, S, x), ss);
			}

			VertDataCopy(FACE_getIFCo(f, lvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], lvl), ss);
		}
	}

	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* stitch together face grids, averaging coordinates at edges
 * and vertices, for multires displacements */
CCGError ccgSubSurf_stitchFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	CCGVert **effectedV;
	CCGEdge **effectedE;
	int numEffectedV, numEffectedE, freeF;
	int i, S, x, gridSize, cornerIdx, subdivLevels, edgeSize;
	int vertDataSize = ss->meshIFC.vertDataSize;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl) ? lvl : subdivLevels;
	gridSize = ccg_gridsize(lvl);
	edgeSize = ccg_edgesize(lvl);
	cornerIdx = gridSize - 1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
	ccgSubSurf__effectedFaceNeighbours(ss, effectedF, numEffectedF,
	                                   &effectedV, &numEffectedV, &effectedE, &numEffectedE);

	/* zero */
	for (i = 0; i < numEffectedV; i++) {
		CCGVert *v = effectedV[i];
		if (v->numFaces)
			VertDataZero(VERT_getCo(v, lvl), ss);
	}

	for (i = 0; i < numEffectedE; i++) {
		CCGEdge *e = effectedE[i];

		if (e->numFaces)
			for (x = 0; x < edgeSize; x++)
				VertDataZero(EDGE_getCo(e, lvl, x), ss);
	}

	/* add */
	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		VertDataZero((float *)FACE_getCenterData(f), ss);

		for (S = 0; S < f->numVerts; S++)
			for (x = 0; x < gridSize; x++)
				VertDataZero(FACE_getIECo(f, lvl, S, x), ss);

		for (S = 0; S < f->numVerts; S++) {
			int prevS = (S + f->numVerts - 1) % f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			VertDataAdd((float *)FACE_getCenterData(f), FACE_getIFCo(f, lvl, S, 0, 0), ss);
			if (FACE_getVerts(f)[S]->flags & Vert_eEffected)
				VertDataAdd(VERT_getCo(FACE_getVerts(f)[S], lvl), FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), ss);

			for (x = 1; x < gridSize - 1; x++) {
				VertDataAdd(FACE_getIECo(f, lvl, S, x), FACE_getIFCo(f, lvl, S, x, 0), ss);
				VertDataAdd(FACE_getIECo(f, lvl, prevS, x), FACE_getIFCo(f, lvl, S, 0, x), ss);
			}

			for (x = 0; x < gridSize - 1; x++) {
				int eI = gridSize - 1 - x;
				if (FACE_getEdges(f)[S]->flags & Edge_eEffected)
					VertDataAdd(_edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, cornerIdx, x), ss);
				if (FACE_getEdges(f)[prevS]->flags & Edge_eEffected)
					if (x != 0)
						VertDataAdd(_edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), FACE_getIFCo(f, lvl, S, x, cornerIdx), ss);
			}
		}
	}

	/* average */
	for (i = 0; i < numEffectedV; i++) {
		CCGVert *v = effectedV[i];
		if (v->numFaces)
			VertDataMulN(VERT_getCo(v, lvl), 1.0f / v->numFaces, ss);
	}

	for (i = 0; i < numEffectedE; i++) {
		CCGEdge *e = effectedE[i];

		VertDataCopy(EDGE_getCo(e, lvl, 0), VERT_getCo(e->v0, lvl), ss);
		VertDataCopy(EDGE_getCo(e, lvl, edgeSize - 1), VERT_getCo(e->v1, lvl), ss);

		if (e->numFaces)
			for (x = 1; x < edgeSize - 1; x++)
				VertDataMulN(EDGE_getCo(e, lvl, x), 1.0f / e->numFaces, ss);
	}

	/* copy */
	for (i = 0; i < numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		VertDataMulN((float *)FACE_getCenterData(f), 1.0f / f->numVerts, ss);

		for (S = 0; S < f->numVerts; S++)
			for (x = 1; x < gridSize - 1; x++)
				VertDataMulN(FACE_getIECo(f, lvl, S, x), 0.5f, ss);

		for (S = 0; S < f->numVerts; S++) {
			int prevS = (S + f->numVerts - 1) % f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			VertDataCopy(FACE_getIFCo(f, lvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], lvl), ss);

			for (x = 1; x < gridSize - 1; x++) {
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, 0), FACE_getIECo(f, lvl, S, x), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, 0, x), FACE_getIECo(f, lvl, prevS, x), ss);
			}

			for (x = 0; x < gridSize - 1; x++) {
				int eI = gridSize - 1 - x;

				VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI, vertDataSize), ss);
			}

			VertDataCopy(FACE_getIECo(f, lvl, S, 0), (float *)FACE_getCenterData(f), ss);
			VertDataCopy(FACE_getIECo(f, lvl, S, gridSize - 1), FACE_getIFCo(f, lvl, S, gridSize - 1, 0), ss);
		}
	}

	for (i = 0; i < numEffectedV; i++)
		effectedV[i]->flags = 0;
	for (i = 0; i < numEffectedE; i++)
		effectedE[i]->flags = 0;
	for (i = 0; i < numEffectedF; i++)
		effectedF[i]->flags = 0;

	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* update normals for specified faces */
CCGError ccgSubSurf_updateNormals(CCGSubSurf *ss, CCGFace **effectedF, int numEffectedF)
{
	CCGVert **effectedV;
	CCGEdge **effectedE;
	int i, numEffectedV, numEffectedE, freeF;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
	ccgSubSurf__effectedFaceNeighbours(ss, effectedF, numEffectedF,
	                                   &effectedV, &numEffectedV, &effectedE, &numEffectedE);

	if (ss->calcVertNormals)
		ccgSubSurf__calcVertNormals(ss,
		                            effectedV, effectedE, effectedF,
		                            numEffectedV, numEffectedE, numEffectedF);

	for (i = 0; i < numEffectedV; i++)
		effectedV[i]->flags = 0;
	for (i = 0; i < numEffectedE; i++)
		effectedE[i]->flags = 0;
	for (i = 0; i < numEffectedF; i++)
		effectedF[i]->flags = 0;

	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* compute subdivision levels from a given starting point, used by
 * multires subdivide/propagate, by filling in coordinates at a
 * certain level, and then subdividing that up to the highest level */
CCGError ccgSubSurf_updateLevels(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	CCGVert **effectedV;
	CCGEdge **effectedE;
	int numEffectedV, numEffectedE, freeF, i;
	int curLvl, subdivLevels = ss->subdivLevels;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
	ccgSubSurf__effectedFaceNeighbours(ss, effectedF, numEffectedF,
	                                   &effectedV, &numEffectedV, &effectedE, &numEffectedE);

	for (curLvl = lvl; curLvl < subdivLevels; curLvl++) {
		ccgSubSurf__calcSubdivLevel(ss,
		                            effectedV, effectedE, effectedF,
		                            numEffectedV, numEffectedE, numEffectedF, curLvl);
	}

	for (i = 0; i < numEffectedV; i++)
		effectedV[i]->flags = 0;
	for (i = 0; i < numEffectedE; i++)
		effectedE[i]->flags = 0;
	for (i = 0; i < numEffectedF; i++)
		effectedF[i]->flags = 0;

	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
	if (freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

#undef VERT_getCo
#undef EDGE_getCo
#undef FACE_getIECo
#undef FACE_getIFCo

/*** External API accessor functions ***/

int ccgSubSurf_getNumVerts(const CCGSubSurf *ss)
{
	return ss->vMap->numEntries;
}
int ccgSubSurf_getNumEdges(const CCGSubSurf *ss)
{
	return ss->eMap->numEntries;
}
int ccgSubSurf_getNumFaces(const CCGSubSurf *ss)
{
	return ss->fMap->numEntries;
}

CCGVert *ccgSubSurf_getVert(CCGSubSurf *ss, CCGVertHDL v)
{
	return (CCGVert *) _ehash_lookup(ss->vMap, v);
}
CCGEdge *ccgSubSurf_getEdge(CCGSubSurf *ss, CCGEdgeHDL e)
{
	return (CCGEdge *) _ehash_lookup(ss->eMap, e);
}
CCGFace *ccgSubSurf_getFace(CCGSubSurf *ss, CCGFaceHDL f)
{
	return (CCGFace *) _ehash_lookup(ss->fMap, f);
}

int ccgSubSurf_getSubdivisionLevels(const CCGSubSurf *ss)
{
	return ss->subdivLevels;
}
int ccgSubSurf_getEdgeSize(const CCGSubSurf *ss)
{
	return ccgSubSurf_getEdgeLevelSize(ss, ss->subdivLevels);
}
int ccgSubSurf_getEdgeLevelSize(const CCGSubSurf *ss, int level)
{
	if (level < 1 || level > ss->subdivLevels) {
		return -1;
	}
	else {
		return ccg_edgesize(level);
	}
}
int ccgSubSurf_getGridSize(const CCGSubSurf *ss)
{
	return ccgSubSurf_getGridLevelSize(ss, ss->subdivLevels);
}
int ccgSubSurf_getGridLevelSize(const CCGSubSurf *ss, int level)
{
	if (level < 1 || level > ss->subdivLevels) {
		return -1;
	}
	else {
		return ccg_gridsize(level);
	}
}

int ccgSubSurf_getSimpleSubdiv(const CCGSubSurf *ss)
{
	return ss->meshIFC.simpleSubdiv;
}

/* Vert accessors */

CCGVertHDL ccgSubSurf_getVertVertHandle(CCGVert *v)
{
	return v->vHDL;
}
int ccgSubSurf_getVertAge(CCGSubSurf *ss, CCGVert *v)
{
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getVertUserData(ss, v);
		return ss->currentAge - *((int *) &userData[ss->vertUserAgeOffset]);
	}
	else {
		return 0;
	}
}
void *ccgSubSurf_getVertUserData(CCGSubSurf *ss, CCGVert *v)
{
	return VERT_getLevelData(v) + ss->meshIFC.vertDataSize * (ss->subdivLevels + 1);
}
int ccgSubSurf_getVertNumFaces(CCGVert *v)
{
	return v->numFaces;
}
CCGFace *ccgSubSurf_getVertFace(CCGVert *v, int index)
{
	if (index < 0 || index >= v->numFaces) {
		return NULL;
	}
	else {
		return v->faces[index];
	}
}
int ccgSubSurf_getVertNumEdges(CCGVert *v)
{
	return v->numEdges;
}
CCGEdge *ccgSubSurf_getVertEdge(CCGVert *v, int index)
{
	if (index < 0 || index >= v->numEdges) {
		return NULL;
	}
	else {
		return v->edges[index];
	}
}
void *ccgSubSurf_getVertData(CCGSubSurf *ss, CCGVert *v)
{
	return ccgSubSurf_getVertLevelData(ss, v, ss->subdivLevels);
}
void *ccgSubSurf_getVertLevelData(CCGSubSurf *ss, CCGVert *v, int level)
{
	if (level < 0 || level > ss->subdivLevels) {
		return NULL;
	}
	else {
		return _vert_getCo(v, level, ss->meshIFC.vertDataSize);
	}
}

/* Edge accessors */

CCGEdgeHDL ccgSubSurf_getEdgeEdgeHandle(CCGEdge *e)
{
	return e->eHDL;
}
int ccgSubSurf_getEdgeAge(CCGSubSurf *ss, CCGEdge *e)
{
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getEdgeUserData(ss, e);
		return ss->currentAge - *((int *) &userData[ss->edgeUserAgeOffset]);
	}
	else {
		return 0;
	}
}
void *ccgSubSurf_getEdgeUserData(CCGSubSurf *ss, CCGEdge *e)
{
	return (EDGE_getLevelData(e) +
	        ss->meshIFC.vertDataSize * ccg_edgebase(ss->subdivLevels + 1));
}
int ccgSubSurf_getEdgeNumFaces(CCGEdge *e)
{
	return e->numFaces;
}
CCGFace *ccgSubSurf_getEdgeFace(CCGEdge *e, int index)
{
	if (index < 0 || index >= e->numFaces) {
		return NULL;
	}
	else {
		return e->faces[index];
	}
}
CCGVert *ccgSubSurf_getEdgeVert0(CCGEdge *e)
{
	return e->v0;
}
CCGVert *ccgSubSurf_getEdgeVert1(CCGEdge *e)
{
	return e->v1;
}
void *ccgSubSurf_getEdgeDataArray(CCGSubSurf *ss, CCGEdge *e)
{
	return ccgSubSurf_getEdgeData(ss, e, 0);
}
void *ccgSubSurf_getEdgeData(CCGSubSurf *ss, CCGEdge *e, int x)
{
	return ccgSubSurf_getEdgeLevelData(ss, e, x, ss->subdivLevels);
}
void *ccgSubSurf_getEdgeLevelData(CCGSubSurf *ss, CCGEdge *e, int x, int level)
{
	if (level < 0 || level > ss->subdivLevels) {
		return NULL;
	}
	else {
		return _edge_getCo(e, level, x, ss->meshIFC.vertDataSize);
	}
}
float ccgSubSurf_getEdgeCrease(CCGEdge *e)
{
	return e->crease;
}

/* Face accessors */

CCGFaceHDL ccgSubSurf_getFaceFaceHandle(CCGFace *f)
{
	return f->fHDL;
}
int ccgSubSurf_getFaceAge(CCGSubSurf *ss, CCGFace *f)
{
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getFaceUserData(ss, f);
		return ss->currentAge - *((int *) &userData[ss->faceUserAgeOffset]);
	}
	else {
		return 0;
	}
}
void *ccgSubSurf_getFaceUserData(CCGSubSurf *ss, CCGFace *f)
{
	int maxGridSize = ccg_gridsize(ss->subdivLevels);
	return FACE_getCenterData(f) + ss->meshIFC.vertDataSize * (1 + f->numVerts * maxGridSize + f->numVerts * maxGridSize * maxGridSize);
}
int ccgSubSurf_getFaceNumVerts(CCGFace *f)
{
	return f->numVerts;
}
CCGVert *ccgSubSurf_getFaceVert(CCGFace *f, int index)
{
	if (index < 0 || index >= f->numVerts) {
		return NULL;
	}
	else {
		return FACE_getVerts(f)[index];
	}
}
CCGEdge *ccgSubSurf_getFaceEdge(CCGFace *f, int index)
{
	if (index < 0 || index >= f->numVerts) {
		return NULL;
	}
	else {
		return FACE_getEdges(f)[index];
	}
}
int ccgSubSurf_getFaceEdgeIndex(CCGFace *f, CCGEdge *e)
{
	int i;

	for (i = 0; i < f->numVerts; i++) {
		if (FACE_getEdges(f)[i] == e) {
			return i;
		}
	}
	return -1;
}
void *ccgSubSurf_getFaceCenterData(CCGFace *f)
{
	return FACE_getCenterData(f);
}
void *ccgSubSurf_getFaceGridEdgeDataArray(CCGSubSurf *ss, CCGFace *f, int gridIndex)
{
	return ccgSubSurf_getFaceGridEdgeData(ss, f, gridIndex, 0);
}
void *ccgSubSurf_getFaceGridEdgeData(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x)
{
	return _face_getIECo(f, ss->subdivLevels, gridIndex, x, ss->subdivLevels, ss->meshIFC.vertDataSize);
}
void *ccgSubSurf_getFaceGridDataArray(CCGSubSurf *ss, CCGFace *f, int gridIndex)
{
	return ccgSubSurf_getFaceGridData(ss, f, gridIndex, 0, 0);
}
void *ccgSubSurf_getFaceGridData(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x, int y)
{
	return _face_getIFCo(f, ss->subdivLevels, gridIndex, x, y, ss->subdivLevels, ss->meshIFC.vertDataSize);
}

/*** External API iterator functions ***/

void ccgSubSurf_initVertIterator(CCGSubSurf *ss, CCGVertIterator *viter)
{
	_ehashIterator_init(ss->vMap, viter);
}
void ccgSubSurf_initEdgeIterator(CCGSubSurf *ss, CCGEdgeIterator *eiter)
{
	_ehashIterator_init(ss->eMap, eiter);
}
void ccgSubSurf_initFaceIterator(CCGSubSurf *ss, CCGFaceIterator *fiter)
{
	_ehashIterator_init(ss->fMap, fiter);
}

CCGVert *ccgVertIterator_getCurrent(CCGVertIterator *vi)
{
	return (CCGVert *) _ehashIterator_getCurrent((EHashIterator *) vi);
}
int ccgVertIterator_isStopped(CCGVertIterator *vi)
{
	return _ehashIterator_isStopped((EHashIterator *) vi);
}
void ccgVertIterator_next(CCGVertIterator *vi)
{
	_ehashIterator_next((EHashIterator *) vi);
}

CCGEdge *ccgEdgeIterator_getCurrent(CCGEdgeIterator *vi)
{
	return (CCGEdge *) _ehashIterator_getCurrent((EHashIterator *) vi);
}
int ccgEdgeIterator_isStopped(CCGEdgeIterator *vi)
{
	return _ehashIterator_isStopped((EHashIterator *) vi);
}
void ccgEdgeIterator_next(CCGEdgeIterator *vi)
{
	_ehashIterator_next((EHashIterator *) vi);
}

CCGFace *ccgFaceIterator_getCurrent(CCGFaceIterator *vi)
{
	return (CCGFace *) _ehashIterator_getCurrent((EHashIterator *) vi);
}
int ccgFaceIterator_isStopped(CCGFaceIterator *vi)
{
	return _ehashIterator_isStopped((EHashIterator *) vi);
}
void ccgFaceIterator_next(CCGFaceIterator *vi)
{
	_ehashIterator_next((EHashIterator *) vi);
}

/*** Extern API final vert/edge/face interface ***/

int ccgSubSurf_getNumFinalVerts(const CCGSubSurf *ss)
{
	int edgeSize = ccg_edgesize(ss->subdivLevels);
	int gridSize = ccg_gridsize(ss->subdivLevels);
	int numFinalVerts = (ss->vMap->numEntries +
	                     ss->eMap->numEntries * (edgeSize - 2) +
	                     ss->fMap->numEntries +
	                     ss->numGrids * ((gridSize - 2) + ((gridSize - 2) * (gridSize - 2))));

	return numFinalVerts;
}
int ccgSubSurf_getNumFinalEdges(const CCGSubSurf *ss)
{
	int edgeSize = ccg_edgesize(ss->subdivLevels);
	int gridSize = ccg_gridsize(ss->subdivLevels);
	int numFinalEdges = (ss->eMap->numEntries * (edgeSize - 1) +
	                     ss->numGrids * ((gridSize - 1) + 2 * ((gridSize - 2) * (gridSize - 1))));

	return numFinalEdges;
}
int ccgSubSurf_getNumFinalFaces(const CCGSubSurf *ss)
{
	int gridSize = ccg_gridsize(ss->subdivLevels);
	int numFinalFaces = ss->numGrids * ((gridSize - 1) * (gridSize - 1));
	return numFinalFaces;
}

/***/

void CCG_key(CCGKey *key, const CCGSubSurf *ss, int level)
{
	key->level = level;
	
	key->elem_size = ss->meshIFC.vertDataSize;
	key->has_normals = ss->calcVertNormals;
	key->num_layers = ss->meshIFC.numLayers;
	
	/* if normals are present, always the last three floats of an
	 * element */
	if (key->has_normals)
		key->normal_offset = key->elem_size - sizeof(float) * 3;
	else
		key->normal_offset = -1;

	key->grid_size = ccgSubSurf_getGridLevelSize(ss, level);
	key->grid_area = key->grid_size * key->grid_size;
	key->grid_bytes = key->elem_size * key->grid_area;

	key->has_mask = ss->allocMask;
	if (key->has_mask)
		key->mask_offset = ss->maskDataOffset;
	else
		key->mask_offset = -1;
}

void CCG_key_top_level(CCGKey *key, const CCGSubSurf *ss)
{
	CCG_key(key, ss, ccgSubSurf_getSubdivisionLevels(ss));
}
