
SET(THISPLUG adj_decenc_ffmpeg)

pkg_search_module(AVC REQUIRED libavcodec)
pkg_search_module(AVF REQUIRED libavformat)
pkg_search_module(AVU REQUIRED libavutil)
pkg_search_module(AVSWR OPTIONAL libswresample swresample)

AUX_SOURCE_DIRECTORY(. SRCFILES)
include_directories(${AVU_INCLUDE_DIRS} ${AVC_INCLUDE_DIRS} ${AVF_INCLUDE_DIRS} ${AVSWR_INCLUDE_DIRS} /usr/include/libavformat /usr/include/libavcodec /usr/include/ffmpeg /usr/include/swresample)
link_directories(${AVU_LIBRARY_DIRS} ${AVC_LIBRARY_DIRS} ${AVF_LIBRARY_DIRS} ${AVSWR_LIBRARY_DIRS})
SET(PLUGLIBS ${AVU_LIBRARIES} ${AVC_LIBRARIES} ${AVF_LIBRARIES} ${AVSWR_LIBRARIES})
add_library (${THISPLUG} MODULE ${SRCFILES})
INCLUDE(../plugin_post_common.txt)
