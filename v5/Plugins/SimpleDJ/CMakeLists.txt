SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Output/plugins/SimpleDJ)

add_subdirectory(sdj_dec_mp3)
add_subdirectory(sdj_dec_nsv)

add_subdirectory(sdjq_memory)
add_subdirectory(sdjq_mysql)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Output/plugins)

add_definitions(-D_SIMPLEDJ_)

SET(THISPLUG SimpleDJ)
SET(PLUGLIBS tinyxml-static)
AUX_SOURCE_DIRECTORY(. SRCFILES)
add_library (${THISPLUG} MODULE ${SRCFILES}) 
include(../plugin_post_common.txt)

