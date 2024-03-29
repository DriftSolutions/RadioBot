cmake_minimum_required (VERSION 2.8.12)

IF(CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
message(STATUS "Overriding compilers for OpenBSD...")
SET(CMAKE_C_COMPILER_INIT egcc)
SET(CMAKE_CXX_COMPILER_INIT eg++)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL OpenBSD)

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckCXXCompilerFlag)
INCLUDE(${CMAKE_ROOT}/Modules/FindPkgConfig.cmake)

project (ircbot_v5)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Output)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Output/plugins)
SET(CMAKE_RANLIB "echo")

SET(LIBS z)

FIND_PACKAGE(OpenSSL REQUIRED)
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_PATH})
link_directories(${OPENSSL_LIBRARY_DIRS})
SET(LIBS ${LIBS} ${OPENSSL_LIBRARIES})

pkg_search_module(SQLITE REQUIRED sqlite3 libsqlite3 sqlite libsqlite)
include_directories(${SQLITE_INCLUDE_DIRS})
link_directories(${SQLITE_LIBRARY_DIRS})
SET(LIBS ${LIBS} ${SQLITE_LIBRARIES})

pkg_search_module(LIBCURL REQUIRED libcurl libcurl-openssl curl libcurl-gnutls)
include_directories(${LIBCURL_INCLUDE_DIRS})
link_directories(${LIBCURL_LIBRARY_DIRS})
SET(LIBS ${LIBS} ${LIBCURL_LIBRARIES})

pkg_search_module(PROTOBUF REQUIRED protobuf)
include_directories(${PROTOBUF_INCLUDE_DIRS})
link_directories(${PROTOBUF_LIBRARY_DIRS})
SET(LIBS ${LIBS} ${PROTOBUF_LIBRARIES})

#pkg_search_module(GNUTLS REQUIRED GnuTLS gnutls)
#SET(LIBS ${LIBS} ${GNUTLS_LIBRARIES})
#INCLUDE_DIRECTORIES(${GNUTLS_INCLUDE_PATH})

pkg_search_module(MYSQL REQUIRED mysqlclient_r mysqlclient)
include_directories(${MYSQL_INCLUDE_DIRS})
link_directories(${MYSQL_LIBRARY_DIRS})
SET(LIBS ${LIBS} ${MYSQL_LIBRARIES})

SET(LIBS ${LIBS} pthread)

include_directories(/usr/local/include)
link_directories(/usr/local/lib ~/ircbot/Common/obj)
SET(DSL_DIRECTORY ${PROJECT_SOURCE_DIR}/Common/DSL/dslsrc)
include_directories(${DSL_DIRECTORY})

SET(DSL_SHARED 0)
add_definitions(-fPIC -DIB_PIC)
add_definitions(-DNO_DTLS1 -DENABLE_OPENSSL -DENABLE_MYSQL -DIRCBOT_V5 -DDSL_DEFAULT_MUTEX_TIMEOUT=1800000)
add_definitions(-Wall -D_REENTRANT -D_THREAD_SAFE -D_POSIX_C_SOURCE=200809 -D_FORTIFY_SOURCE=2 -D_FILE_OFFSET_BITS=64)

IF(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 OR CMAKE_SYSTEM_PROCESSOR STREQUAL amd64)
message(STATUS "Enabling 64-bit support...")
link_directories(/usr/lib/x86_64-linux-gnu)
ELSE(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 OR CMAKE_SYSTEM_PROCESSOR STREQUAL amd64)
# 32-bit only includes
link_directories(/usr/lib/i386-linux-gnu)
ENDIF(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 OR CMAKE_SYSTEM_PROCESSOR STREQUAL amd64)

IF(CMAKE_SYSTEM_NAME STREQUAL FreeBSD OR CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
	message(STATUS "Enabling BSD support...")
	add_definitions(-D__USE_BSD -D__XSI_VISIBLE -D__BSD_VISIBLE)
ELSE(CMAKE_SYSTEM_NAME STREQUAL FreeBSD OR CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
	SET(LIBS ${LIBS} dl)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL FreeBSD OR CMAKE_SYSTEM_NAME STREQUAL OpenBSD)

IF(CMAKE_SYSTEM_NAME STREQUAL Darwin)
	message(STATUS "Enabling Mac OS X support...")
	add_definitions(-D_DARWIN_C_SOURCE)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL Darwin)

function (OptTest option)
	check_cxx_compiler_flag(${option} OPTCHK)
	IF(OPTCHK)
		message(STATUS "GCC has ${option}")
		add_definitions(${option})
	ELSE(OPTCHK)
		message(STATUS "GCC does not have ${option}")
	ENDIF(OPTCHK)
	UNSET(OPTCHK CACHE)
endfunction(OptTest)
function (OptTest2 option def)
	check_cxx_compiler_flag(${option} OPTCHK)
	IF(OPTCHK)
		message(STATUS "GCC has ${option}")
		add_definitions(${option} ${def})
	ELSE(OPTCHK)
		message(STATUS "GCC does not have ${option}")
	ENDIF(OPTCHK)
	UNSET(OPTCHK CACHE)
endfunction(OptTest2)
OptTest2(-fvisibility=hidden -DHAVE_VIS)
OptTest(-fstack-protector-all)
OptTest(-Wstack-protector)
OptTest(-Wno-pragmas)
OptTest(-Wno-write-strings)
OptTest(-Wno-format-contains-nul)
OptTest(-Wno-c++11-narrowing)
OptTest(-Werror=format-security)
OptTest(-Werror=format-extra-args)

CHECK_CXX_COMPILER_FLAG("-std=gnu++11" COMPILER_SUPPORTS_GNU11)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_GNU11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
elseif(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	OptTest(-fpermissive)
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
	OptTest(-fpermissive)
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
#enable debugging
message(STATUS "Debug build")
set(DEBUG 1)
add_definitions(-DDEBUG -D_DEBUG)
#add_definitions(-D_GLIBCXX_DEBUG)
add_definitions(-g -ggdb)
ELSE()
#optimizations
add_definitions(-O3)
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -s")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -s")
ENDIF()

add_subdirectory (Common/tinyxml)
add_subdirectory (Common/libzip)
add_subdirectory (Common/LibSSMT)
add_subdirectory (Common/DSL)

execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND bash ./gen_distro)

SET(LIBS ibdsl ${LIBS})

add_subdirectory (add_checksum)

add_subdirectory (Common/libremote)
add_subdirectory (ibctl)
add_subdirectory (mp3sync)
add_subdirectory (editusers)

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/proto_linux)
execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND protoc -I Common --cpp_out=${PROJECT_BINARY_DIR}/proto_linux Common/remote_protobuf.proto)
execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND protoc -I Common --cpp_out=${PROJECT_BINARY_DIR}/proto_linux Common/autodj.proto)
execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND protoc -I Common --cpp_out=${PROJECT_BINARY_DIR}/proto_linux Common/mumble.proto)
include_directories(${PROJECT_BINARY_DIR}/proto_linux)

add_subdirectory (v5)
add_subdirectory (Client3 Client3)
