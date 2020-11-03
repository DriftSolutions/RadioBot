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

int lua_bind_event(lua_State * L) {
	const char * type = luaL_checkstring(L, 1);
	const char * func = luaL_checkstring(L, 2);
	api->ib_printf("bind_event: %d\n", lua_gettop(L));
	if (type && type[0] && func && func[0]) {
		LuaInstance * i = GetLuaInstance(L);
		if (lua_gettop(L) >= 3) {
			const char * parm = luaL_checkstring(L, 3);
			if (parm) {
				api->ib_printf("Lua -> Instance '%s': bind_event(%s,%s,%s)\n", (i != NULL) ? i->name.c_str():"unknown", type, func, parm);
				if (i != NULL) {
					i->AddBind(type, func, parm);
				}
			} else {
				api->ib_printf("Lua -> Instance '%s': bind_event(): parameter 3 is not string!\n", (i != NULL) ? i->name.c_str():"unknown");
			}
		} else {
			api->ib_printf("Lua -> Instance '%s': bind_event(%s,%s)\n", (i != NULL) ? i->name.c_str():"unknown", type, func);
			if (i != NULL) {
				i->AddBind(type, func);
			}
		}
	}
	return 0;
}
int lua_unbind_event(lua_State * L) {
	const char * type = luaL_checkstring(L, 1);
	const char * func = luaL_checkstring(L, 2);
	if (type && type[0] && func && func[0]) {
		LuaInstance * i = GetLuaInstance(L);
		if (lua_gettop(L) >= 3) {
			const char * parm = luaL_checkstring(L, 3);
			if (parm) {
				api->ib_printf("Lua -> Instance '%s': unbind_event(%s,%s,%s)\n", (i != NULL) ? i->name.c_str():"unknown", type, func, parm);
				if (i != NULL) {
					i->DelBind(type, func, parm);
				}
			} else {
				api->ib_printf("Lua -> Instance '%s': unbind_event(): parameter 3 is not string!\n", (i != NULL) ? i->name.c_str():"unknown");
			}
		} else {
			api->ib_printf("Lua -> Instance '%s': unbind_event(%s,%s)\n", (i != NULL) ? i->name.c_str():"unknown", type, func);
			if (i != NULL) {
				i->DelBind(type, func);
			}
		}
	}
	return 0;
}

int lua_ib_print(lua_State * L) {
	const char * type = luaL_checkstring(L, 1);
	if (type && type[0]) {
		LuaInstance * i = GetLuaInstance(L);
		api->ib_printf("Lua -> Instance '%s': %s\n", (i != NULL) ? i->name.c_str():"unknown", type);
	}
	return 0;
}

int lua_SendPM(lua_State * L) {
	lua_Integer netno = luaL_checkinteger(L, 1);
	const char * target = luaL_checkstring(L, 2);
	const char * text = luaL_checkstring(L, 3);
	if (target && *target && text) {
		stringstream sstr;
		sstr << "PRIVMSG " << target << " :" << text << "\r\n";
		api->irc->SendIRC_Priority(netno, sstr.str().c_str(), sstr.str().length(), PRIORITY_DEFAULT);
	}
	return 0;
}

int lua_SendNotice(lua_State * L) {
	lua_Integer netno = luaL_checkinteger(L, 1);
	const char * target = luaL_checkstring(L, 2);
	const char * text = luaL_checkstring(L, 3);
	if (target && *target && text) {
		stringstream sstr;
		sstr << "NOTICE " << target << " :" << text << "\r\n";
		api->irc->SendIRC_Priority(netno, sstr.str().c_str(), sstr.str().length(), PRIORITY_DEFAULT);
	}
	return 0;
}

int lua_SendAction(lua_State * L) {
	lua_Integer netno = luaL_checkinteger(L, 1);
	const char * target = luaL_checkstring(L, 2);
	const char * text = luaL_checkstring(L, 3);
	if (target && *target && text) {
		stringstream sstr;
		sstr << "PRIVMSG " << target << " :\001ACTION " << text << "\001\r\n";
		api->irc->SendIRC_Priority(netno, sstr.str().c_str(), sstr.str().length(), PRIORITY_DEFAULT);
	}
	return 0;
}

