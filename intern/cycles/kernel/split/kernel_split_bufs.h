#define SD_VAR(type, name) SPLIT_BUF(name ## _sd) SPLIT_BUF(name ## _sd_DL_shadow)
#define SD_CLOSURE_VAR(type, name, num) SPLIT_BUF(name ## _sd) SPLIT_BUF(name ## _sd_DL_shadow)
#include "../kernel_shaderdata_vars.h"
#undef SD_VAR
#undef SD_CLOSURE_VAR

SPLIT_BUF(rng_coop)
SPLIT_BUF(throughput_coop)
SPLIT_BUF(L_transparent_coop)
SPLIT_BUF(PathRadiance_coop)
SPLIT_BUF(Ray_coop)
SPLIT_BUF(PathState_coop)
SPLIT_BUF(Intersection_coop)
SPLIT_BUF(sd)
SPLIT_BUF(sd_DL_shadow)
SPLIT_BUF(BSDFEval_coop)
SPLIT_BUF(ISLamp_coop)
SPLIT_BUF(LightRay_coop)
SPLIT_BUF(AOAlpha_coop)
SPLIT_BUF(AOBSDF_coop)
SPLIT_BUF(AOLightRay_coop)
SPLIT_BUF(Intersection_coop_AO)
SPLIT_BUF(Intersection_coop_DL)
#ifdef WITH_CYCLES_DEBUG
SPLIT_BUF(debugdata_coop)
#endif
SPLIT_BUF(ray_state)
SPLIT_BUF(per_sample_output_buffers)
SPLIT_BUF(work_array)
SPLIT_BUF(Queue_data)
SPLIT_BUF(Queue_index)
SPLIT_BUF(use_queues_flag)
