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

#include "trivia.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned
bool shutdown_now = false;
THREADTYPE GameThread(void *lpData);

Titus_Mutex gameInstMutex;
TRIVIA_CONFIG t_config;

class GAMEINST {
public:
	int netno;
	string chan;
	TriviaGame * game;
};
typedef std::vector<GAMEINST> gameList;
gameList games;

GAMEINST * GetInstance(int netno, string chan) {
	GAMEINST * ret = NULL;
	gameInstMutex.Lock();
	for (gameList::iterator x = games.begin(); x != games.end(); x++) {
		if (x->netno == netno && !stricmp(x->chan.c_str(),chan.c_str())) {
			ret = &(*x);
			break;
		}
	}
	gameInstMutex.Release();
	return ret;
}

GAMEINST * CreateInstance(int netno, string chan) {
	GAMEINST tmp;
	tmp.netno = netno;
	tmp.chan = chan;
	tmp.game = new TriviaGame();
	tmp.game->LoadRules(netno, chan);

	gameInstMutex.Lock();
	games.push_back(tmp);
	gameInstMutex.Release();
	return GetInstance(netno, chan);
}

void DeleteInstance(int netno, string chan) {
	gameInstMutex.Lock();
	for (gameList::iterator x = games.begin(); x != games.end(); x++) {
		if (x->netno == netno && !stricmp(x->chan.c_str(),chan.c_str())) {
			if (x->game) {
				if (x->game->HasLoaded()) {
					x->game->SaveRules();
				}
				delete x->game;
			}
			games.erase(x);
			break;
		}
	}
	gameInstMutex.Release();
}

void DestroyAllGames() {
	gameInstMutex.Lock();
	for (gameList::iterator x = games.begin(); x != games.end(); x = games.begin()) {
		if (x->game) {
			if (x->game->HasLoaded()) {
				x->game->SaveRules();
			}
			delete x->game;
		}
		games.erase(x);
	}
	gameInstMutex.Release();
}

void LoadConfig() {
	memset(&t_config, 0, sizeof(t_config));

	strcpy(t_config.langcode, "en");
	t_config.QuestionTimeout = 60;
	t_config.TimeToFirstQuestion = 30;
	t_config.TimeBetweenQuestions = 10;
	t_config.QuestionsPerRound = 50;
	t_config.PointsToWin = 20;
	t_config.EnableHints = 1;
	t_config.HintHiddenChar = HINT_HIDDEN_CHAR;

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Trivia");
	if (sec) {
		char * x = api->config->GetConfigSectionValue(sec, "LangCode");
		if (x && strlen(x) == 2) {
			sstrcpy(t_config.langcode, x);
		}

		x = api->config->GetConfigSectionValue(sec, "HintHiddenChar");
		if (x && strlen(x) == 1) {
			t_config.HintHiddenChar = *x;
		}

		long n = api->config->GetConfigSectionLong(sec, "QuestionTimeout");
		if (n > 0) { t_config.QuestionTimeout = n; }
		n = api->config->GetConfigSectionLong(sec, "TimeToFirstQuestion");
		if (n > 0) { t_config.TimeToFirstQuestion = n; }
		n = api->config->GetConfigSectionLong(sec, "TimeBetweenQuestions");
		if (n > 0) { t_config.TimeBetweenQuestions = n; }
		n = api->config->GetConfigSectionLong(sec, "QuestionsPerRound");
		if (n >= 0) { t_config.QuestionsPerRound = n; }
		n = api->config->GetConfigSectionLong(sec, "PointsToWin");
		if (n >= 0) { t_config.PointsToWin = n; }
		n = api->config->GetConfigSectionLong(sec, "EnableHints");
		if (n == 0) { t_config.EnableHints = 0; }
		n = api->config->GetConfigSectionLong(sec, "HintsEvery");
		if (n > 0) { t_config.HintsEvery = n; }
	} else {
		api->ib_printf(_("Trivia -> No 'Trivia' section, using defaults...\n"));
	}
}

