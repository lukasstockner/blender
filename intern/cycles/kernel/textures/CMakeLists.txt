
set(INC
	.
	..
	../svm
	../../render
	../../util
	../../device
)

set(INC_SYS

)

set(SRC
	openvdb_volume.cpp
)

set(HEADER_SRC
	openvdb_volume.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_kernel_textures ${SRC} ${HEADER_SRC})

