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

BOTAPI_DEF * api = NULL;
int pluginnum = 0;

void LoadPlugin();

int init(int num, BOTAPI_DEF * BotAPI) {
	api = (BOTAPI_DEF *)BotAPI;
	pluginnum = num;
	api->db->Query("DROP TABLE varsetter", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS varsetter(ID INTEGER PRIMARY KEY AUTOINCREMENT, `Nick` VARCHAR(255), `Type` INT, `FlagsNeeded` INT UNSIGNED, `Var` VARCHAR(255), `OriginalValue` VARCHAR(255), `Value` VARCHAR(255));", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS varsetter_u ON varsetter(Var);", NULL, NULL, NULL);
	LoadPlugin();
	return 1;
}

char * sqlite_escape(const char * str) {
	char * ret = (char *)zmalloc(1);
	ret[0]=0;
	size_t rlen = 0;
	size_t len = strlen(str);
	for (size_t i=0; i < len; i++) {
		ret = (char *)zrealloc(ret, rlen+2);
		ret[rlen] = str[i];
		rlen++;
		if (str[i] == '\'') {
			ret = (char *)zrealloc(ret, rlen+2);
			ret[rlen] = str[i];
			rlen++;
		}
		ret[rlen] = 0;
	}
	return ret;
}

sql_rows GetTable(const char * query, char **errmsg) {
//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = api->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		api->ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
	} else {
		if (ncol && nrow) {
			for (int i=0; i < nrow; i++) {
				sql_row row;
				for (int s=0; s < ncol; s++) {
					char * p = resultp[(i*ncol)+ncol+s];
					if (!p) { p=""; }
					row[resultp[s]] = p;
				}
				rows[i] = row;
			}
		}
		api->db->FreeTable(resultp);
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}

class VS_Var {
public:
	VS_Var() {
		type = 0;
		flags = 0;
	}
	string added_by;
	string var;
	string value;
	string orig_value;
	int type;
	uint32 flags;
};

VS_Var GetVar(const char * var) {
	std::stringstream sstr;
	char * v = sqlite_escape(var);
	sstr << "SELECT * FROM varsetter WHERE `Var`='" << v << "'";
	zfree(v);
	sql_rows res = GetTable(sstr.str().c_str(), NULL);
	VS_Var ret;
	if (res.size() > 0) {
		sql_row r = res[0];
		ret.added_by = r["Nick"];
		ret.var = r["Var"];
		ret.value = r["Value"];
		ret.orig_value = r["OriginalValue"];
		ret.flags = atoul(r["FlagsNeeded"].c_str());
		ret.type = atoi(r["Type"].c_str());
	}
	return ret;
}

void AddVar(const char * nick, int type, uint32 flags, const char * var, const char * value) {
	std::stringstream sstr;
	char * n = sqlite_escape(nick);
	char * v = sqlite_escape(var);
	char * l = sqlite_escape(value);
	sstr << "REPLACE INTO varsetter (`Nick`,`Type`,`FlagsNeeded`,`Var`,`Value`,`OriginalValue`) VALUES ('" << n << "', " << type << ", " << flags << ", '" << v << "', '" << l << "', '" << l << "')";
	zfree(l);
	zfree(v);
	zfree(n);

	api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
	api->SetCustomVar(var, value);
	//bSend(GetConfig()->ircnets[netno].sock, (char *)sstr.str().c_str());
}

void DeleteVar(const char * var) {
	std::stringstream sstr;
	char * v = sqlite_escape(var);
	sstr << "DELETE FROM varsetter WHERE `Var`='" << v << "'";
	zfree(v);
	api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
	api->DeleteCustomVar(var);
}

void UpdateVar(const char * var, const char * value) {
	std::stringstream sstr;
	char * v = sqlite_escape(var);
	if (value == NULL) {
		sstr << "UPDATE varsetter SET `Value`=`OriginalValue` WHERE `Var`='" << v << "')";
	} else {
		char * l = sqlite_escape(value);
		sstr << "UPDATE varsetter SET `Value`='" << l << "' WHERE `Var`='" << v << "')";
		zfree(l);
	}
	zfree(v);
	api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
	api->SetCustomVar(var, value);
}

