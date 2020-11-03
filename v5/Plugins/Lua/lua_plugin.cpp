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

#if defined(WIN32)
	#if defined(DEBUG)
		#pragma comment(lib, "lua53_d.lib")
	#else
		#pragma comment(lib, "lua53.lib")
	#endif
#endif

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned
Titus_Mutex luaMutex;

LuaInstance::LuaInstance() {
	AutoMutex(luaMutex);
	load_error = false;
	inst = luaL_newstate();
	if (inst != NULL) {
		luaL_openlibs(inst);
		register_functions(this);
	}
}

LuaInstance::~LuaInstance() {
	AutoMutex(luaMutex);
	if (inst != NULL) {
		if (!load_error) {
			CallFuncV0(this, "quit");
		}
		binds.clear();
		lua_close(inst);
	}
}

void LuaInstance::AddBind(string ev, string func, string parm) {
	AutoMutex(luaMutex);
	LuaBind b;
	b.func = func;
	b.parm = parm;
	binds.push_back(pair<string, LuaBind>(ev,b));
}

void LuaInstance::DelBind(string ev, string func, string parm) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(ev.c_str(), x->first.c_str()) && !stricmp(func.c_str(), x->second.func.c_str()) && !stricmp(parm.c_str(), x->second.parm.c_str())) {
			binds.erase(x);
			break;
		}
	}
}

void LuaInstance::CallVoid0Binds(const char * type) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), type)) {
			CallFuncV0(this, x->second.func.c_str());
		}
	}
}

void LuaInstance::CallSSBinds(const char * type, bool song_changed) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), type)) {
			CallFuncV1B(this, x->second.func.c_str(), song_changed);
		}
	}
}

int LuaInstance::CallPMBinds(const char * type, USER_PRESENCE * ut, const char * text) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), type) && (x->second.parm.length() == 0 || wildicmp(x->second.parm.c_str(), text))) {
			//hostmask nick uflags netno message
			int i = CallFuncI5SSUIS(this, x->second.func.c_str(), ut->Hostmask, ut->Nick, ut->Flags, ut->NetNo, text);
			if (i) { return i; }
		}
	}
	return 0;
}

int LuaInstance::CallSMSBinds(const char * type, USER_PRESENCE * ut, const char * phone, const char * text) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), type) && (x->second.parm.length() == 0 || wildicmp(x->second.parm.c_str(), text))) {
			//hostmask phone uflags message
			int i = CallFuncI4SSUS(this, x->second.func.c_str(), ut->Hostmask, phone, ut->Flags, text);
			if (i) { return i; }
		}
	}
	return 0;
}

int LuaInstance::CallChannelBinds(const char * type, USER_PRESENCE * ut, const char * text) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), type) && (x->second.parm.length() == 0 || wildicmp(x->second.parm.c_str(), text))) {
			//hostmask nick uflags netno channel message
			int i = CallFuncI6SSUISS(this, x->second.func.c_str(), ut->Hostmask, ut->Nick, ut->Flags, ut->NetNo, ut->Channel, text);
			if (i) { return i; }
		}
	}
	return 0;
}

int LuaInstance::CallJoinBinds(const char * type, USER_PRESENCE * ut) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), type)) {
			//hostmask nick uflags netno channel
			int i = CallFuncI5SSUIS(this, x->second.func.c_str(), ut->Hostmask, ut->Nick, ut->Flags, ut->NetNo, ut->Channel);
			if (i) { return i; }
		}
	}
	return 0;
}

int LuaInstance::CallNickChangeBinds(int netno, const char * old_nick, const char * new_nick) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), "on_nick")) {
			//netno, old_nick, new_nick
			int i = CallFuncI3ISS(this, x->second.func.c_str(), netno, old_nick, new_nick);
			if (i) { return i; }
		}
	}
	return 0;
}

int LuaInstance::CallKickedBinds(const char * type, USER_PRESENCE * ut, const char * kickee_hostmask, const char * kickee, const char * reason) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), type)) {
			//hostmask, nick, uflags, netno, channel, kicked_hostmask, kicked_nick, reason
			int i = CallFuncI8SSUISSSS(this, x->second.func.c_str(), ut->Hostmask, ut->Nick, ut->Flags, ut->NetNo, ut->Channel, kickee_hostmask, kickee, reason);
			if (i) { return i; }
		}
	}
	return 0;
}
int LuaInstance::CallRemoteBinds(T_SOCKET * socket, USER * ut, unsigned char cliversion, REMOTE_HEADER * head, const char * data) {
	AutoMutex(luaMutex);
	for (bindList::iterator x = binds.begin(); x != binds.end(); x++) {
		if (!stricmp(x->first.c_str(), "remote")) {
			//sock, nick, uflags, cliversion, command, datalen, data
			int i = CallFuncI7PSUIUUS(this, x->second.func.c_str(), socket, ut->Nick, ut->Flags, cliversion, head->scmd, head->datalen, data ? data:"");
			if (i) { return i; }
		}
	}
	return 0;
}

