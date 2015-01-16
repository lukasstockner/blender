#include <iostream>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "BPX_pack.h"
#include "ptex_packed_layout.h"

OIIO_NAMESPACE_USING

// TODO
static const int nthreads = 1;

/* Directed edges (uv1 -> uv2)
 *
 * 01______11
 *  |      |
 *  |      |
 *  |______|
 * 00      10
 */
#if 0
typedef enum {
	/* Bottom */
	BPX_EDGE_00_10 = 0,
	BPX_EDGE_10_00 = 1,

	/* Right */
	BPX_EDGE_10_11 = 2,
	BPX_EDGE_11_10 = 3,

	/* Top */
	BPX_EDGE_11_01 = 4,
	BPX_EDGE_01_11 = 5,

	/* Left */
	BPX_EDGE_01_00 = 6,
	BPX_EDGE_00_01 = 7,
} BPXEdge;
#endif

static TypeDesc bpx_type_desc_to_oiio_type_desc(const BPXTypeDesc type_desc)
{
	switch (type_desc) {
		case BPX_TYPE_DESC_UINT8:
			return TypeDesc::UINT8;

		case BPX_TYPE_DESC_FLOAT:
			return TypeDesc::FLOAT;
	}

	assert(!"Invalid BPXTypeDesc");
}

static bool bpx_type_desc_from_oiio_type_desc(const TypeDesc src,
											  BPXTypeDesc *dst)
{
	if (dst) {
		if (src == TypeDesc::UINT8) {
			(*dst) = BPX_TYPE_DESC_UINT8;
			return true;
		}
		else if (src == TypeDesc::FLOAT) {
			(*dst) = BPX_TYPE_DESC_FLOAT;
			return true;
		}
	}
	return false;
}

#define ASSERT_BPX_RECT_VALID(r_) \
	assert(r_.xbegin >= 0 && r_.xend > r_.xbegin && \
		   r_.ybegin >= 0 && r_.yend > r_.ybegin)

static ROI bpx_rect_to_oiio_roi(const BPXRect &rect)
{
	ASSERT_BPX_RECT_VALID(rect);

	return ROI(rect.xbegin, rect.xend, rect.ybegin, rect.yend);
}

static BPXImageBuf *bpx_image_buf_from_oiio_image_buf(ImageBuf *buf)
{
	return reinterpret_cast<BPXImageBuf *>(buf);
}

static ImageBuf *bpx_image_buf_to_oiio_image_buf(BPXImageBuf *buf)
{
	return reinterpret_cast<ImageBuf *>(buf);
}

static const ImageBuf *bpx_image_buf_to_oiio_image_buf(const BPXImageBuf *buf)
{
	return reinterpret_cast<const ImageBuf *>(buf);
}

static BPXImageInput *bpx_image_input_from_oiio_image_input(ImageInput *in)
{
	return reinterpret_cast<BPXImageInput *>(in);
}

static ImageInput *bpx_image_input_to_oiio_image_input(BPXImageInput *in)
{
	return reinterpret_cast<ImageInput *>(in);
}

static const ImageInput *bpx_image_input_to_oiio_image_input(const BPXImageInput *in)
{
	return reinterpret_cast<const ImageInput *>(in);
}

BPXImageBuf *BPX_image_buf_alloc_empty(void)
{
	return bpx_image_buf_from_oiio_image_buf(new ImageBuf());
}

BPXImageBuf *BPX_image_buf_wrap(const int width, const int height,
								const int num_channels,
								const BPXTypeDesc type_desc,
								void * const pixels)
{
	// TODO
	const bool r = OIIO::attribute("threads", 1);
	assert(r);

	if (width > 0 && height > 0 && num_channels > 0 &&
		(type_desc == BPX_TYPE_DESC_UINT8 ||
		 type_desc == BPX_TYPE_DESC_FLOAT)) {
		const TypeDesc td = bpx_type_desc_to_oiio_type_desc(type_desc);
		const ImageSpec spec(width, height, num_channels, td);
		
		return bpx_image_buf_from_oiio_image_buf(new ImageBuf(spec, pixels));
	}
	else {
		return NULL;
	}
}

void BPX_image_buf_free(BPXImageBuf * const buf)
{
	delete bpx_image_buf_to_oiio_image_buf(buf);
}

bool BPX_image_buf_pixels_copy_partial(BPXImageBuf *bpx_dst,
									   const BPXImageBuf *bpx_src,
									   int xbegin, int ybegin,
									   const BPXRect *src_rect)
{
	if (bpx_dst && bpx_src) {
		ImageBuf *dst = bpx_image_buf_to_oiio_image_buf(bpx_dst);
		const ImageBuf *src = bpx_image_buf_to_oiio_image_buf(bpx_src);
		ROI src_roi = src->roi();
		const int zbegin = 0;
		const int chbegin = 0;

		if (src_rect) {
			src_roi = bpx_rect_to_oiio_roi(*src_rect);
		}
		
		return ImageBufAlgo::paste(*dst, xbegin, ybegin, zbegin, chbegin,
								   *src, src_roi);

		return dst->copy(*src);
	}
	return false;
}

