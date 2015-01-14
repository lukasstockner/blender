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

#endif //__DEPSGRAPH_FN_STUBS_H__

