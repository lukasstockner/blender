/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_PIXELFILTER_H__
#define __RENDER_PIXELFILTER_H__

struct Render;
struct RenderSampleData;

/* Pixel Filter
 *
 * - sample: value from 0..RE_MAX_OSA-1, subsample in a pixel
 * - mask: RE_MAX_OSA bits indicating if some sample is active */

void pxf_init(struct Render *re);
void pxf_free(struct Render *re);

int pxf_mask_count(struct RenderSampleData *rsd, unsigned short mask);

void pxf_mask_offset(struct RenderSampleData *rsd, unsigned short mask, float ofs[2]);
void pxf_sample_offset(struct RenderSampleData *rsd, int sample, float ofs[2]);
float (*pxf_sample_offset_table(struct Render *re))[2];

/* Alpha Over / Alpha Under / Additive Blending */

void pxf_add_alpha_over(float dest[4], float source[4]);
void pxf_add_alpha_under(float dest[4], float source[4]);
void pxf_add_alpha_fac(float dest[4], float source[4], char addfac);

/* Alpha Over with Mask */

void pxf_add_alpha_over_mask(struct RenderSampleData *rsd,
	float *dest, float *source, unsigned short dmask, unsigned short smask);

/* Filtered Blending */

void pxf_add_filtered(struct RenderSampleData *rsd, unsigned short mask,
	float *col, float *rowbuf, int row_w);
void pxf_add_filtered_pixsize(struct RenderSampleData *rsd, unsigned short mask,
	float *in, float *rowbuf, int row_w, int pixsize);

/* Filtered Blending with Premade Table */

void pxf_mask_table(struct RenderSampleData *rsd, unsigned short mask,
	float filt[3][3]);
void pxf_add_filtered_table(float filt[3][3], float *col, float *rowbuf, int row_w,
	int col_h, int x, int y);

#endif /* __RENDER_PIXELFILTER_H__ */

