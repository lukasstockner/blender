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
	SnapPoint* closest_point; //previously calculated closest point to mouse and screen.

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

	s->closest_point = NULL;
	s->snap_found = 0;

	//TODO: pass the function pointers in as arguments
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

//This function sets the closest previously calculated closest snap point. When the snap point calculation
//for this object is run, it will compare what it finds to this point. If the point is not
//closer to the mouse, and/or to the screen then it won't be counted as a valid snap point.
//TODO: perhaps a deep copy of snap point might be/more intuitive better here (in case original goes out of scope
//or is freed).
//will work out further into the design when I start working out the best way
//to optimise the snapping code.
void Snap_setClosestPoint(Snap* s, SnapPoint* sp){
	s->closest_point = sp;
}

SnapPoint* Snap_getSnapPoint(Snap* s){
	return &(s->snap_point);
}

void Snap_calc_rays(Snap* s){
	float mval_f[2];
	mval_f[0] = (float)(s->mval[0]);
	mval_f[1] = (float)(s->mval[1]);
	ED_view3d_win_to_ray(s->ar, s->v3d, mval_f, s->ray_start, s->ray_normal);
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
	int edit_mode;
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
		BM_elem_flag_test(bme->v1, BM_ELEM_SELECT) ||
		 BM_elem_flag_test(bme->v2, BM_ELEM_SELECT)){
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
	MVert *mv;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	char hidden, selected;
	verts = dm->getVertArray(dm);
	//TODO: remove these debugging variables mv and hidden
	mv = &(verts[index]);
	hidden =  (verts[index].flag & ME_HIDE);

	//there is a weird bug where the selected flag is always on for meshes which aren't in editmode.
	if(md->edit_mode){
		selected = (verts[index].flag & 1);
	}
	else{
		selected = 0;
	}

	if(hidden || selected){
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
	//TODO: optimise this code a bit, and remove debug variables.
	MEdge* edges;
	MVert* verts;
	MVert *v1, *v2;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	char hidden, v1_selected, v2_selected;
	edges = dm->getEdgeArray(dm);
	verts = dm->getVertArray(dm);
	/* check whether edge is hidden and if either of its vertices (v1 and v2) are selected
	When they are selected their flag will be 0? (if indeed it works similar to bmesh)*/
	/* in this case v1 and v2 are integer indexes for the vertex array*/

	v1 = &(verts[edges[index].v1]);
	v2 = &(verts[edges[index].v2]);
	if(md->edit_mode){
		v1_selected = v1->flag & 1;
		v2_selected = v2->flag & 1;
	}
	else{
		v1_selected = 0;
		v2_selected = 0;
	}

	if(v1_selected ||
		v2_selected ||
		(edges[index].flag & ME_HIDE)){
		return 0;
	}
	return 1;
}

void MeshData_DerivedMesh_getFace(MeshData *md, int index, MVert* mv){

}


MeshData* SnapMesh_createMeshData(Snap* sm, void* mesh_data, SnapMesh_data_type data_type){
	SnapMesh_data* sm_data = (SnapMesh_data*)sm->snap_data;
	MeshData* md = (MeshData*)MEM_mallocN(sizeof(MeshData), "snapmesh_meshdata");
	md->data = mesh_data;
	md->edit_mode = (sm->ob->mode & OB_MODE_EDIT);//TODO: this variable might need to be updated if this thing is cached...
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
	sm_data->mesh_data = SnapMesh_createMeshData(sm, mesh_data, data_type);
	sm_data->dm_index_array = NULL;

	sm->snap_data = (void*)sm_data;

	return sm;
}

void SnapMesh_free(Snap* sm){
	//TODO: there is some memory not getting freed somwhere in here...
	SnapMesh_data *sm_data = sm->snap_data;
	MEM_freeN(sm_data->mesh_data);
	MEM_freeN(sm_data);
	Snap_free_f(sm);
}



/* SnapMesh Implementation */

/*TODO: ask Martin how this code works so I can comment it properly, and better understand it*/
void SnapMesh_snap_vertex(Snap* sm){
	int i, totvert;
	SnapMesh_data* sm_data = (SnapMesh_data*)sm->snap_data;
	MeshData* md = sm_data->mesh_data;

	float dvec[3];
	float location[3];
	int new_dist;
	float new_depth;
	int screen_loc[2];
	MVert mv;

	//TODO: add a check for empty mesh - like in original snapping code.

	sm->retval = 0;

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

		if(new_dist > sm->snap_point.r_dist || new_depth >= sm->snap_point.r_depth){//what is r_depth in original code? is it always FLT_MAX?
			//I'm thinking this checks whether depth of snap point is the deeper than an existing point, if it is, then
			//skip this vert. it also checks whether the distance between the mouse and the vert is small enough, if it
			//isn't then skip this vert. it picks the closest one to the mouse, and the closest one to the screen.
			//if there is no existing snappable vert then r_depth == FLT_MAX;
			continue;
		}

		sm->snap_point.r_depth = new_depth;
		sm->retval = 1;

		copy_v3_v3(sm->snap_point.location, location);

		//printf("SnapPointInternal: %f, %f, %f\n", location[0], location[1], location[2]);

		normal_short_to_float_v3(sm->snap_point.normal, mv.no);
		mul_m3_v3(sm->timat, sm->snap_point.normal);
		normalize_v3(sm->snap_point.normal);

		sm->snap_point.r_dist = new_dist;
	}
}

void SnapMesh_snap_face(Snap* sm){

}

void SnapMesh_snap_face_plane(Snap* sm){

}

