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
 * Contributor(s): Luke Frisken 2012
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BKE_snap.h"
#include "BKE_mesh.h"
#include "BKE_tessmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_anim.h"

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_listBase.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_view3d.h"
#include "ED_screen.h"
#include "ED_mesh.h"

#include "BLI_math_vector.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_kdopbvh.h"
#include "BLI_math_geom.h"

#include "UI_resources.h"

#include "transform.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <assert.h>

//#define DEBUG_SNAP

/* -- PROTOTYPES -- */
typedef struct MeshData MeshData;
typedef struct SnapStack SnapStack;
typedef struct SnapIsect SnapIsect;

void SnapSystem_reset_snappoint(SnapSystem* ssystem);
void SnapSystem_parallel_hack_ob_handler(SnapSystem* ssystem, void* callback_data, Object* ob); //TODO: get rid of this
int SnapSystem_event_handler_idle(SnapSystem* ssystem, wmEvent* event);
int SnapSystem_event_handler_init(SnapSystem* ssystem, wmEvent* event);
int SnapSystem_event_handler_snapping(SnapSystem* ssystem, wmEvent* event);
int SnapSystem_cancel_event(SnapSystem *ssystem);

SnapStack* SnapStack_create(void);
int SnapStack_getN_Snaps(SnapStack* sstack);
void SnapStack_pushSnap(SnapStack* sstack, Snap* s);
Snap* SnapStack_popSnap(SnapStack* sstack);
Snap* SnapStack_getSnap(SnapStack* sstack, int index);
void SnapStack_reset(SnapStack* sstack);
void SnapStack_free(SnapStack*);
void Snap_draw_default(Snap *s);
void SnapMesh_draw_planar(Snap *sm);
void SnapMesh_draw_edge_parallel(Snap *sm);
void SnapMesh_run(Snap *sm);
void SnapMesh_free(Snap *sm);
void MeshData_pickface_free(MeshData_pickface *pface);
MeshData_pickface* MeshData_pickface_create(MFace *face, MVert *verts, int nverts, float *no);
Snap_type Snap_getType(Snap* s);

/* -- STRUCTS and ENUMS -- */
typedef enum{
	SNAP_ISECT_TYPE_GEOMETRY,
	SNAP_ISECT_TYPE_POINT
}SnapIsect_type;

struct SnapIsect{
	SnapIsect_type type;
	Snap* isect_geom; //a new snap containing the snap geometry of the intersection.
	SnapPoint isect_point; //for use if the intersection is a single point (isect_type == SNAP_ISECT_TYPE_POINT)
};

/*Snap struct/class is used for calculating snaps, and drawing them.
 *It can be thought of as an abstract class. It doesn't do anything by itself,
 *but it has many subclasses which do*/
struct Snap{
	/*s_type is the type of snap (Mesh, Curve, Bone, Axis...)*/
	Snap_type s_type;
	/*snap found is boolean whether snap point has been found*/
	int snap_found;
	/*snap_point is the storage point for the calculated snap point*/
	SnapPoint snap_point;
	/*closest_point, when set is used during calculations to ignore
	 *potential snaps which are not as close to the screen (depth) or the mouse.
	 *It is the previously calculated closest point to mouse and screen.*/
	SnapPoint* closest_point;
	/*pick is a previously calculated snap to be used for mosed such as planar
	 *and parallel snapping, as the user picked face/line/vert.*/
	Snap* pick;

	//TODO: all extra data in Snap should be subject to streamlining and change
	//in the near future, as soon as I get something working. A lot of quick
	//not-so-well-thought-out bits of code here that will need more thought later.

	int retval; //temporary variable for linking in with transform code

	/*external data pointers*/
	bContext *C;
	Scene* scene;
	Object* ob;
	View3D* v3d;
	ARegion* ar;
	int mval[2];

	/*internal calculation data*/
	float ray_start[3], ray_normal[3];
	float ray_start_local[3], ray_normal_local[3];
	/*min_distance is the minimum distance from the mouse to the snap point required
	 *for the snap point to be valid during calculations*/
	int min_distance;

	/*matrix storage*/
	float imat[4][4];
	float timat[3][3];

	//snap type -- type of snap to calculate
	//geom_data_type

	//transform

	/*snap_data stores the data for Snap's subclasses (e.g. SnapMesh_data)*/
	void* snap_data;

	/*function pointers*/
	/*run is the function which calculates the snap.*/
	void (*run)(Snap*);
	SnapIsect* (*interesect)(Snap*,Snap*); //for use with multisnap
	/*draws the snap. By default this is set to Snap_draw_default()*/
	void (*draw)(Snap*);
	/*function pointer to the subclass's free function*/
	void (*free)(Snap*);
	//void callback for doing transform -- function pointer
};

typedef struct SnapObRef SnapObRef;
struct SnapObRef{
	SnapObRef *next, *prev;
	Object* ob;
};

/* -- SnapSystem Class -- */
struct SnapSystem{
	SnapStack* sstack;

	/*events and state machine variables*/
	/*most are hacks to get this working for now*/
	SnapSystem_state state;
	int (*event_handler)(SnapSystem *ssystem,wmEvent* event);

	/*used for parallel snap, perpendicular and planar snap to store user picked geometry*/
	Snap* pick;

	/*external data pointers required by all snap modes*/
	bContext* C;
	Scene* scene;
	View3D* v3d;
	ARegion* ar;
	Object* obedit;

	/*snap_point is storage of the snapsystem's current snap point for access by the outside world*/
	SnapPoint snap_point;
	/*minimum distance between mouse and geometry for snap to occur*/
	int min_distance;
	/* reval is a boolean indicating whether or not a valid snap point has been found*/
	int retval;

	//list of objects in the scene which the snapssystem can currently operate upon
	ListBase ob_list;
	int n_obs;

	SnapSystem_mode_select mode_select; //todo: find out more about what this does

	/*callback_data is the data assigned from parent code using snapsystem (e.g. transform)
	 *to be used when the function pointers update, finish or cancel_callback are called.*/
	void* callback_data;

	/*update_callback gets called when the snapping system has not yet recieved confirmation from
	 *the user of the chosen snap point, but there is a temporary snap_point available for providing
	 *realtime feedback to the user of their actions when moving the mouse. In the case of transform,
	 *this means the geometry moves towards the snap point without permenantly being applied.*/
	void (*update_callback)(SnapSystem *ssystem, void* callback_data, SnapPoint sp);

	void (*nofeedback_callback)(SnapSystem *ssytem, void* callback_data); //TODO: change name of this function
	/*finish_callback is the same as update_callback, but the snap_point is to be applied this time.*/
	void (*finish_callback)(SnapSystem *ssystem, void* callback_data, SnapPoint sp);
	/*cancel_callback tells the code using the snapsystem that the current snapsystem snap
	 *search has been cancelled.*/
	void (*cancel_callback)(SnapSystem *ssystem, void* callback_data);

	void (*object_iterator)(SnapSystem* ssystem, void* callback_data);
	void (*object_handler)(SnapSystem* ssystem, void* callback_data, Object* ob);

	MeshData* (*meshdata_selector)(SnapStack* stack, void* callback_data);
	void (*bonedata_selector)(SnapStack* stack, void* callback_data);
};

/*SnapSystem_create is a function used to setup and create an instance of SnapSystem.
 *Remember to call SnapSystem_free() when finished with it.*/
SnapSystem* SnapSystem_create( Scene* scene, View3D* v3d, ARegion* ar, Object* obedit, bContext* C,
								void* callback_data,
								void (*update_callback)(SnapSystem *ssystem, void* callback_data, SnapPoint sp),
							   void (*nofeedback_callback)(SnapSystem *ssystem, void* callback_data),
								void (*finish_callback)(SnapSystem *ssystem, void* callback_data, SnapPoint sp),
							   void (*cancel_callback)(SnapSystem *ssystem, void* callback_data))
{
	SnapSystem* ssystem = (SnapSystem*)MEM_mallocN(sizeof(SnapSystem), "snapsystem");
	ssystem->sstack = SnapStack_create();
	ssystem->C = C;
	ssystem->scene = scene;
	ssystem->v3d = v3d;
	ssystem->ar = ar;
	ssystem->obedit = obedit;
	ssystem->callback_data = callback_data;
	ssystem->update_callback = update_callback;
	ssystem->nofeedback_callback = nofeedback_callback;
	ssystem->finish_callback = finish_callback;
	ssystem->cancel_callback = cancel_callback;
	ssystem->object_iterator = SnapSystem_default_object_iterator;
	/*I'm thinking of having an object_handler struct full of functions for handling each type
	  of object. So the other sections of code can override the handling of a certain type of object, say
	  for a certain tool which uses curves or a certain tool which uses meshes but wants the snapping to work in
	  a particular manner*/
	ssystem->object_handler = SnapSystem_default_object_handler;
	ssystem->min_distance = 30; //TODO: make this settable in user settings.
	SnapSystem_reset_snappoint(ssystem);
	ssystem->n_obs = 0;
	ssystem->ob_list.first = NULL;
	ssystem->ob_list.last = NULL;

	/*TODO: clean up state stuff with proper design*/
	ssystem->retval = 0;
	ssystem->state = SNAPSYSTEM_STATE_IDLE;
	ssystem->event_handler=SnapSystem_event_handler_idle;
	ssystem->pick = NULL;
	return ssystem;
}

