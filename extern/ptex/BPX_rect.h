#ifndef __BPX_RECT_H__
#define __BPX_RECT_H__

#ifdef __cplusplus
extern "C"{
#endif

/*                   Top
 *    (xbegin, yend)______(xend, yend)
 *                 |      |
 *            Left |      | Right
 *                 |______|
 * (xbegin, ybegin)        (xend, ybegin)
 *                  Bottom
 */
typedef enum {
	BPX_RECT_SIDE_BOTTOM = 0,
	BPX_RECT_SIDE_RIGHT  = 1,
	BPX_RECT_SIDE_TOP    = 2,
	BPX_RECT_SIDE_LEFT   = 3,

	BPX_RECT_NUM_SIDES   = 4
} BPXRectSide;

/* TODO(nicholasbishop): this is yet another 2D integer rect
 * structure. Could be nicer to reuse rcti. */

/* Note: begin is inclusive, end is exclusive to match OIIO::ROI */
typedef struct BPXRect {
	int xbegin;
	int xend;

	int ybegin;
	int yend;
} BPXRect;

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
