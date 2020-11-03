//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2020 Drift Solutions / Indy Sams            |
|        More information available at https://www.shoutirc.com        |
|                                                                      |
|                    This file is part of RadioBot.                    |
|                                                                      |
|   RadioBot is free software: you can redistribute it and/or modify   |
| it under the terms of the GNU General Public License as published by |
|  the Free Software Foundation, either version 3 of the License, or   |
|                 (at your option) any later version.                  |
|                                                                      |
|     RadioBot is distributed in the hope that it will be useful,      |
|    but WITHOUT ANY WARRANTY; without even the implied warranty of    |
|     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the     |
|             GNU General Public License for more details.             |
|                                                                      |
|  You should have received a copy of the GNU General Public License   |
|  along with RadioBot. If not, see <https://www.gnu.org/licenses/>.   |
\**********************************************************************/
//@AUTOHEADER@END@

#include "lua_plugin.h"

int IsFunction(LuaInstance * inst, const char * name) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = false;
	if (lua_isfunction(inst->inst, -1)) {
		ret = true;
	}
	lua_pop(inst->inst, 1);
	return ret;
}

int CallFuncI0(LuaInstance * inst, const char * name) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = -1;
	if (lua_isfunction(inst->inst, -1)) {
		lua_call(inst->inst, 0, 1);
		ret = luaL_checkinteger(inst->inst, -1);
	} else {
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
	//if function exists or not, we need to pop off the stack
	lua_pop(inst->inst, 1);
	return ret;
}

void CallFuncV0(LuaInstance * inst, const char * name) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	if (lua_isfunction(inst->inst, -1)) {
		lua_call(inst->inst, 0, 0);
	} else {
		lua_pop(inst->inst, 1);
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
}

void CallFuncV1B(LuaInstance * inst, const char * name, int p1) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	if (lua_isfunction(inst->inst, -1)) {
		lua_pushboolean(inst->inst, p1);
		lua_call(inst->inst, 1, 0);
	} else {
		lua_pop(inst->inst, 1);
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
}

int CallFuncI3ISS(LuaInstance * inst, const char * name, int p1, const char * p2, const char * p3) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = -1;
	if (lua_isfunction(inst->inst, -1)) {
		lua_pushinteger(inst->inst, p1);
		lua_pushstring(inst->inst, p2);
		lua_pushstring(inst->inst, p3);
		lua_call(inst->inst, 3, 1);
		ret = luaL_checkinteger(inst->inst, -1);
	} else {
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
	//if function exists or not, we need to pop off the stack
	lua_pop(inst->inst, 1);
	return ret;
}

int CallFuncI4SSUS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, const char * p4) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = -1;
	if (lua_isfunction(inst->inst, -1)) {
		lua_pushstring(inst->inst, p1);
		lua_pushstring(inst->inst, p2);
		lua_pushinteger(inst->inst, p3);
		lua_pushstring(inst->inst, p4);
		lua_call(inst->inst, 4, 1);
		ret = luaL_checkinteger(inst->inst, -1);
	} else {
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
	//if function exists or not, we need to pop off the stack
	lua_pop(inst->inst, 1);
	return ret;
}

int CallFuncI5SSUIS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, lua_Integer p4, const char * p5) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = -1;
	if (lua_isfunction(inst->inst, -1)) {
		lua_pushstring(inst->inst, p1);
		lua_pushstring(inst->inst, p2);
		lua_pushinteger(inst->inst, p3);
		lua_pushinteger(inst->inst, p4);
		lua_pushstring(inst->inst, p5);
		lua_call(inst->inst, 5, 1);
		ret = luaL_checkinteger(inst->inst, -1);
	} else {
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
	//if function exists or not, we need to pop off the stack
	lua_pop(inst->inst, 1);
	return ret;
}

int CallFuncI6SSUISS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, lua_Integer p4, const char * p5, const char * p6) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = -1;
	if (lua_isfunction(inst->inst, -1)) {
		lua_pushstring(inst->inst, p1);
		lua_pushstring(inst->inst, p2);
		lua_pushinteger(inst->inst, p3);
		lua_pushinteger(inst->inst, p4);
		lua_pushstring(inst->inst, p5);
		lua_pushstring(inst->inst, p6);
		lua_call(inst->inst, 6, 1);
		ret = luaL_checkinteger(inst->inst, -1);
	} else {
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
	//if function exists or not, we need to pop off the stack
	lua_pop(inst->inst, 1);
	return ret;
}

int CallFuncI7PSUIUUS(LuaInstance * inst, const char * name, void * p1, const char * p2, lua_Integer p3, lua_Integer p4, lua_Integer p5, lua_Integer p6, const char * p7) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = -1;
	if (lua_isfunction(inst->inst, -1)) {
		LuaPointer * x = (LuaPointer *)lua_newuserdata (inst->inst, sizeof(LuaPointer));
		x->ptr = p1;
		lua_pushstring(inst->inst, p2);
		lua_pushinteger(inst->inst, p3);
		lua_pushinteger(inst->inst, p4);
		lua_pushinteger(inst->inst, p5);
		lua_pushinteger(inst->inst, p6);
		lua_pushstring(inst->inst, p7);
		lua_call(inst->inst, 7, 1);
		ret = luaL_checkinteger(inst->inst, -1);
	} else {
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
	//if function exists or not, we need to pop off the stack
	lua_pop(inst->inst, 1);
	return ret;
}

int CallFuncI8SSUISSSS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, lua_Integer p4, const char * p5, const char * p6, const char * p7, const char * p8) {
	AutoMutex(luaMutex);
#if LUA_VERSION_NUM >= 502
	lua_getglobal(inst->inst, name);
#else
	lua_getfield(inst->inst, LUA_GLOBALSINDEX, name);
#endif
	int ret = -1;
	if (lua_isfunction(inst->inst, -1)) {
		lua_pushstring(inst->inst, p1);
		lua_pushstring(inst->inst, p2);
		lua_pushinteger(inst->inst, p3);
		lua_pushinteger(inst->inst, p4);
		lua_pushstring(inst->inst, p5);
		lua_pushstring(inst->inst, p6);
		lua_pushstring(inst->inst, p7);
		lua_pushstring(inst->inst, p8);
		lua_call(inst->inst, 8, 1);
		ret = luaL_checkinteger(inst->inst, -1);
	} else {
		api->ib_printf("Lua -> Instance '%s': no function called '%s'\n", inst->name.c_str(), name);
	}
	//if function exists or not, we need to pop off the stack
	lua_pop(inst->inst, 1);
	return ret;
}

