
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
	vdb_volume.cpp
)

set(HEADER_SRC
	vdb_definitions.h
	vdb_volume.h
	vdb_lookup.h
	vdb_util.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_kernel_textures ${SRC} ${HEADER_SRC})

