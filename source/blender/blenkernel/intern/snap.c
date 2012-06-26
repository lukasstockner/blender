#include "BKE_snap.h"

#include "BKE_mesh.h"
#include "BKE_tessmesh.h"
#include "BKE_DerivedMesh.h"

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "ED_view3d.h"

#include "ED_mesh.h"

#include "BLI_math_vector.h"

#include <stdio.h>

#include <stdlib.h>
#include <math.h>
#include <float.h>

/* -- PROTOTYPES -- */
typedef struct MeshData MeshData;
void Snap_draw_default(Snap *s);
void SnapMesh_run(Snap *sm);
void SnapMesh_free(Snap* sm);


struct Snap{
	int snap_found; //boolean whether snap point has been found
	SnapPoint snap_point;

	//TODO: all extra data in Snap should be subject to streamlining and change
	//in the near future, as soon as I get something working. A lot of quick
	//not-so-well-thought-out bits of code here that will need more thought later.

	int retval; //temporary variable for linking in with transform code

	/*external data pointers*/
	Scene* scene;
	Object* ob;
	View3D* v3d;
	ARegion* ar;
	int mval[2];

	/*internal calculation data*/
	float ray_start[3], ray_normal[3];
	float ray_start_local[3], ray_normal_local[3];
	int min_distance;
	int r_dist;

	//matrix storage
	float imat[4][4];
	float timat[3][3];


	//snap type -- type of snap to calculate
	//geom_data_type

	//transform

	/*subclass data struct*/
	void* snap_data;

	/*function pointers*/
	void (*run)(Snap*);
	void (*draw)(Snap*);
	void (*free)(Snap*);
	//void callback for doing transform -- function pointer
};

Snap* Snap_create(Scene *scene, Object* ob, View3D *v3d, ARegion *ar, int mval[2]){
	Snap* s = (Snap*)MEM_mallocN(sizeof(Snap), "snap");

	s->scene = scene;
	s->ob = ob;
	s->v3d = v3d;
	s->ar = ar;
	copy_v2_v2_int(s->mval, mval);

	s->min_distance = 30; //TODO: change to a user defined value;

	//TODO: pass the function pointers in as arguments
	s->snap_found = 0;
	s->run = NULL;
	s->draw = NULL;
	s->snap_data = NULL;
	return s;
}

void Snap_setmval(Snap* s, int mval[2]){
	copy_v2_v2_int(s->mval, mval);
}

int Snap_getretval(Snap* s){
	return s->retval;
}

SnapPoint* Snap_getSnapPoint(Snap* s){
	return &(s->snap_point);
}

void Snap_calc_rays(Snap* s){
	ED_view3d_win_to_ray(s->ar, s->v3d, s->mval, s->ray_start, s->ray_normal);
}

void Snap_calc_matrix(Snap* s){
	invert_m4_m4(s->imat, s->ob->obmat);

	copy_m3_m4(s->timat, s->imat);
	transpose_m3(s->timat);

	copy_v3_v3(s->ray_start_local, s->ray_start);
	copy_v3_v3(s->ray_normal_local, s->ray_normal);

	//for edit mode only? should be placed in calc_rays?
	mul_m4_v3(s->imat, s->ray_start_local);
	mul_mat3_m4_v3(s->imat, s->ray_normal_local);
}

void Snap_free(Snap* s){
	s->free(s);
}

void Snap_free_f(Snap* s){
	MEM_freeN(s);
}

void Snap_run(Snap* s){
	s->run(s);
}

void Snap_draw(Snap* s){
	s->draw(s);
}

void Snap_draw_default(Snap *s){

}



/* -- MESH SNAPPING -- */

typedef struct {
	SnapMesh_type sm_type;// -- type of snap to calculate -- can be combination of several

	SnapMesh_data_type data_type; //the type of data stored in mesh_data pointer.
	MeshData *mesh_data;

	int *dm_index_array; //for use when using derivedmesh data type;

	//function mesh iterator -- hmm actually let's just make this SnapMesh_mesh_itorator()

}SnapMesh_data;

