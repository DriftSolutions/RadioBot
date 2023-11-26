
SET(THISPLUG adj_dec_aac)

pkg_search_module(LIBFAAD REQUIRED libfaad2 libfaad faad2 faad)
pkg_search_module(TAGLIB REQUIRED taglib)

AUX_SOURCE_DIRECTORY(. SRCFILES)
include_directories(${LIBFAAD_INCLUDE_DIRS} ${TAGLIB_INCLUDE_DIRS})
link_directories(${LIBFAAD_LINK_DIRS} ${TAGLIB_LINK_DIRS})
add_library (${THISPLUG} MODULE ${SRCFILES})
SET(PLUGLIBS ${LIBFAAD_LIBRARIES} ${TAGLIB_LIBRARIES})
include(../plugin_post_common.txt)