/* TODO, should probably just remove in favor of new func above */
bool BPX_image_buf_pixels_copy(BPXImageBuf *bpx_dst, const BPXImageBuf *bpx_src,
							   const int xbegin, const int ybegin)
{
	return BPX_image_buf_pixels_copy_partial(bpx_dst, bpx_src, xbegin, ybegin, NULL);
}

static ROI bpx_roi_from_side(const ROI &roi, const BPXSide side)
{
	switch (side) {
		case BPX_SIDE_BOTTOM:
			return ROI(roi.xbegin, roi.xend, roi.ybegin, roi.ybegin + 1);

		case BPX_SIDE_RIGHT:
			return ROI(roi.xend - 1, roi.xend, roi.ybegin, roi.yend);

		case BPX_SIDE_TOP:
			return ROI(roi.xbegin, roi.xend, roi.yend - 1, roi.yend);

		case BPX_SIDE_LEFT:
			return ROI(roi.xbegin, roi.xbegin + 1, roi.ybegin, roi.yend);

		case BPX_NUM_SIDES:
			break;
	}

	assert(!"Invalid side");
}

static ROI bpx_filter_border_roi_from_side(const ROI &dst_roi,
										   const BPXSide side)
{
	ROI roi = bpx_roi_from_side(dst_roi, side);
	switch (side) {
		case BPX_SIDE_BOTTOM:
			roi.ybegin--;
			roi.yend--;
			break;

		case BPX_SIDE_RIGHT:
			roi.xbegin++;
			roi.xend++;
			break;

		case BPX_SIDE_TOP:
			roi.ybegin++;
			roi.yend++;
			break;

		case BPX_SIDE_LEFT:
			roi.xbegin--;
			roi.xend--;
			break;

		case BPX_NUM_SIDES:
			assert(!"Invalid side");
	}

	return roi;
}

// General TODO: reduce scratch buffer allocations

enum BPXTransform {
	BPX_TRANSFORM_NONE      = (1 << 0),
	BPX_TRANSFORM_TRANSPOSE = (1 << 1),
	BPX_TRANSFORM_FLIP      = (1 << 2),
	BPX_TRANSFORM_FLOP      = (1 << 3),
};

inline BPXTransform &operator |=(BPXTransform &a, const BPXTransform b)
{
    return a = (BPXTransform)(a | b);
}

static BPXTransform bpx_calc_edge_transform(const BPXEdge from,
											BPXEdge to)
{
	const bool from_horiz = (from.side % 2) == 1;
	bool to_horiz = (to.side % 2) == 1;
	BPXTransform transform = BPX_TRANSFORM_NONE;

	if (from_horiz != to_horiz) {
		transform |= BPX_TRANSFORM_TRANSPOSE;
		to.reverse = !to.reverse;
	}

	if (from.reverse != to.reverse) {
		transform |= (from_horiz ?
					  BPX_TRANSFORM_FLOP :
					  BPX_TRANSFORM_FLIP);
	}

	return transform;
}

static void bpx_apply_edge_transform(ImageBuf &buf,
									 const BPXTransform transform)
{
	if (transform & BPX_TRANSFORM_TRANSPOSE) {
		ImageBuf tmp;
		const bool result = ImageBufAlgo::transpose(tmp, buf);
		assert(result);
		buf.swap(tmp);
	}
	if (transform & BPX_TRANSFORM_FLIP) {
		ImageBuf tmp;
		const bool result = ImageBufAlgo::flip(tmp, buf);
		assert(result);
		buf.swap(tmp);
	}
	if (transform & BPX_TRANSFORM_FLOP) {
		ImageBuf tmp;
		const bool result = ImageBufAlgo::flop(tmp, buf);
		assert(result);
		buf.swap(tmp);
	}
}

static void bpx_side_to_image_buf(ImageBuf &dst, const ImageBuf &src,
								  const ROI &src_roi, const BPXSide src_side)
{
	const ROI side_roi = bpx_roi_from_side(src_roi, src_side);
	const bool result = ImageBufAlgo::cut(dst, src, side_roi);
	assert(result);
}
							  
static void bpx_write_border(ImageBuf &dst, const ROI &dst_roi,
							 ImageBuf &src, const BPXTransform transform)
{
	bpx_apply_edge_transform(src, transform);

	ImageBuf tmp;
	ROI tmp_roi = src.roi();
	tmp_roi.xbegin = 0;
	tmp_roi.xend = dst_roi.xend - dst_roi.xbegin;
	tmp_roi.ybegin = 0;
	tmp_roi.yend = dst_roi.yend - dst_roi.ybegin;
	const bool r1 = ImageBufAlgo::resize(tmp, src, NULL, tmp_roi);
	assert(r1);

	const bool r2 = ImageBufAlgo::paste(dst,
										dst_roi.xbegin,
										dst_roi.ybegin,
										dst_roi.zbegin,
										dst_roi.chbegin,
										tmp);
	assert(r2);
}