typedef std::map<lua_State *, LuaInstance *> luaInst;
luaInst lua_instances;

LuaInstance * GetLuaInstance(lua_State * inst) {
	AutoMutex(luaMutex);
	luaInst::iterator x = lua_instances.find(inst);
	if (x != lua_instances.end()) {
		return x->second;
	}
	return NULL;
}

LuaInstance * GetLuaInstance(const char * name, bool create_if_not_exist=false) {
	AutoMutex(luaMutex);
	for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
		if (!stricmp(x->second->name.c_str(), name)) {
			return x->second;
		}
	}
	LuaInstance * ret = NULL;
	if (create_if_not_exist) {
		LuaInstance * inst = new LuaInstance();
		inst->name = name;
		if (inst->inst != NULL) {
			ret = inst;
			lua_instances[inst->inst] = inst;
		} else {
			delete inst;
		}
	}
	return ret;
}

bool LoadScript(const char * fn) {
	api->ib_printf("Lua -> Loading script '%s' ...\n", fn);

	string inst = nopath(fn);
	//bool custinst = false;
	char buf[1024];
	FILE * fp = fopen(fn, "rb");
	if (fp != NULL) {
		while (!feof(fp) && fgets(buf, sizeof(buf), fp)) {
			strtrim(buf);
			if (buf[0] == '-' && buf[1] == '-') {
				if (!strnicmp(buf, "--INST:", 7)) {
					char * p = &buf[7];
					//if (stricmp(p, inst.c_str())) {
						//custinst = true;
					//}
					api->ib_printf("Instance: %s\n", p);
					inst = p;
				}
			} else {
				break;
			}
		}
		fclose(fp);
	}

	AutoMutex(luaMutex);
	LuaInstance * lua_inst = GetLuaInstance(inst.c_str(), true);
	if (lua_inst) {
		if (luaL_dofile(lua_inst->inst, fn) == 0) {
			api->ib_printf("Lua -> Script loaded OK.\n");
		} else {
			api->ib_printf("Lua -> Error loading script '%s' ...\n", lua_tostring(lua_inst->inst, -1));
			lua_inst->load_error = true;
		}
	} else {
		api->ib_printf("Lua -> Error creating Lua instance!\n");
	}
	return true;
}

void UnloadScripts() {
	AutoMutex(luaMutex);
	for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
		delete x->second;
	}
	lua_instances.clear();
}

/*
int CallFunc1(lua_State * inst, const char * name) {
	lua_getfield(L, LUA_GLOBALSINDEX, name);
	lua_pushstring(inst, name);
	lua_call(inst, 0, 1);
	return 0;
}
*/

bool LoadScripts() {
UnloadScripts();

	bool ret = true;
	api->ib_printf("Lua -> Searching for scripts in ./lua_scripts ...\n");
	if (access("." PATH_SEPS "lua_scripts", 0) != 0) {
		dsl_mkdir("." PATH_SEPS "lua_scripts", 0755);
	}
	Directory dir("." PATH_SEPS "lua_scripts");
	char buf[MAX_PATH];
	bool is_dir;
	while (dir.Read(buf, sizeof(buf), &is_dir)) {
		char * ext = strrchr(buf, '.');
		if (ext && !stricmp(ext, ".lua")) {
			string ffn = "." PATH_SEPS "lua_scripts" PATH_SEPS "";
			ffn += buf;
			LoadScript(ffn.c_str());
		}
	}

	//call init functions
	for (luaInst::const_iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
		if (IsFunction(x->second, "init")) {
			int ret = CallFuncI0(x->second, "init");
			if (ret == 0) {
				api->ib_printf("Lua -> Instance '%s' init returned 0\n", x->second->name.c_str());
				x->second->load_error = true;
			}
		}
	}
	//unload error'ed instances
	for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end();) {
		if (x->second->load_error) {
			ret = false;
			delete x->second;
			lua_instances.erase(x);
			x = lua_instances.begin();
		} else {
			x++;
		}
	}
	return ret;
}

int Lua_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "lua-reload")) {
		ut->std_reply(ut, _("Reloading scripts..."));
		if (!LoadScripts()) {
			ut->std_reply(ut, _("There were errors loading the scripts! Check the bot console for details..."));
		}
	}
	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	LoadScripts();
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, UFLAG_MASTER, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "lua-reload", &acl, CM_ALLOW_ALL_PRIVATE, Lua_Commands, "This command will reload all your Lua scripts");
	return 1;
}

