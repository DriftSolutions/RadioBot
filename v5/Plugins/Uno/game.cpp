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
//#include <cctype>

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

static char uno_numword[][6] = {
	"zero",
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine"
};

void Deck::Reset() {
	colorCode = false;
	int ind=0;
	int j;
	for (int i=0; i < 4; i++) {
		cards[ind].card = UC_NUM_0;
		cards[ind].color = (UNO_COLORS)i;
		cards[ind].player = -1;
		cards[ind].state = UC_INPILE;
		ind++;
		for (j=1; j <= 9; j++) {
			cards[ind].card = (UNO_CARDS)(UC_NUM_0+j);
			cards[ind].color = (UNO_COLORS)i;
			cards[ind].player = -1;
			cards[ind].state = UC_INPILE;
			ind++;
			cards[ind].card = (UNO_CARDS)(UC_NUM_0+j);
			cards[ind].color = (UNO_COLORS)i;
			cards[ind].player = -1;
			cards[ind].state = UC_INPILE;
			ind++;
		}
		for (j=0; j < 2; j++) {
			cards[ind].card = UC_DRAW_TWO;
			cards[ind].color = (UNO_COLORS)i;
			cards[ind].player = -1;
			cards[ind].state = UC_INPILE;
			ind++;
			cards[ind].card = UC_SKIP;
			cards[ind].color = (UNO_COLORS)i;
			cards[ind].player = -1;
			cards[ind].state = UC_INPILE;
			ind++;
			cards[ind].card = UC_REVERSE;
			cards[ind].color = (UNO_COLORS)i;
			cards[ind].player = -1;
			cards[ind].state = UC_INPILE;
			ind++;
		}
	}
	for (j=0; j < 4; j++) {
		cards[ind].card = UC_WILD;
		cards[ind].color = UCC_WILD;
		cards[ind].player = -1;
		cards[ind].state = UC_INPILE;
		ind++;
		cards[ind].card = UC_WILD_DRAW_FOUR;
		cards[ind].color = UCC_WILD;
		cards[ind].player = -1;
		cards[ind].state = UC_INPILE;
		ind++;
	}
/*
	FILE * fp = fopen("uno-deck.txt", "wb");
	if (fp) {
		for (int i=0; i < UNO_NUM_CARDS; i++) {
			fprintf(fp, "%d %d %d - %s\r\n", i, cards[i].color, int(cards[i].card), GetCardName(&cards[i]).c_str());
		}
		fclose(fp);
	}
	*/
}

string Deck::GetColorName(UNO_COLORS col) {
	switch (col) {
		case UCC_RED:
			return "red";
			break;
		case UCC_BLUE:
			return "blue";
			break;
		case UCC_GREEN:
			return "green";
			break;
		case UCC_YELLOW:
			return "yellow";
			break;
		case UCC_WILD:
			return "wild";
			break;
		default:
			return "unknown";
			break;
	}
}

string Deck::GetCardName(Card * c) {
	stringstream ret;
	switch (c->color) {
		case UCC_RED:
			if (colorCode) { ret << "\00304"; }
			ret << "red";
			break;
		case UCC_BLUE:
			if (colorCode) { ret << "\00312"; }
			ret << "blue";
			break;
		case UCC_GREEN:
			if (colorCode) { ret << "\00303"; }
			ret << "green";
			break;
		case UCC_YELLOW:
			if (colorCode) { ret << "\00308"; }
			ret << "yellow";
			break;
		case UCC_WILD:
			ret << "wild";
			break;
		default:
			ret << "unknown";
			break;
	}
	if (c->card >= UC_NUM_0 && c->card <= UC_NUM_9) {
		ret << " " << c->card;
	} else if (c->card == UC_SKIP) {
		ret << " skip";
	} else if (c->card == UC_REVERSE) {
		ret << " reverse";
	} else if (c->card == UC_DRAW_TWO) {
		ret << " draw 2";
	} else if (c->card == UC_WILD) {
		//ret << "";
	} else if (c->card == UC_WILD_DRAW_FOUR) {
		ret << " draw 4";
	}
	if (colorCode) { ret << "\003"; }
	return ret.str();
}