int VarSetter_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];
	if (!stricmp(command, "varadd") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %cvaradd %%variable +flags_needed_to_modify [initial value]"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "varadd")) {
		StrTokenizer st((char *)parms, ' ', true);
		if (st.NumTok() >= 2) {
			char * var = st.GetSingleTok(1);
			char * sflags = st.GetSingleTok(2);
			uint32 flags = api->user->string_to_uflag(sflags, 0);
			string value;
			if (st.NumTok() >= 3) {
				value = st.stdGetTok(3, st.NumTok());
			}
			DeleteVar(var);
			AddVar(ut->User ? ut->User->Nick:ut->Nick, 0, flags, var, value.c_str());
			ut->std_reply(ut, _("Variable added!"));
			st.FreeString(sflags);
			st.FreeString(var);
		} else {
			snprintf(buf, sizeof(buf), _("Usage: %cvaradd %%variable +flags_needed_to_modify [initial value]"), api->commands->PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	if (!stricmp(command, "vardel") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %cvardel %%variable"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "vardel")) {
		char * tmp = zstrdup(parms);
		char * p2 = NULL;
		char * p = strtok_r(tmp, " ", &p2);
		DeleteVar(p);
		zfree(tmp);
		ut->std_reply(ut, _("Variable deleted!"));
		return 1;
	}

	if (!stricmp(command, "varmod") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %cvarmod %%variable reset OR new value"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "varmod")) {
		StrTokenizer st((char *)parms, ' ', true);
		if (st.NumTok() >= 2) {
			char * var = st.GetSingleTok(1);
			char * val = st.GetTok(2, st.NumTok());
			VS_Var v = GetVar(var);
			if (v.var.length()) {
				if (v.flags == 0 || (v.flags & ut->Flags) != 0) {
					UpdateVar(var, stricmp(val, "reset") ? val:NULL);
					ut->std_reply(ut, _("Variable updated!"));
				} else {
					ut->std_reply(ut, _("You do not have permissions to update that variable!"));
				}
			} else {
				ut->std_reply(ut, _("That variable does not exist!"));
			}
			st.FreeString(val);
			st.FreeString(var);
		} else {
			snprintf(buf, sizeof(buf), _("Usage: %cvarmod %%variable reset OR new value"), api->commands->PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	return 0;
}

void LoadPlugin() {
	sql_rows res = GetTable("SELECT * FROM varsetter", NULL);
	for (sql_rows::iterator x = res.begin(); x != res.end(); x++) {
		api->SetCustomVar(x->second["Var"].c_str(), x->second["Value"].c_str());
	}

	/*
	DS_CONFIG_SECTION * rSec = api->config->GetConfigSection(NULL, "VarSetter");
	char buf[256];
	for (int i=0; i < 16; i++) {
		sprintf(buf, "Command%d", i);
		DS_CONFIG_SECTION * sec = api->config->GetConfigSection(rSec, buf);
		if (sec) {
			string cmd, var, type = "always";
			uint32 flags = 0;
			char * p = api->config->GetConfigSectionValue(sec, "Command");
			if (p) {
				cmd = p;
			}
			p = api->config->GetConfigSectionValue(sec, "Variable");
			if (p) {
				var = p;
			}
			p = api->config->GetConfigSectionValue(sec, "Type");
			if (p) {
				type = p;
			}
			p = api->config->GetConfigSectionValue(sec, "Flags");
			if (p) {
				flags = api->user->string_to_uflag(p, 0);
			}
			if (cmd.length() && var.length()) {

			}
		}
	}
	*/
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_OP|UFLAG_MASTER, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "varadd",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, VarSetter_Commands, _("This will let you add a custom %variable"));
	api->commands->RegisterCommand_Proc(pluginnum, "vardel",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, VarSetter_Commands, _("This will let you delete a custom %variable"));
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "varmod",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, VarSetter_Commands, _("This will let you modify a custom %variable"));
}

void quit() {
	api->commands->UnregisterCommandsByPlugin(pluginnum);
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
		case IB_PROCTEXT:{
				USER_PRESENCE * ui = (USER_PRESENCE *)data;
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{F909D7EC-92F3-4E56-BB08-778BB7CBAC3F}",
	"VarSetter",
	"Variable Setting Plugin 1.0",
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
