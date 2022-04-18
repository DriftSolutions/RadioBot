AUX_SOURCE_DIRECTORY(../src SRCFILES)
#AUX_SOURCE_DIRECTORY(../../Common/tinyxml TXMLFILES)
#AUX_SOURCE_DIRECTORY(../../Common/libzip LZIPFILES)
#${TXMLFILES}
#${LZIPFILES}
add_definitions(-DIRCBOT_STANDALONE)
add_executable (shoutirc_autodj ${SRCFILES} ../src/proto_linux/remote_protobuf.pb.cc)
TARGET_LINK_LIBRARIES(shoutirc_autodj ${LIBS} physfs tinyxml-static zip-static z protobuf)

add_custom_command(
	TARGET shoutirc_autodj
        POST_BUILD
        COMMAND ../Output/add_checksum ../Output/shoutirc_autodj
)

