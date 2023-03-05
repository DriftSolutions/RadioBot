AUX_SOURCE_DIRECTORY(. SRCFILES)
#AUX_SOURCE_DIRECTORY(../../Common/tinyxml TXMLFILES)
#AUX_SOURCE_DIRECTORY(../../Common/libzip LZIPFILES)
#${TXMLFILES}
#${LZIPFILES}
add_executable (radiobot ${SRCFILES} ./proto_linux/remote_protobuf.pb.cc ./pcg/pcg_basic.cpp)
TARGET_LINK_LIBRARIES(radiobot ${LIBS} physfs tinyxml-static zip-static z protobuf)

add_custom_command(
	TARGET radiobot
        POST_BUILD
        COMMAND ${EXECUTABLE_OUTPUT_PATH}/add_checksum ${EXECUTABLE_OUTPUT_PATH}/radiobot
)

