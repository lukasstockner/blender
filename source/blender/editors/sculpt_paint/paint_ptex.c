#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
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
#include "BKE_ptex.h"
#include "BKE_report.h"
#include "BKE_subsurf.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "IMB_imbuf.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"
#include "ptex.h"

#include <assert.h>
#include <stdlib.h>

static void paint_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
	if(BLI_pbvh_node_get_tmin(node) < *tmin) {
		PaintStrokeRaycastData *data = data_v;
		
		if(BLI_pbvh_node_raycast(data->ob->paint->pbvh, node, NULL,
					 data->ray_start, data->ray_normal,
					 &data->dist, NULL, NULL)) {
			data->hit |= 1;
			*tmin = data->dist;
		}
	}
}

static int ptex_paint_stroke_get_location(bContext *C, struct PaintStroke *stroke, float out[3], float mouse[2])
{
	// XXX: sculpt_stroke_modifiers_check(C, ss);
	return paint_stroke_get_location(C, stroke, paint_raycast_cb, NULL, out, mouse, 0);		
}

static int ptex_paint_stroke_test_start(bContext *C, struct wmOperator *op, wmEvent *event)
{
	if(paint_stroke_over_mesh(C, op->customdata, event->x, event->y)) {
		Object *ob= CTX_data_active_object(C);
		Scene *scene = CTX_data_scene(C);
		DerivedMesh *dm;
		Mesh *me;

		/* context checks could be a poll() */
		me= get_mesh(ob);	
	
		dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH|CD_MASK_MCOL);
		ob->paint->pbvh = dm->getPBVH(ob, dm);

		pbvh_undo_push_begin("Vertex paint");

		return 1;
	}
	return 0;
}

static void ptex_paint_blend(Brush *brush, PaintStroke *stroke, float col[4], float alpha, float co[2])
{
	float src_img[4], *src;
	int tool = brush->vertexpaint_tool;

	if(tool == IMB_BLEND_ADD_ALPHA &&
	   (brush->flag & BRUSH_DIR_IN))
		tool = IMB_BLEND_ERASE_ALPHA;

	if(paint_sample_overlay(stroke, src_img, co)) {
		src = src_img;
		alpha *= src_img[3];
	}
	else
		src = brush->rgb;

		IMB_blend_color_float(col, col, src, alpha, tool);
}

static void ptex_elem_to_float4(PtexDataType type, int channels, void *data, float fcol[4])
{
	int i;

	/* default alpha */
	fcol[3] = 1;

	switch(type) {
	case PTEX_DT_UINT8:
		for(i = 0; i < channels; ++i)
			fcol[i] = ((unsigned char*)data)[i] / 255.0;
		break;
	case PTEX_DT_UINT16:
		for(i = 0; i < channels; ++i)
			fcol[i] = ((unsigned char*)data)[i] / 65535.0;
		break;
	case PTEX_DT_FLOAT:
		for(i = 0; i < channels; ++i)
			fcol[i] = ((float*)data)[i];
		break;
	default:
		break;
	}

	if(channels == 1) {
		for(i = 1; i < 4; ++i)
			fcol[i] = fcol[0];
	}
}

static void ptex_elem_from_float4(PtexDataType type, int channels, void *data, float fcol[4])
{
	int i;

	if(channels == 1) {
		float avg = (fcol[0]+fcol[1]+fcol[2]) / 3.0f;
		switch(type) {
		case PTEX_DT_UINT8:
			((unsigned char*)data)[0] = avg * 255;
			break;
		case PTEX_DT_UINT16:
			((unsigned short*)data)[0] = avg * 65535;
			break;
		case PTEX_DT_FLOAT:
			((float*)data)[0] = avg;
			break;
		default:
			break;
		}
	}
	else {
		switch(type) {
		case PTEX_DT_UINT8:
			for(i = 0; i < channels; ++i)
				((unsigned char*)data)[i] = fcol[i] * 255;
			break;
		case PTEX_DT_UINT16:
			for(i = 0; i < channels; ++i)
				((unsigned short*)data)[i] = fcol[i] * 65535;
			break;
		case PTEX_DT_FLOAT:
			for(i = 0; i < channels; ++i)
				((float*)data)[i] = fcol[i];
			break;
		default:
			break;
		}
	}
}

