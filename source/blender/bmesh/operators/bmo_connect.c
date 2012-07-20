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

/** \file blender/bmesh/operators/bmo_connect.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */


#define VERT_INPUT	1
#define EDGE_OUT	1
#define FACE_NEW	2
#define EDGE_MARK	4
#define EDGE_DONE	8

#define FACE_MARK       1
#define EDGE_CONNECTED  7
#define EDGE_NON_CONNECTED  9

#define LINEAR_INTER  2
#define CUBIC_INTER   4

typedef struct VertexItem{
    struct VertexItem *next, *prev;
    BMVert *v;
}VertexItem;


typedef struct BridgeParams{
    ListBase newVertices; // list of new vertex
    int inter;  // interpolation
    int seg; // segmentation param
    float strenght;
    float centrod1[3]; // coordinate of centrod input loops
    float centrod2[3];
} BridgeParams;


void bmo_connect_verts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMFace *f, *nf;
	BMLoop **loops = NULL, *lastl = NULL;
	BLI_array_declare(loops);
	BMLoop *l, *nl;
	BMVert **verts = NULL;
	BLI_array_declare(verts);
	int i;
	
	BMO_slot_buffer_flag_enable(bm, op, "verts", BM_VERT, VERT_INPUT);

	for (f = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, NULL); f; f = BM_iter_step(&iter)) {
		BLI_array_empty(loops);
		BLI_array_empty(verts);
		
		if (BMO_elem_flag_test(bm, f, FACE_NEW)) {
			continue;
		}

		l = BM_iter_new(&liter, bm, BM_LOOPS_OF_FACE, f);
		lastl = NULL;
		for ( ; l; l = BM_iter_step(&liter)) {
			if (BMO_elem_flag_test(bm, l->v, VERT_INPUT)) {
				if (!lastl) {
					lastl = l;
					continue;
				}

				if (lastl != l->prev && lastl != l->next) {
					BLI_array_grow_one(loops);
					loops[BLI_array_count(loops) - 1] = lastl;

					BLI_array_grow_one(loops);
					loops[BLI_array_count(loops) - 1] = l;

				}
				lastl = l;
			}
		}

		if (BLI_array_count(loops) == 0) {
			continue;
		}
		
		if (BLI_array_count(loops) > 2) {
			BLI_array_grow_one(loops);
			loops[BLI_array_count(loops) - 1] = loops[BLI_array_count(loops) - 2];

			BLI_array_grow_one(loops);
			loops[BLI_array_count(loops) - 1] = loops[0];
		}

		BM_face_legal_splits(bm, f, (BMLoop *(*)[2])loops, BLI_array_count(loops) / 2);
		
		for (i = 0; i < BLI_array_count(loops) / 2; i++) {
			if (loops[i * 2] == NULL) {
				continue;
			}

			BLI_array_grow_one(verts);
			verts[BLI_array_count(verts) - 1] = loops[i * 2]->v;

			BLI_array_grow_one(verts);
			verts[BLI_array_count(verts) - 1] = loops[i * 2 + 1]->v;
		}

		for (i = 0; i < BLI_array_count(verts) / 2; i++) {
			nf = BM_face_split(bm, f, verts[i * 2], verts[i * 2 + 1], &nl, NULL, FALSE);
			f = nf;
			
			if (!nl || !nf) {
				BMO_error_raise(bm, op, BMERR_CONNECTVERT_FAILED, NULL);
				BLI_array_free(loops);
				return;
			}
			BMO_elem_flag_enable(bm, nf, FACE_NEW);
			BMO_elem_flag_enable(bm, nl->e, EDGE_OUT);
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, "edgeout", BM_EDGE, EDGE_OUT);

	BLI_array_free(loops);
	BLI_array_free(verts);
}

static BMVert *get_outer_vert(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMEdge *e2;
	int i;

	i = 0;
	BM_ITER_ELEM (e2, &iter, e->v1, BM_EDGES_OF_VERT) {
		if (BMO_elem_flag_test(bm, e2, EDGE_MARK)) {
			i++;
		}
	}

	return (i == 2) ? e->v2 : e->v1;
}

/* Clamp x to the interval {0..len-1}, with wrap-around */
static int clamp_index(const int x, const int len)
{
	if (x >= 0) {
		return x % len;
	}
	else {
		int r = len - (-x % len);
		if (r == len)
			return len - 1;
		else
			return r;
	}
}

/* There probably is a better way to swap BLI_arrays, or if there
 * isn't there should be... */
