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

#include "../../src/plugins.h"
#include "../SMS/sms_interface.h"

#if defined(WIN32)
	#define LUA_BUILD_AS_DLL
#endif
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

extern BOTAPI_DEF *api;
extern int pluginnum; // the number we were assigned
extern Titus_Mutex luaMutex;

class LuaBind {
public:
	string func;
	string parm;
};

class LuaInstance {
public:
	string name;
	lua_State * inst;
	typedef std::vector<pair<string, LuaBind> > bindList;
	bindList binds;
	bool load_error;

	LuaInstance();
	~LuaInstance();

	void AddBind(string ev, string func, string parm="");
	void DelBind(string ev, string func, string parm="");
	void CallVoid0Binds(const char * type);
	void CallSSBinds(const char * type, bool song_changed);
	int CallPMBinds(const char * type, USER_PRESENCE * ut, const char * text);
	int CallChannelBinds(const char * type, USER_PRESENCE * ut, const char * text);
	int CallJoinBinds(const char * type, USER_PRESENCE * ut);
	int CallQuitBinds(const char * type, USER_PRESENCE * ut, const char * text);
	int CallNickChangeBinds(int netno, const char * old_nick, const char * new_nick);
	int CallKickedBinds(const char * type, USER_PRESENCE * ut, const char * kickee_hostmask, const char * kickee, const char * reason);
	int CallRemoteBinds(T_SOCKET * socket, USER * ut, unsigned char cliversion, REMOTE_HEADER * head, const char * data);
	int CallSMSBinds(const char * type, USER_PRESENCE * ut, const char * phone, const char * text);
};

LuaInstance * GetLuaInstance(lua_State * inst);
void register_functions(LuaInstance * inst);

struct LuaPointer {
	union {
		void * ptr;
		T_SOCKET * sock;
	};
};

int IsFunction(LuaInstance * inst, const char * name);
int CallFuncI0(LuaInstance * inst, const char * name);
void CallFuncV0(LuaInstance * inst, const char * name);
void CallFuncV1B(LuaInstance * inst, const char * name, int p1);
int CallFuncI3ISS(LuaInstance * inst, const char * name, int p1, const char * p2, const char * p3);
int CallFuncI4SSUS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, const char * p4);
int CallFuncI5SSUIS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, lua_Integer p4, const char * p5);
int CallFuncI6SSUISS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, lua_Integer p4, const char * p5, const char * p6);
int CallFuncI7PSUIUUS(LuaInstance * inst, const char * name, void * p1, const char * p2, lua_Integer p3, lua_Integer p4, lua_Integer p5, lua_Integer p6, const char * p7);
int CallFuncI8SSUISSSS(LuaInstance * inst, const char * name, const char * p1, const char * p2, lua_Integer p3, lua_Integer p4, const char * p5, const char * p6, const char * p7, const char * p8);
