
SET(THISPLUG adj_dec_ffmpeg)

pkg_search_module(AVC REQUIRED libavcodec)
pkg_search_module(AVF REQUIRED libavformat)
pkg_search_module(AVU REQUIRED libavutil)

AUX_SOURCE_DIRECTORY(. SRCFILES)
include_directories(${AVU_INCLUDE_DIRS} ${AVC_INCLUDE_DIRS} ${AVF_INCLUDE_DIRS} /usr/include/libavformat /usr/include/libavcodec /usr/include/ffmpeg)
link_directories(${AVU_LIBRARY_DIRS} ${AVC_LIBRARY_DIRS} ${AVF_LIBRARY_DIRS})
SET(PLUGLIBS ${AVU_LIBRARIES} ${AVC_LIBRARIES} ${AVF_LIBRARIES})
add_library (${THISPLUG} MODULE ${SRCFILES})
INCLUDE(../plugin_post_common.txt)
