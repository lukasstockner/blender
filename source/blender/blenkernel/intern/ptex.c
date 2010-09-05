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
#include "BKE_mesh.h"
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

void ptex_elem_to_floats(int type, int channels, void *data, float *out)
{
	int i;

	switch(type) {
	case PTEX_DT_UINT8:
		for(i = 0; i < channels; ++i)
			out[i] = ((unsigned char*)data)[i] / 255.0;
		break;
	case PTEX_DT_UINT16:
		for(i = 0; i < channels; ++i)
			out[i] = ((unsigned char*)data)[i] / 65535.0;
		break;
	case PTEX_DT_FLOAT:
		for(i = 0; i < channels; ++i)
			out[i] = ((float*)data)[i];
		break;
	default:
		break;
	}
}

void ptex_elem_from_floats(int type, int channels, void *data, float *in)
{
	int i;

	switch(type) {
	case PTEX_DT_UINT8:
		for(i = 0; i < channels; ++i)
			((unsigned char*)data)[i] = in[i] * 255;
		break;
	case PTEX_DT_UINT16:
		for(i = 0; i < channels; ++i)
			((unsigned short*)data)[i] = in[i] * 65535;
		break;
	case PTEX_DT_FLOAT:
		for(i = 0; i < channels; ++i)
			((float*)data)[i] = in[i];
		break;
	default:
		break;
	}
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
	ptex_elem_from_floats(pt->type, pt->channels, out, tmp);
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

/* copy data to/from ptex file format and internal MPtex format */
static void ptex_transfer_filedata(MPtex *pt, int offset, char *file_data_start, int from_file)
{
	char *mptex_data, *file_data;
	char **src, **dest;
	int file_res[2], file_half_res[2];
	int i, u, v, layersize;

	layersize = pt->channels * ptex_data_size(pt->type);

	if(pt->totsubface == 4) {
		file_res[0] = pt->subfaces[1].res[0] << 1;
		file_res[1] = pt->subfaces[1].res[1] << 1;
	}
	else {
		file_res[0] = pt->subfaces[offset].res[1];
		file_res[1] = pt->subfaces[offset].res[0];
	}

	file_half_res[0] = file_res[0] >> 1;
	file_half_res[1] = file_res[1] >> 1;

	if(from_file) {
		src = &file_data;
		dest = &mptex_data;
	}
	else {
		src = &mptex_data;
		dest = &file_data;
	}

	if(pt->totsubface == 4) {
		/* save quad subfaces as one face */

		for(i = 0; i < 4; ++i) {
			MPtexSubface *subface = &pt->subfaces[i];
			int file_center_offset[2], file_step, file_row_step;

			switch(i) {
			case 0:
				file_center_offset[0] = -1;
				file_center_offset[1] = -1;
				file_step = -file_res[0];
				file_row_step = file_res[0] * file_half_res[1] - 1;
				break;
			case 1:
				file_center_offset[0] = 0;
				file_center_offset[1] = -1;
				file_step = 1;
				file_row_step = -file_res[0] - file_half_res[0];
				break;
			case 2:
				file_center_offset[0] = 0;
				file_center_offset[1] = 0;
				file_step = file_res[0];
				file_row_step = -file_res[0] * file_half_res[1] + 1;
				break;
			case 3:
				file_center_offset[0] = -1;
				file_center_offset[1] = 0;
				file_step = -1;
				file_row_step = file_res[0] + file_half_res[0];
				break;
			}

			mptex_data = subface->data;
			file_data = file_data_start +
				layersize * (file_res[0] * (file_half_res[1]+file_center_offset[1]) +
					     file_half_res[0]+file_center_offset[0]);

			for(v = 0; v < subface->res[1]; ++v) {
				for(u = 0; u < subface->res[0]; ++u) {
					memcpy(*dest, *src, layersize);
					mptex_data += layersize;
					file_data += layersize * file_step;
				}
				file_data += layersize * file_row_step;
			}
		}
	}
	else {
		mptex_data = pt->subfaces[offset].data;
		file_data = file_data_start;

		for(v = 0; v < file_res[1]; ++v) {
			for(u = 0; u < file_res[0]; ++u) {
				mptex_data = (char*)pt->subfaces[offset].data +
					layersize * ((file_res[0] - u - 1) * file_res[1] +
						     (file_res[1] - v - 1));
				file_data = file_data_start + layersize * (v*file_res[0]+u);

				memcpy(*dest, *src, layersize);
			}
		}
	}
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
			int l, file_res[2], file_half_res[2], faceid;
			char *filedata;

			faceid = j+k;
			
			ptex_face = ptex_texture_get_face_info(ptex_texture, faceid);
			ptex_res = ptex_face_info_get_res(ptex_face);

			file_res[0] = ptex_res_u(ptex_res);
			file_res[1] = ptex_res_v(ptex_res);
			file_half_res[0] = file_res[0] >> 1;
			file_half_res[1] = file_res[1] >> 1;

			filedata = MEM_callocN(layersize * file_res[0] * file_res[1], "Ptex data from file");
			ptex_texture_get_data(ptex_texture, faceid, filedata, 0, ptex_res);

			/* allocate mem for ptex data, set subface resolutions */
			if(S==4) {
				int ures, vres;

				/* use quarter resolution for quad subfaces */
				ures = file_half_res[0];
				vres = file_half_res[1];

				/* TODO: handle 1xV and Ux1 inputs */
				assert(ures > 0 && vres > 0);

				for(l = 0; l < 4; ++l) {
					SWAP(int, ures, vres);

					mptex[i].subfaces[l].res[0] = ures;
					mptex[i].subfaces[l].res[1] = vres;
					mptex[i].subfaces[l].data =
						MEM_callocN(layersize * ures * vres,
							    "Ptex quad data from file");
				}


			}
			else {
				mptex[i].subfaces[k].res[0] = file_res[1];
				mptex[i].subfaces[k].res[1] = file_res[0];
				mptex[i].subfaces[k].data =
					MEM_callocN(layersize * file_res[0] * file_res[1],
						    "Ptex tri data from file");
			}

			ptex_transfer_filedata(&mptex[i], k, filedata, 1);
			MEM_freeN(filedata);
		}

		j += file_totsubface;
	}

	/* data is all copied, can release ptex file */
	ptex_texture_release(ptex_texture);
}

