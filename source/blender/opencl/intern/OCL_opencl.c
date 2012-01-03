#include "OCL_opencl.h"

void OCL_init() {
#ifdef _WIN32
	clewInit("OpenCL.dll");
#else
	clewInit("libOpenCL.so");
#endif
}
