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

//#include <drift/dsl.h>
#include "sam.h"
#include <assert.h>

BOTAPI_DEF * api=NULL;
int pluginnum=-1;

SAM_CONFIG config;
DB_MySQL conx;

bool MatchesNoRequestFilter(const char * str) {
	StrTokenizer * st = new StrTokenizer(config.NoRequestFilters, ',');
	int num = st->NumTok();
	for (int i=1; i <= num; i++) {
		char * pat = st->GetSingleTok(i);
		if (strlen(pat)) {
			if (wildicmp(pat, str)) {
				st->FreeString(pat);
				delete st;
				return true;
				break;
			}
		}
		st->FreeString(pat);
	}
	delete st;
	return false;
}

time_t GetPreviousRatingTime(int id) {
	time_t ret = 0;
	std::stringstream sstr;
	sstr << "SELECT shoutirc_lastrating FROM songlist WHERE ID=" << id;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		SC_Row row;
		if (conx.FetchRow(res, row)) {
			ret = atoi64(row.Get("shoutirc_lastrating").c_str());
		}
	}
	conx.FreeResult(res);
	return ret;
}

void DoRating(int id, int rating) {
	if (rating < 1 || rating > api->ss->GetMaxRating() || rating > 10) {
		return;
	}
	if (config.AdjustWeightOnRating[rating-1] == 0) {
		return;
	}

	if (config.AdjustWeightOnRatingTime) {
		time_t x = GetPreviousRatingTime(id);
		if (time(NULL) < (x+config.AdjustWeightOnRatingTime)) {
			api->ib_printf("SAM -> Skipping weight adjustment for song %d, was already adjusted recently...\n", id);
			return;
		}
	}

	api->ib_printf("SAM -> Adjusting weight for song %d by %d\n", id, config.AdjustWeightOnRating[rating-1]);
	AdjustWeight(id, config.AdjustWeightOnRating[rating-1]);

	std::stringstream sstr;
	sstr << "UPDATE songlist SET shoutirc_lastrating=" << time(NULL) << " WHERE ID=" << id;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res) { conx.FreeResult(res); }
}

void AdjustWeight(int id, int amount) {
	if (amount != 0) {
		std::stringstream sstr;
		sstr << "UPDATE songlist SET weight=weight";
		if (amount > 0) {
			sstr << "+" << amount;
		} else {
			sstr << amount;
		}
		sstr << " WHERE ID=" << id;
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (res) { conx.FreeResult(res); }
	}
}

int GetPlayingSongID() {
	std::stringstream sstr;
	sstr << "SELECT * FROM sam_shoutirc WHERE `Name`='CurSongID'";
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	int ret = 0;
	if (res && conx.NumRows(res) > 0) {
		SC_Row row;
		if (conx.FetchRow(res, row)) {
			ret = atoi(row.Get("Value").c_str());
		}
	}
	conx.FreeResult(res);
	return ret;
}

SAM_SONG * GetRowToSong(MYSQL_RES * res) {
	if (!res) { return NULL; }
	SAM_SONG * ret = NULL;
	SC_Row row;
	if (conx.FetchRow(res, row)) {
		ret = (SAM_SONG *)zmalloc(sizeof(SAM_SONG));
		memset(ret, 0, sizeof(SAM_SONG));
		ret->fn = zstrdup(row.Get("filename").c_str());
		const char * p = row.Get("artist").c_str();
		if (p && p[0]) {
			ret->artist = zstrdup(p);
		}
		p = row.Get("title").c_str();
		if (p && p[0]) {
			ret->title = zstrdup(p);
		}
		ret->ID = atoi(row.Get("ID").c_str());
		ret->duration = atoul(row.Get("duration").c_str());
	}
	return ret;
}

SAM_SONG * GetSongFromID(int32 id) {
	std::stringstream sstr;
	sstr << "SELECT * FROM songlist WHERE ID=" << id;
	SAM_SONG * ret = NULL;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		ret = GetRowToSong(res);
	}
	conx.FreeResult(res);
	return ret;
}

