
SET(wxWidgets_USE_LIBS base core xml adv html xrc)
FIND_PACKAGE(wxWidgets)
#FIND_PACKAGE(wxWidgets COMPONENTS base core xrc intl richtext adv locale)

IF(NOT DEBUG)

IF(wxWidgets_FOUND)
#MESSAGE(STATUS "wxWidgets found, enabling Client3 build...")

INCLUDE(${wxWidgets_USE_FILE})
#include_directories(${wxWidgets_INCLUDE_DIRS})
#add_definitions(${wxWidgets_DEFINITIONS})

#link_directories(${GLIB2_LIBRARY_DIRS} ${GTHREAD2_LIBRARY_DIRS} ${X11_LIBRARY_DIRS} ${DBUS_LIBRARY_DIRS})

AUX_SOURCE_DIRECTORY(. SRCFILES)
string(REPLACE client_wdr.cpp "" SRCFILES "${SRCFILES}")
add_executable (client ${SRCFILES}) 
TARGET_LINK_LIBRARIES(client remote ${LIBS} ${wxWidgets_LIBRARIES})

ELSE(wxWidgets_FOUND)
MESSAGE(STATUS "No wxWidgets found, skipping Client3 build...")
ENDIF(wxWidgets_FOUND)

ELSE(NOT DEBUG)
MESSAGE(STATUS "Debug build, skipping Client3 build...")
ENDIF(NOT DEBUG)