static void ptex_get_edge_iter(MPtex *mptex, GridToFace *gtf,
			       char *data,
			       int layersize, int edge, int border,
			       char **start, int *len, int *step)
{
	MPtexSubface *subface = &mptex[gtf->face].subfaces[gtf->offset];
	int *res = subface->res;
	int start_offset;
	int x, y;

	if(!data)
		data = subface->data;

	switch(edge) {
	case 0:
		x = border;
		y = 0;
		*len = res[0];
		*step = layersize;
		break;
	case 2:
		x = border;
		y = res[1] + border + border - 1;
		*len = res[0];
		*step = layersize;
		break;
	case 1:
		x = -1;
		y = border + 1;
		*len = res[1];
		*step = layersize * (res[0] + border + border);
		break;
	case 3:
		x = 0;
		y = border;
		*len = res[1];
		*step = layersize * (res[0] + border + border);
		break;
	}

	start_offset = y * (res[0] + border + border) + x;
	*start = data + layersize * start_offset;
}



/* build a ptex grid that includes borders filled with neighbor data,
   or repeated data if at a mesh boundary. this could be done more efficient
   by not allocating a full grid here, but simpler to get this working first */
static void *ptex_paint_build_blur_input(DMGridAdjacency *grid_adj, MPtex *mptex,
					 int grid_index, GridToFace *grid_face_map,
					 int layersize)
{
	GridToFace *gtf;
	MPtexSubface *subface;
	char *out;
	int i, v;

	gtf = &grid_face_map[grid_index];
	subface = &mptex[gtf->face].subfaces[gtf->offset];

	out = MEM_mallocN(layersize * (subface->res[0]+2) * (subface->res[1]+2), "blurred_data_input");

	/* copy center */
	for(v = 0; v < subface->res[1]; ++v) {
		memcpy(out + layersize * ((v+1) * (subface->res[0]+2) + 1),
		       (char*)subface->data + layersize * v * subface->res[0],
		       layersize * subface->res[0]);
	}

	/* fill in borders with adjacent data */
	for(i = 0; i < 4; ++i) {
		DMGridAdjacency *adj = &grid_adj[grid_index];
		char *t1, *t2;
		float step2_fac;
		int j, len1, len2, step1, step2;

		ptex_get_edge_iter(mptex, &grid_face_map[grid_index], out,
				   layersize, i, 1, &t1, &len1, &step1);

		if(adj->index[i] == -1) {
			/* mesh boundary, just repeat existing border */
			ptex_get_edge_iter(mptex, &grid_face_map[grid_index],
					   NULL, layersize, i, 0,
					   &t2, &len2, &step2);
		}
		else {
			ptex_get_edge_iter(mptex, &grid_face_map[adj->index[i]],
					   NULL, layersize, adj->rotation[i], 0,
					   &t2, &len2, &step2);
		}

		step2_fac = (float)len2 / (float)len1;

		for(j = 0; j < len1; ++j) {
			memcpy(t1 + step1 * j,
			       t2 + step2*(int)(step2_fac*j),
			       layersize);
		}
	}

	return out;
}

static void ptex_blur_texel(MPtex *pt, MPtexSubface *subface,
			    char *blur_input, int layersize,
			    int u_offset, int v_offset,
			    float avg[4])
{
	int i;

	zero_v4(avg);
	
	for(i = 0; i < 4; ++i) {
		float col[4];
		int u = u_offset;
		int v = v_offset;
		
		if(i == 0) u--;
		else if(i == 1) v--;
		else if(i == 2) u++;
		else if(i == 3) v++;

		ptex_elem_to_float4(pt->type,
				    pt->channels,
				    blur_input + layersize*((v+1)*(subface->res[0]+2) + u+1),
				    col);
		avg[0] += col[0];
		avg[1] += col[1];
		avg[2] += col[2];
		avg[3] += col[3];
	}

	mul_v4_fl(avg, 1.0f / i);
}

