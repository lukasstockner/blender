#ifndef __BPX_PTEX_H__
#define __BPX_PTEX_H__

#include "BPX_rect.h"

#ifdef __cplusplus
extern "C"{
#endif

/* BPX is short for Blender Ptex */

/* TODO(nicholasbishop): more comments and organization */

typedef struct BPXImageBuf BPXImageBuf;
typedef struct BPXImageInput BPXImageInput;

typedef struct {
	BPXSide side;
	bool reverse;
} BPXEdge;
	
/* TODO(nicholasbishop): Ptex file format can also contain 16-bit
 * integers and floats, nice to add at some point */
typedef enum {
	BPX_TYPE_DESC_UINT8,
	BPX_TYPE_DESC_FLOAT
} BPXTypeDesc;

/* Allocate an empty BPXImageBuf
 *
 * Return NULL on failure.
 *
 * When no longer needed the BPXImageBuf should be deallocated with
 * BPX_image_buf_free(). */
BPXImageBuf *BPX_image_buf_alloc_empty(void);

/* Allocate a BPXImageBuf to wrap existing image data
 *
 * Return NULL on failure.
 *
 * Note that BPXImageBuf does not take ownership of the pixels
 * pointer. It also does not make a copy of the data, so the original
 * pixels must stay valid until the BPXImageBuf is freed.
 *
 * When no longer needed the BPXImageBuf should be deallocated with
 * BPX_image_buf_free(). */
BPXImageBuf *BPX_image_buf_wrap(int width, int height, int num_channels,
								BPXTypeDesc type_desc, void *pixels);

/* Deallocate a BPXImageBuf */
void BPX_image_buf_free(BPXImageBuf *buf);

/* Copy all of the source into the destination
 *
 * The copy is placed at origin (x, y) in the destination.
 *
 * Return true on success, false otherwise. */
bool BPX_image_buf_pixels_copy(BPXImageBuf *dst, const BPXImageBuf *src,
							   int x, int y);

/* Copy a rectangle from source into the destination
 *
 * The copy is placed at origin (x, y) in the destination.
 *
 * Return true on success, false otherwise. */
bool BPX_image_buf_pixels_copy_partial(BPXImageBuf *dst, const BPXImageBuf *src,
							   int x, int y, const BPXRect *src_rect);

/* Allocate a BPXImageInput with the given filepath
 *
 * Return NULL on failure.
 *
 * When no longer needed the BPXImageInput should be deallocated with
 * BPX_image_input_free(). */
BPXImageInput *BPX_image_input_from_filepath(const char filepath[]);

/* Deallocate a BPXImageInput */
void BPX_image_input_free(BPXImageInput *input);

bool BPX_image_input_type_desc(const BPXImageInput *input,
							   BPXTypeDesc *type_desc);

bool BPX_image_input_num_channels(const BPXImageInput *input,
								  int *num_channels);

bool BPX_image_input_seek_subimage(BPXImageInput *input, const int subimage,
								   int *width, int *height);

bool BPX_image_input_read(BPXImageBuf *bpx_dst, BPXImageInput *bpx_src);

bool BPX_rect_borders_update(BPXImageBuf *bpx_buf,
							 const BPXRect *dst_rect,
							 const BPXRect src_rect[BPX_NUM_SIDES],
							 const BPXEdge src_edge[BPX_NUM_SIDES]);

bool BPX_image_buf_quad_split(BPXImageBuf *dst[4], const BPXImageBuf *src);

bool TODO_test_write(BPXImageBuf *bpx_buf, const char *path);

bool BPX_image_buf_transform(BPXImageBuf *bpx_buf);

/* Resize all of src into dst, using dst's size
 *
 * Return true if successful, false otherwise. */
bool BPX_image_buf_resize(BPXImageBuf *bpx_dst, BPXImageBuf *bpx_src);

int BPX_packed_layout_num_regions(const struct BPXPackedLayout *layout);

typedef BPXImageBuf* (*BPXImageBufFromLayout)
	(const struct BPXPackedLayout *layout, void *context);

BPXImageBuf *BPX_image_buf_ptex_pack(BPXImageInput *bpx_src,
									 BPXImageBufFromLayout dst_create_func,
									 void *dst_create_context);

struct BPXPackedLayout;
struct BPXPackedLayout *BPX_packed_layout_new(int count);
void BPX_packed_layout_add(struct BPXPackedLayout *layout,
						   int u_res, int v_res, int id);
void BPX_packed_layout_finalize(struct BPXPackedLayout *layout);
int BPX_packed_layout_width(const struct BPXPackedLayout *layout);
int BPX_packed_layout_height(const struct BPXPackedLayout *layout);
bool BPX_packed_layout_item(const struct BPXPackedLayout *layout,
							const int index,
							struct BPXRect *r_rect);
void BPX_packed_layout_delete(struct BPXPackedLayout *layout);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