double GetNextSortID() {
	std::stringstream sstr;
	sstr << "SELECT sortID FROM queuelist WHERE shoutirc_req>0 ORDER BY shoutirc_req DESC LIMIT 1";
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	double ret = 2000;
	bool done = false;
	if (res && conx.NumRows(res) > 0) {
		SC_Row row;
		if (conx.FetchRow(res, row)) {
			const char * p = row.Get("sortID").c_str();
			if (p && *p) {
				ret = atof(p)+1;
				done = true;
			}
		}
	}
	conx.FreeResult(res);

	if (!done) {
		std::stringstream sstr2;
		sstr2 << "SELECT sortID FROM queuelist ORDER BY sortID ASC LIMIT 1";
		res = conx.Query(sstr2.str().c_str());
		if (res && conx.NumRows(res) > 0) {
			SC_Row row;
			if (conx.FetchRow(res, row)) {
				const char * p = row.Get("sortID").c_str();
				if (p && *p) {
					ret = atof(p)-2000;
				}
			}
		}
		conx.FreeResult(res);
	}
	return ret;
}

void FreeSong(SAM_SONG * Scan) {
	if (!Scan) { return; }
	zfreenn(Scan->fn);
	zfreenn(Scan->artist);
	zfreenn(Scan->title);
	zfree(Scan);
}

SONG_FIND_RESULT * FindWild(char * pat) {
	SONG_FIND_RESULT * ret = (SONG_FIND_RESULT *)zmalloc(sizeof(SONG_FIND_RESULT));
	memset(ret,0,sizeof(SONG_FIND_RESULT));

	std::stringstream sstr;
	char * pat2 = strdup(conx.EscapeString(pat).c_str());
	str_replace(pat2, strlen(pat2)+1, "*", "%");
	sstr << "SELECT * FROM songlist WHERE ";
	switch (config.WildMode) {
		case 1:
			sstr << "CONCAT_WS(' ',artist,title) LIKE \"" << pat2 << "\"";
			break;
		case 2:
			sstr << "CONCAT_WS(' ',artist,title) LIKE \"" << pat2 << "\" OR SUBSTRING_INDEX(filename, '\\\\', -1) LIKE \"" << pat2 << "\"";
			break;
		default:
			sstr << "SUBSTRING_INDEX(filename, '\\\\', -1) LIKE \"" << pat2 << "\"";
			break;
	}
	free(pat2);
	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		SAM_SONG * Scan=NULL;
		while ((Scan = GetRowToSong(res)) != NULL) {
			if (!MatchesNoRequestFilter(Scan->fn)) {
				if (ret->num == MAX_RESULTS) {
					ret->not_enough = true;
					FreeSong(Scan);
					break;
				}
				ret->results[ret->num] = Scan;
				ret->num++;
			} else {
				FreeSong(Scan);
			}
		}
	}
	conx.FreeResult(res);
	return ret;
}

void FreeFindResult(SONG_FIND_RESULT * qRes) {
	for (int i=0; i < qRes->num; i++) {
		FreeSong(qRes->results[i]);
	}
	zfree(qRes);
}

SAM_SONG * FindFile(const char * fn) {
	SAM_SONG * ret = NULL;

	std::stringstream sstr;
	char * pat2 = zstrdup(conx.EscapeString(fn).c_str());
	sstr << "SELECT * FROM songlist WHERE SUBSTRING_INDEX(filename, '\\\\', -1) LIKE '" << pat2 << "'";
	zfree(pat2);
	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		ret = GetRowToSong(res);
	}
	conx.FreeResult(res);
	if (ret && MatchesNoRequestFilter(ret->fn)) {
		FreeSong(ret);
		ret = NULL;
	}
	return ret;
}

class REQUSERDATA {
public:
	REQUSERDATA() { tme = 0; trycnt=0; }
	time_t tme;
	int trycnt;
};
typedef std::map<int32, time_t> RequestListType;
class BLOCKUSERDATA {
public:
	BLOCKUSERDATA() { last_time = 0; }
	time_t last_time;
	RequestListType songs;
};
typedef std::map<string, REQUSERDATA> RequestUserType;
typedef std::map<string, BLOCKUSERDATA> BlockedUserType;
RequestListType requestList;
RequestUserType reqUserList;
BlockedUserType blockedUserList;
Titus_Mutex reqMutex;

