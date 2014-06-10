/* This file defines some stub function defs used 
 * for various operation callbacks which have not
 * been implemented in Blender yet. It should
 * eventually disappear instead of remaining as
 * part of the code base.
 */

#ifndef __DEPSGRAPH_FN_STUBS_H__
#define __DEPSGRAPH_FN_STUBS_H__

#pragma message("DEPSGRAPH PORTING XXX: There are still some undefined stubs")

struct ID;
struct Scene;
struct Object;
struct FCurve;
struct bPose;
struct bPoseChannel;
struct Mesh;
struct MetaBall;
struct Curve;
struct Lattice;
struct ModifierData;
struct ParticleSystem;

void BKE_animsys_eval_driver(ID *id, FCurve *fcurve);

void BKE_object_constraints_evaluate(Object *ob);
void BKE_pose_constraints_evaluate(Object *ob, bPoseChannel *pchan);

void BKE_pose_iktree_evaluate(Object *ob, bPoseChannel *rootchan);
void BKE_pose_splineik_evaluate(Object *ob, bPoseChannel *rootchan);
void BKE_pose_eval_bone(Object *ob, bPoseChannel *pchan);

void BKE_pose_rebuild_op(Object *ob, bPose *pose);
void BKE_pose_eval_init(Object *ob, bPose *pose);
void BKE_pose_eval_flush(Object *ob, bPose *pose);

void BKE_particle_system_eval(Object *ob, ParticleSystem *psys);

void BKE_rigidbody_rebuild_sim(Scene *scene); // BKE_rigidbody_rebuild_sim
void BKE_rigidbody_eval_simulation(Scene *scene); // BKE_rigidbody_do_simulation
void BKE_rigidbody_object_sync_transforms(Scene *scene, Object *ob); // BKE_rigidbody_sync_transforms

void BKE_object_eval_local_transform(Object *ob);
void BKE_object_eval_parent(Object *ob);
void BKE_object_eval_modifier(Object *ob, ModifierData *md);

void BKE_mesh_eval_geometry(Mesh *mesh);  // wrapper around makeDerivedMesh() - which gets BMesh, etc. data...
void BKE_mball_eval_geometry(MetaBall *mball); // BKE_displist_make_mball
void BKE_curve_eval_geometry(Curve *curve); // BKE_displist_make_curveTypes
void BKE_curve_eval_path(Curve *curve);
void BKE_lattice_eval_geometry(Lattice *latt); // BKE_lattice_modifiers_calc

#endif //__DEPSGRAPH_FN_STUBS_H__