static void ptex_paint_ptex_from_quad(Brush *brush, PaintStroke *stroke, PaintStrokeTest *test,
				      MPtex *pt, MPtexSubface *subface, int res[2],
				      int u_offset, int v_offset, int layersize,
				      char *blur_input,
				      float v1[3], float v2[3], float v3[3], float v4[3])
				  
{
	char *data = (char*)subface->data + layersize * (v_offset * subface->res[0] + u_offset);
	float dtop[3], dbot[3], xoffset, yinterp, ustep, vstep;
	float co_bot[3], co_top[3], start_top[3], start_bot[3];
	int u, v;

	/* start of top and bottom "rails" */
	copy_v3_v3(start_top, v4);
	copy_v3_v3(start_bot, v1);

	/* direction of "rails" */
	sub_v3_v3v3(dtop, v3, v4);
	sub_v3_v3v3(dbot, v2, v1);

	/* offset to use center of texel rather than corner */
	xoffset = 1.0f / (2 * res[0]);
	yinterp = 1.0f / (2 * res[1]);
	madd_v3_v3fl(start_top, dtop, xoffset);
	madd_v3_v3fl(start_bot, dbot, xoffset);

	ustep = 1.0f / res[0];
	vstep = 1.0f / res[1];

	/* precalculate interpolation along "rails" */
	mul_v3_fl(dtop, ustep);
	mul_v3_fl(dbot, ustep);

	for(v = 0; v < res[1]; ++v) {
		copy_v3_v3(co_top, start_top);
		copy_v3_v3(co_bot, start_bot);

		for(u = 0; u < res[0]; ++u) {
			float co[3];

			interp_v3_v3v3(co, co_bot, co_top, yinterp);

			if(paint_stroke_test(test, co)) {
				float strength;
				float fcol[4];
				char *elem = data + layersize*(v*subface->res[0] + u);
					
				strength = brush->alpha *
					paint_stroke_combined_strength(stroke, test->dist, co, 0);
				
				ptex_elem_to_float4(pt->type, pt->channels, elem, fcol);

				if(blur_input) {
					float blurcol[4];
					
					ptex_blur_texel(pt, subface,
							blur_input, layersize,
							u_offset + u, v_offset + v,
							blurcol);

					interp_v4_v4v4(fcol, fcol, blurcol, strength);
				}
				else
					ptex_paint_blend(brush, stroke, fcol, strength, co);

				ptex_elem_from_float4(pt->type, pt->channels, elem, fcol);
			}

			add_v3_v3(co_bot, dbot);
			add_v3_v3(co_top, dtop);
		}

		yinterp += vstep;
	}
}

static void ptex_paint_node_grids(Brush *brush, PaintStroke *stroke,
				  DMGridData **grids, GridKey *gridkey,
				  GridToFace *grid_face_map,
				  DMGridAdjacency *grid_adj,
				  CustomData *fdata,
				  int *grid_indices,
				  int totgrid, int gridsize)
{
	PaintStrokeTest test;
	MPtex *mptex;
	int i;

	mptex = CustomData_get_layer(fdata, CD_MPTEX);

	paint_stroke_test_init(&test, stroke);

	for(i = 0; i < totgrid; ++i) {
		int g = grid_indices[i];
		DMGridData *grid = grids[g];
		GridToFace *gtf = &grid_face_map[g];
		MPtex *pt = &mptex[gtf->face];
		MPtexSubface *subface = &pt->subfaces[gtf->offset];
		char *blur_input = NULL;
		int u, v, x, y, layersize, res[2];

		/* ignore hidden and masked subfaces */
		if(subface->flag & (MPTEX_SUBFACE_HIDDEN|MPTEX_SUBFACE_MASKED))
			continue;

		layersize = pt->channels * ptex_data_size(pt->type);

		if(brush->vertexpaint_tool == VERTEX_PAINT_BLUR) {
			blur_input = ptex_paint_build_blur_input(grid_adj, mptex,
								 g, grid_face_map,
								 layersize);
		}

		res[0] = MAX2(subface->res[0] / (gridsize - 1), 1);
		res[1] = MAX2(subface->res[1] / (gridsize - 1), 1);

		for(v = 0, y = 0; v < subface->res[1]; v += res[1], ++y) {
			for(u = 0, x = 0; u < subface->res[0]; u += res[0], ++x) {
				float *co[4] = {
					GRIDELEM_CO_AT(grid, y*gridsize+x, gridkey),
					GRIDELEM_CO_AT(grid, y*gridsize+(x+1), gridkey),
					
					GRIDELEM_CO_AT(grid, (y+1)*gridsize+(x+1), gridkey),
					GRIDELEM_CO_AT(grid, (y+1)*gridsize+x, gridkey),
				};

				ptex_paint_ptex_from_quad(brush, stroke, &test,
							  pt, subface, res, u, v,
							  layersize, blur_input,
							  co[0], co[1], co[2], co[3]);
			}
		}

		if(blur_input)
			MEM_freeN(blur_input);
	}
}

