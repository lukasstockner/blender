remove_extra_strict_flags()

set(INC
	.
	../util
)

set(INC_SYS

)

set(SRC
	kernels/cpu/filter.cpp
	kernels/cpu/filter_sse2.cpp
	kernels/cpu/filter_sse3.cpp
	kernels/cpu/filter_sse41.cpp
	kernels/cpu/filter_avx.cpp
	kernels/cpu/filter_avx2.cpp
	kernels/cuda/filter.cu
)

set(SRC_HEADERS
	filter.h
	filter_compat_cpu.h
	filter_compat_cuda.h
	filter_compat_opencl.h
	filter_defines.h
	filter_features.h
	filter_features_sse.h
	filter_kernel.h
	filter_nlm_cpu.h
	filter_nlm_gpu.h
	filter_prefilter.h
	filter_reconstruction.h
	filter_transform.h
	filter_transform_gpu.h
	filter_transform_sse.h
)

set(SRC_KERNELS_CPU_HEADERS
	kernels/cpu/filter_cpu.h
	kernels/cpu/filter_cpu_impl.h
)

set(SRC_KERNELS_CUDA_HEADERS
	kernels/cuda/kernel_config.h
)

set(SRC_UTIL_HEADERS
	../util/util_atomic.h
	../util/util_color.h
	../util/util_half.h
	../util/util_hash.h
	../util/util_math.h
	../util/util_math_fast.h
	../util/util_math_matrix.h
	../util/util_static_assert.h
	../util/util_transform.h
	../util/util_texture.h
	../util/util_types.h
)

# CUDA module

if(WITH_CYCLES_CUDA_BINARIES)
	# 32 bit or 64 bit
	if(CUDA_64_BIT_DEVICE_CODE)
		set(CUDA_BITS 64)
	else()
		set(CUDA_BITS 32)
	endif()

	# CUDA version
	execute_process(COMMAND ${CUDA_NVCC_EXECUTABLE} "--version" OUTPUT_VARIABLE NVCC_OUT)
	string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\1" CUDA_VERSION_MAJOR "${NVCC_OUT}")
	string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\2" CUDA_VERSION_MINOR "${NVCC_OUT}")
	set(CUDA_VERSION "${CUDA_VERSION_MAJOR}${CUDA_VERSION_MINOR}")

	# warn for other versions
	if(CUDA_VERSION MATCHES "80")
	else()
		message(WARNING
			"CUDA version ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR} detected, "
			"build may succeed but only CUDA 8.0 is officially supported")
	endif()

	# build for each arch
	set(cuda_sources kernels/cuda/filter.cu
		${SRC_HEADERS}
		${SRC_UTIL_HEADERS}
		${SRC_KERNELS_CUDA_HEADERS}
	)
	set(cuda_cubins)

	macro(CYCLES_CUDA_KERNEL_ADD arch)
		set(cuda_extra_flags "")
		set(cuda_cubin kernel_filter_${arch}.cubin)

		set(cuda_nvcc_command ${CUDA_NVCC_EXECUTABLE})
		set(cuda_nvcc_version ${CUDA_VERSION})

		set(cuda_version_flags "-D__KERNEL_CUDA_VERSION__=${cuda_nvcc_version}")
		set(cuda_math_flags "--use_fast_math")

		add_custom_command(
			OUTPUT ${cuda_cubin}
			COMMAND ${cuda_nvcc_command}
					-arch=${arch}
					${CUDA_NVCC_FLAGS}
					-m${CUDA_BITS}
					--cubin ${CMAKE_CURRENT_SOURCE_DIR}/kernels/cuda/filter.cu
					-o ${CMAKE_CURRENT_BINARY_DIR}/${cuda_cubin}
					--ptxas-options="-v"
					${cuda_arch_flags}
					${cuda_version_flags}
					${cuda_math_flags}
					${cuda_extra_flags}
					-I${CMAKE_CURRENT_SOURCE_DIR}/../util
					-DCCL_NAMESPACE_BEGIN=
					-DCCL_NAMESPACE_END=
					-DNVCC
			DEPENDS ${cuda_sources})

		delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${cuda_cubin}" ${CYCLES_INSTALL_PATH}/lib)
		list(APPEND cuda_cubins ${cuda_cubin})

		unset(cuda_extra_flags)
		unset(cuda_debug_flags)

		unset(cuda_nvcc_command)
		unset(cuda_nvcc_version)
	endmacro()

	foreach(arch ${CYCLES_CUDA_BINARIES_ARCH})
		# Compile regular kernel
		CYCLES_CUDA_KERNEL_ADD(${arch})
	endforeach()

	add_custom_target(cycles_filter_cuda ALL DEPENDS ${cuda_cubins})
endif()

# CPU module

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

if(CXX_HAS_SSE)
	set_source_files_properties(kernels/cpu/filter_sse2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE2_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/filter_sse3.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE3_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/filter_sse41.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE41_KERNEL_FLAGS}")
endif()

if(CXX_HAS_AVX)
	set_source_files_properties(kernels/cpu/filter_avx.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX_KERNEL_FLAGS}")
endif()

if(CXX_HAS_AVX2)
	set_source_files_properties(kernels/cpu/filter_avx2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_KERNEL_FLAGS}")
endif()

add_library(cycles_filter
	${SRC}
	${SRC_HEADERS}
	${SRC_KERNELS_CPU_HEADERS}
	${SRC_KERNELS_CUDA_HEADERS}
)

if(WITH_CYCLES_CUDA)
	add_dependencies(cycles_filter cycles_filter_cuda)
endif()

# OpenCL kernel

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/filter.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/cuda/filter.cu" ${CYCLES_INSTALL_PATH}/kernel/kernels/cuda)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel)
