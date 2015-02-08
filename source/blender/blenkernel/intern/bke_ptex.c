/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_image.h"
#include "BKE_ptex.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#ifdef WITH_PTEX
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math_base.h"
#include "BLI_math_interp.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_subsurf.h"

#include "BPX_ptex.h"
#endif

/* Like MPtexLogRes, but actual values instead of log */
typedef struct {
	int u;
	int v;
} MPtexRes;

static int ptex_data_type_num_bytes(const PtexDataType data_type)
{
	switch (data_type) {
		case MPTEX_DATA_TYPE_UINT8:
			return 1;
		case MPTEX_DATA_TYPE_FLOAT32:
			return 4;
	}

	BLI_assert(!"Invalid PtexDataType");
	return 0;
}

const int ptex_rlog2_limit = 30;
static bool ptex_rlog2_valid(const int rlog2)
{
	/* Limit sides to about a billion texels */
	return rlog2 >= 0 && rlog2 <= ptex_rlog2_limit;
}

static int ptex_res_from_rlog2(const int rlog2)
{
	BLI_assert(ptex_rlog2_valid(rlog2));
	return (1 << rlog2);
}

static size_t ptex_area_from_logres(const MPtexLogRes logres)
{
	return ptex_res_from_rlog2(logres.u) * ptex_res_from_rlog2(logres.v);
}

static MPtexRes bke_ptex_res_from_logres(const MPtexLogRes logres)
{
	MPtexRes res = {
		ptex_res_from_rlog2(logres.u),
		ptex_res_from_rlog2(logres.v),
	};
	return res;
}

size_t BKE_ptex_bytes_per_texel(const MPtexTexelInfo texel_info)
{
	return (ptex_data_type_num_bytes(texel_info.data_type) *
			texel_info.num_channels);
}

size_t BKE_ptex_rect_num_bytes(const MPtexTexelInfo texel_info,
							   const MPtexLogRes logres)
{
	return (BKE_ptex_bytes_per_texel(texel_info) *
			ptex_area_from_logres(logres));
}

size_t BKE_loop_ptex_rect_num_bytes(const MLoopPtex *loop_ptex)
{
	return BKE_ptex_rect_num_bytes(loop_ptex->texel_info,
								   loop_ptex->logres);
}

void BKE_ptex_tess_face_interp(MTessFacePtex *tess_face_ptex,
							   const MLoopInterp *loop_interp,
							   const unsigned int *loop_indices,
							   const int num_loop_indices)
{
	int i;
	BLI_assert(num_loop_indices == 4);
	for (i = 0; i < 4; i++) {
		const MLoopInterp *src = &loop_interp[loop_indices[i]];

		if (i == 0) {
			tess_face_ptex->id = src->id;
		}
		else {
			BLI_assert(tess_face_ptex->id == src->id);
		}

		copy_v2_v2(tess_face_ptex->uv[i], src->uv);
	}
}

void BKE_ptex_update_from_image(MLoopPtex *loop_ptex, const int totloop)
{
	int i;

	// TODO
	Image *image;
	ImBuf *ibuf;

	if (!loop_ptex) {
		return;
	}

	image = loop_ptex->image;
	if (!image) {
		return;
	}

	ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);

	if (!ibuf) {
		return;
	}

	// TODO
	BLI_assert(ibuf->rect);
	BLI_assert(ibuf->num_ptex_regions == totloop);
	BLI_assert(loop_ptex->texel_info.num_channels == 4);

	BLI_assert(ibuf->ptex_regions);
	for (i = 0; i < totloop; i++) {
		// TODO
		MLoopPtex *pt = &loop_ptex[i];
		const MPtexRes dst_res = bke_ptex_res_from_logres(pt->logres);
		const size_t bytes_per_texel = BKE_ptex_bytes_per_texel(pt->texel_info);
		const size_t dst_bytes_per_row = dst_res.u * bytes_per_texel;
		const size_t src_bytes_per_row = ibuf->x * bytes_per_texel;
		unsigned char *dst = pt->rect;
		unsigned char *src = (unsigned char*)ibuf->rect;
		ImPtexRegion *ptex_regions = &ibuf->ptex_regions[i];
		int v;

		BLI_assert(dst_res.u == ptex_regions->width);
		BLI_assert(dst_res.v == ptex_regions->height);

		src += (size_t)(src_bytes_per_row * ptex_regions->y + ptex_regions->x * bytes_per_texel);

		for (v = 0; v < dst_res.v; v++) {
			memcpy(dst, src, dst_bytes_per_row);
			dst += dst_bytes_per_row;
			src += src_bytes_per_row;
		}
	}

	BKE_image_release_ibuf(image, ibuf, NULL);
}