struct MeshData{
	void* data;
	int (*getNumVerts)(MeshData* md);
	int (*getNumEdges)(MeshData* md);
	int (*getNumFaces)(MeshData* md);

	void (*index_init)(MeshData* md, SnapMesh_data_array_type array_type);

	void (*getVert)(MeshData* md, int index, MVert* mv);
	void (*getEdgeVerts)(MeshData* md, int index, MVert* mv1, MVert* mv2);

	int (*checkVert)(MeshData* md, int index);
	int (*checkEdge)(MeshData* md, int index);

};

/* Mesh Data Functions */

void copy_mvert(MVert* out, MVert* in){
	copy_v3_v3(out->co, in->co);
	copy_v3_v3_short(out->no, in->no);
	out->flag = in->flag;
	out->bweight = in->bweight;
}

//copied from editderivedmesh.c
void bmvert_to_mvert(BMesh *bm, BMVert *bmv, MVert *mv)
{
	copy_v3_v3(mv->co, bmv->co);
	normal_float_to_short_v3(mv->no, bmv->no);

	mv->flag = BM_vert_flag_to_mflag(bmv);

	if (CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
		mv->bweight = (unsigned char) (BM_elem_float_data_get(&bm->vdata, bmv, CD_BWEIGHT) * 255.0f);
	}
}

void bmedge_to_medge(BMesh *bm, BMEdge *ee, MEdge *edge_r)
{

}

/* - MeshData BMEditMesh accessor implementation - */

void MeshData_BMEditMesh_index_init(MeshData* md, SnapMesh_data_array_type array_type){
	BMEditMesh* em;
	switch(array_type){
	case SNAPMESH_DAT_vert:
		em = (BMEditMesh*)md->data;
		EDBM_index_arrays_init(em, 1, 0, 0);
		break;
	case SNAPMESH_DAT_edge:
		em = (BMEditMesh*)md->data;
		EDBM_index_arrays_init(em, 0, 1, 0);
		break;
	case SNAPMESH_DAT_face:
		em = (BMEditMesh*)md->data;
		EDBM_index_arrays_init(em, 0, 0, 1);
		break;
	}
}

int MeshData_BMEditMesh_getNumVerts(MeshData* md){
	BMEditMesh* em = (BMEditMesh*)md->data;
	return em->bm->totvert;
}

int MeshData_BMEditMesh_getNumEdges(MeshData* md){
	BMEditMesh* em = (BMEditMesh*)md->data;
	return em->bm->totedge;
}

int MeshData_BMEditMesh_getNumFaces(MeshData *md){
	BMEditMesh* em = (BMEditMesh*)md->data;
	return em->bm->totface;
}

/* I've chosen to have MVert and MEdge as the format in use in snapping because it's
  much easier to convert from BMVert to MVert than the reverse*/
//should be inline?
void MeshData_BMEditMesh_getVert(MeshData *md, int index, MVert* mv){
	BMVert* bmv;
	BMEditMesh* em = (BMEditMesh*)md->data;
	bmv = EDBM_vert_at_index(em, index);
	bmvert_to_mvert(em->bm, bmv, mv);
}

//if vert is not hidden and not selected return 1 else 0
int MeshData_BMEditMesh_checkVert(MeshData *md, int index){
	BMVert* bmv;
	BMEditMesh* em = (BMEditMesh*)md->data;
	bmv = EDBM_vert_at_index(em, index);
	if(BM_elem_flag_test(bmv, BM_ELEM_HIDDEN) || BM_elem_flag_test(bmv, BM_ELEM_SELECT)){
		return 0;
	}
	return 1;
}

