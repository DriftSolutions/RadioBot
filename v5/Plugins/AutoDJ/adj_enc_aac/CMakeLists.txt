
SET(THISPLUG adj_enc_aac)

AUX_SOURCE_DIRECTORY(. SRCFILES)
add_library (${THISPLUG} MODULE ${SRCFILES})
SET(PLUGLIBS faac)
INCLUDE(../plugin_post_common.txt)
