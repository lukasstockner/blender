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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_dmgrid.h"
#include "BKE_subsurf.h"

#include "BLI_math.h"

#include "ptex.h"

#include <assert.h>

DerivedMesh *quad_dm_create_from_derived(DerivedMesh *dm)
{
	DerivedMesh *ccgdm;
	SubsurfModifierData smd;
	GridKey gridkey;
	
	memset(&smd, 0, sizeof(SubsurfModifierData));
	smd.levels = 1;
	smd.subdivType = ME_SIMPLE_SUBSURF;
	GRIDELEM_KEY_INIT(&gridkey, 1, 0, 0, 1);
	ccgdm = subsurf_make_derived_from_derived(dm, &smd, &gridkey,
						  0, NULL, 0, 0);

	return ccgdm;
}

static void ptex_elem_to_floats_mul_add(MPtex *pt, void *data, float *out, float fac)
{
	int i;

	switch(pt->type) {
	case PTEX_DT_UINT8:
		for(i = 0; i < pt->channels; ++i)
			out[i] += (((unsigned char*)data)[i] / 255.0) * fac;
		break;
	case PTEX_DT_UINT16:
		for(i = 0; i < pt->channels; ++i)
			out[i] += (((unsigned char*)data)[i] / 65535.0) * fac;
		break;
	case PTEX_DT_FLOAT:
		for(i = 0; i < pt->channels; ++i)
			out[i] += ((float*)data)[i] * fac;
		break;
	default:
		break;
	}
}

static void ptex_elem_from_floats(MPtex *pt, void *data, float *in)
{
	int i;

	switch(pt->type) {
	case PTEX_DT_UINT8:
		for(i = 0; i < pt->channels; ++i)
			((unsigned char*)data)[i] = in[i] * 255;
		break;
	case PTEX_DT_UINT16:
		for(i = 0; i < pt->channels; ++i)
			((unsigned short*)data)[i] = in[i] * 65535;
		break;
	case PTEX_DT_FLOAT:
		for(i = 0; i < pt->channels; ++i)
			((float*)data)[i] = in[i];
		break;
	default:
		break;
	}
}

/* get interpolated value for one texel */
static void ptex_bilinear_interp(MPtex *pt, MPtexSubface *subface,
				 void *out, int layersize,
				 float x, float y, float *tmp)
{
	char *input_start = subface->data;
	int rowlen = subface->res[0];
	int xi = (int)x;
	int yi = (int)y;
	int xt = xi+1, yt = yi+1;
	float s = x - xi;
	float t = y - yi;
	float u = 1 - s;
	float v = 1 - t;

	if(xt == subface->res[0])
		--xt;
	if(yt == subface->res[1])
		--yt;

	memset(tmp, 0, sizeof(float)*pt->channels);
	ptex_elem_to_floats_mul_add(pt, input_start + layersize * (yi*rowlen+xi), tmp, u*v);
	ptex_elem_to_floats_mul_add(pt, input_start + layersize * (yi*rowlen+xt), tmp, s*v);
	ptex_elem_to_floats_mul_add(pt, input_start + layersize * (yt*rowlen+xt), tmp, s*t);
	ptex_elem_to_floats_mul_add(pt, input_start + layersize * (yt*rowlen+xi), tmp, u*t);
	ptex_elem_from_floats(pt, out, tmp);
}

/* interpolate subface to new resolution */
void ptex_subface_scale(MPtex *pt, MPtexSubface *subface, int ures, int vres)
{
	float ui, vi, ui_step, vi_step;
	float *tmp;
	char *new_data, *new_data_start;
	int u, v, layersize;

	layersize = pt->channels * ptex_data_size(pt->type);

	new_data_start = new_data =
		MEM_callocN(layersize * ures * vres, "ptex_subface_scale.new_data");

	/* tmp buffer used in interpolation */
	tmp = MEM_callocN(sizeof(float) * pt->channels, "ptex_subface_scale.tmp");

	ui_step = subface->res[0] / (float)ures;
	vi_step = subface->res[1] / (float)vres;
	for(v = 0, vi = 0; v < vres; ++v, vi += vi_step) {
		for(u = 0, ui = 0; u < ures; ++u, ui += ui_step, new_data += layersize) {
			ptex_bilinear_interp(pt, subface, new_data, layersize, ui, vi, tmp);
		}
	}

	MEM_freeN(subface->data);
	subface->data = new_data_start;

	subface->res[0] = ures;
	subface->res[1] = vres;

	MEM_freeN(tmp);
}

