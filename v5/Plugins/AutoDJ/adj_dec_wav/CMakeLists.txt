
SET(THISPLUG adj_dec_wav)

pkg_search_module(SNDFILE REQUIRED sndfile libsndfile)

AUX_SOURCE_DIRECTORY(. SRCFILES)
include_directories(${SNDFILE_INCLUDE_DIRS})
link_directories(${SNDFILE_LINK_DIRS})
add_library (${THISPLUG} MODULE ${SRCFILES})
SET(PLUGLIBS sndfile)
include(../plugin_post_common.txt)
