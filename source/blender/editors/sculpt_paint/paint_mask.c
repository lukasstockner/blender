#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_dmgrid.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_texture.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_pbvh.h"
#include "BLI_string.h"

#include "ED_mesh.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"
 
#include "RE_render_ext.h"
#include "RE_shader_ext.h"

/* for redraw, just need to update the pbvh's vbo buffers */
static void paintmask_redraw(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	paint_refresh_mask_display(ob);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);	
}

static int mask_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	
	return ob && get_mesh(ob) && ob->paint &&
		(ob->mode & (OB_MODE_SCULPT|OB_MODE_VERTEX_PAINT));
}

static int mask_active_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if(mask_poll(C)) {
		Mesh *me = get_mesh(ob);
		return CustomData_get_active_layer_index(&me->vdata, CD_PAINTMASK) != -1;
	}

	return 0;
}

void paintmask_brush_apply(Paint *paint, PaintStroke *stroke, PBVHNode **nodes, int totnode,
			   float bstrength)
{
	Object *ob = paint_stroke_view_context(stroke)->obact;
	int n;
	
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		
		pbvh_undo_push_node(nodes[n], PBVH_UNDO_PMASK, ob, NULL);
		paint_stroke_test_init(&test, stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			/* TODO: should add a mask layer if needed */
			if(vd.mask_active) {
				if(paint_stroke_test(&test, vd.co)) {
					float fade;

					fade = paint_stroke_combined_strength(stroke,
									      test.dist,
									      vd.co,
									      0) *
						bstrength;

					*vd.mask_active += fade;
					CLAMP(*vd.mask_active, 0, 1);
				
					if(vd.mvert) vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
				}
			}
		}
		BLI_pbvh_vertex_iter_end;

		BLI_pbvh_node_set_flags(nodes[n], SET_INT_IN_POINTER(PBVH_UpdateColorBuffers|PBVH_UpdateRedraw));
	}
}

static float get_tex_mask_strength(MTex *tex_slot, float *uv, float vco[3])
{
	float texvec[3] = {0, 0, 0};
	TexResult texres;
	int mapping = tex_slot->texco;
	
	if(mapping == TEXCO_UV && !uv)
		mapping = TEXCO_ORCO;
	
	switch(tex_slot->texco) {
	case TEXCO_UV:
		texvec[0] = uv[0] * 2 - 1;
		texvec[1] = uv[1] * 2 - 1;
		break;
	default:
		copy_v3_v3(texvec, vco);
	}
	
	memset(&texres, 0, sizeof(TexResult));
	get_texture_value(tex_slot->tex, texvec, &texres);
	
	return texres.tin;
}

/* Set the value of a single mask element from either a UV or a coord */
#define SET_MASK(elem, uv)						\
	GRIDELEM_MASK(elem, gridkey)[active] =				\
		get_tex_mask_strength(tex_slot, uv,			\
				      GRIDELEM_CO(elem, gridkey))	\

/* Fill active mask layer of entire grid from either MTFaces or coords */
static void mask_grid_from_tex(DMGridData *grid, int gridsize,
			       GridKey *gridkey, MTFace *mtface,
			       MTex *tex_slot, int active)
{
	int x, y, boundary;

	boundary = gridsize - 2;

	for(y = 0; y <= boundary; ++y) {
		for(x = 0; x <= boundary; ++x) {
			SET_MASK(GRIDELEM_AT(grid, y*gridsize+x, gridkey),
				 mtface ? mtface->uv[0] : NULL);

			/* Do the edge of the grid separately because the UV
			   grid is one element smaller on each side compared
			   to the vert-data grid */
			if(x == boundary && y == boundary) {
				SET_MASK(GRIDELEM_AT(grid, (y+1)*gridsize+x+1, gridkey),
					 mtface ? mtface->uv[2] : NULL);
			}
			if(x == boundary) {
				SET_MASK(GRIDELEM_AT(grid, y*gridsize+x+1, gridkey),
					 mtface ? mtface->uv[3] : NULL);
			}
			if(y == boundary) {
				SET_MASK(GRIDELEM_AT(grid, (y+1)*gridsize+x, gridkey),
					 mtface ? mtface->uv[1] : NULL);
			}
						
			if(mtface) ++mtface;
		}
	}
}

static void mask_face_from_tex(MFace *f, MVert *mvert, MTFace *mtface,
			       float *pmask, MTex *tex_slot, int active)
{
	int S = f->v4 ? 4 : 3;
	int i;

	/* Masks are per-vertex, not per-face-corner, so the mask
	   value at each vertex comes from one arbitrary face
	   corner; not averaged or otherwise combined yet */
	for(i = 0; i < S; ++i) {
		int vndx = (&f->v1)[i];
		float *vco = mvert[vndx].co;
				
		pmask[vndx] = get_tex_mask_strength(tex_slot,
				mtface ? mtface->uv[i] : NULL, vco);
	}
}

