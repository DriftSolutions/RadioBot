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

extern BOTAPI_DEF *api;
extern int pluginnum;

enum UNO_GAME_MODES {
	UGM_STANDARD,
	UGM_ONE_HAND
};

struct UNO_CONFIG {
	int TurnTimeout;
	UNO_GAME_MODES GameMode;
	bool AllowStacking;
	bool ColorCode;
};

extern UNO_CONFIG uno_config;

sql_rows GetTable(const char * query, char **errmsg=NULL);

#define UNO_NUM_CARDS 108

enum UNO_GAME_STATUS {
	UGS_STOPPED,
	UGS_GETTING_PLAYERS,
	UGS_PLAYER_TURN,
	UGS_END
};

enum UNO_COLORS {
	UCC_RED,
	UCC_GREEN,
	UCC_YELLOW,
	UCC_BLUE,
	UCC_WILD,
	UCC_ERROR
};

enum UNO_CARDS {
	UC_NUM_0,
	UC_NUM_1,
	UC_NUM_2,
	UC_NUM_3,
	UC_NUM_4,
	UC_NUM_5,
	UC_NUM_6,
	UC_NUM_7,
	UC_NUM_8,
	UC_NUM_9,
	UC_DRAW_TWO,
	UC_SKIP,
	UC_REVERSE,
	UC_WILD,
	UC_WILD_DRAW_FOUR,
	UC_ERROR
};

enum UNO_CARD_STATES {
	UC_INPILE,
	UC_INHAND,
	UC_DISCARDED
};

class Card {
public:
	UNO_CARDS card;
	UNO_COLORS color;
	UNO_CARD_STATES state;
	int player;
};

class CardPlay {
public:
	CardPlay() { card = UC_ERROR; col = excol = UCC_ERROR; }
	UNO_CARDS card;
	UNO_COLORS col, excol;
};

class Deck {
public:
	void Reset();
	Deck() { Reset(); }

	Card cards[UNO_NUM_CARDS];
	bool colorCode;

	Card * getCard(int num);
	Card * getCard(UNO_CARDS card, UNO_COLORS col);
	Card * getRandomCard(int * ind=NULL);
	Card * getStartingCard(int * ind=NULL);
	Card * getCardOwnedBy(UNO_CARDS card, UNO_COLORS col, int player);

	int calculateWinnerPoints();

	string GetCardName(Card * c);
	string GetShortCardName(Card * c);
	string GetColorName(UNO_COLORS col);

	UNO_CARDS txtToCard(UNO_COLORS color, const char * card);
	UNO_COLORS txtToColor(const char * col);
};

class PlayerInfo {
public:
	int num_cards;
	int points;
	string acct;
	string nick;

	bool didDraw;
	Card * drawCard;

	PlayerInfo() { points = num_cards = 0; didDraw = false; drawCard = NULL; }
};

typedef std::vector<PlayerInfo> playerMap;

class UnoGame {
private:
	int NetNo;
	string Channel;
	string GameStarter;
	UNO_GAME_STATUS status;
	Deck deck;

	struct {
		int TurnTimeout;
		UNO_GAME_MODES GameMode;
		bool AllowStacking;
		bool ColorCode;
	} GameOptions;

	struct {
		time_t timeGameStarted;
		time_t timeTurnStarted;
		int curPlayer;
		Card * pileCard;
		UNO_COLORS pileColor;

		int direction;
		int skipCount;
		int drawCount;
	} GameVars;
	playerMap players;

	void showColorParseError(USER_PRESENCE * ut, const char * col);
	bool playCard(USER_PRESENCE * ut, const char * acct, CardPlay * cp);
	void giveCard(int player, Card * c);
	void doTurn();
	void gameWon();
	void newDeal(bool first);
	void nextTurn();
	bool canPlayCard(UNO_CARDS card, UNO_COLORS col);

	bool playerHaveStats(string acct);
	bool playerHaveChanStats(string acct);
	void addPlayerPoints(string acct, int points);
	void addPlayerWin(string acct);
public:
	UnoGame();
	~UnoGame();

	void Prepare(USER_PRESENCE * ut);
	void AddPlayer(USER_PRESENCE * ut);
	void Start(USER_PRESENCE * ut);
	void Stop();
	void ShowScores();
	//void ShowTopCard();

	void ShowHand(int player);

	void Play(USER_PRESENCE * ut, const char * parms);
	void Draw(USER_PRESENCE * ut, bool in_pass);
	void Pass(USER_PRESENCE * ut);
	void ShowHand(USER_PRESENCE * ut);
	void ShowTopCard(USER_PRESENCE * ut);
	void TimeoutTurn();

	void Work();

	UNO_GAME_STATUS GetStatus() { return status; }
};
