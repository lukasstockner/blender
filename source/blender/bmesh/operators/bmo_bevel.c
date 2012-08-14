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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_bevel.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_smallhash.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */
#include "intern/bmesh_private.h"


#define BEVEL_FLAG	1
#define BEVEL_DEL	2
#define FACE_NEW	4
#define EDGE_OLD	8
#define FACE_OLD	16
#define VERT_OLD	32
#define FACE_SPAN	64
#define FACE_HOLE	128

#define EDGE_SELECTED 256

#define BEVEL_EPSILON  1e-6

typedef struct LoopTag {
	BMVert *newv;
} LoopTag;

typedef struct EdgeTag {
	BMVert *newv1, *newv2;
} EdgeTag;

/* this structure is item of new vertices list */
typedef struct NewVertexItem {
	struct NewVertexItem *next, *prev;
	BMVert *v;
} NewVertexItem;

/* Item in the list of additional vertices */
typedef struct VertexItem{
	struct VertexItem *next, *prev;
	BMVert *v;		/* parent vertex */
	int onEdge;		/*	1 if new vertex located on edge; edge1 = edge, edge2 = NULL
					*	0 if new vert located betwen edge1 and edge2
						3 additional vert for rounding case	*/
	BMEdge *edge1;
	BMEdge *edge2;
	BMFace *f;
	float hv[3];	/* coordinate of support vertex */
}VertexItem;

/* list of new vertices formed around v */
typedef struct AdditionalVert{
	struct AdditionalVert *next, *prev;
	BMVert *v;			/* parrent vertex */
	ListBase vertices;  /* List of auxiliary vertices */
	int count;			/* count input edges, alse count additioanl vertex */
	int countSelect;	/* count input selection edges */
} AdditionalVert;

/*
* struct with bevel parametrs
*/
typedef struct BevelParams{
	ListBase vertList;		/* list additional vertex */
	ListBase newVertList;	/* list of creation vertices */
	float offset;
	int byPolygon;			/* 1 - make bevel on each polygon, 0 - ignore internal polygon */
	int seg;				/* segmentation */

	BMOperator *op;
} BevelParams;


typedef struct SurfaceEdgeData {
	BMEdge *e;
	BMVert *a, *b;
	BMVert *boundaryA, *boundaryB;
	ListBase vertexList;
	int count;
	float h[3];

} SurfaceEdgeData;

BMVert* bevel_create_unic_vertex(BMesh *bm, BevelParams *bp, float co[3]);

static void calc_corner_co(BMLoop *l, const float fac, float r_co[3],
                           const short do_dist, const short do_even)
{
	float  no[3], l_vec_prev[3], l_vec_next[3], l_co_prev[3], l_co[3], l_co_next[3], co_ofs[3];
	int is_concave;

	/* first get the prev/next verts */
	if (l->f->len > 2) {
		copy_v3_v3(l_co_prev, l->prev->v->co);
		copy_v3_v3(l_co, l->v->co);
		copy_v3_v3(l_co_next, l->next->v->co);

		/* calculate normal */
		sub_v3_v3v3(l_vec_prev, l_co_prev, l_co);
		sub_v3_v3v3(l_vec_next, l_co_next, l_co);

		cross_v3_v3v3(no, l_vec_prev, l_vec_next);
		is_concave = dot_v3v3(no, l->f->no) > 0.0f;
	}
	else {
		BMIter iter;
		BMLoop *l2;
		float up[3] = {0.0f, 0.0f, 1.0f};

		copy_v3_v3(l_co_prev, l->prev->v->co);
		copy_v3_v3(l_co, l->v->co);
		
		BM_ITER_ELEM (l2, &iter, l->v, BM_LOOPS_OF_VERT) {
			if (l2->f != l->f) {
				copy_v3_v3(l_co_next, BM_edge_other_vert(l2->e, l2->next->v)->co);
				break;
			}
		}
		
		sub_v3_v3v3(l_vec_prev, l_co_prev, l_co);
		sub_v3_v3v3(l_vec_next, l_co_next, l_co);

		cross_v3_v3v3(no, l_vec_prev, l_vec_next);
		if (dot_v3v3(no, no) == 0.0f) {
			no[0] = no[1] = 0.0f; no[2] = -1.0f;
		}
		
		is_concave = dot_v3v3(no, up) < 0.0f;
	}


	/* now calculate the new location */
	if (do_dist) { /* treat 'fac' as distance */

		normalize_v3(l_vec_prev);
		normalize_v3(l_vec_next);

		add_v3_v3v3(co_ofs, l_vec_prev, l_vec_next);
		if (UNLIKELY(normalize_v3(co_ofs) == 0.0f)) {  /* edges form a straight line */
			cross_v3_v3v3(co_ofs, l_vec_prev, l->f->no);
		}

		if (do_even) {
			negate_v3(l_vec_next);
			mul_v3_fl(co_ofs, fac * shell_angle_to_dist(0.5f * angle_normalized_v3v3(l_vec_prev, l_vec_next)));
			/* negate_v3(l_vec_next); */ /* no need unless we use again */
		}
		else {
			mul_v3_fl(co_ofs, fac);
		}
	}
	else { /* treat as 'fac' as a factor (0 - 1) */

		/* not strictly necessary, balance vectors
		 * so the longer edge doesn't skew the result,
		 * gives nicer, move even output.
		 *
		 * Use the minimum rather then the middle value so skinny faces don't flip along the short axis */
		float min_fac = minf(normalize_v3(l_vec_prev), normalize_v3(l_vec_next));
		float angle;

		if (do_even) {
			negate_v3(l_vec_next);
			angle = angle_normalized_v3v3(l_vec_prev, l_vec_next);
			negate_v3(l_vec_next); /* no need unless we use again */
		}
		else {
			angle = 0.0f;
		}

		mul_v3_fl(l_vec_prev, min_fac);
		mul_v3_fl(l_vec_next, min_fac);

		add_v3_v3v3(co_ofs, l_vec_prev, l_vec_next);

		if (UNLIKELY(is_zero_v3(co_ofs))) {
			cross_v3_v3v3(co_ofs, l_vec_prev, l->f->no);
			normalize_v3(co_ofs);
			mul_v3_fl(co_ofs, min_fac);
		}

		/* done */
		if (do_even) {
			mul_v3_fl(co_ofs, (fac * 0.5f) * shell_angle_to_dist(0.5f * angle));
		}
		else {
			mul_v3_fl(co_ofs, fac * 0.5f);
		}
	}

	/* apply delta vec */
	if (is_concave)
		negate_v3(co_ofs);

	add_v3_v3v3(r_co, co_ofs, l->v->co);
}


#define ETAG_SET(e, v, nv)  (                                                 \
	(v) == (e)->v1 ?                                                          \
		(etags[BM_elem_index_get((e))].newv1 = (nv)) :                        \
		(etags[BM_elem_index_get((e))].newv2 = (nv))                          \
	)

#define ETAG_GET(e, v)  (                                                     \
	(v) == (e)->v1 ?                                                          \
		(etags[BM_elem_index_get((e))].newv1) :                               \
		(etags[BM_elem_index_get((e))].newv2)                                 \
	)

/* build point on edge
*  sEdge - selectes edge */
BMVert* bevel_calc_aditional_vert(BMesh *bm, BevelParams *bp, BMEdge *sEdge, BMEdge* edge, BMVert* v)
{
	BMVert *new_Vert = NULL;
/*	float vect[3], normV[3];
	sub_v3_v3v3(vect, BM_edge_other_vert(edge, v)->co, v->co);
	normalize_v3_v3(normV, vect);
	mul_v3_fl(normV, bp->offset);

	add_v3_v3(normV, v->co);

	new_Vert = bevel_create_unic_vertex(bm, bp, normV);
	return new_Vert;*/

	float ve[3], sve[3], angle, lenght;

	sub_v3_v3v3(ve, BM_edge_other_vert(edge, v)->co, v->co);
	sub_v3_v3v3(sve, BM_edge_other_vert(sEdge, v)->co, v->co);

	angle = angle_v3v3(ve, sve);
	lenght = bp->offset / sin(angle);
	normalize_v3(ve);
	mul_v3_fl(ve, lenght);
	add_v3_v3(ve, v->co);

	new_Vert = bevel_create_unic_vertex(bm, bp, ve);

	return new_Vert;
}

BMVert* bevel_create_unic_vertex(BMesh *bm, BevelParams *bp, float co[3])
{
	float epsilon = 1e-6;
	BMVert *v = NULL;
	NewVertexItem *item;
	for (item = bp->newVertList.first; item; item = item->next) {
		if (compare_v3v3(item->v->co, co, epsilon))
			v = item->v;
	}
	if (!v) {
		item = (NewVertexItem*)MEM_callocN(sizeof(NewVertexItem), "VertexItem");
		item->v = BM_vert_create(bm, co, NULL);
		BLI_addtail(&bp->newVertList, item);
		v = item->v;
	}
	return v;
}

/*
* build point between edges
*/
BMVert* bevel_middle_vert(BMesh *bm, BevelParams *bp, BMEdge *edge_a, BMEdge *edge_b, BMVert *vert)
{
	float offset, v_a[3], v_b[3], v_c[3], norm_a[3], norm_b[3],norm_c[3],  angel;
	float co[3];
	BMVert* new_vert = NULL;
	offset = bp->offset;
	/* calc vectors */
	sub_v3_v3v3(v_a, BM_edge_other_vert(edge_a, vert)->co, vert->co);
	sub_v3_v3v3(v_b, BM_edge_other_vert(edge_b, vert)->co, vert->co);
	normalize_v3_v3(norm_a, v_a);
	normalize_v3_v3(norm_b, v_b);

	add_v3_v3v3(v_c, norm_a, norm_b);
	normalize_v3_v3(norm_c, v_c);
	angel = angle_v3v3(norm_a, norm_b);
	mul_v3_fl(norm_c, offset / sin(angel/2));
	add_v3_v3(norm_c, vert->co);
	/*
	mul_v3_fl(v_c, 0.5);
	normalize_v3_v3(norm_c, v_c);

	// v_c ^ edge_a
	//cos = (v_c[0]*v_a[0] + v_c[1]*v_a[1] + v_c[2]*v_a[0]) / (len_v3(v_c)*len_v3(v_a));
	angel = angle_normalized_v3v3(norm_c, norm_a);
	//offset = offset / (sqrt(1-cos*cos));
	offset = offset / sin(angel);

	mul_v3_fl(norm_c, offset);
	add_v3_v3(norm_c, vert->co);
	*/

	new_vert = BM_vert_create(bm, norm_c, NULL);

	return new_vert;
}

/*
* looking neighboring unselected Edge at the face
*/
BMEdge* find_non_selected_adjacent_edge(BMesh *bm, BMFace *f, BMEdge *e, BMVert *v){
	BMEdge *oe = NULL;
	BMLoop *l = f->l_first;
	do {
		if (!(BMO_elem_flag_test(bm, l->e, EDGE_SELECTED)) &&
			(l->e != e) &&
				((l->e->v1 == v ) ||
				 BM_edge_other_vert(l->e, l->e->v1) == v  )) {
			oe = l->e;
		}
		l = l->next;
	} while (l != f->l_first);
	return oe;
}

/*
* e - selected edge
* return other selected edge
*/
BMEdge* find_selected_edge_in_face(BMesh *bm, BMFace *f, BMEdge *e, BMVert *v)
{
	BMEdge *oe = NULL;
	BMLoop *l = f->l_first;
	do {
		if (BMO_elem_flag_test(bm, l->e, EDGE_SELECTED) &&
			(l->e != e) &&
				((l->e->v1 == v ) ||
				 BM_edge_other_vert(l->e, l->e->v1) == v  )) {
			oe = l->e;
		}
		l = l->next;
	} while (l != f->l_first);
	return oe;
}

BMEdge* find_selected_edge_in_av(BMesh *bm, BevelParams *bp, AdditionalVert *av)
{
	BMEdge *result = NULL, *e;
	BMOIter siter;
	BMO_ITER (e, &siter, bm, bp->op, "geom", BM_EDGE) {
		if ( (BMO_elem_flag_test(bm, e, EDGE_SELECTED)) &&
			 ((e->v1 == av->v) || (BM_edge_other_vert(e, e->v1) == av->v)))
			result = e;
	}

	/*e = bmesh_disk_faceedge_find_first(ed, av->v);
	do {
		e = bmesh_disk_edge_next(e, av->v);
		if (BMO_elem_flag_test(bm, e, EDGE_SELECTED))
			result = e;
	} while (e != ed);*/
	return result;
}

int check_dublicated_vertex_item(AdditionalVert *item, BMFace *f)
{
	VertexItem *vItem;
	int result = 0;
	for (vItem = item->vertices.first; vItem; vItem = vItem->next) {
		if (vItem->f == f)
			result = 1;
	}
	return result;
}