int lua_SendIRC(lua_State * L) {
	lua_Integer netno = luaL_checkinteger(L, 1);
	const char * text = luaL_checkstring(L, 2);
	lua_Integer prio = PRIORITY_DEFAULT;
	lua_Integer delay = 0;
	if (lua_gettop(L) >= 3) {
		prio = luaL_checkinteger(L, 3);
	}
	if (lua_gettop(L) >= 4) {
		delay = luaL_checkinteger(L, 4);
	}
	if (text && text[0]) {
		stringstream sstr;
		sstr << text << "\r\n";
		api->irc->SendIRC_Delay(netno, sstr.str().c_str(), sstr.str().length(), prio, delay);
	}
	return 0;
}

int lua_SendIRC2(lua_State * L) {
	//SendIRC2(netno, "text", priority, delay)
	lua_Integer netno = luaL_checkinteger(L, 1);
	const char * text = luaL_checkstring(L, 2);
	lua_Integer prio = luaL_checkinteger(L, 3);
	lua_Integer delay = luaL_checkinteger(L, 4);
	if (text && text[0]) {
		stringstream sstr;
		sstr << text << "\r\n";
		api->irc->SendIRC_Delay(netno, sstr.str().c_str(), sstr.str().length(), prio, delay);
	}
	return 0;
}