void MeshData_BMEditMesh_getEdgeVerts(MeshData *md, int index, MVert* mv1, MVert* mv2){
	BMEdge* bme;
	BMEditMesh* em = (BMEditMesh*)md->data;
	bme = EDBM_edge_at_index(em, index);
	bmvert_to_mvert(em->bm, bme->v1, mv1);
	bmvert_to_mvert(em->bm, bme->v2, mv2);
}


//if vert is not hidden and not selected return 1 else 0
int MeshData_BMEditMesh_checkEdge(MeshData *md, int index){
	BMEdge* bme;
	BMEditMesh* em = (BMEditMesh*)md->data;
	bme = EDBM_edge_at_index(em, index);
	/* check whether edge is hidden and if either of its vertices are selected*/
	if( BM_elem_flag_test(bme, BM_ELEM_HIDDEN) ||
		BM_elem_flag_test(bme->v1, BM_ELEM_SELECT ||
		BM_elem_flag_test(bme->v2, BM_ELEM_SELECT))){
		return 0;
	}
	return 1;
}

void MeshData_BMEditMesh_getFace(MeshData *md, int index, MVert* mv){

}



/* - MeshData DerivedMesh accessor implementation - */

void MeshData_DerivedMesh_index_init(MeshData* md, SnapMesh_data_array_type array_type){
//	DerivedMesh* dm;
//	dm = (DerivedMesh*)md->data;
//	sm_data->dm_index_array = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	//TODO: check why this code would be needed, and why it is currently commented out.
}

int MeshData_DerivedMesh_getNumVerts(MeshData* md){
	DerivedMesh* dm = (DerivedMesh*)md->data;
	return dm->getNumVerts(dm);
}

int MeshData_DerivedMesh_getNumEdges(MeshData* md){
	DerivedMesh* dm = (DerivedMesh*)md->data;
	return dm->getNumEdges(dm);
}

int MeshData_DerivedMesh_getNumFaces(MeshData *md){
	DerivedMesh* dm = (DerivedMesh*)md->data;
	return dm->getNumPolys(dm);
}

int MeshData_DerivedMesh_checkVert(MeshData *md, int index){
	MVert *verts;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	verts = dm->getVertArray(dm);
	if((verts[index].flag & 0) || (verts[index].flag & ME_HIDE)){
		return 0;
	}
	return 1;
}

void MeshData_DerivedMesh_getVert(MeshData *md, int index, MVert* mv){
	MVert *verts;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	verts = dm->getVertArray(dm);
	copy_mvert(mv, &(verts[index])); //TODO: not sure whether it's better to pass by reference or value here...
}

void MeshData_DerivedMesh_getEdgeVerts(MeshData *md, int index, MVert* mv1, MVert* mv2){
	MEdge* edges;
	MVert* verts;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	edges = dm->getEdgeArray(dm);
	verts = dm->getVertArray(dm);
	/* v1 and v2 are integer indexes for the verts array */
	copy_mvert(mv1, &(verts[edges[index].v1]));
	copy_mvert(mv2, &(verts[edges[index].v2]));
}

int MeshData_DerivedMesh_checkEdge(MeshData *md, int index){
	MEdge* edges;
	MVert* verts;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	edges = dm->getEdgeArray(dm);
	verts = dm->getVertArray(dm);
	/* check whether edge is hidden and if either of its vertices (v1 and v2) are selected
	When they are selected their flag will be 0? (if indeed it works similar to bmesh)*/
	/* in this case v1 and v2 are integer indexes for the vertex array*/
	if((verts[edges[index].v1].flag & 0) ||
		(verts[edges[index].v2].flag & 0) ||
		(edges[index].flag & ME_HIDE)){
		return 0;
	}
	return 1;
}

void MeshData_DerivedMesh_getFace(MeshData *md, int index, MVert* mv){

}


