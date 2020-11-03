SET(THISPLUG sdj_dec_mp3)

pkg_search_module(TAGLIB REQUIRED taglib)
SET(PLUGLIBS ${TAGLIB_LIBRARIES} ssmt)

add_definitions(-DUSE_TAGLIB)
AUX_SOURCE_DIRECTORY(. SRCFILES)
include_directories(${TAGLIB_INCLUDE_DIRS})
add_library (${THISPLUG} MODULE ${SRCFILES})
include(../plugin_post_common.txt)