void SnapMesh_snap_edge(Snap* sm){
	int i, totedge, result, new_dist;
	int screen_loc[2];

	float intersect[3] = {0, 0, 0}, ray_end[3], dvec[3];
	float edge_loc[3], vec[3], location[3];
	float n1[3], n2[3];
	float mul, new_depth;

	SnapMesh_data* sm_data = (SnapMesh_data*)sm->snap_data;
	MeshData* md = sm_data->mesh_data;
	MVert v1, v2;

	copy_v3_v3(ray_end, sm->ray_normal_local);
	mul_v3_fl(ray_end, 2000); //TODO: what!?
	add_v3_v3v3(ray_end, sm->ray_start_local, ray_end);

	sm->retval = 0;

	md->index_init(md, SNAPMESH_DAT_edge); //should perhaps only be called once per mesh...

	totedge = md->getNumEdges(md);
	for(i=0;i<totedge;i++){
		if(md->checkEdge(md, i) == 0){
			continue;
		}

		md->getEdgeVerts(md, i, &v1, &v2);

		//why don't we care about result?
		result = isect_line_line_v3(v1.co, v2.co, sm->ray_start_local, ray_end, intersect, dvec); /* dvec used but we don't care about result */
		//intersect is closest point on line (beteween v1 and v2) to the line between ray_start_local and ray_end.
		if(!result){
			continue;
		}

		//dvec is calculated to be from mouse location to closest intersection point on line.
		sub_v3_v3v3(dvec, intersect, sm->ray_start_local);

		sub_v3_v3v3(edge_loc, v1.co, v2.co); //edge loc is vector representing line from v2 to v1.
		sub_v3_v3v3(vec, intersect, v2.co); //vec is vector from v2 to intersect

		//mul is value representing how large vec is compared to edge_loc.
		//if vec is the same length and in the same direction as edge_loc, then mul == 1
		mul = dot_v3v3(vec, edge_loc) / dot_v3v3(edge_loc, edge_loc);

		//this code constrains the intersect point on line to be lying on the edge_loc vector.
		//because mul will be greater than 1 when vec is longer and in the same direction as edge_loc
		//and mul will be less than 0 when it is going in the opposite direction to edge_loc.
		if (mul > 1) {
			mul = 1; //TODO: this code is probably useless, same with mul = 0 just below.
			copy_v3_v3(intersect, v1.co);
		}
		else if (mul < 0) {
			mul = 0;
			copy_v3_v3(intersect, v2.co);
		}

		//if intersect point along ray vector is not in front of the screen/camera then quit checking this line.
		//I would have thought this case would be eliminated by isect_line function in the first place.
		//TODO: explore this issue when coding parallel snapping.
		if (dot_v3v3(sm->ray_normal_local, dvec) <= 0){
			continue;
		}

		copy_v3_v3(location, intersect);

		//TODO: understand the matrix calculations here, and the init calculations for timat.
		mul_m4_v3(sm->ob->obmat, location);

		new_depth = len_v3v3(location, sm->ray_start);

		//I'm guessing location needed to be multiplied by the matrix so it can be in "world space"? for this
		//calculation...
		project_int(sm->ar, location, screen_loc);


		new_dist = abs(screen_loc[0] - (int)sm->mval[0]) + abs(screen_loc[1] - (int)sm->mval[1]);

		/* 10% threshold if edge is closer but a bit further
		 * this takes care of series of connected edges a bit slanted w.r.t the viewport
		 * otherwise, it would stick to the verts of the closest edge and not slide along merrily
		 * */
		if (new_dist > sm->snap_point.r_dist || new_depth >= sm->snap_point.r_depth * 1.001f){
			continue;
		}


		// = new_depth;
		sm->retval = 1;

		sub_v3_v3v3(edge_loc, v1.co, v2.co);
		sub_v3_v3v3(vec, intersect, v2.co);

		mul = dot_v3v3(vec, edge_loc) / dot_v3v3(edge_loc, edge_loc);

		/*normals calculation*/
		normal_short_to_float_v3(n1, v1.no);
		normal_short_to_float_v3(n2, v2.no);
		interp_v3_v3v3(sm->snap_point.normal, n2, n1, mul);
		mul_m3_v3(sm->timat, sm->snap_point.normal);
		normalize_v3(sm->snap_point.normal);

		copy_v3_v3(sm->snap_point.normal, location);

		copy_v3_v3(sm->snap_point.location, location);

		sm->snap_point.r_dist = new_dist;
		sm->snap_point.r_depth = new_depth;
	}

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

	//Check whether there has been provided previously calculated snap point to compare
	//distance and depth values to when calculating. if not then set default values.
	if(sm->closest_point == NULL){
		sm->snap_point.r_dist = sm->min_distance; //TODO: investigate, does this r_dist value need to be stored in sm class?
		sm->snap_point.r_depth = FLT_MAX;
	}else{
		sm->snap_point.r_dist = sm->closest_point->r_dist;
		sm->snap_point.r_depth = sm->closest_point->r_depth;
	}

	switch(sm_data->sm_type){
	case SNAPMESH_TYPE_VERTEX:
		SnapMesh_snap_vertex(sm);
		break;
	case SNAPMESH_TYPE_FACE:
		return; //call face snap function here
	case SNAPMESH_TYPE_FACE_PLANE:
		return; //call face plane snap function here
	case SNAPMESH_TYPE_EDGE:
		SnapMesh_snap_edge(sm);
		break;
	case SNAPMESH_TYPE_EDGE_MIDPOINT:
		return; //call edge midpoint snap function here
	case SNAPMESH_TYPE_EDGE_PARALLEL:
		return; //call edge parallel snap function here

	}

	//call callback here?
}



/* -- AXIS SNAPPING -- */



/* -- BONE SNAPPING -- */
