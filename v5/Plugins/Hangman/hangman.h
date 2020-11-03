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

struct HANGMAN_CONFIG {
	int TurnTimeout;
	int NumTries;
};

extern HANGMAN_CONFIG hangman_config;

sql_rows GetTable(const char * query, char **errmsg=NULL);

#define HANGMAN_NUM_TRIES 6
#define HANGMAN_NUM_LETTERS 26

enum HANGMAN_GAME_STATUS {
	UGS_STOPPED,
	UGS_PLAYING,
	UGS_END
};

class Wordlist {
private:
	vector<string> wordList;
public:
	~Wordlist();
	bool LoadList(string fn);
	size_t Count();
	string GetRandomWord();
};

class Letter {
public:
	char letter;
	bool tried;
};

class HangmanGame {
private:
	USER_PRESENCE * player;
	void setPlayer(USER_PRESENCE * p);
	HANGMAN_GAME_STATUS status;
	Letter letters[HANGMAN_NUM_LETTERS];
	void resetLetters();
	Letter * getLetter(char c);
	Wordlist wordList;

	struct {
		int TurnTimeout;
		int NumTries;
	} GameOptions;

	struct {
		char word[128];
		char wordmasked[128];
		char tried[32];
		time_t timeGameStarted;
		time_t timeLastMove;
		int fail_count, good_count;
	} GameVars;

	bool playLetter(USER_PRESENCE * ut, char c);
	void doTurn();
	void gameLoss();
	void gameWon();
	void newWord(bool first);
	void timeout();

	bool playerHaveStats(string acct);
	void addPlayerWin(string acct);
	void addPlayerLoss(string acct);
public:
	HangmanGame();
	~HangmanGame();

	void Start(USER_PRESENCE * ut);
	void Stop(USER_PRESENCE * ut=NULL);

	void ShowWordStatus(USER_PRESENCE * ut);

	void Play(USER_PRESENCE * ut, const char * parms);
	//void Draw(USER_PRESENCE * ut, bool in_pass);
	//void Pass(USER_PRESENCE * ut);
	//void ShowHand(USER_PRESENCE * ut);
	//void ShowTopLetter(USER_PRESENCE * ut);
	void Timeout();

	void Work();

	USER_PRESENCE * GetPlayer() { return player; }
	HANGMAN_GAME_STATUS GetStatus() { return status; }
};