#define ARRAY_SWAP(elemtype, arr1, arr2)                                      \
	{                                                                         \
		int i;                                                                \
		elemtype *arr_tmp = NULL;                                             \
		BLI_array_declare(arr_tmp);                                           \
		for (i = 0; i < BLI_array_count(arr1); i++) {                         \
			BLI_array_append(arr_tmp, arr1[i]);                               \
		}                                                                     \
		BLI_array_empty(arr1);                                                \
		for (i = 0; i < BLI_array_count(arr2); i++) {                         \
			BLI_array_append(arr1, arr2[i]);                                  \
		}                                                                     \
		BLI_array_empty(arr2);                                                \
		for (i = 0; i < BLI_array_count(arr_tmp); i++) {                      \
			BLI_array_append(arr2, arr_tmp[i]);                               \
		}                                                                     \
		BLI_array_free(arr_tmp);                                              \
	}

/* get the 2 loops matching 2 verts.
 * first attempt to get the face corners that use the edge defined by v1 & v2,
 * if that fails just get any loop thats on the vert (the first one) */
static void bm_vert_loop_pair(BMesh *bm, BMVert *v1, BMVert *v2, BMLoop **l1, BMLoop **l2)
{
	BMIter liter;
	BMLoop *l;

	if ((v1->e && v1->e->l) &&
	    (v2->e && v2->e->l))
	{
		BM_ITER_ELEM (l, &liter, v1, BM_LOOPS_OF_VERT) {
			if (l->prev->v == v2) {
				*l1 = l;
				*l2 = l->prev;
				return;
			}
			else if (l->next->v == v2) {
				*l1 = l;
				*l2 = l->next;
				return;
			}
		}
	}

	/* fallback to _any_ loop */
	*l1 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v1, 0);
	*l2 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v2, 0);
}

//TODO: rename function
// return count entry element into list
int bmo_bridge_array_entry_elem(BMEdge **list, int size,  BMEdge *elem)
{
    int i = 0, count = 0;

    for (i = 0; i < size; i++)
    {
        if (list[i] == elem)
            count ++;
    }
    return count;
}

/*
* return coordinate of middle edge
*/
void  get_middle_edge(BMEdge* e, float v[3]){
    BMVert *v1, *v2;
    v1 = e->v1;
    v2 = BM_edge_other_vert(e, v1);
    mid_v3_v3v3(v, v1->co, v2->co);
}

/*
*   this function find centroid of group vertex
*   return coordinate of centroid
*/
void get_centroid(BMVert **vv, int vert_count, float center[3]){
    int i;
    center[0] = 0; center[1] = 0; center[2] = 0;
    for (i = 0; i < vert_count; i++){
        add_v3_v3(center, vv[i]->co);
    }
    mul_v3_fl(center, 1.0f/vert_count);
}


BMVert* bridge_check_existing_vertex(BridgeParams *bp, float co[3])
{
    float epsilon = 1e-6;
    BMVert *vert = NULL;
    VertexItem *item;
    for (item = bp->newVertices.first; item; item = item->next)
    {
        if (compare_v3v3(item->v->co, co, epsilon))
            vert = item->v;
    }
    return vert;
}
/*
* this function return vertex of linear interpolation between v1 and v2
*/
BMVert* get_linear_seg(BMesh *bm, BridgeParams* bp, int n, BMVert *v1, BMVert *v2)
{
    float co[3];
    BMVert *v;
    VertexItem *item;

    co[0] = v1->co[0] + (v2->co[0]-v1->co[0])*n / bp->seg;
    co[1] = v1->co[1] + (v2->co[1]-v1->co[1])*n / bp->seg;
    co[2] = v1->co[2] + (v2->co[2]-v1->co[2])*n / bp->seg;

    v = bridge_check_existing_vertex(bp, co);
    if (!v)
    {
        item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
        item->v = BM_vert_create(bm, co, NULL);
        BLI_addtail(&bp->newVertices, item);
        v = item->v;
    }
    return v;
}

