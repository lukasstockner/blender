#ifndef __BPX_RECT_H__
#define __BPX_RECT_H__

#ifdef __cplusplus
extern "C"{
#endif

/* TODO(nicholasbishop): this is yet another 2D integer rect
 * structure. Would be better to reuse rcti. */

typedef struct BPXRect {
	int xbegin;
	int xend;

	int ybegin;
	int yend;
} BPXRect;

/*                   Top
 *    (xbegin, yend)______(xend, yend)
 *                 |      |
 *            Left |      | Right
 *                 |______|
 * (xbegin, ybegin)        (xend, ybegin)
 *                  Bottom
 */
typedef enum {
	BPX_SIDE_BOTTOM = 0,
	BPX_SIDE_RIGHT  = 1,
	BPX_SIDE_TOP    = 2,
	BPX_SIDE_LEFT   = 3,

	BPX_NUM_SIDES   = 4
} BPXSide;

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
