#ifndef __BKE_SNAP_H__
#define __BKE_SNAP_H__

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "BKE_context.h"
#include "WM_types.h"

typedef struct{
	//perhaps more info later I might guess
	float location[3];
	float normal[3];
	int r_dist;
	float r_depth;
}SnapPoint;

typedef struct SnapSystem SnapSystem;

typedef enum{
	SNAPSYSTEM_MODE_SELECT_ALL = 0,
	SNAPSYSTEM_MODE_SELECT_NOT_SELECTED = 1,
	SNAPSYSTEM_MODE_SELECT_NOT_OBEDIT = 2
}SnapSystem_mode_select;

SnapSystem* SnapSystem_create( Scene* scene, View3D* v3d, ARegion* ar, Object* obedit, bContext* C,
								void* callback_data,
								void (*update_callback)(void* callback_data, SnapPoint sp),
								void (*finish_callback)(void* callback_data, SnapPoint sp),
							   void (*cancel_callback)(void* callback_data));

void SnapSystem_test_run(SnapSystem* ssystem);
void SnapSystem_reset_ob_list(SnapSystem* ssystem);
void SnapSystem_evaluate_stack(SnapSystem* ssystem);
void SnapSystem_find_snap(SnapSystem* ssystem);
void SnapSystem_pick_snap(SnapSystem* ssystem);
void SnapSystem_add_ob(SnapSystem* ssystem, Object* ob);
Object* SnapSystem_get_ob(SnapSystem* ssystem, int index);
void SnapSystem_set_mode_select(SnapSystem* ssystem, SnapSystem_mode_select mode_select);
int SnapSystem_get_retval(SnapSystem* ssystem);
int SnapSystem_Event(SnapSystem* ssystem, wmEvent* event);
void SnapSystem_reset_snappoint(SnapSystem* ssystem);
void SnapSystem_clear_pick(SnapSystem* ssystem);
void SnapSystem_reset(SnapSystem* ssystem);
void SnapSystem_free(SnapSystem* ssystem);

/*default callbacks*/
void SnapSystem_default_object_iterator(SnapSystem* ssystem, void* callback_data);
void SnapSystem_default_object_handler(SnapSystem* ssystem, void* callback_data, Object* ob);

typedef struct Snap Snap;

typedef enum{
	SNAP_TYPE_MESH,
	SNAP_TYPE_CURVE,
	SNAP_TYPE_BONE,
	SNAP_TYPE_AXIS
}Snap_type;

typedef enum{
	SNAPMESH_DATA_TYPE_DerivedMesh,
	SNAPMESH_DATA_TYPE_BMEditMesh
}SnapMesh_data_type;

typedef enum{
	SNAPMESH_TYPE_VERTEX,
	SNAPMESH_TYPE_FACE,
	SNAPMESH_TYPE_FACE_PLANE,
	SNAPMESH_TYPE_EDGE,
	SNAPMESH_TYPE_EDGE_MIDPOINT,
	SNAPMESH_TYPE_EDGE_PARALLEL
}SnapMesh_type;

typedef enum{
	SNAPMESH_DAT_vert,
	SNAPMESH_DAT_edge,
	SNAPMESH_DAT_face
}SnapMesh_data_array_type;

void Snap_run(Snap* s);
void Snap_free(Snap* s);

int Snap_getretval(Snap* s);
SnapPoint* Snap_getSnapPoint(Snap* s);
void Snap_setClosestPoint(Snap* s, SnapPoint* sp);
void Snap_setpick(Snap* s, Snap* pick);

Snap* SnapMesh_create(	void* mesh_data,
						SnapMesh_data_type data_type,
					   int free_mesh_data,
						SnapMesh_type sm_type,
						Scene *scene, Object *ob, View3D *v3d, ARegion *ar, bContext *C, int mval[2]);



#endif // __BKE_SNAP_H__