static int paint_mask_from_texture_exec(bContext *C, wmOperator *op)
{
	struct Scene *scene;
	Object *ob;
	Mesh *me;
	struct MultiresModifierData *mmd;
	PaintSession *ps;
	MTex *tex_slot;
	DerivedMesh *dm = NULL;
	PBVH *pbvh;
	MTFace *mtfaces = NULL;
	PBVHNode **nodes;
	int totnode, n, i, active;
	
	tex_slot = CTX_data_pointer_get_type(C, "texture_slot",
					     &RNA_TextureSlot).data;
	
	scene = CTX_data_scene(C);
	ob = CTX_data_active_object(C);
	ps = ob->paint;
	me = get_mesh(ob);
	mmd = ED_paint_multires_active(scene, ob);
	
	pbvh_undo_push_begin("Paint mask from texture");

	active = CustomData_get_active_layer(&me->vdata, CD_PAINTMASK);

	/* if using UV mapping, check for a matching MTFace layer */
	if(tex_slot->texco == TEXCO_UV) {
		mtfaces = CustomData_get_layer_named(&me->fdata, CD_MTFACE,
						     tex_slot->uvname);
	}

	/* the MTFace mask is needed only for multires+UV */
	dm = mesh_get_derived_final(scene, ob,
				    (mtfaces && mmd) ? CD_MASK_MTFACE : 0);

	/* use the subdivided UVs for multires */
	if(mtfaces && mmd) {
		mtfaces = CustomData_get_layer_named(&dm->faceData,
						     CD_MTFACE,
						     tex_slot->uvname);
	}

	/* update the pbvh */
	ps->pbvh = pbvh = dm->getPBVH(ob, dm);

	/* get all nodes in the pbvh */
	BLI_pbvh_search_gather(pbvh,
			       NULL, NULL,
			       &nodes, &totnode);

	if(mmd) {
		/* For all grids, find offset into mtfaces and apply
		   the texture to the grid */
		for(n = 0; n < totnode; ++n) {
			DMGridData **grids;
			GridKey *gridkey;
			int *grid_indices, totgrid, gridsize;

			pbvh_undo_push_node(nodes[n], PBVH_UNDO_PMASK, ob, scene);

			BLI_pbvh_node_get_grids(pbvh, nodes[n], &grid_indices,
						&totgrid, NULL, &gridsize,
						&grids, NULL, &gridkey);

			for(i = 0; i < totgrid; ++i) {
				int grid_index = grid_indices[i];
				MTFace *mtface = NULL;

				if(mtfaces) {
					mtface = &mtfaces[grid_index *
							  ((gridsize-1) *
							   (gridsize-1))];
				}

				mask_grid_from_tex(grids[grid_index],
						   gridsize, gridkey,
						   mtface, tex_slot, active);
			}

			BLI_pbvh_node_set_flags(nodes[n],
				SET_INT_IN_POINTER(PBVH_UpdateColorBuffers));
		}

		multires_mark_as_modified(ob);
	}
	else {
		float *pmask = CustomData_get_layer(&me->vdata, CD_PAINTMASK);

		for(n = 0; n < totnode; ++n) {
			MFace *mface;
			int *face_indices, totface;

			pbvh_undo_push_node(nodes[n], PBVH_UNDO_PMASK, ob, scene);

			BLI_pbvh_node_get_faces(pbvh, nodes[n], &mface,
						&face_indices, NULL, &totface);

			for(i = 0; i < totface; ++i) {
				int face_index = face_indices[i];
				MTFace *mtface = NULL;

				if(mtfaces)
					mtface = mtfaces + face_index;

				mask_face_from_tex(me->mface + face_index,
						   me->mvert, mtface,
						   pmask, tex_slot, active);
			}

			BLI_pbvh_node_set_flags(nodes[n],
				SET_INT_IN_POINTER(PBVH_UpdateColorBuffers));
		}
	}
	
	MEM_freeN(nodes);

	pbvh_undo_push_end();

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

/* fills a mask with intensity values from a texture, using an
   mtex to provide mapping */
void PAINT_OT_mask_from_texture(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mask From Texture";
	ot->idname= "PAINT_OT_mask_from_texture";
	
	/* api callbacks */
	ot->exec= paint_mask_from_texture_exec;
	ot->poll= mask_active_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER;
}

static void set_mask_value(MaskSetMode mode, float *m)
{
	*m = (mode == MASKING_CLEAR ? 0 :
	      mode == MASKING_FILL ? 1 :
	      mode == MASKING_INVERT ? (1 - *m) :
	      mode == MASKING_RANDOM ? (float)rand() / RAND_MAX : 0);
}

static int paint_mask_set_exec(bContext *C, wmOperator *op)
{
	MaskSetMode mode = RNA_enum_get(op->ptr, "mode");
	struct Scene *scene;
	Object *ob;
	DerivedMesh *dm;
	struct MultiresModifierData *mmd;
	PaintSession *ps;
	Mesh *me;
	PBVH *pbvh;

	scene = CTX_data_scene(C);
	ob = CTX_data_active_object(C);
	ps = ob->paint;
	me = get_mesh(ob);
	mmd = ED_paint_multires_active(scene, ob);

	dm = mesh_get_derived_final(scene, ob, 0);
	ps->pbvh = pbvh = dm->getPBVH(ob, dm);

	if(pbvh) {
		PBVHNode **nodes;
		int n, totnode;

		BLI_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

		pbvh_undo_push_begin("Paint mask fill");

		for(n=0; n<totnode; n++) {
			PBVHVertexIter vd;

			pbvh_undo_push_node(nodes[n], PBVH_UNDO_PMASK, ob, scene);

			BLI_pbvh_vertex_iter_begin(pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(vd.mask_active)
					set_mask_value(mode, vd.mask_active);
			}
			BLI_pbvh_vertex_iter_end;

			BLI_pbvh_node_set_flags(nodes[n], SET_INT_IN_POINTER(PBVH_UpdateColorBuffers|PBVH_UpdateRedraw));
		}

		if(nodes)
			MEM_freeN(nodes);

		if(mmd)
			multires_mark_as_modified(ob);

		pbvh_undo_push_end();

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}
	
	return OPERATOR_FINISHED;
}

/* fills up a mask for the entire object, setting each vertex to
   either 0, 1, or a random value */
void PAINT_OT_mask_set(wmOperatorType *ot)
{
	static EnumPropertyItem mask_items[] = {
		{MASKING_CLEAR, "CLEAR", 0, "Clear", ""},
		{MASKING_FILL, "FILL", 0, "Fill", ""},
		{MASKING_INVERT, "INVERT", 0, "Invert", ""},
		{MASKING_RANDOM, "RANDOM", 0, "Random", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Set Mask";
	ot->idname= "PAINT_OT_mask_set";
	
	/* api callbacks */
	ot->exec= paint_mask_set_exec;
	ot->poll= mask_active_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER;

	/* properties */
	RNA_def_enum(ot->srna, "mode", mask_items, MASKING_CLEAR, "Mode", "");
}

/* if this is a multires mesh, update it and free the DM.
   returns 1 if this is a multires mesh, 0 otherwise */
static int paintmask_check_multires(bContext *C)
{
	struct Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	if(ED_paint_multires_active(scene, ob)) {
		multires_force_update(ob);
		return 1;
	}

	return 0;
}

static int mask_layer_poll(bContext *C)
{
	return mask_poll(C) && ED_mesh_layers_poll(C);
}

static int mask_layer_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Mesh *me= ob->data;
	int multires, top, first;
	char *layer_name;
	PaintLayerUndoNode *unode;

	unode = paint_layer_undo_push(CD_PAINTMASK, "Add mask layer");

	multires = paintmask_check_multires(C);

	top= CustomData_number_of_layers(&me->vdata, CD_PAINTMASK);
	CustomData_add_layer(&me->vdata, CD_PAINTMASK, CD_DEFAULT,
			     NULL, me->totvert);

	first = CustomData_get_layer_index(&me->vdata, CD_PAINTMASK);
	assert(first >= 0);

	me->vdata.layers[first + top].flag |= CD_FLAG_MULTIRES;
	CustomData_set_layer_active(&me->vdata, CD_PAINTMASK, top);
	layer_name = me->vdata.layers[first + top].name;

	/* now that we have correct name, update multires and finish undo push */

	if(multires) {
		CustomDataMultires *cdm;

		cdm = CustomData_get_layer(&me->fdata, CD_GRIDS);

		CustomData_multires_add_layers(cdm, me->totface,
					       CD_PAINTMASK, layer_name);
	}

	paint_layer_undo_set_add(unode, layer_name);

	paintmask_redraw(C);

	return OPERATOR_FINISHED;
}

static int mask_layer_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Mesh *me= ob->data;
	int active_offset, first;

	active_offset = CustomData_get_active_layer(&me->vdata, CD_PAINTMASK);

	if(active_offset >= 0) {
		PaintLayerUndoNode *unode;
		int multires = paintmask_check_multires(C);
		char *layer_name;

		unode = paint_layer_undo_push(CD_PAINTMASK,
					      "Remove mask layer");

		first = CustomData_get_layer_index(&me->vdata, CD_PAINTMASK);
		layer_name = me->vdata.layers[first + active_offset].name;

		paint_layer_undo_set_remove(unode, layer_name,
					    &me->vdata, &me->fdata,
					    me->totvert, me->totface);

		if(multires) {
			CustomDataMultires *cdm;

			cdm = CustomData_get_layer(&me->fdata, CD_GRIDS);

			CustomData_multires_remove_layers(cdm, me->totface,
							  CD_PAINTMASK,
							  layer_name);
		}

		CustomData_free_layer_active(&me->vdata, CD_PAINTMASK,
					     me->totvert);

		paintmask_redraw(C);
	}
	else
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

void PAINT_OT_mask_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Mask Layer";
	ot->description= "Add a paint mask layer";
	ot->idname= "PAINT_OT_mask_layer_add";
	
	/* api callbacks */
	ot->poll= mask_layer_poll;
	ot->exec= mask_layer_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void PAINT_OT_mask_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Mask Layer";
	ot->description= "Remove the active paint mask layer";
	ot->idname= "PAINT_OT_mask_layer_remove";
	
	/* api callbacks */
	ot->poll= mask_layer_poll;
	ot->exec= mask_layer_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