MeshData* MeshData_create(void* mesh_data, SnapMesh_data_type data_type){
	MeshData* md = (MeshData*)MEM_mallocN(sizeof(MeshData), "snapmesh_meshdata");
	md->data = mesh_data;
	switch(data_type){
	case SNAPMESH_DATA_TYPE_DerivedMesh:
		md->getNumVerts = MeshData_DerivedMesh_getNumVerts;
		md->getNumEdges = MeshData_DerivedMesh_getNumEdges;
		md->getNumFaces = MeshData_DerivedMesh_getNumFaces;
		md->index_init = MeshData_DerivedMesh_index_init;
		md->checkVert = MeshData_DerivedMesh_checkVert;
		md->checkEdge = MeshData_DerivedMesh_checkEdge;
		md->getVert = MeshData_DerivedMesh_getVert;
		md->getEdgeVerts = MeshData_DerivedMesh_getEdgeVerts;
		break;
	case SNAPMESH_DATA_TYPE_BMEditMesh:
		md->getNumVerts = MeshData_BMEditMesh_getNumVerts;
		md->getNumEdges = MeshData_BMEditMesh_getNumEdges;
		md->getNumFaces = MeshData_BMEditMesh_getNumFaces;
		md->index_init = MeshData_BMEditMesh_index_init;
		md->checkVert = MeshData_BMEditMesh_checkVert;
		md->checkEdge = MeshData_BMEditMesh_checkEdge;
		md->getVert = MeshData_BMEditMesh_getVert;
		md->getEdgeVerts = MeshData_BMEditMesh_getEdgeVerts;
		break;
	}
	return md;
}



/* SnapMesh functions */

Snap* SnapMesh_create(	void* mesh_data,
						SnapMesh_data_type data_type,
						SnapMesh_type sm_type,
						Scene *scene, Object *ob, View3D *v3d, ARegion *ar, int mval[2]){
	Snap* sm = Snap_create(scene, ob, v3d, ar, mval);
	SnapMesh_data* sm_data;

	sm->run = SnapMesh_run; //not sure whether to split into the seperate SnapMesh types here or not
	//leaving generic SnapMesh_run for now.
	sm->draw = Snap_draw_default; //default drawing function. should put an optional over-ride here.
	sm->free = SnapMesh_free;


	sm_data = (SnapMesh_data*)MEM_mallocN(sizeof(SnapMesh_data), "snapmesh_data");

	sm_data->sm_type = sm_type;

	//put an early exit flag somewhere here for the case when there is no geometry in mesh_data;
	sm_data->data_type = data_type;
	sm_data->mesh_data = MeshData_create(mesh_data, data_type);
	sm_data->dm_index_array = NULL;

	sm->snap_data = (void*)sm_data;

	return sm;
}

void SnapMesh_free(Snap* sm){
	SnapMesh_data *sm_data = sm->snap_data;
	MEM_freeN(sm_data->mesh_data);
	MEM_freeN(sm_data);
	Snap_free_f(sm);
}



/* SnapMesh Implementation */

