/*
 * Copyright 2011-2016 Blender Foundation
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

CCL_NAMESPACE_BEGIN

/* Since the filtering may be performed across tile edged, all the neighboring tiles have to be passed along as well.
 * tile_x/y contain the x/y positions of the tile grid, 4 entries each:
 * - Start of the lower/left neighbor
 * - Start of the own tile
 * - Start of the upper/right neighbor
 * - Start of the next upper/right neighbor (not accessed)
 * buffers contains the nine buffer pointers (y-major ordering, starting with the lower left tile), offset and stride the respective parameters of the tile.
 */
ccl_device void kernel_filter_estimate_params(KernelGlobals *kg, int sample, float **buffers, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, FilterStorage *storage)
{
	/* TODO(lukas): Implement filter. */
}




ccl_device void kernel_filter_final_pass(KernelGlobals *kg, int sample, float **buffers, int x, int y, int *tile_x, int *tile_y, int *offset, int *stride, FilterStorage *storage)
{
	/* TODO(lukas): Implement filter. */
}

CCL_NAMESPACE_END
