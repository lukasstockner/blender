#ifndef __UTIL_LWRR_H__
#define __UTIL_LWRR_H__

#include "buffers.h"

CCL_NAMESPACE_BEGIN

static inline bool write_pfm(const char *name, float *data, int w, int h) {
	FILE *f = fopen(name, "wb");
	if(!f)
		return false;
	fprintf(f, "Pf\n%d %d\n-1.0\n", w, h);
	fwrite(data, sizeof(float), w*h, f);
	fclose(f);
	return true;
}

static inline bool write_pfm(const char *name, float *data, int w, int h, int stride) {
	FILE *f = fopen(name, "wb");
	if(!f)
		return false;
	fprintf(f, "Pf\n%d %d\n-1.0\n", w, h);
	for(int i = 0; i < w*h; i++)
		fwrite(data + stride*i, sizeof(float), 1, f);
	fclose(f);
	return true;
}

static inline bool write_pfm(const char *name, float3 *data, int w, int h) {
	FILE *f = fopen(name, "wb");
	if(!f)
		return false;
	fprintf(f, "PF\n%d %d\n-1.0\n", w, h);
	fwrite(data, sizeof(float3), w*h, f);
	fclose(f);
	return true;
}

void LWRR_apply(RenderBuffers *buffers);

class SampleMap {
public:
	SampleMap(RenderTile &tile, int ofs, int lwr_passes);
	~SampleMap();
	void sample(int sample, int2 &p);

	int w, h;
	int offset;
	float *marginal, *conditional;
};

CCL_NAMESPACE_END

#endif
