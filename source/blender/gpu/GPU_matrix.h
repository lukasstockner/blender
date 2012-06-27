#include "GPU_material.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef GPU_MATRIX
#define GPU_MATRIX

#define GPU_MODELVIEW	1<<0
#define GPU_PROJECTION	1<<1
#define GPU_TEXTURE		1<<2

void GPU_matrix_forced_update(void);

void GPU_ms_init(void);
void GPU_ms_exit(void);

void gpuMatrixLock(void);
void gpuMatrixUnlock(void);

void gpuMatrixCommit(void);

void gpuPushMatrix(void);
void gpuPopMatrix(void);

void gpuMatrixMode(int mode);

void gpuLoadMatrix(const float * m);
float * gpuGetMatrix(float * m);

void gpuLoadIdentity(void);

void gpuMultMatrix(const float *m);
void gpuMultMatrixd(const double *m);

void gpuTranslate(float x, float y, float z);
void gpuScale(float x, float y, float z);
void gpuRotateAxis(float angle, char axis);

void gpuOrtho(float left, float right, float bottom, float top, float nearVal, float farVal);
void gpuFrustum(float left, float right, float bottom, float top, float nearVal, float farVal);

void gpuLoadOrtho(float left, float right, float bottom, float top, float nearVal, float farVal);
void gpuLoadFrustum(float left, float right, float bottom, float top, float nearVal, float farVal);

void gpuLookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ);

#ifndef GPU_MAT_CAST_ANY
#define GPU_MAT_CAST_ANY


#define gpuLoadMatrix(m) gpuLoadMatrix((const float *) m);
#define gpuGetMatrix(m) gpuGetMatrix((float *) m);
#define gpuMultMatrix(m) gpuMultMatrix((const float *) m);
#define gpuMultMatrixd(m) gpuMultMatrixd((const double *) m);

#endif

#endif

#ifdef __cplusplus
}
#endif


