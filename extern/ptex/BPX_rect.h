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

/* Constant for BPXRectSideAdj.index */
enum {
	BPX_RECT_SIDE_ADJ_NONE = -1
};

/* Adjacency data for creating filter borders */
typedef struct BPXRectSideAdj {
	/* Index of adjacent rectangle, or BPX_RECT_SIDE_ADJ_NONE */
	int index;

	/* Adjacent side of the adjacent rectangle
	 *
	 * Undefined if index is BPX_RECT_SIDE_ADJ_NONE
	 *
	 * TODO(nicholasbishop): to save memory could pack this and index
	 * into a single 32-bit int */
	BPXRectSide side;
} BPXRectSideAdj;

/* TODO(nicholasbishop): this is yet another 2D integer rect
 * structure. Could be nicer to reuse rcti. */

typedef struct BPXRect {
	BPXRectSideAdj adj[4];

	/* Note: begin is inclusive, end is exclusive to match
	 * OIIO::ROI */

	int xbegin;
	int xend;

	int ybegin;
	int yend;
} BPXRect;

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