static void bpx_create_border(ImageBuf &buf,
							  const ROI &dst_roi, const BPXEdge dst_edge,
							  const ROI &src_roi, const BPXEdge src_edge)
{
	// Copy source border into new image buffer
	ImageBuf side_buf;
	bpx_side_to_image_buf(side_buf, buf, src_roi, src_edge.side);

	const BPXTransform transform = bpx_calc_edge_transform(src_edge,
														   dst_edge);

	const ROI dst_edge_roi =
		bpx_filter_border_roi_from_side(dst_roi, dst_edge.side);
	bpx_write_border(buf, dst_edge_roi, side_buf, transform);
}

static const int CORNER_NUM_SOURCES = 2;
static bool bpx_corner_average(ImageBuf &buf, const int dst_co[2],
							   const int src_co[CORNER_NUM_SOURCES][2])
{
	const int nchannels = buf.nchannels();
	std::vector<float> a(nchannels, 0.0f);
	std::vector<float> b(nchannels, 0.0f);

	buf.getpixel(src_co[0][0], src_co[0][1], a.data());
	buf.getpixel(src_co[1][0], src_co[1][1], b.data());

	for (int i = 0; i < nchannels; i++) {
		a[i] = (a[i] + b[i]) * 0.5f;
	}

	buf.setpixel(dst_co[0], dst_co[1], a.data());

	return true;
}

bool BPX_rect_borders_update(BPXImageBuf *bpx_buf,
							 const BPXRect *dst_rect,
							 const BPXRect src_rect[BPX_NUM_SIDES],
							 const BPXEdge src_edge[BPX_NUM_SIDES])
{
	if (!bpx_buf || !dst_rect) {
		return false;
	}

	ImageBuf &buf = *bpx_image_buf_to_oiio_image_buf(bpx_buf);
	const ROI dst_roi = bpx_rect_to_oiio_roi(*dst_rect);

	// Sample adjacent regions to create filter edges
	for (int i = 0; i < BPX_NUM_SIDES; i++) {
		const ROI src_roi = bpx_rect_to_oiio_roi(src_rect[i]);
		const BPXSide dst_side = static_cast<BPXSide>(i);
		const bool dst_reverse = false;
		const BPXEdge dst_edge = {dst_side, dst_reverse};
		bpx_create_border(buf, dst_roi, dst_edge, src_roi, src_edge[i]);
	}

	// Average adjacent borders to fill in corners (not really correct
	// but I'm guessing the difference won't be visible, and anyway
	// this is only for bilinear filtering)
	const int dst_co[BPX_NUM_SIDES][2] = {
		{dst_rect->xbegin - 1, dst_rect->ybegin - 1},
		{dst_rect->xend      , dst_rect->ybegin - 1},
		{dst_rect->xend      , dst_rect->yend      },
		{dst_rect->xbegin - 1, dst_rect->yend      }
	};
	const int src_co[BPX_NUM_SIDES][CORNER_NUM_SOURCES][2] = {
		{{dst_co[0][0] + 1, dst_co[0][1]    },
		 {dst_co[0][0],     dst_co[0][1] + 1}},
		{{dst_co[1][0] - 1, dst_co[1][1]},
		 {dst_co[1][0],     dst_co[1][1] + 1}},
		{{dst_co[2][0] - 1, dst_co[2][1]},
		 {dst_co[2][0],     dst_co[2][1] - 1}},
		{{dst_co[3][0] + 1, dst_co[3][1]},
		 {dst_co[3][0],     dst_co[3][1] - 1}}
	};
	for (int i = 0; i < BPX_NUM_SIDES; i++) {
		if (!bpx_corner_average(buf, dst_co[i], src_co[i])) {
			return false;
		}
	}

	return true;
}

BPXImageInput *BPX_image_input_from_filepath(const char filepath[])
{
	ImageInput *in = ImageInput::open(filepath);
	if (in) {
		return bpx_image_input_from_oiio_image_input(in);
	}
	return NULL;
}

void BPX_image_input_free(BPXImageInput *in)
{
	delete bpx_image_input_to_oiio_image_input(in);
}
							  
bool BPX_image_input_type_desc(const BPXImageInput *bpx_in,
							   BPXTypeDesc *type_desc)
{
	if (bpx_in && type_desc) {
		const ImageInput *input = bpx_image_input_to_oiio_image_input(bpx_in);

		return bpx_type_desc_from_oiio_type_desc(input->spec().format,
												 type_desc);
	}
	return false;
}

bool BPX_image_input_num_channels(const BPXImageInput *bpx_in, int *num_channels)
{
	if (bpx_in && num_channels) {
		const ImageInput *input = bpx_image_input_to_oiio_image_input(bpx_in);

		(*num_channels) = input->spec().nchannels;
		return true;
	}
	return false;
}