int ptex_layer_save_file(struct Mesh *me, const char *filename)
{
	MPtex *mptex;
	PtexWriterHandle *ptex_writer;
	char *file_data;
	int i, j, totface, faceid;

	mptex = CustomData_get_layer(&me->fdata, CD_MPTEX);

	for(i = 0, totface = 0; i < me->totface; ++i)
		totface += (me->mface[i].v4 ? 4 : 3);

	ptex_writer = ptex_writer_open(filename, mptex->type, mptex->channels, 0, totface, 1);
	if(!ptex_writer)
		return -1;

	for(i = 0, faceid = 0; i < me->totface; ++i, ++mptex) {
		PtexFaceInfoHandle *face_info;
		int adjfaces[4] = {0,0,0,0}, adjedges[4] = {0,0,0,0};
		int layersize;

		layersize = ptex_data_size(mptex->type) * mptex->channels;

		/* TODO: adjacency data (needed for filtering) */

		if(mptex->totsubface == 4) {
			int res[2];

			res[0] = mptex->subfaces[1].res[0]*2;
			res[1] = mptex->subfaces[1].res[1]*2;

			file_data = MEM_callocN(res[0] * res[1] * layersize,
						"mptex save quad data");

			ptex_transfer_filedata(mptex, 0, file_data, 0);

			face_info = ptex_face_info_new(res[0], res[1],
						       adjfaces, adjedges, 0);
			ptex_writer_write_face(ptex_writer, faceid, face_info, file_data, 0);
			faceid += 1;

			MEM_freeN(file_data);
		}
		else if(mptex->totsubface == 3) {
			for(j = 0; j < mptex->totsubface; ++j, ++faceid) {
				MPtexSubface *subface = &mptex->subfaces[j];

				file_data =
					MEM_callocN(subface->res[0] * subface->res[1] * layersize,
						    "mptex save subface data");

				ptex_transfer_filedata(mptex, j, file_data, 0);

				face_info = ptex_face_info_new(subface->res[1],
							       subface->res[0],
							       adjfaces, adjedges, 1);
				ptex_writer_write_face(ptex_writer, faceid, face_info,
						       file_data, 0);

				MEM_freeN(file_data);
			}
		}
	}

	ptex_writer_release(ptex_writer);

	return 0;
}