string Deck::GetShortCardName(Card * c) {
	stringstream ret;
	switch (c->color) {
		case UCC_RED:
			if (colorCode) { ret << "\00304"; }
			ret << "R";
			break;
		case UCC_BLUE:
			if (colorCode) { ret << "\00312"; }
			ret << "B";
			break;
		case UCC_GREEN:
			if (colorCode) { ret << "\00303"; }
			ret << "G";
			break;
		case UCC_YELLOW:
			if (colorCode) { ret << "\00308"; }
			ret << "Y";
			break;
		case UCC_WILD:
			ret << "W";
			break;
		default:
			ret << "unknown";
			break;
	}
	if (c->card >= UC_NUM_0 && c->card <= UC_NUM_9) {
		ret << c->card;
	} else if (c->card == UC_SKIP) {
		ret << "S";
	} else if (c->card == UC_REVERSE) {
		ret << "R";
	} else if (c->card == UC_DRAW_TWO) {
		ret << "D2";
	} else if (c->card == UC_WILD) {
		//ret << "";
	} else if (c->card == UC_WILD_DRAW_FOUR) {
		ret << "D4";
	}
	if (colorCode) { ret << "\003"; }
	return ret.str();
}

Card * Deck::getCard(int i) {
	if (i >= 0 && i < UNO_NUM_CARDS) {
		return &cards[i];
	}
	return NULL;
}

int Deck::calculateWinnerPoints() {
	int ret = 0;
	for (int i=0; i < UNO_NUM_CARDS; i++) {
		if (cards[i].state == UC_INHAND) {
			if (cards[i].card >= UC_NUM_0 && cards[i].card <= UC_NUM_9) {
				ret += cards[i].card;
			} else if (cards[i].card == UC_WILD || cards[i].card == UC_WILD_DRAW_FOUR) {
				ret += 50;
			} else {
				ret += 20;
			}
		}
	}
	return ret;
}

Card * Deck::getCard(UNO_CARDS card, UNO_COLORS col) {
	for (int i=0; i < UNO_NUM_CARDS; i++) {
		if (cards[i].card == card && cards[i].color == col) {
			return &cards[i];
		}
	}
	return NULL;
}

Card * Deck::getCardOwnedBy(UNO_CARDS card, UNO_COLORS col, int player) {
	for (int i=0; i < UNO_NUM_CARDS; i++) {
		if (cards[i].card == card && cards[i].color == col && cards[i].state == UC_INHAND && cards[i].player == player) {
			return &cards[i];
		}
	}
	return NULL;
}

Card * Deck::getRandomCard(int * ind) {
	int orig, num_dis = 0;
	int i = orig = (api->genrand_int32()%(UNO_NUM_CARDS*4))/4;
	if (cards[i].state == UC_INPILE) {
		if (ind) { *ind = i; }
		return &cards[i];
	} else if (cards[i].state == UC_DISCARDED) {
		num_dis++;
	}
	i++;
	while (i < UNO_NUM_CARDS) {
		if (cards[i].state == UC_INPILE) {
			if (ind) { *ind = i; }
			return &cards[i];
		} else if (cards[i].state == UC_DISCARDED) {
			num_dis++;
		}
		i++;
	}
	i = orig-1;
	while (i >= 0) {
		if (cards[i].state == UC_INPILE) {
			if (ind) { *ind = i; }
			return &cards[i];
		} else if (cards[i].state == UC_DISCARDED) {
			num_dis++;
		}
		i--;
	}
	if (num_dis > 0) {
		for (i=0; i < UNO_NUM_CARDS; i++) {
			if (cards[i].state == UC_DISCARDED) {
				cards[i].state = UC_INPILE;
			}
		}
		return getRandomCard(ind);
	}
	return NULL;
}

Card * Deck::getStartingCard(int * ind) {
	Card * c = getRandomCard(ind);
	while (c->color == UCC_WILD) {
		c = getRandomCard(ind);
	}
	return c;
}

UnoGame::UnoGame() {
	status = UGS_STOPPED;
	memset(&GameOptions, 0, sizeof(GameOptions));
	GameOptions.TurnTimeout = uno_config.TurnTimeout;
	GameOptions.GameMode = uno_config.GameMode;
	GameOptions.AllowStacking = uno_config.AllowStacking;
	GameOptions.ColorCode = uno_config.ColorCode;
	memset(&GameVars, 0, sizeof(GameVars));
}

