
SET(THISPLUG adj_decenc_ogg)

pkg_search_module(VENCFILE REQUIRED vorbisenc)
pkg_search_module(VBFILE REQUIRED vorbisfile)

AUX_SOURCE_DIRECTORY(. SRCFILES) 
include_directories(${VENCFILE_INCLUDE_DIRS}) 
link_directories(${VENCFILE_LIBRARY_DIRS}) 
include_directories(${VBFILE_INCLUDE_DIRS}) 
link_directories(${VBFILE_LIBRARY_DIRS})
add_library (${THISPLUG} MODULE ${SRCFILES})
SET(PLUGLIBS ${VENCFILE_LIBRARIES} ${VBFILE_LIBRARIES})
INCLUDE(../plugin_post_common.txt)
