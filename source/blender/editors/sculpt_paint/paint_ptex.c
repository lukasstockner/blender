#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_dmgrid.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_subsurf.h"

#include "BLI_math.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"
#include "ptex.h"

#include <assert.h>
#include <stdlib.h>

static int next_power_of_two(int n)
{
	n--;
	n = (n >> 1) | n;
	n = (n >> 2) | n;
	n = (n >> 4) | n;
	n = (n >> 8) | n;
	n = (n >> 16) | n;
	n++;

	return n;
}

static const void *ptex_default_data(PtexDataType type) {
	static const unsigned char ptex_def_val_uc[] = {255, 255, 255, 255};
	static const unsigned short ptex_def_val_us[] = {65535, 65535, 65535, 65535};
	static const float ptex_def_val_f[] = {1, 1, 1, 1};

	switch(type) {
	case PTEX_DT_UINT8:
		return ptex_def_val_uc;
	case PTEX_DT_UINT16:
		return ptex_def_val_us;
	case PTEX_DT_FLOAT:
		return ptex_def_val_f;
	default:
		return NULL;
	};
}

/* add a new ptex layer
   automatically sets resolution based on face area */
static int ptex_layer_add_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	MPtex *mptex;
	float (*limit_pos)[3], *face_area, totarea;
	float density;
	float largest_face_area = 0;
	const void *def_val;
	PtexDataType type;
	int totchannel;
	int layer_size;
	int tottexel = 0;
	int i, j;

	type = RNA_enum_get(op->ptr, "type");
	totchannel = RNA_int_get(op->ptr, "channels");
	layer_size = ptex_data_size(type) * totchannel;
	def_val = ptex_default_data(type);

	mptex = CustomData_add_layer(&me->fdata, CD_MPTEX, CD_CALLOC,
				     NULL, me->totface);

	/* TODO: for now i'm allocating texels based on limit surface area;
	   according to ptex paper it's better to use surface derivatives */

	limit_pos = MEM_callocN(sizeof(float)*3*me->totvert, "limit_pos");
	face_area = MEM_callocN(sizeof(float)*me->totface, "face_area");
	subsurf_calculate_limit_positions(me, limit_pos);
	for(i = 0, totarea = 0; i < me->totface; ++i) {
		MFace *f = &me->mface[i];
		if(f->v4) {
			face_area[i] = area_quad_v3(limit_pos[f->v1], limit_pos[f->v2],
						    limit_pos[f->v3], limit_pos[f->v4]);
		}
		else {
			face_area[i] = area_tri_v3(limit_pos[f->v1], limit_pos[f->v2],
						   limit_pos[f->v3]);
		}
		largest_face_area = MAX2(largest_face_area, face_area[i]);
		totarea += face_area[i];
	}

	/* try to make the density factor less dependent on mesh size */
	density = RNA_float_get(op->ptr, "density") * 1000 / largest_face_area;
	
	for(i = 0; i < me->totface; ++i) {
		int S = me->mface[i].v4 ? 4 : 3;
		int ures;
		int vres;
		int gridsize;
		char *data;

		if(S == 4) {
			/* adjust u and v resolution by the ration
			   between the average edge size in u and v
			   directions */
			float len1 = (len_v3v3(limit_pos[me->mface[i].v1],
					       limit_pos[me->mface[i].v2]) +
				      len_v3v3(limit_pos[me->mface[i].v3],
					       limit_pos[me->mface[i].v4])) * 0.5f;
			float len2 = (len_v3v3(limit_pos[me->mface[i].v2],
					       limit_pos[me->mface[i].v3]) +
				      len_v3v3(limit_pos[me->mface[i].v4],
					       limit_pos[me->mface[i].v1])) * 0.5f;
			float r = len2/len1;

			ures = next_power_of_two(sqrtf((face_area[i] * density) * r)) / 2;
			vres = next_power_of_two(sqrtf((face_area[i] * density) / r)) / 2;
		}
		else {
			/* do triangles uniform (subfaces) */
			ures = sqrtf(face_area[i] * (density / 3.0f));
			vres = ures = next_power_of_two(ures);
		}

		ures = MAX2(ures, 1);
		vres = MAX2(vres, 1);
		gridsize = ures * vres;

		mptex[i].totsubface = S;
		mptex[i].type = type;
		mptex[i].channels = totchannel;

		for(j = 0; j < S; ++j) {
			int texels, k;

			mptex[i].subfaces[j].res[0] = ures;
			mptex[i].subfaces[j].res[1] = vres;

			texels = ures*vres;
			data = mptex[i].subfaces[j].data =
				MEM_callocN(layer_size * texels, "MptexSubface.data");
			tottexel += texels;

			for(k = 0; k < texels; ++k) {
				memcpy(data, def_val, layer_size);
				data += layer_size;
			}
		}
	}

	printf("total texels = %d, sqrt(texels)=%.1f\n", tottexel, sqrtf(tottexel));

	MEM_freeN(face_area);
	MEM_freeN(limit_pos);

	return OPERATOR_FINISHED;
}

