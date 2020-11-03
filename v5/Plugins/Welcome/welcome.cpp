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

#include "welcome.h"
#if defined(WIN32)
	#ifndef MUPARSER_STATIC
		#define MUPARSER_STATIC
	#endif
	#if defined(DEBUG)
	#pragma comment(lib, "muparser32_d.lib")
	#else
	#pragma comment(lib, "muparser32.lib")
	#endif
#endif
#include <muParser.h>

//en.wikipedia.org/w/api.php?action=query&prop=revisions&format=xml&rvprop=content&rvlimit=1&rvparse=&titles=Hollywood

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

WELCOME_CONFIG wel_config;
THREADTYPE WebThread(void *lpData);
Titus_Sockets3 * sockets = NULL;
Titus_Mutex queMutex;
WEB_QUEUE *wQue=NULL;
PING_QUEUE *pingQue=NULL;

typedef std::map<std::string, time_t, ci_less> weatherNicksType;
weatherNicksType weatherNicks;

bool AllowWeatherNick(const char * nick) {
	bool ret = true;
	queMutex.Lock();
	for (weatherNicksType::iterator x = weatherNicks.begin(); x != weatherNicks.end(); x++) {
		if (x->second < (time(NULL) - wel_config.weatherLimit)) {
			weatherNicks.erase(x);
		}
	}
	weatherNicksType::const_iterator x2 = weatherNicks.find(nick);
	if (x2 != weatherNicks.end()) {
		ret = false;
	} else {
		weatherNicks[nick] = time(NULL);
	}
	queMutex.Release();
	return ret;
}

void AddToWebQueue(WEB_QUEUE * Add) {
	queMutex.Lock();
	Add->Next = NULL;
	if (wQue == NULL) {
		wQue = Add;
	} else {
		WEB_QUEUE * Scan = wQue;
		while (Scan->Next != NULL) {
			Scan=Scan->Next;
		}
		Scan->Next=Add;
	}
	queMutex.Release();
}
void AddToPingQueue(PING_QUEUE * Add) {
	queMutex.Lock();
	Add->Next = NULL;
	if (pingQue == NULL) {
		pingQue = Add;
	} else {
		PING_QUEUE * Scan = pingQue;
		while (Scan->Next != NULL) {
			Scan=Scan->Next;
		}
		Scan->Next=Add;
	}
	queMutex.Release();
}

static char ballmessages[20][26] = {
	"Outlook Good",
	"Outlook Not So Good",
	"My Reply Is No",
	"Don't Count On It",
	"You May Rely On It",
	"Ask Again Later",
	"Most Likely",
	"Cannot Predict Now",
	"Yes",
	"Yes Definately",
	"Better Not Tell You Now",
	"It Is Certain",
	"Very Doubtful",
	"It Is Decidedly So",
	"Concentrate and Ask Again",
	"Signs Point to Yes",
	"My Sources Say No",
	"Without a Doubt",
	"Reply Hazy, Try Again",
	"As I See It, Yes"
};

#define NUM_CALC_VARS 16
struct WELCOME_CALC_VARS {
	double vals[NUM_CALC_VARS];
	int ind;
};

double * Welcome_AddVariable(const char *a_szName, void *pUserData) {
	WELCOME_CALC_VARS * cv = (WELCOME_CALC_VARS *)pUserData;
	if (cv->ind >= NUM_CALC_VARS) {
		throw mu::Parser::exception_type("Too many variables used in equation");
	}
	cv->vals[cv->ind] = 0;
	return &cv->vals[cv->ind++];
}

