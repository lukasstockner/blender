#ifndef BKE_DMGRID_H
#define BKE_DMGRID_H

/* Each grid element can contain zero or more layers of coordinates,
   paint masks, and normals; these numbers are stored in the GridKey

   For now, co and no can have only zero or one layers, only mask is
   really variable.
*/
typedef struct GridKey {
	int co;
	int color;
	int mask;
	int no;
} GridKey;

#define GRIDELEM_KEY_INIT(_gridkey, _totco, _totcolor, _totmask, _totno) \
	((_gridkey)->co = _totco, (_gridkey)->color = _totcolor, (_gridkey)->mask = _totmask, (_gridkey)->no = _totno)

#define GRIDELEM_SIZE(_key) ((3*(_key)->co + 3*(_key)->color + (_key)->mask + 3*(_key)->no) * sizeof(float))
#define GRIDELEM_INTERP_COUNT(_key) (3*(_key)->co + 3*(_key)->color + (_key)->mask)

#define GRIDELEM_COLOR_OFFSET(_key) (3*(_key)->co*sizeof(float))
#define GRIDELEM_MASK_OFFSET(_key) (GRIDELEM_COLOR_OFFSET(_key) + 3*(_key)->color*sizeof(float))
#define GRIDELEM_NO_OFFSET(_key) (GRIDELEM_MASK_OFFSET(_key) + (_key)->mask*sizeof(float))

#define GRIDELEM_AT(_grid, _elem, _key) ((struct DMGridData*)(((char*)(_grid)) + (_elem) * GRIDELEM_SIZE(_key)))
#define GRIDELEM_INC(_grid, _inc, _key) ((_grid) = GRIDELEM_AT(_grid, _inc, _key))

#define GRIDELEM_CO(_grid, _key) ((float*)(_grid))
#define GRIDELEM_COLOR(_grid, _key) ((float*)((char*)(_grid) + GRIDELEM_COLOR_OFFSET(_key)))
#define GRIDELEM_NO(_grid, _key) ((float*)((char*)(_grid) + GRIDELEM_NO_OFFSET(_key)))
#define GRIDELEM_MASK(_grid, _key) ((float*)((char*)(_grid) + GRIDELEM_MASK_OFFSET(_key)))

#define GRIDELEM_CO_AT(_grid, _elem, _key) GRIDELEM_CO(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_COLOR_AT(_grid, _elem, _key) GRIDELEM_COLOR(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_NO_AT(_grid, _elem, _key) GRIDELEM_NO(GRIDELEM_AT(_grid, _elem, _key), _key)
#define GRIDELEM_MASK_AT(_grid, _elem, _key) GRIDELEM_MASK(GRIDELEM_AT(_grid, _elem, _key), _key)

#endif
