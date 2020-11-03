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
#if defined(WIN32)
	#define PCRE_STATIC
	#include <pcre/pcre.h>
#else
	#include <pcre.h>
#endif

extern BOTAPI_DEF *api;
extern int pluginnum;

struct TRIVIA_CONFIG {
	char langcode[3];
	int QuestionTimeout;
	int TimeToFirstQuestion;
	int TimeBetweenQuestions;
	int QuestionsPerRound;
	int PointsToWin;
	int EnableHints;
	int HintsEvery;
	char HintHiddenChar;
};

extern TRIVIA_CONFIG t_config;

enum GAME_STATUS {
	GS_STOPPED,
	GS_ANNOUNCE,
	GS_WAITING_TO_ASK,
	GS_ASKED,
};

sql_rows GetTable(const char * query, char **errmsg=NULL);

class Question {
public:
	string category;
	string question, answer;
	string regexp;
	int points;
	time_t lastAsked;

	Question() { Reset(); }
	void Reset() { category = ""; question = ""; answer = ""; regexp = ""; points = 1; }
};

class PlayerInfo {
public:
	int points;
	string acct;
	string dispnick;

	PlayerInfo() { points = 0; }
};

typedef std::map<string, PlayerInfo> playerMap;
#define HINT_HIDDEN_CHAR '_'

class TriviaGame {
private:
	bool has_loaded;
	bool EnableHints;
	string LangCode;
	int QuestionTimeout;
	int TimeToFirstQuestion;
	int TimeBetweenQuestions;
	int QuestionsPerRound;
	int PointsToWin;
	int NetNo;
	int HintLevel, HintsEvery;
	char HintHiddenChar;
	string Channel;
	GAME_STATUS status;
	std::vector<Question> questions;

	struct {
		time_t timeStarted;
		time_t timeAsked;
		int QuestionNum;
		Question * CurQuestion;
		char AnswerProcessed[256];
		char last_winner[64];
		int win_streak;
	} GameVars;
	playerMap players;

public:
	TriviaGame();
	~TriviaGame();

	void LoadRules(int NetNo, string Chan);
	void SaveRules();
	void LoadQuestions();
	void LoadQuestions(string fn);

	void Start(string StartedBy, USER_PRESENCE * ut);
	void Stop();
	void ShowScores(bool announce_winner=false);
	void ShowHint(USER_PRESENCE * ut, int level=1, bool force=false);

	bool PlayerHaveStats(string acct);
	bool PlayerHaveChanStats(string acct);
	void AddPlayerPoints(string acct, int points);
	void AddPlayerWin(string acct);

	void AskQuestion();
	void TimeoutQuestion();
	void PostQuestion();
	void ProcAnswer(const char * in, char * out, size_t bufSize);
	void CheckAnswer(string acct, string dispnick, string text);
	void OnHint();

	void Work();

	bool HasLoaded() { return has_loaded; }
	GAME_STATUS GetStatus() { return status; }
};
