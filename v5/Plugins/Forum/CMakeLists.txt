SET(THISPLUG Forum)

AUX_SOURCE_DIRECTORY(. SRCFILES)

add_library (${THISPLUG} MODULE ${SRCFILES}) 
include(../plugin_post_common.txt)