bool AllowRequest(SAM_SONG * Scan, USER_PRESENCE * ut, char * buf, int bufSize) {
	reqMutex.Lock();

	if (config.MinReqTimePerSong) {
		RequestListType::iterator s = requestList.begin();
		while (s != requestList.end()) {
			time_t tmp = s->second;
			if (tmp <= (time(NULL)-config.MinReqTimePerSong)) {
				requestList.erase(s);
				s = requestList.begin();
			} else {
				s++;
			}
		}

		RequestListType::const_iterator x = requestList.find(Scan->ID);
		if (x != requestList.end()) {
			reqMutex.Release();
			strlcpy(buf, _("Sorry, that song was already requested recently..."), bufSize);
			return false;
		}
	}

	const char * nick = ut->User ? ut->User->Nick:ut->Nick;
	if (config.MinReqTimePerUser) {
		RequestUserType::iterator t = reqUserList.begin();
		while (t != reqUserList.end()) {
			if (t->second.tme <= (time(NULL)-config.MinReqTimePerUser)) {
				reqUserList.erase(t);
				t = reqUserList.begin();
			} else {
				t++;
			}
		}
		RequestUserType::iterator x = reqUserList.find(nick);
		if (x != reqUserList.end()) {
			x->second.trycnt++;
			if (config.MinReqTimePerUserKick > 0 && x->second.trycnt >= config.MinReqTimePerUserKick && !stricmp(ut->Desc,"IRC") && ut->NetNo >= 0 && ut->Channel) {
				config.ca.add_ban(ut->NetNo, ut->Channel, ut->Hostmask, api->irc->GetCurrentNick(ut->NetNo), time(NULL) + config.MinReqTimePerUserBanTime, "Requesting too many times after warned");
			}
			reqMutex.Release();
			snprintf(buf, bufSize, _("You can only request a song once every %d seconds..."), config.MinReqTimePerUser);
			return false;
		}
	}

	if (config.NoRequestFilters[0]) {
		BlockedUserType::iterator t = blockedUserList.begin();
		while (t != blockedUserList.end()) {
			if (t->second.last_time <= (time(NULL)-3600)) {
				blockedUserList.erase(t);
				t = blockedUserList.begin();
			} else {
				t++;
			}
		}
	}

	if (MatchesNoRequestFilter(Scan->fn)) {
		strlcpy(buf, _("The DJ has blocked that song from being requested!"), bufSize);

		blockedUserList[nick].last_time = time(NULL);
		blockedUserList[nick].songs[Scan->ID]++;

		BlockedUserType::iterator x = blockedUserList.find(nick);
		if (x != blockedUserList.end()) {
			RequestListType::iterator y = x->second.songs.find(Scan->ID);
			if (y != x->second.songs.end()) {
				if (config.NoReqFiltersKick > 0 && y->second >= config.NoReqFiltersKick && !stricmp(ut->Desc,"IRC") && ut->NetNo >= 0 && ut->Channel) {
					config.ca.add_ban(ut->NetNo, ut->Channel, ut->Hostmask, api->irc->GetCurrentNick(ut->NetNo), time(NULL) + config.NoReqFiltersBanTime, "Abusing Blocked Songs");
				}
			}
		}
		reqMutex.Release();
		return false;
	}

	if (config.MinReqTimePerSong) { requestList[Scan->ID] = time(NULL); }
	if (config.MinReqTimePerUser) {
		reqUserList[nick].tme = time(NULL);
		reqUserList[nick].trycnt = 0;
	}
	reqMutex.Release();
	return true;
}

/*
int SAM_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	return 0;
}
*/

