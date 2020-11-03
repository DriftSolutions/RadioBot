SET(THIS_EXE editusers)

AUX_SOURCE_DIRECTORY(. SRCFILES)
add_definitions(-D__EDITUSERS__)
add_executable (${THIS_EXE} ${SRCFILES} ../v5/src/db.cpp ../v5/src/users.cpp)
TARGET_LINK_LIBRARIES(${THIS_EXE} ${LIBS})