void PTEX_OT_layer_add(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[]= {
		{PTEX_DT_UINT8, "PTEX_DT_UINT8", 0, "8-bit channels", ""},
		{PTEX_DT_UINT16, "PTEX_DT_UINT16", 0, "16-bit channels", ""},
		{PTEX_DT_FLOAT, "PTEX_DT_FLOAT", 0, "32-bit floating-point channels", ""},

		{0, NULL, 0, NULL, NULL}};
	
	/* identifiers */
	ot->name= "Add Layer";
	ot->description= "Add a new ptex layer";
	ot->idname= "PTEX_OT_layer_add";
	
	/* api callbacks */
	ot->exec= ptex_layer_add_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float(ot->srna, "density", 10, 0, 6000, "Density", "Density of texels to generate", 0, 6000);
	RNA_def_int(ot->srna, "channels", 3, 1, 4, "Channels", "", 1, 4);
	RNA_def_enum(ot->srna, "type", type_items, PTEX_DT_FLOAT, "Type", "Layer channels and data type");
}

/* loads a .ptx file
   makes some fairly strict assumptions that could
   be relaxed later as our ptex implementation is refined

   on the other hand, some unsupported ptex features
   are not checked for yet
*/
int ptex_open_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	MPtex *mptex;

	PtexTextureHandle *ptex_texture;
	PtexDataType ptex_data_type;
	int totchannel, layersize, active_offset;

	char *path;
	int i, j;
	
	path = RNA_string_get_alloc(op->ptr, "filepath", NULL, 0);

	ptex_texture = ptex_open(path, 1, 0);
	MEM_freeN(path);

	/* check if loader worked */
	if(!ptex_texture) {
		BKE_report(op->reports, RPT_ERROR, "Error loading ptex file (see stdout for now, TODO)");
		return OPERATOR_CANCELLED;
	}

	/* data type */
	ptex_data_type = ptex_texture_data_type(ptex_texture);
	if(ptex_data_type == PTEX_DT_UNSUPPORTED) {
		BKE_report(op->reports, RPT_ERROR, "Ptex format unsupported");
		ptex_texture_release(ptex_texture);
		return OPERATOR_CANCELLED;
	}

	/* data channels */
	totchannel = ptex_texture_num_channels(ptex_texture);
	if(totchannel == 2 || totchannel > 4) {
		BKE_report(op->reports, RPT_ERROR, "Ptex channel count unsupported");
		ptex_texture_release(ptex_texture);
		return OPERATOR_CANCELLED;
	}

	/* check that ptex file matches mesh topology */
	for(i = 0, j = 0; i < me->totface; ++i) {
		MFace *f = &me->mface[i];
		PtexFaceInfoHandle *ptex_face = ptex_texture_get_face_info(ptex_texture, j);
		int subface;

		if(!ptex_face) {
			BKE_report(op->reports, RPT_ERROR, "Ptex/mesh topology mismatch");
			ptex_texture_release(ptex_texture);
			return OPERATOR_CANCELLED;
		}

		subface = ptex_face_info_is_subface(ptex_face);

		if(subface != (f->v4 == 0)) {
			BKE_report(op->reports, RPT_ERROR, "Ptex/mesh topology mismatch");
			ptex_texture_release(ptex_texture);
			return OPERATOR_CANCELLED;
		}

		j += (f->v4 ? 1 : 3);
	}

	/* number of bytes for one ptex element */
	layersize = ptex_data_size(ptex_data_type) * totchannel;

	active_offset = CustomData_number_of_layers(&me->fdata, CD_MPTEX);
	mptex = CustomData_add_layer(&me->fdata, CD_MPTEX, CD_CALLOC,
				     NULL, me->totface);
	CustomData_set_layer_active(&me->fdata, CD_MPTEX, active_offset);

	for(i = 0, j = 0; i < me->totface; ++i) {
		int S = me->mface[i].v4 ? 4 : 3;
		int k, file_totsubface;

		mptex[i].type = ptex_data_type;
		mptex[i].channels = totchannel;
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
	
	return OPERATOR_FINISHED;
}