bool BPX_image_input_seek_subimage(BPXImageInput *bpx_in, const int subimage,
								   int *width, int *height)
{
	if (bpx_in && width && height) {
		ImageInput *in = bpx_image_input_to_oiio_image_input(bpx_in);
		ImageSpec spec;

		/* Gotcha in the API, if already on this subimage it doesn't
		 * update the image spec */
		if (in->current_subimage() == subimage) {
			spec = in->spec();
		}
		else if (!in->seek_subimage(subimage, spec)) {
			return false;
		}
		
		(*width) = spec.width;
		(*height) = spec.height;

		return true;
	}
	return false;
}

bool BPX_image_input_read(BPXImageBuf *bpx_dst, BPXImageInput *bpx_src)
{
	if (bpx_dst && bpx_src) {
		ImageBuf *dst = bpx_image_buf_to_oiio_image_buf(bpx_dst);
		ImageInput *src = bpx_image_input_to_oiio_image_input(bpx_src);
		void *data = dst->localpixels();

		if (data) {
			return src->read_image(dst->spec().format, data);
		}
		else {
			dst->reset(src->spec());
			return src->read_image(dst->spec().format, dst->localpixels());
		}
	}
	return false;
}

bool TODO_test_write(BPXImageBuf *bpx_buf, const char *path)
{
	const ImageBuf *buf = bpx_image_buf_to_oiio_image_buf(bpx_buf);
	return buf->write(path);
}

static bool bpx_image_buf_quad_split(ImageBuf *dst[4], const ImageBuf *src,
									 ROI dst_roi[4])
{
	const int width = src->spec().width;
	const int height = src->spec().height;
	const int half_width = width / 2;
	const int half_height = height / 2;
	const int left1 = 0;
	const int left2 = (width == 1) ? 0 : half_width;
	const int right1 = (width == 1) ? 1 : half_width;
	const int right2 = width;
	const int bottom1 = 0;
	const int bottom2 = (height == 1) ? 0 : half_height;
	const int top1 = (height == 1) ? 1 : half_height;
	const int top2 = height;
	
	for (int i = 0; i < 4; i++) {
		ROI src_roi;
		
		if (i == 0) {
			src_roi = ROI(left1, right1, bottom1, top1);
		}
		else if (i == 1) {
			src_roi = ROI(left2, right2, bottom1, top1);
		}
		else if (i == 2) {
			src_roi = ROI(left2, right2, bottom2, top2);
		}
		else if (i == 3) {
			src_roi = ROI(left1, right1, bottom2, top2);
		}

		ImageBuf tmp1;
		if (!ImageBufAlgo::cut(tmp1, *src, src_roi)) {
			return false;
		}

		ImageBuf tmp2;
		if (i == 0) {
			if (!ImageBufAlgo::flipflop(tmp2, tmp1)) {
				return false;
			}
		}
		else if (i == 1) {
			if (!ImageBufAlgo::transpose(tmp2, tmp1)) {
				return false;
			}
			tmp1.swap(tmp2);
			tmp2.clear();
			if (!ImageBufAlgo::flop(tmp2, tmp1)) {
				return false;
			}
		}
		else if (i == 2) {
			if (!ImageBufAlgo::cut(tmp2, tmp1)) {
				return false;
			}
		}
		else if (i == 3) {
			if (!ImageBufAlgo::transpose(tmp2, tmp1)) {
				return false;
			}
			tmp1.swap(tmp2);
			tmp2.clear();
			if (!ImageBufAlgo::flip(tmp2, tmp1)) {
				return false;
			}
		}

		if (dst_roi) {
			const int zbegin = 0;
			const int chbegin = 0;
			if (!ImageBufAlgo::paste(*dst[i],
									 dst_roi[i].xbegin, dst_roi[i].ybegin,
									 zbegin, chbegin, tmp2))
			{
				return false;
			}
		}
		else {
			if (!dst[i]->copy_pixels(tmp2)) {
				return false;
			}
		}
	}

	return true;
}

bool BPX_image_buf_quad_split(BPXImageBuf *bpx_dst[4],
							  const BPXImageBuf *bpx_src)
{
	if (!bpx_src) {
		return false;
	}
	
	const ImageBuf *src = bpx_image_buf_to_oiio_image_buf(bpx_src);

	ImageBuf *dst[4];
	for (int i = 0; i < 4; i++) {
		if (bpx_dst[i]) {
			dst[i] = bpx_image_buf_to_oiio_image_buf(bpx_dst[i]);
		}
		else {
			return false;
		}
	}

	return bpx_image_buf_quad_split(dst, src, NULL);
}

// TODO, fix name
bool BPX_image_buf_transform(BPXImageBuf *bpx_dst)
{
	if (!bpx_dst) {
		return false;
	}

	ImageBuf *dst = bpx_image_buf_to_oiio_image_buf(bpx_dst);
	ImageBuf tmp;
	if (!ImageBufAlgo::flipflop(tmp, *dst)) {
		return false;
	}

	if (!dst->copy_pixels(tmp)) {
		return false;
	}

	return true;
}

