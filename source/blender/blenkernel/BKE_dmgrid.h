#ifndef BKE_DMGRID_H
#define BKE_DMGRID_H

struct CustomData;

/* Each grid element can contain zero or more layers of coordinates,
   paint masks, and normals; these numbers are stored in the GridKey

   The name arrays are the unique names of the source customdata layer
*/
typedef struct GridKey {
	int co;
	int color;
	int mask;
	int no;

	/* key to identify the source layer */
	char (*color_names)[32];
	char (*mask_names)[32];
} GridKey;

#define GRIDELEM_KEY_INIT(_key, _totco, _totcolor, _totmask, _totno) \
	((_key)->co = _totco, (_key)->color = _totcolor,	     \
	 (_key)->mask = _totmask, (_key)->no = _totno,		     \
	 (_key)->color_names = NULL, (_key)->mask_names = NULL)

#define GRIDELEM_SIZE(_key) ((3*(_key)->co + 4*(_key)->color + (_key)->mask + 3*(_key)->no) * sizeof(float))
#define GRIDELEM_INTERP_COUNT(_key) (3*(_key)->co + 4*(_key)->color + (_key)->mask)

#define GRIDELEM_COLOR_OFFSET(_key) (3*(_key)->co*sizeof(float))
#define GRIDELEM_MASK_OFFSET(_key) (GRIDELEM_COLOR_OFFSET(_key) + 4*(_key)->color*sizeof(float))
#define GRIDELEM_NO_OFFSET(_key) (GRIDELEM_MASK_OFFSET(_key) + (_key)->mask*sizeof(float))

#define GRIDELEM_AT(_grid, _elem, _key) ((struct DMGridData*)(((char*)(_grid)) + (_elem) * GRIDELEM_SIZE(_key)))
#define GRIDELEM_INC(_grid, _inc, _key) ((_grid) = GRIDELEM_AT(_grid, _inc, _key))

 /* I can't figure out how to cast this type without a typedef,
    having the array length is useful to directly index layers */
typedef float (*gridelem_f4)[4];
#define GRIDELEM_CO(_grid, _key) ((float*)(_grid))
#define GRIDELEM_COLOR(_grid, _key) ((gridelem_f4)((char*)(_grid) + GRIDELEM_COLOR_OFFSET(_key)))
#define GRIDELEM_MASK(_grid, _key) ((float*)((char*)(_grid) + GRIDELEM_MASK_OFFSET(_key)))
#define GRIDELEM_NO(_grid, _key) ((float*)((char*)(_grid) + GRIDELEM_NO_OFFSET(_key)))

#define GRIDELEM_CO_AT(_grid, _elem, _key) GRIDELEM_CO(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_COLOR_AT(_grid, _elem, _key) GRIDELEM_COLOR(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_MASK_AT(_grid, _elem, _key) GRIDELEM_MASK(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_NO_AT(_grid, _elem, _key) GRIDELEM_NO(GRIDELEM_AT(_grid, _elem, _key), _key)

/* returns the active gridelem layer offset for either colors
   or masks, -1 if not found */
int gridelem_active_offset(struct CustomData *data, GridKey *gridkey, int type);

typedef struct GridToFace {
	int face;
	char offset;
} GridToFace;

#endif