bool LoadConfig() {
	memset(&config, 0, sizeof(config));

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "SAM");
	if (sec == NULL) {
		api->ib_printf(_("SAM -> No 'SAM' section found in your config!\n"));
		return false;
	}

	config.EnableRequests = api->config->GetConfigSectionLong(sec, "EnableRequests") != 0 ? true:false;
	config.EnableFind = api->config->GetConfigSectionLong(sec, "EnableFind") != 0 ? true:false;
	config.EnableYP = api->config->GetConfigSectionLong(sec, "EnableYP") != 0 ? true:false;
	config.AuxData = api->config->GetConfigSectionLong(sec, "AuxData") != 0 ? true:false;
	config.SAM_Port = api->config->GetConfigSectionLong(sec, "SAM_Port") > 0 ? api->config->GetConfigSectionLong(sec, "SAM_Port"):1221;
	config.MinReqTimePerSong = api->config->GetConfigSectionLong(sec, "MinReqTimePerSong") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerSong"):0;
	config.MinReqTimePerUser = api->config->GetConfigSectionLong(sec, "MinReqTimePerUser") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerUser"):0;
	config.MinReqTimePerUserKick = api->config->GetConfigSectionLong(sec, "MinReqTimePerUserKick") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerUserKick"):0;
	config.MinReqTimePerUserBanTime = api->config->GetConfigSectionLong(sec, "MinReqTimePerUserBanTime") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerUserBanTime"):300;
	config.NoReqFiltersKick = api->config->GetConfigSectionLong(sec, "NoRequestFiltersKick") > 0 ? api->config->GetConfigSectionLong(sec, "NoRequestFiltersKick"):0;
	config.NoReqFiltersBanTime = api->config->GetConfigSectionLong(sec, "NoReqFiltersBanTime") > 0 ? api->config->GetConfigSectionLong(sec, "NoReqFiltersBanTime"):300;
	config.MaxNextResults = api->config->GetConfigSectionLong(sec, "MaxNextResults") > 0 ? api->config->GetConfigSectionLong(sec, "MaxNextResults"):5;
	config.AdjustWeightOnRatingTime = api->config->GetConfigSectionLong(sec, "AdjustWeightOnRatingTime") >= 0 ? api->config->GetConfigSectionLong(sec, "AdjustWeightOnRatingTime"):86400;
	if (api->config->IsConfigSectionValue(sec, "AdjustWeight")) {
		config.AdjustWeight = api->config->GetConfigSectionLong(sec, "AdjustWeight");
	}
	long x = api->config->GetConfigSectionLong(sec, "WildMode");
	if (x >= 0 && x <= 2) {
		config.WildMode = x;
	}
	char buf[40];
	for (int i=0; i < 10; i++) {
		sprintf(buf, "AdjustWeightOnRating%d", i+1);
		if (api->config->IsConfigSectionValue(sec, buf)) {
			config.AdjustWeightOnRating[i] = api->config->GetConfigSectionLong(sec, buf);
		}
	}

	if ((config.MinReqTimePerUserKick || config.NoReqFiltersKick) && api->SendMessage(-1, CHANADMIN_IS_LOADED, NULL, 0)) {
		api->SendMessage(-1, CHANADMIN_GET_INTERFACE, (char *)&config.ca, sizeof(config.ca));
	} else {
		api->ib_printf(_("SAM -> The ChanAdmin plugin must be loaded to use MinReqTimePerUserKick and/or NoRequestFiltersKick, disabling...\n"));
		config.MinReqTimePerUserKick = 0;
		config.NoReqFiltersKick = 0;
	}

	api->config->GetConfigSectionValueBuf(sec, "MatchDJ", config.MatchDJ, sizeof(config.MatchDJ));
	if (!strlen(config.MatchDJ)) {
		api->ib_printf(_("SAM -> You must set MatchDJ in your SAM section! How else will I know if SAM is playing???\n"));
		return false;
	}
	sstrcpy(config.SAM_Host, "localhost");
	api->config->GetConfigSectionValueBuf(sec, "SAM_Host", config.SAM_Host, sizeof(config.SAM_Host));
	api->config->GetConfigSectionValueBuf(sec, "NoRequestFilters", config.NoRequestFilters, sizeof(config.NoRequestFilters));
	api->config->GetConfigSectionValueBuf(sec, "YPGenre", config.yp_genre, sizeof(config.yp_genre));

	strcpy(config.mysql_host, "localhost");
	strcpy(config.mysql_user, "root");
	strcpy(config.mysql_db, "samdb");

	DS_CONFIG_SECTION * ssec = api->config->GetConfigSection(sec, "MySQL");
	if (sec) {
		api->config->GetConfigSectionValueBuf(ssec, "Host", config.mysql_host, sizeof(config.mysql_host));
		api->config->GetConfigSectionValueBuf(ssec, "User", config.mysql_user, sizeof(config.mysql_user));
		api->config->GetConfigSectionValueBuf(ssec, "Pass", config.mysql_pass, sizeof(config.mysql_pass));
		api->config->GetConfigSectionValueBuf(ssec, "DBName", config.mysql_db, sizeof(config.mysql_db));
		if (api->config->GetConfigSectionLong(ssec, "Port") > 0) {
			config.mysql_port = api->config->GetConfigSectionLong(ssec, "Port");
		}
	} else {
		api->ib_printf(_("SAM -> No 'MySQL' section found in your SAM section, using pure defaults\n"));
	}

	return true;
}

/*
int sam_mysql_printf(const char * fmt, ...) {
	api->ib_printf2(pluginnum, fmt, __VA_ARGS__);
}
*/

