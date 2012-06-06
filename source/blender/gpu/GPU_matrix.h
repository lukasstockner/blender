
#ifdef __cplusplus
extern "C" {
#endif

#define GPU_MODELVIEW	1<<0
#define GPU_PROJECTION	1<<1
#define GPU_TEXTURE		1<<2

void GPU_ms_init(void);
void GPU_ms_exit(void);

void gpuMatrixLock(void);
void gpuMatrixUnlock(void);

void gpuMatrixCommit(void);

void gpuPushMatrix(void);
void gpuPopMatrix(void);

void gpuMatrixMode(int mode);

void gpuLoadMatrix(const float * m);
void gpuGetMatrix(float * m);

void gpuLoadIdentity(void);

void gpuTranslate(float x, float y, float z);
void gpuScale(float x, float y, float z);

#ifdef __cplusplus
}
#endif
