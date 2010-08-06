SET(INC ${ZLIB_INC} src .)

SET(SRC
	src/ptex/PtexCache.cpp
	src/ptex/PtexFilters.cpp
	src/ptex/PtexHalf.cpp
	src/ptex/PtexReader.cpp
	src/ptex/PtexSeparableFilter.cpp
	src/ptex/PtexSeparableKernel.cpp
	src/ptex/PtexTriangleFilter.cpp
	src/ptex/PtexTriangleKernel.cpp
	src/ptex/PtexUtils.cpp
	src/ptex/PtexWriter.cpp
	ptex_C_api.cpp
)

ADD_DEFINITIONS(-DPTEX_STATIC)

BLENDERLIB(extern_ptex "${SRC}" "${INC}")

