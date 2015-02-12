#include <iostream>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "BPX_packed_layout.h"
#include "BPX_ptex.h"

OIIO_NAMESPACE_USING

// TODO
static const int nthreads = 1;

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
									   const int xbegin, const int ybegin,
									   const BPXRect *src_rect)
{
	if (bpx_dst && bpx_src) {
		ImageBuf *dst = bpx_image_buf_to_oiio_image_buf(bpx_dst);
		const ImageSpec &dst_spec = dst->spec();
		const ImageBuf *src = bpx_image_buf_to_oiio_image_buf(bpx_src);
		ROI src_roi = src->roi();
		const int chbegin = 0;

		if (src_rect) {
			src_roi = bpx_rect_to_oiio_roi(*src_rect);
		}

#if 0
		const int zbegin = 0;
		return ImageBufAlgo::paste(*dst, xbegin, ybegin, zbegin, chbegin,
								   *src, src_roi);
#else
		/* TODO(nicholasbishop): for some reason this code is much
		 * faster than paste, but obviously a lot uglier too... would
		 * like to figure out why, maybe just doing something silly */
		const stride_t xstride = dst_spec.pixel_bytes();
		const stride_t ystride = dst_spec.scanline_bytes();;
		const stride_t zstride = 1;
		const int chend = std::min(dst_spec.nchannels,
								   src->spec().nchannels);
		return src->get_pixel_channels(src_roi.xbegin, src_roi.xend,
									   src_roi.ybegin, src_roi.yend,
									   src_roi.zbegin, src_roi.zend,
									   chbegin, chend,
									   dst_spec.format,
									   dst->pixeladdr(xbegin, ybegin),
									   xstride, ystride, zstride);
#endif
	}
	return false;
}

/* TODO, should probably just remove in favor of new func above */
bool BPX_image_buf_pixels_copy(BPXImageBuf *bpx_dst, const BPXImageBuf *bpx_src,
							   const int xbegin, const int ybegin)
{
	return BPX_image_buf_pixels_copy_partial(bpx_dst, bpx_src, xbegin, ybegin, NULL);
}

static ROI bpx_roi_from_side(const ROI &roi, const BPXRectSide side)
{
	switch (side) {
		case BPX_RECT_SIDE_BOTTOM:
			return ROI(roi.xbegin, roi.xend, roi.ybegin, roi.ybegin + 1);

		case BPX_RECT_SIDE_RIGHT:
			return ROI(roi.xend - 1, roi.xend, roi.ybegin, roi.yend);

		case BPX_RECT_SIDE_TOP:
			return ROI(roi.xbegin, roi.xend, roi.yend - 1, roi.yend);

		case BPX_RECT_SIDE_LEFT:
			return ROI(roi.xbegin, roi.xbegin + 1, roi.ybegin, roi.yend);

		case BPX_RECT_NUM_SIDES:
			break;
	}

	assert(!"Invalid side");
}