static int ptex_open_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	WM_event_add_fileselect(C, op);
	return OPERATOR_RUNNING_MODAL;
}

void PTEX_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Open";
	ot->idname= "PTEX_OT_open";
	
	/* api callbacks */
	ot->exec= ptex_open_exec;
	ot->invoke= ptex_open_invoke;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH|WM_FILESEL_RELPATH);
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
static void ptex_subface_scale(MPtex *pt, MPtexSubface *subface,
			       int ures, int vres)
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

typedef enum {
	RES_OP_NUMERIC,
	RES_OP_DOUBLE,
	RES_OP_HALF
} PtexResOp;

static void ptex_face_resolution_set(MPtex *pt, int offset, ToolSettings *ts, PtexResOp op)
{
	int i;

	for(i = 0; i < pt->totsubface; ++i) {
		int ures, vres;

		if(i == offset || pt->totsubface == 4) {
			MPtexSubface *subface = &pt->subfaces[i];

			switch(op) {
			case RES_OP_NUMERIC:
				ures = ts->ptex_ures;
				vres = ts->ptex_vres;
				if(pt->totsubface == 4) {
					ures >>= 1;
					vres >>= 1;
				}
				break;
			case RES_OP_DOUBLE:
				ures = subface->res[0] << 1;
				vres = subface->res[1] << 1;
				break;
			case RES_OP_HALF:
				ures = subface->res[0] >> 1;
				vres = subface->res[1] >> 1;
				break;
			}

			if(ures < 1) ures = 1;
			if(vres < 1) vres = 1;

			ptex_subface_scale(pt, subface, ures, vres);
		}
	}
}

static int ptex_face_resolution_set_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	MPtex *mptex = CustomData_get_layer(&me->fdata, CD_MPTEX);
	PtexResOp operation = RNA_enum_get(op->ptr, "operation");
	int i, j;

	for(i = 0; i < me->totface; ++i) {
		for(j = 0; j < mptex[i].totsubface; ++j) {
			if(mptex[i].subfaces[j].flag & MPTEX_SUBFACE_SELECTED) {
				ptex_face_resolution_set(mptex + i, j, ts, operation);
				if(mptex[i].totsubface == 4)
					break;
			}
		}
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

static int ptex_face_resolution_set_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	if(ob) {
		Mesh *me = get_mesh(ob);
		if(me && CustomData_get_layer(&me->fdata, CD_MPTEX))
			return 1;
	}
	return 0;
}

void PTEX_OT_face_resolution_set(wmOperatorType *ot)
{
	static EnumPropertyItem op_items[] = {
		{RES_OP_NUMERIC, "NUMERIC", 0, "Numeric", ""},
		{RES_OP_DOUBLE, "DOUBLE", 0, "Double", ""},
		{RES_OP_HALF, "HALF", 0, "Half", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Set Face Resolution";
	ot->idname= "PTEX_OT_face_resolution_set";
	
	/* api callbacks */
	ot->exec= ptex_face_resolution_set_exec;
	ot->poll= ptex_face_resolution_set_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "operation", op_items, RES_OP_NUMERIC, "Operation", "How to modify the resolution");
}

typedef struct {
	int grid_index;
	PBVHNode *node;
} PtexSelectData;

static void select_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
	if(BLI_pbvh_node_get_tmin(node) < *tmin) {
		PaintStrokeRaycastData *data = data_v;
		PtexSelectData *mode_data = data->mode_data;
		
		if(BLI_pbvh_node_raycast(data->ob->paint->pbvh, node, NULL,
					 data->ray_start, data->ray_normal,
					 &data->dist, &mode_data->grid_index, NULL)) {
			data->hit |= 1;
			*tmin = data->dist;
			mode_data->node = node;
		}
	}
}