static const int QUAD_NUM_SIDES = 4;
struct BPXPtexFaceSpec {
	int h;
	int w;
	int qw[QUAD_NUM_SIDES];
	int qh[QUAD_NUM_SIDES];
	bool subface;
};
typedef std::vector<BPXPtexFaceSpec> BPXPtexFaceSpecVec;

// TODO, messy code

static BPXPtexFaceSpec bpx_ptex_face_spec(const ImageSpec &spec,
										  const bool subface)
{
	BPXPtexFaceSpec result;

	// TODO
	result.subface = subface;

	result.w = spec.width;
	result.h = spec.height;
	if (!result.subface) {
		const int hw = std::max(1, result.w / 2);
		const int hh = std::max(1, result.h / 2);
		int i;
		for (i = 0; i < QUAD_NUM_SIDES; i++) {
			if (i % 2 == 0) {
				result.qw[i] = hw;
				result.qh[i] = hh;
			}
			else {
				result.qw[i] = hh;
				result.qh[i] = hw;
			}
		}
	}
	
	return result;
}

// Return number of subimages.
//
// TODO(nicholasbishop): probably a more direct way to access this
// somehow?
static int bpx_ptex_num_subimages(const std::vector<int> &face_vert_counts)
{
	const size_t end = face_vert_counts.size();
	int count = 0;
	for (size_t i = 0; i < end; i++) {
		const int verts_in_face = face_vert_counts[i];
		count += (verts_in_face == 4) ? 1 : verts_in_face;
	}
	return count;
}

// Reset vector as a copy of image metadata
static bool bpx_array_attrib_copy(std::vector<int> &vec, const char *key,
								  const ImageSpec &spec)
{
	const ImageIOParameter *p = spec.find_attribute(key);
	if (!p || p->type().basetype != TypeDesc::INT32) {
		return false;
	}

	const int *src = static_cast<const int *>(p->data());
	if (!src) {
		return false;
	}

	vec = std::vector<int>(src, src + p->type().numelements());
	return true;
}


// TODO(nicholasbishop): some of this code for packing ptx files can
// be simplified if and when OIIO provides access to additional Ptex
// data like "isSubface" and adjacency. For now use the standard (but
// not required?) attributes as a workaround.
//
// Reference for Ptex standard metakeys: http://ptex.us/metakeys.html

static bool bpx_ptex_face_vert_counts(std::vector<int> &vec, ImageInput &in)
{
	return bpx_array_attrib_copy(vec, "PtexFaceVertCounts", in.spec());
}

static bool bpx_ptex_face_vert_indices(std::vector<int> &vec, ImageInput &in)
{
	return bpx_array_attrib_copy(vec, "PtexFaceVertIndices", in.spec());
}

// Return number of vertices, assuming indices start at zero and none
// are skipped
static int bpx_ptex_num_verts(const std::vector<int> &face_vert_indices)
{
	if (face_vert_indices.empty()) {
		return 0;
	}
	else {
		return *std::max_element(face_vert_indices.begin(),
								 face_vert_indices.end());
	}
}

static const int BPX_ADJ_NONE = -1;
struct BPXMeshEdge {
	int vert;
	int faces[2];
};

// Outer vector index is a vertex index. The vertex stored in the
// BPXMeshEdge is the whichever one has a higher index.
//
// TODO(nicholasbishop)
typedef std::vector<std::vector<BPXMeshEdge> > BPXMeshEdges;

static bool bpx_ptex_file_mesh_edges(BPXMeshEdges &edges,
									 const std::vector<int> &face_vert_counts,
									 const std::vector<int> &face_vert_indices)
{
	const int num_verts = bpx_ptex_num_verts(face_vert_indices);
	const int num_faces = face_vert_counts.size();

	edges.clear();
	edges.resize(num_verts);

	int vstart = 0;
	for (int face_index = 0; face_index < num_faces; face_index++) {
		const int num_verts = face_vert_counts[face_index];
		const int *verts = &face_vert_indices[vstart];

		for (int fv = 0; fv < num_verts; fv++) {
			int v1 = verts[fv];
			int v2 = verts[(fv + 1) % num_verts];
			if (v1 > v2) {
				std::swap(v1, v2);
			}
			else if (v1 == v2) {
				return false;
			}

			std::vector<BPXMeshEdge> &vec = edges.at(v1);
			bool found = false;
			for (int k = 0; k < vec.size(); k++) {
				BPXMeshEdge &edge = vec[k];
				if (edge.vert == v2) {
					if (edge.faces[0] == BPX_ADJ_NONE) {
						// Should have been set already
						return false;
					}
					else if (edge.faces[1] == BPX_ADJ_NONE) {
						edge.faces[1] = face_index;
					}
					else {
						// Assume zero, one, or two faces per edge
						return false;
					}

					found = true;
					break;
				}
			}

			if (!found) {
				const BPXMeshEdge e = {v2, {face_index, BPX_ADJ_NONE}};
				edges.at(v1).push_back(e);
			}
		}

		vstart += num_verts;
	}

	return true;
}

