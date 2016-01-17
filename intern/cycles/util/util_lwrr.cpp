#include "util_lwrr.h"
#include "film.h"
#include "util_foreach.h"
#include "util_hash.h"

#include "lwrr.h"
#include "lwrr_fit.h"

CCL_NAMESPACE_BEGIN

thread_mutex gpu_mutex;

void LWRR_apply(RenderBuffers *buffers) {
	thread_scoped_lock lock(gpu_mutex);
//	if((tile.sample <= 32 && (tile.sample % 16)) || (tile.sample > 32 && tile.sample <= 128 && (tile.sample % 32)) || (tile.sample > 128 && (tile.sample % 64)))
//		return;

	buffers->copy_from_device();

	int oLWR = buffers->params.get_passes_size(true);
	float *buffer = (float*)buffers->buffer.data_pointer + oLWR;
	int oC = 14, oC2 = 17, oN = 2, oN2 = 5, oM = 8, oM2 = 11, oD = 0, oD2 = 1, oS = -oLWR, oT = 20, oF = -oLWR;

	foreach(Pass& pass, buffers->params.passes) {
		if(pass.type == PASS_MIST)
			break;
		oS += pass.components;
	}

	int pass_stride = buffers->params.get_passes_size();
	int w = buffers->params.width;
	int h = buffers->params.height;

	float *bC = new float[3*w*h], *bC2 = new float[3*w*h];
	float *bN = new float[3*w*h], *bN2 = new float[3*w*h];
	float *bM = new float[3*w*h], *bM2 = new float[3*w*h];
	float *bD = new float[  w*h], *bD2 = new float[  w*h];
	int *bS   = new   int[  w*h];

	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++) {
			float *base = buffer + (y*w + x)*pass_stride;
			int i = y*w+x;
			float iS = 1.0f / base[oS];
			bC [3*i+0] = base[oC +0]; bC [3*i+1] = base[oC +1]; bC [3*i+2] = base[oC +2];
			bC2[3*i+0] = base[oC2+0]+base[oC+0]*base[oC+0]*iS; bC2[3*i+1] = base[oC2+1]+base[oC+1]*base[oC+1]*iS; bC2[3*i+2] = base[oC2+2]+base[oC+2]*base[oC+2]*iS;
			bN [3*i+0] = base[oN +0]; bN [3*i+1] = base[oN +1]; bN [3*i+2] = base[oN +2];
			bN2[3*i+0] = base[oN2+0]+base[oN+0]*base[oN+0]*iS; bN2[3*i+1] = base[oN2+1]+base[oN+1]*base[oN+1]*iS; bN2[3*i+2] = base[oN2+2]+base[oN+2]*base[oN+2]*iS;
			bM [3*i+0] = base[oM +0]; bM [3*i+1] = base[oM +1]; bM [3*i+2] = base[oM +2];
			bM2[3*i+0] = base[oM2+0]+base[oM+0]*base[oM+0]*iS; bM2[3*i+1] = base[oM2+1]+base[oM+1]*base[oM+1]*iS; bM2[3*i+2] = base[oM2+2]+base[oM+2]*base[oM+2]*iS;

			bD [i] = base[oD ];
			bD2[i] = base[oD2]+base[oD]*base[oD]*iS;
			bS [i] = base[oS ];
		}

	LWRR lwrr(w, h, w*h);
	lwrr.init_lwrr(bC, bC2, bN, bN2, bM, bM2, bD, bD2, bS, NULL, NULL, NULL);
	lwrr.run_lwrr(1000000, false);
	freeDeviceMemory();

	delete[] bC;
	delete[] bC2;
	delete[] bN;
	delete[] bN2;
	delete[] bM;
	delete[] bM2;
	delete[] bD;
	delete[] bD2;

	float *outImg = lwrr.get_optImg();
	float *outMse = lwrr.get_mse_optImg();
	float *quantileMse = new float[w*h];
	for(int i = 0; i < w*h; i++)
		quantileMse[i] = outMse[3*i];
	std::sort(quantileMse, quantileMse + w*h);
	float maxMse = quantileMse[int(w*h*0.95f)];
	float sumMse = 0.0f;
	for(int i = 0; i < w*h; i++)
		sumMse += outMse[3*i];
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++) {
			float *base = buffer + (y*w+x)*pass_stride;
			base[oF+0] = outImg[3*(y*w+x)+0];
			base[oF+1] = outImg[3*(y*w+x)+1];
			base[oF+2] = outImg[3*(y*w+x)+2];
//			base[oF+3] = base[oC+3] / base[oS];
			base[oT] = min(outMse[3*(y*w+x)], maxMse) * w*h / sumMse;
		}

	delete[] bS;

	buffers->copy_to_device();