VertexItem* calc_support_vertex(BevelParams* bp, BMEdge *e, BMVert *v)
{
	VertexItem *item;
	float vect[3], normV[3];

	item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
	item->onEdge = 3;
	item->edge1 = e;
	item->edge2 = NULL;
	item->f = NULL;
	item->v = NULL;

	sub_v3_v3v3(vect, BM_edge_other_vert(e, v)->co, v->co);
	normalize_v3_v3(normV, vect);
	mul_v3_fl(normV, bp->offset);
	add_v3_v3(normV, v->co);

	copy_v3_v3(item->hv, normV);
	return item;
}

int check_dublicated_vertex_item_by_edge(AdditionalVert *av, BMEdge* edge)
{
	VertexItem *vitem;
	int result = 0;
	for (vitem = av->vertices.first; vitem; vitem = vitem->next) {
		if  ((vitem->onEdge == 1) &&
			(vitem->edge1 == edge))
			result = 1;
	}
	return result;

}/*
* additional construction arround the vertex
*/
void bevel_aditional_construction_by_vert(BMesh *bm, BevelParams *bp, BMOperator *op, BMVert *v)
{

	BMOIter siter;
	//BMIter iter;
	BMEdge *e, **edges = NULL;
	int i;
	BLI_array_declare(edges);

	// calc count input selected edges
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		if ((e->v1 == v)|| (BM_edge_other_vert(e, e->v1) == v))
		{
			BMO_elem_flag_enable (bm, e, EDGE_SELECTED);
			BLI_array_append(edges, e);
		}
	}

	if (BLI_array_count(edges) > 0) {
		AdditionalVert *av;
		av = (AdditionalVert*)MEM_callocN(sizeof(AdditionalVert), "AdditionalVert");
		av->v = v;
		av->count = 0;
		av->countSelect = 0;
		av->vertices.first = av->vertices.last = NULL;
		BLI_addtail(&bp->vertList, av);

		e = bmesh_disk_faceedge_find_first(edges[0], v);
		do {
			e = bmesh_disk_edge_next(e, v);
			/* selected edge  */
			if (BMO_elem_flag_test(bm, e, EDGE_SELECTED)) {
				BMFace *f;
				BMIter iter;
				av->countSelect++;
				/* calc additional point,    calc support vertex on edge */
				BLI_addtail(&av->vertices, calc_support_vertex(bp, e, v));

				/* point located beteween selecion edges*/
				BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
					BMLoop *l = f->l_first;
					BMEdge *adjacentE = NULL;
					do {
						if ((l->e == e) && (find_selected_edge_in_face(bm, f, e, v) !=NULL )){
							if (!check_dublicated_vertex_item(av, f)) {
								VertexItem *item;
								item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
								item->onEdge = 0; // false
								item->edge1 = e;
								item->edge2 = find_selected_edge_in_face(bm, f, e, v);
								item->f = f;
								item->v = bevel_middle_vert(bm, bp,e, item->edge2, v);
								BLI_addtail(&av->vertices, item);
								av->count++;
							}
						}

						adjacentE = find_non_selected_adjacent_edge(bm, f, e, v);
						if ((l->e == e) && (adjacentE != NULL)){
							if (!check_dublicated_vertex_item_by_edge(av, adjacentE)){
								VertexItem *item;
								item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
								item->onEdge = 1; /* true */
								item->edge1 = adjacentE;
								item->edge2 = NULL;
								item->f = NULL;
								item->v = bevel_calc_aditional_vert(bm, bp, e, adjacentE, v);

								BLI_addtail(&av->vertices, item);
								av->count ++;
							}
						}

						l = l->next;
					} while (l != f->l_first);
				}


			}
			/* non seleced edge */
			/*if (!BMO_elem_flag_test(bm, e, EDGE_SELECTED)){
				VertexItem *item;
				item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
				item->onEdge = 1; /* true */
				/*item->edge1 = e;
				item->edge2 = NULL;
				item->f = NULL;
				item->v = bevel_calc_aditional_vert(bm, bp, e, v);
				BLI_addtail(&av->vertices, item);
				av->count ++;
			}*/
		} while (e != edges[0]);
	}
	BLI_array_free(edges);
}

AdditionalVert* get_additionalVert_by_vert(BevelParams *bp, BMVert *v)
{

	VertexItem *item;
	AdditionalVert *av = NULL;
	for (item = bp->vertList.first; item ; item = item->next){
		if (item->v == v)
			av = item;
	}
	return av;

}

/*
* return 1 if face content e
*/
int is_edge_of_face (BMFace *f, BMEdge* e)
{
	int result  = 0;
	BMLoop *l = f->l_first;
	do {
		if (l->e == e)
			result = 1;
		l = l->next;
	} while  (l != f->l_first);
	return result;
}

/*
* calculation of points on the round profile
*/
void get_point_on_round_profile(float r[3], float offset, int i, int count, float va[3], float v[3], float vb[3])
{
	float vva[3], vvb[3], angle, center[3], rv[3], axis[3], co[3];

	sub_v3_v3v3(vva, va, v);
	sub_v3_v3v3(vvb, vb, v);
	angle = angle_v3v3(vva, vvb);

	add_v3_v3v3(center, vva, vvb);
	normalize_v3(center);
	mul_v3_fl(center, offset * (1.0 / cos(0.5 * angle)));
	add_v3_v3(center, v);			/* coordinates of the center of the inscribed circle */


	sub_v3_v3v3(rv, va, center);	/* radius vector */


	sub_v3_v3v3(co, v, center);
	cross_v3_v3v3(axis, rv, co);	/* calculate axis */


	sub_v3_v3v3(vva, va, center);
	sub_v3_v3v3(vvb, vb, center);
	angle = angle_v3v3(vva, vvb);

	rotate_v3_v3v3fl(co, rv, axis, angle * (float)(i) / (float)(count));

	add_v3_v3(co, center);
	copy_v3_v3(r, co);
}

/*
* Search for crossing the line and plane
* p1, p2, p3 points which is given by the plane
* a - line vector
* m - point through which the line
*/

void find_intersection_point_plan(BMesh *bm, float r[3], float p1[3], float p2[3], float p3[3],
								  float a[3], float m[3])
{
	float P[3], N[3], A[3], M[3];
	float vv1[3], vv2[3];
	double t;
	double C, D, E;
	float null = 1e-20;

	/* calculation of the normal to the surface */
	sub_v3_v3v3(vv1, p1, p2);
	sub_v3_v3v3(vv2, p3, p2);
	cross_v3_v3v3(N, vv1, vv2);

	copy_v3_v3(A, a);
	copy_v3_v3(P, p2);
	copy_v3_v3(M, m);

	C = N[0] * P[0] + N[1] * P[1] + N[2] * P[2];
	D = N[0] * M[0] + N[1] * M[1] + N[2] * M[2];
	E = (A[0] * N[0] + A[1] * N[1] + A[2] * N[2]);

	if (fabs(E)< null)
		t = 0;
	else
		t = (C-D)/E;

	//t = (N[0] * P[0] + N[1] * P[1] + N[2] * P[2] - N[0] * m[0] - N[1] * m[1] - N[2] * m[2]) /
	//   (a[0] * N[0] + a[1] * N[1] + a[2] * N[2]);

	r[0] = m[0] + t * a [0];
	r[1] = m[1] + t * a [1];
	r[2] = m[2] + t * a [2];

}


BMVert* get_vert_on_round_profile(BMesh* bm,  BevelParams *bp, int i, int n,
								  float direction[3],
								  float v[3], float va[3], float vb[3])
{
	BMVert *result = NULL;
	float vva[3], vvb[3], ve[3], coa[3], cob[3], point[3], co[3];
	float ve1[3], ve2[3];
	// input data
	//BM_edge_create(bm, BM_vert_create(bm, v, NULL), BM_vert_create(bm, va, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, v, NULL), BM_vert_create(bm, vb, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, vb, NULL), BM_vert_create(bm, va, NULL), NULL, 0);

	sub_v3_v3v3(vva, va, v);
	sub_v3_v3v3(vvb, vb, v);

	normalize_v3_v3(coa, direction);
	mul_v3_fl(coa, len_v3(vva) * cos(angle_v3v3(vva, direction)));
	//mul_v3_fl(coa, dot_v3v3(vva, ve) /  len_v3(vva));
	add_v3_v3(coa, v);
	sub_v3_v3v3(vva, va, coa);
	add_v3_v3(vva, v);
		//BM_edge_create(bm, BM_vert_create(bm, v, NULL), BM_vert_create(bm, vva, NULL), NULL, 0);

	/* Search the orthogonal vector */
	normalize_v3_v3(cob, direction);
	mul_v3_fl(cob, len_v3(vvb) * cos(angle_v3v3(vvb, direction)));
	//mul_v3_fl(cob, dot_v3v3(vvb, ve) /  len_v3(vvb));
	add_v3_v3(cob, v);
	sub_v3_v3v3(vvb, vb, cob);
	add_v3_v3(vvb, v);
	//	BM_edge_create(bm, BM_vert_create(bm, v, NULL), BM_vert_create(bm, vvb, NULL), NULL, 0);

	/* search points in the profile */
	get_point_on_round_profile(point, bp->offset, i, n, vva, v, vvb);
	bevel_create_unic_vertex(bm, bp, point);

	//BM_edge_create(bm, BM_vert_create(bm, va, NULL), BM_vert_create(bm, v, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, vb, NULL), BM_vert_create(bm, v, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, vb, NULL), BM_vert_create(bm, va, NULL), NULL, 0);

	find_intersection_point_plan(bm, co, va, v ,vb , direction, point );

	result = bevel_create_unic_vertex(bm, bp, co);
	return result;
}

/*
 *   v
 *	/ \
 * v1.v2
 * only works for an isosceles triangle
 */
BMVert* get_vert_by_round_profile(BMesh* bm,  BevelParams *bp, int i,
								  int n, float v1[3],  float v[3],  float v2[3])
{
	BMVert* vert = NULL;
	float vect1[3], vect2[3], center[3], co[3], rv[3], axis[3]; // два вектора направленные к точке V
	float angle, r, c;

	// ------debug input data----------
	//BM_edge_create(bm, BM_vert_create(bm, v1, NULL), BM_vert_create(bm, v, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, v2, NULL), BM_vert_create(bm, v, NULL), NULL, 0);
	// --------------------------------

	sub_v3_v3v3(vect1, v1, v);
	sub_v3_v3v3(vect2, v2, v);
	angle = angle_v3v3(vect1, vect2);
	c = bp->offset * (1.0 / cos(0.5 * angle)); // локальный центр
	//r = c * sin(angle/2);

	add_v3_v3v3(center, vect1, vect2);
	// testVert = BM_vert_create(bm, center, NULL);
	normalize_v3(center);
	mul_v3_fl(center, c);

	add_v3_v3(center, v);			/* coordinates of the center of the inscribed circle */

	sub_v3_v3v3(rv, v1, center);	/* radius vector */

	sub_v3_v3v3(co, v, center);
	cross_v3_v3v3(axis, rv, co);	/* calculate axis */

	sub_v3_v3v3(vect1, v1, center);
	sub_v3_v3v3(vect2, v2, center);
	angle = angle_v3v3(vect1, vect2);

	//rotate_v3_v3v3fl(co, rv, axis, (M_PI - angle) * (float)(n) / (float)(bp->seg));
	//rotate_v3_v3v3fl(co, rv, axis, angle * (float)(n) / (float)(bp->seg));
	rotate_v3_v3v3fl(co, rv, axis, angle * (float)(i) / (float)(n));
	add_v3_v3(co, center);

	//BM_edge_create(bm, BM_vert_create(bm, co, NULL), BM_vert_create(bm, center, NULL), NULL, 0);

	vert = bevel_create_unic_vertex(bm, bp, co);
	return vert;
}



// search for points of intersection of two segments of a (a1, a2) and b (b1, b2) given two points
void find_intersection_point(float r[3], float a1[3], float a2[3], float b1[3], float b2[3])
{
	double s, t, z1, z2;
	double mx, my, nx, ny;

	mx = a2[0] - a1[0];
	my = a2[1] - a1[1];

	nx = b2[0] - b1[0];
	ny = b2[1] - b1[1];

	//s = (b1[1] / my - b1[0] / mx + a1[0]/ mx - a1[1] / my) / (nx / mx - ny / my);
	//t = b1[0] / mx - a1[0] / mx + s * nx / mx;
	s = ((b1[1] - a1[1]) / my + (a1[0] - b1[0])/ mx ) / (nx / mx - ny / my);
	t = (b1[0] - a1[0] + s * nx) / mx;


	z1 = a1[2] + t * (a2[2] -a1[2]);
	z2 = b1[2] + s * (b2[2] -b1[2]);

	/*r[0] = a1[0]  + t * mx;
	r[1] = a1[1]  + t * my;
	r[2] = z1;
	*/
	if ( fabs(z1-z2) < BEVEL_EPSILON ){
		r[0] = a1[0]  + t * mx;
		r[1] = a1[1]  + t * my;
		r[2] = z1;
	}
	else
		r = NULL;
}