/*
* this function return vertex of cubic interpolation between v1 and v2
* Parametric cubic spline in Hermite form
* 0 <= t <= 1
*/
BMVert* get_cubic_seg(BMesh *bm, BridgeParams *bp, int n, BMVert *v1, BMVert *v2){
    BMVert *v = NULL;
    VertexItem *item;
    float t, co[3], r1[3], r2[3]/*, mn[3]*/;
    t = (float) (n) / bp->seg;

    //sub_v3_v3v3(r1,  bp->centrod2 , v1->co);
    //sub_v3_v3v3(r2,  v2->co, bp->centrod1);

    sub_v3_v3v3(r1,  bp->centrod1 , v1->co);
    sub_v3_v3v3(r2,  v2->co, bp->centrod2);
    //sub_v3_v3v3(mn, v1->co, v2->co);

    //mul_v3_fl(r1, bp->strenght);
    //mul_v3_fl(r2, bp->strenght);
    //add_v3_v3(r1, mn);
    //add_v3_v3(r2, mn);

    mul_v3_fl(r1, 2*asin(bp->strenght));
    mul_v3_fl(r2, 2*asin(bp->strenght));

    co[0] = v1->co[0] * (2 * t * t * t - 3 * t * t + 1) +
            v2->co[0] * (-2 * t * t * t + 3 * t * t ) +
            r1[0] * (t * t * t - 2 * t * t + t) +
            r2[0] * (t * t * t - t * t);
    co[1] = v1->co[1] * (2 * t * t * t - 3 * t * t + 1) +
            v2->co[1] * (-2 * t * t * t + 3 * t * t ) +
            r1[1] * (t * t * t - 2 * t * t + t) +
            r2[1] * (t * t * t - t * t);
    co[2] = v1->co[2] * (2 * t * t * t - 3 * t * t + 1) +
            v2->co[2] * (-2 * t * t * t + 3 * t * t ) +
            r1[2] * (t * t * t - 2 * t * t + t) +
            r2[2] * (t * t * t - t * t);

    v = bridge_check_existing_vertex(bp, co);
    if (!v)
    {
        item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
        item->v = BM_vert_create(bm, co, NULL);
        BLI_addtail(&bp->newVertices, item);
        v = item->v;
    }
    return v;
}


float get_cos_v3v3(float a[3], float b[3]){
    float cos_ab = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) /
            (sqrt(a[0]*a[0]+ a[1]*a[1]+ a[2]*a[2]) * sqrt(b[0]*b[0]+ b[1]*b[1]+ b[2]*b[2]));
    return cos_ab;
}
/*
  create polygons between two edge
  */
