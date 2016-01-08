#ifndef SPLIT_BUF2
#define SPLIT_BUF2(name, type) SPLIT_BUF(name, type)
#endif
#ifndef SPLIT_BUF_CL
#define SPLIT_BUF_CL(name, type) SPLIT_BUF(name, type)
#endif
#ifndef SPLIT_BUF2_CL
#define SPLIT_BUF2_CL(name, type) SPLIT_BUF(name, type)
#endif

#define SD_VAR(type, name) SPLIT_BUF(name ## _sd, type) SPLIT_BUF2(name ## _sd_DL_shadow, type)
#define SD_CLOSURE_VAR(type, name, num) SPLIT_BUF_CL(name ## _sd, type) SPLIT_BUF2_CL(name ## _sd_DL_shadow, type)
#include "../kernel_shaderdata_vars.h"
#undef SD_VAR
#undef SD_CLOSURE_VAR

SPLIT_BUF(rng_coop, RNG)
SPLIT_BUF(throughput_coop, float3)
SPLIT_BUF(L_transparent_coop, float)
SPLIT_BUF(PathRadiance_coop, PathRadiance)
SPLIT_BUF(Ray_coop, Ray)
SPLIT_BUF(PathState_coop, PathState)
SPLIT_BUF(Intersection_coop, Intersection)
SPLIT_BUF(BSDFEval_coop, BsdfEval)
SPLIT_BUF(ISLamp_coop, int)
SPLIT_BUF(LightRay_coop, Ray)
SPLIT_BUF(AOAlpha_coop, float3)
SPLIT_BUF(AOBSDF_coop, float3)
SPLIT_BUF(AOLightRay_coop, Ray)
SPLIT_BUF(Intersection_coop_AO, Intersection)
SPLIT_BUF(Intersection_coop_DL, Intersection)
#ifdef WITH_CYCLES_DEBUG
SPLIT_BUF(debugdata_coop, DebugData)
#endif
SPLIT_BUF(ray_state, char)
SPLIT_BUF(work_array, unsigned int)

#undef SPLIT_BUF
#undef SPLIT_BUF2
#undef SPLIT_BUF_CL
#undef SPLIT_BUF2_CL
