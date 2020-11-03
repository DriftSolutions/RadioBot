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
//#include <boost/shared_ptr.hpp>

BOTAPI_DEF * api = NULL;
int pluginnum = 0;

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

sql_rows GetTable(const char * query, char **errmsg=NULL) {
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

uint32 AddQuote(const char * from, const char * content) {
	std::stringstream sstr;
	uint32 ret = 0;
	char * sfrom = sqlite_escape(from);
	char * con = sqlite_escape(content);
	sstr << "INSERT INTO quotes (`From`,`Content`,`TimeStamp`) VALUES ('" << sfrom << "', '" << con << "', " << time(NULL) << ")";
	api->db->Lock();
	if (api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL) == SQLITE_OK) {
		ret = api->db->InsertID();
	}
	api->db->Release();
	zfree(con);
	zfree(sfrom);
	return ret;
}

bool DelQuote(uint32 id) {
	std::stringstream sstr;
	sstr << "DELETE FROM quotes WHERE ID=" << id;
	if (api->db->Query(sstr.str().c_str(), NULL, NULL, NULL) == SQLITE_OK) {
		return true;
	}
	return false;
}

int64 NumQuotes() {
	std::stringstream sstr,sstr2;
	sstr << "SELECT COUNT(*) AS Count FROM quotes";
	sql_rows res = GetTable(sstr.str().c_str());
	if (res.size() > 0) {
		return atoi64(res.begin()->second["Count"].c_str());
	}
	return 0;
}

string GetQuote(uint32 id) {
	std::stringstream sstr,sstr2;
	sstr << "SELECT Content FROM quotes WHERE ID=" << id;
	sql_rows res = GetTable(sstr.str().c_str());
	if (res.size() > 0) {
		sstr2 << "UPDATE quotes SET LastView=" << time(NULL) << ", ViewCount=ViewCount+1 WHERE ID=" << id;
		api->db->Query(sstr2.str().c_str(), NULL, NULL, NULL);
		return res.begin()->second["Content"].c_str();
	}
	return "";
}

class RandomQuote {
	public:
		RandomQuote() { num = 0; }
		int32 num;
		string str;
};

RandomQuote GetRandomQuote() {
	std::stringstream sstr,sstr2;
	RandomQuote rq;
	int32 num = NumQuotes();
	sstr << "SELECT ID,Content FROM quotes LIMIT " << api->genrand_int32()%num << ", 1";
	sql_rows res = GetTable(sstr.str().c_str());
	if (res.size() > 0) {
		sstr2 << "UPDATE quotes SET LastView=" << time(NULL) << ", ViewCount=ViewCount+1 WHERE ID=" << atoul(res.begin()->second["ID"].c_str());
		api->db->Query(sstr2.str().c_str(), NULL, NULL, NULL);
		rq.num = atoul(res.begin()->second["ID"].c_str());
		rq.str = res.begin()->second["Content"];
	}
	return rq;
}

uint32 FindQuote(string str) {
	std::stringstream sstr,sstr2;
	char * con = sqlite_escape(str.c_str());
	sstr << "SELECT ID FROM quotes WHERE Content='" << con << "'";
	sql_rows res = GetTable(sstr.str().c_str());
	uint32 ret = 0;
	if (res.size() > 0) {
		ret = atoul(res.begin()->second["ID"].c_str());
	}
	zfree(con);
	return ret;
}

int Quotes_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];
	if (!stricmp(command, "sign") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %csign text"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "sign")) {
		uint32 id = FindQuote(parms);
		if (id == 0) {
			id = AddQuote(ut->User ? ut->User->Nick : ut->Nick, parms);
			if (id != 0) {
				snprintf(buf, sizeof(buf), _("Quote added with number %u. Use !q %u to view it."), id, id);
				ut->send_chan(ut, buf);
			} else {
				ut->std_reply(ut, _("Error adding that quote to the database!"));
			}
		} else {
			snprintf(buf, sizeof(buf), _("That quote already exists in the database as number %u. Use !q %u to view it."), id, id);
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	if (!stricmp(command, "q")) {
		uint32 id = 0;
		if (parms != NULL && strlen(parms)) {
			id = atoul(parms);
		}
		if (id != 0) {
			string ret = GetQuote(id);
			if (ret.length()) {
				stringstream sstr;
				sstr << "[" << id << "] " << ret;
				ut->send_chan(ut, sstr.str().c_str());
			} else {
				ut->std_reply(ut, _("Invalid quote number!"));
			}
		} else {
			RandomQuote ret = GetRandomQuote();
			if (ret.str.length()) {
				stringstream sstr;
				sstr << "[" << ret.num << "] " << ret.str;
				ut->send_chan(ut, sstr.str().c_str());
			} else {
				ut->std_reply(ut, _("No quotes in the database!"));
			}
		}
		return 1;
	}

	if (!stricmp(command, "quote-del") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %cquote-del number"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "quote-del")) {
		uint32 id = atoul(parms);
		if (id != 0) {
			if (DelQuote(id)) {
				ut->std_reply(ut, _("Quote deleted."));
			} else {
				ut->std_reply(ut, _("Error deleting quote, check bot console for details."));
			}
		} else {
			ut->std_reply(ut, _("Invalid quote number!"));
		}
		return 1;
	}

	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = (BOTAPI_DEF *)BotAPI;
	pluginnum = num;
	api->db->Query("CREATE TABLE IF NOT EXISTS quotes(ID INTEGER PRIMARY KEY AUTOINCREMENT, `From` VARCHAR(255), `Content` TEXT, TimeStamp INTEGER, LastView INTEGER DEFAULT 0, ViewCount INTEGER DEFAULT 0);", NULL, NULL, NULL);

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "sign",	&acl, CM_ALLOW_ALL, Quotes_Commands, _("This will add a quote/signature to the database"));
	api->commands->RegisterCommand_Proc(pluginnum, "q",	&acl, CM_ALLOW_ALL, Quotes_Commands, _("This will let you view a quote/signature"));

	api->commands->FillACL(&acl, 0, UFLAG_OP|UFLAG_MASTER, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "quote-del", &acl, CM_ALLOW_ALL_PRIVATE, Quotes_Commands, _("This will delete a saved quote"));
	return 1;
}

void quit() {
	api->commands->UnregisterCommandsByPlugin(pluginnum);
}

int message(unsigned int msg, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{1AA8F3F3-3AB7-4CAB-B074-56EE8A6F4F81}",
	"Quotes/Signatures",
	"Quotes/Signatures Plugin 1.0",
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