int SAM_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024],buf2[32];
	if (!stricmp(command, "next") || !stricmp(command, "sam-next")) {
		if (config.playing) {
			std::stringstream sstr;
			sstr << "SELECT * FROM queuelist ORDER BY sortID ASC LIMIT " << config.MaxNextResults;
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			if (res && conx.NumRows(res) > 0) {
				if (!api->LoadMessage("SAM_ShowQueue", buf, sizeof(buf))) {
					sstrcpy(buf, "Here are the next %num songs in queue...");
				}
				sprintf(buf2, U64FMT, conx.NumRows(res));
				str_replace(buf, sizeof(buf), "%num", buf2);
				api->ProcText(buf, sizeof(buf));
				ut->std_reply(ut, buf);

				SC_Row row;
				int num = 0;
				while (conx.FetchRow(res, row)) {
					num++;
					SAM_SONG * s = GetSongFromID(strtol(row.Get("songID").c_str(), NULL, 10));
					if (s) {
						stringstream title;
						if (s->artist && s->title) {
							title << s->artist << " - " << s->title;
						} else if (s->title) {
							title << s->title;
						} else {
							title << nopath(s->fn);
						}
						snprintf(buf, sizeof(buf), "%d. %s", num, title.str().c_str());
						ut->std_reply(ut, buf);
						FreeSong(s);
					}
				}

			} else {
				if (!api->LoadMessage("SAM_EmptyQueue", buf, sizeof(buf))) {
					sstrcpy(buf, "The queue is currently empty...");
				}
				api->ProcText(buf, sizeof(buf));
				ut->std_reply(ut, buf);
			}
			conx.FreeResult(res);
			return 1;
		}
	}

	if (!stricmp(command, "sam-stop")) {
		ut->std_reply(ut, _("Telling SAM to stop streaming..."));
		api->SendMessage(pluginnum, SOURCE_C_STOP, NULL, 0);
		return 1;
	}

	if (!stricmp(command, "sam-play")) {
		ut->std_reply(ut, _("Telling SAM to start playback..."));
		api->SendMessage(pluginnum, SOURCE_C_PLAY, NULL, 0);
		return 1;
	}

	if (!stricmp(command, "sam-skip")) {
		if (config.playing) {
			ut->std_reply(ut, _("Telling SAM to skip to the next song..."));
			api->SendMessage(pluginnum, SOURCE_C_NEXT, NULL, 0);
		} else {
			ut->std_reply(ut, _("SAM is not playing right now!"));
		}
		return 1;
	}

	return 0;
}

