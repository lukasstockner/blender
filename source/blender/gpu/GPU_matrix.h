#include <GL/glew.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GPU_MATRIX
#define GPU_MATRIX

void GPU_matrix_forced_update(void);

void GPU_ms_init(void);
void GPU_ms_exit(void);

void gpuMatrixLock(void);
void gpuMatrixUnlock(void);

void gpuMatrixCommit(void);

void gpuPushMatrix(void);
void gpuPopMatrix(void);

void gpuMatrixMode(GLenum mode);
GLenum gpuGetMatrixMode(void);

void gpuLoadMatrix(const GLfloat * m);
GLfloat * gpuGetMatrix(GLfloat * m);

void gpuLoadIdentity(void);

void gpuMultMatrix(const GLfloat *m);
void gpuMultMatrixd(const double *m);

void gpuTranslate(GLfloat x, GLfloat y, GLfloat z);
void gpuScale(GLfloat x, GLfloat y, GLfloat z);
void gpuRotateAxis(GLfloat angle, char axis);

void gpuOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLoadOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuLoadFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLookAt(GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ, GLfloat centerX, GLfloat centerY, GLfloat centerZ, GLfloat upX, GLfloat upY, GLfloat upZ);

#ifndef GPU_MAT_CAST_ANY
#define GPU_MAT_CAST_ANY


#define gpuLoadMatrix(m) gpuLoadMatrix((const GLfloat *) m);
#define gpuGetMatrix(m) gpuGetMatrix((GLfloat *) m);
#define gpuMultMatrix(m) gpuMultMatrix((const GLfloat *) m);
#define gpuMultMatrixd(m) gpuMultMatrixd((const double *) m);

#endif

#endif

#ifdef __cplusplus
}
#endif


