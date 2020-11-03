SET(THISPLUG Jabber)

pkg_search_module(LOUDMOUTH OPTIONAL loudmouth-1.0 loudmouth)

IF(LOUDMOUTH_FOUND)

AUX_SOURCE_DIRECTORY(. SRCFILES)
include_directories(${LOUDMOUTH_INCLUDE_DIRS})
link_directories(${LOUDMOUTH_LIBRARY_DIRS})
add_library (${THISPLUG} MODULE ${SRCFILES}) 

SET(PLUGLIBS ${LOUDMOUTH_LIBRARIES})
INCLUDE(../plugin_post_common.txt)

ELSE(LOUDMOUTH_FOUND)
MESSAGE(STATUS "loudmouth not found, disabling Jabber plugin...")
ENDIF(LOUDMOUTH_FOUND)