int init(int num, BOTAPI_DEF * pApi) {
	pluginnum = num;
	api = (BOTAPI_DEF *)pApi;

	if (api->ss == NULL || api->irc == NULL) {
		api->ib_printf2(pluginnum,_("SAM -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	if (!LoadConfig()) {
		return 0;
	}

	bool connected = false;
	for (int tries=0; tries < 5; tries++) {
		api->ib_printf(_("SAM -> Connecting to MySQL server...\n"));
		if (conx.Connect(config.mysql_host, config.mysql_user, config.mysql_pass, config.mysql_db, config.mysql_port)) {
			connected = true;
			break;
		} else {
			api->ib_printf(_("SAM -> Waiting 2 seconds for another attempt...\n"));
			api->safe_sleep_seconds(2);
		}
	}
	if (!connected) { return 0; }

	{
		std::stringstream sstr, sstr2;
		sstr << "SHOW COLUMNS IN `queuelist` LIKE \"shoutirc_req\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `queuelist` ADD `shoutirc_req` INT NOT NULL DEFAULT '0'";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr, sstr2;
		sstr << "SHOW COLUMNS IN `songlist` LIKE \"shoutirc_lastrating\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `songlist` ADD `shoutirc_lastrating` INT NOT NULL DEFAULT '0'";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}

	conx.FreeResult(conx.Query("CREATE TABLE IF NOT EXISTS sam_shoutirc (`Name` VARCHAR(255) NOT NULL DEFAULT '' PRIMARY KEY, `Value` VARCHAR(255) NOT NULL DEFAULT '');"));

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "next",		&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC,		SAM_Commands, "This will show you the next songs in SAM's queue.");
	api->commands->RegisterCommand_Proc(pluginnum, "sam-next",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,	SAM_Commands, "This will show you the next songs in SAM's queue.");

	api->commands->FillACL(&acl, UFLAG_ADVANCED_SOURCE, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "sam-stop",	&acl, CM_ALLOW_ALL_PRIVATE|CM_FLAG_ASYNC,	SAM_Commands, "This will tell SAM to stop streaming.");
	api->commands->RegisterCommand_Proc(pluginnum, "sam-skip",	&acl, CM_ALLOW_ALL_PRIVATE|CM_FLAG_ASYNC,	SAM_Commands, "This will tell SAM to skip to the next song.");

	api->commands->FillACL(&acl, UFLAG_BASIC_SOURCE, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "sam-play",	&acl, CM_ALLOW_ALL_PRIVATE|CM_FLAG_ASYNC,	SAM_Commands, "This will tell SAM to start streaming.");

	return 1;
}

void quit() {
	api->commands->UnregisterCommandsByPlugin(pluginnum);

	api->ib_printf(_("SAM -> Dropping database connection...\n"));
	conx.Disconnect();
}

void HandleYP(STATS * s) {
	if (config.EnableYP && config.playing && (time(NULL) - config.yp_last_try) >= 1) {
		if (config.yp_handle.yp_id) {
			if (api->yp->TouchYP(&config.yp_handle, s->cursong) == -2) {
				memset(&config.yp_handle, 0, sizeof(config.yp_handle));
			}
		} else {
			API_SS sc;
			api->ss->GetSSInfo(0,&sc);
			YP_CREATE_INFO yp_info;
			memset(&yp_info, 0, sizeof(yp_info));
			yp_info.cur_playing = s->cursong;
			yp_info.genre = config.yp_genre;
			yp_info.mime_type = "";
			yp_info.source_name = "SAM";
			if (!api->yp->AddToYP(&config.yp_handle, &sc, &yp_info)) {
				memset(&config.yp_handle, 0, sizeof(config.yp_handle));
			}
		}
		config.yp_last_try = time(NULL);
	}
}

#if (LIBCURL_VERSION_MAJOR < 7) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 17)
#define OLD_CURL 1
#else
#undef OLD_CURL
#endif

struct SAM_BUFFER {
	char * data;
	uint32 len;
};

size_t sam_buffer_write(void *ptr, size_t size, size_t count, void *stream) {
	SAM_BUFFER * buf = (SAM_BUFFER *)stream;
	uint32 len = size * count;
	if (len > 0) {
		buf->data = (char *)zrealloc(buf->data, buf->len + len + 1);
		memcpy(buf->data+buf->len, ptr, len);
		buf->len += len;
		buf->data[buf->len] = 0;
	}
	return count;
}

CURL * sam_prepare_handle(SAM_BUFFER * buf) {
	CURL * h = api->curl->easy_init();
	if (h == NULL) { return NULL; }
	api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 30);
	api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 20);
	api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
	api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1 );
	api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, sam_buffer_write);
	api->curl->easy_setopt(h, CURLOPT_WRITEDATA, buf);
	char buf2[128];
	sprintf(buf2, "RadioBot SAM Support - www.shoutirc.com (Mozilla)");
	api->curl->easy_setopt(h, CURLOPT_USERAGENT, buf2);
	return h;
}