BMVert* get_vert_by_round_profile_edge(BMesh* bm,  BevelParams *bp, BMEdge *e, AdditionalVert *av, int i,
								  int n, float v1[3],  float v[3],  float v2[3])
{
	BMVert *vert = NULL;
	// найти вектора ортогональные е
	// построить по ним профиль и найти пересечение с тремя точками
	float vv1[3], vv2[3], vve[3], angle, axis[3], co1[3], co2[3], co[3];
	sub_v3_v3v3(vv1, v1, av->v->co);
	sub_v3_v3v3(vv2, v2, av->v->co);
	sub_v3_v3v3(vve, av->v, BM_edge_other_vert(e, av->v));

	angle = angle_v3v3(vv1, vve);
	if (fabs(angle - M_PI_2) > BEVEL_EPSILON){
		cross_v3_v3v3(axis, vv1, vve);
		rotate_v3_v3v3fl(co1, vv1, axis, M_PI_2 - angle);
		normalize_v3(co1);
		mul_v3_fl(co1, bp->offset);

		add_v3_v3(co1, v);
	} else {
		copy_v3_v3(co1, v1);
	}

	angle = angle_v3v3(vv2, vve);
	if (fabs(angle - M_PI_2) > BEVEL_EPSILON){
		cross_v3_v3v3(axis, vv2, vve);
		rotate_v3_v3v3fl(co2, vv2, axis, M_PI_2 - angle);
		normalize_v3(co2);
		mul_v3_fl(co2, bp->offset);
		add_v3_v3(co2, v);
	} else {
		copy_v3_v3(co2, v2);
	}

	//BM_edge_create(bm, BM_vert_create(bm, co2, NULL), BM_vert_create(bm, co1, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, co1, NULL), BM_vert_create(bm, v, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, co2, NULL), BM_vert_create(bm, v, NULL), NULL, 0);


	return vert;
}

/*
* searches a point at the intersection of the two profiles
* i - iteration number
* Av contains only two selected Edge */

BMVert* get_vert_by_intersection(BMesh *bm, BevelParams *bp, int i, int n,
								 AdditionalVert *av, BMEdge *e,
								 BMVert *aVert, BMVert *bVert)

{
	BMVert *vert =  NULL;
	VertexItem *item, *itemAh = NULL, *itemBh = NULL;
	BMEdge *eItem =  NULL, *otherE = NULL;
	float co[3], vva[3], vvb[3], ve[3], veOther[3],
			middleCo[3], pointAco[3], pointBco[3];
	BMVert *aRVert = NULL, *bRVert = NULL;

	if (av->countSelect == 2){
		eItem = e = bmesh_disk_faceedge_find_first(e, av->v);
		do {
			eItem = bmesh_disk_edge_next(eItem, av->v);
			if (BMO_elem_flag_test(bm, eItem, EDGE_SELECTED)){
				if (eItem != e)
					otherE = eItem;
			}
		} while (eItem != e);
		if (otherE !=NULL){
			for (item = av->vertices.first; item; item = item->next){
				if (item->onEdge == 3) {
					if ((item->edge1 == e) || (item->edge2 == e))
						itemAh = item;
					if ((item->edge1 == otherE) || (item->edge2 == otherE))
						itemBh = item;
				}
			}
		}
		sub_v3_v3v3(vva, aVert->co, av->v->co);
		sub_v3_v3v3(vvb, bVert->co, av->v->co);

		sub_v3_v3v3(ve, BM_edge_other_vert(e, av->v)->co, av->v->co);
		sub_v3_v3v3(veOther, BM_edge_other_vert(otherE, av->v)->co, av->v->co);

		normalize_v3_v3(pointAco, ve);
		mul_v3_fl(pointAco, bp->offset);

		normalize_v3_v3(pointBco, veOther);
		mul_v3_fl(pointBco, bp->offset);
		if (fabs(len_v3(vva) - bp->offset) < BEVEL_EPSILON){
			copy_v3_v3(middleCo, bVert->co);

			add_v3_v3(pointAco, aVert->co);
			add_v3_v3(pointBco, aVert->co);
		}

		if (fabs(len_v3(vvb) - bp->offset) < BEVEL_EPSILON){
			copy_v3_v3(middleCo, aVert->co);

			add_v3_v3(pointAco, bVert->co);
			add_v3_v3(pointBco, bVert->co);
		}

		aRVert = get_vert_by_round_profile(bm, bp, i, n, middleCo, itemAh->hv, pointAco);
		bRVert = get_vert_by_round_profile(bm, bp, i, n, middleCo, itemBh->hv, pointBco);

		sub_v3_v3v3(ve, av->v->co, BM_edge_other_vert(e, av->v)->co);
		sub_v3_v3v3(veOther, av->v->co, BM_edge_other_vert(otherE, av->v)->co);

		normalize_v3_v3(pointAco, ve);
		mul_v3_fl(pointAco, bp->offset * 10.0);
		add_v3_v3(pointAco, aRVert->co );

		normalize_v3_v3(pointBco, veOther);
		mul_v3_fl(pointBco, bp->offset * 10.0);
		add_v3_v3(pointBco, bRVert->co);

	//	BM_edge_create(bm, BM_vert_create(bm, aRVert->co, NULL), BM_vert_create(bm, pointAco, NULL), NULL, 0);
	//	BM_edge_create(bm, BM_vert_create(bm, bRVert->co, NULL), BM_vert_create(bm, pointBco, NULL), NULL, 0);
	//	BM_edge_create(bm, BM_vert_create(bm, pointAco, NULL), BM_vert_create(bm, pointBco, NULL), NULL, 0);

		BM_edge_create(bm, aVert, av->v, NULL, 0);
		BM_edge_create(bm, bVert, av->v, NULL, 0);
		find_intersection_point_plan(bm, co, aVert->co, av->v->co, bVert->co, ve, aRVert->co);

		if (co != NULL)
			vert = BM_vert_create(bm, co, NULL);
	}
	return vert;
}

BMVert* get_vert_by_seg(BMesh* bm,  BevelParams *bp, int i,
						int n, BMVert *bmv, VertexItem *viA, VertexItem *viB,
						float v1[3],  float v[3],  float v2[3])
{
	BMVert* vert= NULL;
	float vect1[3], vect2[3], center[3], co[3], rv[3], axis[3];
	float angle, c;
	float hva[3], hvb[3]; /* support vectors which rotate between the radius vector */
	float hAngle;
	float aLen, bLen, hrv[3], crv[3];
	float tmpA[3], tmpB[3];
	float t;

	int j;
	float testAxis[3], testV[3], t1[2], t2[3];
	BMVert *bv1, *bv2;

	sub_v3_v3v3(vect1, v1, v);
	sub_v3_v3v3(vect2, v2, v);

	angle = angle_v3v3(vect1, vect2);

	c = bp->offset * (1 / cos(angle/2));

	add_v3_v3v3(center, vect1, vect2);
	normalize_v3(center);
	mul_v3_fl(center, c);
	add_v3_v3(center, v);			/* coordinates of the center of the inscribed circle */

	/*  local center, together with the starting points of curvature */
	//BM_edge_create(bm, BM_vert_create(bm, v, NULL), BM_vert_create(bm, center, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, v1, NULL), BM_vert_create(bm, center, NULL), NULL, 0);
	//BM_edge_create(bm, BM_vert_create(bm, v2, NULL), BM_vert_create(bm, center, NULL), NULL, 0);


	sub_v3_v3v3(rv, v1, center);	/* radius vector */
	add_v3_v3v3(co, rv, center);
	//BM_edge_create(bm, BM_vert_create(bm, co, NULL), BM_vert_create(bm, center, NULL), NULL, 0);

	sub_v3_v3v3(co, v, center);
	cross_v3_v3v3(axis, rv, co);    /* axis */


	/* center the radius vector */
	rotate_v3_v3v3fl(crv, rv, axis, 0.5 * (M_PI - angle) );
	add_v3_v3v3(co, crv, center);
	//BM_edge_create(bm, BM_vert_create(bm, co, NULL), BM_vert_create(bm, center, NULL), NULL, 0);

	sub_v3_v3v3(hva, bmv->co, viA->v->co );
	sub_v3_v3v3(hvb, bmv->co, viB->v->co );

	sub_v3_v3v3(testV, bmv->co, viA->v->co );
	cross_v3_v3v3(testAxis, testV, rv);
	hAngle = angle_v3v3(testV, rv);
	/*for (j = 0; j < bp->seg; j++){
		rotate_v3_v3v3fl(co, rv, testAxis, hAngle * (float)(j)/ (float)(bp->seg));
		add_v3_v3(co, center);
		bv1 = BM_vert_create(bm, co, NULL);
		rotate_v3_v3v3fl(co, rv, testAxis, hAngle * (float)(j+1)/ (float)(bp->seg));
		add_v3_v3(co, center);
		bv2 = BM_vert_create(bm, co, NULL);
		//BM_edge_create(bm, bv1, bv2, NULL, 0);
	}*/

	aLen = (float) (i) * len_v3(hva) / (float)(bp->seg);

	normalize_v3(hva);
	mul_v3_fl(hva, aLen);
	add_v3_v3(hva, viA->v->co);
	//BM_edge_create(bm, BM_vert_create(bm, hva, NULL), BM_vert_create(bm, center, NULL), NULL, 0);

	bLen = (float) (i) * len_v3(hvb) / (float)(bp->seg);
	normalize_v3(hvb);
	mul_v3_fl(hvb, bLen);

	add_v3_v3(hvb, viB->v->co);
	//BM_edge_create(bm, BM_vert_create(bm, hvb, NULL), BM_vert_create(bm, center, NULL), NULL, 0);

	sub_v3_v3v3(tmpA, hva, center);
	sub_v3_v3v3(tmpB, hvb, center);

	hAngle = angle_v3v3(tmpA, tmpB);


	rotate_v3_v3v3fl(hrv, crv, axis, -0.5 * hAngle);
	//add_v3_v3(hrv, center);
	//BM_edge_create(bm, BM_vert_create(bm, hrv, NULL), BM_vert_create(bm, center, NULL), NULL, 0);


	t = (float) (n) / (float) (bp->seg);
	rotate_v3_v3v3fl(co, hrv, axis, hAngle * t);
	add_v3_v3(co, center);

	vert = bevel_create_unic_vertex(bm, bp, co);

	//BM_edge_create(bm, vert, BM_vert_create(bm, center, NULL), NULL, 0);

	return vert;
}


BMVert* get_vert_by_round_profile_rotate(BMesh* bm,  BevelParams *bp,
										 int n, BMVert *v1,  BMVert *v,  BMVert* v2,
										 BMEdge *e)
{
	float vv1[3], vv2[3], ve[3], vc1[3], vc2[3], vc[3];
	float angle_vv1_e, angle_vv2_e, r;
	BMVert *vert;
	BMVert *tv1, *tv2, *tv3;
	float limit = 1e-6;

	/* calculate the vector */
	sub_v3_v3v3(vv1, v1->co, v->co);
	r = len_v3(vv1);
	sub_v3_v3v3(vv2, v2->co, v->co);
	sub_v3_v3v3(ve, BM_edge_other_vert(e, v)->co, v->co);
	/* calculate the angles between them */
	angle_vv1_e = angle_v3v3(vv1, ve);
	angle_vv2_e = angle_v3v3(vv2, ve);

	normalize_v3_v3(vc, ve);
	mul_v3_fl(vc, bp->offset);
	add_v3_v3(vc, v->co);

	if (fabs(len_v3(vv1) - bp->offset) < limit)
		add_v3_v3v3(vc1, vv1, vc);
	else
		copy_v3_v3(vc1, v1->co);
	if (fabs(len_v3(vv2) - bp->offset) < limit)
		add_v3_v3v3(vc2, vv2, vc);
	else
		copy_v3_v3(vc2, v2->co);

	tv1 = BM_vert_create(bm, vc1, NULL);
	tv2 = BM_vert_create(bm, vc2, NULL);
	tv3 = BM_vert_create(bm, vc, NULL);
	//BM_edge_create(bm, tv1, tv2, NULL, 0);
	//BM_edge_create(bm, tv1, tv3, NULL, 0);
	//BM_edge_create(bm, tv2, tv3, NULL, 0);

	vert = get_vert_by_round_profile(bm, bp, n,bp->seg, vc1, vc, vc2);


	sub_v3_v3v3(ve, v->co, BM_edge_other_vert(e, v)->co );
	normalize_v3(ve);
	//mul_v3_fl(ve, bp->offset * cos(angle_vv1_e) * n / bp->seg);
	//mul_v3_fl(ve, bp->offset * cos(angle_vv1_e));
	r = sqrt(1 - (float)((bp->seg - n) * (bp->seg - n)) / (float)(bp->seg * bp->seg));
	mul_v3_fl(ve, bp->offset * r);

	//mul_v3_fl(ve, bp->offset );
	//sub_v3_v3v3(vc, vert->co, ve);
	add_v3_v3v3(vc, vert->co, ve);
	BM_vert_kill(bm, vert);
	vert = BM_vert_create(bm, vc, NULL);

	return vert;
}