void SnapSystem_test_run(SnapSystem* ssystem){
	SnapSystem_evaluate_stack(ssystem);
}

void SnapSystem_reset_snappoint(SnapSystem* ssystem){
	ssystem->snap_point.r_dist = ssystem->min_distance;
	ssystem->snap_point.r_depth = FLT_MAX;
	//ssystem->retval = 0;
}

SnapPoint* SnapSystem_getSnapPoint(SnapSystem* ssystem){
	return &(ssystem->snap_point);
}

void SnapSystem_state_logic(SnapSystem* ssystem){
//	ToolSettings *ts= ssystem->scene->toolsettings;
	switch(ssystem->state){
	case SNAPSYSTEM_STATE_INIT_SNAP:
		SnapSystem_pick_snap(ssystem);
		break;
	case SNAPSYSTEM_STATE_SNAPPING:
		SnapSystem_find_snap(ssystem);
		break;
	default:
		break;
	}
}

void SnapSystem_find_snap(SnapSystem* ssystem){
	Object* ob;
	int i;
	for(i=0;i<ssystem->n_obs;i++){
		ob = SnapSystem_get_ob(ssystem, i);
		ssystem->object_handler(ssystem, ssystem->callback_data, ob);
	}
}

void SnapSystem_pick_snap(SnapSystem* ssystem){
	//use the parallel snap hack object handler
	Object* ob;
	int i;
	ssystem->nofeedback_callback(ssystem, ssystem->callback_data); //resets transform
	for(i=0;i<ssystem->n_obs;i++){
		ob = SnapSystem_get_ob(ssystem, i);
		SnapSystem_parallel_hack_ob_handler(ssystem, ssystem->callback_data, ob);
	}
	SnapSystem_reset_snappoint(ssystem); //reset snap point generated by picking
}

//TODO: change this when I refactor the way object handlers work
Snap* snapmesh_edit_create(SnapSystem* ssystem, void* callback_data, MeshData_check check, SnapMesh_type sm_type, Object* ob, BMEditMesh *em, DerivedMesh *dm){
	TransInfo* t = (TransInfo*)callback_data; //hack to test using existing option system
	Scene* scene = ssystem->scene;
	bContext *C = ssystem->C;
	if(em == NULL){
		return SnapMesh_create(dm, SNAPMESH_DATA_TYPE_DerivedMesh, 1, check, sm_type, scene, ob, t->view, t->ar, C, t->mval);
	}else{
		dm->release(dm);
		return SnapMesh_create(em, SNAPMESH_DATA_TYPE_BMEditMesh, 0, check, sm_type, scene, ob, t->view, t->ar, C, t->mval);
	}
}

void SnapSystem_parallel_hack_ob_handler(SnapSystem* ssystem, void* callback_data, Object* ob){
	int retval;
	BMEditMesh *em;
	DerivedMesh *dm;
	Scene* scene = ssystem->scene;
	Snap* sm;
	SnapPoint* sp;
	SnapPoint sp_prev;
	float* r_depth = &(ssystem->snap_point.r_depth);
	int* r_dist = &(ssystem->snap_point.r_dist);

	ToolSettings *ts= scene->toolsettings;

	if (ob->type != OB_MESH){
		return;
	}

	if (ssystem->mode_select == SNAPSYSTEM_MODE_SELECT_ALL && ob->mode == OB_MODE_EDIT) {
		em = BMEdit_FromObject(ob);
		/* dm = editbmesh_get_derived_cage(scene, ob, em, CD_MASK_BAREMESH); */
		dm = editbmesh_get_derived_base(ob, em); /* limitation, em & dm MUST have the same number of faces */
	}
	else {
		em = NULL;
		dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	}

	if(ts->snap_mode == SCE_SNAP_MODE_EDGE_PARALLEL){
		sm = snapmesh_edit_create(ssystem, callback_data, MESHDATA_CHECK_HIDDEN, SNAPMESH_TYPE_EDGE, ob, em, dm);
	}
	if(ts->snap_mode == SCE_SNAP_MODE_PLANAR){
		//todo: fix bvh function so it can filter out hidden and selected
		sm = snapmesh_edit_create(ssystem, callback_data, MESHDATA_CHECK_NONE, SNAPMESH_TYPE_FACE, ob, em, dm);
	}

	//if this is not the first run through.
	//TODO: not sure how this will work with planar snapping
	if(*r_depth < FLT_MAX){
		sp_prev.r_depth = *r_depth;
		sp_prev.r_dist = *r_dist;
		Snap_setClosestPoint(sm, &sp_prev);
	}

	Snap_run(sm);
	retval = Snap_getretval(sm);
	if(retval){
		sp = Snap_getSnapPoint(sm);
		//printf("SnapPointExternal: %f, %f, %f\n", sp->location[0], sp->location[1], sp->location[2]);
		copy_v3_v3(ssystem->snap_point.location, sp->location);
		copy_v3_v3(ssystem->snap_point.normal, sp->normal);
		*r_depth = sp->r_depth;
		*r_dist = sp->r_dist;
	}

	if(retval){
		SnapSystem_clear_pick(ssystem); //free and clear the pick variable
		ssystem->pick = sm;
		if(ssystem->state != SNAPSYSTEM_STATE_INIT_SNAP){
			ssystem->update_callback(ssystem, ssystem->callback_data, ssystem->snap_point);
		}else{
#ifdef DEBUG_SNAP
			//printf("missed");
#endif
		}

	}else{
		Snap_free(sm);
	}
	ssystem->retval |= retval;
}

/*evaluate the SnapStack contained within SnapSystem to
 *allow final snappoint to be aquired and for draw functions to work. */
void SnapSystem_evaluate_stack(SnapSystem* ssystem){
	ssystem->object_iterator(ssystem, NULL);
	SnapSystem_state_logic(ssystem);
	//for snap in stack do evaluate blah blah blah
}

/*TODO:
  object handler functionality is going to change drastically, by using a list of snap combinations
  with objects.
  */

void SnapSystem_default_object_handler(SnapSystem* ssystem, void* callback_data, Object* ob){
	int retval;
//	TransInfo* t = (TransInfo*)callback_data; //hack to test using existing option system
	BMEditMesh *em;
	DerivedMesh *dm;
	Scene* scene = ssystem->scene;
//	bContext *C = ssystem->C;
	Snap* sm;
	SnapPoint* sp;
	SnapPoint sp_prev;
	MeshData_check default_check = MESHDATA_CHECK_HIDDEN|MESHDATA_CHECK_SELECTED;
	float* r_depth = &(ssystem->snap_point.r_depth);
	int* r_dist = &(ssystem->snap_point.r_dist);

	ToolSettings *ts= scene->toolsettings;

	if (ob->type != OB_MESH){
		return;
	}else{
#ifdef DEBUG_SNAP
		//printf("yaya mesh object");
#endif //DEBUG_SNAP
	}

	//TODO: move to snapmesh_editmesh_create
	if (ssystem->mode_select == SNAPSYSTEM_MODE_SELECT_ALL && ob->mode == OB_MODE_EDIT) {
		em = BMEdit_FromObject(ob);
		/* dm = editbmesh_get_derived_cage(scene, ob, em, CD_MASK_BAREMESH); */
		dm = editbmesh_get_derived_base(ob, em); /* limitation, em & dm MUST have the same number of faces */
	}
	else {
		em = NULL;
		dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	}

	if(ts->snap_mode == SCE_SNAP_MODE_VERTEX){
		/*TODO, IDEAS:
		  currently the snap system is set up so you need to pick a snap type based on object type.
		  This is a problem with multiple types of objects in a scene. In this case I would like to see
		  an option for point snap, which each type of object must have a set snapping mode that supports
		  point snap. For mesh snap this would be vertex snap. For curve this would be curve snap. and for
		  any other type of object (camera, light, etc) it could just be the location of the object. Some way of resolving
			which types of objects a given snap mode can apply to. and also only display snapping modes which are possible
			on the currently edited object? no that would not be good. Need to think of a good way to deal with this.
		EDIT: see comment above this function*/
		sm = snapmesh_edit_create(ssystem, callback_data, default_check, SNAPMESH_TYPE_VERTEX, ob, em, dm);
	}else if(ts->snap_mode == SCE_SNAP_MODE_EDGE){
		sm = snapmesh_edit_create(ssystem, callback_data, default_check, SNAPMESH_TYPE_EDGE, ob, em, dm);
	}else if(ts->snap_mode == SCE_SNAP_MODE_EDGE_MIDDLE){
		sm = snapmesh_edit_create(ssystem, callback_data, default_check, SNAPMESH_TYPE_EDGE_MIDPOINT, ob, em, dm);
	}else if(ts->snap_mode == SCE_SNAP_MODE_FACE){
		sm = snapmesh_edit_create(ssystem, callback_data, default_check, SNAPMESH_TYPE_FACE, ob, em, dm);
	}else if(ts->snap_mode == SCE_SNAP_MODE_EDGE_PARALLEL){
		sm = snapmesh_edit_create(ssystem, callback_data, MESHDATA_CHECK_HIDDEN, SNAPMESH_TYPE_EDGE_PARALLEL, ob, em, dm);
		Snap_setpick(sm, ssystem->pick);
	}else if(ts->snap_mode == SCE_SNAP_MODE_PLANAR){
			sm = snapmesh_edit_create(ssystem, callback_data, MESHDATA_CHECK_HIDDEN, SNAPMESH_TYPE_PLANAR, ob, em, dm);
			Snap_setpick(sm, ssystem->pick);
	}else{
		dm->release(dm);
		return;
	}

	//if this is not the first run through.
	if(*r_depth < FLT_MAX){
		sp_prev.r_depth = *r_depth;
		sp_prev.r_dist = *r_dist;
		Snap_setClosestPoint(sm, &sp_prev);
	}

	Snap_run(sm);
	retval = Snap_getretval(sm);
	if(retval){
		sp = Snap_getSnapPoint(sm);
		//printf("SnapPointExternal: %f, %f, %f\n", sp->location[0], sp->location[1], sp->location[2]);
		copy_v3_v3(ssystem->snap_point.location, sp->location);
		copy_v3_v3(ssystem->snap_point.normal, sp->normal);
		*r_depth = sp->r_depth;
		*r_dist = sp->r_dist;
	}
	SnapStack_reset(ssystem->sstack);
	SnapStack_pushSnap(ssystem->sstack, sm);

//	Snap_free(sm);


	ssystem->retval |= retval;
	if(retval){
		if(ssystem->state != SNAPSYSTEM_STATE_INIT_SNAP){
			ssystem->update_callback(ssystem, ssystem->callback_data, ssystem->snap_point);
		}
	}
}

