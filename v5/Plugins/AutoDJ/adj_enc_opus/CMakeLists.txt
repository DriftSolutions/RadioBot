
SET(THISPLUG adj_enc_opus)

pkg_search_module(LIBOGG REQUIRED ogg)
pkg_search_module(LIBOPUS REQUIRED opus)

AUX_SOURCE_DIRECTORY(. SRCFILES) 
include_directories(${LIBOGG_INCLUDE_DIRS}) 
link_directories(${LIBOGG_LIBRARY_DIRS}) 
include_directories(${LIBOPUS_INCLUDE_DIRS}) 
link_directories(${LIBOPUS_LIBRARY_DIRS})
add_library (${THISPLUG} MODULE ${SRCFILES})
SET(PLUGLIBS ${LIBOGG_LIBRARIES} ${LIBOPUS_LIBRARIES})
INCLUDE(../plugin_post_common.txt)