void ShowScores(USER_PRESENCE * ut, USER_PRESENCE::userPresSendFunc method, string sort_by="Points", int limit=10, int netno=-1, string chan="") {
	stringstream str;
	if (netno >= 0 && chan.length()) {
		str << "SELECT * FROM TriviaChanStats WHERE NetNo=" << netno << " AND Channel=\"" << chan << "\" ORDER BY " << sort_by << " DESC LIMIT " << limit;
	} else {
		str << "SELECT * FROM TriviaStats ORDER BY " << sort_by << " DESC LIMIT " << limit;
	}
	sql_rows ret = GetTable(str.str().c_str());

	char buf[1024],buf2[1024];
	if (ret.size() == 0) {
		if (!api->LoadMessage("TriviaHistoryNone", buf2, sizeof(buf))) {
			sprintf(buf2, "Sorry, there is no score history.");
		}
		str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
		api->ProcText(buf2, sizeof(buf2));
		//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
		//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
		method(ut, buf2);
		return;
	}

	if (!api->LoadMessage("TriviaHistoryBegin", buf2, sizeof(buf))) {
		strcpy(buf2, "Trivia Top Players");
	}
	str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
	api->ProcText(buf2, sizeof(buf2));
	//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
	//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
	method(ut, buf2);
	if (api->LoadMessage("TriviaHistoryBegin2", buf2, sizeof(buf))) {
		str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
		api->ProcText(buf2, sizeof(buf2));
		//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
		//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
		method(ut, buf2);
	}

	int rank = 0;
	int lastscore = 0;
	for (sql_rows::const_iterator x = ret.begin(); x != ret.end(); x++) {
		int points = (stricmp(sort_by.c_str(),"Wins") == 0) ? atoi(x->second.find("Wins")->second.c_str()) : atoi(x->second.find("Points")->second.c_str());
		if (points != lastscore) {
			//api->ib_printf("%d %d %d\n", rank, lastscore, points);
			rank++;
			lastscore = points;
		}

		if (!api->LoadMessage("TriviaHistoryScore", buf2, sizeof(buf))) {
			strcpy(buf2, "%rank. %nick %wins %points");
		}
		sprintf(buf, "%d", rank);
		str_replace(buf2, sizeof(buf2), "%rank", buf);
		str_replace(buf2, sizeof(buf2), "%points", x->second.find("Points")->second.c_str());
		str_replace(buf2, sizeof(buf2), "%wins", x->second.find("Wins")->second.c_str());
		str_replace(buf2, sizeof(buf2), "%nick", x->second.find("Nick")->second.c_str());
		//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
		//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
		method(ut, buf2);
	}

	if (!api->LoadMessage("TriviaHistoryEnd", buf2, sizeof(buf))) {
		strcpy(buf2, "End of list.");
	}
	str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
	api->ProcText(buf2, sizeof(buf2));
	method(ut, buf2);
	//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
	//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

int Trivia_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
//	char buf[1024];

	/*
	if (!stricmp(command, "8ball") && (!parms || !strlen(parms))) {
		if (ut->Channel) {
			ut->send_notice(ut, _("Usage: 8ball question"));
		} else {
			ut->send_msg(ut, _("Usage: 8ball question"));
		}
		return 1;
	}
	*/

	if (!stricmp(command, "trivia")) {
		gameInstMutex.Lock();
		GAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() != GS_STOPPED) {
			ut->send_notice(ut, _("There is a game already in progress!"));
			gameInstMutex.Release();
			return 0;
		}
		i = CreateInstance(ut->NetNo, ut->Channel);
		if (i && i->game) {
			i->game->Start(ut->Nick, ut);
		} else {
			ut->std_reply(ut, _("Error creating game instance!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "stoptrivia") || !stricmp(command, "strivia")) {
		gameInstMutex.Lock();
		GAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i) {
			ut->send_notice(ut, _("Stopping game..."));
			DeleteInstance(ut->NetNo, ut->Channel);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "hint")) {
		gameInstMutex.Lock();
		GAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() == GS_ASKED) {
			i->game->ShowHint(ut);
		} else {
			ut->std_reply(ut, _("There is no active question!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "trivia-curscore")) {
		gameInstMutex.Lock();
		GAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() != GS_STOPPED) {
			i->game->ShowScores(false);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "trivia-scores")) {
		if (ut->Channel && ut->NetNo >= 0) {
			USER_PRESENCE::userPresSendFunc target = ut->send_notice;
			if ((ut->Flags & (UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ)) && parms && !stricmp(parms,"public")) {
				target = ut->send_chan;
			}
			ShowScores(ut, target, "Points", 10, ut->NetNo, ut->Channel);
		} else if (parms && strlen(parms)) {
			StrTokenizer st((char *)parms, ' ');
			if (st.NumTok() == 2) {
				ShowScores(ut, ut->std_reply, "Points", 10, atoi(st.stdGetSingleTok(1).c_str()), st.stdGetSingleTok(2));
			} else {
				ut->std_reply(ut, _("Usage: !trivia-scores netno channel"));
			}
		} else {
			ut->std_reply(ut, _("You must specify a netno and channel!"));
		}
	}

	if (!stricmp(command, "trivia-wins")) {
		if (ut->Channel && ut->NetNo >= 0) {
			USER_PRESENCE::userPresSendFunc target = ut->send_notice;
			if ((ut->Flags & (UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ)) && parms && !stricmp(parms,"public")) {
				target = ut->send_chan;
			}
			ShowScores(ut, target, "Wins", 10, ut->NetNo, ut->Channel);
		} else if (parms && strlen(parms)) {
			StrTokenizer st((char *)parms, ' ');
			if (st.NumTok() == 2) {
				ShowScores(ut, ut->std_reply, "Wins", 10, atoi(st.stdGetSingleTok(1).c_str()), st.stdGetSingleTok(2));
			} else {
				ut->std_reply(ut, _("Usage: !trivia-wins netno channel"));
			}
		} else {
			ut->std_reply(ut, _("You must specify a netno and channel!"));
		}
	}

	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;

	api->ib_printf(_("Trivia Plugin -> Loading...\n"),pluginnum);
	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Trivia Plugin -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	api->db->Query("CREATE TABLE IF NOT EXISTS TriviaGames (ID INTEGER PRIMARY KEY AUTOINCREMENT, NetNo INT DEFAULT 0, Channel VARCHAR(255));", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS TriviaGamesUnique ON TriviaGames (NetNo, Channel);", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS TriviaConfig (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INT, Name VARCHAR(255), Value VARCHAR(255));", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS TriviaConfigUnique ON TriviaConfig (GameID, Name);", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS TriviaStats (ID INTEGER PRIMARY KEY AUTOINCREMENT, Nick VARCHAR(255), Points INT DEFAULT 0, Wins INT DEFAULT 0);", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS TriviaStatsUnique ON TriviaStats (Nick);", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS TriviaChanStats (ID INTEGER PRIMARY KEY AUTOINCREMENT, NetNo INT DEFAULT 0, Channel VARCHAR(255), Nick VARCHAR(255), Points INT DEFAULT 0, Wins INT DEFAULT 0);", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS TriviaChanStatsUnique ON TriviaChanStats (NetNo, Channel, Nick);", NULL, NULL, NULL);
	LoadConfig();
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "trivia",		&acl, CM_ALLOW_IRC_PUBLIC, Trivia_Commands, _("Starts a game of Trivia"));
	api->commands->RegisterCommand_Proc(pluginnum, "hint",			&acl, CM_ALLOW_IRC_PUBLIC, Trivia_Commands, _("Get a hint for the current question"));
	api->commands->RegisterCommand_Proc(pluginnum, "trivia-scores",	&acl, CM_ALLOW_ALL, Trivia_Commands, _("Shows the (up to) top 10 trivia players by number of points"));
	api->commands->RegisterCommand_Proc(pluginnum, "trivia-wins",		&acl, CM_ALLOW_ALL, Trivia_Commands, _("Shows the (up to) top 10 trivia players by number of game wins"));

	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "strivia",		&acl, CM_ALLOW_IRC_PUBLIC, Trivia_Commands, _("Stops a game of Trivia"));
	api->commands->RegisterCommand_Proc(pluginnum, "stoptrivia",	&acl, CM_ALLOW_IRC_PUBLIC, Trivia_Commands, _("Stops a game of Trivia"));

	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "trivia-curscore",	&acl, CM_ALLOW_IRC_PUBLIC, Trivia_Commands, _("Shows the score of the current game to the game's channel"));
	TT_StartThread(GameThread, NULL, _("GameThread"), pluginnum);

	return 1;
}