void SnapSystem_default_object_iterator(SnapSystem* ssystem, void* UNUSED(callback_data)){
	Base* base;
	Scene* scene = ssystem->scene;
	View3D* v3d = ssystem->v3d;

	SnapSystem_reset_ob_list(ssystem);

	if (ssystem->mode_select == SNAPSYSTEM_MODE_SELECT_ALL && ssystem->obedit) {
		Object *ob = ssystem->obedit;
		SnapSystem_add_ob(ssystem, ob);
		//ssystem->object_handler(ssystem, ssystem->callback_data, ob);
	}

	/* Need an exception for particle edit because the base is flagged with BA_HAS_RECALC_DATA
	 * which makes the loop skip it, even the derived mesh will never change
	 *
	 * To solve that problem, we do it first as an exception.
	 * */

	base = BASACT;
	if (base && base->object && base->object->mode & OB_MODE_PARTICLE_EDIT) {
		Object *ob = base->object;
		ssystem->obedit = ob;
		//ssystem->object_handler(ssystem, ssystem->callback_data, ob);
		SnapSystem_add_ob(ssystem, ob);
	}

	for ( base = FIRSTBASE; base != NULL; base = base->next ) {
		if ( (BASE_VISIBLE(v3d, base)) &&
			 (base->flag & (BA_HAS_RECALC_OB|BA_HAS_RECALC_DATA)) == 0 &&

			 (  (ssystem->mode_select == SNAPSYSTEM_MODE_SELECT_NOT_SELECTED && (base->flag & (SELECT|BA_WAS_SEL)) == 0) ||
				(ELEM(ssystem->mode_select, SNAPSYSTEM_MODE_SELECT_ALL, SNAPSYSTEM_MODE_SELECT_NOT_OBEDIT) && base != BASACT))  )
		{
			Object *ob = base->object;

			if (ob->transflag & OB_DUPLI) {
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(scene, ob);

				for (dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next) {
					Object *dob = dupli_ob->ob;

					SnapSystem_add_ob(ssystem, dob);
					//ssystem->object_handler(ssystem, ssystem->callback_data, dob);
				}

				free_object_duplilist(lb);
			}

			SnapSystem_add_ob(ssystem, ob);
			//ssystem->object_handler(ssystem, ssystem->callback_data, ob);
		}
	}
}

//static int snap_type_exec(bContext *C, wmOperator *op)
//{
//	ToolSettings *ts= CTX_data_tool_settings(C);

//	ts->snap_mode = RNA_enum_get(op->ptr, "type");

//	WM_event_add_notifier(C, NC_SCENE|ND_TOOLSETTINGS, NULL); /* header redraw */

//	return OPERATOR_FINISHED;
//}

extern EnumPropertyItem snap_element_items[];

/*TODO: in the future I want to make it so the snap type selector only shows the available snap types
  to select based on the state of the snapsystem*/
//static void SNAP_OT_snap_type(wmOperatorType *ot)
//{
//	/* identifiers */
//	ot->name = "Snap Type";
//	ot->description = "Set the snap element type";
//	ot->idname = "SNAP_OT_snap_type";

//	/* api callbacks */
//	ot->invoke = WM_menu_invoke;
//	ot->exec = snap_type_exec;
//	//ot->poll = ED_operator_areaactive;

//	/* flags */
//	ot->flag = OPTYPE_UNDO;

//	/* props */
//	ot->prop = RNA_def_enum(ot->srna, "type", snap_element_items, 0, "Type", "Set the snap element type");

//}


/*This function drives the snapping system by passing it events. It indicates whether the events
 * have been consumed by its return value, so external code can act accordingly.*/
/*TODO:
  there will be a patch soon to replace this with function based state handling, according to Martin's Idea.
  I've experimented with it a bit so far on a small scale and it makes sense, just ran out of time for this patch
  to implement it properly. That's the goal for next patch. Everything here should be a lot cleaner, and easier
  to explain with that.*/


//TODO: just get rid of this function, I don't think it's really needed
int SnapSystem_Event(SnapSystem* ssystem, wmEvent* event){
	//This function returns a 1 if the event was handled by this code to prevent
	//the rest of transform code from processing the event.
	//currently this function is just the bare bones support for parallel snap
	//The interpretation of events should be over-ridable by the code which uses the
	//snapping system, such as the pen tool.
	return ssystem->event_handler(ssystem, event);
}

int SnapSystem_cancel_event(SnapSystem *ssystem){
	ssystem->cancel_callback(ssystem, ssystem->callback_data);
	ssystem->state = SNAPSYSTEM_STATE_IDLE;
	ssystem->event_handler = SnapSystem_event_handler_idle;
	return 1;
}

int SnapSystem_event_handler_idle(SnapSystem* ssystem, wmEvent* event){
	ToolSettings *ts= ssystem->scene->toolsettings;
	int handled = 0;
	if (event->type == EVT_MODAL_MAP){
		/*TODO: move all this keymap stuff to snap system and out of transform.h*/
		switch(event->val){
			case TFM_MODAL_SNAP_INV_ON:
				if(ts->snap_mode == SCE_SNAP_MODE_EDGE_PARALLEL || ts->snap_mode == SCE_SNAP_MODE_PLANAR){
					ssystem->state = SNAPSYSTEM_STATE_INIT_SNAP;
					ssystem->event_handler = SnapSystem_event_handler_init;
				}else{
					ssystem->state = SNAPSYSTEM_STATE_SNAPPING;
					ssystem->event_handler = SnapSystem_event_handler_snapping;
				}
				SnapSystem_evaluate_stack(ssystem);
				handled = 1;
				break;
			case TFM_MODAL_SNAP_TOGGLE:
				/*toggle consuming all events*/
			case TFM_MODAL_AXIS_X:
				/*IDEA: initiate special axis snap mode*/
				break;
			case TFM_MODAL_AXIS_Y:
				break;
			case TFM_MODAL_AXIS_Z:
				break;
			default:
				break;
		}
	}
	return handled;
}

int SnapSystem_event_handler_init(SnapSystem *ssystem, wmEvent* event){
	int handled = 0;
	if (event->type == EVT_MODAL_MAP){
		/*TODO: move all this keymap stuff to snap system and out of transform.h*/
		switch(event->val){
			case TFM_MODAL_CANCEL:
				/*stop consuming all the events*/
				ssystem->cancel_callback(ssystem, ssystem->callback_data);
				ssystem->state = SNAPSYSTEM_STATE_IDLE;
				ssystem->event_handler = SnapSystem_event_handler_idle;
				handled = 1;
				break;
			case TFM_MODAL_CONFIRM:
				/*depending on state, either continue, or stop snapping and call finish_callback*/
				SnapSystem_evaluate_stack(ssystem);
				//if we managed to pick something
				if(ssystem->retval){
					ssystem->state = SNAPSYSTEM_STATE_SNAPPING;
					ssystem->event_handler = SnapSystem_event_handler_snapping;
					ssystem->retval = 0; //reset retval after having used hack function to do the grunt work
				}
				handled = 1;
				break;
			case TFM_MODAL_SNAP_INV_OFF:
				/*stop consuming all the events*/
				handled = SnapSystem_cancel_event(ssystem);
				SnapStack_reset(ssystem->sstack);
				break;
			default:
				SnapSystem_evaluate_stack(ssystem);
				handled = 1;
				break;
		}
	}else if (event->val == KM_PRESS){
		switch(event->type){
		case RIGHTMOUSE:
			handled = SnapSystem_cancel_event(ssystem);
			break;
		default:
			SnapSystem_evaluate_stack(ssystem);
			handled = 1;
			break;
		}
	}
	return handled;
}

