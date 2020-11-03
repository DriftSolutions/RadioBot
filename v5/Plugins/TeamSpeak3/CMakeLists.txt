SET(THISPLUG TeamSpeak3)
#SET(PLUGLIBS protobuf)

AUX_SOURCE_DIRECTORY(. SRCFILES)
add_library (${THISPLUG} MODULE ${SRCFILES})
include(../plugin_post_common.txt)
