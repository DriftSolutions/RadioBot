
AUX_SOURCE_DIRECTORY(. SRCFILES)
add_definitions(-DTIXML_USE_STL)
add_library (tinyxml-static STATIC ${SRCFILES}) 
#SET_TARGET_PROPERTIES(tinyxml-static PROPERTIES LIBRARY_OUTPUT_DIRECTORY .)
SET_TARGET_PROPERTIES(tinyxml-static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ../../obj)
