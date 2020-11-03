SET(THISPLUG Lua)

pkg_search_module(LUA OPTIONAL lua5.5 lua-5.4 lua5.4 lua-5.4 lua5.3 lua-5.3 lua5.2 lua-5.2 lua5.1 lua-5.1 lua5.0 lua)
IF(LUA_FOUND)
include_directories(${LUA_INCLUDE_DIRS})
link_directories(${LUA_LIBRARY_DIRS})
SET(PLUGLIBS ${LUA_LIBRARIES})
ELSE(LUA_FOUND)
MESSAGE(STATUS "Lua not found...")
ENDIF(LUA_FOUND)

INCLUDE(../plugin_generic.txt)
