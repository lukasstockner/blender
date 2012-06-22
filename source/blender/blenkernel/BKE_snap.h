#ifndef __BKE_SNAP_H__
#define __BKE_SNAP_H__

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

#endif // __BKE_SNAP_H__