int Welcome_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];

	if (!stricmp(command, "ping")) {
		string nick = ut->Nick;
		bool doPublic = wel_config.pingDefaultPublic;
		if (parms && parms[0]) {
			StrTokenizer st((char *)parms, ' ');
			if (!stricmp(st.stdGetSingleTok(1).c_str(), "public")) {
				doPublic = true;
			} else if (!stricmp(st.stdGetSingleTok(1).c_str(), "private")) {
				doPublic = false;
			} else {
				nick = st.stdGetSingleTok(1);
			}
			if (st.NumTok() >= 2) {
				nick = st.stdGetSingleTok(2);
			}
		}

		PING_QUEUE * Scan = (PING_QUEUE *)zmalloc(sizeof(PING_QUEUE));
		memset(Scan, 0, sizeof(PING_QUEUE));
		RefUser(ut);
		Scan->pres = ut;
		Scan->req_time = api->get_cycles();
//		Scan->type = Ping;
		Scan->nofree = true;
		sstrcpy(Scan->data, nick.c_str());
		Scan->doPublic = doPublic;
		AddToPingQueue(Scan);

		stringstream sstr;
		sstr << "PRIVMSG " << nick << " :\001PING " << Scan->req_time << "\001\r\n";
		api->irc->SendIRC_Priority(ut->NetNo, sstr.str().c_str(), sstr.str().length(), PRIORITY_IMMEDIATE);
		return 1;
	}

	if (!stricmp(command, "calc") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: calc equation"));
		return 1;
	}

	if (!stricmp(command, "calc")) {
		mu::Parser p;
		WELCOME_CALC_VARS cv;
		cv.ind = 0;
		p.SetVarFactory(Welcome_AddVariable, &cv);
		p.SetExpr(parms);
		try {
			mu::value_type res = p.Eval();
			int64 res2 = res;
			if (res == res2) {
				snprintf(buf, sizeof(buf), "Result: " I64FMT "\n", res2);
			} else {
				snprintf(buf, sizeof(buf), "Result: %f\n", res);
			}
			if (ut->Channel) {
				ut->send_chan(ut, buf);
			} else {
				ut->send_msg(ut, buf);
			}
			const mu::varmap_type vars = p.GetUsedVar();
			for (mu::varmap_type::const_iterator x = vars.begin(); x != vars.end(); x++) {
				res = *x->second;
				res2 = res;
				if (res == res2) {
					snprintf(buf, sizeof(buf), "Variable %s = " I64FMT "\n", x->first.c_str(), res2);
				} else {
					snprintf(buf, sizeof(buf), "Variable %s = %f\n", x->first.c_str(), res);
				}
				if (ut->Channel) {
					ut->send_chan(ut, buf);
				} else {
					ut->send_msg(ut, buf);
				}
			}
		} catch (mu::ParserError e) {
			snprintf(buf, sizeof(buf), _("Error parsing equation: %s (error pos: %u, token: %s)"), e.GetMsg().c_str(), e.GetPos(), e.GetToken().c_str());
			ut->std_reply(ut, buf);
		} catch (...) {
			ut->std_reply(ut, _("Unknown error parsing your equation"));
		}
	}

	if (!stricmp(command, "8ball") && (!parms || !strlen(parms))) {
		if (ut->Channel) {
			ut->send_notice(ut, _("Usage: 8ball question"));
		} else {
			ut->send_msg(ut, _("Usage: 8ball question"));
		}
		return 1;
	}

	if (!stricmp(command, "8ball")) {
		sprintf(buf,"\x02%s\x02, The Magic 8 Ball Says: \x02%s\x02\n",ut->Nick,ballmessages[api->genrand_int32()%20]);
		if (ut->Channel) {
			ut->send_chan(ut, buf);
		} else {
			ut->send_msg(ut, buf);
		}
	}

	if (!stricmp(command, "wordnik") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: wordnik word"));
		return 1;
	}

	if (!stricmp(command, "wordnik")) {
		const char * p = strstr(parms, " ");
		if (p) {
			ut->std_reply(ut, _("You can only look up 1 word at a time!"));
			return 1;
		}

		WEB_QUEUE * Scan = (WEB_QUEUE *)zmalloc(sizeof(WEB_QUEUE));
		memset(Scan, 0, sizeof(WEB_QUEUE));
		RefUser(ut);
		Scan->pres = ut;
		sstrcpy(Scan->data, parms);

		Scan->req_time = time(NULL);
		Scan->handler = DoWordnik;
		AddToWebQueue(Scan);
	}

	if (!stricmp(command, "youtube") && (!parms || !strlen(parms))) {
		if (ut->Channel) {
			ut->send_notice(ut, _("Usage: youtube url or video id"));
		} else {
			ut->send_msg(ut, _("Usage: youtube url or video id"));
		}
		return 1;
	}

	if (!stricmp(command, "youtube")) {
		const char * p = strstr(parms, "v=");
		if (p) {
			sstrcpy(buf, p+2);
			char * q = strchr(buf, '&');
			if (q) { *q = 0; }
		} else {
			sstrcpy(buf, parms);
		}
		WEB_QUEUE * Scan = (WEB_QUEUE *)zmalloc(sizeof(WEB_QUEUE));
		memset(Scan, 0, sizeof(WEB_QUEUE));
		RefUser(ut);
		Scan->pres = ut;
		sstrcpy(Scan->data, buf);

		Scan->req_time = time(NULL);
		Scan->handler = DoYouTubeInfo;
		AddToWebQueue(Scan);
	}

	if (!stricmp(command, "tinyurl") && (!parms || !strlen(parms))) {
		if (ut->Channel) {
			ut->send_notice(ut, _("Usage: tinyurl url"));
		} else {
			ut->send_msg(ut, _("Usage: tinyurl url"));
		}
		return 1;
	}

	if (!stricmp(command, "tinyurl")) {
		WEB_QUEUE * Scan = (WEB_QUEUE *)zmalloc(sizeof(WEB_QUEUE));
		memset(Scan, 0, sizeof(WEB_QUEUE));
		RefUser(ut);
		Scan->pres = ut;
		sstrcpy(Scan->data, parms);

		Scan->req_time = time(NULL);
		Scan->handler = DoTinyURL;
		AddToWebQueue(Scan);
	}

	if (!stricmp(command, "bing") && (!parms || !strlen(parms))) {
		if (ut->Channel) {
			ut->send_notice(ut, _("Usage: bing keyword [more keywords]"));
		} else {
			ut->send_msg(ut, _("Usage: bing keyword [more keywords]"));
		}
		return 1;
	}

	if (!stricmp(command, "bing")) {
		WEB_QUEUE * Scan = (WEB_QUEUE *)zmalloc(sizeof(WEB_QUEUE));
		memset(Scan, 0, sizeof(WEB_QUEUE));
		RefUser(ut);
		Scan->pres = ut;
		sstrcpy(Scan->data, parms);

		Scan->req_time = time(NULL);
		Scan->handler = DoWebSearch;
		AddToWebQueue(Scan);
	}

	if (!stricmp(command, "spinbottle")) {
		int num = api->ial->num_nicks_in_chan(ut->NetNo, ut->Channel);
		if (num > 2) {
			int left = num;
			int n = api->genrand_int32()%num;
			while (left-- && api->ial->get_nick_in_chan(ut->NetNo, ut->Channel, n, buf, sizeof(buf))) {
				if (stricmp(buf, ut->Nick)) { left = -99; break; }
				n++;
				if (n >= num) { n = 0; }
			}
			if (left == -99) {
				WEB_QUEUE * Scan = (WEB_QUEUE *)zmalloc(sizeof(WEB_QUEUE));
				memset(Scan, 0, sizeof(WEB_QUEUE));
				RefUser(ut);
				Scan->pres = ut;
				sstrcpy(Scan->data, buf);

				if (!api->LoadMessage("SpinBottleActions", buf, sizeof(buf))) {
					strcpy(buf, "make out with|kiss|make love with|have sex with|kiss the ass of|lick|smooch|hump|go on a date with|lick|french kiss");
				}
				StrTokenizer st(buf, '|');
				int n = (api->genrand_int32()%st.NumTok())+1;
				sstrcpy(Scan->data2, st.stdGetSingleTok(n).c_str());

				Scan->req_time = time(NULL)+3;
				Scan->stage = 0;
				Scan->handler = DoSpinTheBottle;
				Scan->nofree = true;
				AddToWebQueue(Scan);
				ut->send_chan(ut, "\001ACTION spins the bottle...\001");
			} else {
				sstrcpy(buf, _("Error finding someone for you to play with..."));
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf, _("It's just me and you %s ..."), ut->Nick);
			ut->std_reply(ut, buf);
		}
	}

	if (!stricmp(command, "weather") || !stricmp(command, "weather_public")) {
		if (api->user->uflag_have_any_of(ut->Flags, UFLAG_MASTER|UFLAG_OP) || AllowWeatherNick(ut->Nick)) {
			WEB_QUEUE * Scan = (WEB_QUEUE *)zmalloc(sizeof(WEB_QUEUE));
			memset(Scan, 0, sizeof(WEB_QUEUE));

			Scan->send_func = (stricmp(command, "weather_public") == 0) ? ut->send_chan : ut->send_msg;

			stringstream sstr;
			char * nick = api->db->MPrintf("%q", ut->User ? ut->User->Nick:ut->Nick);
			sstr << "SELECT City,IsCelcius FROM Weather WHERE Nick='" << nick << "'";
			api->db->Free(nick);
			char ** resultp;
			int nrow=0, ncol=0;
			if (api->db->GetTable(sstr.str().c_str(), &resultp, &nrow, &ncol, NULL) == SQLITE_OK) {
				if (nrow > 0) {
					const char * city = api->db->GetTableEntry(1, 0, resultp, nrow, ncol);
					strlcpy(Scan->data, city, sizeof(Scan->data));
					city = api->db->GetTableEntry(1, 0, resultp, nrow, ncol);
					Scan->getCelcius = atoi(city);
				}
				api->db->FreeTable(resultp);
				//api->ib_printf("From DB: %s %d\n", Scan->data, Scan->getCelcius);
			}

			if (parms && parms[0]) {
				if (parms[1] == ' ' || parms[1] == 0) {
					if (parms[0] == 'c') {
						Scan->getCelcius = true;
					}
					if (parms[0] == 'f') {
						Scan->getCelcius = false;
					}
					if (parms[1] == ' ') {
						strlcpy(Scan->data, parms+2, sizeof(Scan->data));
					}
				} else {
					strlcpy(Scan->data, parms, sizeof(Scan->data));
				}
			}

			if (Scan->data[0] == 0) {
				ut->send_msg(ut, _("Usage: weather [c/f for celcius/fahrenheit, optional] city-or-zip-code"));
				zfree(Scan);
				return 0;
			}

			Scan->pres = ut;
			RefUser(ut);
			Scan->req_time = time(NULL);
			Scan->handler = DoWeather;
			AddToWebQueue(Scan);
			if (!stricmp(command, "weather_public")) {
				ut->std_reply(ut, _("I have queued that city for forecast, it will be displayed in the channel when retrieval is complete..."));
			} else {
				ut->std_reply(ut, _("I have queued that city for forecast, you will be PMed when retrieval is complete..."));
			}
		} else {
			ut->std_reply(ut, _("You may only use weather once every 5 minutes."));
		}
	}

	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=(BOTAPI_DEF *)BotAPI;

	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Welcome Plugin -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	sockets = new Titus_Sockets();
	api->db->Query("CREATE TABLE IF NOT EXISTS Weather (ID INTEGER PRIMARY KEY AUTOINCREMENT, Nick VARCHAR(255), City VARCHAR(255), IsCelcius TINYINT DEFAULT 0, TimeStamp INT DEFAULT 0);", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS WeatherUnique ON Weather (Nick);", NULL, NULL, NULL);

	memset(&wel_config, 0, sizeof(wel_config));
	wel_config.weatherLimit = 300;
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Welcome");
	if (sec) {
		if (api->config->GetConfigSectionLong(sec, "PingDefaultPublic") > 0) {
			wel_config.pingDefaultPublic = true;
		}
		int n = api->config->GetConfigSectionLong(sec, "WeatherLimit");
		if (n >= 0) {
			wel_config.weatherLimit = n;
		}
		api->config->GetConfigSectionValueBuf(sec, "BingAccountKey", wel_config.BingAccountKey, sizeof(wel_config.BingAccountKey));
		api->config->GetConfigSectionValueBuf(sec, "WeatherKey", wel_config.WeatherKey, sizeof(wel_config.WeatherKey));
	}
	sec = api->config->GetConfigSection(NULL, "Base");
	if (sec) {
		api->config->GetConfigSectionValueBuf(sec, "RegKey", wel_config.RegKey, sizeof(wel_config.RegKey));
	}

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "calc",			&acl, CM_ALLOW_ALL, Welcome_Commands, _("Do a calculation for you. Simple or more advanced equations with variables are supported."));
	api->commands->RegisterCommand_Proc(pluginnum, "8ball",			&acl, CM_ALLOW_ALL, Welcome_Commands, _("A magic 8 Ball for public use"));
	if (wel_config.WeatherKey[0]) { api->commands->RegisterCommand_Proc(pluginnum, "weather", &acl, CM_ALLOW_ALL, Welcome_Commands, _("A function that will show you your weather information")); }
	api->commands->RegisterCommand_Proc(pluginnum, "youtube",		&acl, CM_ALLOW_ALL, Welcome_Commands, _("This will show you information about a YouTube video"));
	api->commands->RegisterCommand_Proc(pluginnum, "tinyurl",		&acl, CM_ALLOW_ALL, Welcome_Commands, _("This will shorten a URL using TinyURL.com"));
	if (wel_config.RegKey[0]) { api->commands->RegisterCommand_Proc(pluginnum, "wordnik",		&acl, CM_ALLOW_ALL, Welcome_Commands, _("This will look up definitions for a word on Wordnik.com")); }
	if (wel_config.BingAccountKey[0]) { api->commands->RegisterCommand_Proc(pluginnum, "bing",		&acl, CM_ALLOW_ALL, Welcome_Commands, _("This will search the web for the term you specified")); }
	api->commands->RegisterCommand_Proc(pluginnum, "spinbottle",	&acl, CM_ALLOW_IRC_PUBLIC, Welcome_Commands, _("Play spin the bottle"));
	api->commands->RegisterCommand_Proc(pluginnum, "ping",			&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_IRC_PRIVATE, Welcome_Commands, _("This will show you the lag between the bot and yourself or another user"));

	api->commands->FillACL(&acl, 0, UFLAG_DJ, 0);
	if (wel_config.WeatherKey[0]) { api->commands->RegisterCommand_Proc(pluginnum, "weather_public", &acl, CM_ALLOW_ALL_PUBLIC, Welcome_Commands, _("A function that will show the channel weather information")); }

	TT_StartThread(WebThread, NULL, _("WebThread"), pluginnum);
	return 1;
}

