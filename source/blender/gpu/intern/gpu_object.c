#include "GPU_object.h"
#include "gpu_object_gles.h"
#ifndef WITH_GLES
#include "gpu_object_gl11.h"
#endif
#include "GPU_extensions.h"

GPU_object_func gpugameobj = {0}; 
 

void GPU_init_object_func(void)
{

if(!GPU_GLTYPE_FIXED_ENABLED)
{
gpugameobj.gpuVertexPointer = gpuVertexPointer_gles;
gpugameobj.gpuNormalPointer = gpuNormalPointer_gles;
gpugameobj.gpuColorPointer = gpuColorPointer_gles;
gpugameobj.gpuTexCoordPointer = gpuTexCoordPointer_gles;
gpugameobj.gpuClientActiveTexture = gpuClientActiveTexture_gles;

gpugameobj.gpuColorSet = gpuColorSet_gles;


gpugameobj.gpuCleanupAfterDraw = gpuCleanupAfterDraw_gles;
}
#ifndef WITH_GLES
else {
gpugameobj.gpuVertexPointer = gpuVertexPointer_gl11;
gpugameobj.gpuNormalPointer = gpuNormalPointer_gl11;
gpugameobj.gpuColorPointer = gpuColorPointer_gl11;
gpugameobj.gpuTexCoordPointer = gpuTexCoordPointer_gl11;
gpugameobj.gpuClientActiveTexture = gpuClientActiveTexture_gl11;

gpugameobj.gpuCleanupAfterDraw = gpuCleanupAfterDraw_gl11;
}
#endif




}