static void *bke_ptex_texels_calloc(const MPtexTexelInfo texel_info,
									const MPtexLogRes logres)
{
	return MEM_callocN(BKE_ptex_rect_num_bytes(texel_info, logres),
					   "bke_ptex_texels_calloc");
}

void BKE_loop_ptex_init(MLoopPtex *loop_ptex,
						const MPtexTexelInfo texel_info,
						const MPtexLogRes logres)
{
	BLI_assert(ptex_rlog2_valid(logres.u));
	BLI_assert(ptex_rlog2_valid(logres.v));
	BLI_assert(texel_info.num_channels >= 1 &&
			   texel_info.num_channels < 255);

	loop_ptex->texel_info = texel_info;
	loop_ptex->logres = logres;

	loop_ptex->rect = bke_ptex_texels_calloc(texel_info, logres);
}

void BKE_loop_ptex_free(MLoopPtex *loop_ptex)
{
	if (loop_ptex->rect) {
		MEM_freeN(loop_ptex->rect);
	}
	//mesh_ptex_pack_free(loop_ptex->pack);
}

#ifdef WITH_PTEX

static void ptex_data_from_float(void *dst_v, const float *src,
								 const PtexDataType data_type,
								 const size_t count)
{
	int i;
	switch (data_type) {
		case MPTEX_DATA_TYPE_UINT8:
			{
				uint8_t *dst = dst_v;
				for (i = 0; i < count; i++) {
					dst[i] = FTOCHAR(src[i]);
				}
				return;
			}

		case MPTEX_DATA_TYPE_FLOAT32:
			{
				memcpy(dst_v, src, sizeof(*src) * count);
				return;
			}
	}

	BLI_assert(!"Invalid PtexDataType");
}

static void *bke_ptex_texels_malloc(const MPtexTexelInfo texel_info,
									const MPtexLogRes logres)
{
	return MEM_mallocN(BKE_ptex_rect_num_bytes(texel_info, logres),
					   "bke_ptex_texels_malloc");
}

/* TODO: for testing, fill initialized loop with some data */
void BKE_loop_ptex_pattern_fill(MLoopPtex *lp, const int index)
{
	const int u_res = ptex_res_from_rlog2(lp->logres.u);
	const int v_res = ptex_res_from_rlog2(lp->logres.v);
	const int bytes_per_texel = BKE_ptex_bytes_per_texel(lp->texel_info);
	char *dst = lp->rect;
	int x, y;
	BLI_assert(lp->texel_info.num_channels <= 4);
	for (y = 0; y < v_res; y++) {
		for (x = 0; x < u_res; x++) {
			const float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
			float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
			const float u = (float)x / (float)(u_res - 1);
			const float v = (float)y / (float)(v_res - 1);
			const float z = ((float)index) / 2.0f;
			rgba[0] = u;
			rgba[1] = v;
			rgba[2] = z;
			rgba[3] = 1.0f;
			if (index == 0) {
				rgba[0] = 1.0f; rgba[1] = 0.0f; rgba[2] = 0.0f;
			}
			else if (index == 1) {
				rgba[0] = 0.0f; rgba[1] = 1.0f; rgba[2] = 0.0f;
			}
			else if (index == 2) {
				rgba[0] = 0.0f; rgba[1] = 0.0f; rgba[2] = 1.0f;
			}
			else if (index == 3) {
				rgba[0] = 1.0f; rgba[1] = 1.0f; rgba[2] = 0.0f;
			}

			if (u == 0 && v == 0) {
				copy_v4_v4(rgba, white);
			}
			else if (u == 0) {
				int c;
				for (c = 0; c < 3; c++) {
					if (rgba[c] == 0) {
						rgba[c] += 0.5;
					}
				}
			}
			ptex_data_from_float(dst, rgba,
								 lp->texel_info.data_type,
								 lp->texel_info.num_channels);

			dst += bytes_per_texel;
		}
	}
}

