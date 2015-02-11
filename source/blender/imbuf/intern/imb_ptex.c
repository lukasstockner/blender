#include "BLI_utildefines.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "BLI_string.h"
#include "MEM_guardedalloc.h"

#include "BPX_ptex.h"

/* TODO(nicholasbishop): color space stuff */

int imb_is_a_ptex(unsigned char *buf)
{
	unsigned char magic[4] = "Ptex";
	return memcmp(buf, magic, sizeof(magic)) == 0;
}

int imb_is_a_ptex_filepath(const char *name)
{
	return BLI_str_endswith(name, ".ptx");
}

ImBuf *IMB_alloc_from_ptex_layout(const struct BPXPackedLayout *layout)
{
	ImBuf *ibuf = IMB_allocImBuf(BPX_packed_layout_width(layout),
								 BPX_packed_layout_height(layout),
								 /* TODO? */
								 32,
								 /* TODO */
								 IB_rect);
	if (ibuf) {
		int i;

		/* Copy layout items into ImBuf.ptex_regions */
		ibuf->num_ptex_regions = BPX_packed_layout_num_regions(layout);
		ibuf->ptex_regions = MEM_mallocN(sizeof(*ibuf->ptex_regions) *
										 ibuf->num_ptex_regions,
										 "ImBuf ptex_regions");

		for (i = 0; i < ibuf->num_ptex_regions; i++) {
			BPXRect *rect = &ibuf->ptex_regions[i];
			if (!BPX_packed_layout_item(layout, i, rect)) {
				/* Error */
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
	}

	return ibuf;
}

static BPXImageBuf *imb_alloc_from_ptex_layout_cb(const struct BPXPackedLayout *layout,
												  void *vcontext)
{
	ImBuf **ibuf = vcontext;
	(*ibuf) = IMB_alloc_from_ptex_layout(layout);
	return IMB_imbuf_as_bpx_image_buf(*ibuf);
}

BPXImageBuf *IMB_imbuf_as_bpx_image_buf(ImBuf *ibuf)
{
	/* TODO: channels and data type */
	const int num_channels = 4;
	return BPX_image_buf_wrap(ibuf->x, ibuf->y, num_channels, 
							  BPX_TYPE_DESC_UINT8, ibuf->rect);
}

struct ImBuf *imb_load_ptex_filepath(const char *path,
									 const int UNUSED(flags),
									 char colorspace[IM_MAX_SPACE])
{
	ImBuf *ibuf = NULL;

	if (imb_is_a_ptex_filepath(path)) {
		BPXImageInput *bpx_src = BPX_image_input_from_filepath(path);

		if (bpx_src) {
			BPXImageBuf *dst;
			dst = BPX_image_buf_ptex_pack(bpx_src,
										  imb_alloc_from_ptex_layout_cb,
										  &ibuf);
			if (!dst) {
				if (ibuf) {
					IMB_freeImBuf(ibuf);
				}
				return NULL;
			}
			
			BPX_image_buf_free(dst);
					
			// TODO
			IMB_rectfill_alpha(ibuf, 1);
		}
	}

	return ibuf;
}