static int ptex_subface_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	ViewContext vc;
	float out[3], mouse[2] = {event->x, event->y};
	PtexSelectData mode_data;

	view3d_set_viewcontext(C, &vc);
	if(paint_util_raycast(&vc, select_raycast_cb, &mode_data, out, mouse, 0)) {
		PBVH *pbvh = ob->paint->pbvh;
		GridToFace *grid_face_map, *gtf;
		CustomData *fdata;
		MPtex *mptex, *pt;
		int *grid_indices;
		int i, j;

		grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);
		BLI_pbvh_get_customdata(pbvh, NULL, &fdata);
		BLI_pbvh_node_get_grids(pbvh, mode_data.node,
					&grid_indices, NULL, NULL, NULL,
					NULL, NULL, NULL);

		mptex = CustomData_get_layer(fdata, CD_MPTEX);

		/* deselect everything */
		if(!RNA_boolean_get(op->ptr, "extend")) {
			for(i = 0; i < me->totface; ++i) {
				for(j = 0; j < mptex[i].totsubface; ++j)
					mptex[i].subfaces[j].flag &= ~MPTEX_SUBFACE_SELECTED;
			}
		}

		gtf = &grid_face_map[grid_indices[mode_data.grid_index]];		
		pt = &mptex[gtf->face];

		if(pt->totsubface == 4) {
			for(i = 0; i < 4; ++i)
				pt->subfaces[i].flag ^= MPTEX_SUBFACE_SELECTED;
		}
		else
			pt->subfaces[gtf->offset].flag ^= MPTEX_SUBFACE_SELECTED;

		me->act_face = gtf->face;
		me->act_subface = gtf->offset;

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}

	return OPERATOR_FINISHED;
}

static int ptex_select_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if(ob && (ob->mode & OB_MODE_VERTEX_PAINT)) {
		Mesh *me = get_mesh(ob);
		return me && (me->editflag & ME_EDIT_PTEX);
	}

	return 0;
}

void PTEX_OT_subface_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Subface";
	ot->idname= "PTEX_OT_subface_select";
	
	/* api callbacks */
	ot->invoke= ptex_subface_select_invoke;
	ot->poll= ptex_select_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "");
}

static int ptex_select_all_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	MPtex *mptex;
	int i, j, action = RNA_enum_get(op->ptr, "action");

	mptex = CustomData_get_layer(&me->fdata, CD_MPTEX);
	
	if(action == SEL_TOGGLE) {
		for(i = 0; i < me->totface; ++i) {
			for(j = 0; j < mptex[i].totsubface; ++j) {
				if(mptex[i].subfaces[j].flag & MPTEX_SUBFACE_SELECTED) {
					action = SEL_DESELECT;
					break;
				}
			}
		}
	}

	if(action == SEL_TOGGLE)
		action = SEL_SELECT;

	for(i = 0; i < me->totface; ++i) {
		for(j = 0; j < mptex[i].totsubface; ++j) {
			MPtexSubface *subface = &mptex[i].subfaces[j];
			switch(action) {
			case SEL_SELECT:
				subface->flag |= MPTEX_SUBFACE_SELECTED;
				break;
			case SEL_DESELECT:
				subface->flag &= ~MPTEX_SUBFACE_SELECTED;
				break;
			case SEL_INVERT:
				subface->flag ^= MPTEX_SUBFACE_SELECTED;
				break;
			}
		}
	}
	
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void PTEX_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select or Deselect All";
	ot->description= "Change selection of all ptex faces";
	ot->idname= "PTEX_OT_select_all";
	
	/* api callbacks */
	ot->exec= ptex_select_all_exec;
	ot->poll= ptex_select_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}
