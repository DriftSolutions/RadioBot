SET(THIS_EXE ibctl)

AUX_SOURCE_DIRECTORY(. SRCFILES)
#AUX_SOURCE_DIRECTORY(../../Common/libzip LZIPFILES)
#${TXMLFILES}
#${LZIPFILES}
add_executable (${THIS_EXE} ${SRCFILES})
TARGET_LINK_LIBRARIES(${THIS_EXE} remote ${LIBS})

