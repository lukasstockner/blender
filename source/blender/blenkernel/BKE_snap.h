#ifndef __BKE_SNAP_H__
#define __BKE_SNAP_H__

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

typedef struct{
	//perhaps more info later I might guess
	float location[3];
	float normal[3];
}SnapPoint;

typedef struct Snap Snap;

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

Snap* SnapMesh_create(	void* mesh_data,
						SnapMesh_data_type data_type,
						SnapMesh_type sm_type,
						Scene *scene, Object *ob, View3D *v3d, ARegion *ar, int mval[2]);



#endif // __BKE_SNAP_H__