void bmo_edge_face_connect(BMesh *bm, BridgeParams* bp, BMEdge *e1, BMEdge *e2, BMFace *f_example)
{
    int i;
    BMVert *vv[4];
    BMVert *v1, *v2, *v3, *v4;
    BMVert *vi1, *vi2, *vi3, *vi4; // input vertex
    BMFace *f;
    float vect1[3], vect2[3], vect3[3], vect4[3], normalA[3], normalB[3], normalVector[3];
    float vp1[3], vp2[3], vp3[3], vp4[3];

    vi1 = e1->v1;
    vi2 = e2->v1;
    vi3 = BM_edge_other_vert(e2,vi2);
    vi4 = BM_edge_other_vert(e1,vi1);
// calculate vector betwen input vertx
    sub_v3_v3v3(vect1, vi1->co, vi2->co);  // v1-v2
    sub_v3_v3v3(vect2, vi2->co, vi3->co);  // v2-v3
    sub_v3_v3v3(vect3, vi3->co, vi4->co);  // v3-v4
    sub_v3_v3v3(vect4, vi4->co, vi1->co);  // v4-v1

// cros product
    cross_v3_v3v3(vp1, vect1, vect2);
    cross_v3_v3v3(vp2, vect2, vect3);
    cross_v3_v3v3(vp3, vect3, vect4);
    cross_v3_v3v3(vp4, vect4, vect1);
// calculate normal vector
   mid_v3_v3v3(normalA, bp->centrod1, bp->centrod2);
   vv[0] = vi1;
   vv[1] = vi2;
   vv[2] = vi3;
   vv[3] = vi4;
   get_centroid(vv, 4, normalB);
   sub_v3_v3v3(normalVector, normalB, normalA);

    // check direction cros produc result
    if (!(get_cos_v3v3(vp1, vp2)>0 &&
        get_cos_v3v3(vp2, vp3)>0 &&
        get_cos_v3v3(vp3, vp4)>0 &&
        get_cos_v3v3(vp4, vp1)>0))
    {
        vi1 = e1->v1;
        vi2 = BM_edge_other_vert(e2, e2->v1);
        vi3 = BM_edge_other_vert(e2,vi2);
        vi4 = BM_edge_other_vert(e1,vi1);
    }
    // vi1  - v1 - v2 ... vi2
    // |      |    |       |
    // vi4 .. v4 - v3 ... vi3
   v1 = vi1;
   v2 = vi2;
   v3 = vi3;
   v4 = vi4;
    if (bp->seg > 1){
        for (i = 1; i < bp->seg; i++){
            if (bp->inter == LINEAR_INTER)
            {
                v2 = get_linear_seg (bm, bp, i, vi1, vi2);
                v3 = get_linear_seg(bm, bp,  i, vi4, vi3);
            }
            if (bp->inter == CUBIC_INTER)
            {
                v2 = get_cubic_seg (bm, bp, i, vi1, vi2);
                v3 = get_cubic_seg(bm, bp,  i, vi4, vi3);
            }
           f =  BM_face_create_quad_tri(bm, v1, v2, v3, v4, f_example, TRUE);
           BM_face_normal_update(f);
           //check normal BM_face_normal_flip(bm, f);
           if (get_cos_v3v3(f->no, normalVector)<0)
               BM_face_normal_flip(bm, f);
           v1 = v2;
           v4 = v3;
        }
       f =  BM_face_create_quad_tri(bm, v1, vi2, vi3, v4, f_example, TRUE);
       BM_face_normal_update(f);
       if (get_cos_v3v3(f->no, normalVector)<0)
           BM_face_normal_flip(bm, f);
    }
    else {
        f = BM_face_create_quad_tri(bm, v1, v2, v3, v4, f_example, TRUE);
        BM_face_normal_update(f);
        if (get_cos_v3v3(f->no, normalVector)<0)
            BM_face_normal_flip(bm, f);
    }
}
/*
This function create polygons between edge and vert
*/
void bmo_edge_vert_connect(BMesh *bm, BridgeParams *bp, BMEdge *e, BMVert *v, BMFace *f_example)
{
    BMVert *v1, *v2, *v3, *v4, *vv[3];
    BMFace *f;
    float normalA[3], normalB[3], normalVector[3];
    int i;
    v1 = e->v1;
    v2 = v;
    v3 = NULL;
    v4 = BM_edge_other_vert(e, e->v1);

    // calculate normal vector
       mid_v3_v3v3(normalA, bp->centrod1, bp->centrod2);
       vv[0] = v1;
       vv[1] = v2;
       vv[2] = v4;
       get_centroid(vv, 3, normalB);
       sub_v3_v3v3(normalVector, normalB, normalA);

    if (bp->seg > 1){
        for (i = 1; i < bp->seg; i++){
            if (bp->inter == LINEAR_INTER)
            {
                v2 = get_linear_seg(bm, bp, i, v, e->v1);
                v3 = get_linear_seg(bm, bp, i, v, BM_edge_other_vert(e, e->v1));
            }
            if (bp->inter == CUBIC_INTER)
            {
                v2 = get_cubic_seg (bm, bp, i, v,  e->v1);
                v3 = get_cubic_seg (bm, bp, i, v, BM_edge_other_vert(e, e->v1));
            }
            if (i == 1)
                f = BM_face_create_quad_tri(bm, v2, v, v3, NULL, f_example, TRUE);
            else
                f = BM_face_create_quad_tri(bm, v1, v2, v3, v4, f_example, TRUE);

            BM_face_normal_update(f);
            if (get_cos_v3v3(f->no, normalVector)<0)
                BM_face_normal_flip(bm, f);
            v1 = v2;
            v4 = v3;
        }
        f = BM_face_create_quad_tri(bm, e->v1, v2, v3, BM_edge_other_vert(e, e->v1), f_example, TRUE);
        BM_face_normal_update(f);
        if (get_cos_v3v3(f->no, normalVector)<0)
            BM_face_normal_flip(bm, f);

    }
    else{
        f = BM_face_create_quad_tri(bm, e->v1, BM_edge_other_vert(e, e->v1), v, NULL, f_example, TRUE);
        BM_face_normal_update(f);
        if (get_cos_v3v3(f->no, normalVector)<0)
            BM_face_normal_flip(bm, f);
    }
}