BLI_STATIC_ASSERT(sizeof(MPtexTexelInfo) == 4, "MPtexTexelInfo size != 4");
BLI_STATIC_ASSERT(sizeof(MPtexLogRes) == 4, "MPtexLogRes size != 4");

static void bpx_rect_from_im_ptex_region(BPXRect *dst,
										 const ImPtexRegion *src)
{
	dst->xbegin = src->x;
	dst->ybegin = src->y;
	dst->xend = src->x + src->width;
	dst->yend = src->y + src->height;
}

/* Constants */
enum {
	BKE_PTEX_NO_ADJ_POLY = -1,

	/* Filtering expects edges to have one or two adjacent polys */
	BKE_PTEX_MAX_ADJ_POLYS = 2
};

typedef struct {
	int polys[BKE_PTEX_MAX_ADJ_POLYS];
} BKEPtexEdgeAdj;

static BKEPtexEdgeAdj *bke_ptex_edge_adj_alloc(const Mesh *me)
{
	BKEPtexEdgeAdj *adj;
	int edge_index;

	adj = MEM_mallocN(sizeof(*adj) * me->totedge, "BKEPtexEdgeAdj");
	for (edge_index = 0; edge_index < me->totedge; edge_index++) {
		int i;
		for (i = 0; i < BKE_PTEX_MAX_ADJ_POLYS; i++) {
			adj[edge_index].polys[i] = BKE_PTEX_NO_ADJ_POLY;
		}
	}

	return adj;
}

/* TODO(nicholasbishop): code like this probably already exists
 * somewhere? */
static BKEPtexEdgeAdj *bke_ptex_edge_adj_init(const Mesh *me)
{
	BKEPtexEdgeAdj *adj;
	int poly_index;

	adj = bke_ptex_edge_adj_alloc(me);
	if (!adj) {
		return NULL;
	}

	for (poly_index = 0; poly_index < me->totpoly; poly_index++) {
		const MPoly *p = &me->mpoly[poly_index];
		int i;
		for (i = 0; i < p->totloop; i++) {
			const int li = p->loopstart + i;
			const MLoop *l = &me->mloop[li];
			const int ei = l->e;
			int j;

			BLI_assert(ei >= 0 && ei < me->totedge);
			for (j = 0; j < BKE_PTEX_MAX_ADJ_POLYS; j++) {
				if (adj[ei].polys[j] == BKE_PTEX_NO_ADJ_POLY) {
					adj[ei].polys[j] = poly_index;
					break;
				}
			}
		}
	}

	return adj;
}

static int bke_ptex_edge_adj_other_poly(const BKEPtexEdgeAdj *edge_adj,
										const int poly_index)
{
	if (edge_adj) {
		int i;
		for (i = 0; i < BKE_PTEX_MAX_ADJ_POLYS; i++) {
			if (edge_adj->polys[i] == poly_index) {
				return edge_adj->polys[BKE_PTEX_MAX_ADJ_POLYS - i - 1];
			}
		}
	}
	return BKE_PTEX_NO_ADJ_POLY;
}

