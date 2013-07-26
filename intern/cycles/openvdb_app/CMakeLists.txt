
set(INC
	.
)
set(INC_SYS
)

set(LIBRARIES
	${BOOST_LIBRARIES}
	${OPENGL_LIBRARIES}
	${CYCLES_GLEW_LIBRARY}
	${OPENIMAGEIO_LIBRARIES}
	${OPENEXR_LIBRARIES}
	${ZLIB_LIBRARIES}
)

link_directories(${OPENIMAGEIO_LIBPATH} ${BOOST_LIBPATH} ${OPENEXR_LIBPATH} ${ZLIB_LIBPATH} ${TBB_LIBPATH})

if(WITH_CYCLES_TEST)
	list(APPEND LIBRARIES ${GLUT_LIBRARIES})
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

if (WITH_CYCLES_OPENVDB)
	set(SRC
		cycles_openvdb_test.cpp
	)
	add_executable(cycles_openvdb_test ${SRC})

	list(
		APPEND LIBRARIES 
		${TBB_LIBRARIES}
		extern_openvdb
		)

	target_link_libraries(cycles_openvdb_test ${LIBRARIES})
	unset(SRC)
endif()