BMEdge* find_first_edge(BMesh* bm, BMOperator *op,  BMVert *v)
{
	BMOIter siter;
	BMEdge *e, *r = NULL;
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		if ((e->v1 == v) || (BM_edge_other_vert(e, e->v1) == v))
			r = e;
	}
	return r;
}

/* looking points around the Edge */
VertexItem* find_middle_vertex_item(AdditionalVert *av, BMEdge* e, VertexItem* point)
{
	VertexItem *item, *r = NULL;

	for (item = av->vertices.first; item; item = item->next){
		if ((item->onEdge == 0) &&
			((item->edge1 == e) || (item->edge2 == e))){
			if (point == NULL)
				r = item;
			else if (point != item)
				r = item;
		}
	}
	return r;
}

VertexItem* find_helper_vertex_item(AdditionalVert *av, BMEdge *e)
{
	VertexItem *item, *r = NULL;
	for (item = av->vertices.first; item; item = item->next){
		if ((item->onEdge == 3) && (item->edge1 == e))
			r = item;
	}
	return r;
}

BMEdge* get_other_edge(VertexItem* item, BMEdge *e ){
	BMEdge *r = NULL;
	if (item->edge1 != e)
		r = item->edge1;
	if (item->edge2 != e)
		r = item->edge2;
	return r;
}

void calc_new_helper_point (BMesh *bm, BevelParams* bp,
							AdditionalVert *av, int n, float a[3], float h[3], float b[3],  float r[3])
{

	float vect[3], va[3], vb[3], rv[3], center[3], axis[3], result[3];
	float c, angle;

	sub_v3_v3v3(va, a, h);
	sub_v3_v3v3(vb, b, h);
	angle  = angle_v3v3(va, vb);

	add_v3_v3v3(center, va, vb);
	normalize_v3(center);
	mul_v3_fl(center, (float)(bp->offset) * (1 / cos(angle / 2)));
	add_v3_v3(center, h); /* local center*/

	sub_v3_v3v3(rv, h, center);/* radius vector */

	//add_v3_v3(rv, center);
	//BM_edge_create(bm, BM_vert_create(bm, center, NULL), BM_vert_create(bm, rv, NULL), NULL, 0);

	sub_v3_v3v3(vect, av->v->co, center);
	//add_v3_v3(vect, center);
	//BM_edge_create(bm, BM_vert_create(bm, center, NULL), BM_vert_create(bm, vect, NULL), NULL, 0);

	cross_v3_v3v3(axis, rv, vect);  /* axis */

	//add_v3_v3(axis, center);


	//BM_edge_create(bm, BM_vert_create(bm, center, NULL), BM_vert_create(bm, axis, NULL), NULL, 0);

	/*sub_v3_v3v3(va, center, h);
	sub_v3_v3v3(vb, av->v->co, h);
	angle  = angle_v3v3(va, vb);
*/
	rotate_v3_v3v3fl(result, rv, axis, 2 * angle * (float) (n) / (float) (bp->seg));
	//rotate_v3_v3v3fl(result, rv, axis, (M_PI - angle) * (float) (n) / (float) (bp->seg));
	add_v3_v3(result, center);
	//BM_edge_create(bm, BM_vert_create(bm, center, NULL), BM_vert_create(bm, result, NULL), NULL, 0);


	sub_v3_v3v3(vect, av->v->co, h);
	normalize_v3(vect);
	//mul_v3_fl(vect, bp->offset * (float)(n) / (float) (bp->seg));
	mul_v3_fl(vect, bp->offset * sin ((M_PI - angle) * (float) (n) / (float) (bp->seg)));
	add_v3_v3(vect, h);

	copy_v3_v3(r, vect);

	add_v3_v3(rv, center);

	//BM_edge_create(bm, BM_vert_create(bm, center, NULL), BM_vert_create(bm, rv, NULL), NULL, 0);
}


void find_boundary_point_1(BMesh *bm, BevelParams* bp, AdditionalVert *av, BMEdge *e, VertexItem *item, float r[3])
{
	VertexItem *pointA = NULL, *pointB = NULL, *pointH = NULL;
	BMEdge *ea, *eb;
	VertexItem *pointAn = NULL, *pointBn = NULL, *pointHan = NULL, *pointHbn = NULL;

	BMVert *vai, *vbi, *vaj, *vbj;
	float hcoi[3], hcoj[3];

	pointA = find_middle_vertex_item(av, e, NULL);
	pointB = find_middle_vertex_item(av, e, pointA);
	pointH = find_helper_vertex_item(av, e);

	if ((pointA != NULL) && (pointB != NULL)){
		BMVert *bVa, *bVb;
		ea = get_other_edge(pointA, e);
		eb = get_other_edge(pointB, e);

		pointAn = find_middle_vertex_item(av, ea, pointA);
		pointBn = find_middle_vertex_item(av, eb, pointB);
		pointHan = find_helper_vertex_item(av, ea);
		pointHbn = find_helper_vertex_item(av, eb);

		vaj = get_vert_by_round_profile(bm, bp, 1, bp->seg, pointA->v->co, pointHan->hv, pointAn->v->co);
		vbj = get_vert_by_round_profile(bm, bp, 1, bp->seg, pointB->v->co, pointHbn->hv, pointBn->v->co);
		calc_new_helper_point(bm, bp, av, 1, pointA->v->co, pointH->hv, pointB->v->co, hcoj);

		if (item == pointA){
			bVa = get_vert_by_round_profile(bm, bp, 1, bp->seg, vaj->co, hcoj, vbj->co);
			copy_v3_v3(r, bVa->co);
		} else {
		bVb = get_vert_by_round_profile(bm, bp, bp->seg - 1,bp->seg, vaj->co, hcoj, vbj->co);
		copy_v3_v3(r, bVb->co);
		}

	}

}

/*
 * coordinates of the midpoint
 */
void find_middle_vect(float r[3], float a[3], float b[3])
{
	float vect[3], l;
	sub_v3_v3v3(vect, a, b);
	l = len_v3(vect) * 0.5;
	normalize_v3(vect);
	mul_v3_fl(vect, l);
	add_v3_v3v3(r, vect, b);
}

/*
* building a polygons on the structure of the sector
* returns with a new segment of the circuit but without the boundary points
*/

SurfaceEdgeData* bevel_build_segment(BMesh *bm, BevelParams* bp, int iter,
									 SurfaceEdgeData* sd, AdditionalVert *av)
{
	float h[3];
	NewVertexItem *vItem;
	BMVert *listV[4];
	BMVert *v1, *v2, *v3, *v4;
	int i = 1;
	float dir[3];
	SurfaceEdgeData *data = NULL; // return data

	sub_v3_v3v3(dir, BM_edge_other_vert(sd->e, av->v)->co, av->v->co);

	data = (SurfaceEdgeData*) calloc (sizeof(SurfaceEdgeData), 1);
	data->e = sd->e;
	data->vertexList.first = data->vertexList.last = NULL;
	data->count = 0;

	calc_new_helper_point(bm, bp, av, iter, sd->boundaryA->co, sd->h, sd->boundaryB->co, h);
	vItem = sd->vertexList.first;

	v2 = vItem->v;
	//v4 = get_vert_by_round_profile(bm, bp, 0, sd->count-1, sd->boundaryA->co, h, sd->boundaryB->co);
	v4 = get_vert_on_round_profile(bm, bp, 0, sd->count-1, dir, h, sd->boundaryA->co, sd->boundaryB->co);
	do{
		vItem = vItem->next;
		if (vItem){
			v1 = vItem->v;
			//v3 = get_vert_by_round_profile(bm, bp, i,sd->count-1, sd->boundaryA->co, h, sd->boundaryB->co);
			v3 = get_vert_on_round_profile(bm, bp, i,sd->count-1, dir, h, sd->boundaryA->co, sd->boundaryB->co);
			if (vItem->next != NULL){
				NewVertexItem *item;
				item = (NewVertexItem*)MEM_callocN(sizeof(NewVertexItem), "VertexItem");
				item->v = v3;
				BLI_addtail(&data->vertexList, item);
				data->count ++;
			}

			listV[0] = v1;
			listV[1] = v2;
			listV[2] = v3;
			listV[3] = v4;
			BM_face_create_ngon_vcloud(bm, listV, 4, 1);

			v2 = v1;
			v4 = v3;
			i++;
		}
	}while (vItem);

	return data;
}


SurfaceEdgeData* init_start_segment_data(BMesh *bm, BevelParams *bp,  AdditionalVert *av, BMEdge *e)
{
	SurfaceEdgeData	*sd = NULL;
	VertexItem *pointA = NULL, *pointB = NULL, *pointH = NULL;
	int j;
	float dir[3];
	pointA = find_middle_vertex_item(av, e, NULL);
	pointB = find_middle_vertex_item(av, e, pointA);
	pointH = find_helper_vertex_item(av, e);
	sub_v3_v3v3(dir, BM_edge_other_vert(e, av->v)->co, av->v->co);


	if ((pointA != NULL) && (pointB != NULL) && (pointH != NULL)) {
		sd = (SurfaceEdgeData*) calloc (sizeof(SurfaceEdgeData), 1);
		sd->e = e;
		sd->vertexList.first = sd->vertexList.last = NULL;
		sd->count = 0;
		copy_v3_v3(sd->h, pointH->hv);
		for (j = 1; j < bp->seg; j++) {
			NewVertexItem *item;
			item = (NewVertexItem*)MEM_callocN(sizeof(NewVertexItem), "VertexItem");
			//item->v = get_vert_by_round_profile(bm, bp, j, bp->seg, pointA->v->co, pointH->hv, pointB->v->co);

			item->v = get_vert_on_round_profile(bm, bp, j, bp->seg, dir, pointH->hv, pointA->v->co, pointB->v->co);

			BLI_addtail(&sd->vertexList, item);
			sd->count ++;
		}
		//sd->a = get_vert_by_round_profile(bm, bp, 0, bp->seg, pointA->v->co, pointH->hv, pointB->v->co);
		//sd->b = get_vert_by_round_profile(bm, bp, bp->seg, bp->seg, pointA->v->co, pointH->hv, pointB->v->co);
		sd->a = get_vert_on_round_profile(bm, bp, 0, bp->seg, dir,  pointH->hv, pointA->v->co, pointB->v->co);
		sd->b = get_vert_on_round_profile(bm, bp, bp->seg, bp->seg, dir, pointH->hv, pointA->v->co, pointB->v->co);
	}
	else {
		sd = NULL;
	}
	return sd;
}

SurfaceEdgeData* getSurfaceEdgeItem(SurfaceEdgeData** listData, int count, BMEdge *e){
	SurfaceEdgeData *sd = NULL;
	int i;

	for (i = 0; i< count; i++) {
		if (listData[i]->e == e)
			sd = listData[i];
	}
	return sd;
}

void updatSurfaceEdgeItems(SurfaceEdgeData** listData, SurfaceEdgeData** newListData, int count)
{
	int i;
	for (i = 0; i< count; i++) {
		listData[i]->count = newListData[i]->count;
		listData[i]->vertexList.first = newListData[i]->vertexList.first;
		listData[i]->vertexList.last = newListData[i]->vertexList.last;
		listData[i]->a = listData[i]->boundaryA;
		listData[i]->b = listData[i]->boundaryB;
	}
}

void find_boundary_point(BMesh *bm, BevelParams *bp, AdditionalVert *av,
							SurfaceEdgeData *sd, SurfaceEdgeData **listData, int count, int i,
							  float bva[3], float bvb[3])
{
	VertexItem *pointA = NULL, *pointB = NULL;
	BMEdge *ea, *eb;
	SurfaceEdgeData *sdA, *sdB;
	float h[3], ha[3], hb[3];
	float dir[3];

	pointA = find_middle_vertex_item(av, sd->e, NULL);
	pointB = find_middle_vertex_item(av, sd->e, pointA);
	sub_v3_v3v3(dir, BM_edge_other_vert(sd->e, av->v)->co, av->v->co);

	if ((pointA != NULL) && (pointB != NULL)){
		ea = get_other_edge(pointA, sd->e);
		eb = get_other_edge(pointB, sd->e);

		sdA = getSurfaceEdgeItem(listData, count, ea);
		sdB = getSurfaceEdgeItem(listData, count, eb);
		if ((sdA != NULL) && (sdB != NULL)){
			NewVertexItem *itemA, *itemB;
			BMVert *a, *b;

			if (len_v3v3(((NewVertexItem*)(sd->vertexList.first))->v->co, ((NewVertexItem*)(sdA->vertexList.first))->v->co) <
				len_v3v3(((NewVertexItem*)(sd->vertexList.first))->v->co, ((NewVertexItem*)(sdA->vertexList.last))->v->co))
				itemA = sdA->vertexList.first;
			else
				itemA = sdA->vertexList.last;

			if (len_v3v3(((NewVertexItem*)(sd->vertexList.first))->v->co, ((NewVertexItem*)(sdB->vertexList.first))->v->co) <
				len_v3v3(((NewVertexItem*)(sd->vertexList.first))->v->co, ((NewVertexItem*)(sdB->vertexList.last))->v->co))
				itemB = sdB->vertexList.first;
			else
				itemB = sdB->vertexList.last;

			calc_new_helper_point(bm, bp, av, 1, itemA->v->co, sd->h, itemB, h);

			//a = get_vert_by_round_profile(bm, bp, 1, sd->count, itemA->v->co, h, itemB->v->co);
			//b = get_vert_by_round_profile(bm, bp, sd->count - 1, sd->count, itemA->v->co, h, itemB->v->co);
			a = get_vert_on_round_profile(bm, bp, 1, sd->count, dir, h, itemA->v->co,  itemB->v->co);
			b = get_vert_on_round_profile(bm, bp, sd->count - 1, sd->count, dir, h, itemA->v->co, itemB->v->co);

			copy_v3_v3(bva, a->co);
			copy_v3_v3(bvb, b->co);
		}
	}
}

