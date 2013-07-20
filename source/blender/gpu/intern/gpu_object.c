#include "GPU_object.h"
#include "gpu_object_gles.h"
#include "gpu_object_gl11.h"
#include "GPU_extensions.h"



GPU_object_func gpugameobj;



void GPU_init_object_func(void)
{
#if defined(WITH_GL_PROFILE_ES20) || defined(WITH_GL_PROFILE_CORE)
	gpugameobj.gpuVertexPointer = gpuVertexPointer_gles;
	gpugameobj.gpuNormalPointer = gpuNormalPointer_gles;
	gpugameobj.gpuColorPointer = gpuColorPointer_gles;
	gpugameobj.gpuTexCoordPointer = gpuTexCoordPointer_gles;
#if !defined(GLEW_ES_ONLY)
	gpugameobj.gpuClientActiveTexture = gpuClientActiveTexture_gles;
#endif

	gpugameobj.gpuColorSet = gpuColorSet_gles;

	gpugameobj.gpuCleanupAfterDraw = gpuCleanupAfterDraw_gles;
#else
	gpugameobj.gpuVertexPointer = gpuVertexPointer_gl11;
	gpugameobj.gpuNormalPointer = gpuNormalPointer_gl11;
	gpugameobj.gpuColorPointer = gpuColorPointer_gl11;
	gpugameobj.gpuTexCoordPointer = gpuTexCoordPointer_gl11;
#if !defined(GLEW_ES_ONLY)
	gpugameobj.gpuClientActiveTexture = gpuClientActiveTexture_gl11;
#endif

	gpugameobj.gpuCleanupAfterDraw = gpuCleanupAfterDraw_gl11;
#endif
}