int SnapSystem_event_handler_snapping(SnapSystem *ssystem, wmEvent* event){
	int handled = 0;
	if (event->type == EVT_MODAL_MAP){
		/*TODO: move all this keymap stuff to snap system and out of transform.h*/
		switch(event->val){
			case TFM_MODAL_CANCEL:
				/*stop consuming all the events*/
				handled = SnapSystem_cancel_event(ssystem);
				break;
			case TFM_MODAL_CONFIRM:
				/*depending on state, either continue, or stop snapping and call finish_callback*/
				SnapSystem_evaluate_stack(ssystem);
				ssystem->state = SNAPSYSTEM_STATE_IDLE;
				ssystem->event_handler = SnapSystem_event_handler_idle;
				ssystem->finish_callback(ssystem, ssystem->callback_data, ssystem->snap_point);
				handled = 1;
				break;
			case TFM_MODAL_SNAP_INV_OFF:
				/*stop consuming all the events*/
				handled = SnapSystem_cancel_event(ssystem);
				SnapStack_reset(ssystem->sstack);
				break;
			default:
				break;
		}
	}else if (event->val == KM_PRESS){
		switch(event->type){
		case RIGHTMOUSE:
			handled = SnapSystem_cancel_event(ssystem);
			break;
		default:
			SnapSystem_evaluate_stack(ssystem);
			handled = 1;
			break;
		}
	}
	return handled;
}

void SnapSystem_reset_ob_list(SnapSystem* ssystem){
	BLI_freelistN(&(ssystem->ob_list));
	ssystem->n_obs = 0;
}

/*SnapSystem operates on a specific list of objects. This function is used to add objects to that list.
 *There are other internal functions which use this function to add objects automatically by a filter.*/
void SnapSystem_add_ob(SnapSystem* ssystem, Object* ob){
	SnapObRef *ob_ref = MEM_callocN(sizeof(TransSnapPoint), "SnapObRef");
	ob_ref->ob = ob;
	BLI_addtail(&(ssystem->ob_list), ob_ref);
	ssystem->n_obs++;
}

Object* SnapSystem_get_ob(SnapSystem* ssystem, int index){
	SnapObRef* ob_ref;
	assert(index < ssystem->n_obs);
	ob_ref = (SnapObRef*)BLI_findlink(&(ssystem->ob_list), index);
	return ob_ref->ob;
}

/*calls draw for each of the active snaps in the stack*/
void SnapSystem_draw(SnapSystem* ssystem){
	Snap* s;
	if(ssystem->retval && SnapStack_getN_Snaps(ssystem->sstack) > 0){
		s = (Snap*)SnapStack_getSnap(ssystem->sstack, 0);
		if(s == NULL){
			return;
		}
		s->draw(s);
	}

}

//TODO: find out how to clean up this function and the way it gets used within snapsystem
void SnapSystem_set_mode_select(SnapSystem* ssystem, SnapSystem_mode_select mode_select){
	ssystem->mode_select = mode_select;
}

int SnapSystem_get_retval(SnapSystem* ssystem){
	return ssystem->retval;
}

bContext* SnapSystem_get_C(SnapSystem* ssystem){
	return ssystem->C;
}

SnapSystem_state SnapSystem_get_state(SnapSystem* ssystem){
	return ssystem->state;
}

void SnapSystem_clear_pick(SnapSystem* ssystem){
	if(ssystem->pick){
		Snap_free(ssystem->pick);
		ssystem->pick = NULL;
	}
}

/*reset snap system back to default state*/
void SnapSystem_reset(SnapSystem* ssystem){
	SnapSystem_reset_ob_list(ssystem);
	SnapSystem_reset_snappoint(ssystem);
	ssystem->state = SNAPSYSTEM_STATE_IDLE;
}

void SnapSystem_free(SnapSystem* ssystem){
	SnapStack_free(ssystem->sstack);
	BLI_freelistN(&(ssystem->ob_list));
	SnapSystem_clear_pick(ssystem);
	MEM_freeN(ssystem);

}


/* -- SnapStack Class -- */
/*This is used for storing a stack of snaps to collapse and evaluate when using multi-snap*/

#define MAX_SNAPS 3

struct SnapStack{
	int n_snaps;
	Snap* snap_stack[MAX_SNAPS]; //maximum of 3 simultaneous snaps
	void (*bone_iterator)(SnapStack* stack, void* callback_data);
};

SnapStack* SnapStack_create(){
	SnapStack* sstack = (SnapStack*)MEM_mallocN(sizeof(SnapStack), "snapstack");
	sstack->n_snaps = 0;
	return sstack;
}

int SnapStack_getN_Snaps(SnapStack* sstack){
	return sstack->n_snaps;
}

void SnapStack_pushSnap(SnapStack* sstack, Snap* s){
	assert(sstack->n_snaps + 1 <= MAX_SNAPS);
	//need to add some kind of error message here when exceed stack MAX_SNAPS
	sstack->snap_stack[sstack->n_snaps] = s;
	sstack->n_snaps++;
	return;
}

Snap* SnapStack_popSnap(SnapStack* sstack){
	assert(sstack->n_snaps > 0);
	sstack->n_snaps--;
	return sstack->snap_stack[sstack->n_snaps];
}

Snap* SnapStack_getSnap(SnapStack* sstack, int index){
	assert(index < sstack->n_snaps && index >= 0);
	return sstack->snap_stack[index];
}

void SnapStack_reset(SnapStack* sstack){
	int i;
	for(i=0; i<sstack->n_snaps; i++){
		Snap_free(sstack->snap_stack[i]);
		sstack->snap_stack[i] = NULL;
	}
	sstack->n_snaps = 0;
}

void SnapStack_free(SnapStack* sstack){
	SnapStack_reset(sstack);
	MEM_freeN(sstack);
}



/* -- Snap Class -- */
/*This class represents an individual snap. */

/*This function creates an instance of the Snap class.
 *Don't forget to call Snap_free on it when you're finished with it!*/
Snap* Snap_create(Snap_type s_type,
				  Scene *scene, Object* ob, View3D *v3d, ARegion *ar, bContext *C, int mval[2],
				  void (*run)(Snap*), void (*draw)(Snap*), void (*free)(Snap*)){
	Snap* s = (Snap*)MEM_mallocN(sizeof(Snap), "snap");

	/*s_type is the type of snap (Mesh, Curve, Bone, Axis...)*/
	s->s_type = s_type;

	s->scene = scene;
	s->ob = ob;
	s->v3d = v3d;
	s->ar = ar;
	s->C = C;
	copy_v2_v2_int(s->mval, mval);

	/*min_distance is the minimum distance from the mouse to the snap point required
	 *for the snap point to be valid during calculations*/
	s->min_distance = 30; //TODO: change to a user defined value;
	/*pick is a previously calculated snap to be used for mosed such as planar and
	 *parallel snapping, as the user picked face/line/vert. */
	s->pick = NULL;
	/*closes_point is a pointer, that when set is used during calculations to ignore
	 *potential snaps which are not as close to the screen (depth) or the mouse.*/
	s->closest_point = NULL;
	s->snap_found = 0;

	//TODO: pass the function pointers in as arguments
	s->run = run;
	s->draw = draw;

	/*This variable stores the function pointer to the subclass's free function.
	 *It's an internal variable, please call Snap_free instead if you want to free
	 * the Snap class*/
	s->free = free;

	/*snap_data stores the data for Snap's subclasses (e.g. SnapMesh_data)*/
	s->snap_data = NULL;
	return s;
}

void Snap_setmval(Snap* s, int mval[2]){
	copy_v2_v2_int(s->mval, mval);
}

int Snap_getretval(Snap* s){
	return s->retval;
}

Snap_type Snap_getType(Snap* s){
	return s->s_type;
}