void quit() {
	api->ib_printf(_("Trivia Plugin -> Shutting down...\n"));
	shutdown_now=true;
	api->commands->UnregisterCommandByName("trivia");
	api->commands->UnregisterCommandByName("strivia");
	api->commands->UnregisterCommandByName("stoptrivia");
	api->commands->UnregisterCommandByName("hint");
	api->commands->UnregisterCommandByName("trivia-scores");
	api->commands->UnregisterCommandByName("trivia-wins");
	api->commands->UnregisterCommandByName("trivia-curscore");
	//api->commands->UnregisterCommandByName("weather");
	while (TT_NumThreadsWithID(pluginnum)) {
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}
	DestroyAllGames();
	api->ib_printf(_("Trivia Plugin -> Shutdown complete\n"));
}

int message(unsigned int MsgID, char * data, int datalen) {
	switch(MsgID) {
		case IB_ONTEXT:{
			IBM_USERTEXT * ot = (IBM_USERTEXT *)data;
			if (ot->type == IBM_UTT_CHANNEL) {
				gameInstMutex.Lock();
				GAMEINST * i = GetInstance(ot->userpres->NetNo, ot->userpres->Channel);
				if (i && i->game && i->game->GetStatus() == GS_ASKED) {
					//api->ib_printf("Trivia -> Text: %s\n", ot->text);
					i->game->CheckAnswer(ot->userpres->User ? ot->userpres->User->Nick:ot->userpres->Nick, ot->userpres->Nick, ot->text);
				}
				gameInstMutex.Release();
			}
			/*
				char buf[4096],buf2[256];
				IBM_USER * oj = (IBM_USER *)data;
				if (oj == NULL) { return 0; }
				if (stricmp(oj->nick, api->GetBotNick(oj->netno))) {
					sprintf(buf2, "WelcomeOnJoin%d", oj->ulevel);
					if (api->LoadMessage(buf2, buf, sizeof(buf))) {
						api->ib_printf("Welcome -> Welcoming %s on %s (%s)\n", oj->nick, oj->channel, api->GetBotNick(oj->netno));
						api->ProcText(buf, sizeof(buf));
						str_replace(buf, sizeof(buf), "%chan", oj->channel);
						str_replace(buf, sizeof(buf), "%nick", oj->nick);
						sprintf(buf2,"%d",oj->ulevel);
						str_replace(buf, sizeof(buf), "%level", buf2);
						strcat(buf, "\r\n");
						api->SendIRC(oj->netno,buf,strlen(buf));
					}
				}
				*/
			}
			break;

	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{F8D8624F-0E96-4E7F-9381-34B66AAF678D}",
	"Trivia",
	"Trivia Plugin 1.0",
	"ShoutIRC.com",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL

THREADTYPE GameThread(VOID *lpData) {
//	char buf[1024*5], buf2[1024];
	TT_THREAD_START

	while (!shutdown_now) {
		gameInstMutex.Lock();
		for (gameList::iterator x = games.begin(); x != games.end(); x++) {
			if (x->game) {
				if (x->game->GetStatus() != GS_STOPPED) {
					x->game->Work();
				}
			}
		}
		gameInstMutex.Release();
		api->safe_sleep_seconds(1);
	}

	TT_THREAD_END
}