static BPXRect bpx_rect_from_layout_item(const PtexPackedLayout::Item &item)
{
	BPXRect rect;
	rect.xbegin = item.x;
	rect.ybegin = item.y;
	rect.xend = item.x + item.u_res;
	rect.yend = item.y + item.v_res;
	return rect;
}

// TODO(nicholasbishop): lots of terrible code here, clean this up...

struct BPXPtexMesh {
	std::vector<int> face_vert_counts;
	std::vector<int> face_vert_indices;
	std::vector<int> face_to_region_map;
	BPXMeshEdges edges;
	int num_faces;
};

static const BPXMeshEdge *bpx_mesh_edge_find(const BPXPtexMesh &mesh, int v1,
											 int v2)
{
	if (v1 > v2) {
		std::swap(v1, v2);
	}
	if ((v1 != v2) && (v1 < mesh.edges.size())) {
		const std::vector<BPXMeshEdge> &vec = mesh.edges[v1];
		const int end = vec.size();
		for (int i = 0; i < end; i++) {
			if (vec[i].vert == v2) {
				return &vec[i];
			}
		}
	}
	return NULL;
}

static int bpx_mesh_other_face(const BPXMeshEdge &edge, const int f1)
{
	if (edge.faces[0] == f1) {
		return edge.faces[1];
	}
	else {
		return edge.faces[0];
	}
}

static int bpx_mesh_face_find_edge(const BPXPtexMesh &mesh,
								   const int face_index,
								   const BPXMeshEdge &e1)
{
	const int nsides1 = mesh.face_vert_counts.at(face_index);
	const int region1 = mesh.face_to_region_map.at(face_index);

	const int num_fv = mesh.face_vert_counts[face_index];
	for (int fv = 0; fv < num_fv; fv++) {
		const int v1 = mesh.face_vert_indices[region1 + fv];
		const int v2 = mesh.face_vert_indices[region1 + (fv + 1) % nsides1];
		const BPXMeshEdge *e2 = bpx_mesh_edge_find(mesh, v1, v2);
		if (&e1 == e2) {
			return fv;
		}
	}
	return BPX_ADJ_NONE;
}

// TODO(nicholasbishop): deduplicate with bke_ptex.c
static bool bpx_ptex_adj_layout_item(int &adj_layout_item, BPXEdge &adj_edge,
									 const BPXPtexMesh &mesh,
									 const int f1, const int fv1,
									 const BPXSide &side1)
{
	const int nsides1 = mesh.face_vert_counts.at(f1);
	const int region1 = mesh.face_to_region_map.at(f1);

	const int v1 = mesh.face_vert_indices[region1 + fv1];
	const int vn = mesh.face_vert_indices[region1 + (fv1 + 1) % nsides1];
	const int vp = mesh.face_vert_indices[region1 + (nsides1 + fv1 - 1) % nsides1];

	// TODO
	//if (side1 == BPX_SIDE_TOP || side1 == BPX_SIDE_RIGHT) {
	if (0) {
		// Reuse self
		adj_layout_item = region1 + fv1;
		adj_edge.side = side1;
		return true;
	}

	// TODO?
	adj_edge.reverse = true;

	if (side1 == BPX_SIDE_BOTTOM) {
		// Previous loop
		adj_layout_item = region1 + ((nsides1 + fv1 - 1) % nsides1);
		adj_edge.side = BPX_SIDE_LEFT;
	}
	else if (side1 == BPX_SIDE_LEFT) {
		// Next loop
		adj_layout_item = region1 + ((fv1 + 1) % nsides1);
		adj_edge.side = BPX_SIDE_BOTTOM;
	}
	else {
		const int v2 = (side1 == BPX_SIDE_TOP) ? vn : vp;

		// Map from face to edge
		const BPXMeshEdge *edge = bpx_mesh_edge_find(mesh, v1, v2);
		if (!edge) {
			return false;
		}

		// Map to other face
		const int f2 = bpx_mesh_other_face(*edge, f1);
		if (f2 == BPX_ADJ_NONE) {
			// Reuse self
			adj_layout_item = region1 + fv1;
			adj_edge.side = side1;
			return true;
		}

		const int nsides2 = mesh.face_vert_counts.at(f2);
		const int region2 = mesh.face_to_region_map.at(f2);

		// Find same edge in other face
		const int fv2 = bpx_mesh_face_find_edge(mesh, f2, *edge);
		if (fv2 == BPX_ADJ_NONE) {
			// Reuse self
			adj_layout_item = region1 + fv1;
			adj_edge.side = side1;
			return true;
		}

		if (side1 == BPX_SIDE_TOP) {
			adj_layout_item = region2 + ((fv2 + 1) % nsides2);
			adj_edge.side = BPX_SIDE_RIGHT;
		}
		else if (side1 == BPX_SIDE_RIGHT) {
			adj_layout_item = region2 + fv2;
			adj_edge.side = BPX_SIDE_TOP;
		}
		else {
			return false;
		}
	}

	return true;
}

