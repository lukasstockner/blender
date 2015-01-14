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
struct EvaluationContext;
struct TimeSourceDepsNode;

void BKE_animsys_eval_animdata(struct EvaluationContext *eval_ctx, ID *id, TimeSourceDepsNode *time_src);
void BKE_animsys_eval_driver(struct EvaluationContext *eval_ctx, ID *id, FCurve *fcurve, TimeSourceDepsNode *time_src);

void BKE_particle_system_eval(struct EvaluationContext *eval_ctx, Object *ob, ParticleSystem *psys);

void BKE_rigidbody_rebuild_sim(struct EvaluationContext *eval_ctx, Scene *scene); // BKE_rigidbody_rebuild_sim
void BKE_rigidbody_eval_simulation(struct EvaluationContext *eval_ctx, Scene *scene); // BKE_rigidbody_do_simulation
void BKE_rigidbody_object_sync_transforms(struct EvaluationContext *eval_ctx, Scene *scene, Object *ob); // BKE_rigidbody_sync_transforms

void BKE_mesh_eval_geometry(struct EvaluationContext *eval_ctx, Mesh *mesh);  // wrapper around makeDerivedMesh() - which gets BMesh, etc. data...
void BKE_mball_eval_geometry(struct EvaluationContext *eval_ctx, MetaBall *mball); // BKE_displist_make_mball
void BKE_curve_eval_geometry(struct EvaluationContext *eval_ctx, Curve *curve); // BKE_displist_make_curveTypes
void BKE_curve_eval_path(struct EvaluationContext *eval_ctx, Curve *curve);
void BKE_lattice_eval_geometry(struct EvaluationContext *eval_ctx, Lattice *latt); // BKE_lattice_modifiers_calc

#endif //__DEPSGRAPH_FN_STUBS_H__

