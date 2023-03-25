SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Output/plugins/AutoDJ)

#add_subdirectory(adj_dec_ffmpeg)
add_subdirectory(adj_dec_flac)
add_subdirectory(adj_dec_mp3)
add_subdirectory(adj_dec_playlist)
add_subdirectory(adj_dec_wav)

add_subdirectory(adj_enc_aac)
CHECK_INCLUDE_FILES("aacplus.h" HAVE_AACPLUS_H)
IF(HAVE_AACPLUS_H)
add_subdirectory(adj_enc_aacplus)
ELSE()
message(NOTICE "libaacplus not found, skipping AAC+ encoder. See https://wiki.shoutirc.com/index.php/Plugin:AutoDJ:AAC%2B_Encoder")
ENDIF()
add_subdirectory(adj_enc_ffmpeg)
add_subdirectory(adj_enc_mp3)
add_subdirectory(adj_enc_null)
add_subdirectory(adj_enc_ogg)
add_subdirectory(adj_enc_opus)

add_subdirectory(adjq_memory)
add_subdirectory(adjq_mysql)
add_subdirectory(adjq_mysql_new)

add_subdirectory(adjr_libresample)
add_subdirectory(adjr_soxr)


SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Output/plugins)

add_definitions(-D_AUTODJ_ -DOUTSIDE_SPEEX)

SET(THISPLUG AutoDJ)
SET(PLUGLIBS tinyxml-static)
AUX_SOURCE_DIRECTORY(. SRCFILES)
AUX_SOURCE_DIRECTORY(./resample RESAMPLEFILES)
add_library (${THISPLUG} MODULE ${SRCFILES} ${RESAMPLEFILES} ${PROJECT_BINARY_DIR}/proto_linux/autodj.pb.cc)
include(../plugin_post_common.txt)