int MessageHandler(unsigned int msg, char * data, int datalen) {
	switch (msg) {
		case IB_SS_DRAGCOMPLETE:{
				STATS * s = api->ss->GetStreamInfo();
				if (s->online && stristr(s->curdj, config.MatchDJ)) {
					config.playing = true;
					if (!config.lastPlaying) {
						api->ib_printf(_("SAM -> SAM detected, enabling requests...\n"));
					}
					api->ss->EnableRequestsHooked(&sam_req_iface);
					if (s->title_changed || (config.yp_handle.yp_id && (time(NULL) - config.yp_last_try) >= 300)) {
						HandleYP(s);
					}
				} else {
					config.playing = false;
					if (config.lastPlaying) {
						api->ib_printf(_("SAM -> SAM no longer detected, disabling requests...\n"));
						api->ss->EnableRequests(false);
					}
					if (config.yp_handle.yp_id) {
						api->yp->DelFromYP(&config.yp_handle);
						memset(&config.yp_handle, 0, sizeof(config.yp_handle));
					}
				}
				config.lastPlaying = config.playing;
			}
			break;

		case IB_ON_SONG_RATING:{
			if (config.playing) {
				IBM_RATING * ir = (IBM_RATING *)data;
				if (ir->song && ir->song[0]) {
					int id = GetPlayingSongID();
					if (id != 0) {
						if (ir->rating >= 1 && ir->rating <= 5) {
							DoRating(id, ir->rating);
						}
					}
				}
			}
			break;
		}

		case SOURCE_C_PLAY:{
			ostringstream url;
			url << "http://" << config.SAM_Host << ":" << config.SAM_Port << "/event/ib_play";

			SAM_BUFFER cvbuf;
			memset(&cvbuf, 0, sizeof(cvbuf));
			CURL * h = sam_prepare_handle(&cvbuf);
			if (h) {
				api->curl->easy_setopt(h, CURLOPT_URL, url.str().c_str());
				CURLcode code = api->curl->easy_perform(h);
				if (code == CURLE_OK) {
					if (cvbuf.data && cvbuf.len > 0) {
						api->ib_printf("SAM -> Successfully told SAM to start playback...\n");
					} else {
						api->ib_printf("SAM -> No reply from SAM!\n");
					}
				} else {
					api->ib_printf("SAM -> Error talking to SAM: %s!\n", api->curl->easy_strerror(code));
				}
				api->curl->easy_cleanup(h);
			} else {
				api->ib_printf("SAM -> Error creating CURL handle!\n");
			}
			zfreenn(cvbuf.data);
			break;
		}

		case SOURCE_C_STOP:{
			ostringstream url;
			url << "http://" << config.SAM_Host << ":" << config.SAM_Port << "/event/ib_stop";

			SAM_BUFFER cvbuf;
			memset(&cvbuf, 0, sizeof(cvbuf));
			CURL * h = sam_prepare_handle(&cvbuf);
			if (h) {
				api->curl->easy_setopt(h, CURLOPT_URL, url.str().c_str());
				CURLcode code = api->curl->easy_perform(h);
				if (code == CURLE_OK) {
					if (cvbuf.data && cvbuf.len > 0) {
						api->ib_printf("SAM -> Successfully told SAM to stop playback...\n");
					} else {
						api->ib_printf("SAM -> No reply from SAM!\n");
					}
				} else {
					api->ib_printf("SAM -> Error talking to SAM: %s!\n", api->curl->easy_strerror(code));
				}
				api->curl->easy_cleanup(h);
			} else {
				api->ib_printf("SAM -> Error creating CURL handle!\n");
			}
			zfreenn(cvbuf.data);
			break;
		}

		case SOURCE_C_NEXT:{
			if (config.playing) {
				ostringstream url;
				url << "http://" << config.SAM_Host << ":" << config.SAM_Port << "/event/ib_next";

				SAM_BUFFER cvbuf;
				memset(&cvbuf, 0, sizeof(cvbuf));
				CURL * h = sam_prepare_handle(&cvbuf);
				if (h) {
					api->curl->easy_setopt(h, CURLOPT_URL, url.str().c_str());
					CURLcode code = api->curl->easy_perform(h);
					if (code == CURLE_OK) {
						if (cvbuf.data && cvbuf.len > 0) {
							api->ib_printf("SAM -> Successfully told SAM to skip to the next song...\n");
						} else {
							api->ib_printf("SAM -> No reply from SAM!\n");
						}
					} else {
						api->ib_printf("SAM -> Error talking to SAM: %s!\n", api->curl->easy_strerror(code));
					}
					api->curl->easy_cleanup(h);
				} else {
					api->ib_printf("SAM -> Error creating CURL handle!\n");
				}
				zfreenn(cvbuf.data);
			}
			break;
		}

	}

	return 0;
}

int remote(USER_PRESENCE * user, unsigned char cliversion, REMOTE_HEADER * head, char * data) {
	char buf[1024];
	T_SOCKET * socket = user->Sock;

	if (head->ccmd == RCMD_SRC_STATUS) {
		if (config.playing) {
			strcpy(buf, "playing");
		} else {
			strcpy(buf, "stopped");
		}
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(socket,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_GET_NAME) {
		if (config.playing) {
			strcpy(buf, "sam");
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(user->Sock,head,buf);
		}
		return 1;
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{42BE4204-17AA-4a9e-86AD-419F6D17D395}",

	"SAM",
	"SAM Integration",
	"Indy Sams / Drift Solutions",

	init,
	quit,
	MessageHandler,
	NULL,
	NULL,
	NULL,
	remote,
};

PLUGIN_EXPORT_FULL