UnoGame::~UnoGame() {
	Stop();
	//UnloadQuestions();
}

void UnoGame::Prepare(USER_PRESENCE * ut) {
	char buf[1024],buf2[1024];

	Stop();

	ut->send_notice(ut, _("Starting game..."));
	NetNo = ut->NetNo;
	Channel = ut->Channel;
	GameStarter = ut->User ? ut->User->Nick:ut->Nick;
	if (!api->LoadMessage("UnoAnnounce", buf2, sizeof(buf))) {
		sstrcpy(buf2, "%nick has started Uno! Type !uno if you would like to play...");
	}
	str_replace(buf2, sizeof(buf2), "%nick", ut->Nick);
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	//GameVars.timeGameStarted = time(NULL);
	status = UGS_GETTING_PLAYERS;
	AddPlayer(ut);

	//auto-game for debug
	//AddPlayer(ut);
	//Start(ut);
}

void UnoGame::AddPlayer(USER_PRESENCE * ut) {
	if (status == UGS_GETTING_PLAYERS) {
		ut->send_notice(ut, _("You have been added to the game..."));
		PlayerInfo pi;
		pi.acct = ut->User ? ut->User->Nick:ut->Nick;
		pi.nick = ut->Nick;
		players.push_back(pi);
	} else {
		ut->send_notice(ut, _("The game is not in the current state to accept new players..."));
	}
}

