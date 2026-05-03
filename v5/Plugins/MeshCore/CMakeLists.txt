SET(THISPLUG MeshCore)

pkg_search_module(LIBMOSQ OPTIONAL libmosquitto)
IF(DEFINED LIBMOSQ_FOUND)
        message(STATUS "Found libmosquitto package, MeshCore plugin enabled...")
        include_directories(${LIBMOSQ_INCLUDE_DIRS})
        link_directories(${LIBMOSQ_LIBRARY_DIRS})

	AUX_SOURCE_DIRECTORY(. SRCFILES)
	add_library (${THISPLUG} MODULE ${SRCFILES})
	SET(PLUGLIBS ${LIBMOSQ_LIBRARIES})
	include(../plugin_post_common.txt)

ELSE(DEFINED LIBMOSQ_FOUND)
        message(STATUS "Could not find libmosquitto, skipping MeshCore plugin...")
ENDIF(DEFINED LIBMOSQ_FOUND)