static void ptex_adj_edge(const BKEPtexEdgeAdj *adj,
						  int *adj_loop,
						  BPXEdge *adj_edge,
						  const Mesh *me,
						  const int poly_index1,
						  const int loop_offset,
						  const BPXSide loop_side)
{
	const MPoly *p1 = &me->mpoly[poly_index1];

	BLI_assert(adj_loop);
	BLI_assert(adj_edge);
	BLI_assert(loop_offset >= 0 && loop_offset < p1->totloop);

	/* TODO */
	adj_edge->reverse = true;

	if (loop_side == BPX_SIDE_BOTTOM) {
		/* Previous loop */
		(*adj_loop) = p1->loopstart + ((p1->totloop + loop_offset - 1) %
									   p1->totloop);
		adj_edge->side = BPX_SIDE_LEFT;
	}
	else if (loop_side == BPX_SIDE_LEFT) {
		/* Next loop */
		(*adj_loop) = p1->loopstart + ((loop_offset + 1) % p1->totloop);

		adj_edge->side = BPX_SIDE_BOTTOM;
	}
	else {
		const MLoop *l1;
		int poly_index2;
		int e1_offset;

		if (loop_side == BPX_SIDE_TOP) {
			e1_offset = loop_offset;
		}
		else {
			e1_offset = (p1->totloop + loop_offset - 1) % p1->totloop;
		}
		l1 = &me->mloop[p1->loopstart + e1_offset];

		poly_index2 = bke_ptex_edge_adj_other_poly(&adj[l1->e], poly_index1);
		if (poly_index2 == BKE_PTEX_NO_ADJ_POLY) {
			/* Reuse self */
			(*adj_loop) = p1->loopstart + loop_offset;
			adj_edge->side = loop_side;
		}
		else {
			MPoly *p2 = &me->mpoly[poly_index2];
			int i;
			for (i = 0; i < p2->totloop; i++) {
				const int li2 = p2->loopstart + i;
				const MLoop *l2 = &me->mloop[li2];
				if (l1->e == l2->e) {
					/* TODO(nicholasbishop): probably making an
					 * incorrect assumption here about consistent
					 * winding? */

					/* Next loop */
					
					if (loop_side == BPX_SIDE_TOP) {
						(*adj_loop) = p2->loopstart + ((i + 1) % p2->totloop);
						adj_edge->side = BPX_SIDE_RIGHT;
					}
					else {
						(*adj_loop) = p2->loopstart + i;
						adj_edge->side = BPX_SIDE_TOP;
					}
					return;
				}
			}
		}
		
		/* Reuse self */
		(*adj_loop) = p1->loopstart + loop_offset;
		adj_edge->side = loop_side;
	}
}

static void ptex_filter_borders_update(ImBuf *ibuf, const Mesh *me)
{
	BPXImageBuf *bpx_buf = IMB_imbuf_as_bpx_image_buf(ibuf);
	BKEPtexEdgeAdj *adj = bke_ptex_edge_adj_init(me);
	int i;

	if (!adj) {
		return;
	}

	BLI_assert(bpx_buf);

	for (i = 0; i < me->totpoly; i++) {
		const MPoly *p = &me->mpoly[i];
		int j;

		for (j = 0; j < p->totloop; j++) {
			const int cur_loop = p->loopstart + j;
			int k;

			BPXRect dst_rect;

			BPXRect adj_rect[4];
			BPXEdge adj_edge[4];

			bpx_rect_from_im_ptex_region(&dst_rect,
										 &ibuf->ptex_regions[cur_loop]);
			for (k = 0; k < 4; k++) {
				int adj_loop = -1;

				ptex_adj_edge(adj, &adj_loop, &adj_edge[k], me, i, j, k);
				BLI_assert(adj_loop >= 0);

				bpx_rect_from_im_ptex_region(&adj_rect[k],
											 &ibuf->ptex_regions[adj_loop]);
			}

			if (!BPX_rect_borders_update(bpx_buf, &dst_rect,
										 adj_rect, adj_edge))
			{
				BLI_assert(!"TODO");
			}
		}
	}

	MEM_freeN(adj);
	BPX_image_buf_free(bpx_buf);
}

static bool bpx_type_desc_to_mptex_data_type(const BPXTypeDesc type_desc,
											 PtexDataType *data_type)
{
	if (data_type) {
		switch (type_desc) {
			case BPX_TYPE_DESC_UINT8:
				(*data_type) = MPTEX_DATA_TYPE_UINT8;
				return true;

			case BPX_TYPE_DESC_FLOAT:
				(*data_type) = MPTEX_DATA_TYPE_FLOAT32;
				return true;
		}
	}

	return false;
}

