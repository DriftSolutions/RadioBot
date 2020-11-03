
SET(THISPLUG adj_enc_aacplus)

AUX_SOURCE_DIRECTORY(. SRCFILES)
add_library (${THISPLUG} MODULE ${SRCFILES})
#SET(PLUGLIBS aacplus)
INCLUDE(../plugin_post_common.txt)