static void ptex_paint_nodes(VPaint *vp, PaintStroke *stroke,
			 Scene *scene, Object *ob,
			 PBVHNode **nodes, int totnode)
{
	PBVH *pbvh = ob->paint->pbvh;
	Brush *brush = paint_brush(&vp->paint);
	CustomData *vdata = NULL;
	CustomData *fdata = NULL;
	GridToFace *grid_face_map;
	int n;

	assert(BLI_pbvh_uses_grids(pbvh));

	BLI_pbvh_get_customdata(pbvh, &vdata, &fdata);
	grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);

	for(n = 0; n < totnode; ++n) {
		DMGridData **grids;
		DMGridAdjacency *grid_adj;
		GridKey *gridkey;
		int *grid_indices;
		int totgrid, gridsize;

		pbvh_undo_push_node(nodes[n], PBVH_UNDO_PTEX, ob, scene);

		BLI_pbvh_node_get_grids(pbvh, nodes[n],
					&grid_indices, &totgrid, NULL,
					&gridsize, &grids, &grid_adj, &gridkey);

		ptex_paint_node_grids(brush, stroke,
				      grids, gridkey,
				      grid_face_map,
				      grid_adj, fdata,
				      grid_indices,
				      totgrid, gridsize);

		BLI_pbvh_node_set_flags(nodes[n],
			SET_INT_IN_POINTER(PBVH_UpdateColorBuffers|
					   PBVH_UpdateRedraw));
	}
}

static void ptex_paint_restore_node(PBVH *pbvh, PBVHNode *node, PBVHUndoNode *unode,
				CustomData *fdata, GridToFace *grid_face_map)
{
	MPtex *mptex;
	int *grid_indices, totgrid, i;

	mptex = CustomData_get_layer_named(fdata, CD_MPTEX,
					   (char*)pbvh_undo_node_mptex_name(unode));

	grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);

	BLI_pbvh_node_get_grids(pbvh, node,
				&grid_indices, &totgrid,
				NULL, NULL, NULL, NULL, NULL);

	for(i = 0; i < totgrid; i++) {
		GridToFace *gtf = &grid_face_map[grid_indices[i]];
		MPtex *pt = &mptex[gtf->face];
		MPtexSubface *subface = &pt->subfaces[gtf->offset];
		int layersize;
			
		layersize = pt->channels * ptex_data_size(pt->type);

		memcpy(subface->data, pbvh_undo_node_mptex_data(unode, i),
		       layersize * subface->res[0] * subface->res[1]);
	}

	BLI_pbvh_node_set_flags(node, SET_INT_IN_POINTER(PBVH_UpdateColorBuffers|
							 PBVH_UpdateRedraw));
}

