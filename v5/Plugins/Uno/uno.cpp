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

#include "uno.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned
bool shutdown_now = false;
THREADTYPE GameThread(void *lpData);

Titus_Mutex gameInstMutex;
UNO_CONFIG uno_config;

class UNOGAMEINST {
public:
	int netno;
	string chan;
	UnoGame * game;
};
typedef std::vector<UNOGAMEINST> unoGameList;
unoGameList unoGames;

UNOGAMEINST * GetInstance(int netno, string chan) {
	UNOGAMEINST * ret = NULL;
	gameInstMutex.Lock();
	for (unoGameList::iterator x = unoGames.begin(); x != unoGames.end(); x++) {
		if (x->netno == netno && !stricmp(x->chan.c_str(),chan.c_str())) {
			ret = &(*x);
			break;
		}
	}
	gameInstMutex.Release();
	return ret;
}

UNOGAMEINST * CreateInstance(int netno, string chan) {
	UNOGAMEINST tmp;
	tmp.netno = netno;
	tmp.chan = chan;
	tmp.game = new UnoGame();

	gameInstMutex.Lock();
	unoGames.push_back(tmp);
	gameInstMutex.Release();
	return GetInstance(netno, chan);
}

void DeleteInstance(int netno, string chan) {
	gameInstMutex.Lock();
	for (unoGameList::iterator x = unoGames.begin(); x != unoGames.end(); x++) {
		if (x->netno == netno && !stricmp(x->chan.c_str(),chan.c_str())) {
			if (x->game) {
				delete x->game;
			}
			unoGames.erase(x);
			break;
		}
	}
	gameInstMutex.Release();
}

void DestroyAllGames() {
	gameInstMutex.Lock();
	for (unoGameList::iterator x = unoGames.begin(); x != unoGames.end(); x = unoGames.begin()) {
		if (x->game) {
			delete x->game;
		}
		unoGames.erase(x);
	}
	gameInstMutex.Release();
}

void LoadConfig() {
	memset(&uno_config, 0, sizeof(uno_config));

	uno_config.GameMode = UGM_STANDARD;
	uno_config.TurnTimeout = 60;
	uno_config.AllowStacking = true;

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Uno");
	if (sec) {
		char * p = api->config->GetConfigSectionValue(sec, "GameMode");
		if (p) {
			if (!stricmp(p, "onehand")) {
				uno_config.GameMode = UGM_ONE_HAND;
			}
		}
		long n = api->config->GetConfigSectionLong(sec, "TurnTimeout");
		if (n > 0) { uno_config.TurnTimeout = n; }
		n = api->config->GetConfigSectionLong(sec, "AllowStacking");
		if (n == 0) { uno_config.AllowStacking = false; }
		n = api->config->GetConfigSectionLong(sec, "ColorCode");
		if (n > 0) { uno_config.ColorCode = true; }
	} else {
		api->ib_printf(_("Uno -> No 'Uno' section, using defaults...\n"));
	}
}