void Snap_setpick(Snap* s, Snap* pick){
	s->pick = pick;
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

/*Snap_free_f() frees Snap struct's internal data. This is useful to call
 *after Snap's subclasses have finished their own free, and need to free
 *parent data. */
void Snap_free_f(Snap* s){
	MEM_freeN(s);
}

void Snap_run(Snap* s){
	s->run(s);
}

void Snap_draw(Snap* s){
	s->draw(s);
}

/*Default drawing function to be assigned to s->draw.
 *It draws the snap_point as a circle at its location.*/
void Snap_draw_default(Snap *UNUSED(s)){
//	unsigned char col[4], selectedCol[4], activeCol[4];
//	UI_GetThemeColor3ubv(TH_TRANSFORM, col);
//	col[3]= 128;

//	UI_GetThemeColor3ubv(TH_SELECT, selectedCol);
//	selectedCol[3]= 128;

//	UI_GetThemeColor3ubv(TH_ACTIVE, activeCol);
//	activeCol[3]= 192;

//	if (t->spacetype == SPACE_VIEW3D) {
//		TransSnapPoint *p;
//		View3D *v3d = s->v3d;
//		RegionView3D *rv3d = CTX_wm_region_view3d(C);
//		float imat[4][4];
//		float size;

//		glDisable(GL_DEPTH_TEST);

//		size = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);

//		invert_m4_m4(imat, rv3d->viewmat);

//		for (p = t->tsnap.points.first; p; p = p->next) {
//			if (p == t->tsnap.selectedPoint) {
//				glColor4ubv(selectedCol);
//			}
//			else {
//				glColor4ubv(col);
//			}

//			drawcircball(GL_LINE_LOOP, p->co, ED_view3d_pixel_size(rv3d, p->co) * size * 0.75f, imat);
//		}

//		if (t->tsnap.status & POINT_INIT) {
//			glColor4ubv(activeCol);

//			drawcircball(GL_LINE_LOOP, t->tsnap.snapPoint, ED_view3d_pixel_size(rv3d, t->tsnap.snapPoint) * size, imat);
//		}

//		/* draw normal if needed */
//		if (usingSnappingNormal(t) && validSnappingNormal(t)) {
//			glColor4ubv(activeCol);

//			glBegin(GL_LINES);
//				glVertex3f(t->tsnap.snapPoint[0], t->tsnap.snapPoint[1], t->tsnap.snapPoint[2]);
//				glVertex3f(t->tsnap.snapPoint[0] + t->tsnap.snapNormal[0],
//						   t->tsnap.snapPoint[1] + t->tsnap.snapNormal[1],
//						   t->tsnap.snapPoint[2] + t->tsnap.snapNormal[2]);
//				glEnd();
//		}

//		if (v3d->zbuf)
//			glEnable(GL_DEPTH_TEST);
//	}
}



/* -- MESH SNAPPING -- */

/* SnapMesh isn't really a class, but rather a collection of functions
 *designed to operate on one type (Mesh) of instance of Snap defined by
 *its s_type variable. It could be thought of as a subclass of Snap, introducing
 *the necessary methods to snap with meshes*/

/*TODO: implement a better system for input/output with snaps to allow things such as
  user configurable values for snaps like angles, number of sections for midpoint, and other things
  */
typedef struct {
	/*Stores the type of meshsnap to calculate (planar, face, vertex, etc..).
	 *It can be a combination of several.*/
	SnapMesh_type sm_type;

	/*actual mesh data stored in consistent interface (see struct MeshData)*/
	MeshData *mesh_data;

	/*return the vertex, edge or face picked by user for
	use in combining snap modes (parallel and edge snap)*/

	int ret_data_index; //index to use when getting element from mesh_data

	/*When ret_data_pface is assigned snap returns a face.
	 *pface (pikcface) is a struct storing all the details of a face, with its
	 *own internal vert array*/

	MeshData_pickface *ret_data_pface; // as the return value.
	SnapMesh_ret_data_type ret_data_type; //vert index, edge index or pick face.

	/*check is a variable which stores the various check types that a snap needs to make
	 *on the mesh during its calculations to avoid snapping on them. (selected or
	 *hidden geometry)*/
	MeshData_check check;

	int *dm_index_array; //for use when using derivedmesh data type; not in use?

	//function mesh iterator -- hmm actually let's just make this SnapMesh_mesh_itorator()

}SnapMesh_data;

/* The purpose of meshdata is to provide a consistent interface to the mesh system for use in mesh based
  snapping modes without the requirement to convert between them, and thus suffer a performance loss.
At the moment it supports two backends: BMEditmesh and DerivedMesh. It does this using a struct with
function pointers that are assigned according to the type of mesh system that is getting used*/
struct MeshData{
	SnapMesh_data_type data_type;
	void* data;
	DerivedMesh* dm_bvh; //use derivedmesh if bvh functions are called.

	/*edit_mode is a boolean for if mesh is from an object in edit mode*/
	int edit_mode;

	/*free_data is a boolean for if the data given the the MeshData struct during creation should be
	 *free'd along with the MeshData struct when the MeshData->free() function is called.
	 *This is useful if a temporary mesh has been passed to MeshData.
	 *TODO: to let SnapMesh_free know whether to free the meshdata.*/
	int free_data;

	int (*getNumVerts)(MeshData* md);
	int (*getNumEdges)(MeshData* md);
	int (*getNumFaces)(MeshData* md);

	//call before using getVert, getEdge, getFace or bvh_from_faces functions
	void (*index_init)(MeshData* md, SnapMesh_data_array_type array_type);

	void (*getVert)(MeshData* md, int index, MVert* mv);
	void (*getEdgeVerts)(MeshData* md, int index, MVert* mv1, MVert* mv2);

	/*create a bvh from faces contained within MeshData*/
	BVHTreeFromMesh* (*bvh_from_faces)(MeshData* md, Object* ob, MeshData_check check);
	void (*free_bvh_tree)(MeshData* md, BVHTreeFromMesh* treeData);

	/*Conduct checks on vert or edge to determine whether they have various qualities (hidden, selected)
	 *which might make them unsuitable for snapping.*/
	int (*checkVert)(MeshData* md, int index, MeshData_check check);
	int (*checkEdge)(MeshData* md, int index, MeshData_check check);

	void (*free)(MeshData* md);
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

//void bmedge_to_medge(BMesh *bm, BMEdge *ee, MEdge *edge_r)
//{

//}

//TODO: finish writing this function!
void bmface_to_mface(BMesh *UNUSED(bm), BMFace *UNUSED(bmf), MFace *UNUSED(mf)){

}

MeshData_pickface* MeshData_pickface_create(MFace *face, MVert *verts, int nverts, float *no){
	int i;
	MeshData_pickface *pickface = (MeshData_pickface*)MEM_mallocN(sizeof(MeshData_pickface), "snap_meshdata_pickface");
	pickface->face = (MFace*)MEM_mallocN(sizeof(MFace), "snap_meshdata_pickface_face");
	memcpy(pickface->face, face, sizeof(MFace));
	pickface->face->v1 = 0;
	pickface->face->v2 = 1;
	pickface->face->v3 = 2;
	pickface->verts = (MVert*)MEM_mallocN(sizeof(MVert) * nverts, "snap_meshdata_pickface_mverts");
	pickface->nverts = nverts;
	for(i=0;i<nverts;i++){
		memcpy(&(pickface->verts[i]), &(verts[i]), sizeof(MVert));
	}
	copy_v3_v3(pickface->no, no);
	return pickface;
}

void MeshData_pickface_free(MeshData_pickface *pface){
	MEM_freeN(pface->verts);
	MEM_freeN(pface->face);
	MEM_freeN(pface);
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
int MeshData_BMEditMesh_checkVert(MeshData *md, int index, MeshData_check UNUSED(check)){
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


//return a 1 if any of the checks return a positive
int MeshData_BMEditMesh_checkEdge(MeshData *md, int index, MeshData_check check){
	BMEdge* bme;
	BMEditMesh* em = (BMEditMesh*)md->data;
	int retval = 0;
	bme = EDBM_edge_at_index(em, index);
	/* check whether edge is hidden and if either of its vertices are selected*/
	if(check & MESHDATA_CHECK_HIDDEN){
		if(BM_elem_flag_test(bme, BM_ELEM_HIDDEN)){
			retval |= 1;
		}
	}
	if(check & MESHDATA_CHECK_SELECTED){
		if(BM_elem_flag_test(bme->v1, BM_ELEM_SELECT) ||
		 BM_elem_flag_test(bme->v2, BM_ELEM_SELECT)){
			retval |= 1;
		}
	}
	return retval;
}

void MeshData_BMEditMesh_getFace(MeshData *md, int index, MFace* mf){
	BMFace* bmf;
	BMEditMesh* em = (BMEditMesh*)md->data;
	bmf = EDBM_face_at_index(em, index);
	bmface_to_mface(em->bm, bmf, mf);
}

BVHTreeFromMesh* MeshData_BMEditMesh_bvh_from_faces(MeshData* md, Object* ob, MeshData_check check){
	/*this function creates a bvh tree from the faces in meshdata using the struct passed in through
	the treeData pointer*/
	BMEditMesh* em = (BMEditMesh*)md->data;
	BVHTreeFromMesh* treeData;
	if(md->dm_bvh == NULL){ //if this code has already executed, use previous dm_bvh to avoid calculations
		md->dm_bvh = editbmesh_get_derived_base(ob, em);
	}
	treeData = (BVHTreeFromMesh*)MEM_mallocN(sizeof(BVHTreeFromMesh), "snap_bvhtreedata");
	if(check & MESHDATA_CHECK_SELECTED || check & MESHDATA_CHECK_HIDDEN){
		treeData->em_evil = em; //see bvhtree_from_mesh_faces function for comments on this value.
	}else{
		treeData->em_evil = NULL;
	}
	bvhtree_from_mesh_faces(treeData, md->dm_bvh, 0.0f, 4, 6);
	return treeData;
}

void MeshData_BMEditMesh_free(MeshData* md){
	BMEditMesh* em;
	if(md->free_data){
		em = (BMEditMesh*)md->data;
		//TODO: free bmeditmesh
	}
	if(md->dm_bvh){
		md->dm_bvh->release(md->dm_bvh);
	}
	MEM_freeN(md);
}


/* - MeshData DerivedMesh accessor implementation - */

void MeshData_DerivedMesh_index_init(MeshData* UNUSED(md), SnapMesh_data_array_type UNUSED(array_type)){
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

//TODO: implement check argument functionality
int MeshData_DerivedMesh_checkVert(MeshData *md, int index, MeshData_check UNUSED(check)){
	MVert *verts;
	MVert *mv;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	char hidden, selected;
	verts = dm->getVertArray(dm);
	//TODO: remove these debugging variables mv and hidden
	mv = &(verts[index]);
	hidden =  (mv->flag & ME_HIDE);

	//there is a weird bug where the selected flag is always on for meshes which aren't in editmode.
	if(md->edit_mode){
		selected = (mv->flag & 1);
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
//TODO: make this more clear that there is only one edge in the snap_edge_parallel meshdata (the edge that is selected for snapping is copied)
int MeshData_DerivedMesh_checkEdge(MeshData *md, int index, MeshData_check check){
	//TODO: optimise this code a bit, and remove debug variables.
	MEdge* edges;
	MVert* verts;
	MVert *v1, *v2;
	DerivedMesh* dm = (DerivedMesh*)md->data;
	char v1_selected, v2_selected;
	int retval = 0;
	edges = dm->getEdgeArray(dm);
	verts = dm->getVertArray(dm);
	/* check whether edge is hidden and if either of its vertices (v1 and v2) are selected
	When they are selected their flag will be 0? (if indeed it works similar to bmesh)*/
	/* in this case v1 and v2 are integer indexes for the vertex array*/


	if(check & MESHDATA_CHECK_HIDDEN){
		if(edges[index].flag & ME_HIDE){
			retval |= 1;
		}
	}
	if(check & MESHDATA_CHECK_SELECTED){
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
		if(v1_selected || v2_selected){
			retval |= 1;
		}
	}

	return retval;
}

void MeshData_DerivedMesh_getFace(MeshData *UNUSED(md), int UNUSED(index), MVert* UNUSED(mv)){

}

//TODO: add support for check variable
BVHTreeFromMesh*  MeshData_DerivedMesh_bvh_from_faces(MeshData* md, Object* UNUSED(ob), MeshData_check UNUSED(check)){
	/*this function creates a bvh tree from the faces in meshdata using the struct passed in through
	the treeData pointer*/
	DerivedMesh* dm = (DerivedMesh*)md->data;
	BVHTreeFromMesh* treeData = (BVHTreeFromMesh*)MEM_mallocN(sizeof(BVHTreeFromMesh), "snap_bvhtreedata");
	treeData->em_evil = NULL;
	bvhtree_from_mesh_faces(treeData, dm, 0.0f, 4, 6);
	return treeData;
}

void MeshData_DerivedMesh_free(MeshData *md){
	DerivedMesh* dm;
	if(md->free_data){
		dm = (DerivedMesh*)md->data;
		dm->release(dm);
	}
	MEM_freeN(md);
}

void MeshData_free_bvh_tree(MeshData* UNUSED(md), BVHTreeFromMesh* treeData){
	free_bvhtree_from_mesh(treeData);
	MEM_freeN(treeData);
}

/*Create a MeshData struct for use in MeshSnap functions. see struct MeshData comments for more details*/
MeshData* MeshData_create(void* mesh_data, SnapMesh_data_type data_type, int free_data, int edit_mode){
	MeshData* md = (MeshData*)MEM_mallocN(sizeof(MeshData), "snapmesh_meshdata");
	md->data_type = data_type;
	md->data = mesh_data;
	md->dm_bvh = NULL;
	md->edit_mode = edit_mode;//TODO: this variable might need to be updated if this thing is cached...
	md->free_data = free_data;
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
		md->bvh_from_faces = MeshData_DerivedMesh_bvh_from_faces;
		md->free_bvh_tree = MeshData_free_bvh_tree;
		md->free = MeshData_DerivedMesh_free;
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
		md->bvh_from_faces = MeshData_BMEditMesh_bvh_from_faces;
		md->free_bvh_tree = MeshData_free_bvh_tree;
		md->free = MeshData_BMEditMesh_free;
		break;
	}
	return md;
}

/* SnapMesh functions */

Snap* SnapMesh_create(	void* mesh_data,
						SnapMesh_data_type data_type,
						int free_mesh_data,
						MeshData_check check,
						SnapMesh_type sm_type,
						Scene *scene, Object *ob, View3D *v3d, ARegion *ar, bContext *C, int mval[2])
{
	int edit_mode;
	Snap* sm;
	SnapMesh_data* sm_data;
//	void (*run)(Snap*);
	void (*draw)(Snap*);

	switch(sm_type){
	case SNAPMESH_TYPE_VERTEX:
		//TODO: add SnapMesh_snap_vertex in here too.
		//run = SnapMesh_snap_vertex;
		draw = Snap_draw_default;
		break;
	case SNAPMESH_TYPE_FACE:
		draw = Snap_draw_default;
		break;
	case SNAPMESH_TYPE_PLANAR:
		draw = SnapMesh_draw_planar;
		break;
	case SNAPMESH_TYPE_EDGE:
		draw = Snap_draw_default;
		break;
	case SNAPMESH_TYPE_EDGE_MIDPOINT:
		draw = Snap_draw_default;
		break;
	case SNAPMESH_TYPE_EDGE_PARALLEL:
		draw = SnapMesh_draw_edge_parallel;
		break;
	}
	sm = Snap_create(SNAP_TYPE_MESH, scene, ob, v3d, ar, C, mval, SnapMesh_run, draw, SnapMesh_free);
	sm_data = (SnapMesh_data*)MEM_mallocN(sizeof(SnapMesh_data), "snapmesh_data");
	sm_data->sm_type = sm_type;

	//put an early exit flag somewhere here for the case when there is no geometry in mesh_data;
	edit_mode = (sm->ob->mode & OB_MODE_EDIT);
	sm_data->mesh_data = MeshData_create(mesh_data, data_type, free_mesh_data, edit_mode);
	sm_data->dm_index_array = NULL;
	sm_data->check = check;

	sm->snap_data = (void*)sm_data;

	return sm;
}

void SnapMesh_free(Snap* sm){
	//TODO: there is some memory not getting freed somwhere in here...
	SnapMesh_data *sm_data = sm->snap_data;
	sm_data->mesh_data->free(sm_data->mesh_data);
	if(sm_data->ret_data_type == SNAPMESH_RET_DAT_pface){
		MeshData_pickface_free((MeshData_pickface*)(sm_data->ret_data_pface));
	}
	MEM_freeN(sm_data);
	Snap_free_f(sm);
}

//This function is a custom callback for use with bvh intersection to
//return the mface which was intersected for use as pick data in face snap
//TODO: remove I don't think I'm using this function after all...
//static void SnapMesh_faces_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
//{
//	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
//	MVert *vert = data->vert;
//	MFace *face = data->face + index;

//	float *t0, *t1, *t2, *t3;
//	t0 = vert[face->v1].co;
//	t1 = vert[face->v2].co;
//	t2 = vert[face->v3].co;
//	t3 = face->v4 ? vert[face->v4].co : NULL;


//	do {
//		float dist;
//		if (data->sphere_radius == 0.0f)
//			dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);
//		else
//			dist = sphereray_tri_intersection(ray, data->sphere_radius, hit->dist, t0, t1, t2);

//		if (dist >= 0 && dist < hit->dist) {
//			hit->index = index;
//			hit->dist = dist;
//			madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

//			normal_tri_v3(hit->no, t0, t1, t2);

//			if (t1 == vert[face->v3].co)
//				hit->flags |= BVH_ONQUAD;
//		}

//		t1 = t2;
//		t2 = t3;
//		t3 = NULL;

//	} while (t2);
//}

/* SnapMesh Implementation */
void snap_face_return_val(SnapMesh_data* sm_data, BVHTreeFromMesh *treeData, BVHTreeRayHit *hit){
	/*This function is a method to figure out the return pickface for snap_face function*/
	/*this function is kind of a dud without better bvh support for ngons and quads*/
	/*for some reason bvh seems to only contain tris!?*/
	/*TODO: make this better when bvh stuff gets investigated/fixed*/
	MVert verts[4];
	MVert *v1, *v2, *v3; //, *v4;
	MFace *face = treeData->face + hit->index;
	v1 = treeData->vert + face->v1;
	v2 = treeData->vert + face->v2;
	v3 = treeData->vert + face->v3;
	//v1 = treeData->vert + face->v1;
	memcpy(&(verts[0]), v1, sizeof(MVert));
	memcpy(&(verts[1]), v2, sizeof(MVert));
	memcpy(&(verts[2]), v3, sizeof(MVert));

	sm_data->ret_data_pface = MeshData_pickface_create(face, verts, 3, hit->no);
	sm_data->ret_data_type = SNAPMESH_RET_DAT_pface;
}

/*The following SnapMesh_snap_face(), SnapMesh_snap_planar(), SnapMesh_snap_vertex(),
 *SnapMesh_snap_edge(), SnapMesh_snap_edge_midpoint(), SnapMesh_snap_parallel() are
 *The various snap function calculate a snap point given the mouse position and various
 *other inputs, and then assign the retval variable of Snap according to whether a valid
 *snap was found or not. All, but planar and parallel have a return value indicating the
 *geometry which the user has picked during their snap, for use in combination with other
 *snap modes (planar, parallel etc..). */

void SnapMesh_snap_face(Snap* sm){
	//Todo: try to use the MeshData face functions here to test them better, even though performance with them will suck.
	SnapMesh_data* sm_data = (SnapMesh_data*)sm->snap_data;
	MeshData* md = sm_data->mesh_data;
	BVHTreeRayHit hit;
	BVHTreeFromMesh* treeData;
	MFace *face;
	float local_scale = len_v3(sm->ray_normal_local);

	sm->retval = 0;
	treeData = md->bvh_from_faces(md, sm->ob, sm_data->check);

	hit.index = -1;
	hit.dist = sm->snap_point.r_depth * (sm->snap_point.r_depth == FLT_MAX ? 1.0f : local_scale);

	if (treeData->tree && BLI_bvhtree_ray_cast(treeData->tree, sm->ray_start_local, sm->ray_normal_local, 0.0f, &hit, treeData->raycast_callback, treeData) != -1) {
		if (hit.dist / local_scale <= sm->snap_point.r_depth) {
			face = treeData->face + hit.index;
			sm->snap_point.r_depth = hit.dist / local_scale;
			copy_v3_v3(sm->snap_point.location, hit.co);
			copy_v3_v3(sm->snap_point.normal, hit.no);

			/* back to worldspace */
			mul_m4_v3(sm->ob->obmat, sm->snap_point.location);

			mul_m3_v3(sm->timat, sm->snap_point.normal);
			normalize_v3(sm->snap_point.normal);

			snap_face_return_val(sm_data, treeData, &hit);
			sm->retval = 1;
		}
	}
	md->free_bvh_tree(md, treeData);
}

void SnapMesh_snap_planar(Snap* sm){
	SnapMesh_data* sm_data = (SnapMesh_data*)sm->pick->snap_data; //use data from pick
	/*float[0] -> float[2] represent normal of plane. float[3] is the distance from origin*/
//	float plane[4]; //could alternatively use isect_line_plane
	MVert *v1, *v2, *v3;
	float lambda;
	int result;

	assert(sm_data->ret_data_type == SNAPMESH_RET_DAT_pface);
	sm->retval = 0;
	v1 = &(sm_data->ret_data_pface->verts[0]); //grab 3 verts from pick
	v2 = &(sm_data->ret_data_pface->verts[1]);
	v3 = &(sm_data->ret_data_pface->verts[2]);
	//find whether there is an intersection between ray from screen and a plane based upon these
	//3 verts. Lambda is the distance along the ray that the intersection occurs.
	result = isect_ray_plane_v3(sm->ray_start_local, sm->ray_normal_local, v1->co, v2->co, v3->co, &lambda, 0.001);
	if(result){
		float intersect[3];
		float location[3];

		copy_v3_v3(intersect, sm->ray_normal_local);
		//find intersection vector from camera using lambda which is the length along the normal.
		mul_v3_fl(intersect, lambda);
		//find the intersection point in absolute local space
		add_v3_v3(intersect, sm->ray_start_local);
		copy_v3_v3(location, intersect);

		mul_m4_v3(sm->ob->obmat, location); //transform location into global space
		copy_v3_v3(sm->snap_point.location, location);
		copy_v3_v3(sm->snap_point.normal, sm_data->ret_data_pface->no); //grab normal from pick face
		mul_m3_v3(sm->timat, sm->snap_point.normal); //transform normal into global space
		sm->retval = 1;
	}
}

extern GLubyte stipple_quarttone[128];
extern GLubyte stipple_diag_stripes_pos[128];

//TODO: make this function cool :)
void SnapMesh_draw_planar(Snap* UNUSED(sm)){
//	MeshData_pickface* pf;
//	float quat[4];
//	int i;
//	unsigned char col[] = {255, 255, 255};
//	Snap* pick = sm->pick;
//	SnapMesh_data* sm_pick_data = (SnapMesh_data*)pick->snap_data;
//	View3D *v3d = sm->v3d;
//	float verts[4][3] = {{-1.0f, -1.0f, 0.0f},
//						 {-1.0f, 1.0f, 0.0f},
//						 {1.0f, 1.0f, 0.0f},
//						 {1.0f, -1.0f, 0.0f}};

//	assert(sm_pick_data->ret_data_type == SNAPMESH_RET_DAT_pface);
//	pf = sm_pick_data->ret_data_pface;
//	assert(pf->nverts >= 3);

//	tri_to_quat(quat, pf->verts[0].co, pf->verts[1].co, pf->verts[2].co); //produce quaternion from existing face user picked

//	glPushMatrix();

//	glColor3ubv(col);

//	glBegin(GL_QUADS);
//	for(i=0;i<4;i++){
//		mul_qt_v3(quat,verts[i]);
//		mul_v3_fl(verts[i], v3d->far);
//		add_v3_v3(verts[i], pick->snap_point.location);

//		glVertex3fv(verts[i]);
//	}
//	glEnd();

//	glPopMatrix();
}

void SnapMesh_draw_planar_texture(Snap* sm){
	MeshData_pickface* pf;
	float quat[4];
	GLuint texture;
	int i, j;
	unsigned char col[] = {255, 255, 255};
	Snap* pick = sm->pick;
	SnapMesh_data* sm_pick_data = (SnapMesh_data*)pick->snap_data;
	View3D *v3d = sm->v3d;
	GLubyte pixels[32][32][3];
	float verts[4][3] = {{-1.0f, -1.0f, 0.0f},
						 {-1.0f, 1.0f, 0.0f},
						 {1.0f, 1.0f, 0.0f},
						 {1.0f, -1.0f, 0.0f}};

	float tmap[4][2] = {{0.0f, 0.0f},
						{1.0f, 0.0f},
						{1.0f, 1.0f},
						{0.0f, 1.0f}};

	assert(sm_pick_data->ret_data_type == SNAPMESH_RET_DAT_pface);
	pf = sm_pick_data->ret_data_pface;
	assert(pf->nverts >= 3);

	tri_to_quat(quat, pf->verts[0].co, pf->verts[1].co, pf->verts[2].co); //produce quaternion from existing face user picked


	//Commented code for future reference and experimentation, if going down the road of textured plane display again.
	//will remove if not.
	/*glPushMatrix();

	glEnable(GL_TEXTURE_2D);
	glGenTextures(1,&texture);
	glBindTexture(GL_TEXTURE_2D, texture);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 16, 8, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, stipple_quarttone);
//	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, 32, 32, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	gluBuild2DMipmapLevels(GL_TEXTURE_2D, GL_RGB, 32, 32, GL_RGB, GL_UNSIGNED_BYTE,4,4,0, pixels);
	//	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); // Linear Filtering
//	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); // Linear Filtering
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); // Linear Filtering
//	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); // Linear Filtering
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR); // Linear Filtering

//	glEnable(GL_POLYGON_STIPPLE);
//	glPolygonStipple(stipple_diag_stripes_pos);
	glColor3ubv(col);

	glBindTexture(GL_TEXTURE_2D, texture);
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	for(i=0;i<4;i++){
		mul_qt_v3(quat,verts[i]);
		mul_v3_fl(verts[i], v3d->far);
		add_v3_v3(verts[i], pick->snap_point.location);

		mul_v3_fl(tmap[i], v3d->far/100.0f);
		glTexCoord2fv(tmap[i]);

		glVertex3fv(verts[i]);
	}
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(10.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(10.0f, 10.0f); glVertex3f( 1.0f,  1.0f,  1.0f);  // Top Right Of The Texture and Quad
	glTexCoord2f(0.0f, 10.0f); glVertex3f(-1.0f,  1.0f,  1.0f);  // Top Left Of The Texture and Quad
	glEnd();
//	glDisable(GL_POLYGON_STIPPLE);
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1, &texture);

	glPopMatrix();*/
}

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
		if(md->checkVert(md, i, MESHDATA_CHECK_HIDDEN|MESHDATA_CHECK_SELECTED) == 0){
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
			//this checks whether depth of snap point is the deeper than an existing point, if it is, then
			//skip this vert. it also checks whether the distance between the mouse and the vert is small enough, if it
			//isn't then skip this vert. it picks the closest one to the mouse, and the closest one to the screen.
			//if there is no existing snappable vert then r_depth == FLT_MAX;
			continue;
		}

		sm->snap_point.r_depth = new_depth;
		sm->retval = 1;

		copy_v3_v3(sm->snap_point.location, location);

		normal_short_to_float_v3(sm->snap_point.normal, mv.no);
		mul_m3_v3(sm->timat, sm->snap_point.normal);
		normalize_v3(sm->snap_point.normal);


		sm->snap_point.r_dist = new_dist;

		sm_data->ret_data_index = i;
		sm_data->ret_data_type = SNAPMESH_RET_DAT_vert_index;
	}
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
		if(md->checkEdge(md, i, sm_data->check) == 1){
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

		copy_v3_v3(sm->snap_point.location, location);

		sm->snap_point.r_dist = new_dist;
		sm->snap_point.r_depth = new_depth;

		sm_data->ret_data_index = i;
		sm_data->ret_data_type = SNAPMESH_RET_DAT_edge_index;
	}

}

void SnapMesh_snap_edge_midpoint(Snap* sm){
	int i, totedge, new_dist;
	int screen_loc[2];

	float ray_end[3], dvec[3];
	float edge_loc[3], vec[3], location[3];
	float n1[3], n2[3];
	float middle[3];
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
		if(md->checkEdge(md, i, sm_data->check) == 1){
			continue;
		}

		md->getEdgeVerts(md, i, &v1, &v2);

		mid_v3_v3v3(middle, v1.co, v2.co);

		sub_v3_v3v3(dvec, middle, sm->ray_start_local);

		if(dot_v3v3(sm->ray_normal_local, dvec)<=0){
			continue; //skip this vert if it is behind the view
		}

		copy_v3_v3(location, middle);
		mul_m4_v3(sm->ob->obmat, location);
		new_depth = len_v3v3(location, sm->ray_start);

		project_int(sm->ar, location, screen_loc);
		new_dist = abs(screen_loc[0] - sm->mval[0]) + abs(screen_loc[1] - sm->mval[1]);

		if(new_dist > sm->snap_point.r_dist || new_depth >= sm->snap_point.r_depth){//what is r_depth in original code? is it always FLT_MAX?
			//This checks whether depth of snap point is the deeper than an existing point, if it is, then
			//skip this vert. it also checks whether the distance between the mouse and the vert is small enough, if it
			//isn't then skip this vert. it picks the closest one to the mouse, and the closest one to the screen.
			//if there is no existing snappable vert then r_depth == FLT_MAX;
			continue;
		}

		sm->snap_point.r_depth = new_depth;
		sm->retval = 1;

		copy_v3_v3(sm->snap_point.location, location);

		/*normals calculation*/
		sub_v3_v3v3(edge_loc, v1.co, v2.co);
		sub_v3_v3v3(vec, middle, v2.co);

		mul = dot_v3v3(vec, edge_loc) / dot_v3v3(edge_loc, edge_loc);

		normal_short_to_float_v3(n1, v1.no);
		normal_short_to_float_v3(n2, v2.no);
		interp_v3_v3v3(sm->snap_point.normal, n2, n1, mul);
		mul_m3_v3(sm->timat, sm->snap_point.normal);
		normalize_v3(sm->snap_point.normal);

		sm->snap_point.r_dist = new_dist;
	}//call before using getVert, getEdge, getFace or bvh_from_faces functions
}

void SnapMesh_snap_edge_parallel(Snap* sm){
	int i, result, new_dist;
	int screen_loc[2];

	float intersect[3] = {0, 0, 0}, ray_end[3], dvec[3];
	float edge_loc[3], vec[3], location[3];
	float n1[3], n2[3];
	float mul, new_depth;

	SnapMesh_data* sm_data = (SnapMesh_data*)sm->pick->snap_data; //use mesh data from pick
	MeshData* md = sm_data->mesh_data;
	MVert v1, v2;

	copy_v3_v3(ray_end, sm->ray_normal_local);
	mul_v3_fl(ray_end, 2000); //TODO: what!?
	add_v3_v3v3(ray_end, sm->ray_start_local, ray_end);

	sm->retval = 0;

	assert(sm_data->ret_data_type == SNAPMESH_RET_DAT_edge_index);
	md->index_init(md, SNAPMESH_DAT_edge); //should perhaps only be called once per mesh...

	i = sm_data->ret_data_index;

	if(md->checkEdge(md, i, sm_data->check) == 1){
		return;
	}

	md->getEdgeVerts(md, i, &v1, &v2);

	result = isect_line_line_v3(v1.co, v2.co, sm->ray_start_local, ray_end, intersect, dvec); /* dvec used but we don't care about result */
	//intersect is closest point on line (beteween v1 and v2) to the line between ray_start_local and ray_end.
	if(!result){
		return;
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
//		if (mul > 1) {
//			mul = 1;
//			copy_v3_v3(intersect, v1.co);
//		}
//		else if (mul < 0) {
//			mul = 0;
//			copy_v3_v3(intersect, v2.co);
//		}

	//if intersect point along ray vector is not in front of the screen/camera then quit checking this line.
	//I would have thought this case would be eliminated by isect_line function in the first place.
	//TODO: explore this issue when coding parallel snapping.
	if (dot_v3v3(sm->ray_normal_local, dvec) <= 0){
		return;
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
	if (new_depth >= sm->snap_point.r_depth * 1.001f){
		return;
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

	copy_v3_v3(sm->snap_point.location, location);

	sm->snap_point.r_dist = new_dist;
	sm->snap_point.r_depth = new_depth;
}

void SnapMesh_draw_edge_parallel(Snap *sm){
	unsigned char col[4];
	MVert mv1, mv2;
	float v1[3], v2[3], v3[3], v4[3], v5[3], center[3];
	View3D *v3d = sm->v3d;
	//using user picked edge data for calculations
	SnapMesh_data* sm_pick_data = (SnapMesh_data*)sm->pick->snap_data;

	assert(sm_pick_data->ret_data_type == SNAPMESH_RET_DAT_edge_index);
	sm_pick_data->mesh_data->index_init(sm_pick_data->mesh_data, SNAPMESH_DAT_edge);
	sm_pick_data->mesh_data->getEdgeVerts(sm_pick_data->mesh_data, sm_pick_data->ret_data_index, &mv1, &mv2);
	copy_v3_v3(v1, mv1.co);
	copy_v3_v3(v2, mv2.co);
	sub_v3_v3v3(v3, v1, v2);
	normalize_v3(v3);

	copy_v3_v3(center, v1);


	glPushMatrix();

	if (sm->ob->mode == OB_MODE_EDIT){
		//glLoadMatrixf(sm->ob->obmat);	// sets opengl viewing
	}


	mul_v3_fl(v3, v3d->far);

	sub_v3_v3v3(v5, center, v3);
	add_v3_v3v3(v4, center, v3);
	mul_m4_v3(sm->ob->obmat, v5);
	mul_m4_v3(sm->ob->obmat, v4);

	col[0] = col[1] = col[2] = 220;
	glColor3ubv(col);

	setlinestyle(0);
	glBegin(GL_LINE_STRIP);
		glVertex3fv(v4);
		glVertex3fv(v5);
	glEnd();

	glPopMatrix();
}

/*TODO: do the same thing I did to SnapMesh_draw, and dissolve this into function pointer assignment at runtime*/
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
		SnapMesh_snap_face(sm);
		break;
	case SNAPMESH_TYPE_PLANAR:
		SnapMesh_snap_planar(sm);
		break;
	case SNAPMESH_TYPE_EDGE:
		SnapMesh_snap_edge(sm);
		break;
	case SNAPMESH_TYPE_EDGE_MIDPOINT:
		SnapMesh_snap_edge_midpoint(sm);
		break;
	case SNAPMESH_TYPE_EDGE_PARALLEL:
		SnapMesh_snap_edge_parallel(sm);
		break;

	}

	//call callback here?
}

/*snap intersection*/
SnapIsect* SnapIsect_create(SnapIsect_type type){
	SnapIsect* si = (SnapIsect*)MEM_mallocN(sizeof(SnapIsect), "snapisect");
	si->type = type;
	return si;
}

void SnapIsect_set_point(SnapIsect* si, SnapPoint point){
	memcpy(&(si->isect_point), &point, sizeof(SnapPoint));
	//SnapPoint_deepcopy(&(si->isect_point), &point);
}

void SnapIsect_free(SnapIsect* si){
	switch(si->type){
	case SNAP_ISECT_TYPE_GEOMETRY:
		Snap_free(si->isect_geom);
		break;
	default:
		break;
	}
	MEM_freeN(si);
}

//SnapIsect* SnapMesh_intersect_Plane_Plane(){

//}

/*this function tries to find the intersection between two snaps.
  if the SnapType of s2 is not supported, or an intersection cannot
be found then this function retruns NULL pointer.*/
/*when a NULL is returned, the SnapSystem evaluate function interprets differently;
  using perpendicular snapping instead*/
//SnapIsect* SnapMesh_intersect(Snap* sm, Snap* s2){
//	switch(s2->s_type){
//	case SNAP_TYPE_MESH:
////		if(s2->type == VERTEX){
////			return NULL;
////		}
//		sm->interesect(sm, s2);
//		break;
//	}
//}


//void SnapMesh_perpendicular(Snap* sm, SnapPoint point){
	//set the sm->snap_point to the point on the snap_geometry which is perpendicular
	//to the argurment point.
//}

/* -- AXIS SNAPPING -- */
/* this system will need some method of inputting the origin of the axis.
  for example:
	with transform that would be snap target or pivot center
	with pen tool that would be the previous vertex/click*/


/* -- BONE SNAPPING -- */



/* -- CURVE SNAPPING -- */
