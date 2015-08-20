find_package(CUDA REQUIRED)

set(INC
	.
)

set(INC_SYS
)

set(SRC
	lwrr.cpp
	lwrr_fit.cu
	lwrr_fit.h
	lwrr.h
	lwrr_mem.h
	lwrr_setting.h
	svd.cuh
	helper_cuda.h        
)

blender_add_cuda_lib(extern_lwrr "${SRC}" "${INC}" "${INC_SYS}")