void UnoGame::newDeal(bool first) {
	char buf[1024],buf2[1024];

	deck.Reset();
	deck.colorCode = GameOptions.ColorCode;
	GameVars.curPlayer = (api->genrand_int32()%(players.size()*5))/5;
	GameVars.pileCard = deck.getStartingCard();
	GameVars.pileCard->state = UC_DISCARDED;
	GameVars.pileColor = GameVars.pileCard->color;
	GameVars.direction = 0;
	GameVars.drawCount = GameVars.skipCount = 0;

	if (first) {
		if (!api->LoadMessage("UnoStart", buf2, sizeof(buf))) {
			strcpy(buf2, "Uno is now starting with %num players...");
		}
	} else {
		if (!api->LoadMessage("UnoNewDeal", buf2, sizeof(buf))) {
			strcpy(buf2, "Dealing a new hand to players...");
		}
	}
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	sprintf(buf, "%u", players.size());
	str_replace(buf2, sizeof(buf2), "%num", buf);
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	if (!api->LoadMessage("UnoStart2", buf2, sizeof(buf))) {
		strcpy(buf2, "I have selected '%card' to start the discard pile...");
	}
	str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	str_replace(buf2, sizeof(buf2), "%card", deck.GetCardName(GameVars.pileCard).c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	for (size_t j=0; j < players.size(); j++) {
		players[j].num_cards = 0;
		for (int i=0; i < 7; i++) {
			Card * c = deck.getRandomCard();
			if (c) {
				c->player = j;
				c->state = UC_INHAND;
				players[j].num_cards++;
			} else {
				break;
			}
		}
		if (j != GameVars.curPlayer) {
			ShowHand(j);
		}
	}
	doTurn();

}

void UnoGame::Start(USER_PRESENCE * ut) {

	const char * acct = ut->User ? ut->User->Nick:ut->Nick;
	if (!api->user->uflag_have_any_of(ut->Flags, UFLAG_MASTER|UFLAG_OP) && stricmp(acct, GameStarter.c_str())) {
		ut->send_notice(ut, "Only the person who typed !uno-start can start the game!");
		return;
	}
	if (players.size() < 2) {
		ut->send_notice(ut, "There needs to be at least 2 players to start a game...");
		return;
	}

	GameVars.timeGameStarted = time(NULL);
	newDeal(true);
}

void UnoGame::gameWon() {
	char buf[1024],buf2[1024];

	//int points =
	players[GameVars.curPlayer].points += deck.calculateWinnerPoints();//points;

	if (GameOptions.GameMode == UGM_ONE_HAND || players[GameVars.curPlayer].points >= 500) {
		if (!api->LoadMessage("UnoWonGame", buf2, sizeof(buf))) {
			strcpy(buf2, "Game over! This round of Uno was won by %nick with %points points.");
		}
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		sprintf(buf, "%d", players[GameVars.curPlayer].points);
		str_replace(buf2, sizeof(buf2), "%points", buf);
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

		addPlayerWin(players[GameVars.curPlayer].acct);
		for (playerMap::iterator x = players.begin(); x != players.end(); x++) {
			addPlayerPoints(x->acct, x->points);
		}

		ShowScores();

		status = UGS_STOPPED;
		Stop();
	} else {
		if (!api->LoadMessage("UnoWonHand", buf2, sizeof(buf))) {
			strcpy(buf2, "%nick is out of cards, he wins this hand...");
		}
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		sprintf(buf, "%d", players[GameVars.curPlayer].points);
		str_replace(buf2, sizeof(buf2), "%points", buf);
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

		ShowScores();
		newDeal(false);
	}

}

void UnoGame::Stop() {
	char buf[1024],buf2[1024];

	if (status != UGS_STOPPED) {
		if (!api->LoadMessage("UnoOver", buf2, sizeof(buf))) {
			strcpy(buf2, "Uno has ended, we hope you enjoyed yourself!");
		}
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
	}

	if (players.size()) {
		players.clear();
	}
	deck.Reset();
	status = UGS_STOPPED;
	memset(&GameVars, 0, sizeof(GameVars));
}

void UnoGame::ShowHand(int player) {
	char buf[1024],buf2[1024];

	stringstream out;
	for (int j=0; j < UNO_NUM_CARDS; j++) {
		Card * c = deck.getCard(j);
		if (c && c->state == UC_INHAND && c->player == player) {
			if (players[player].num_cards <= 7) {
				out << " " << deck.GetCardName(c);
			} else {
				out << " " << deck.GetShortCardName(c);
			}
		}
	}

	if (!api->LoadMessage("UnoShowHand", buf2, sizeof(buf))) {
		sstrcpy(buf2, "Your Hand (%num cards): %hand");
	}
	str_replace(buf2, sizeof(buf2), "%nick", players[player].nick.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	sprintf(buf, "%d", players[player].num_cards);
	str_replace(buf2, sizeof(buf2), "%num", buf);
	str_replace(buf2, sizeof(buf2), "%hand", out.str().c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "NOTICE %s :%s\r\n", players[player].nick.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

void UnoGame::doTurn() {
	char buf[1024],buf2[1024];

	if (!api->LoadMessage("UnoTurnStart", buf2, sizeof(buf))) {
		sstrcpy(buf2, "%nick's turn has now begun...");
	}
	str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	ShowHand(GameVars.curPlayer);
	players[GameVars.curPlayer].didDraw = false;
	players[GameVars.curPlayer].drawCard = NULL;

	GameVars.timeTurnStarted = time(NULL);
	status = UGS_PLAYER_TURN;
}

void UnoGame::nextTurn() {
	char buf[1024],buf2[1024];

	players[GameVars.curPlayer].didDraw = false;
	players[GameVars.curPlayer].drawCard = NULL;

	if (GameVars.direction == 0) {
		GameVars.curPlayer--;
		if (GameVars.curPlayer < 0) { GameVars.curPlayer = players.size()-1; }
	} else {
		GameVars.curPlayer++;
		if (GameVars.curPlayer >= players.size()) { GameVars.curPlayer = 0; }
	}

	if (GameVars.drawCount) {
		if (!api->LoadMessage("UnoDrawCards", buf2, sizeof(buf))) {
			sstrcpy(buf2, "%nick has to draw %num cards and lose a turn...");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		sprintf(buf, "%d", GameVars.drawCount);
		str_replace(buf2, sizeof(buf2), "%num", buf);
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
		for (int i=0; i < GameVars.drawCount; i++) {
			giveCard(GameVars.curPlayer, deck.getRandomCard());
		}
		GameVars.drawCount = 0;
		nextTurn();
		return;
	}

	if (GameVars.skipCount) {
		if (!api->LoadMessage("UnoSkipTurn", buf2, sizeof(buf))) {
			sstrcpy(buf2, "%nick's turn was skipped...");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
		GameVars.skipCount--;
		nextTurn();
		return;
	}

	doTurn();
}

void UnoGame::TimeoutTurn() {
	char buf[1024],buf2[1024];

	if (!api->LoadMessage("UnoTimeout", buf2, sizeof(buf))) {
		sstrcpy(buf2, "%nick's turn has timed out, drawing card...");
	}
	str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	giveCard(GameVars.curPlayer, deck.getRandomCard());
	nextTurn();
}

void UnoGame::Draw(USER_PRESENCE * ut, bool in_pass) {
	char buf[1024],buf2[1024];

	const char * acct = ut->User ? ut->User->Nick:ut->Nick;
	if (!stricmp(acct, players[GameVars.curPlayer].acct.c_str())) {
		GameVars.timeTurnStarted = time(NULL);
		if (!api->LoadMessage("UnoDraw", buf2, sizeof(buf))) {
			sstrcpy(buf2, "%nick has drawn a card...");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

		Card * c = deck.getRandomCard();
		if (c) {
			giveCard(GameVars.curPlayer, c);
			players[GameVars.curPlayer].didDraw = true;
			players[GameVars.curPlayer].drawCard = c;

			if (!in_pass) {
				if (!api->LoadMessage("UnoDrawPlayer", buf2, sizeof(buf))) {
					sstrcpy(buf2, "You may now !play the card you have drawn or use !pass to go to the next player...");
				}
				str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
				str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
				api->ProcText(buf2, sizeof(buf2));
				ut->send_notice(ut, buf2);
			}
		}
	} else {
		ut->send_notice(ut, "It is not your turn...");
	}
}

void UnoGame::ShowHand(USER_PRESENCE * ut) {
	const char * acct = ut->User ? ut->User->Nick:ut->Nick;
	for (size_t i=0; i < players.size(); i++) {
		if (!stricmp(acct, players[i].acct.c_str())) {
			ShowHand(i);
			break;
		}
	}
	ut->std_reply(ut, "You are not in this match!");
}

void UnoGame::ShowTopCard(USER_PRESENCE * ut) {
	char buf[1024],buf2[1024];
	if (GameVars.pileCard != NULL) {
		if (!api->LoadMessage("UnoShowTopCard", buf2, sizeof(buf))) {
			sstrcpy(buf2, "The current top card is: %card");
		}
		str_replace(buf2, sizeof(buf2), "%nick", ut->Nick);
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		str_replace(buf2, sizeof(buf2), "%card", deck.GetCardName(GameVars.pileCard).c_str());
		api->ProcText(buf2, sizeof(buf2));
		ut->std_reply(ut, buf2);
	} else {
		ut->std_reply(ut, "There is no top card currently!");
	}
}

void UnoGame::Pass(USER_PRESENCE * ut) {
	char buf[1024],buf2[1024];

	const char * acct = ut->User ? ut->User->Nick:ut->Nick;
	if (!stricmp(acct, players[GameVars.curPlayer].acct.c_str())) {
		GameVars.timeTurnStarted = time(NULL);
		if (!players[GameVars.curPlayer].didDraw) {
			Draw(ut, true);
		} else {
			if (!api->LoadMessage("UnoPass", buf2, sizeof(buf))) {
				sstrcpy(buf2, "%nick has ended his turn...");
			}
			str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
			str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
			api->ProcText(buf2, sizeof(buf2));
			sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
			api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
		}

		nextTurn();
	} else {
		ut->send_notice(ut, "It is not your turn...");
	}
}

UNO_COLORS Deck::txtToColor(const char * col) {
	if (!stricmp(col, "red") || !stricmp(col, "r")) {
		return UCC_RED;
	}
	if (!stricmp(col, "green") || !stricmp(col, "g")) {
		return UCC_GREEN;
	}
	if (!stricmp(col, "yellow") || !stricmp(col, "y")) {
		return UCC_YELLOW;
	}
	if (!stricmp(col, "blue") || !stricmp(col, "b")) {
		return UCC_BLUE;
	}
	if (!stricmp(col, "wild") || !stricmp(col, "w")) {
		return UCC_WILD;
	}
	return UCC_ERROR;
}

UNO_CARDS Deck::txtToCard(UNO_COLORS color, const char * card) {
	if (color != UCC_WILD) {
		if (!stricmp(card, "d") || !stricmp(card, "d2") || !stricmp(card, "draw")) {
			return UC_DRAW_TWO;
		}
		if (!stricmp(card, "r") || !stricmp(card, "rev") || !stricmp(card, "reverse")) {
			return UC_REVERSE;
		}
		if (!stricmp(card, "s") || !stricmp(card, "skip")) {
			return UC_SKIP;
		}
		char buf[2] = {0,0};
		for (int i=0; i <= 9; i++) {
			buf[0] = '0'+i;
			if (!stricmp(card, buf) || !stricmp(card, uno_numword[i])) {
				return (UNO_CARDS)i;
			}
		}
	} else {
		if (!stricmp(card, "d") || !stricmp(card, "d4") || !stricmp(card, "draw")) {
			return UC_WILD_DRAW_FOUR;
		}
		return UC_WILD;
	}
	return UC_ERROR;
}

bool UnoGame::canPlayCard(UNO_CARDS card, UNO_COLORS col) {
	if (col == UCC_WILD) { return true; }
	if (col == GameVars.pileColor) { return true; }
	if (card == GameVars.pileCard->card) { return true; }
	return false;
}

typedef std::vector<CardPlay> cardPlayList;

bool UnoGame::playCard(USER_PRESENCE * ut, const char * acct, CardPlay * cp) {
	char buf[1024],buf2[1024];
	Card * c = deck.getCardOwnedBy(cp->card, cp->col, GameVars.curPlayer);
	if (c == NULL) {
		c = deck.getCard(cp->card, cp->col);
		if (!api->LoadMessage("UnoCardNotInHand", buf2, sizeof(buf))) {
			sstrcpy(buf2, "You do not have a '%card' in your hand!");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		str_replace(buf2, sizeof(buf2), "%card", c ? deck.GetCardName(c).c_str():"unknown card");
		api->ProcText(buf2, sizeof(buf2));
		ut->send_notice(ut, buf2);
		return false;
	}
	if (!canPlayCard(cp->card, cp->col)) {
		if (!api->LoadMessage("UnoCardNotAllowed", buf2, sizeof(buf))) {
			sstrcpy(buf2, "'%card' can not be played right now!");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		str_replace(buf2, sizeof(buf2), "%card", deck.GetCardName(c).c_str());
		api->ProcText(buf2, sizeof(buf2));
		ut->send_notice(ut, buf2);
		return false;
	}

	GameVars.pileCard = c;
	GameVars.pileColor = c->color;
	c->player = -1;
	c->state = UC_DISCARDED;
	players[GameVars.curPlayer].num_cards--;

	if (cp->card == UC_WILD) {
		GameVars.pileColor = cp->excol;
	} else if (cp->card == UC_WILD_DRAW_FOUR) {
		GameVars.pileColor = cp->excol;
		GameVars.drawCount += 4;
	} else if (cp->card == UC_DRAW_TWO) {
		GameVars.drawCount += 2;
	} else if (cp->card == UC_SKIP || (cp->card == UC_REVERSE && players.size() == 2)) {
		GameVars.skipCount++;
	} else if (cp->card == UC_REVERSE) {
		GameVars.direction = GameVars.direction ? 0:1;
	} else if (cp->card >= UC_NUM_0 && cp->card <= UC_NUM_9) {
	} else {
		api->ib_printf("Uno -> Error: unhandled card %d!\n", cp->card);
		return false;
	}

	if (cp->col == UCC_WILD) {
		if (!api->LoadMessage("UnoPlayedCardWild", buf2, sizeof(buf))) {
			sstrcpy(buf2, "%nick played '%card', setting new color to '%pilecol'");
		}
	} else {
		if (!api->LoadMessage("UnoPlayedCard", buf2, sizeof(buf))) {
			sstrcpy(buf2, "%nick played '%card'");
		}
	}
	str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	str_replace(buf2, sizeof(buf2), "%card", deck.GetCardName(c).c_str());
	str_replace(buf2, sizeof(buf2), "%pilecol", deck.GetColorName(GameVars.pileColor).c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	if (players[GameVars.curPlayer].num_cards == 1) {
		if (!api->LoadMessage("UnoUno", buf2, sizeof(buf))) {
			sstrcpy(buf2, "Uno! %nick has 1 card left...");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
	}

	return true;
}

void UnoGame::showColorParseError(USER_PRESENCE * ut, const char * col) {
	char buf[1024],buf2[1024];
	if (!api->LoadMessage("UnoColorError", buf2, sizeof(buf))) {
		sstrcpy(buf2, "Unknown color: %colcode");
	}
	str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	str_replace(buf2, sizeof(buf2), "%colcode", col);
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "NOTICE %s :%s\r\n", players[GameVars.curPlayer].nick.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

void UnoGame::Play(USER_PRESENCE * ut, const char * parms) {
	char buf[1024],buf2[1024];

	const char * acct = ut->User ? ut->User->Nick:ut->Nick;
	if (!stricmp(acct, players[GameVars.curPlayer].acct.c_str())) {
		GameVars.timeTurnStarted = time(NULL);
		if (parms && parms[0]) {
			cardPlayList cpl;
			StrTokenizer st((char *)parms, ' ');
			for (long i=1; i <= st.NumTok()-1; i++) {
				CardPlay cp;
				cp.col = deck.txtToColor(st.stdGetSingleTok(i).c_str());
				if (cp.col != UCC_ERROR) {
					i++;
					cp.card = deck.txtToCard(cp.col, st.stdGetSingleTok(i).c_str());
					if (cp.card == UC_WILD) {
						cp.excol = deck.txtToColor(st.stdGetSingleTok(i).c_str());
						if (cp.excol == UCC_ERROR) {
							showColorParseError(ut, st.stdGetSingleTok(i).c_str());
							return;
						}
					}
					if (cp.card == UC_WILD_DRAW_FOUR) {
						i++;
						cp.excol = deck.txtToColor(st.stdGetSingleTok(i).c_str());
						if (cp.excol == UCC_ERROR) {
							showColorParseError(ut, st.stdGetSingleTok(i).c_str());
							return;
						}
					}
					if (players[GameVars.curPlayer].didDraw && players[GameVars.curPlayer].drawCard) {
						if (cp.card != players[GameVars.curPlayer].drawCard->card || cp.col != players[GameVars.curPlayer].drawCard->color) {
							if (!api->LoadMessage("UnoOnlyDrawnCard", buf2, sizeof(buf))) {
								sstrcpy(buf2, "After !draw'ing a card, you can only play that card or !pass");
							}
							str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
							str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
							api->ProcText(buf2, sizeof(buf2));
							sprintf(buf, "NOTICE %s :%s\r\n", players[GameVars.curPlayer].nick.c_str(), buf2);
							api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
							return;
						}
					}
					cpl.push_back(cp);
					if (!GameOptions.AllowStacking) {
						break;
					}
				} else {
					showColorParseError(ut, st.stdGetSingleTok(i).c_str());
					return;
				}
			}

			bool do_next_turn = true;
			bool did_play = false;
			for (cardPlayList::iterator x = cpl.begin(); status != UGS_STOPPED && x != cpl.end(); x++) {
				if (playCard(ut, acct, &(*x))) {
					did_play = true;
				} else {
					if (!did_play) { return; }
					break;
				}
				if (players[GameVars.curPlayer].num_cards <= 0) {
					gameWon();
					do_next_turn = false;
				}
			}
			if (do_next_turn) {
				nextTurn();
			}
		} else {
			ut->send_notice(ut, "You haven't included any cards to play! (Example: !play red 4)");
		}
	} else {
		ut->send_notice(ut, "It is not your turn...");
	}
}

void UnoGame::giveCard(int player, Card * c) {
	char buf[1024],buf2[1024];
	if (c != NULL) {
		c->player = player;
		c->state = UC_INHAND;
		players[player].num_cards++;

		string cn = deck.GetCardName(c);

		if (!api->LoadMessage("UnoGiveCard", buf2, sizeof(buf))) {
			sstrcpy(buf2, "The card '%card' has been added to your hand...");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[player].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		str_replace(buf2, sizeof(buf2), "%card", cn.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "NOTICE %s :%s\r\n", players[player].nick.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
	} else {
		sprintf(buf, "NOTICE %s :Error drawing a card for you...\r\n", players[player].nick.c_str());
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
	}
}

void UnoGame::Work() {
	if (status == UGS_PLAYER_TURN && (time(NULL) - GameVars.timeTurnStarted) >= GameOptions.TurnTimeout) {
		TimeoutTurn();
	}
}

typedef std::multimap<int, PlayerInfo> scoreList;
void UnoGame::ShowScores() {
	if (players.size() == 0) { return; }

	scoreList scores;
	for (playerMap::const_iterator x = players.begin(); x != players.end(); x++) {
		scores.insert(std::pair<int, PlayerInfo>(x->points, *x));
	}

	char buf[1024],buf2[1024];
	if (!api->LoadMessage("UnoScores", buf2, sizeof(buf))) {
		strcpy(buf2, "Uno Scoreboard");
	}
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	int rank = 0;
	int lastscore = 0;
	for (scoreList::reverse_iterator x = scores.rbegin(); x != scores.rend() && rank <= 10; x++) {
		if (x->second.points != lastscore) {
			rank++;
			lastscore = x->second.points;
		}
		if (rank == 11) { break; }

		if (!api->LoadMessage("UnoScore", buf2, sizeof(buf))) {
			strcpy(buf2, "%nick: %points point(s)");
		}
		sprintf(buf, "%d", x->second.points);
		str_replace(buf2, sizeof(buf2), "%points", buf);
		str_replace(buf2, sizeof(buf2), "%nick", x->second.nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
	}

	if (!api->LoadMessage("UnoScoresEnd", buf2, sizeof(buf))) {
		strcpy(buf2, "End of list.");
	}
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

bool UnoGame::playerHaveChanStats(string acct) {
	stringstream str;
	str << "SELECT * FROM UnoChanStats WHERE NetNo=" << NetNo << " AND Channel=\"" << Channel << "\" AND Nick=\"" << acct << "\"";
	sql_rows ret = GetTable(str.str().c_str());
	return ret.size() > 0 ? true:false;
}
bool UnoGame::playerHaveStats(string acct) {
	stringstream str;
	str << "SELECT * FROM UnoStats WHERE Nick=\"" << acct << "\"";
	sql_rows ret = GetTable(str.str().c_str());
	return ret.size() > 0 ? true:false;
}
void UnoGame::addPlayerPoints(string acct, int points) {
	stringstream str,str2;
	if (playerHaveChanStats(acct)) {
		str << "UPDATE UnoChanStats SET Points=Points+" << points << " WHERE NetNo=" << NetNo << " AND Channel=\"" << Channel << "\" AND Nick=\"" << acct << "\"";
	} else {
		str << "INSERT INTO UnoChanStats (NetNo, Channel, Nick, Points) VALUES (" << NetNo << ", \"" << Channel << "\", \"" << acct << "\", " << points << ")";
	}
	api->db->Query(str.str().c_str(), NULL, NULL, NULL);
	if (playerHaveStats(acct)) {
		str2 << "UPDATE UnoStats SET Points=Points+" << points << " WHERE Nick=\"" << acct << "\"";
	} else {
		str2 << "INSERT INTO UnoStats (Nick, Points) VALUES (\"" << acct << "\", " << points << ")";
	}
	api->db->Query(str2.str().c_str(), NULL, NULL, NULL);
}
void UnoGame::addPlayerWin(string acct) {
	stringstream str,str2;
	if (playerHaveChanStats(acct)) {
		str << "UPDATE UnoChanStats SET Wins=Wins+1 WHERE NetNo=" << NetNo << " AND Channel=\"" << Channel << "\" AND Nick=\"" << acct << "\"";
	} else {
		str << "INSERT INTO UnoChanStats (NetNo, Channel, Nick, Wins) VALUES (" << NetNo << ", \"" << Channel << "\", \"" << acct << "\", 1)";
	}
	api->db->Query(str.str().c_str(), NULL, NULL, NULL);
	if (playerHaveStats(acct)) {
		str2 << "UPDATE UnoStats SET Wins=Wins+1 WHERE Nick=\"" << acct << "\"";
	} else {
		str2 << "INSERT INTO UnoStats (Nick, Wins) VALUES (\"" << acct << "\", 1)";
	}
	api->db->Query(str2.str().c_str(), NULL, NULL, NULL);
}