static bool bpx_type_desc_from_mptex_data_type(const PtexDataType data_type,
											   BPXTypeDesc *type_desc) {
	if (type_desc) {
		switch (data_type) {
			case MPTEX_DATA_TYPE_UINT8:
				(*type_desc) = BPX_TYPE_DESC_UINT8;
				return true;

			case MPTEX_DATA_TYPE_FLOAT32:
				(*type_desc) = BPX_TYPE_DESC_FLOAT;
				return true;
		}
	}

	return false;
}

static BPXImageBuf *bpx_image_buf_wrap_loop_ptex(MLoopPtex *loop_ptex)
{
	if (loop_ptex && loop_ptex->rect) {
		const MPtexRes res = bke_ptex_res_from_logres(loop_ptex->logres);
		const MPtexTexelInfo texel_info = loop_ptex->texel_info;
		BPXTypeDesc type_desc = BPX_TYPE_DESC_UINT8;
		
		if (bpx_type_desc_from_mptex_data_type(texel_info.data_type,
											   &type_desc)) {
			return BPX_image_buf_wrap(res.u, res.v,
									  texel_info.num_channels,
									  type_desc, loop_ptex->rect);
		}
	}

	return NULL;
}

/* TODO(nicholasbishop): sync up with code in imb_ptex.c */
static bool ptex_pack_loops(Image **image, Mesh *me, MLoopPtex *loop_ptex)
{
	BPXImageBuf *bpx_dst;
	const int num_loops = me->totloop;
	struct BPXPackedLayout *layout = NULL;
	struct ImBuf *ibuf;
	MPtexTexelInfo texel_info;
	BPXTypeDesc type_desc;
	int i;

	if (!loop_ptex) {
		return false;
	}
	texel_info = loop_ptex->texel_info;

	if (!bpx_type_desc_from_mptex_data_type(texel_info.data_type, &type_desc)) {
		return false;
	}

	/* Create layout */
	layout = BPX_packed_layout_new(num_loops);
	for (i = 0; i < num_loops; i++) {
		BPX_packed_layout_add(layout,
							  ptex_res_from_rlog2(loop_ptex[i].logres.u),
							  ptex_res_from_rlog2(loop_ptex[i].logres.v),
							  i);
	}
	BPX_packed_layout_finalize(layout);

	/* Create ImBuf destination, this will get the region info too so
	 * the layout can then be deleted */
	ibuf = IMB_alloc_from_ptex_layout(layout);
	BPX_packed_layout_delete(layout);
	if (!ibuf) {
		return false;
	}

	/* Allocate BPX wrapper for the ImBuf */
	bpx_dst = IMB_imbuf_as_bpx_image_buf(ibuf);
	if (!bpx_dst) {
		IMB_freeImBuf(ibuf);
		BPX_packed_layout_delete(layout);
		return false;
	}

	/* Copy from loop data into ImBuf */
	for (i = 0; i < num_loops; i++) {
		MLoopPtex *lp = &loop_ptex[i];
		BPXImageBuf *bpx_src = bpx_image_buf_wrap_loop_ptex(lp);
		const ImPtexRegion *region = &ibuf->ptex_regions[i];
		bool r;
		BLI_assert(bpx_src);

		r = BPX_image_buf_pixels_copy(bpx_dst, bpx_src, region->x, region->y);
		BLI_assert(r);
		
		BPX_image_buf_free(bpx_src);
	}

	// TODO
	IMB_rectfill_alpha(ibuf, 1);
	ptex_filter_borders_update(ibuf, me);

	BPX_image_buf_free(bpx_dst);

	// TODO
	IMB_rectfill_alpha(ibuf, 1);

	IMB_float_from_rect(ibuf);

	(*image) = BKE_image_add_from_imbuf(ibuf);

	return true;
}