static bool bpx_ptex_mesh_init(BPXPtexMesh &mesh, ImageInput &src)
{
	if (!bpx_ptex_face_vert_counts(mesh.face_vert_counts, src)) {
		return false;
	}

	mesh.num_faces = mesh.face_vert_counts.size();

	if (!bpx_ptex_face_vert_indices(mesh.face_vert_indices, src)) {
		return false;
	}

	if (!bpx_ptex_file_mesh_edges(mesh.edges, mesh.face_vert_counts,
								  mesh.face_vert_indices))
	{
		return false;
	}

	mesh.face_to_region_map.reserve(mesh.num_faces);
	int cur_layout_item = 0;
	for (int face_index = 0; face_index < mesh.num_faces; face_index++) {
		mesh.face_to_region_map.push_back(cur_layout_item);
		cur_layout_item += mesh.face_vert_counts[face_index];
	}

	return true;
}

static bool bpx_ptex_filter_borders_update_from_file(ImageBuf &dst,
													 ImageInput &src,
													 PtexPackedLayout &layout)
{
	BPXPtexMesh mesh;
	if (!bpx_ptex_mesh_init(mesh, src)) {
		return false;
	}

	const PtexPackedLayout::Items &items = layout.get_items();

	int cur_layout_item = 0;
	for (int face_index = 0; face_index < mesh.num_faces; face_index++) {
		for (int fv = 0; fv < mesh.face_vert_counts[face_index]; fv++) {
			if (cur_layout_item >= items.size()) {
				return false;
			}

			const PtexPackedLayout::Item &item = items[cur_layout_item];
			const BPXRect dst_rect = bpx_rect_from_layout_item(item);

			// TODO
			BPXRect adj_rect[4];
			BPXEdge adj_edge[4];

			for (int side = 0; side < BPX_NUM_SIDES; side++) {
				const BPXSide bpx_side = static_cast<BPXSide>(side);
				int adj_layout_item = BPX_ADJ_NONE;

				bpx_ptex_adj_layout_item(adj_layout_item, adj_edge[side],
										 mesh, face_index, fv, bpx_side);

				if (adj_layout_item == BPX_ADJ_NONE ||
					adj_layout_item >= items.size())
				{
					return false;
				}
				const PtexPackedLayout::Item &adj_item = items[adj_layout_item];
				adj_rect[side] = bpx_rect_from_layout_item(adj_item);
			}

			if (!BPX_rect_borders_update(bpx_image_buf_from_oiio_image_buf(&dst),
										 &dst_rect, adj_rect, adj_edge))
			{
				return false;
			}

			cur_layout_item++;
		}
	}

	return true;
}

static bool bpx_ptex_face_vector(BPXPtexFaceSpecVec &vec, ImageInput &src)
{
	std::vector<int> face_vert_counts;
	if (!bpx_ptex_face_vert_counts(face_vert_counts, src)) {
		return false;
	}

	const int num_faces = face_vert_counts.size();
	const int num_subimages = bpx_ptex_num_subimages(face_vert_counts);
	vec.reserve(num_subimages);

	// Important to initialize this, first seek doesn't set spec
	ImageSpec spec = src.spec();

	for (size_t i = 0; i < num_faces; i++) {
		const int verts_in_face = face_vert_counts[i];

		if (verts_in_face == 4) {
			if (!src.seek_subimage(vec.size(), spec)) {
				return false;
			}
			vec.push_back(bpx_ptex_face_spec(spec, false));
		}
		else if (verts_in_face >= 3) {
			for (int j = 0; j < verts_in_face; j++) {
				if (!src.seek_subimage(vec.size(), spec)) {
					return false;
				}
				vec.push_back(bpx_ptex_face_spec(spec, true));
			}
		}
		else {
			// Invalid face information
			return false;
		}

		// Sanity check the reserved length
		if (vec.size() > num_subimages) {
			return false;
		}
	}

	return true;
}

int BPX_packed_layout_num_regions(const PtexPackedLayout *layout)
{
	return layout->get_items().size();
}

static bool bpx_image_buf_ptex_layout(PtexPackedLayout &layout, ImageInput &in,
									  const BPXPtexFaceSpecVec &face_specs)
{
	ImageSpec spec = in.spec();
	int subimage = 0;
	while (in.seek_subimage(subimage, spec)) {
		if (subimage >= face_specs.size()) {
			return false;
		}

		const BPXPtexFaceSpec &pfs = face_specs.at(subimage);
		if (pfs.subface) {
			layout.add_item(PtexPackedLayout::Item(pfs.w, pfs.h));
		}
		else {
			for (int i = 0; i < QUAD_NUM_SIDES; i++) {
				layout.add_item(PtexPackedLayout::Item(pfs.qw[i], pfs.qh[i]));
			}
		}

		subimage++;
	}

	if (subimage != face_specs.size()) {
		return false;
	}

	layout.finalize();

	return true;
}