static void ptex_paint_restore(VPaint *vp, Object *ob)
{
	Brush *brush = paint_brush(&vp->paint);
	PBVH *pbvh = ob->paint->pbvh;

	/* Restore the mesh before continuing with anchored stroke */
	if((brush->flag & BRUSH_ANCHORED) ||
	   (brush->flag & BRUSH_RESTORE_MESH))
	{
		PBVHNode **nodes;
		CustomData *fdata;
		GridToFace *grid_face_map;
		int n, totnode;

		BLI_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

		grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);
		BLI_pbvh_get_customdata(pbvh, NULL, &fdata);

		for(n = 0; n < totnode; n++) {
			PBVHUndoNode *unode;
			
			unode= pbvh_undo_get_node(nodes[n]);
			if(unode) {
				ptex_paint_restore_node(pbvh, nodes[n], unode, fdata, grid_face_map);
			}
		}

		if(nodes)
			MEM_freeN(nodes);
	}
}

static void ptex_stitch_subfaces()
{

}

static void ptex_paint_stroke_update_step(bContext *C, PaintStroke *stroke,
					  PointerRNA *itemptr)
{
	VPaint *vp= CTX_data_tool_settings(C)->vpaint;
	Object *ob = CTX_data_active_object(C);

	ptex_paint_restore(vp, ob);

	paint_stroke_apply_brush(C, stroke, &vp->paint);

	if(paint_brush(&vp->paint)->vertexpaint_tool == VERTEX_PAINT_BLUR)
		ptex_stitch_subfaces();

	/* partial redraw */
	paint_tag_partial_redraw(C, ob);
}

static void ptex_paint_stroke_brush_action(bContext *C, PaintStroke *stroke)
{

	VPaint *vp= CTX_data_tool_settings(C)->vpaint;
	ViewContext *vc = paint_stroke_view_context(stroke);
	Scene *scene = CTX_data_scene(C);
	Object *ob = vc->obact;
	PBVHSearchSphereData search_data;
	PBVHNode **nodes;
	int totnode;
	float center[3], radius;

	paint_stroke_symmetry_location(stroke, center);

	search_data.center = center;
		
	radius = paint_stroke_radius(stroke);
	search_data.radius_squared = radius*radius;
	search_data.original = 0;

	BLI_pbvh_search_gather(ob->paint->pbvh, BLI_pbvh_search_sphere_cb,
			       &search_data, &nodes, &totnode);
		
	ptex_paint_nodes(vp, stroke, scene, ob, nodes, totnode);

	if(nodes)
		MEM_freeN(nodes);
}

static void ptex_paint_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	pbvh_undo_push_end();
}

static int ptex_paint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata = paint_stroke_new(C,
					  ptex_paint_stroke_get_location,
					  ptex_paint_stroke_test_start,
					  ptex_paint_stroke_update_step,
					  NULL,
					  ptex_paint_stroke_brush_action,
					  ptex_paint_stroke_done);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Vertex Paint";
	ot->idname= "PAINT_OT_vertex_paint";
	
	/* api callbacks */
	ot->invoke= ptex_paint_invoke;
	ot->modal= paint_stroke_modal;
	ot->exec= paint_stroke_exec;
	ot->poll= vertex_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

static EnumPropertyItem ptex_layer_type_items[]= {
	{PTEX_DT_UINT8, "PTEX_DT_UINT8", 0, "8-bit channels", ""},
	{PTEX_DT_UINT16, "PTEX_DT_UINT16", 0, "16-bit channels", ""},
	{PTEX_DT_FLOAT, "PTEX_DT_FLOAT", 0, "32-bit floating-point channels", ""},

	{0, NULL, 0, NULL, NULL}};

static int ptex_active_layer_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	if(ob) {
		Mesh *me = get_mesh(ob);
		if(me)
			return !!CustomData_get_layer(&me->fdata, CD_MPTEX);
	}
	return 0;
}

