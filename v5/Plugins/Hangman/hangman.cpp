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

#include "hangman.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned
bool shutdown_now = false;
THREADTYPE GameThread(void *lpData);

Titus_Mutex gameInstMutex;
HANGMAN_CONFIG hangman_config;

class HANGMANGAMEINST {
public:
	string acctid;
	HangmanGame * game;
};
typedef std::vector<HANGMANGAMEINST> hangmanGameList;
hangmanGameList hangmanGames;

string GetInstanceName(USER_PRESENCE * ut) {
	if (ut->User) {
		string ret = "user_";
		ret += ut->User->Nick;
		return ret;
	}

	stringstream ret;
	ret << "nonuser_" << ut->Desc << "_" << ut->NetNo << "_" << ut->Nick;
	return ret.str();
}

HANGMANGAMEINST * GetInstance(USER_PRESENCE * ut) {
	//HANGMANGAMEINST * ret = NULL;
	string acctid = GetInstanceName(ut);
	AutoMutex(gameInstMutex);
	for (hangmanGameList::iterator x = hangmanGames.begin(); x != hangmanGames.end(); x++) {
		if (!stricmp(acctid.c_str(),x->acctid.c_str())) {
			return &(*x);
		}
	}
	return NULL;
}

HANGMANGAMEINST * CreateInstance(USER_PRESENCE * ut) {
	HANGMANGAMEINST tmp;
	tmp.acctid = GetInstanceName(ut);
	tmp.game = new HangmanGame();
	RefUser(ut);

	gameInstMutex.Lock();
	hangmanGames.push_back(tmp);
	gameInstMutex.Release();
	return GetInstance(ut);
}

void DeleteInstance(USER_PRESENCE * ut) {
	string acctid = GetInstanceName(ut);
	gameInstMutex.Lock();
	for (hangmanGameList::iterator x = hangmanGames.begin(); x != hangmanGames.end(); x++) {
		if (!stricmp(acctid.c_str(),x->acctid.c_str())) {
			if (x->game) {
				delete x->game;
			}
			hangmanGames.erase(x);
			break;
		}
	}
	gameInstMutex.Release();
}

void DestroyAllGames() {
	gameInstMutex.Lock();
	for (hangmanGameList::iterator x = hangmanGames.begin(); x != hangmanGames.end(); x = hangmanGames.begin()) {
		if (x->game) {
			delete x->game;
		}
		hangmanGames.erase(x);
	}
	gameInstMutex.Release();
}

void LoadConfig() {
	memset(&hangman_config, 0, sizeof(hangman_config));

	hangman_config.NumTries = HANGMAN_NUM_TRIES;
	hangman_config.TurnTimeout = 60;

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Hangman");
	if (sec) {
		long n = api->config->GetConfigSectionLong(sec, "NumTries");
		if (n > 0) {
			hangman_config.NumTries = n;
		}
		n = api->config->GetConfigSectionLong(sec, "TurnTimeout");
		if (n > 0) { hangman_config.TurnTimeout = n; }
	} else {
		api->ib_printf(_("Hangman -> No 'Hangman' section, using defaults...\n"));
	}
}

void ShowScores(USER_PRESENCE * ut, int limit=10) {
	stringstream str;
	str << "SELECT * FROM HangmanStats WHERE Nick='";
	char * n = api->db->MPrintf("%q", ut->User ? ut->User->Nick:ut->Nick);
	str << n;
	api->db->Free(n);
	str << "' LIMIT " << limit;
	sql_rows ret = GetTable(str.str().c_str());

	char buf[1024],buf2[1024];
	if (ret.size() == 0) {
		if (!api->LoadMessage("HangmanHistoryNone", buf2, sizeof(buf))) {
			sprintf(buf2, "Sorry, you have no score history.");
		}
		api->ProcText(buf2, sizeof(buf2));
		ut->std_reply(ut, buf2);
		return;
	}

	for (sql_rows::const_iterator x = ret.begin(); x != ret.end(); x++) {
		if (!api->LoadMessage("HangmanHistoryScore", buf2, sizeof(buf))) {
			strcpy(buf2, "You have won %wins% games and lost %losses%.");
		}
		str_replace(buf2, sizeof(buf2), "%wins%", x->second.find("Wins")->second.c_str());
		str_replace(buf2, sizeof(buf2), "%losses%", x->second.find("Losses")->second.c_str());
		str_replace(buf2, sizeof(buf2), "%nick", x->second.find("Nick")->second.c_str());
		ut->std_reply(ut, buf2);
	}
}