void quit() {
	api->ib_printf(_("Welcome Plugin -> Shutting down...\n"));
	wel_config.shutdown_now=true;
	api->commands->UnregisterCommandByName("calc");
	api->commands->UnregisterCommandByName("8ball");
	api->commands->UnregisterCommandByName("spinbottle");
	api->commands->UnregisterCommandByName("weather");
	api->commands->UnregisterCommandByName("weather_public");
	api->commands->UnregisterCommandByName("youtube");
	api->commands->UnregisterCommandByName("wordnik");
	api->commands->UnregisterCommandByName("tinyurl");
	api->commands->UnregisterCommandByName("bing");
	api->commands->UnregisterCommandByName("ping");
	while (TT_NumThreadsWithID(pluginnum)) {
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}
	delete sockets;
	sockets = NULL;
	api->ib_printf(_("Welcome Plugin -> Shutdown complete\n"));
}

int noticeproc(USER_PRESENCE * ut, const char * msg) {
	if (!strnicmp(msg,"\001PING ",6)) {
		char * tmp = zstrdup(msg);
		strtrim(tmp, "\001 ");
		char * p = tmp + 5;
		int64 diff = atoi64(p);
		zfree(tmp);

		queMutex.Lock();
		PING_QUEUE * Scan = pingQue;
		while (Scan) {
			//api->ib_printf("Check %d/%d %s/%s " I64FMT "/" I64FMT " :", Scan->type, Ping, ut->Nick, Scan->data, diff, Scan->req_time);
			if (!stricmp(ut->Nick, Scan->data) && diff == Scan->req_time) {
				//api->ib_printf("found\n");
				break;
			}
			//api->ib_printf("not found\n");
			Scan = Scan->Next;
		}
		queMutex.Release();

		if (Scan) {
			ut = Scan->pres;
			USER_PRESENCE::userPresSendFunc send = NULL;
			if (ut->Channel) {
				if (Scan->doPublic) {
					send = ut->send_chan;
				} else {
					send = ut->send_chan_notice;
				}
			} else {
				send = ut->send_msg;
			}
			char buf[512],buf2[32];
			if (!api->LoadMessage("PingUser", buf, sizeof(buf))) {
				sstrcpy(buf, "Round-trip between me and %target: %lagsec secs");
			}
			str_replace(buf, sizeof(buf), "%nick", ut->Nick);
			str_replace(buf, sizeof(buf), "%target", Scan->data);
			diff = api->get_cycles() - diff;
			sprintf(buf2,  I64FMT , diff);
			str_replace(buf, sizeof(buf), "%lagms", buf2);
			double x =diff;
			x /= 1000;
			sprintf(buf2, "%.2f", x);
			str_replace(buf, sizeof(buf), "%lagsec", buf2);
			api->ProcText(buf, sizeof(buf));
			send(ut, buf);
			Scan->nofree = false;
		} else {
			api->ib_printf("Welcome -> Unknown ping reply from %s\n", ut->Nick);
		}
		//sprintf(buf2,"NOTICE %s :%s\r\n",from,parms[1]);
		//bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
	}
	return 0;
}