BMVert* get_nearest_vert(SurfaceEdgeData *sd, BMVert *v)
{
	BMVert *result = NULL;
	NewVertexItem *item;
	if (sd->vertexList.first && sd->vertexList.last){
		if (len_v3v3(((NewVertexItem*)(sd->vertexList.first))->v->co, v->co) <
			len_v3v3(((NewVertexItem*)(sd->vertexList.last))->v->co, v->co))
			result = ((NewVertexItem*)(sd->vertexList.first))->v;
		else
			result = ((NewVertexItem*) (sd->vertexList.last))->v;
	}
	return result;
}

void calculate_boundary_point(BMesh *bm, BevelParams *bp,
							  AdditionalVert *av, SurfaceEdgeData *sd,
							  SurfaceEdgeData **listData, int count, int i)
{
	VertexItem *pointA = NULL, *pointB = NULL;
	BMEdge *ea, *eb;
	SurfaceEdgeData *sdA, *sdB;
	BMVert **verts[4];
	NewVertexItem *vertItem, *tVertItem;

	pointA = find_middle_vertex_item(av, sd->e, NULL);
	pointB = find_middle_vertex_item(av, sd->e, pointA);
	if ((pointA != NULL) && (pointB != NULL)){
		float a[3], b[3], aa1[3], aa2[3],  bb1[3], bb2[3], co[3];
		ea = get_other_edge(pointA, sd->e);
		eb = get_other_edge(pointB, sd->e);

		sdA = getSurfaceEdgeItem(listData, count, ea);
		sdB = getSurfaceEdgeItem(listData, count, eb);

		find_boundary_point (bm, bp, av, sd, listData, count, i, a, b);
		find_boundary_point (bm, bp, av, sdA, listData, count, i, aa1, aa2);
		find_boundary_point (bm, bp, av, sdB, listData, count, i, bb1, bb2);

		if (len_v3v3(a, aa1) < len_v3v3(a, aa2))
			find_middle_vect(co, a, aa1);
		else
			find_middle_vect(co, a, aa2);

		sd->boundaryA =  bevel_create_unic_vertex(bm, bp, co);

		if (len_v3v3(b, bb1) < len_v3v3(b, bb2))
			find_middle_vect(co, b, bb1);
		else
			find_middle_vect(co, b, bb2);

		sd->boundaryB =  bevel_create_unic_vertex(bm, bp, co);

		if (get_nearest_vert(sdA, sd->a)){
			verts[0] = sd->boundaryA;
			verts[1] = sd->a;
			verts[2] = ((NewVertexItem*)(sd->vertexList.first))->v;
			verts[3] = get_nearest_vert(sdA, sd->a);
			BM_face_create_ngon_vcloud(bm, verts, 4, 1);
		}

		if (get_nearest_vert(sdB, sd->b)){
			verts[0] = sd->boundaryB;
			verts[1] = sd->b;
			verts[2] = ((NewVertexItem*)(sd->vertexList.last))->v;
			verts[3] = get_nearest_vert(sdB, sd->b);
			BM_face_create_ngon_vcloud(bm, verts, 4, 1);
		}

	}
}

int check_dublicated_vertex(BMVert **vv, int count, BMVert *v)
{
	int result =0, i;
	for (i = 0; i< count; i++){
		if (vv[i] == v)
			result = 1;
	}
	return result;
}

void bevel_build_ring(BMesh *bm, BMOperator *op, BevelParams* bp, AdditionalVert *av)
{
	BMEdge *e, *firstE;
	int i = 0, j;
	SurfaceEdgeData **segmentList = NULL, **newSegmentsList = NULL;
	BMVert **nGonList = NULL;
	BLI_array_declare(segmentList);
	BLI_array_declare(newSegmentsList);
	BLI_array_declare(nGonList);

	// ------------------------------------------------
	for (i = 0; i <= bp->seg / 2; i++){
		firstE = find_first_edge(bm, op, av->v);
		e = firstE;
		do {
			if (i == 0){
				SurfaceEdgeData *sd = NULL;
				sd = init_start_segment_data(bm, bp, av, e);
				if (sd != NULL)
					BLI_array_append(segmentList, sd);
			}
			else {
				SurfaceEdgeData *sd =  NULL, *newSd;
				sd = getSurfaceEdgeItem(segmentList, BLI_array_count(segmentList), e);

				if (sd != NULL){
					calculate_boundary_point(bm, bp, av, sd, segmentList, BLI_array_count(segmentList), i);
					newSd = bevel_build_segment(bm, bp, i, sd, av);
					BLI_array_append(newSegmentsList, newSd);
				//if (newSd != NULL)
				//	updatSurfaceEdgeItem(segmentList,BLI_array_count(segmentList), e, newSd);
				//break;
				}
			}
			e = bmesh_disk_edge_next(e, av->v);


		} while (e != firstE);
		if (i != 0){
			updatSurfaceEdgeItems(segmentList, newSegmentsList, BLI_array_count(segmentList));
			//BLI_array_free(newSegmentsList);
			BLI_array_empty(newSegmentsList);
		}
	}

	if (bp->seg % 2 != 0){
		int k;
		for(i = 0; i < BLI_array_count(segmentList); i++){
			if (!check_dublicated_vertex(nGonList,BLI_array_count(nGonList), segmentList[i]->a))
				BLI_array_append(nGonList, segmentList[i]->a);
			if (!check_dublicated_vertex(nGonList,BLI_array_count(nGonList), segmentList[i]->b))
				BLI_array_append(nGonList, segmentList[i]->b);
		}
		k = BLI_array_count(nGonList);
		if (BLI_array_count(nGonList) > 2)
			BM_face_create_ngon_vcloud(bm, nGonList, BLI_array_count(nGonList), 1);
	}


	// ------------------------------------------------
	firstE = find_first_edge(bm, op, av->v);
	e = firstE;
}

void bevel_build_polygons_arround_vertex(BMesh *bm, BevelParams *bp, BMOperator *op, BMVert *v)
{
	AdditionalVert *av = get_additionalVert_by_vert(bp, v);
	VertexItem *vi, *viCur, *viPrev;
	int i;
	float centroid[3];
	BMVert **vv = NULL;
    BLI_array_declare(vv);

	if ((av->count > 2) && (bp->seg == 1 )) {

		for (vi = av->vertices.first; vi; vi = vi->next){
			if (vi->onEdge != 3)
				BLI_array_append(vv, vi->v);
		}
        /* TODO check normal */
        if (BLI_array_count(vv) > 0)
            BM_face_create_ngon_vcloud(bm, vv, BLI_array_count(vv), 0);
    }


	/*if ((av->count > 2) && (bp->seg > 1)){
		BMEdge *e, *firstE;
		VertexItem *pointA = NULL, *pointB = NULL, *pointH = NULL;
		BMVert *vai;
		int count = 0, i;

		centroid[0] = 0;
		centroid[1] = 0;
		centroid[2] = 0;
//		copy_v3_v3(centroid, av->v);

		/*firstE = find_first_edge(bm, op, av->v);
		e = firstE;
		do {
			e = bmesh_disk_edge_next(e, av->v);
			 if (BMO_elem_flag_test(bm, e, EDGE_SELECTED)){
				pointA = find_middle_vertex_item(av, e, NULL);
				pointB = find_middle_vertex_item(av, e, pointA);
				pointH = find_helper_vertex_item(av, e);
				 // в цикле проходим по профилю
				if ((pointA != NULL) && (pointB != NULL) && (pointH != NULL))
				for (i = 0; i < bp->seg; i++){
					vai = get_vert_by_round_profile(bm, bp, i, pointA->v->co, pointH->hv, pointB->v->co);
					add_v3_v3(centroid, vai->co);
					count++;
				}
			 }
		} while (e != firstE);
*/
	/*	for (vi = av->vertices.first; vi; vi = vi->next){
			if (vi->onEdge != 3){
				add_v3_v3(centroid, vi->v->co);
				count++;
			}
		}

		mul_v3_fl(centroid, 1.0f/count);

		BM_edge_create(bm, BM_vert_create(bm, centroid, NULL), v, NULL, 0);
	}*/

	if ((av->count > 2) && (bp->seg > 1)){
		BMEdge *e, *firstE;
		bevel_build_ring(bm, op,  bp, av);
		firstE = find_first_edge(bm, op, av->v);
		e = firstE;
		//do {
			e = bmesh_disk_edge_next(e, v);
			 if (BMO_elem_flag_test(bm, e, EDGE_SELECTED)){

				VertexItem *pointA = NULL, *pointB = NULL, *pointH = NULL;
				VertexItem *pointAn = NULL, *pointBn = NULL, *pointHan = NULL, *pointHbn = NULL;

				VertexItem *eaPoint = NULL, *ebPoin = NULL;

				BMEdge *ea, *eb;
				BMEdge *eaa, *ebb;
				VertexItem *pointAAn = NULL, *pointBBn = NULL, *pointHaan = NULL, *pointHbbn = NULL;

				BMVert *va, *vb;
				BMVert *prev =  NULL, *cur = NULL;
				float hi[3];
				int j, n;
				pointA = find_middle_vertex_item(av, e, NULL);
				pointB = find_middle_vertex_item(av, e, pointA);
				pointH = find_helper_vertex_item(av, e);
				if ((pointA != NULL) && (pointB != NULL)){
					ea = get_other_edge(pointA, e);
					eb = get_other_edge(pointB, e);

					pointAn = find_middle_vertex_item(av, ea, pointA);
					pointBn = find_middle_vertex_item(av, eb, pointB);
					pointHan = find_helper_vertex_item(av, ea);
					pointHbn = find_helper_vertex_item(av, eb);
					eaa = get_other_edge(pointAn, ea);
					ebb = get_other_edge(pointBn, eb);

					pointAAn = find_middle_vertex_item(av, eaa, pointAn);
					pointBBn = find_middle_vertex_item(av, ebb, pointBn);

					pointHaan = find_helper_vertex_item(av, eaa);
					pointHbbn = find_helper_vertex_item(av, ebb);

					n = bp->seg/2;
					for (i = 0; i < n; i++){
						BMVert *vai, *vbi, *vaj, *vbj;
						BMVert *curi, *previ, *curj, *prevj;
						float hcoi[3], hcoj[3];
						BMVert *listV[4];

						BMVert *bVa, *bVb;
						BMVert *bVaa, *bVbb, *vaaj, *vbbj;

/*
						vai = get_vert_by_round_profile(bm, bp, i, pointA->v->co, pointHan->hv, pointAn->v->co);
						vbi = get_vert_by_round_profile(bm, bp, i, pointB->v->co, pointHbn->hv, pointBn->v->co);
						calc_new_helper_point(bm, bp, av, i, pointA->v->co, pointH->hv, pointB->v->co, hcoi);
						// debug
						//BM_edge_create(bm, BM_vert_create(bm, hcoi, NULL), BM_vert_create(bm, vai->co, NULL), NULL, 0);
						//BM_edge_create(bm, BM_vert_create(bm, hcoi, NULL), BM_vert_create(bm, vbi->co, NULL), NULL, 0);

						vaj = get_vert_by_round_profile(bm, bp, i + 1, pointA->v->co, pointHan->hv, pointAn->v->co);
						vbj = get_vert_by_round_profile(bm, bp, i + 1, pointB->v->co, pointHbn->hv, pointBn->v->co);
						calc_new_helper_point(bm, bp, av, i + 1, pointA->v->co, pointH->hv, pointB->v->co, hcoj);
						// debug
						//BM_edge_create(bm, BM_vert_create(bm, hcoj, NULL), BM_vert_create(bm, vaj->co, NULL), NULL, 0);
						//BM_edge_create(bm, BM_vert_create(bm, hcoj, NULL), BM_vert_create(bm, vbj->co, NULL), NULL, 0);

						previ = vai;
						prevj = vaj;

						//bVa = get_vert_by_round_profile(bm, bp, 1, vaj->co, hcoj, vbj->co);
						//bVb = get_vert_by_round_profile(bm, bp, bp->seg - 1, vaj->co, hcoj, vbj->co);
						//BM_edge_create(bm, bVb, bVa, NULL, 0);

						//vaaj = get_vert_by_round_profile(bm, bp, i + 1, pointAn->v->co, pointHaan->hv, pointAAn->v->co);
						//vbbj = get_vert_by_round_profile(bm, bp, bp->seg - 1  , pointA->v->co, pointH->hv, pointB->v->co);
						//BM_edge_create(bm, pointAAn->v, vbbj, NULL, 0);



						for (j = 0; j < bp->seg; j++){

							//curi = get_vert_by_seg(bm, bp, i, j, v, pointA, pointB, vai->co, hcoi, vbi->co);

							//curj = get_vert_by_seg(bm, bp, i, j, v, pointA, pointB, vaj->co, hcoj, vbj->co);

						//	curi  =  get_vert_by_round_profile(bm, bp, j, vai->co, hcoi, vbi->co);
						//	curj  =  get_vert_by_round_profile(bm, bp, j, vaj->co, hcoj, vbj->co);


							listV[0] = curi;
							listV[1] = curj;
							listV[2] = previ;
							listV[3] = prevj;

							//BM_face_create_ngon_vcloud(bm, listV, 4, 1);

							previ = curi;
							prevj = curj;
						}
						listV[0] = curi;
						listV[1] = curj;
						listV[2] = vbi;
						listV[3] = vbj;
						//BM_face_create_ngon_vcloud(bm, listV, 4, 1);

						/*
						va = get_vert_by_round_profile(bm, bp, i, pointA->v->co, pointHan->hv, pointAn->v->co);
						vb = get_vert_by_round_profile(bm, bp, i, pointB->v->co, pointHbn->hv, pointBn->v->co);
						// доп точка на текущем эдже, с отступом
						//calc_new_helper_point(bp, av, i, pointH->hv, hi);
						calc_new_helper_point(bm, bp, av, i, pointA->v->co, pointH->hv, pointB->v->co, hi);

						//BM_edge_create(bm, BM_vert_create(bm, hi, NULL), BM_vert_create(bm, va->co, NULL), NULL, 0);
						//BM_edge_create(bm, BM_vert_create(bm, hi, NULL), BM_vert_create(bm, vb->co, NULL), NULL, 0);

						prev = va;
						for (j = 0; j < bp->seg; j++){
							cur  =  get_vert_by_round_profile(bm, bp, j, va->co, hi, vb->co);
							BM_edge_create(bm, cur, prev, NULL, 0);
							prev = cur;
						}
						BM_edge_create(bm, cur, vb, NULL, 0);
						*/
					}
				}
			 }
		//} while (e != firstE);
	}
    BLI_array_free(vv);
}