/*
	char name[1024];
	sprintf(name, "in_%d.pfm", tile.sample);
	write_pfm3(name, lwrr.get_inputImg(), w, h);
	sprintf(name, "out_%d.pfm", tile.sample);
	write_pfm3(name, lwrr.get_optImg(), w, h);
	sprintf(name, "rank_%d.pfm", tile.sample);
	write_pfm(name, lwrr.get_ranks(), w, h);
	sprintf(name, "mse_%d.pfm", tile.sample);
	write_pfm3(name, lwrr.get_mse_optImg(), w, h);
*/
}

SampleMap::SampleMap(RenderTile &tile, int ofs, int lwr_passes)
{
	offset = ofs;

	w = tile.w;
	h = tile.h;

	marginal = new float[h];
	conditional = new float[w*h];

	int oT = lwr_passes + 20;

	float *buffer = (float*)tile.buffers->buffer.data_pointer;
	int pass_stride = tile.buffers->params.get_passes_size();
	//Prefilter
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			float sum_w = 0.0f;
			float sum = 0.0f;
			for(int py = max(0, y + tile.y - 3); py < min(tile.buffers->params.height, y + tile.y + 4); py++) {
				for(int px = max(0, x + tile.x - 3); px < min(tile.buffers->params.width, x + tile.x + 4); px++) {
					float dist = (py - (y+tile.y))*(py - (y+tile.y)) + (px - (x+tile.x))*(px - (x+tile.x));
					float w = expf(-dist/2.0f);
					sum_w += w;
					sum += w*buffer[(tile.offset + py*tile.stride + px)*pass_stride + oT];
				}
			}
			conditional[y*w+x] = sum/sum_w;
		}
	}

	for(int y = 0; y < h; y++) {
		float *row = conditional + y*w;
		for(int x = 1; x < w; x++)
			row[x] += row[x-1];
		marginal[y] = row[w-1];
		for(int x = 0; x < w; x++)
			row[x] /= row[w-1];
	}
	for(int y = 1; y < h; y++)
		marginal[y] += marginal[y-1];
	for(int y = 0; y < h; y++)
		marginal[y] /= marginal[h-1];
}

SampleMap::~SampleMap()
{
	delete[] marginal;
	delete[] conditional;
}

void SampleMap::sample(int sample, int2 &p)
{
	sample -= offset;

	float u, v;
	/* Sample 02-Sequence for pixel jittering (1D Sobol for v, Van-der-Corput for u) */
	uint r = 0, i = sample;
	for(uint va = 1U << 31; i; i >>= 1, va ^= va >> 1)
		if(i & 1)
			r ^= va;

	uint rotation = hash_int_2d(p.x, p.y);
	v = (float)r * (1.0f/(float)0xFFFFFFFF) + (rotation & 0xFFFF) * (1.0f/(float)0xFFFF);
	v -= floorf(v);

	i = (sample << 16) | (sample >> 16);
	i = ((i & 0x00ff00ff) << 8) | ((i & 0xff00ff00) >> 8);
	i = ((i & 0x0f0f0f0f) << 4) | ((i & 0xf0f0f0f0) >> 4);
	i = ((i & 0x33333333) << 2) | ((i & 0xcccccccc) >> 2);
	i = ((i & 0x55555555) << 1) | ((i & 0xaaaaaaaa) >> 1);
	u = (float)i * (1.0f/(float)0xFFFFFFFF) + (rotation >> 16) * (1.0f/(float)0xFFFF);
	u -= floorf(u);

	/* Sample 2D CDF */
	u = (p.x + u)/w;
	v = (p.y + v)/h;
	p.y = min(std::upper_bound(marginal, marginal + h, v) - marginal, h-1);
	p.x = min(std::upper_bound(conditional + p.y*w, conditional + (p.y+1)*w, u) - (conditional + p.y*w), w-1);
}

CCL_NAMESPACE_END