int Hangman_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
//	char buf[1024];

	if (!stricmp(command, "hangman-start")) {
		AutoMutex(gameInstMutex);
		HANGMANGAMEINST * i = GetInstance(ut);
		if (i && i->game) {
			if (i->game->GetStatus() == UGS_STOPPED || i->game->GetStatus() == UGS_END) {
				i->game->Start(ut);
			} else {
				ut->std_reply(ut, _("There is already a game in progress..."));
			}
		} else {
			i = CreateInstance(ut);
			if (i && i->game) {
				i->game->Start(ut);
			} else {
				ut->std_reply(ut, _("Error creating game instance!"));
			}
		}
		return 0;
	}

	if (!stricmp(command, "hangman-stop")) {
		gameInstMutex.Lock();
		HANGMANGAMEINST * i = GetInstance(ut);
		if (i) {
			ut->std_reply(ut, _("Stopping game..."));
			DeleteInstance(ut);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "letter")) {
		gameInstMutex.Lock();
		HANGMANGAMEINST * i = GetInstance(ut);
		if (i && i->game && i->game->GetStatus() == UGS_PLAYING) {
			i->game->Play(ut, parms);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	/*
	if (!stricmp(command, "hangman-curscore")) {
		gameInstMutex.Lock();
		HANGMANGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() >= UGS_GETTING_PLAYERS) {
			i->game->ShowScores();
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "hangman-points")) {
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
				ut->std_reply(ut, _("Usage: !hangman-points netno channel"));
			}
		} else {
			ut->std_reply(ut, _("You must specify a netno and channel!"));
		}
	}
	*/

	if (!stricmp(command, "hangman-score")) {
		ShowScores(ut);
		return 1;
	}

	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	//api->db->Query("CREATE TABLE IF NOT EXISTS HangmanGames (ID INTEGER PRIMARY KEY AUTOINCREMENT, NetNo INT DEFAULT 0, Channel VARCHAR(255));", NULL, NULL, NULL);
	//api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS HangmanGamesUnique ON HangmanGames (NetNo, Channel);", NULL, NULL, NULL);
	//api->db->Query("CREATE TABLE IF NOT EXISTS HangmanConfig (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INT, Name VARCHAR(255), Value VARCHAR(255));", NULL, NULL, NULL);
	//api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS HangmanConfigUnique ON HangmanConfig (GameID, Name);", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS HangmanStats (ID INTEGER PRIMARY KEY AUTOINCREMENT, Nick VARCHAR(255), Losses INT DEFAULT 0, Wins INT DEFAULT 0);", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS HangmanStatsUnique ON HangmanStats (Nick);", NULL, NULL, NULL);
	LoadConfig();
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "hangman-start",		&acl, CM_ALLOW_ALL_PRIVATE|CM_ALLOW_ALL_PUBLIC, Hangman_Commands, _("Starts a game of Hangman"));
	api->commands->RegisterCommand_Proc(pluginnum, "hangman-stop",		&acl, CM_ALLOW_ALL_PRIVATE|CM_ALLOW_ALL_PUBLIC, Hangman_Commands, _("Stops a game of Hangman in progress early (counts as a loss if you have played any letters in the current word)"));
	api->commands->RegisterCommand_Proc(pluginnum, "letter",			&acl, CM_ALLOW_ALL_PRIVATE|CM_ALLOW_ALL_PUBLIC, Hangman_Commands, _("Tries a letter for the current word in Hangman"));
	api->commands->RegisterCommand_Proc(pluginnum, "hangman-score",		&acl, CM_ALLOW_ALL_PRIVATE|CM_ALLOW_ALL_PUBLIC, Hangman_Commands, _("Shows you your wins and losses in Hangman"));


//	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ, 0);
	//api->commands->RegisterCommand_Proc(pluginnum, "hangman-curscore",	&acl, CM_ALLOW_IRC_PUBLIC, Hangman_Commands, _("Shows the score of the current game to the game's channel"));
	TT_StartThread(GameThread, NULL, _("Hangman GameThread"), pluginnum);

	return 1;
}

void quit() {
	api->ib_printf(_("Hangman Plugin -> Shutting down...\n"));
	shutdown_now=true;
	api->commands->UnregisterCommandsByPlugin(pluginnum);
	while (TT_NumThreadsWithID(pluginnum)) {
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}
	DestroyAllGames();
	api->ib_printf(_("Hangman Plugin -> Shutdown complete\n"));
}

int message(unsigned int MsgID, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{98FA0E34-B951-4943-9C93-09FEC2D34A88}",
	"Hangman",
	"Hangman Plugin 1.0",
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
		for (hangmanGameList::iterator x = hangmanGames.begin(); x != hangmanGames.end(); x++) {
			if (x->game) {
				if (x->game->GetStatus() != UGS_STOPPED) {
					x->game->Work();
				}
			}
		}
		for (hangmanGameList::iterator x = hangmanGames.begin(); x != hangmanGames.end(); x++) {
			if (x->game && x->game->GetStatus() == UGS_END) {
				delete x->game;
				hangmanGames.erase(x);
				break;
			}
		}
		gameInstMutex.Release();
		api->safe_sleep_seconds(1);
	}

	TT_THREAD_END
}