Image *BKE_ptex_mesh_image_get(struct Object *ob,
							   const char layer_name[MAX_CUSTOMDATA_LAYER_NAME])
{
	Mesh *me = BKE_mesh_from_object(ob);
	if (me) {
		MLoopPtex *loop_ptex = CustomData_get_layer_named(&me->ldata,
														  CD_LOOP_PTEX,
														  layer_name);
		if (loop_ptex) {
			// TODO
			if (!loop_ptex->image) {
				// TODO
				const bool r = ptex_pack_loops(&loop_ptex->image, me, loop_ptex);
				BLI_assert(r);
			}
			else if (loop_ptex->image->tpageflag & IMA_TPAGE_REFRESH) {
				//mesh_ptex_pack_textures_free(loop_ptex->pack);
				loop_ptex->image->tpageflag &= ~IMA_TPAGE_REFRESH;
			}
			// TODO

			/* if (!loop_ptex->pack->mapping_texture) { */
			/* 	mesh_ptex_pack_update_textures(loop_ptex->pack); */
			/* } */

			return loop_ptex->image;
		}
	}
	return NULL;
}

PtexDataType BKE_ptex_texel_data_type(const MPtexTexelInfo texel_info)
{
	return texel_info.data_type;
}

PtexDataType BKE_loop_ptex_texel_data_type(const MLoopPtex *loop_ptex)
{
	return loop_ptex->texel_info.data_type;
}

void BKE_loop_ptex_resize(MLoopPtex *loop_ptex, const MPtexLogRes dst_logres)
{
	MPtexRes src_res = bke_ptex_res_from_logres(loop_ptex->logres);
	MPtexRes dst_res = bke_ptex_res_from_logres(dst_logres);

	/* Same between src and dst */
	const MPtexTexelInfo texel_info = loop_ptex->texel_info;

	// TODO
	BLI_assert(BKE_ptex_texel_data_type(texel_info) ==
			   MPTEX_DATA_TYPE_UINT8);
	BLI_assert(texel_info.num_channels <= 4);

	if (loop_ptex->rect) {
		/* Allocate rect for new size */
		void *dst_rect = bke_ptex_texels_malloc(texel_info, dst_logres);
		unsigned char *dst_texel = dst_rect;
		int u, v;
		const float step_u = (float)(src_res.u - 1) / (float)dst_res.u;
		const float step_v = (float)(src_res.v - 1) / (float)dst_res.v;
		const float half_step_u = step_u * 0.5f;
		float fv = step_v * 0.5f;
		const size_t bytes_per_texel = BKE_ptex_bytes_per_texel(texel_info);

		for (v = 0; v < dst_res.v; v++) {
			float fu = half_step_u;
			for (u = 0; u < dst_res.u; u++) {
				BLI_assert(fu >= 0 && fu <= src_res.u);
				BLI_assert(fv >= 0 && fv <= src_res.v);
				BLI_bilinear_interpolation_char(loop_ptex->rect, dst_texel,
												src_res.u, src_res.v,
												texel_info.num_channels,
												fu, fv);
				dst_texel += bytes_per_texel;
				fu += step_u;
			}
			fv += step_v;
		}

		MEM_freeN(loop_ptex->rect);
		loop_ptex->rect = dst_rect;
		loop_ptex->logres = dst_logres;
	}
	else {
		BLI_assert(!"TODO: BKE_loop_ptex_resize");
	}
}

// All TODO
void BKE_ptex_derived_mesh_inject(struct DerivedMesh *dm)
{
	MLoopInterp *loop_interp;
	const int num_polys = dm->getNumPolys(dm);
	const int num_loops = dm->getNumLoops(dm);
	const MPoly *mpoly = dm->getPolyArray(dm);
	int i;

	BLI_assert(!CustomData_has_layer(&dm->loopData, CD_LOOP_INTERP));

	loop_interp = CustomData_add_layer(&dm->loopData, CD_LOOP_INTERP,
									   CD_CALLOC, NULL, num_loops);
	for (i = 0; i < num_polys; i++) {
		const MPoly *p = &mpoly[i];
		int j;

		for (j = 0; j < p->totloop; j++) {
			const int orig_loop_index = p->loopstart + j;
			MLoopInterp *dst = &loop_interp[orig_loop_index];

			dst->id = orig_loop_index;
		}
	}
}