void  bmo_bridge_loops_exec(BMesh *bm, BMOperator *op)
{
    BMEdge *e;
    BMOIter siter;
    BMIter iter;
    BMEdge **skip_edge = NULL, **all_edge = NULL, **ee1=NULL, **ee2=NULL;
    BMVert **vv1 = NULL, **vv2 = NULL;
    BridgeParams bp;

    int i = 0, j = 0, loops_count = 0, cl1 = 0, cl2 = 0;
    BMFace *f;

    BLI_array_declare(skip_edge);
    BLI_array_declare(all_edge);
    BLI_array_declare(ee1);
    BLI_array_declare(ee2);
    BLI_array_declare(vv1);
    BLI_array_declare(vv2);

    // init bridge param
    bp.newVertices.first = bp.newVertices.last = NULL;
    bp.seg = BMO_slot_int_get(op, "segmentation");
    if (BMO_slot_int_get(op,"interpolation"))
        bp.inter = CUBIC_INTER;
    else
        bp.inter = LINEAR_INTER;
    bp.strenght = BMO_slot_float_get(op, "strenght");

    // find all edges in faces
    BMO_ITER (f, &siter, bm, op, "edgefacein", BM_FACE)
    {
       BMLoop* loop;
       loop = f->l_first;
       do
       {
           BLI_array_append(all_edge, loop->e);
           loop = loop->next;
       }
       while (loop != f->l_first);
    }
    // count entry edges
    for (i = 0; i< BLI_array_count(all_edge); i++)
    {
        int counter = 0;
        for (j = 0; j< BLI_array_count(all_edge); j++)
        {
            if ((all_edge[i] == all_edge[j]) && (i != j))
                counter ++;
        }
        if ((counter > 0) &&
            (bmo_bridge_array_entry_elem(skip_edge,
                                         BLI_array_count(skip_edge),
                                         all_edge[i]) == 0 ))
        {
            BLI_array_append(skip_edge, all_edge[i]);
            BMO_elem_flag_enable(bm, all_edge[i], EDGE_DONE); // skiped adjacent faces edge

        }
    }
    //TODO develop non connected case
    // create loop according with skiped edges
    BMO_slot_buffer_flag_enable(bm, op, "edgefacein", BM_EDGE, EDGE_MARK);
    BMO_ITER (e, &siter, bm, op, "edgefacein", BM_EDGE)
    {
        if (!BMO_elem_flag_test(bm, e, EDGE_DONE))
        {
        BMVert *v, *ov;
        BMEdge *e2, *e3;

        if (loops_count > 2)
        {
            BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Select only two edge loops");
            goto cleanup_2;
        }
            v = e->v1;
            e2 = e;
            ov = v;
            do
            {
                if (loops_count == 0)
                {
                    BLI_array_append(ee1, e2);
                    BLI_array_append(vv1, v);
                }
                else
                {
                    BLI_array_append(ee2, e2);
                    BLI_array_append(vv2, v);
                }

                BMO_elem_flag_enable(bm, e2, EDGE_DONE);

                v = BM_edge_other_vert(e2, v);
                BM_ITER_ELEM (e3, &iter, v, BM_EDGES_OF_VERT)
                {
                    if ((e3 != e2)
                        && (BMO_elem_flag_test(bm, e3, EDGE_MARK))
                        && (!BMO_elem_flag_test(bm, e3, EDGE_DONE)))
                    {
                        break;
                    }
                }
                if (e3)
                    e2 = e3;
            }
            while (e3 && e2 != e);
            /* test for connected loops, and set cl1 or cl2 if so */
            if (v == ov) {
                if (loops_count == 0) {
                    cl1 = 1;
                }
                else {
                    cl2 = 1;
                }
            }
            loops_count ++;
        }
    }
// -----------------------------------------------
// ---------------CREATE BRIDGE-------------------
// -----------------------------------------------
    if (ee1 && ee2)
    {
       float min = 1e32;
       int min_j;
       float center1[3], center2[3];
       BMEdge **non_conected = NULL, **conected = NULL;
       BLI_array_declare(non_conected);
       BLI_array_declare(conected);

       if(BLI_array_count(ee1) > BLI_array_count(ee2))
       {
           ARRAY_SWAP(BMVert *, vv1, vv2);
           ARRAY_SWAP(BMEdge *, ee1, ee2);
       }

       get_centroid(vv1, BLI_array_count(vv1), center1);
       get_centroid(vv2, BLI_array_count(vv2), center2);

       copy_v3_v3(bp.centrod1, center1);
       copy_v3_v3(bp.centrod2, center2);

       for (i = 0; i < BLI_array_count(ee1); i++)
            BMO_elem_flag_enable(bm, ee1[i], EDGE_NON_CONNECTED);
       for (i = 0; i < BLI_array_count(ee2); i++)
            BMO_elem_flag_enable(bm, ee2[i], EDGE_NON_CONNECTED);

       for (i = 0; i < BLI_array_count(ee1); i++)
       {
           float mid_V_i[3], mid_V_j[3]/*, centerV[3]*/;
           float co1[3], angel;

           get_middle_edge(ee1[i], mid_V_i);
           sub_v3_v3v3(co1, mid_V_i, bp.centrod1);
         /*  sub_v3_v3v3(centerV, bp.centrod2, bp.centrod1);
           angel = angle_v3v3(centerV, co1);
           mul_v3_fl(co1, cos(angel - M_PI_2));*/
           min = 1e32;
           min_j =  -1;
           for (j = 0; j < BLI_array_count(ee2); j++)
           {
               float co2[3], summ[3]; // new coordinate with center of loops
               if (BMO_elem_flag_test(bm, ee2[j], EDGE_NON_CONNECTED))
               {
                   get_middle_edge(ee2[j],mid_V_j);
                   sub_v3_v3v3(co2, mid_V_j, bp.centrod2);

                   //---------------------------
                   /*
                   sub_v3_v3v3(centerV, bp.centrod1, bp.centrod2);
                   angel = angle_v3v3(centerV, co2);
                   mul_v3_fl(co2, cos(angel-M_PI_2));
                   */
                   //-----------------
                   //sub_v3_v3v3(co1, mid_V_i, bp.centrod1);
                   //sub_v3_v3v3(co2, mid_V_j, bp.centrod2);
                   //---------------------------------
                   //sub_v3_v3v3(co1, co1, bp.centrod1);
                   //sub_v3_v3v3(co2, co2, bp.centrod2);
                   //---------------------------------
                   angel = angle_v3v3(co1, co2);
                   add_v3_v3v3(summ, co1, co2);
                   //angel = angle_v3v3(co1, summ);
                   if (len_v3v3(co1, co2) < min)
                   {
                       min = len_v3v3(co1, co2);
                       min_j = j;
                   }
               }
           }
           if (min_j != -1){
               bmo_edge_face_connect(bm, &bp, ee1[i], ee2[min_j], NULL);
               BLI_array_append(conected, ee2[min_j]);
               BMO_elem_flag_enable(bm, ee2[min_j], EDGE_CONNECTED);
           }
       }


        // create triangle
       for (i = 0; i < BLI_array_count(ee2); i++)
       {
           int flag = 0;
           for (j = 0; j < BLI_array_count(conected); j++)
           {
               if (ee2[i] == conected[j])
                   flag = 1;
            }
           if (flag  == 0)
               BLI_array_append(non_conected, ee2[i]);

        }

       // найти ближающую точку в vv1; и приконектить...
        for (i = 0; i< BLI_array_count(non_conected); i++)
        {
            //if (BMO_elem_flag_test(bm, ee2[i], EDGE_CONNECTED))
            //{
                float  mid[3];
                float co1[3], co2[3]; // new coordinate with center of loops

                get_middle_edge(non_conected[i], mid);
                sub_v3_v3v3(co1, mid, center2);
                min = 1e32;
                min_j = 0;
                for (j = 0; j < BLI_array_count(vv1); j++) {
                    sub_v3_v3v3(co2, vv1[j]->co, center1);
                    if (len_v3v3(co1, co2) < min)
                    {
                        min = len_v3v3(co1, co2);
                        min_j = j;
                    }
                }
                bmo_edge_vert_connect(bm, &bp,  non_conected[i], vv1[min_j], NULL);
            //}
        }
        BLI_array_free(non_conected);
        BLI_array_free(conected);
    }

// -----kill input data-------------------------
    BMO_ITER (f, &siter, bm, op, "edgefacein", BM_FACE)
    {
        BM_face_kill(bm,f);
    }
    for (i=0; i< BLI_array_count(skip_edge); i++){
        BM_edge_kill(bm, skip_edge[i]);
    }

cleanup_2:
    BLI_array_free(ee1);
    BLI_array_free(ee2);
    BLI_array_free(vv1);
    BLI_array_free(vv2);
    BLI_array_free(skip_edge);
    BLI_array_free(all_edge);
    BLI_freelistN(&bp.newVertices);

}