VertexItem* find_vertex_item(AdditionalVert* av, BMEdge *e)
{
	VertexItem *item, *r = NULL;
	for (item = av->vertices.first; item; item = item->next){
		if ((item->onEdge == 1) && (item->edge1 == e))
			r = item;
	}
	return r;
}

/*
* Find Edge on the face with a vertex v
*/
BMEdge* find_edge_in_face(BMFace *f, BMEdge *e, BMVert* v)
{
	BMEdge *oe = NULL;
	BMLoop *l = f->l_first;
	do {
		if ((l->e != e) &&
				((l->e->v1 == v ) ||
				 BM_edge_other_vert(l->e, l->e->v1) == v)) {
			oe = l->e;
		}
		l = l->next;
	} while (l != f->l_first);
	return oe;
}

void rebuild_polygon(BMesh *bm, BevelParams *bp, BMFace *f)
{
	BMVert **vv = NULL; /* list vertex for new ngons */
	BMLoop *l;
	VertexItem *vItem;
	AdditionalVert *av;
	BLI_array_declare(vv);
	int count;
	int selectedEdgeCount = 0;
	int onEdgeCount = 0;
	int betweenEdge = 0;
	BMVert *savePoint = NULL;
	l = f->l_first;

	do {
		av = get_additionalVert_by_vert(bp, l->v);
		if ((BMO_elem_flag_test(bm, l->e, EDGE_SELECTED))){
			selectedEdgeCount ++;
		}
		if (av != NULL){
			for (vItem = av->vertices.first; vItem; vItem = vItem->next) {
				/* case 1, point located in edges */
				if ((vItem->onEdge == 1) && (is_edge_of_face(f, vItem->edge1))) {
					BLI_array_append(vv, vItem->v);
					onEdgeCount ++;
					if (bp->seg > 1){
						BMEdge *e = NULL, *se = NULL;
						VertexItem *tItem;
						BMVert *tv;
						int i;
						e = find_edge_in_face(f, vItem->edge1, av->v);
						tItem = find_vertex_item(av, e);
						if (tItem != NULL){
							float dir[3];
							se = find_selected_edge_in_av(bm, bp, av);
							sub_v3_v3v3(dir, BM_edge_other_vert(se, av->v)->co, av->v->co);
							for (i = 1; i < bp->seg; i++){
								int j, flag = 0;


								//tv = get_vert_by_round_profile(bm, bp, i, bp->seg, vItem->v->co, av->v->co, tItem->v->co);
								tv = get_vert_on_round_profile(bm, bp, i, bp->seg, dir, av->v->co, vItem->v->co, tItem->v->co);

								for (j = 0; j < BLI_array_count(vv); j++){
									if (vv[j] == tv)
										flag = 1;
								}
								if (!flag)
									BLI_array_append(vv, tv);
							}
						}
					}
				}
				/* case 2, point located between */
				if ((vItem->onEdge == 0) &&
					(is_edge_of_face(f, vItem->edge1)) &&
					(is_edge_of_face(f, vItem->edge2)))
					{
						BLI_array_append(vv, vItem->v);
						betweenEdge ++;
					}
				}

			if ((betweenEdge == 0) && ( !(onEdgeCount > 1) ) && (selectedEdgeCount == 0))
				savePoint = bevel_create_unic_vertex(bm, bp, av->v->co);


		} else {
			BLI_array_append(vv, l->v);
		}
		l = l->next;
	} while (l != f->l_first);

	if ((betweenEdge == 0) && ( !(onEdgeCount > 1) ) && (selectedEdgeCount == 0))
		BLI_array_append(vv, savePoint);

	count = BLI_array_count(vv);
	if (count > 2)
		BM_face_create_ngon_vcloud(bm, vv, count, 0);
	BLI_array_free(vv);
}

void bevel_rebuild_exist_polygons(BMesh *bm, BevelParams *bp, BMVert *v)
{
	BMFace *f;
	BMIter iter;
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l = f->l_first;
		do {
			if (l->v == v) {
				rebuild_polygon (bm, bp, f);
				BM_face_kill(bm,f);
			}
			l = l->next;
		} while (l != f->l_first);
	}
}



/*
*   aE = adjacentE
*          v ... v
*         /     /
*        aE f aE
*       /     /
*    --v--e--v--
*      |     |
*      aE f aE
*      |     |
*      v ... v
*/
BMVert* get_additional_vert(AdditionalVert *av, BMFace *f,  BMEdge *adjacentE)
{
	VertexItem *vi;
	BMVert *v = NULL;
	for (vi = av->vertices.first; vi; vi = vi->next) {
		if ((vi->onEdge == 1) && (vi->edge1 == adjacentE))
			v = vi->v;
			if (vi->f == f)
				v = vi->v;
	}
	return v;
}

void getOffsetCoordinate(float r[3], BevelParams *bp, BMEdge *e, BMVert* v)
{
	sub_v3_v3v3(r, BM_edge_other_vert(e, v)->co, v->co);
	normalize_v3(r);
	mul_v3_fl(r, bp->offset);
	add_v3_v3(r, v->co);
}



/*
* Build the polygons along the selected Edge
*/
void bevel_build_polygon(BMesh *bm, BevelParams *bp, BMEdge *e)
{

	AdditionalVert *item, *av1, *av2; // две доп структуры для текщего эджа
	BMVert *v, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
	BMFace *f;
	BMIter iter;
	BMEdge *e1, *e2;


	for (item = bp->vertList.first; item ; item = item->next){
		if (item->v == e->v1)
			av1 = item;
		if (item->v == BM_edge_other_vert(e, e->v1))
			av2 = item;
	}

	/* finding additional vertex for this edge */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l = f->l_first;
		do {
			if (l->e ==  e){
				e1 = find_edge_in_face(f, e, e->v1);
				v = get_additional_vert(av1, f, e1);
				if ((v != NULL) && ( v1 == NULL))
					v1 = v; //BLI_array_append(vv,v);
				if ((v != NULL) && (v1 != NULL))
					v4 = v;

				e2 = find_edge_in_face(f, e, BM_edge_other_vert(e, e->v1));
				v = get_additional_vert(av2, f, e2);
				if ((v != NULL) && (v2 == NULL))
					v2 = v; //BLI_array_append(vv,v);
				if ((v != NULL) && (v2 != NULL))
					v3 = v;
			}
			l = l->next;
		} while (l != f->l_first);
	}
	//if (BLI_array_count(vv) > 0)
		//BM_face_create_ngon_vcloud(bm, vv, BLI_array_count(vv), 0);
	//BLI_array_free(vv);

	/*	v4			 v3
	*	 \			/
	*	 e->v1(co1) - e->v2(co2)
	*	 /			\
	*  v1             v2 */

	/* round case */

	if (bp->seg > 1) {
		int i = 0;
		BMVert *v1i, *v2i, *v3i, *v4i;
		VertexItem *vItem1, *vItem2;
		float co1[3], co2[3];

		copy_v3_v3(co1, e->v1->co);
		copy_v3_v3(co2, BM_edge_other_vert(e, e->v1)->co);

	/*	BM_edge_create(bm, v1, BM_vert_create(bm, co1, NULL), NULL, 1);
		BM_edge_create(bm, v4, BM_vert_create(bm, co1, NULL), NULL, 1);
		BM_edge_create(bm, v3, BM_vert_create(bm, co2, NULL), NULL, 1);
		BM_edge_create(bm, v2, BM_vert_create(bm, co2, NULL), NULL, 1);
*/
		vItem1 = find_helper_vertex_item(av1, e);
		vItem2 = find_helper_vertex_item(av2, e);
		if (av1->countSelect > 2){
			//getOffsetCoordinate(co1, bp, e, e->v1);
			copy_v3_v3(co1, vItem1->hv);

		}
		if (av2->countSelect > 2)
			copy_v3_v3(co2, vItem2->hv);{
			//getOffsetCoordinate(co2, bp, e, BM_edge_other_vert(e,e->v1));
		}

		v1i = v1;
		v2i = v2;

		for (i = 1; i < bp->seg; i++){
			float dir[3];
			sub_v3_v3v3(dir, BM_edge_other_vert(e, av1->v)->co, av1->v->co);
			if (av1->countSelect == 2)
				v4i = get_vert_on_round_profile(bm, bp, i, bp->seg, dir, co1, v1->co, v4->co);
			else
				v4i = get_vert_on_round_profile(bm, bp, i, bp->seg, dir, co1, v1->co, v4->co);

			sub_v3_v3v3(dir, BM_edge_other_vert(e, av2->v)->co, av2->v->co);
			if (av2->countSelect == 2)
				v3i = get_vert_on_round_profile(bm, bp, i, bp->seg, dir, co2, v2->co, v3->co);
			else
				v3i = get_vert_on_round_profile(bm, bp, i, bp->seg, dir, co2, v2->co, v3->co);



			/*if (av1->countSelect == 2) {
				//v4i = get_vert_by_round_profile_rotate(bm, bp, i, v1, e->v1, v4, e);
				//v4i = get_vert_by_intersection(bm, bp, i, bp->seg, av1, e, v1, v4);
				v4i = get_vert_on_round_profile(bm, bp, i, bp->seg, e, av1->v, v1->co, v4->co);
			}
			else {
//				get_vert_by_round_profile_edge(bm, bp, e, av1, i, bp->seg, v1->co, co1, v4->co);
				//v4i = get_vert_by_round_profile(bm, bp, i,bp->seg, v1->co, co1, v4->co);
				v4i = get_vert_on_round_profile(bm, bp, i, bp->seg, e, av1->v, v1->co, v4->co);
			}

			if (av2->countSelect ==  2){
				//v3i = get_vert_by_round_profile_rotate(bm, bp, i, v2, BM_edge_other_vert(e,e->v1), v3, e);
				//v3i = get_vert_by_intersection(bm, bp, i, bp->seg, av2, e, v2, v3 );
				v3i = get_vert_on_round_profile(bm, bp, i, bp->seg, e, av2->v, v2->co, v3->co);
			}
			else {
				//get_vert_by_round_profile_edge(bm, bp, e, av2, i, bp->seg, v2->co, co2, v3->co);
			//	v3i = get_vert_by_round_profile(bm, bp, i, bp->seg, v2->co, co2, v3->co);
				v3i = get_vert_on_round_profile(bm, bp, i, bp->seg, e, av2->v, v2->co, v3->co);

			}*/


			BM_face_create_quad_tri(bm, v1i, v2i, v3i, v4i, NULL, 0);
			v1i = v4i;
			v2i = v3i;
		}
		BM_face_create_quad_tri(bm, v1i, v2i, v3, v4, NULL, 0);
	}

	/* linear case */
	if (bp->seg == 1){
		if ((v1 != NULL) && (v2 != NULL) && (v3 != NULL) && (v4 != NULL))
			BM_face_create_quad_tri(bm, v1, v2, v3, v4, NULL, 0);
	}
}