struct DerivedMesh *BKE_ptex_derived_mesh_subdivide(struct DerivedMesh *dm)
{
	/* TODO? */
	if (CustomData_has_layer(&dm->faceData, CD_TESSFACE_PTEX)) {
		return dm;
	}
	else {
		SubsurfModifierData smd = {{NULL}};

		// TODO
		SubsurfFlags flags = SUBSURF_IS_FINAL_CALC;
		smd.subdivType = ME_SIMPLE_SUBSURF;
		smd.levels = 1;
		smd.renderLevels = 1;

		return subsurf_make_derived_from_derived(dm, &smd, NULL, flags);
	}
}

bool BKE_ptex_log_res_from_res(MPtexLogRes *logres,
							   const int u, const int v)
{
	// TODO: upper limit
	if (logres &&
		is_power_of_2_i(u) && u >= 0 &&
		is_power_of_2_i(v) && v >= 0)
	{
		logres->u = (int)(log(u) / M_LN2);
		logres->v = (int)(log(v) / M_LN2);
		return true;
	}
	else {
		return false;
	}
}

bool BKE_ptex_texel_info_init(MPtexTexelInfo *texel_info,
							  const PtexDataType data_type,
							  const int num_channels)
{
	/* TODO(nicholasbishop): for now limit number of channels to
	 * four */

	if (texel_info && num_channels >= 0 && num_channels <= 4) {
		texel_info->data_type = data_type;
		texel_info->num_channels = num_channels;
		return true;
	}

	return false;
}

static bool bke_ptex_texel_info_from_bpx(const BPXImageInput *input,
										 MPtexTexelInfo *texel_info)
{
	BPXTypeDesc type_desc = BPX_TYPE_DESC_UINT8;
	int num_channels = 0;
	PtexDataType data_type = MPTEX_DATA_TYPE_UINT8;

	if (!input || !texel_info) {
		return false;
	}

	if (!BPX_image_input_type_desc(input, &type_desc)) {
		return false;
	}

	if (!BPX_image_input_num_channels(input, &num_channels)) {
		return false;
	}

	if (!bpx_type_desc_to_mptex_data_type(type_desc, &data_type)) {
		return false;
	}

	if (!BKE_ptex_texel_info_init(texel_info, data_type, num_channels)) {
		return false;
	}

	return true;
}

static bool mesh_ptex_import_loop(BPXImageInput *src,
								  MLoopPtex *loop_ptex,
								  const MPtexTexelInfo texel_info,
								  const int width,
								  const int height)
{
	BPXImageBuf *dst;
	MPtexLogRes logres;
	bool result;

	if (!BKE_ptex_log_res_from_res(&logres, width, height)) {
		return false;
	}

	BKE_loop_ptex_init(loop_ptex, texel_info, logres);

	dst = bpx_image_buf_wrap_loop_ptex(loop_ptex);
	if (!dst) {
		return false;
	}

	result = BPX_image_input_read(dst, src);

	if (result) {
		result = BPX_image_buf_transform(dst);
	}

	BPX_image_buf_free(dst);
	return result;
}

bool TODO_test_write_loop(MLoopPtex *lp, const char *filename);
bool TODO_test_write_loop(MLoopPtex *lp, const char *filename)
{
	BPXImageBuf *buf;
	bool r;

	buf = bpx_image_buf_wrap_loop_ptex(lp);
	if (!buf) {
		return false;
	}

	r = TODO_test_write(buf, filename);

	BPX_image_buf_free(buf);

	return r;
}