void ShowScores(USER_PRESENCE * ut, USER_PRESENCE::userPresSendFunc method, string sort_by="Points", int limit=10, int netno=-1, string chan="") {
	stringstream str;
	if (netno >= 0 && chan.length()) {
		str << "SELECT * FROM UnoChanStats WHERE NetNo=" << netno << " AND Channel=\"" << chan << "\" ORDER BY " << sort_by << " DESC LIMIT " << limit;
	} else {
		str << "SELECT * FROM UnoStats ORDER BY " << sort_by << " DESC LIMIT " << limit;
	}
	sql_rows ret = GetTable(str.str().c_str());

	char buf[1024],buf2[1024];
	if (ret.size() == 0) {
		if (!api->LoadMessage("UnoHistoryNone", buf2, sizeof(buf))) {
			sprintf(buf2, "Sorry, there is no score history.");
		}
		str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
		api->ProcText(buf2, sizeof(buf2));
		//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
		//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
		method(ut, buf2);
		return;
	}

	if (!api->LoadMessage("UnoHistoryBegin", buf2, sizeof(buf))) {
		strcpy(buf2, "Uno Top Players");
	}
	str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
	api->ProcText(buf2, sizeof(buf2));
	//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
	//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
	method(ut, buf2);
	if (!api->LoadMessage("UnoHistoryBegin2", buf2, sizeof(buf))) {
		strcpy(buf2, "Player | Wins | Points");
	}
	str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
	api->ProcText(buf2, sizeof(buf2));
	method(ut, buf2);

	int rank = 0;
	int lastscore = 0;
	for (sql_rows::const_iterator x = ret.begin(); x != ret.end(); x++) {
		int points = (stricmp(sort_by.c_str(),"Wins") == 0) ? atoi(x->second.find("Wins")->second.c_str()) : atoi(x->second.find("Points")->second.c_str());
		if (points != lastscore) {
			//api->ib_printf("%d %d %d\n", rank, lastscore, points);
			rank++;
			lastscore = points;
		}

		if (!api->LoadMessage("UnoHistoryScore", buf2, sizeof(buf))) {
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

	if (!api->LoadMessage("UnoHistoryEnd", buf2, sizeof(buf))) {
		strcpy(buf2, "End of list.");
	}
	str_replace(buf2, sizeof(buf2), "%chan", chan.c_str());
	api->ProcText(buf2, sizeof(buf2));
	method(ut, buf2);
	//sprintf(buf, "PRIVMSG %s :%s\r\n", destchan.c_str(), buf2);
	//api->SendIRC_Priority(destnetno, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

int Uno_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
//	char buf[1024];

	if (!stricmp(command, "uno-start")) {
		AutoMutex(gameInstMutex);
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game) {
			if (i->game->GetStatus() == UGS_STOPPED) {
				i->game->Prepare(ut);
			} else if (i->game->GetStatus() == UGS_GETTING_PLAYERS) {
				i->game->Start(ut);
			} else {
				ut->std_reply(ut, _("There is already a game in progress..."));
			}
		} else {
			i = CreateInstance(ut->NetNo, ut->Channel);
			if (i && i->game) {
				i->game->Prepare(ut);
			} else {
				ut->std_reply(ut, _("Error creating game instance!"));
			}
		}
		return 0;
	}

	if (!stricmp(command, "uno")) {
		AutoMutex(gameInstMutex);
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() == UGS_GETTING_PLAYERS) {
			i->game->AddPlayer(ut);
		} else {
			ut->send_notice(ut, _("There is not a game in progress or the game has already started!"));
		}
		return 0;
	}

	if (!stricmp(command, "uno-stop")) {
		gameInstMutex.Lock();
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i) {
			ut->send_notice(ut, _("Stopping game..."));
			DeleteInstance(ut->NetNo, ut->Channel);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "play")) {
		gameInstMutex.Lock();
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() == UGS_PLAYER_TURN) {
			i->game->Play(ut, parms);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "pass")) {
		gameInstMutex.Lock();
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() == UGS_PLAYER_TURN) {
			i->game->Pass(ut);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "draw")) {
		gameInstMutex.Lock();
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() == UGS_PLAYER_TURN) {
			i->game->Draw(ut, false);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "hand")) {
		gameInstMutex.Lock();
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() == UGS_PLAYER_TURN) {
			i->game->ShowHand(ut);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "topcard")) {
		gameInstMutex.Lock();
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() == UGS_PLAYER_TURN) {
			i->game->ShowTopCard(ut);
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "uno-curscore")) {
		gameInstMutex.Lock();
		UNOGAMEINST * i = GetInstance(ut->NetNo, ut->Channel);
		if (i && i->game && i->game->GetStatus() >= UGS_GETTING_PLAYERS) {
			i->game->ShowScores();
		} else {
			ut->std_reply(ut, _("There is no game in progress!"));
		}
		gameInstMutex.Release();
	}

	if (!stricmp(command, "uno-points")) {
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
				ut->std_reply(ut, _("Usage: !uno-points netno channel"));
			}
		} else {
			ut->std_reply(ut, _("You must specify a netno and channel!"));
		}
	}

	if (!stricmp(command, "uno-wins")) {
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
				ut->std_reply(ut, _("Usage: !uno-wins netno channel"));
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

	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Uno Plugin -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	//api->db->Query("CREATE TABLE IF NOT EXISTS UnoGames (ID INTEGER PRIMARY KEY AUTOINCREMENT, NetNo INT DEFAULT 0, Channel VARCHAR(255));", NULL, NULL, NULL);
	//api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS UnoGamesUnique ON UnoGames (NetNo, Channel);", NULL, NULL, NULL);
	//api->db->Query("CREATE TABLE IF NOT EXISTS UnoConfig (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INT, Name VARCHAR(255), Value VARCHAR(255));", NULL, NULL, NULL);
	//api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS UnoConfigUnique ON UnoConfig (GameID, Name);", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS UnoStats (ID INTEGER PRIMARY KEY AUTOINCREMENT, Nick VARCHAR(255), Points INT DEFAULT 0, Wins INT DEFAULT 0);", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS UnoStatsUnique ON UnoStats (Nick);", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS UnoChanStats (ID INTEGER PRIMARY KEY AUTOINCREMENT, NetNo INT DEFAULT 0, Channel VARCHAR(255), Nick VARCHAR(255), Points INT DEFAULT 0, Wins INT DEFAULT 0);", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS UnoChanStatsUnique ON UnoChanStats (NetNo, Channel, Nick);", NULL, NULL, NULL);
	LoadConfig();
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "uno-start",		&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Starts a game of Uno"));
	api->commands->RegisterCommand_Proc(pluginnum, "uno",			&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Adds you to a game of Uno, and used when you have only 1 card left"));
	api->commands->RegisterCommand_Proc(pluginnum, "play",			&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Plays a card from your hand in Uno"));
	api->commands->RegisterCommand_Proc(pluginnum, "draw",			&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Draws a card from the deck in Uno"));
	api->commands->RegisterCommand_Proc(pluginnum, "hand",			&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Shows you your current hand in Uno"));
	api->commands->RegisterCommand_Proc(pluginnum, "topcard",			&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Shows you the current top card in Uno"));
	api->commands->RegisterCommand_Proc(pluginnum, "pass",			&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("End your turn and go to the next player in Uno"));
	api->commands->RegisterCommand_Proc(pluginnum, "uno-points",	&acl, CM_ALLOW_ALL, Uno_Commands, _("Shows the (up to) top 10 Uno players by number of points"));
	api->commands->RegisterCommand_Proc(pluginnum, "uno-wins",		&acl, CM_ALLOW_ALL, Uno_Commands, _("Shows the (up to) top 10 Uno players by number of game wins"));

	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "uno-stop",		&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Stops a game of Uno in progress early (forfeiting points/wins)"));

	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "uno-curscore",	&acl, CM_ALLOW_IRC_PUBLIC, Uno_Commands, _("Shows the score of the current game to the game's channel"));
	TT_StartThread(GameThread, NULL, _("Uno GameThread"), pluginnum);

	return 1;
}

void quit() {
	api->ib_printf(_("Uno Plugin -> Shutting down...\n"));
	shutdown_now=true;
	api->commands->UnregisterCommandsByPlugin(pluginnum);
	while (TT_NumThreadsWithID(pluginnum)) {
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}
	DestroyAllGames();
	api->ib_printf(_("Uno Plugin -> Shutdown complete\n"));
}

int message(unsigned int MsgID, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{79FA4903-E125-443D-801E-91057C2C3492}",
	"Uno",
	"Uno Plugin 1.0",
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
		for (unoGameList::iterator x = unoGames.begin(); x != unoGames.end(); x++) {
			if (x->game) {
				if (x->game->GetStatus() != UGS_STOPPED) {
					x->game->Work();
				}
			}
		}
		gameInstMutex.Release();
		api->safe_sleep_seconds(1);
	}

	TT_THREAD_END
}

