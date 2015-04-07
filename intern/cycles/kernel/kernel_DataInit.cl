/*
 * Copyright 2011-2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kernel_split.h"

ccl_device_inline void kernel_path_trace_setup_SPLIT_KERNEL(__ADDR_SPACE__ KernelGlobals *kg, ccl_global uint *rng_state, int sample, int x, int y, __ADDR_SPACE__ RNG *rng, __ADDR_SPACE__ Ray *ray)
{
	float filter_u;
	float filter_v;

	int num_samples = kernel_data.integrator.aa_samples;

	path_rng_init(kg, rng_state, sample, num_samples, rng, x, y, &filter_u, &filter_v);

	/* sample camera ray */

	float lens_u = 0.0f, lens_v = 0.0f;

	if(kernel_data.cam.aperturesize > 0.0f)
		path_rng_2D(kg, rng, sample, num_samples, PRNG_LENS_U, &lens_u, &lens_v);

	float time = 0.0f;

#ifdef __CAMERA_MOTION__
	if(kernel_data.cam.shuttertime != -1.0f)
		time = path_rng_1D(kg, rng, sample, num_samples, PRNG_TIME);
#endif

	camera_sample(kg, x, y, filter_u, filter_v, lens_u, lens_v, time, ray);
}

/*
 * Note on kernel_ocl_path_trace_data_initialization_SPLIT_KERNEL kernel
 * This kernel Initializes structures needed in path-iteration kernels.
 * This is the first kernel in ray-tracing logic.
 *
 * Ray state of rays outside the tile-boundary will be marked RAY_INACTIVE
 *
 * Its input and output are as follows,
 *
 * Un-initialized rng---------------|--- kernel_ocl_path_trace_data_initialization_SPLIT_KERNEL ---|--- Initialized rng
 * Un-initialized throughput -------|                                                              |--- Initialized throughput
 * Un-initialized L_transparent ----|                                                              |--- Initialized L_transparent
 * Un-initialized PathRadiance -----|                                                              |--- Initialized PathRadiance
 * Un-initialized Ray --------------|                                                              |--- Initialized Ray
 * Un-initialized PathState --------|                                                              |--- Initialized PathState
 * Un-initialized QueueData --------|                                                              |--- Initialized QueueData (to QUEUE_EMPTY_SLOT)
 * Un-initilaized QueueIndex -------|                                                              |--- Initialized QueueIndex (to 0)
 * Un-initialized use_queues_flag---|                                                              |--- Initialized use_queues_flag (to false)
 * Un-initialized ray_state --------|                                                              |--- Initialized ray_state
 * parallel_samples --------------- |                                                              |--- Initialized per_sample_output_buffers
 * rng_state -----------------------|                                                              |--- Initialized work_array
 * data ----------------------------|                                                              |--- Initialized work_pool_wgs
 * start_sample --------------------|                                                              |
 * sx ------------------------------|                                                              |
 * sy ------------------------------|                                                              |
 * sw ------------------------------|                                                              |
 * sh ------------------------------|                                                              |
 * stride --------------------------|                                                              |
 * queuesize -----------------------|                                                              |
 * num_samples ---------------------|                                                              |
 *
 * Note on Queues :
 * All slots in queues are initialized to queue empty slot;
 * The number of elements in the queues is initialized to 0;
 */