void bmo_bridge_loops_exec_old(BMesh *bm, BMOperator *op)
{
	BMEdge **ee1 = NULL, **ee2 = NULL;
	BMVert **vv1 = NULL, **vv2 = NULL;
	BLI_array_declare(ee1);
	BLI_array_declare(ee2);
	BLI_array_declare(vv1);
	BLI_array_declare(vv2);
	BMOIter siter;
	BMIter iter;
	BMEdge *e, *nexte;
	int c = 0, cl1 = 0, cl2 = 0;

	BMO_slot_buffer_flag_enable(bm, op, "edges", BM_EDGE, EDGE_MARK);

	BMO_ITER (e, &siter, bm, op, "edges", BM_EDGE) {
		if (!BMO_elem_flag_test(bm, e, EDGE_DONE)) {
			BMVert *v, *ov;
			/* BMEdge *e2, *e3, *oe = e; */ /* UNUSED */
			BMEdge *e2, *e3;
			
			if (c > 2) {
				BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Select only two edge loops");
				goto cleanup;
			}
			
			e2 = e;
			v = e->v1;
			do {
				v = BM_edge_other_vert(e2, v);
				nexte = NULL;
				BM_ITER_ELEM (e3, &iter, v, BM_EDGES_OF_VERT) {
					if (e3 != e2 && BMO_elem_flag_test(bm, e3, EDGE_MARK)) {
						if (nexte == NULL) {
							nexte = e3;
						}
						else {
							/* edges do not form a loop: there is a disk
							 * with more than two marked edges. */
							BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
							                "Selection must only contain edges from two edge loops");
							goto cleanup;
						}
					}
				}
				
				if (nexte)
					e2 = nexte;
			} while (nexte && e2 != e);
			
			if (!e2)
				e2 = e;

			e = e2;
			ov = v;
			do {
				if (c == 0) {
					BLI_array_append(ee1, e2);
					BLI_array_append(vv1, v);
				}
				else {
					BLI_array_append(ee2, e2);
					BLI_array_append(vv2, v);
				}
				
				BMO_elem_flag_enable(bm, e2, EDGE_DONE);
				
				v = BM_edge_other_vert(e2, v);
				BM_ITER_ELEM (e3, &iter, v, BM_EDGES_OF_VERT) {
					if (e3 != e2 && BMO_elem_flag_test(bm, e3, EDGE_MARK) && !BMO_elem_flag_test(bm, e3, EDGE_DONE)) {
						break;
					}
				}
				if (e3)
					e2 = e3;
			} while (e3 && e2 != e);
			
			if (v && !e3) {
				if (c == 0) {
					if (BLI_array_count(vv1) && v == vv1[BLI_array_count(vv1) - 1]) {
						printf("%s: internal state waning *TODO DESCRIPTION!*\n", __func__);
					}
					BLI_array_append(vv1, v);
				}
				else {
					BLI_array_append(vv2, v);
				}
			}
			
			/* test for connected loops, and set cl1 or cl2 if so */
			if (v == ov) {
				if (c == 0) {
					cl1 = 1;
				}
				else {
					cl2 = 1;
				}
			}
			
			c++;
		}
	}

	if (ee1 && ee2) {
		int i, j;
		BMVert *v1, *v2, *v3, *v4;
		int starti = 0, dir1 = 1, wdir = 0, lenv1, lenv2;

		/* Simplify code below by avoiding the (!cl1 && cl2) case */
		if (!cl1 && cl2) {
			SWAP(int, cl1, cl2);
			ARRAY_SWAP(BMVert *, vv1, vv2);
			ARRAY_SWAP(BMEdge *, ee1, ee2);
		}

		lenv1 = lenv2 = BLI_array_count(vv1);

		/* Below code assumes vv1/vv2 each have at least two verts. should always be
		 * a safe assumption, since ee1/ee2 are non-empty and an edge has two verts. */
		BLI_assert((lenv1 > 1) && (lenv2 > 1));

		/* BMESH_TODO: Would be nice to handle cases where the edge loops
		 * have different edge counts by generating triangles & quads for
		 * the bridge instead of quads only. */
		if (BLI_array_count(ee1) != BLI_array_count(ee2)) {
			BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
			                "Selected loops must have equal edge counts");
			goto cleanup;
		}

		j = 0;
		if (vv1[0] == vv1[lenv1 - 1]) {
			lenv1--;
		}
		if (vv2[0] == vv2[lenv2 - 1]) {
			lenv2--;
		}

		/* Find starting point and winding direction for two unclosed loops */
		if (!cl1 && !cl2) {
			/* First point of loop 1 */
			v1 = get_outer_vert(bm, ee1[0]);
			/* Last point of loop 1 */
			v2 = get_outer_vert(bm, ee1[clamp_index(-1, BLI_array_count(ee1))]);
			/* First point of loop 2 */
			v3 = get_outer_vert(bm, ee2[0]);
			/* Last point of loop 2 */
			v4 = get_outer_vert(bm, ee2[clamp_index(-1, BLI_array_count(ee2))]);

			/* If v1 is a better match for v4 than v3, AND v2 is a better match
			 * for v3 than v4, the loops are in opposite directions, so reverse
			 * the order of reads from vv1. We can avoid sqrt for comparison */
			if (len_squared_v3v3(v1->co, v3->co) > len_squared_v3v3(v1->co, v4->co) &&
			    len_squared_v3v3(v2->co, v4->co) > len_squared_v3v3(v2->co, v3->co))
			{
				dir1 = -1;
				starti = clamp_index(-1, lenv1);
			}
		}

		/* Find the shortest distance from a vert in vv1 to vv2[0]. Use that
		 * vertex in vv1 as a starting point in the first loop, while starting
		 * from vv2[0] in the second loop. This is a simplistic attempt to get
		 * a better edge-to-edge match between the two loops. */
		if (cl1) {
			int previ, nexti;
			float min = 1e32;

			/* BMESH_TODO: Would be nice to do a more thorough analysis of all
			 * the vertices in both loops to find a more accurate match for the
			 * starting point and winding direction of the bridge generation. */
			
			for (i = 0; i < BLI_array_count(vv1); i++) {
				if (len_v3v3(vv1[i]->co, vv2[0]->co) < min) {
					min = len_v3v3(vv1[i]->co, vv2[0]->co);
					starti = i;
				}
			}

			/* Reverse iteration order for the first loop if the distance of
			 * the (starti - 1) vert from vv1 is a better match for vv2[1] than
			 * the (starti + 1) vert.
			 *
			 * This is not always going to be right, but it will work better in
			 * the average case.
			 */
			previ = clamp_index(starti - 1, lenv1);
			nexti = clamp_index(starti + 1, lenv1);

			/* avoid sqrt for comparison */
			if (len_squared_v3v3(vv1[nexti]->co, vv2[1]->co) > len_squared_v3v3(vv1[previ]->co, vv2[1]->co)) {
				/* reverse direction for reading vv1 (1 is forward, -1 is backward) */
				dir1 = -1;
			}
		}

		/* Vert rough attempt to determine proper winding for the bridge quads:
		 * just uses the first loop it finds for any of the edges of ee2 or ee1 */
		if (wdir == 0) {
			for (i = 0; i < BLI_array_count(ee2); i++) {
				if (ee2[i]->l) {
					wdir = (ee2[i]->l->v == vv2[i]) ? (-1) : (1);
					break;
				}
			}
		}
		if (wdir == 0) {
			for (i = 0; i < BLI_array_count(ee1); i++) {
				j = clamp_index((i * dir1) + starti, BLI_array_count(ee1));
				if (ee1[j]->l && ee2[j]->l) {
					wdir = (ee2[j]->l->v == vv2[j]) ? (1) : (-1);
					break;
				}
			}
		}
		
		/* Generate the bridge quads */
		for (i = 0; i < BLI_array_count(ee1) && i < BLI_array_count(ee2); i++) {
			BMFace *f;

			BMLoop *l_1 = NULL;
			BMLoop *l_2 = NULL;
			BMLoop *l_1_next = NULL;
			BMLoop *l_2_next = NULL;
			BMLoop *l_iter;
			BMFace *f_example;

			int i1, i1next, i2, i2next;

			i1 = clamp_index(i * dir1 + starti, lenv1);
			i1next = clamp_index((i + 1) * dir1 + starti, lenv1);
			i2 = i;
			i2next = clamp_index(i + 1, lenv2);

			if (vv1[i1] == vv1[i1next]) {
				continue;
			}

			if (wdir < 0) {
				SWAP(int, i1, i1next);
				SWAP(int, i2, i2next);
			}

			/* get loop data - before making the face */
			bm_vert_loop_pair(bm, vv1[i1], vv2[i2], &l_1, &l_2);
			bm_vert_loop_pair(bm, vv1[i1next], vv2[i2next], &l_1_next, &l_2_next);
			/* copy if loop data if its is missing on one ring */
			if (l_1 && l_1_next == NULL) l_1_next = l_1;
			if (l_1_next && l_1 == NULL) l_1 = l_1_next;
			if (l_2 && l_2_next == NULL) l_2_next = l_2;
			if (l_2_next && l_2 == NULL) l_2 = l_2_next;
			f_example = l_1 ? l_1->f : (l_2 ? l_2->f : NULL);

			f = BM_face_create_quad_tri(bm,
			                            vv1[i1],
			                            vv2[i2],
			                            vv2[i2next],
			                            vv1[i1next],
			                            f_example, TRUE);
			if (!f || f->len != 4) {
				fprintf(stderr, "%s: in bridge! (bmesh internal error)\n", __func__);
			}
			else {
				l_iter = BM_FACE_FIRST_LOOP(f);

				if (l_1)      BM_elem_attrs_copy(bm, bm, l_1,      l_iter); l_iter = l_iter->next;
				if (l_2)      BM_elem_attrs_copy(bm, bm, l_2,      l_iter); l_iter = l_iter->next;
				if (l_2_next) BM_elem_attrs_copy(bm, bm, l_2_next, l_iter); l_iter = l_iter->next;
				if (l_1_next) BM_elem_attrs_copy(bm, bm, l_1_next, l_iter);
			}
		}
	}

cleanup:
	BLI_array_free(ee1);
	BLI_array_free(ee2);
	BLI_array_free(vv1);
	BLI_array_free(vv2);
}