void free_bevel_params(BevelParams *bp)
{
	AdditionalVert *item;
	for (item = bp->vertList.first; item ; item = item->next)
			BLI_freelistN(&item->vertices);
	BLI_freelistN(&bp->vertList);
	BLI_freelistN(&bp->newVertList);
}

float get_max_offset(BMesh *bm, BMOperator *op)
{
	BMIter iter;
	BMEdge *e = NULL;
	BMEdge *minE = NULL;
	BMVert *v;
	float min = 1e30, vect[3], localMin = 1e30;


	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		sub_v3_v3v3( vect, e->v1->co,(BM_edge_other_vert(e, e->v1)->co));
		if (len_v3(vect) < min){
			min = len_v3(vect);
			minE = e;
		}
	}

	if (minE!= NULL) {
		localMin = min;

		v = minE->v1;
		e = bmesh_disk_faceedge_find_first(minE, v);
		do {
			float va[3], vb[3], len;
			e = bmesh_disk_edge_next(e, v);
			sub_v3_v3v3(va,  BM_edge_other_vert(minE,v )->co, v->co);
			sub_v3_v3v3(vb,  BM_edge_other_vert(e, v)->co, v->co);
			len = min * sin (angle_v3v3(va, vb));

			if ((fabs(len) > BEVEL_EPSILON) && (len < localMin))
				localMin = len;

		} while (e != minE);

		v = BM_edge_other_vert (minE, minE->v1);
		e = bmesh_disk_faceedge_find_first(minE, v);
		do {
			float va[3], vb[3], len;
			e = bmesh_disk_edge_next(e, v);
			sub_v3_v3v3(va,  BM_edge_other_vert(minE,v )->co, v->co);
			sub_v3_v3v3(vb,  BM_edge_other_vert(e, v)->co, v->co);
			len = min * sin (angle_v3v3(va, vb));
			if ((fabs(len)> BEVEL_EPSILON) && (len < localMin))
				localMin = len;

		} while (e != minE);
		min = localMin;
	}
	return min;
}

void bmo_bevel_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v;
	BMEdge *e;
	BevelParams bp;
	float fac = BMO_slot_float_get(op, "percent");
	float max;
	//int seg = BMO_slot_int_get(op, "segmentation");
	//bp.offset = 3;
	//max = get_max_offset(bm, op);

	//if (fac > max)
	//	bp.offset = max;
	//else
		bp.offset = fac;

	bp.op = op;

	bp.seg = BMO_slot_int_get(op, "segmentation");
	if (bp.offset > 0 ){
		bp.vertList.first = bp.vertList.last = NULL;
		bp.newVertList.first = bp.newVertList.last = NULL;
		/* The analysis of the input vertices and execution additional constructions */
		BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
			bevel_aditional_construction_by_vert(bm, &bp, op, v);
		}
		/* Build polgiony found at verteces */
		BMO_ITER(e, &siter, bm, op, "geom", BM_EDGE){
			bevel_build_polygon(bm, &bp, e);
		}

		BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
			bevel_build_polygons_arround_vertex(bm, &bp, op, v);
			bevel_rebuild_exist_polygons(bm, &bp,v);
		}

		BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
			BM_vert_kill(bm, v);
		}
		//free_bevel_params(&bp);
	}

}


void bmo_bevel_exec_old(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMEdge *e;
	BMVert *v;
	BMFace **faces = NULL, *f;
	LoopTag *tags = NULL, *tag;
	EdgeTag *etags = NULL;
	BMVert **verts = NULL;
	BMEdge **edges = NULL;
	BLI_array_declare(faces);
	BLI_array_declare(tags);
	BLI_array_declare(etags);
	BLI_array_declare(verts);
	BLI_array_declare(edges);
	SmallHash hash;
	float fac = BMO_slot_float_get(op, "percent");
	const short do_even = BMO_slot_bool_get(op, "use_even");
	const short do_dist = BMO_slot_bool_get(op, "use_dist");
	int i, li, has_elens, HasMDisps = CustomData_has_layer(&bm->ldata, CD_MDISPS);
	
	has_elens = CustomData_has_layer(&bm->edata, CD_PROP_FLT) && BMO_slot_bool_get(op, "use_lengths");
	if (has_elens) {
		li = BMO_slot_int_get(op, "lengthlayer");
	}
	
	BLI_smallhash_init(&hash);
	
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		BMO_elem_flag_enable(bm, e, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v1, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v2, BEVEL_FLAG | BEVEL_DEL);
		if (BM_edge_face_count(e) < 2) {
			BMO_elem_flag_disable(bm, e, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v1, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v2, BEVEL_DEL);
		}
#if 0
		if (BM_edge_face_count(e) == 0) {
			BMVert *verts[2] = {e->v1, e->v2};
			BMEdge *edges[2] = {e, BM_edge_create(bm, e->v1, e->v2, e, 0)};
			
			BMO_elem_flag_enable(bm, edges[1], BEVEL_FLAG);
			BM_face_create(bm, verts, edges, 2, FALSE);
		}
#endif
	}
	
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMO_elem_flag_enable(bm, v, VERT_OLD);
	}

#if 0
	//a bit of cleaner code that, alas, doens't work.
	/* build edge tag */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e->v1, BEVEL_FLAG) || BMO_elem_flag_test(bm, e->v2, BEVEL_FLAG)) {
			BMIter liter;
			BMLoop *l;
			
			if (!BMO_elem_flag_test(bm, e, EDGE_OLD)) {
				BM_elem_index_set(e, BLI_array_count(etags)); /* set_dirty! */
				BLI_array_grow_one(etags);
				
				BMO_elem_flag_enable(bm, e, EDGE_OLD);
			}
			
			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				BMLoop *l2;
				BMIter liter2;
				
				if (BMO_elem_flag_test(bm, l->f, BEVEL_FLAG))
					continue;

				BM_ITER_ELEM (l2, &liter2, l->f, BM_LOOPS_OF_FACE) {
					BM_elem_index_set(l2, BLI_array_count(tags)); /* set_loop */
					BLI_array_grow_one(tags);

					if (!BMO_elem_flag_test(bm, l2->e, EDGE_OLD)) {
						BM_elem_index_set(l2->e, BLI_array_count(etags)); /* set_dirty! */
						BLI_array_grow_one(etags);
						
						BMO_elem_flag_enable(bm, l2->e, EDGE_OLD);
					}
				}

				BMO_elem_flag_enable(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		}
		else {
			BM_elem_index_set(e, -1); /* set_dirty! */
		}
	}