__kernel void kernel_ocl_path_trace_data_initialization_SPLIT_KERNEL(
	ccl_global char *globals,
	ccl_global char *shader_data_sd,                  /* Arguments related to ShaderData */
	ccl_global char *shader_data_sd_dl,             /* Arguments related to ShaderData */
	ccl_global char *shader_data_sd_shadow,     /* Arguments related to ShaderData */

	ccl_global float3 *P_sd,
	ccl_global float3 *P_sd_dl,
	ccl_global float3 *P_sd_shadow,

	ccl_global float3 *N_sd,
	ccl_global float3 *N_sd_dl,
	ccl_global float3 *N_sd_shadow,

	ccl_global float3 *Ng_sd,
	ccl_global float3 *Ng_sd_dl,
	ccl_global float3 *Ng_sd_shadow,

	ccl_global float3 *I_sd,
	ccl_global float3 *I_sd_dl,
	ccl_global float3 *I_sd_shadow,

	ccl_global int *shader_sd,
	ccl_global int *shader_sd_dl,
	ccl_global int *shader_sd_shadow,

	ccl_global int *flag_sd,
	ccl_global int *flag_sd_dl,
	ccl_global int *flag_sd_shadow,

	ccl_global int *prim_sd,
	ccl_global int *prim_sd_dl,
	ccl_global int *prim_sd_shadow,

	ccl_global int *type_sd,
	ccl_global int *type_sd_dl,
	ccl_global int *type_sd_shadow,

	ccl_global float *u_sd,
	ccl_global float *u_sd_dl,
	ccl_global float *u_sd_shadow,

	ccl_global float *v_sd,
	ccl_global float *v_sd_dl,
	ccl_global float *v_sd_shadow,

	ccl_global int *object_sd,
	ccl_global int *object_sd_dl,
	ccl_global int *object_sd_shadow,

	ccl_global float *time_sd,
	ccl_global float *time_sd_dl,
	ccl_global float *time_sd_shadow,

	ccl_global float *ray_length_sd,
	ccl_global float *ray_length_sd_dl,
	ccl_global float *ray_length_sd_shadow,

	ccl_global int *ray_depth_sd,
	ccl_global int *ray_depth_sd_dl,
	ccl_global int *ray_depth_sd_shadow,

	ccl_global int *transparent_depth_sd,
	ccl_global int *transparent_depth_sd_dl,
	ccl_global int *transparent_depth_sd_shadow,
	#ifdef __RAY_DIFFERENTIALS__
	ccl_global differential3 *dP_sd,
	ccl_global differential3 *dP_sd_dl,
	ccl_global differential3 *dP_sd_shadow,

	ccl_global differential3 *dI_sd,
	ccl_global differential3 *dI_sd_dl,
	ccl_global differential3 *dI_sd_shadow,

	ccl_global differential *du_sd,
	ccl_global differential *du_sd_dl,
	ccl_global differential *du_sd_shadow,

	ccl_global differential *dv_sd,
	ccl_global differential *dv_sd_dl,
	ccl_global differential *dv_sd_shadow,
	#endif
	#ifdef __DPDU__
	ccl_global float3 *dPdu_sd,
	ccl_global float3 *dPdu_sd_dl,
	ccl_global float3 *dPdu_sd_shadow,

	ccl_global float3 *dPdv_sd,
	ccl_global float3 *dPdv_sd_dl,
	ccl_global float3 *dPdv_sd_shadow,
	#endif
	ccl_global ShaderClosure *closure_sd,
	ccl_global ShaderClosure *closure_sd_dl,
	ccl_global ShaderClosure *closure_sd_shadow,

	ccl_global int *num_closure_sd,
	ccl_global int *num_closure_sd_dl,
	ccl_global int *num_closure_sd_shadow,

	ccl_global float *randb_closure_sd,
	ccl_global float *randb_closure_sd_dl,
	ccl_global float *randb_closure_sd_shadow,

	ccl_global float3 *ray_P_sd,
	ccl_global float3 *ray_P_sd_dl,
	ccl_global float3 *ray_P_sd_shadow,

	ccl_global differential3 *ray_dP_sd,
	ccl_global differential3 *ray_dP_sd_dl,
	ccl_global differential3 *ray_dP_sd_shadow,

	ccl_constant KernelData *data,
	ccl_global float *per_sample_output_buffers,
	ccl_global uint *rng_state,
	ccl_global uint *rng_coop,                   /* rng array to store rng values for all rays */
	ccl_global float3 *throughput_coop,          /* throughput array to store throughput values for all rays */
	ccl_global float *L_transparent_coop,        /* L_transparent array to store L_transparent values for all rays */
	ccl_global PathRadiance *PathRadiance_coop,  /* PathRadiance array to store PathRadiance values for all rays */
	ccl_global Ray *Ray_coop,                    /* Ray array to store Ray information for all rays */
	ccl_global PathState *PathState_coop,        /* PathState array to store PathState information for all rays */
	ccl_global char *ray_state,                  /* Stores information on current state of a ray */

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name,
#include "kernel_textures.h"

	int start_sample, int sx, int sy, int sw, int sh, int offset, int stride,
	int rng_state_offset_x,
	int rng_state_offset_y,
	ccl_global int *Queue_data,                  /* Memory for queues */
	ccl_global int *Queue_index,                 /* Tracks the number of elements in queues */
	int queuesize,                               /* size (capacity) of the queue */
	ccl_global char *use_queues_flag,            /* flag to decide if scene-intersect kernel should use queues to fetch ray index */
	ccl_global unsigned int *work_array,         /* work array to store which work each ray belongs to */
#ifdef __WORK_STEALING__
	ccl_global unsigned int *work_pool_wgs,      /* Work pool for each work group */
	unsigned int num_samples,                    /* Total number of samples per pixel */
#endif
	int parallel_samples                         /* Number of samples to be processed in parallel */
	)
{

	/* Load kernel globals structure */
	ccl_global KernelGlobals *kg = (ccl_global KernelGlobals *)globals;

	kg->data = data;
#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "kernel_textures.h"

	/* Load ShaderData structure */
	ccl_global ShaderData *sd = (ccl_global ShaderData *)shader_data_sd;
	ccl_global ShaderData *sd_dl = (ccl_global ShaderData *)shader_data_sd_dl;
	ccl_global ShaderData *sd_shadow = (ccl_global ShaderData *)shader_data_sd_shadow;

	sd->P = P_sd;
	sd_dl->P = P_sd_dl;
	sd_shadow->P = P_sd_shadow;

	sd->N = N_sd;
	sd_dl->N = N_sd_dl;
	sd_shadow->N = N_sd_shadow;

	sd->Ng = Ng_sd;
	sd_dl->Ng = Ng_sd_dl;
	sd_shadow->Ng = Ng_sd_shadow;

	sd->I = I_sd;
	sd_dl->I = I_sd_dl;
	sd_shadow->I = I_sd_shadow;

	sd->shader = shader_sd;
	sd_dl->shader = shader_sd_dl;
	sd_shadow->shader = shader_sd_shadow;

	sd->flag = flag_sd;
	sd_dl->flag = flag_sd_dl;
	sd_shadow->flag = flag_sd_shadow;

	sd->prim = prim_sd;
	sd_dl->prim = prim_sd_dl;
	sd_shadow->prim = prim_sd_shadow;

	sd->type = type_sd;
	sd_dl->type = type_sd_dl;
	sd_shadow->type = type_sd_shadow;

	sd->u = u_sd;
	sd_dl->u = u_sd_dl;
	sd_shadow->u = u_sd_shadow;

	sd->v = v_sd;
	sd_dl->v = v_sd_dl;
	sd_shadow->v = v_sd_shadow;

	sd->object = object_sd;
	sd_dl->object = object_sd_dl;
	sd_shadow->object = object_sd_shadow;

	sd->time = time_sd;
	sd_dl->time = time_sd_dl;
	sd_shadow->time = time_sd_shadow;

	sd->ray_length = ray_length_sd;
	sd_dl->ray_length = ray_length_sd_dl;
	sd_shadow->ray_length = ray_length_sd_shadow;

	sd->ray_depth = ray_depth_sd;
	sd_dl->ray_depth = ray_depth_sd_dl;
	sd_shadow->ray_depth = ray_depth_sd_shadow;

	sd->transparent_depth = transparent_depth_sd;
	sd_dl->transparent_depth = transparent_depth_sd_dl;
	sd_shadow->transparent_depth = transparent_depth_sd_shadow;

	#ifdef __RAY_DIFFERENTIALS__
	sd->dP = dP_sd;
	sd_dl->dP = dP_sd_dl;
	sd_shadow->dP = dP_sd_shadow;

	sd->dI = dI_sd;
	sd_dl->dI = dI_sd_dl;
	sd_shadow->dI = dI_sd_shadow;

	sd->du = du_sd;
	sd_dl->du = du_sd_dl;
	sd_shadow->du = du_sd_shadow;

	sd->dv = dv_sd;
	sd_dl->dv = dv_sd_dl;
	sd_shadow->dv = dv_sd_shadow;
	#ifdef __DPDU__
	sd->dPdu = dPdu_sd;
	sd_dl->dPdu = dPdu_sd_dl;
	sd_shadow->dPdu = dPdu_sd_shadow;

	sd->dPdv = dPdv_sd;
	sd_dl->dPdv = dPdv_sd_dl;
	sd_shadow->dPdv = dPdv_sd_shadow;
	#endif
	#endif

	sd->closure = closure_sd;
	sd_dl->closure = closure_sd_dl;
	sd_shadow->closure = closure_sd_shadow;

	sd->num_closure = num_closure_sd;
	sd_dl->num_closure = num_closure_sd_dl;
	sd_shadow->num_closure = num_closure_sd_shadow;

	sd->randb_closure = randb_closure_sd;
	sd_dl->randb_closure = randb_closure_sd_dl;
	sd_shadow->randb_closure = randb_closure_sd_shadow;

	sd->ray_P = ray_P_sd;
	sd_dl->ray_P = ray_P_sd_dl;
	sd_shadow->ray_P = ray_P_sd_shadow;

	sd->ray_dP = ray_dP_sd;
	sd_dl->ray_dP = ray_dP_sd_dl;
	sd_shadow->ray_dP = ray_dP_sd_shadow;

	int thread_index = get_global_id(1) * get_global_size(0) + get_global_id(0);

#ifdef __WORK_STEALING__
	int lid = get_local_id(1) * get_local_size(0) + get_local_id(0);
	/* Initialize work_pool_wgs */
	if(lid == 0) {
		int group_index = get_group_id(1) * get_num_groups(0) + get_group_id(0);
		work_pool_wgs[group_index] = 0;
	}
	barrier(CLK_LOCAL_MEM_FENCE);
#endif // __WORK_STEALING__

	/* Initialize queue data and queue index */
	if(thread_index < queuesize) {
		/* Initialize active ray queue */
		Queue_data[QUEUE_ACTIVE_AND_REGENERATED_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
		/* Initialize background and buffer update queue */
		Queue_data[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
		/* Initialize shadow ray cast of AO queue */
		Queue_data[QUEUE_SHADOW_RAY_CAST_AO_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
		/* Initialize shadow ray cast of direct lighting queue */
		Queue_data[QUEUE_SHADOW_RAY_CAST_DL_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
	}

	if(thread_index == 0) {
		Queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
		Queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
		Queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS] = 0;
		Queue_index[QUEUE_SHADOW_RAY_CAST_DL_RAYS] = 0;
		/* The scene-intersect kernel should not use the queues very first time.
		 * since the queue would be empty.
		 */
		use_queues_flag[0] = 0;
	}

	int x = get_global_id(0);
	int y = get_global_id(1);

	if(x < (sw * parallel_samples) && y < sh) {

		int ray_index = x + y * (sw * parallel_samples);

		/* This is the first assignment to ray_state; So we dont use ASSIGN_RAY_STATE macro */
		ray_state[ray_index] = RAY_ACTIVE;

		unsigned int my_sample;
		unsigned int pixel_x;
		unsigned int pixel_y;
		unsigned int tile_x;
		unsigned int tile_y;
		unsigned int my_sample_tile;

#ifdef __WORK_STEALING__
		unsigned int my_work = 0;
		/* get work */
		get_next_work(work_pool_wgs, &my_work, sw, sh, num_samples, parallel_samples, ray_index);
		/* Get the sample associated with the work */
		my_sample = get_my_sample(my_work, sw, sh, parallel_samples, ray_index) + start_sample;

		my_sample_tile = 0;

		/* Get pixel and tile position associated with the work */
		get_pixel_tile_position(&pixel_x, &pixel_y, &tile_x, &tile_y, my_work, sw, sh, sx, sy, parallel_samples, ray_index);
		work_array[ray_index] = my_work;
#else // __WORK_STEALING__

		unsigned int tile_index = ray_index / parallel_samples;
		tile_x = tile_index % sw;
		tile_y = tile_index / sw;
		my_sample_tile = ray_index - (tile_index * parallel_samples);
		my_sample = my_sample_tile + start_sample;

		/* Initialize work array */
		work_array[ray_index] = my_sample ;

		/* Calculate pixel position of this ray */
		pixel_x = sx + tile_x;
		pixel_y = sy + tile_y;
#endif // __WORK_STEALING__

		rng_state += (rng_state_offset_x + tile_x) + (rng_state_offset_y + tile_y) * stride;

		/* Initialise per_sample_output_buffers to all zeros */
		per_sample_output_buffers += (((tile_x + (tile_y * stride)) * parallel_samples) + (my_sample_tile)) * kernel_data.film.pass_stride;
		int per_sample_output_buffers_iterator = 0;
		for(per_sample_output_buffers_iterator = 0; per_sample_output_buffers_iterator < kernel_data.film.pass_stride; per_sample_output_buffers_iterator++) {
			per_sample_output_buffers[per_sample_output_buffers_iterator] = 0.0f;
		}

		/* initialize random numbers and ray */
		kernel_path_trace_setup_SPLIT_KERNEL(kg, rng_state, my_sample, pixel_x, pixel_y, &rng_coop[ray_index], &Ray_coop[ray_index]);

		if(Ray_coop[ray_index].t != 0.0f) {
			/* Initialize throuput, L_transparent, Ray, PathState; These rays proceed with path-iteration*/
			throughput_coop[ray_index] = make_float3(1.0f, 1.0f, 1.0f);
			L_transparent_coop[ray_index] = 0.0f;
			path_radiance_init(&PathRadiance_coop[ray_index], kernel_data.film.use_light_pass);
			path_state_init(kg, &PathState_coop[ray_index], &rng_coop[ray_index], my_sample, &Ray_coop[ray_index]);
			/* __KERNEL_DEBUG__ is disabled */
#ifdef __KERNEL_DEBUG__
			DebugData debug_data;
			debug_data_init(&debug_data);
#endif
		} else {
			/*These rays do not participate in path-iteration */

			float4 L_rad = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
			/* accumulate result in output buffer */
			kernel_write_pass_float4(per_sample_output_buffers, my_sample, L_rad);
			path_rng_end(kg, rng_state, rng_coop[ray_index]);

			ASSIGN_RAY_STATE(ray_state, ray_index, RAY_TO_REGENERATE);
		}
	}

	/* Mark rest of the ray-state indices as RAY_INACTIVE */
	if(thread_index < (get_global_size(0) * get_global_size(1)) - (sh * (sw * parallel_samples))) {
		ASSIGN_RAY_STATE(ray_state, (sw * parallel_samples) * sh + thread_index, RAY_INACTIVE);
	}
}
