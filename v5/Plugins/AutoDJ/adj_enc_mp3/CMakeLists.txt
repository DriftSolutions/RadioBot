
SET(THISPLUG adj_enc_mp3)

#PkgConfigSearch("libmp3lame;lame")
#pkg_search_module(MAD OPTIONAL mad libmad)

AUX_SOURCE_DIRECTORY(. SRCFILES)
#include_directories(${MAD_INCLUDE_DIRS})
#link_directories(${MAD_LIBRARY_DIRS})
add_library (${THISPLUG} MODULE ${SRCFILES})
SET(PLUGLIBS mp3lame)
INCLUDE(../plugin_post_common.txt)