static ROI bpx_filter_border_roi_from_side(const ROI &dst_roi,
										   const BPXRectSide side)
{
	ROI roi = bpx_roi_from_side(dst_roi, side);
	switch (side) {
		case BPX_RECT_SIDE_BOTTOM:
			roi.ybegin--;
			roi.yend--;
			break;

		case BPX_RECT_SIDE_RIGHT:
			roi.xbegin++;
			roi.xend++;
			break;

		case BPX_RECT_SIDE_TOP:
			roi.ybegin++;
			roi.yend++;
			break;

		case BPX_RECT_SIDE_LEFT:
			roi.xbegin--;
			roi.xend--;
			break;

		case BPX_RECT_NUM_SIDES:
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
								  const ROI &src_roi, const BPXRectSide src_side)
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

bool BPX_rect_borders_update(BPXImageBuf *bpx_buf, const BPXRect *dst_rect,
							 const void *rects_v, const int rects_stride)
{
	if (!bpx_buf || !rects_v) {
		return false;
	}

	ImageBuf &buf = *bpx_image_buf_to_oiio_image_buf(bpx_buf);
	const ROI dst_roi = bpx_rect_to_oiio_roi(*dst_rect);

	const unsigned char *rects_uc = static_cast<const unsigned char*>(rects_v);

	// Sample adjacent regions to create filter edges
	for (int i = 0; i < BPX_RECT_NUM_SIDES; i++) {
		const BPXRectSide dst_side = static_cast<BPXRectSide>(i);
		const bool dst_reverse = false;
		const BPXEdge dst_edge = {dst_side, dst_reverse};

		const BPXRectSideAdj &adj = dst_rect->adj[i];
		const BPXRect *src_rect;
		BPXEdge src_edge;
		if (adj.index == BPX_RECT_SIDE_ADJ_NONE) {
			// Re-use own border, effectively clamping the filter
			src_rect = dst_rect;
			src_edge.side = dst_side;
			src_edge.reverse = false;

		}
		else {
			const int offset = adj.index * rects_stride;
			src_rect = reinterpret_cast<const BPXRect *>(rects_uc + offset);
			src_edge.side = adj.side;
			src_edge.reverse = true;
		}
			
		const ROI src_roi = bpx_rect_to_oiio_roi(*src_rect);
		bpx_create_border(buf, dst_roi, dst_edge, src_roi, src_edge);

		// Also update in the other direction. TODO(nicholasbishop):
		// names are a bit confusing now
		bpx_create_border(buf, src_roi, src_edge, dst_roi, dst_edge);
	}

	// Average adjacent borders to fill in
	// corners. TODO(nicholasbishop): need to improve this, it is
	// noticable after all
	const int dst_co[BPX_RECT_NUM_SIDES][2] = {
		{dst_roi.xbegin - 1, dst_roi.ybegin - 1},
		{dst_roi.xend      , dst_roi.ybegin - 1},
		{dst_roi.xend      , dst_roi.yend      },
		{dst_roi.xbegin - 1, dst_roi.yend      }
	};
	const int src_co[BPX_RECT_NUM_SIDES][CORNER_NUM_SOURCES][2] = {
		{{dst_co[0][0] + 1, dst_co[0][1]    },
		 {dst_co[0][0],     dst_co[0][1] + 1}},
		{{dst_co[1][0] - 1, dst_co[1][1]},
		 {dst_co[1][0],     dst_co[1][1] + 1}},
		{{dst_co[2][0] - 1, dst_co[2][1]},
		 {dst_co[2][0],     dst_co[2][1] - 1}},
		{{dst_co[3][0] + 1, dst_co[3][1]},
		 {dst_co[3][0],     dst_co[3][1] - 1}}
	};
	for (int i = 0; i < BPX_RECT_NUM_SIDES; i++) {
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

bool BPX_image_buf_resize(BPXImageBuf *bpx_dst, BPXImageBuf *bpx_src)
{
	if (!bpx_dst || !bpx_src) {
		return false;
	}

	ImageBuf &dst = *bpx_image_buf_to_oiio_image_buf(bpx_dst);
	ImageBuf &src = *bpx_image_buf_to_oiio_image_buf(bpx_src);

	return ImageBufAlgo::resize(dst, src);
}

// TODO, messy code

static const int *bpx_array_attrib(const char *key, const ImageSpec &spec,
								   int &r_len)
{
	r_len = 0;

	const ImageIOParameter *p = spec.find_attribute(key);
	if (!p || p->type().basetype != TypeDesc::INT32) {
		return NULL;
	}

	const int *src = static_cast<const int *>(p->data());
	if (src) {
		r_len = p->type().numelements();
	}

	return src;
}

// Reset vector as a copy of image metadata
static bool bpx_array_attrib_copy(std::vector<int> &vec, const char *key,
								  const ImageSpec &spec)
{
	int len = 0;
	const int *src = bpx_array_attrib(key, spec, len);
	if (!src) {
		return false;
	}

	vec = std::vector<int>(src, src + len);
	return true;
}


// TODO(nicholasbishop): some of this code for packing ptx files can
// be simplified if and when OIIO provides access to additional Ptex
// data like "isSubface" and adjacency. For now use the standard (but
// not required?) attributes as a workaround.
//
// Reference for Ptex standard metakeys: http://ptex.us/metakeys.html

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

struct BPXPtexMeshFace {
	int len;

	// Index of the face's first vertex within the face_vert_indices
	// vector. The same index is used for accessing layout items.
	int vert_index;

	// Index of the face's first Ptex face in the file. Quads count as
	// one image, all other polygons have as many subfaces as
	// vertices.
	int subimage;
};

// Outer vector index is a vertex index. The vertex stored in the
// BPXMeshEdge is the whichever one has a higher index.
//
// TODO(nicholasbishop)
typedef std::vector<std::vector<BPXMeshEdge> > BPXMeshEdges;

struct BPXPtexMesh {
	std::vector<BPXPtexMeshFace> faces;
	std::vector<int> face_vert_indices;
	BPXMeshEdges edges;
};

static bool bpx_ptex_mesh_edges_init(BPXPtexMesh &mesh)
{
	const int num_mesh_verts = bpx_ptex_num_verts(mesh.face_vert_indices);

	mesh.edges.clear();
	mesh.edges.resize(num_mesh_verts);

	const int num_faces = mesh.faces.size();
	for (int face_index = 0; face_index < num_faces; face_index++) {
		const BPXPtexMeshFace &face = mesh.faces[face_index];
		const int num_fv = face.len;
		const int *verts = &mesh.face_vert_indices[face.vert_index];

		for (int fv = 0; fv < num_fv; fv++) {
			int v1 = verts[fv];
			int v2 = verts[(fv + 1) % num_fv];
			if (v1 > v2) {
				std::swap(v1, v2);
			}
			else if (v1 == v2) {
				return false;
			}

			std::vector<BPXMeshEdge> &vec = mesh.edges.at(v1);
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
				mesh.edges.at(v1).push_back(e);
			}
		}
	}

	return true;
}

// TODO(nicholasbishop): still some stupid code, clean this up...

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
	const int region1 = mesh.faces.at(face_index).vert_index;

	const int num_fv = mesh.faces[face_index].len;
	for (int fv = 0; fv < num_fv; fv++) {
		const int v1 = mesh.face_vert_indices[region1 + fv];
		const int v2 = mesh.face_vert_indices[region1 + (fv + 1) % num_fv];
		const BPXMeshEdge *e2 = bpx_mesh_edge_find(mesh, v1, v2);
		if (&e1 == e2) {
			return fv;
		}
	}
	return BPX_ADJ_NONE;
}

// TODO(nicholasbishop): deduplicate with bke_ptex.c
static bool bpx_ptex_adj_rect(const BPXPtexMesh &mesh,
							  const int f1, const int fv1,
							  const BPXRectSide &side1,
							  BPXRectSideAdj &r_adj)
{
	const int nsides1 = mesh.faces.at(f1).len;
	const int region1 = mesh.faces.at(f1).vert_index;

	const int v1 = mesh.face_vert_indices[region1 + fv1];
	const int vn = mesh.face_vert_indices[region1 + (fv1 + 1) % nsides1];
	const int vp = mesh.face_vert_indices[region1 + (nsides1 + fv1 - 1) % nsides1];

	if (side1 == BPX_RECT_SIDE_BOTTOM) {
		// Previous loop
		r_adj.index = region1 + ((nsides1 + fv1 - 1) % nsides1);
		r_adj.side = BPX_RECT_SIDE_LEFT;
	}
	else if (side1 == BPX_RECT_SIDE_LEFT) {
		// Next loop
		r_adj.index = region1 + ((fv1 + 1) % nsides1);
		r_adj.side = BPX_RECT_SIDE_BOTTOM;
	}
	else {
		const int v2 = (side1 == BPX_RECT_SIDE_TOP) ? vn : vp;

		// Map from face to edge
		const BPXMeshEdge *edge = bpx_mesh_edge_find(mesh, v1, v2);
		if (!edge) {
			return false;
		}

		// Map to other face
		const int f2 = bpx_mesh_other_face(*edge, f1);
		if (f2 == BPX_ADJ_NONE) {
			r_adj.index = BPX_RECT_SIDE_ADJ_NONE;
			return true;
		}

		const int nsides2 = mesh.faces.at(f2).len;
		const int region2 = mesh.faces.at(f2).vert_index;

		// Find same edge in other face
		const int fv2 = bpx_mesh_face_find_edge(mesh, f2, *edge);
		if (fv2 == BPX_ADJ_NONE) {
			r_adj.index = BPX_RECT_SIDE_ADJ_NONE;
			return true;
		}

		if (side1 == BPX_RECT_SIDE_TOP) {
			r_adj.index = region2 + ((fv2 + 1) % nsides2);
			r_adj.side = BPX_RECT_SIDE_RIGHT;
		}
		else if (side1 == BPX_RECT_SIDE_RIGHT) {
			r_adj.index = region2 + fv2;
			r_adj.side = BPX_RECT_SIDE_TOP;
		}
		else {
			return false;
		}
	}

	return true;
}

static bool bpx_ptex_mesh_init(BPXPtexMesh &mesh, ImageInput &src)
{
	int num_faces = 0;
	const int *face_vert_counts = bpx_array_attrib("PtexFaceVertCounts",
												   src.spec(), num_faces);
	if (!face_vert_counts) {
		return false;
	}

	if (!bpx_ptex_face_vert_indices(mesh.face_vert_indices, src)) {
		return false;
	}

	mesh.faces.resize(num_faces);
	int cur_vert = 0;
	int cur_subimage = 0;
	for (int i = 0; i < num_faces; i++) {
		const int face_len = face_vert_counts[i];

		mesh.faces[i].len = face_len;
		mesh.faces[i].vert_index = cur_vert;
		mesh.faces[i].subimage = cur_subimage;
		cur_vert += face_len;
		cur_subimage += (face_len == 4) ? 1 : face_len;
	}

	if (!bpx_ptex_mesh_edges_init(mesh)) {
		return false;
	}

	return true;
}

int BPX_packed_layout_num_regions(const BPXPackedLayout *layout)
{
	return layout->get_items().size();
}

static bool bpx_ptex_filter_borders_update(ImageBuf &buf,
										   const BPXPackedLayout &layout)
{
	BPXImageBuf *bpx_buf = bpx_image_buf_from_oiio_image_buf(&buf);

	const BPXPackedLayout::Items &items = layout.get_items();
	const void *rects = items.data();
	const int rects_stride = sizeof(BPXPackedLayout::Item);

	const int num_rects = items.size();
	for (int i = 0; i < num_rects; i++) {
		if (!BPX_rect_borders_update(bpx_buf, &items[i].rect,
									 rects, rects_stride)) {
			return false;
		}
	}
	return true;
}

static bool bpx_image_buf_ptex_layout(BPXPackedLayout &layout, ImageInput &in,
									  const BPXPtexMesh &mesh)
{
	ImageSpec spec = in.spec();

	const int num_faces = mesh.faces.size();
	for (int face_index = 0; face_index < num_faces; face_index++) {
		const BPXPtexMeshFace &face = mesh.faces[face_index];

		const bool is_quad = (face.len == 4);
		int subimage = face.subimage;

		for (int fv = 0; fv < face.len; fv++) {
			if (!in.seek_subimage(subimage, spec)) {
				// Mesh data does not match up with texture data
				return false;
			}

			// Width and height should already be powers of two
			BPXRect rect;
			rect.xbegin = 0;
			rect.ybegin = 0;
			rect.xend = spec.width;
			rect.yend = spec.height;

			// Halve/rotate subquads
			if (is_quad) {
				const int hw = std::max(1, spec.width  / 2);
				const int hh = std::max(1, spec.height / 2);

				if (fv % 2 == 0) {
					rect.xend = hw;
					rect.yend = hh;
				}
				else {
					rect.xend = hh;
					rect.yend = hw;
				}
			}
			else {
				subimage++;
			}

			// Add adjacency data
			for (int rect_side = 0; rect_side < BPX_RECT_NUM_SIDES; rect_side++) {
				const BPXRectSide bpx_rect_side = static_cast<BPXRectSide>(rect_side);

				bpx_ptex_adj_rect(mesh, face_index, fv, bpx_rect_side,
								  rect.adj[rect_side]);
			}

			layout.add_rect(rect);
		}
	}

	layout.finalize();

	return true;
}

// TODO(nicholasbishop): dedup with above
static bool bpx_image_buf_fill_from_layout(ImageBuf &all_dst,
										   const BPXPackedLayout &layout,
										   const BPXPtexMesh &mesh,
										   ImageInput &in)
{
	ImageSpec spec = in.spec();

	const int num_faces = mesh.faces.size();
	for (int face_index = 0; face_index < num_faces; face_index++) {
		const BPXPtexMeshFace &face = mesh.faces[face_index];

		// TODO

		if (face.len == 4) {
			if (!in.seek_subimage(face.subimage, spec)) {
				// Mesh data does not match up with texture data
				return false;
			}

			ImageBuf tmp(spec);
			in.read_image(spec.format, tmp.localpixels());

			ImageBuf *dsts[BPX_RECT_NUM_SIDES];
			ROI dst_roi[BPX_RECT_NUM_SIDES];
			
			for (int i = 0; i < BPX_RECT_NUM_SIDES; i++) {
 				const BPXPackedLayout::Item &item =
					layout.get_items().at(face.vert_index + i);
				ImageSpec spec2 = spec;
				spec2.width = item.width();
				spec2.height = item.height();
				dsts[i] = new ImageBuf(all_dst.spec(), all_dst.localpixels());
				dst_roi[i].xbegin = item.rect.xbegin;
				dst_roi[i].ybegin = item.rect.ybegin;
			}
			const bool r = bpx_image_buf_quad_split(dsts, &tmp, dst_roi);
			for (int i = 0; i < BPX_RECT_NUM_SIDES; i++) {
				delete dsts[i];
			}
			if (!r) {
				return false;
			}
		}
		else {
			for (int fv = 0; fv < face.len; fv++) {
				if (!in.seek_subimage(face.subimage + fv, spec)) {
					// Mesh data does not match up with texture data
					return false;
				}

				const BPXPackedLayout::Item &item =
					layout.get_items().at(face.vert_index + fv);

				ImageBuf tmp(spec);
				in.read_image(spec.format, tmp.localpixels());

				const int xbegin = item.rect.xbegin;
				const int ybegin = item.rect.ybegin;
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
			}			
		}
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

	BPXPtexMesh mesh;
	if (!bpx_ptex_mesh_init(mesh, in)) {
		return NULL;
	}

	BPXPackedLayout layout(mesh.face_vert_indices.size());
	if (!bpx_image_buf_ptex_layout(layout, in, mesh)) {
		return NULL;
	}

	BPXImageBuf *bpx_dst = dst_create_func(&layout, dst_create_context);
	if (!bpx_dst) {
		return NULL;
	}
	ImageBuf &dst = *bpx_image_buf_to_oiio_image_buf(bpx_dst);

	if (!bpx_image_buf_fill_from_layout(dst, layout, mesh, in)) {
		return NULL;
	}

	if (!bpx_ptex_filter_borders_update(dst, layout)) {
		return NULL;
	}
	
	return bpx_dst;
}

BPXPackedLayout *BPX_packed_layout_new(const int count)
{
	return new BPXPackedLayout(count);
}

void BPX_packed_layout_add(BPXPackedLayout * const layout,
						   const BPXRect *rect)
{
	layout->add_rect(*rect);
}

void BPX_packed_layout_finalize(BPXPackedLayout * const layout)
{
	layout->finalize();
}

int BPX_packed_layout_width(const BPXPackedLayout * const layout)
{
	return layout->get_width();
}

int BPX_packed_layout_height(const BPXPackedLayout * const layout)
{
	return layout->get_height();
}

bool BPX_packed_layout_item(const BPXPackedLayout * const layout,
							const int item_id, BPXRect *r_rect)
{
	if (layout && r_rect) {
		const BPXPackedLayout::Items &items = layout->get_items();
		if (item_id >= 0 && item_id < items.size()) {
			(*r_rect) = items[item_id].rect;
			return true;
		}
	}
	return false;
}

void BPX_packed_layout_delete(BPXPackedLayout *layout)
{
	delete layout;
}

bool bpx_rect_contains_point(const BPXRect *rect, const int x,
							 const int y)
{
	return (x >= rect->xbegin && x < rect->xend &&
			y >= rect->ybegin && y < rect->yend);
}
