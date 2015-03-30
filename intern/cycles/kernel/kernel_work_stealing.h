#ifndef __KERNEL_WORK_STEALING_H__
#define __KERNEL_WORK_STEALING_H__

/*
 * Utility functions for work stealing
 */

#ifdef __WORK_STEALING__

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

unsigned int get_group_id_with_ray_index(unsigned int ray_index,
                                         unsigned int tile_dim_x,
                                         unsigned int tile_dim_y,
                                         unsigned int parallel_samples,
                                         int dim) {
	unsigned int retval;
	if(dim == 0) {
		unsigned int x_span = ray_index % (tile_dim_x * parallel_samples);
		retval = x_span / get_local_size(0);
	}
	else if(dim == 1) {
		unsigned int y_span = ray_index / (tile_dim_x * parallel_samples);
		retval = y_span / get_local_size(1);
	}
	return retval;
}

unsigned int get_total_work(unsigned int tile_dim_x,
                            unsigned int tile_dim_y,
                            unsigned int grp_idx,
                            unsigned int grp_idy,
                            unsigned int num_samples) {
	unsigned int threads_within_tile_border_x;
	unsigned int threads_within_tile_border_y;

	threads_within_tile_border_x = (grp_idx == (get_num_groups(0) - 1)) ? tile_dim_x % get_local_size(0) : get_local_size(0);
	threads_within_tile_border_y = (grp_idy == (get_num_groups(1) - 1)) ? tile_dim_y % get_local_size(1) : get_local_size(1);

	threads_within_tile_border_x = (threads_within_tile_border_x == 0) ? get_local_size(0) : threads_within_tile_border_x;
	threads_within_tile_border_y = (threads_within_tile_border_y == 0) ? get_local_size(1) : threads_within_tile_border_y;

	return (threads_within_tile_border_x * threads_within_tile_border_y * num_samples);
}

/* Returns 0 in case there is no next work available */
/* Returns 1 in case work assigned is valid */
int get_next_work(__global unsigned int *work_pool,
                  __private unsigned int *my_work,
                  unsigned int tile_dim_x,
                  unsigned int tile_dim_y,
                  unsigned int num_samples,
                  unsigned int parallel_samples,
                  unsigned int ray_index) {

		unsigned int grp_idx = get_group_id_with_ray_index(ray_index, tile_dim_x, tile_dim_y, parallel_samples, 0);
		unsigned int grp_idy = get_group_id_with_ray_index(ray_index, tile_dim_x, tile_dim_y, parallel_samples, 1);
		unsigned int total_work = get_total_work(tile_dim_x, tile_dim_y, grp_idx, grp_idy, num_samples);
		unsigned int group_index = grp_idy * get_num_groups(0) + grp_idx;

		*my_work = atomic_inc(&work_pool[group_index]);

		int retval = (*my_work < total_work) ? 1 : 0;

		return retval;
}

/* This function assumes that the passed my_work is valid */
/* Decode sample number w.r.t. assigned my_work */
unsigned int get_my_sample(unsigned int my_work,
                           unsigned int tile_dim_x,
                           unsigned int tile_dim_y,
                           unsigned int parallel_samples,
                           unsigned int ray_index) {

	unsigned int grp_idx = get_group_id_with_ray_index(ray_index, tile_dim_x, tile_dim_y, parallel_samples, 0);
	unsigned int grp_idy = get_group_id_with_ray_index(ray_index, tile_dim_x, tile_dim_y, parallel_samples, 1);

	unsigned int threads_within_tile_border_x;
	unsigned int threads_within_tile_border_y;

	threads_within_tile_border_x = (grp_idx == (get_num_groups(0) - 1)) ? tile_dim_x % get_local_size(0) : get_local_size(0);
	threads_within_tile_border_y = (grp_idy == (get_num_groups(1) - 1)) ? tile_dim_y % get_local_size(1) : get_local_size(1);

	threads_within_tile_border_x = (threads_within_tile_border_x == 0) ? get_local_size(0) : threads_within_tile_border_x;
	threads_within_tile_border_y = (threads_within_tile_border_y == 0) ? get_local_size(1) : threads_within_tile_border_y;

	return (my_work / (threads_within_tile_border_x * threads_within_tile_border_y));
}

/* Decode pixel and tile position w.r.t. assigned my_work */
void get_pixel_tile_position(__private unsigned int *pixel_x,
                             __private unsigned int *pixel_y,
                             __private unsigned int *tile_x,
                             __private unsigned int *tile_y,
                             unsigned int my_work,
                             unsigned int tile_dim_x,
                             unsigned int tile_dim_y,
                             unsigned int tile_offset_x,
                             unsigned int tile_offset_y,
                             unsigned int parallel_samples,
                             unsigned int ray_index) {

	unsigned int grp_idx = get_group_id_with_ray_index(ray_index, tile_dim_x, tile_dim_y, parallel_samples, 0);
	unsigned int grp_idy = get_group_id_with_ray_index(ray_index, tile_dim_x, tile_dim_y, parallel_samples, 1);

	unsigned int threads_within_tile_border_x;
	unsigned int threads_within_tile_border_y;

	threads_within_tile_border_x = (grp_idx == (get_num_groups(0) - 1)) ? tile_dim_x % get_local_size(0) : get_local_size(0);
	threads_within_tile_border_y = (grp_idy == (get_num_groups(1) - 1)) ? tile_dim_y % get_local_size(1) : get_local_size(1);

	threads_within_tile_border_x = (threads_within_tile_border_x == 0) ? get_local_size(0) : threads_within_tile_border_x;
	threads_within_tile_border_y = (threads_within_tile_border_y == 0) ? get_local_size(1) : threads_within_tile_border_y;

	unsigned int total_associated_pixels = threads_within_tile_border_x * threads_within_tile_border_y;
	unsigned int work_group_pixel_index = my_work % total_associated_pixels;
	unsigned int work_group_pixel_x = work_group_pixel_index % threads_within_tile_border_x;
	unsigned int work_group_pixel_y = work_group_pixel_index / threads_within_tile_border_x;

	*pixel_x = tile_offset_x + (grp_idx * get_local_size(0)) + work_group_pixel_x;
	*pixel_y = tile_offset_y + (grp_idy * get_local_size(1)) + work_group_pixel_y;
	*tile_x = *pixel_x - tile_offset_x;
	*tile_y = *pixel_y - tile_offset_y;
}
#endif // __WORK_STEALING__
#endif // __KERNEL_WORK_STEALING_H__