static int ptex_layer_convert_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	MPtex *mptex;
	PtexDataType new_type;
	int i, j, x, y;

	new_type = RNA_enum_get(op->ptr, "type");
	mptex = CustomData_get_layer(&me->fdata, CD_MPTEX);

	for(i = 0; i < me->totface; ++i) {
		MPtex *pt = &mptex[i];

		for(j = 0; j < mptex[i].totsubface; ++j) {
			MPtexSubface *subface = &pt->subfaces[j];
			int orig_layersize, new_layersize;
			float *f;
			char *orig_data, *new_data, *new_data_start;

			orig_layersize = pt->channels * ptex_data_size(pt->type);
			new_layersize = pt->channels * ptex_data_size(new_type);

			f = MEM_callocN(sizeof(float) * pt->channels, "tmp ptex elem");
			new_data_start = new_data =
				MEM_callocN(new_layersize * subface->res[0] * subface->res[1],
					    "mptex converted data");
			orig_data = subface->data;

			for(y = 0; y < subface->res[1]; ++y) {
				for(x = 0; x < subface->res[0]; ++x,
					    orig_data += orig_layersize,
					    new_data += new_layersize) {
					ptex_elem_to_floats(pt->type, pt->channels, orig_data, f);
					ptex_elem_from_floats(new_type, pt->channels, new_data, f);
				}
			}

			MEM_freeN(subface->data);
			subface->data = new_data_start;
			MEM_freeN(f);
		}

		pt->type = new_type;
	}

	return OPERATOR_FINISHED;
}

void PTEX_OT_layer_convert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Convert Layer";
	ot->description= "Convert Ptex data to another type";
	ot->idname= "PTEX_OT_layer_convert";
	
	/* api callbacks */
	ot->exec= ptex_layer_convert_exec;
	ot->poll= ptex_active_layer_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", ptex_layer_type_items, PTEX_DT_FLOAT, "Type", "Layer channels and data type");
}

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
	int active_offset;
	int i, j;

	type = RNA_enum_get(op->ptr, "type");
	totchannel = RNA_int_get(op->ptr, "channels");
	layer_size = ptex_data_size(type) * totchannel;
	def_val = ptex_default_data(type);

	active_offset = CustomData_number_of_layers(&me->fdata, CD_MPTEX);
	mptex = CustomData_add_layer(&me->fdata, CD_MPTEX, CD_CALLOC,
				     NULL, me->totface);
	CustomData_set_layer_active(&me->fdata, CD_MPTEX, active_offset);

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
	RNA_def_enum(ot->srna, "type", ptex_layer_type_items, PTEX_DT_FLOAT, "Type", "Layer channels and data type");
}

static int ptex_layer_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;

	CustomData_free_layer_active(&me->fdata, CD_MPTEX,
				     me->totface);

	if((ob->mode & OB_MODE_VERTEX_PAINT) &&
	   !CustomData_number_of_layers(&me->fdata, CD_MPTEX))
		ED_object_toggle_modes(C, OB_MODE_VERTEX_PAINT);		

	return OPERATOR_FINISHED;
}

void PTEX_OT_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Layer";
	ot->description= "Remove active ptex layer";
	ot->idname= "PTEX_OT_layer_remove";
	
	/* api callbacks */
	ot->exec= ptex_layer_remove_exec;
	ot->poll= ptex_active_layer_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int ptex_layer_save_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	char str[FILE_MAX];

	if(!me->totface)
		return OPERATOR_CANCELLED;

	RNA_string_get(op->ptr, "filepath", str);
	if(!ptex_layer_save_file(me, str))
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED;
}

static int ptex_layer_save_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/*Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	const char *name;
	char buf[FILE_MAX];*/
	
	if(RNA_property_is_set(op->ptr, "filepath"))
		return ptex_layer_save_exec(C, op);

	/*name = me->fdata.layers[CustomData_get_active_layer_index(&me->fdata, CD_MPTEX)].name;
	BLI_snprintf(buf, FILE_MAX, "%s.ptx", name);
	
	RNA_string_set(op->ptr, "filepath", buf);*/
		       
	WM_event_add_fileselect(C, op); 

	return OPERATOR_RUNNING_MODAL;
}

