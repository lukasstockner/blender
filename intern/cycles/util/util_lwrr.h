#ifndef __UTIL_LWRR_H__
#define __UTIL_LWRR_H__

#include "buffers.h"

CCL_NAMESPACE_BEGIN

void LWRR_apply(RenderTile &tile);

class SampleMap {
public:
	SampleMap(RenderTile &tile);
	~SampleMap();
	void sample(int sample, int2 &p);

	int w, h;
	float *marginal, *conditional;
};

CCL_NAMESPACE_END

#endif