#endif
	
	/* create and assign looptag structure */
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		BMLoop *l;
		BMIter liter;

		BMO_elem_flag_enable(bm, e->v1, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v2, BEVEL_FLAG | BEVEL_DEL);
		
		if (BM_edge_face_count(e) < 2) {
			BMO_elem_flag_disable(bm, e, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v1, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v2, BEVEL_DEL);
		}
		
		if (!BLI_smallhash_haskey(&hash, (intptr_t)e)) {
			BLI_array_grow_one(etags);
			BM_elem_index_set(e, BLI_array_count(etags) - 1); /* set_dirty! */
			BLI_smallhash_insert(&hash, (intptr_t)e, NULL);
			BMO_elem_flag_enable(bm, e, EDGE_OLD);
		}
		
		/* find all faces surrounding e->v1 and, e->v2 */
		for (i = 0; i < 2; i++) {
			BM_ITER_ELEM (l, &liter, i ? e->v2:e->v1, BM_LOOPS_OF_VERT) {
				BMLoop *l2;
				BMIter liter2;
				
				/* see if we've already processed this loop's fac */
				if (BLI_smallhash_haskey(&hash, (intptr_t)l->f))
					continue;
				
				/* create tags for all loops in l-> */
				BM_ITER_ELEM (l2, &liter2, l->f, BM_LOOPS_OF_FACE) {
					BLI_array_grow_one(tags);
					BM_elem_index_set(l2, BLI_array_count(tags) - 1); /* set_loop */
					
					if (!BLI_smallhash_haskey(&hash, (intptr_t)l2->e)) {
						BLI_array_grow_one(etags);
						BM_elem_index_set(l2->e, BLI_array_count(etags) - 1); /* set_dirty! */
						BLI_smallhash_insert(&hash, (intptr_t)l2->e, NULL);
						BMO_elem_flag_enable(bm, l2->e, EDGE_OLD);
					}
				}

				BLI_smallhash_insert(&hash, (intptr_t)l->f, NULL);
				BMO_elem_flag_enable(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		}
	}

	bm->elem_index_dirty |= BM_EDGE;
	
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMIter eiter;
		
		if (!BMO_elem_flag_test(bm, v, BEVEL_FLAG))
			continue;
		
		BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
			if (!BMO_elem_flag_test(bm, e, BEVEL_FLAG) && !ETAG_GET(e, v)) {
				BMVert *v2;
				float co[3];
				
				v2 = BM_edge_other_vert(e, v);
				sub_v3_v3v3(co, v2->co, v->co);
				if (has_elens) {
					float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, e->head.data, CD_PROP_FLT, li);

					normalize_v3(co);
					mul_v3_fl(co, elen);
				}
				
				mul_v3_fl(co, fac);
				add_v3_v3(co, v->co);
				
				v2 = BM_vert_create(bm, co, v);
				ETAG_SET(e, v, v2);
			}
		}
	}
	
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		
		BMO_elem_flag_enable(bm, faces[i], FACE_OLD);
		
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			float co[3];

			if (BMO_elem_flag_test(bm, l->e, BEVEL_FLAG)) {
				if (BMO_elem_flag_test(bm, l->prev->e, BEVEL_FLAG)) {
					tag = tags + BM_elem_index_get(l);
					calc_corner_co(l, fac, co, do_dist, do_even);
					tag->newv = BM_vert_create(bm, co, l->v);
				}
				else {
					tag = tags + BM_elem_index_get(l);
					tag->newv = ETAG_GET(l->prev->e, l->v);
					
					if (!tag->newv) {
						sub_v3_v3v3(co, l->prev->v->co, l->v->co);
						if (has_elens) {
							float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, l->prev->e->head.data,
							                                              CD_PROP_FLT, li);

							normalize_v3(co);
							mul_v3_fl(co, elen);
						}

						mul_v3_fl(co, fac);
						add_v3_v3(co, l->v->co);

						tag->newv = BM_vert_create(bm, co, l->v);
						
						ETAG_SET(l->prev->e, l->v, tag->newv);
					}
				}
			}
			else if (BMO_elem_flag_test(bm, l->v, BEVEL_FLAG)) {
				tag = tags + BM_elem_index_get(l);
				tag->newv = ETAG_GET(l->e, l->v);

				if (!tag->newv) {
					sub_v3_v3v3(co, l->next->v->co, l->v->co);
					if (has_elens) {
						float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, l->e->head.data, CD_PROP_FLT, li);

						normalize_v3(co);
						mul_v3_fl(co, elen);
					}
					
					mul_v3_fl(co, fac);
					add_v3_v3(co, l->v->co);

					tag = tags + BM_elem_index_get(l);
					tag->newv = BM_vert_create(bm, co, l->v);
					
					ETAG_SET(l->e, l->v, tag->newv);
				}
			}
			else {
				tag = tags + BM_elem_index_get(l);
				tag->newv = l->v;
				BMO_elem_flag_disable(bm, l->v, BEVEL_DEL);
			}
		}
	}
	
	/* create new faces inset from original face */
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f;
		BMVert *lastv = NULL, *firstv = NULL;

		BMO_elem_flag_enable(bm, faces[i], BEVEL_DEL);
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			BMVert *v2;
			
			tag = tags + BM_elem_index_get(l);
			BLI_array_append(verts, tag->newv);
			
			if (!firstv)
				firstv = tag->newv;
			
			if (lastv) {
				e = BM_edge_create(bm, lastv, tag->newv, l->e, TRUE);
				BM_elem_attrs_copy(bm, bm, l->prev->e, e);
				BLI_array_append(edges, e);
			}
			lastv = tag->newv;
			
			v2 = ETAG_GET(l->e, l->next->v);
			
			tag = &tags[BM_elem_index_get(l->next)];
			if (!BMO_elem_flag_test(bm, l->e, BEVEL_FLAG) && v2 && v2 != tag->newv) {
				BLI_array_append(verts, v2);
				
				e = BM_edge_create(bm, lastv, v2, l->e, TRUE);
				BM_elem_attrs_copy(bm, bm, l->e, e);
				
				BLI_array_append(edges, e);
				lastv = v2;
			}
		}
		
		e = BM_edge_create(bm, firstv, lastv, BM_FACE_FIRST_LOOP(faces[i])->e, TRUE);
		if (BM_FACE_FIRST_LOOP(faces[i])->prev->e != e) {
			BM_elem_attrs_copy(bm, bm, BM_FACE_FIRST_LOOP(faces[i])->prev->e, e);
		}
		BLI_array_append(edges, e);
		
		f = BM_face_create_ngon(bm, verts[0], verts[1], edges, BLI_array_count(edges), FALSE);
		if (!f) {
			printf("%s: could not make face!\n", __func__);
			continue;
		}

		BMO_elem_flag_enable(bm, f, FACE_NEW);
	}

	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		int j;
		
		/* create quad spans between split edge */
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			BMVert *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
			
			if (!BMO_elem_flag_test(bm, l->e, BEVEL_FLAG))
				continue;
			
			v1 = tags[BM_elem_index_get(l)].newv;
			v2 = tags[BM_elem_index_get(l->next)].newv;
			if (l->radial_next != l) {
				v3 = tags[BM_elem_index_get(l->radial_next)].newv;
				if (l->radial_next->next->v == l->next->v) {
					v4 = v3;
					v3 = tags[BM_elem_index_get(l->radial_next->next)].newv;
				}
				else {
					v4 = tags[BM_elem_index_get(l->radial_next->next)].newv;
				}
			}
			else {
				/* the loop is on a boundar */
				v3 = l->next->v;
				v4 = l->v;
				
				for (j = 0; j < 2; j++) {
					BMIter eiter;
					BMVert *v = j ? v4 : v3;

					BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
						if (!BM_vert_in_edge(e, v3) || !BM_vert_in_edge(e, v4))
							continue;
						
						if (!BMO_elem_flag_test(bm, e, BEVEL_FLAG) && BMO_elem_flag_test(bm, e, EDGE_OLD)) {
							BMVert *vv;
							
							vv = ETAG_GET(e, v);
							if (!vv || BMO_elem_flag_test(bm, vv, BEVEL_FLAG))
								continue;
							
							if (j) {
								v1 = vv;
							}
							else {
								v2 = vv;
							}
							break;
						}
					}
				}

				BMO_elem_flag_disable(bm, v3, BEVEL_DEL);
				BMO_elem_flag_disable(bm, v4, BEVEL_DEL);
			}
			
			if (v1 != v2 && v2 != v3 && v3 != v4) {
				BMIter liter2;
				BMLoop *l2, *l3;
				BMEdge *e1, *e2;
				float d1, d2, *d3;
				
				f = BM_face_create_quad_tri(bm, v4, v3, v2, v1, l->f, TRUE);

				e1 = BM_edge_exists(v4, v3);
				e2 = BM_edge_exists(v2, v1);
				BM_elem_attrs_copy(bm, bm, l->e, e1);
				BM_elem_attrs_copy(bm, bm, l->e, e2);
				
				/* set edge lengths of cross edges as the average of the cross edges they're based o */
				if (has_elens) {
					/* angle happens not to be used. why? - not sure it just isn't - campbell.
					 * leave this in in case we need to use it later */
#if 0
					float ang;
#endif
					e1 = BM_edge_exists(v1, v4);
					e2 = BM_edge_exists(v2, v3);
					
					if (l->radial_next->v == l->v) {
						l2 = l->radial_next->prev;
						l3 = l->radial_next->next;
					}
					else {
						l2 = l->radial_next->next;
						l3 = l->radial_next->prev;
					}
					
					d3 = CustomData_bmesh_get_n(&bm->edata, e1->head.data, CD_PROP_FLT, li);
					d1 = *(float *)CustomData_bmesh_get_n(&bm->edata, l->prev->e->head.data, CD_PROP_FLT, li);
					d2 = *(float *)CustomData_bmesh_get_n(&bm->edata, l2->e->head.data, CD_PROP_FLT, li);
#if 0
					ang = angle_v3v3v3(l->prev->v->co, l->v->co, BM_edge_other_vert(l2->e, l->v)->co);
#endif
					*d3 = (d1 + d2) * 0.5f;
					
					d3 = CustomData_bmesh_get_n(&bm->edata, e2->head.data, CD_PROP_FLT, li);
					d1 = *(float *)CustomData_bmesh_get_n(&bm->edata, l->next->e->head.data, CD_PROP_FLT, li);
					d2 = *(float *)CustomData_bmesh_get_n(&bm->edata, l3->e->head.data, CD_PROP_FLT, li);
#if 0
					ang = angle_v3v3v3(BM_edge_other_vert(l->next->e, l->next->v)->co, l->next->v->co,
					                   BM_edge_other_vert(l3->e, l->next->v)->co);
#endif
					*d3 = (d1 + d2) * 0.5f;
				}

				if (!f) {
					fprintf(stderr, "%s: face index out of range! (bmesh internal error)\n", __func__);
					continue;
				}
				
				BMO_elem_flag_enable(bm, f, FACE_NEW | FACE_SPAN);
				
				/* un-tag edges in f for deletio */
				BM_ITER_ELEM (l2, &liter2, f, BM_LOOPS_OF_FACE) {
					BMO_elem_flag_disable(bm, l2->e, BEVEL_DEL);
				}
			}
			else {
				f = NULL;
			}
		}
	}
	
	/* fill in holes at vertices */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMIter eiter;
		BMVert *vv, *vstart = NULL, *lastv = NULL;
		SmallHash tmphash;
		int rad, insorig = 0, err = 0;

		BLI_smallhash_init(&tmphash);

		if (!BMO_elem_flag_test(bm, v, BEVEL_FLAG))
			continue;
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
			BMIter liter;
			BMVert *v1 = NULL, *v2 = NULL;
			BMLoop *l;
			
			if (BM_edge_face_count(e) < 2)
				insorig = 1;
			
			if (BM_elem_index_get(e) == -1)
				continue;
			
			rad = 0;
			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				if (!BMO_elem_flag_test(bm, l->f, FACE_OLD))
					continue;
				
				rad++;
				
				tag = tags + BM_elem_index_get((l->v == v) ? l : l->next);
				
				if (!v1)
					v1 = tag->newv;
				else if (!v2)
					v2 = tag->newv;
			}
			
			if (rad < 2)
				insorig = 1;
			
			if (!v1)
				v1 = ETAG_GET(e, v);
			if (!v2 || v1 == v2)
				v2 = ETAG_GET(e, v);
			
			if (v1) {
				if (!BLI_smallhash_haskey(&tmphash, (intptr_t)v1)) {
					BLI_array_append(verts, v1);
					BLI_smallhash_insert(&tmphash, (intptr_t)v1, NULL);
				}
				
				if (v2 && v1 != v2 && !BLI_smallhash_haskey(&tmphash, (intptr_t)v2)) {
					BLI_array_append(verts, v2);
					BLI_smallhash_insert(&tmphash, (intptr_t)v2, NULL);
				}
			}
		}
		
		if (!BLI_array_count(verts))
			continue;
		
		if (insorig) {
			BLI_array_append(verts, v);
			BLI_smallhash_insert(&tmphash, (intptr_t)v, NULL);
		}
		
		/* find edges that exist between vertices in verts.  this is basically
		 * a topological walk of the edges connecting them */
		vstart = vstart ? vstart : verts[0];
		vv = vstart;
		do {
			BM_ITER_ELEM (e, &eiter, vv, BM_EDGES_OF_VERT) {
				BMVert *vv2 = BM_edge_other_vert(e, vv);
				
				if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
					/* if we've go over the same vert twice, break out of outer loop */
					if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
						e = NULL;
						err = 1;
						break;
					}
					
					/* use self pointer as ta */
					BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
					BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
					
					lastv = vv;
					BLI_array_append(edges, e);
					vv = vv2;
					break;
				}
			}

			if (e == NULL) {
				break;
			}
		} while (vv != vstart);
		
		if (err) {
			continue;
		}

		/* there may not be a complete loop of edges, so start again and make
		 * final edge afterwards.  in this case, the previous loop worked to
		 * find one of the two edges at the extremes. */
		if (vv != vstart) {
			/* undo previous taggin */
			for (i = 0; i < BLI_array_count(verts); i++) {
				BLI_smallhash_remove(&tmphash, (intptr_t)verts[i]);
				BLI_smallhash_insert(&tmphash, (intptr_t)verts[i], NULL);
			}

			vstart = vv;
			lastv = NULL;
			BLI_array_empty(edges);
			do {
				BM_ITER_ELEM (e, &eiter, vv, BM_EDGES_OF_VERT) {
					BMVert *vv2 = BM_edge_other_vert(e, vv);
					
					if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
						/* if we've go over the same vert twice, break out of outer loo */
						if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
							e = NULL;
							err = 1;
							break;
						}
						
						/* use self pointer as ta */
						BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
						BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
						
						lastv = vv;
						BLI_array_append(edges, e);
						vv = vv2;
						break;
					}
				}
				if (e == NULL)
					break;
			} while (vv != vstart);
			
			if (!err) {
				e = BM_edge_create(bm, vv, vstart, NULL, TRUE);
				BLI_array_append(edges, e);
			}
		}
		
		if (err)
			continue;
		
		if (BLI_array_count(edges) >= 3) {
			BMFace *f;
			
			if (BM_face_exists(bm, verts, BLI_array_count(verts), &f))
				continue;
			
			f = BM_face_create_ngon(bm, lastv, vstart, edges, BLI_array_count(edges), FALSE);
			if (!f) {
				fprintf(stderr, "%s: in bevel vert fill! (bmesh internal error)\n", __func__);
			}
			else {
				BMO_elem_flag_enable(bm, f, FACE_NEW | FACE_HOLE);
			}
		}
		BLI_smallhash_release(&tmphash);
	}
	
	/* copy over customdat */
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f = faces[i];
		
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			BMLoop *l2;
			BMIter liter2;
			
			tag = tags + BM_elem_index_get(l);
			if (!tag->newv)
				continue;
			
			BM_ITER_ELEM (l2, &liter2, tag->newv, BM_LOOPS_OF_VERT) {
				if (!BMO_elem_flag_test(bm, l2->f, FACE_NEW) || (l2->v != tag->newv && l2->v != l->v))
					continue;
				
				if (tag->newv != l->v || HasMDisps) {
					BM_elem_attrs_copy(bm, bm, l->f, l2->f);
					BM_loop_interp_from_face(bm, l2, l->f, TRUE, TRUE);
				}
				else {
					BM_elem_attrs_copy(bm, bm, l->f, l2->f);
					BM_elem_attrs_copy(bm, bm, l, l2);
				}
				
				if (HasMDisps) {
					BMLoop *l3;
					BMIter liter3;
					
					BM_ITER_ELEM (l3, &liter3, l2->f, BM_LOOPS_OF_FACE) {
						BM_loop_interp_multires(bm, l3, l->f);
					}
				}
			}
		}
	}
	
	/* handle vertices along boundary edge */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, VERT_OLD) &&
		    BMO_elem_flag_test(bm, v, BEVEL_FLAG) &&
		    !BMO_elem_flag_test(bm, v, BEVEL_DEL))
		{
			BMLoop *l;
			BMLoop *lorig = NULL;
			BMIter liter;
			
			BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
				// BMIter liter2;
				// BMLoop *l2 = l->v == v ? l : l->next, *l3;
				
				if (BMO_elem_flag_test(bm, l->f, FACE_OLD)) {
					lorig = l;
					break;
				}
			}
			
			if (!lorig)
				continue;
			
			BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
				BMLoop *l2 = l->v == v ? l : l->next;
				
				BM_elem_attrs_copy(bm, bm, lorig->f, l2->f);
				BM_elem_attrs_copy(bm, bm, lorig, l2);
			}
		}
	}
#if 0
	/* clean up any remaining 2-edged face */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (f->len == 2) {
			BMFace *faces[2] = {f, BM_FACE_FIRST_LOOP(f)->radial_next->f};
			
			if (faces[0] == faces[1])
				BM_face_kill(bm, f);
			else
				BM_faces_join(bm, faces, 2);
		}
	}
#endif

	BMO_op_callf(bm, op->flag, "delete geom=%fv context=%i", BEVEL_DEL, DEL_VERTS);

	/* clean up any edges that might not get properly delete */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e, EDGE_OLD) && !e->l)
			BMO_elem_flag_enable(bm, e, BEVEL_DEL);
	}

	BMO_op_callf(bm, op->flag, "delete geom=%fe context=%i", BEVEL_DEL, DEL_EDGES);
	BMO_op_callf(bm, op->flag, "delete geom=%ff context=%i", BEVEL_DEL, DEL_FACES);
	
	BLI_smallhash_release(&hash);
	BLI_array_free(tags);
	BLI_array_free(etags);
	BLI_array_free(verts);
	BLI_array_free(edges);
	BLI_array_free(faces);
	
	BMO_slot_buffer_from_enabled_flag(bm, op, "face_spans", BM_FACE, FACE_SPAN);
	BMO_slot_buffer_from_enabled_flag(bm, op, "face_holes", BM_FACE, FACE_HOLE);
}