int lua_GetCurrentNick(lua_State * L) {
	lua_Integer net = luaL_checkinteger(L, 1);
	if (net >= 0 && net < MAX_IRC_SERVERS) {
		lua_pushstring(L, api->irc->GetCurrentNick(net));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int lua_GetDefaultNick(lua_State * L) {
	lua_Integer net = luaL_checkinteger(L, 1);
	lua_pushstring(L, api->irc->GetDefaultNick(net));
	return 1;
}

int lua_GetBotVersion(lua_State * L) {
	lua_pushinteger(L, api->GetVersion());
	return 1;
}

int lua_GetBotVersionString(lua_State * L) {
	lua_pushstring(L, api->GetVersionString());
	return 1;
}

int lua_GetIP(lua_State * L) {
	lua_pushstring(L, api->GetIP());
	return 1;
}

int lua_GetBasePath(lua_State * L) {
	lua_pushstring(L, api->GetBasePath());
	return 1;
}

int lua_get_argc(lua_State * L) {
	lua_pushinteger(L, api->get_argc());
	return 1;
}

int lua_get_argv(lua_State * L) {
	lua_Integer n = luaL_checkinteger(L, 1);
	if (n >= 0 && n < api->get_argc()) {
		lua_pushstring(L, api->get_argv(n));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int lua_LogToChan(lua_State * L) {
	const char *p = luaL_checkstring(L, 1);
	if (p && p[0]) {
		api->irc->LogToChan(p);
	}
	return 0;
}

int lua_Rehash(lua_State * L) {
	const char *p = luaL_checkstring(L, 1);
	if (p && p[0]) {
		api->Rehash(p);
	} else {
		api->Rehash(NULL);
	}
	return 0;
}

int lua_IsValidUserName(lua_State * L) {
	const char *p = luaL_checkstring(L, 1);
	if (p && p[0]) {
		lua_pushboolean(L, api->user->IsValidUserName(p));
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

int lua_GetUserNameFromHostmask(lua_State * L) {
	const char *p = luaL_checkstring(L, 1);
	if (p && p[0]) {
		USER * u = api->user->GetUser(p);
		if (u) {
			lua_pushstring(L, u->Nick);
			UnrefUser(u);
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int lua_GetUserFlags(lua_State * L) {
	const char *p = luaL_checkstring(L, 1);
	uint32 ret = api->user->GetDefaultFlags();
	if (p && p[0]) {
		USER * u = api->user->GetUser(p);
		if (u) {
			ret = u->Flags;
			UnrefUser(u);
		}
	}
	lua_pushinteger(L, ret);
	return 1;
}

int lua_GetUserFlagsString(lua_State * L) {
	const char *p = luaL_checkstring(L, 1);
	uint32 ret = api->user->GetDefaultFlags();
	if (p && p[0]) {
		USER * u = api->user->GetUser(p);
		if (u) {
			ret = u->Flags;
			UnrefUser(u);
		}
	}
	char buf[256];
	buf[0] = 0;
	api->user->uflag_to_string(ret, buf, sizeof(buf));
	lua_pushstring(L, buf);
	return 1;
}

int lua_GetConfigSectionValue(lua_State * L) {
	const char *sec = luaL_checkstring(L, 1);
	const char *name = luaL_checkstring(L, 2);
	if (sec && sec[0] && name && name[0]) {
		DS_CONFIG_SECTION * par = NULL;
		char * tmp = strdup(sec);
		char *p2=NULL;
		char *p = strtok_r(tmp, "/", &p2);
		while (p) {
			par = api->config->GetConfigSection(par, p);
			p = strtok_r(NULL, "/", &p2);
		}
		free(tmp);
		if (par != NULL) {
			p = api->config->GetConfigSectionValue(par, name);
			if (p) {
				lua_pushstring(L, p);
			} else {
				lua_pushnil(L);
			}
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int lua_GetConfigSectionLong(lua_State * L) {
	const char *sec = luaL_checkstring(L, 1);
	const char *name = luaL_checkstring(L, 2);
	if (sec && sec[0] && name && name[0]) {
		DS_CONFIG_SECTION * par = NULL;
		char * tmp = strdup(sec);
		char *p2=NULL;
		char *p = strtok_r(tmp, "/", &p2);
		while (p) {
			par = api->config->GetConfigSection(par, p);
			p = strtok_r(NULL, "/", &p2);
		}
		free(tmp);
		if (par != NULL) {
			lua_pushinteger(L, api->config->GetConfigSectionLong(par, name));
		} else {
			lua_pushinteger(L, -1);
		}
	} else {
		lua_pushinteger(L, -1);
	}
	return 1;
}

int lua_IsConfigSectionValue(lua_State * L) {
	const char *sec = luaL_checkstring(L, 1);
	const char *name = luaL_checkstring(L, 2);
	if (sec && sec[0] && name && name[0]) {
		DS_CONFIG_SECTION * par = NULL;
		char * tmp = strdup(sec);
		char *p2=NULL;
		char *p = strtok_r(tmp, "/", &p2);
		while (p) {
			par = api->config->GetConfigSection(par, p);
			p = strtok_r(NULL, "/", &p2);
		}
		free(tmp);
		if (par != NULL) {
			lua_pushboolean(L, api->config->IsConfigSectionValue(par, name));
		} else {
			lua_pushboolean(L, 0);
		}
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

int lua_NumNetworks(lua_State * L) {
	lua_pushinteger(L, api->irc->NumNetworks());
	return 1;
}

int lua_IsNetworkReady(lua_State * L) {
	lua_Integer netno = luaL_checkinteger(L, 1);
	lua_pushboolean(L, api->irc->IsNetworkReady(netno));
	return 1;
}

int lua_ProcText(lua_State * L) {
	const char * p = luaL_checkstring(L, 1);
	int bufSize = luaL_checkinteger(L, 2);
	if (p && p[0]) {
		char * buf = (char *)malloc(bufSize);
		strlcpy(buf, p, bufSize);
		api->ProcText(buf, bufSize);
		lua_pushstring(L, buf);
		free(buf);
	} else {
		lua_pushstring(L, "");
	}
	return 1;
}

inline void lua_add_to_table(lua_State * L, const char * key, const char * val) {
	lua_pushstring(L, key);
	lua_pushstring(L, val);
	lua_settable(L, -3);
}
inline void lua_add_to_table(lua_State * L, const char * key, lua_Integer val) {
	lua_pushstring(L, key);
	lua_pushinteger(L, val);
	lua_settable(L, -3);
}
inline void lua_add_to_table_bool(lua_State * L, const char * key, bool val) {
	lua_pushstring(L, key);
	lua_pushboolean(L, val);
	lua_settable(L, -3);
}

int lua_GetStreamInfo(lua_State * L) {
	STATS * s = api->ss->GetStreamInfo();
	lua_newtable(L);
	lua_add_to_table_bool(L, "online", s->online);
	lua_add_to_table_bool(L, "title_changed", s->title_changed);
	lua_add_to_table(L, "bitrate", s->bitrate);
	lua_add_to_table(L, "listeners", s->listeners);
	lua_add_to_table(L, "peak", s->peak);
	lua_add_to_table(L, "maxusers", s->maxusers);
	lua_add_to_table(L, "curdj", s->curdj);
	lua_add_to_table(L, "lastdj", s->lastdj);
	lua_add_to_table(L, "cursong", s->cursong);
	return 1;
}

int lua_AreRatingsEnabled(lua_State * L) {
	lua_pushboolean(L, api->ss->AreRatingsEnabled());
	return 1;
}

int lua_GetMaxRating(lua_State * L) {
	lua_pushinteger(L, api->ss->GetMaxRating());
	return 1;
}

int lua_GetSongRating(lua_State * L) {
	const char * p = luaL_checkstring(L, 1);
	SONG_RATING r;
	memset(&r, 0, sizeof(r));
	if (p && p[0]) {
		api->ss->GetSongRating(p, &r);
	}
	lua_newtable(L);
	lua_add_to_table(L, "rating", r.Rating);
	lua_add_to_table(L, "votes", r.Votes);
	return 1;
}

int lua_LoadMessage(lua_State * L) {
	const char * p = luaL_checkstring(L, 1);
	int bufSize = luaL_checkinteger(L, 2);
	if (p && p[0] && bufSize > 0) {
		char * buf = (char *)malloc(bufSize);
		if (api->LoadMessage(p, buf, bufSize)) {
			lua_pushstring(L, buf);
		} else {
			lua_pushnil(L);
		}
		free(buf);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int lua_genrand_int32(lua_State * L) {
	lua_pushinteger(L, api->genrand_int32());
	return 1;
}

int lua_SendRemoteReply(lua_State * L) {
	LuaPointer * ptr = (LuaPointer *)lua_touserdata(L, 1);
	REMOTE_HEADER h;
	h.scmd = (REMOTE_COMMANDS_S2C)luaL_checkinteger(L, 2);
	h.datalen = (uint32)luaL_checkinteger(L, 3);
	const char * p = NULL;
	if (lua_gettop(L) >= 4) {
		p = luaL_checkstring(L, 4);
	}

	if (h.datalen > 0 && p == NULL) {
		LuaInstance * i = GetLuaInstance(L);
		api->ib_printf("Lua -> Instance '%s': SendRemoteReply(): datalen > 0 but data == NULL!\n", (i != NULL) ? i->name.c_str():"unknown");
	} else {
		api->SendRemoteReply(ptr->sock, &h, p);
	}
	return 0;
}

//void (*SetDoSpam)(bool dospam);
int lua_SetDoSpam(lua_State * L) {
	luaL_checktype(L, 1, LUA_TBOOLEAN);
	bool dospam = lua_toboolean(L, 1);
	api->irc->SetDoSpam(dospam);
	return 0;
}

//void (*SetDoSpamChannel)(bool dospam, int netno, const char * chan);
int lua_SetDoSpamChannel(lua_State * L) {
	luaL_checktype(L, 1, LUA_TBOOLEAN);
	bool dospam = lua_toboolean(L, 1);
	int netno = luaL_checkinteger(L, 2);
	const char * chan = luaL_checkstring(L, 3);
	api->irc->SetDoSpamChannel(dospam, netno, chan);
	return 0;
}

int lua_SetDoOnJoin(lua_State * L) {
	luaL_checktype(L, 1, LUA_TBOOLEAN);
	bool dospam = lua_toboolean(L, 1);
	api->irc->SetDoOnJoin(dospam);
	return 0;
}

int lua_SetDoOnJoinChannel(lua_State * L) {
	luaL_checktype(L, 1, LUA_TBOOLEAN);
	bool dospam = lua_toboolean(L, 1);
	int netno = luaL_checkinteger(L, 2);
	const char * chan = luaL_checkstring(L, 3);
	api->irc->SetDoOnJoinChannel(dospam, netno, chan);
	return 0;
}

int lua_SendSMS(lua_State * L) {
	const char * target = luaL_checkstring(L, 1);
	const char * text = luaL_checkstring(L, 2);
	if (target && *target && text) {
		SMS_INTERFACE sms;
		if (api->SendMessage(-1, SMS_GET_INTERFACE, (char *)&sms, sizeof(sms)) == 1) {
			sms.send_sms(target, text);
		} else {
			api->ib_printf("Lua -> Error sending SMS: could not load SMS interface!\n");
			api->ib_printf("Lua -> Is the SMS plugin loaded and configured?\n");
		}
	}
	return 0;
}

void register_functions(LuaInstance * inst) {
	lua_pushstring(inst->inst, api->platform);
	lua_setglobal(inst->inst, "PLATFORM");
	lua_pushinteger(inst->inst, PRIORITY_LOWEST);
	lua_setglobal(inst->inst, "PRIORITY_LOWEST");
	lua_pushinteger(inst->inst, PRIORITY_SPAM);
	lua_setglobal(inst->inst, "PRIORITY_SPAM");
	lua_pushinteger(inst->inst, PRIORITY_DEFAULT);
	lua_setglobal(inst->inst, "PRIORITY_DEFAULT");
	lua_pushinteger(inst->inst, PRIORITY_INTERACTIVE);
	lua_setglobal(inst->inst, "PRIORITY_INTERACTIVE");
	lua_pushinteger(inst->inst, PRIORITY_IMMEDIATE);
	lua_setglobal(inst->inst, "PRIORITY_IMMEDIATE");

	lua_pushcfunction(inst->inst, lua_bind_event);
    lua_setglobal(inst->inst, "bind_event");
	lua_pushcfunction(inst->inst, lua_unbind_event);
    lua_setglobal(inst->inst, "unbind_event");
	lua_pushcfunction(inst->inst, lua_ib_print);
    lua_setglobal(inst->inst, "ib_print");
	if (api->irc != NULL) {
		lua_pushcfunction(inst->inst, lua_SendPM);
		lua_setglobal(inst->inst, "SendPM");
		lua_pushcfunction(inst->inst, lua_SendPM);
		lua_setglobal(inst->inst, "msg");
		lua_pushcfunction(inst->inst, lua_SendNotice);
		lua_setglobal(inst->inst, "SendNotice");
		lua_pushcfunction(inst->inst, lua_SendNotice);
		lua_setglobal(inst->inst, "notice");
		lua_pushcfunction(inst->inst, lua_SendAction);
		lua_setglobal(inst->inst, "SendAction");
		lua_pushcfunction(inst->inst, lua_SendIRC);
		lua_setglobal(inst->inst, "SendIRC");
		lua_pushcfunction(inst->inst, lua_SendIRC2);
		lua_setglobal(inst->inst, "SendIRC2");

		lua_pushcfunction(inst->inst, lua_GetCurrentNick);
		lua_setglobal(inst->inst, "GetCurrentNick");
		lua_pushcfunction(inst->inst, lua_GetDefaultNick);
		lua_setglobal(inst->inst, "GetDefaultNick");
	}
	lua_pushcfunction(inst->inst, lua_GetBotVersion);
    lua_setglobal(inst->inst, "GetBotVersion");
	lua_pushcfunction(inst->inst, lua_GetBotVersionString);
    lua_setglobal(inst->inst, "GetBotVersionString");
	lua_pushcfunction(inst->inst, lua_GetIP);
    lua_setglobal(inst->inst, "GetIP");
	lua_pushcfunction(inst->inst, lua_GetBasePath);
    lua_setglobal(inst->inst, "GetBasePath");
	lua_pushcfunction(inst->inst, lua_get_argc);
    lua_setglobal(inst->inst, "get_argc");
	lua_pushcfunction(inst->inst, lua_get_argv);
    lua_setglobal(inst->inst, "get_argv");
	lua_pushcfunction(inst->inst, lua_LoadMessage);
    lua_setglobal(inst->inst, "LoadMessage");
	lua_pushcfunction(inst->inst, lua_Rehash);
    lua_setglobal(inst->inst, "Rehash");
	if (api->irc != NULL) {
		lua_pushcfunction(inst->inst, lua_LogToChan);
		lua_setglobal(inst->inst, "LogToChan");
		lua_pushcfunction(inst->inst, lua_SetDoSpam);
		lua_setglobal(inst->inst, "SetDoSpam");
		lua_pushcfunction(inst->inst, lua_SetDoSpamChannel);
		lua_setglobal(inst->inst, "SetDoSpamChannel");
		lua_pushcfunction(inst->inst, lua_SetDoOnJoin);
		lua_setglobal(inst->inst, "SetDoOnJoin");
		lua_pushcfunction(inst->inst, lua_SetDoOnJoinChannel);
		lua_setglobal(inst->inst, "SetDoOnJoinChannel");
	}
	lua_pushcfunction(inst->inst, lua_IsValidUserName);
    lua_setglobal(inst->inst, "IsValidUserName");
	lua_pushcfunction(inst->inst, lua_GetUserNameFromHostmask);
    lua_setglobal(inst->inst, "GetUserNameFromHostmask");
	lua_pushcfunction(inst->inst, lua_GetUserFlags);
    lua_setglobal(inst->inst, "GetUserFlags");
	lua_pushcfunction(inst->inst, lua_GetUserFlagsString);
    lua_setglobal(inst->inst, "GetUserFlagsString");

	lua_pushcfunction(inst->inst, lua_GetConfigSectionValue);
    lua_setglobal(inst->inst, "GetConfigSectionValue");
	lua_pushcfunction(inst->inst, lua_GetConfigSectionLong);
    lua_setglobal(inst->inst, "GetConfigSectionLong");
	lua_pushcfunction(inst->inst, lua_IsConfigSectionValue);
    lua_setglobal(inst->inst, "IsConfigSectionValue");
	if (api->irc != NULL) {
		lua_pushcfunction(inst->inst, lua_NumNetworks);
		lua_setglobal(inst->inst, "NumNetworks");
		lua_pushcfunction(inst->inst, lua_IsNetworkReady);
		lua_setglobal(inst->inst, "IsNetworkReady");
	}
	lua_pushcfunction(inst->inst, lua_ProcText);
    lua_setglobal(inst->inst, "ProcText");

	if (api->ss != NULL) {
	lua_pushcfunction(inst->inst, lua_GetStreamInfo);
		lua_setglobal(inst->inst, "GetStreamInfo");
		lua_pushcfunction(inst->inst, lua_AreRatingsEnabled);
		lua_setglobal(inst->inst, "AreRatingsEnabled");
		lua_pushcfunction(inst->inst, lua_GetMaxRating);
		lua_setglobal(inst->inst, "GetMaxRating");
		lua_pushcfunction(inst->inst, lua_GetSongRating);
		lua_setglobal(inst->inst, "GetSongRating");
	}
	lua_pushcfunction(inst->inst, lua_genrand_int32);
    lua_setglobal(inst->inst, "genrand_int32");

	lua_pushcfunction(inst->inst, lua_SendRemoteReply);
    lua_setglobal(inst->inst, "SendRemoteReply");

	lua_pushcfunction(inst->inst, lua_SendSMS);
    lua_setglobal(inst->inst, "SendSMS");
}