struct WELCOME_LEVELS {
	uint32 flag;
	const char * title;
	int level;
};

static WELCOME_LEVELS w_levels[4] = {
	{ UFLAG_MASTER, "Bot Master", 1 },
	{ UFLAG_OP, "Operator", 2 },
	{ UFLAG_HOP, "HalfOp", 3 },
	{ UFLAG_DJ, "DJ", 4 }
};

int message(unsigned int MsgID, char * data, int datalen) {
	switch(MsgID) {
		case IB_ONJOIN:{
				char buf[4096],buf2[256];
				USER_PRESENCE * oj = (USER_PRESENCE *)data;
				if (oj == NULL) { return 0; }
				if (stricmp(oj->Nick, api->irc->GetCurrentNick(oj->NetNo))) {
					for (int i=0; i < 4; i++) {
						if (oj->Flags & w_levels[i].flag) {
							sprintf(buf2, "WelcomeOnJoin%d", w_levels[i].level);
							if (api->LoadMessage(buf2, buf, sizeof(buf)) || api->LoadMessage("WelcomeOnJoin", buf, sizeof(buf))) {
								api->ib_printf("Welcome -> Welcoming %s on %s ...\n", oj->Nick, oj->Channel);
								api->ProcText(buf, sizeof(buf));
								str_replace(buf, sizeof(buf), "%chan", oj->Channel);
								str_replace(buf, sizeof(buf), "%nick", oj->Nick);
								str_replace(buf, sizeof(buf), "%title", w_levels[i].title);
								strcat(buf, "\r\n");
								api->irc->SendIRC(oj->NetNo,buf,strlen(buf));
							}
							break;
						}
					}
				}
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{B772AD63-2E85-4dba-BD31-A3870B0D74F1}",
	"Welcome",
	"Welcome Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	noticeproc,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL

void strcpy_safe(char * dest, const char * src, size_t len) {
	if (src) {
		strlcpy(dest,src,len);
	} else {
		strlcpy(dest,"(null)",len);
	}
}

void strcat_safe(char * dest, const char * src, size_t len) {
	if (src) {
		strlcat(dest, src, len);
	} else {
		strlcat(dest, "(null)", len);
	}
}

int atoi_safe(const char * p, int iDefault) {
	if (p) {
		return atoi(p);
	}
	return iDefault;
}

size_t cachedDownloadWrite(void *ptr, size_t size, size_t nmemb, void *stream) {
	FILE * fp = (FILE *)stream;
	return fwrite(ptr, size, nmemb, fp);
};

char * get_cached_download(const char * url, const char * fn, char * error, size_t errSize, CACHED_DOWNLOAD_OPTIONS * opts) {
	char * ret = NULL;

	struct stat st;
	if (stat(fn, &st) == 0) {
		time_t timeLimit = (opts && opts->cache_time_limit) ? opts->cache_time_limit:300;
		if ((time(NULL) - st.st_mtime) < timeLimit) {
			FILE * fp = fopen(fn, "rb");
			if (fp) {
				long len = st.st_size;
				ret = (char *)zmalloc(len+1);
				ret[len] = 0;
				if (fread(ret, len, 1, fp) != 1) {
					zfree(ret);
					ret = NULL;
				}
				fclose(fp);
			}
		}
	}

	if (ret == NULL) {
		FILE * fp = fopen(fn, "w+b");
		if (fp) {
			CURL * h = api->curl->easy_init();
			if (h) {
				api->curl->easy_setopt(h, CURLOPT_URL, url);
				api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
				api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1 );
				api->curl->easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1);
				api->curl->easy_setopt(h, CURLOPT_MAXREDIRS, 3);
				api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 60);
				api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10);
				api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, cachedDownloadWrite);
				api->curl->easy_setopt(h, CURLOPT_WRITEDATA, fp);
				api->curl->easy_setopt(h, CURLOPT_FAILONERROR, 0);
				api->curl->easy_setopt(h, CURLOPT_ERRORBUFFER, error);
				api->curl->easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0);
				api->curl->easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0);
				api->curl->easy_setopt(h, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);

				if (opts && opts->userpwd) {
					api->curl->easy_setopt(h, CURLOPT_USERPWD, opts->userpwd);
				}

				api->ib_printf(_("Welcome -> Performing URL fetch...\n"));
				CURLcode res = api->curl->easy_perform(h);
				if (res == CURLE_OK) {
					long len = ftell(fp);
					fseek(fp, 0, SEEK_SET);
					ret = (char *)zmalloc(len+1);
					ret[len] = 0;
					if (fread(ret, len, 1, fp) != 1) {
						snprintf(error, errSize, "error reading file");
						zfree(ret);
						ret = NULL;
					}
				} else {
					snprintf(error, errSize, "cURL Error: %s", api->curl->easy_strerror(res));
				}
				api->curl->easy_cleanup(h);
			} else {
				strlcpy(error, "error allocating curl handle", errSize);
			}
			fclose(fp);
			if (ret == NULL) { remove(fn); }
		} else {
			strlcpy(error, "error allocating temp file", errSize);
		}
	}

	return ret;
}