/* creates a new CD_MPTEX layer and loads ptex_texture into it */
void ptex_layer_from_file(Mesh *me, PtexTextureHandle *ptex_texture)
{
	MPtex *mptex;
	PtexDataType ptex_data_type;
	int channels;
	int i, j, layersize, active_offset;

	channels = ptex_texture_num_channels(ptex_texture);
	ptex_data_type = ptex_texture_data_type(ptex_texture);

	/* number of bytes for one ptex element */
	layersize = ptex_data_size(ptex_data_type) * channels;

	active_offset = CustomData_number_of_layers(&me->fdata, CD_MPTEX);
	mptex = CustomData_add_layer(&me->fdata, CD_MPTEX, CD_CALLOC,
				     NULL, me->totface);
	CustomData_set_layer_active(&me->fdata, CD_MPTEX, active_offset);

	for(i = 0, j = 0; i < me->totface; ++i) {
		int S = me->mface[i].v4 ? 4 : 3;
		int k, file_totsubface;

		mptex[i].type = ptex_data_type;
		mptex[i].channels = channels;
		mptex[i].totsubface = S;

		/* quads don't have subfaces in ptex files */
		file_totsubface = (S==4)? 1 : S;

		for(k = 0; k < file_totsubface; ++k) {
			PtexFaceInfoHandle *ptex_face;
			PtexResHandle *ptex_res;
			int l, u, v, file_res[2], file_half_res[2], faceid;
			char *filedata;

			faceid = j+k;
			
			ptex_face = ptex_texture_get_face_info(ptex_texture, faceid);
			ptex_res = ptex_face_get_res(ptex_face);

			file_res[0] = ptex_res_u(ptex_res);
			file_res[1] = ptex_res_v(ptex_res);
			file_half_res[0] = file_res[0] >> 1;
			file_half_res[1] = file_res[1] >> 1;

			filedata = MEM_callocN(layersize * file_res[0] * file_res[1], "Ptex data from file");
			ptex_texture_get_data(ptex_texture, faceid, filedata, 0, ptex_res);

			if(S==4) {
				int ures, vres;

				/* use quarter resolution for quad subfaces */
				ures = file_half_res[0];
				vres = file_half_res[1];

				/* TODO: handle 1xV and Ux1 inputs */
				assert(ures > 0 && vres > 0);

				for(l = 0; l < 4; ++l) {
					char *dest, *src = filedata;
					int src_center_offset[2], src_step, src_row_step;

					SWAP(int, ures, vres);

					mptex[i].subfaces[l].res[0] = ures;
					mptex[i].subfaces[l].res[1] = vres;
					dest = mptex[i].subfaces[l].data =
						MEM_callocN(layersize * ures * vres,
							    "Ptex quad data from file");

					switch(l) {
					case 0:
						src_center_offset[0] = -1;
						src_center_offset[1] = -1;
						src_step = -file_res[0];
						src_row_step = file_res[0] * file_half_res[1] - 1;
						break;
					case 1:
						src_center_offset[0] = 0;
						src_center_offset[1] = -1;
						src_step = 1;
						src_row_step = -file_res[0] - file_half_res[0];
						break;
					case 2:
						src_center_offset[0] = 0;
						src_center_offset[1] = 0;
						src_step = file_res[0];
						src_row_step = -file_res[0] * file_half_res[1] + 1;
						break;
					case 3:
						src_center_offset[0] = -1;
						src_center_offset[1] = 0;
						src_step = -1;
						src_row_step = file_res[0] + file_half_res[0];
						break;
					}

					src += layersize * (file_res[0] * (file_half_res[1]+src_center_offset[1]) +
							    file_half_res[0]+src_center_offset[0]);

					for(v = 0; v < vres; ++v) {
						for(u = 0; u < ures; ++u) {
							memcpy(dest, src, layersize);
							dest += layersize;
							src += layersize * src_step;
						}
						src += layersize * src_row_step;
					}
				}
			}
			else {
				mptex[i].subfaces[k].res[0] = file_res[1];
				mptex[i].subfaces[k].res[1] = file_res[0];
				mptex[i].subfaces[k].data = MEM_callocN(layersize * file_res[0] * file_res[1],
								       "Ptex tri data from file");
			
				for(v = 0; v < file_res[1]; ++v) {
					for(u = 0; u < file_res[0]; ++u) {
						memcpy((char*)mptex[i].subfaces[k].data +
						       layersize * ((file_res[0] - u - 1)*file_res[1]+ (file_res[1] - v - 1)),
						       filedata + layersize * (v*file_res[0]+u),
						       layersize);
					}
				}
			}

			MEM_freeN(filedata);
		}

		j += file_totsubface;
	}

	/* data is all copied, can release ptex file */
	ptex_texture_release(ptex_texture);
}