/*TODO: ask Martin how this code works so I can comment it properly, and better understand it*/
void SnapMesh_snap_vertex(Snap* sm){
	/*There are a couple of bugs with my code for vertex snapping at the moment.
	  1. When using a DerivedMesh in edit mode it is snapping to points in mid air, in a grid like fashion.
	  2. When using a BMEditMesh or DerivedMesh in edit mode, with more than one mesh object in the scene,
		 and the mouse is close to a vertex in the currently edited object, there are some weird
		 false positions for the snap point location
	TODO: solve these bugs! might be wise to wait until the changes to transform are made before doing this, because
			they, especially the second bug are probably related to the transform side of things*/
	int i, totvert;
	SnapMesh_data* sm_data = (SnapMesh_data*)sm->snap_data;
	MeshData* md = sm_data->mesh_data;

	float dvec[3];
	float location[3];
	int new_dist;
	float new_depth;
	float r_depth;
	int screen_loc[2];
	MVert mv;

	//TODO: add a check for empty mesh - like in original snapping code.

	sm->retval = 0;

	sm->r_dist = sm->min_distance; //TODO: investigate, does this r_dist value need to be stored in sm class?
	r_depth = FLT_MAX;

	md->index_init(md, SNAPMESH_DAT_vert); //should perhaps only be called once per mesh...
	//was causing some segfault issues before with getVert.

	totvert  = md->getNumVerts(md);
	for(i=0;i<totvert;i++){
		if(md->checkVert(md, i) == 0){
			continue; //skip this vert if it is selected or hidden
		}

		md->getVert(md, i, &mv);
		sub_v3_v3v3(dvec, mv.co, sm->ray_start_local);

		if(dot_v3v3(sm->ray_normal_local, dvec)<=0){
			continue; //skip this vert if it is behind the view
		}

		copy_v3_v3(location, mv.co);
		mul_m4_v3(sm->ob->obmat, location);
		new_depth = len_v3v3(location, sm->ray_start);

		project_int(sm->ar, location, screen_loc);
		new_dist = abs(screen_loc[0] - sm->mval[0]) + abs(screen_loc[1] - sm->mval[1]);

		if(new_dist > sm->r_dist || new_depth >= r_depth){//what is r_depth in original code? is it always FLT_MAX?
			//I'm thinking this checks whether depth of snap point is the deeper than an existing point, if it is, then
			//skip this vert. it also checks whether the distance between the mouse and the vert is small enough, if it
			//isn't then skip this vert. it picks the closest one to the mouse, and the closest one to the screen.
			//if there is no existing snappable vert then r_depth == FLT_MAX;
			continue;
		}

		r_depth = new_depth;
		sm->retval = 1;
		copy_v3_v3(sm->snap_point.location, location);

		normal_short_to_float_v3(sm->snap_point.normal, mv.no);
		mul_m3_v3(sm->timat, sm->snap_point.normal);
		normalize_v3(sm->snap_point.normal);

		sm->r_dist = new_dist;
	}
}

void SnapMesh_snap_face(Snap* sm){

}

void SnapMesh_snap_face_plane(Snap* sm){

}

void SnapMesh_snap_edge(Snap* sm){
	int i, totedge;
	SnapMesh_data* sm_data = (SnapMesh_data*)sm->snap_data;
	MeshData* md = sm_data->mesh_data;

	md->index_init(md, SNAPMESH_DAT_edge); //should perhaps only be called once per mesh...

	totedge = md->getNumEdges(md);

}

void SnapMesh_snap_edge_midpoint(Snap* sm){

}

void SnapMesh_snap_edge_parallel(Snap* sm){

}

void SnapMesh_run(Snap *sm){
	SnapMesh_data* sm_data = (SnapMesh_data*)sm->snap_data;

	//will have to work out the best configuration for performance of these
	//initial function calls. perhaps they should go in the SnapMesh_create function
	//so they don't get called every run.
	Snap_calc_rays(sm);
	Snap_calc_matrix(sm);
	//add some checks here for early exits?

//	if (totface > 16) {
//		struct BoundBox *bb = BKE_object_boundbox_get(ob);
//		test = BKE_boundbox_ray_hit_check(bb, ray_start_local, ray_normal_local);
//	}

	switch(sm_data->sm_type){
	case SNAPMESH_TYPE_VERTEX:
		SnapMesh_snap_vertex(sm);
		break;
	case SNAPMESH_TYPE_FACE:
		return; //call face snap function here
	case SNAPMESH_TYPE_FACE_PLANE:
		return; //call face plane snap function here
	case SNAPMESH_TYPE_EDGE:
		return; //call edge snap function here
	case SNAPMESH_TYPE_EDGE_MIDPOINT:
		return; //call edge midpoint snap function here
	case SNAPMESH_TYPE_EDGE_PARALLEL:
		return; //call edge parallel snap function here

	}

	//call callback here?
}



/* -- AXIS SNAPPING -- */



/* -- BONE SNAPPING -- */
