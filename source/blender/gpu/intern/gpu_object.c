#include "GPU_object.h"
#ifdef GLES
#include "gpu_object_gles.h"
#else
#include "gpu_object_gl11.h"
#endif

GPU_object_func gpugameobj = {0}; 
 

void GPU_init_object_func(void)
{
#ifdef GLES

gpugameobj.gpuVertexPointer = gpuVertexPointer_gles;
gpugameobj.gpuNormalPointer = gpuNormalPointer_gles;
gpugameobj.gpuColorPointer = gpuColorPointer_gles;


#else

gpugameobj.gpuVertexPointer = gpuVertexPointer_gl11;
gpugameobj.gpuNormalPointer = gpuNormalPointer_gl11;
gpugameobj.gpuColorPointer = gpuColorPointer_gl11;

#endif




}