static bool bpx_image_buf_fill_from_layout(ImageBuf &all_dst,
										   const PtexPackedLayout &layout,
										   const BPXPtexFaceSpecVec &face_specs,
										   ImageInput &src)
{
	ImageSpec spec = src.spec();

	int subimage = 0;
	int face_id = 0;
	while (src.seek_subimage(subimage, spec)) {
		if (subimage >= face_specs.size()) {
			return false;
		}

		const BPXPtexFaceSpec &pfs = face_specs.at(subimage);

		ImageBuf tmp(spec);
		src.read_image(src.spec().format, tmp.localpixels());

		if (pfs.subface) {
			const PtexPackedLayout::Item &item =
				layout.get_items().at(face_id);
			const int xbegin = item.x;
			const int ybegin = item.y;
			const int zbegin = 0;
			const int chbegin = 0;

			ImageBuf tmp2;
			if (!ImageBufAlgo::flipflop(tmp2, tmp)) {
				return false;
			}
			tmp2.swap(tmp);
			
			if (!ImageBufAlgo::paste(all_dst, xbegin, ybegin, zbegin, chbegin,
									 tmp)) {
				return false;
			}
			face_id++;
		}
		else {
			ImageBuf *dsts[QUAD_NUM_SIDES];
			ROI dst_roi[QUAD_NUM_SIDES];
			
			for (int i = 0; i < QUAD_NUM_SIDES; i++) {
				const PtexPackedLayout::Item &item =
					layout.get_items().at(face_id);
				ImageSpec spec2 = spec;
				spec2.width = pfs.qw[i];
				spec2.height = pfs.qh[i];
				dsts[i] = new ImageBuf(all_dst.spec(), all_dst.localpixels());
				dst_roi[i].xbegin = item.x;
				dst_roi[i].ybegin = item.y;
				
				face_id++;
			}
			const bool r = bpx_image_buf_quad_split(dsts, &tmp, dst_roi);
			for (int i = 0; i < QUAD_NUM_SIDES; i++) {
				delete dsts[i];
			}
			if (!r) {
				return false;
			}
		}

		subimage++;
	}

	return true;
}

BPXImageBuf *BPX_image_buf_ptex_pack(BPXImageInput *bpx_src,
									 BPXImageBufFromLayout dst_create_func,
									 void *dst_create_context)
{
	if (!bpx_src || !dst_create_func) {
		return NULL;
	}
	ImageInput &in = *bpx_image_input_to_oiio_image_input(bpx_src);

	BPXPtexFaceSpecVec face_specs;
	if (!bpx_ptex_face_vector(face_specs, in)) {
		return NULL;
	}

	PtexPackedLayout layout(face_specs.size());
	if (!bpx_image_buf_ptex_layout(layout, in, face_specs)) {
		return NULL;
	}

	BPXImageBuf *bpx_dst = dst_create_func(&layout, dst_create_context);
	if (!bpx_dst) {
		return NULL;
	}
	ImageBuf &dst = *bpx_image_buf_to_oiio_image_buf(bpx_dst);

	if (!bpx_image_buf_fill_from_layout(dst, layout, face_specs, in)) {
		return NULL;
	}

	if (!bpx_ptex_filter_borders_update_from_file(dst, in, layout)) {
		return NULL;
	}
	
	return bpx_dst;
}

PtexPackedLayout *ptex_packed_layout_new(const int count)
{
	return new PtexPackedLayout(count);
}

void ptex_packed_layout_add(PtexPackedLayout * const layout,
							const int u_res, const int v_res,
							const int id)
{
	layout->add_item(PtexPackedLayout::Item(u_res, v_res));
}

void ptex_packed_layout_finalize(PtexPackedLayout * const layout)
{
	layout->finalize();
}

int ptex_packed_layout_width(const PtexPackedLayout * const layout)
{
	return layout->get_width();
}

int ptex_packed_layout_height(const PtexPackedLayout * const layout)
{
	return layout->get_height();
}

bool ptex_packed_layout_item(const PtexPackedLayout * const layout,
							 const int item_id, int *x, int *y,
							 int *width, int *height)
{
	if (layout && x && y && width && height) {
		const PtexPackedLayout::Items &items = layout->get_items();
		if (item_id >= 0 && item_id < items.size()) {
			(*x) = items[item_id].x;
			(*y) = items[item_id].y;
			(*width) = items[item_id].u_res;
			(*height) = items[item_id].v_res;
			return true;
		}
	}
	return false;
}

void ptex_packed_layout_delete(PtexPackedLayout *layout)
{
	delete layout;
}