static bool mesh_ptex_import_quad(BPXImageInput *src,
								  MLoopPtex *loop_ptex,
								  const MPtexTexelInfo texel_info,
								  const int width,
								  const int height)
{
	BPXImageBuf *all_buf;
	BPXImageBuf *dst[4];
	MPtexLogRes logres, logres_transposed;
	bool r;
	int i;

	if (!BKE_ptex_log_res_from_res(&logres, width, height)) {
		return false;
	}

	all_buf = BPX_image_buf_alloc_empty();

	if (!all_buf) {
		return false;
	}
	
	if (!BPX_image_input_read(all_buf, src)) {
		BPX_image_buf_free(all_buf);
		return false;
	}

	/* Allocate the four loops as quadrants of the Ptex face */
	if (logres.u >= 1) {
		logres.u--;
	}
	if (logres.v >= 1) {
		logres.v--;
	}
	logres_transposed.u = logres.v;
	logres_transposed.v = logres.u;
	for (i = 0; i < 4; i++) {
		MLoopPtex *lp = &loop_ptex[i];

		BKE_loop_ptex_init(lp, texel_info,
						   (i % 2 == 0) ? logres : logres_transposed);

		dst[i] = bpx_image_buf_wrap_loop_ptex(lp);
		BLI_assert(dst[i]);
	}

	r = BPX_image_buf_quad_split(dst, all_buf);
	BLI_assert(r);

	for (i = 0; i < 4; i++) {
		BPX_image_buf_free(dst[i]);
	}

	BPX_image_buf_free(all_buf);
	return true;
}

bool BKE_ptex_import(Mesh *me, const char *filepath)
{
	BPXImageInput *input = BPX_image_input_from_filepath(filepath);
	MPtexTexelInfo texel_info;
	MLoopPtex *loop_ptex;
	int ptex_face_id;
	int i;

	if (!input) {
		return false;
	}

	if (!bke_ptex_texel_info_from_bpx(input, &texel_info)) {
		BPX_image_input_free(input);
		return false;
	}

	// Layer name TODO
	loop_ptex = CustomData_add_layer_named(&me->ldata, CD_LOOP_PTEX,
										   CD_CALLOC, NULL, me->totloop,
										   "<TODO>");

	if (!loop_ptex) {
		BPX_image_input_free(input);
		return false;
	}

	// TODO(nicholasbishop): get number of faces, check if it matches
	// model?

	ptex_face_id = 0;
	for (i = 0; i < me->totpoly; i++) {
		const MPoly *p = &me->mpoly[i];
		int j;

		for (j = 0; j < p->totloop; j++) {
			const int loop_index = p->loopstart + j;
			int width = 0, height = 0;

			if (!BPX_image_input_seek_subimage(input, ptex_face_id,
											   &width,
											   &height)) {
				// TODO(nicholasbishop)
				BLI_assert(!"ptex import error");
				i = me->totpoly;
				break;
			}

			if (p->totloop == 4) {
				if (!mesh_ptex_import_quad(input, &loop_ptex[loop_index],
										   texel_info, width, height))
				{
					// TODO(nicholasbishop)
					BLI_assert(!"ptex import error");
					i = me->totpoly;
					break;
				}

				ptex_face_id++;
				/* Quad split does four loops, so exit inner loop */
				break;
			}
			else {
				if (!mesh_ptex_import_loop(input, &loop_ptex[loop_index],
										   texel_info, width, height))
				{
					// TODO(nicholasbishop)
					BLI_assert(!"ptex import error");
					i = me->totpoly;
					break;
				}

				ptex_face_id++;
			}
		}
	}

	BPX_image_input_free(input);

	return true;
}
#else
/* Stubs if WITH_PTEX is not defined */

struct DerivedMesh;

void BKE_ptex_derived_mesh_inject(struct DerivedMesh *UNUSED(dm))
{
}

struct DerivedMesh *BKE_ptex_derived_mesh_subdivide(struct DerivedMesh *dm)
{
	return dm;
}

struct Image *BKE_ptex_mesh_image_get(struct Object *UNUSED(ob),
									  const char UNUSED(layer_name[]))
{
	return NULL;
}

// TODO: should return error
void BKE_loop_ptex_resize(MLoopPtex *UNUSED(loop_ptex),
						  const MPtexLogRes UNUSED(dst_logres))
{
}

bool BKE_ptex_import(struct Mesh *UNUSED(me), const char *UNUSED(filepath))
{
	return false;
}

#endif
