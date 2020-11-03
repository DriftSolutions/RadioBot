SET(THISPLUG Welcome)
SET(PLUGLIBS tinyxml-static)

pkg_search_module(MUPARSER OPTIONAL muparser muParser)
IF(MUPARSER_FOUND)
include_directories(${MUPARSER_INCLUDE_DIRS})
link_directories(${MUPARSER_LIBRARY_DIRS})
SET(PLUGLIBS ${MUPARSER_LIBRARIES} ${PLUGLIBS})
ELSE(MUPARSER_FOUND)
MESSAGE(STATUS "muParser not found...")
ENDIF(MUPARSER_FOUND)

INCLUDE(../plugin_generic.txt)