void PTEX_OT_layer_save(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Save Layer";
	ot->description= "Save active ptex layer";
	ot->idname= "PTEX_OT_layer_save";
	
	/* api callbacks */
	ot->invoke= ptex_layer_save_invoke;
	ot->exec= ptex_layer_save_exec;
	ot->poll= ptex_active_layer_poll;

	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL, FILE_SAVE, WM_FILESEL_FILEPATH);
}

/* loads a .ptx file
   makes some assumptions that could be relaxed
   later as our ptex implementation is refined

   on the other hand, some unsupported ptex features
   are not checked for yet
*/
int ptex_open_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;

	PtexTextureHandle *ptex_texture;
	PtexDataType ptex_data_type;
	int totchannel;

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
	
	ptex_layer_from_file(me, ptex_texture);
	
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

static void ptex_redraw_selected(PBVHNode *node, void *data)
{
	PBVH *pbvh = data;
	MPtex *mptex;
	GridToFace *grid_face_map;
	CustomData *fdata;
	int totgrid, *grid_indices, i;

	BLI_pbvh_get_customdata(pbvh, NULL, &fdata);
	mptex = CustomData_get_layer(fdata, CD_MPTEX);
	grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);
	BLI_pbvh_node_get_grids(pbvh, node,
				&grid_indices, &totgrid, NULL, NULL,
				NULL, NULL, NULL);

	for(i = 0; i < totgrid; ++i) {
		GridToFace *gtf = &grid_face_map[grid_indices[i]];
		if(mptex[gtf->face].subfaces[gtf->offset].flag & MPTEX_SUBFACE_SELECTED) {
			BLI_pbvh_node_set_flags(node,
						SET_INT_IN_POINTER(PBVH_UpdateColorBuffers|
								   PBVH_UpdateRedraw));
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

	BLI_pbvh_search_callback(ob->paint->pbvh, NULL, NULL, ptex_redraw_selected, ob->paint->pbvh);

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

static int ptex_edit_poll(bContext *C)
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
	ot->poll= ptex_edit_poll;

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
	ot->poll= ptex_edit_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

int ptex_subface_flag_set_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	MPtex *mptex = CustomData_get_layer(&me->fdata, CD_MPTEX);
	int flag = RNA_enum_get(op->ptr, "flag");
	int set = RNA_boolean_get(op->ptr, "set");
	int ignore_unselected = RNA_boolean_get(op->ptr, "ignore_unselected");
	int ignore_hidden = RNA_boolean_get(op->ptr, "ignore_hidden");
	int i, j;

	for(i = 0; i < me->totface; ++i) {
		for(j = 0; j < mptex[i].totsubface; ++j) {
			MPtexSubface *subface = &mptex[i].subfaces[j];
			
			if((!ignore_unselected || (subface->flag & MPTEX_SUBFACE_SELECTED)) &&
			   (!ignore_hidden || !(subface->flag & MPTEX_SUBFACE_HIDDEN))) {
				if(set)
					subface->flag |= flag;
				else
					subface->flag &= ~flag;
			}
		}
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void PTEX_OT_subface_flag_set(wmOperatorType *ot)
{
	static EnumPropertyItem flag_items[] = {
		{MPTEX_SUBFACE_HIDDEN, "HIDDEN", 0, "Hidden", ""},
		{MPTEX_SUBFACE_MASKED, "MASKED", 0, "Masked", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Set Subface Flags";
	ot->description= "Set or clear a flag from ptex subfaces";
	ot->idname= "PTEX_OT_subface_flag_set";
	
	/* api callbacks */
	ot->exec= ptex_subface_flag_set_exec;
	ot->poll= ptex_active_layer_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "flag", flag_items, 0, "Flag", "");
	RNA_def_boolean(ot->srna, "set", 1, "Set", "Set the flag if true, otherwise clear the flag");	
	RNA_def_boolean(ot->srna, "ignore_unselected", 1, "Ignore Unselected", "Don't change the flags of unselected faces");
	RNA_def_boolean(ot->srna, "ignore_hidden", 1, "Ignore Hidden", "Don't change the flags of hidden faces");
}