void quit() {
	api->ib_printf(_("Lua -> Shutting down...\n"));
	UnloadScripts();
	api->ib_printf(_("Lua -> Shut down.\n"));
}

int message(unsigned int MsgID, char * data, int datalen) {
	switch(MsgID) {
#if !defined(IRCBOT_LITE)
	case IB_SS_DRAGCOMPLETE:{
				bool * song_changed = (bool *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					x->second->CallSSBinds("ss_dragcomplete", *song_changed);
				}
			}
			break;
#endif

	case IB_ONTEXT:{
				IBM_USERTEXT * ut = (IBM_USERTEXT *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					if (ut->type == IBM_UTT_CHANNEL) {
						int n = x->second->CallChannelBinds("on_text", ut->userpres, ut->text);
						if (n) {
							return n;
						}
					} else {
						int n = x->second->CallPMBinds("on_pm", ut->userpres, ut->text);
						if (n) {
							return n;
						}
					}
				}
				return 0;
			}
			break;

		case SMS_ON_RECEIVED:{
				SMS_MESSAGE * ut = (SMS_MESSAGE *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallSMSBinds("on_sms", ut->pres, ut->phone, ut->text);
					if (n) {
						return n;
					}
				}
				return 0;
			}
			break;

		case IB_ONNOTICE:{
				IBM_USERTEXT * ut = (IBM_USERTEXT *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					if (ut->type == IBM_UTT_CHANNEL) {
						int n = x->second->CallChannelBinds("on_notice_text", ut->userpres, ut->text);
						if (n) {
							return n;
						}
					} else {
						int n = x->second->CallPMBinds("on_notice_pm", ut->userpres, ut->text);
						if (n) {
							return n;
						}
					}
				}
				return 0;
			}
			break;

		case IB_ONBAN:{
				IBM_ON_BAN * ut = (IBM_ON_BAN *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallChannelBinds("on_ban", ut->banner, ut->banmask);
					if (n) {
						return n;
					}
				}
				return 0;
			}
			break;

		case IB_ONUNBAN:{
				IBM_ON_BAN * ut = (IBM_ON_BAN *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallChannelBinds("on_unban", ut->banner, ut->banmask);
					if (n) {
						return n;
					}
				}
				return 0;
			}
			break;

		case IB_ONTOPIC:{
				IBM_USERTEXT * ut = (IBM_USERTEXT *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallChannelBinds("on_topic", ut->userpres, ut->text);
					if (n) { return n; }
				}
				return 0;
			}
			break;

		case IB_ONJOIN:{
				USER_PRESENCE * ut = (USER_PRESENCE *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallJoinBinds("on_join", ut);
					if (n) { return n; }
				}
				return 0;
			}
			break;

		case IB_ONPART:{
				IBM_USERTEXT * ut = (IBM_USERTEXT *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallChannelBinds("on_part", ut->userpres, ut->text);
					if (n) { return n; }
				}
				return 0;
			}
			break;

		case IB_ONQUIT:{
				IBM_USERTEXT * ut = (IBM_USERTEXT *)data;
				if (ut->type == IBM_UTT_PRIVMSG) {
					AutoMutex(luaMutex);
					for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
						int n = x->second->CallPMBinds("on_quit", ut->userpres, ut->text);
						if (n) { return n; }
					}
				}
				return 0;
			}
			break;

		case IB_ONNICK:{
				IBM_NICKCHANGE * in = (IBM_NICKCHANGE *)data;
				if (in->channel == NULL) {
					AutoMutex(luaMutex);
					for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
						int n = x->second->CallNickChangeBinds(in->netno, in->old_nick, in->new_nick);
						if (n) { return n; }
					}
				}
				return 0;
			}
			break;

		case IB_ONKICK:{
				IBM_ON_KICK * ut = (IBM_ON_KICK *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallKickedBinds("on_kick", ut->kicker, ut->kickee_hostmask, ut->kickee, ut->reason);
					if (n) { return n; }
				}
				return 0;
			}
			break;

		case IB_REMOTE:{
				IBM_REMOTE * ut = (IBM_REMOTE *)data;
				AutoMutex(luaMutex);
				for (luaInst::iterator x = lua_instances.begin(); x != lua_instances.end(); x++) {
					int n = x->second->CallRemoteBinds(ut->userpres->Sock, ut->userpres->User, ut->cliversion, ut->head, ut->data);
					if (n) {
						return n;
					}
				}
				return 0;
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{50C94AD7-3759-4E77-877B-1B836B138312}",
	"Lua",
	"Lua Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