void DoPing(PING_QUEUE * Scan) {
	if ((api->get_cycles() - Scan->req_time) > 10000) {
		USER_PRESENCE * ut = Scan->pres;
		USER_PRESENCE::userPresSendFunc send = NULL;
		if (ut->Channel) {
			if (Scan->doPublic) {
				send = ut->send_chan;
			} else {
				send = ut->send_chan_notice;
			}
		} else {
			send = ut->send_msg;
		}
		char buf[512];//,buf2[32];
		if (!api->LoadMessage("PingTimeout", buf, sizeof(buf))) {
			strcpy(buf, "Timed out trying to ping %target!");
		}
		str_replace(buf, sizeof(buf), "%nick", ut->Nick);
		str_replace(buf, sizeof(buf), "%target", Scan->data);
		api->ProcText(buf, sizeof(buf));
		send(ut, buf);

		Scan->nofree = false;
	}
}

THREADTYPE WebThread(VOID *lpData) {
//	char buf[1024*5], buf2[1024];
	TT_THREAD_START

	while (!wel_config.shutdown_now) {
		if (wQue != NULL) {
			if (wQue->handler) {
				wQue->handler(wQue);
			}

			if (!wQue->nofree) {
				WEB_QUEUE *toFree=wQue;
				queMutex.Lock();
				wQue=wQue->Next;
				queMutex.Release();
				if (toFree->pres) { UnrefUser(toFree->pres); }
				zfree(toFree);
			}
		}
		if (pingQue != NULL) {
			DoPing(pingQue);
			if (!pingQue->nofree) {
				PING_QUEUE *toFree=pingQue;
				queMutex.Lock();
				pingQue=pingQue->Next;
				queMutex.Release();
				if (toFree->pres) { UnrefUser(toFree->pres); }
				zfree(toFree);
			}
		}
		api->safe_sleep_seconds(1);
	}

	TT_THREAD_END
}

