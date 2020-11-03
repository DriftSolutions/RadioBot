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
#include <cctype>
#if defined(WIN32)
	#if defined(DEBUG)
	#pragma comment(lib, "pcre_d.lib")
	#else
	#pragma comment(lib, "pcre.lib")
	#endif
#endif

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

TriviaGame::TriviaGame() {
	has_loaded = false;
	EnableHints = t_config.EnableHints ? true:false;
	HintsEvery = t_config.HintsEvery;
	HintLevel = 3;
	HintHiddenChar = t_config.HintHiddenChar;
	LangCode = t_config.langcode;
	QuestionTimeout = t_config.QuestionTimeout;
	TimeToFirstQuestion = t_config.TimeToFirstQuestion;
	TimeBetweenQuestions = t_config.TimeBetweenQuestions;
	QuestionsPerRound = t_config.QuestionsPerRound;
	PointsToWin = t_config.PointsToWin;
	status = GS_STOPPED;
	memset(&GameVars, 0, sizeof(GameVars));
}

TriviaGame::~TriviaGame() {
	Stop();
	//UnloadQuestions();
}

void TriviaGame::LoadRules(int iNetNo, string Chan) {
	NetNo = iNetNo;
	Channel = Chan;
	has_loaded = true;
}

void TriviaGame::SaveRules() {
}

void TriviaGame::LoadQuestions(string fn) {
	api->ib_printf(_("Trivia -> Loading questions from %s ...\n"), fn.c_str());
	FILE * fp = fopen(fn.c_str(), "rb");
	char buf[1024];
	Question q;
	if (fp) {
		while (!feof(fp) && fgets(buf, sizeof(buf), fp)) {
			strtrim(buf);
			if (buf[0] == '#') { continue; }
			if (buf[0] == 0) {
				if (q.question.length() && q.answer.length()) {
					questions.push_back(q);
				}
				q.Reset();
			}
			if (!strnicmp(buf, "Category:", 9)) {
				char * p = &buf[9];
				strtrim(p);
				q.category = p;
			}
			if (!strnicmp(buf, "Question:", 9)) {
				char * p = &buf[9];
				strtrim(p);
				q.question = p;
			}
			if (!strnicmp(buf, "Answer:", 7)) {
				char * p = &buf[7];
				strtrim(p);
				q.answer = p;
			}
			if (!strnicmp(buf, "Regexp:", 7)) {
				char * p = &buf[7];
				strtrim(p);
				q.regexp = p;
			}
			if (!strnicmp(buf, "Score:", 6)) {
				char * p = &buf[6];
				strtrim(p);
				q.points = atoi(p);
			}
		}
		fclose(fp);
	} else {
		api->ib_printf(_("Trivia -> Error opening %s for read...\n"), fn.c_str());
	}
}

void TriviaGame::LoadQuestions() {
	api->ib_printf(_("Trivia -> Loading questions with language code %s ...\n"), LangCode.c_str());
	questions.clear();

	Directory dir("./trivia");
	char buf[MAX_PATH];
	bool is_dir;
	while (dir.Read(buf, sizeof(buf), &is_dir)) {
		if (!is_dir && buf[0] != '.') {
			if (!strnicmp(buf, "questions.", 10)) {
				char * ext = strrchr(buf, '.');
				if (ext++) {
					if (!stricmp(ext, LangCode.c_str())) {
						string ffn = "./trivia/";
						ffn += buf;
						LoadQuestions(ffn);
					}
				}
			}
		}
	}
	api->ib_printf(_("Trivia -> Loaded %u questions...\n"), questions.size());
}

void TriviaGame::Start(string StartedBy, USER_PRESENCE * ut) {
	char buf[1024],buf2[1024];
	if (questions.size() == 0) {
		LoadQuestions();
	}
	if (questions.size() == 0) {
		api->ib_printf(_("Trivia -> No questions loaded, can not start game!\n"));
		if (ut) { ut->send_notice(ut, _("No questions loaded, can not start game!\n")); }
		return;
	}

	if (ut) { ut->send_notice(ut, _("Starting game...\n")); }
	if (!api->LoadMessage("TriviaAnnounce", buf2, sizeof(buf))) {
		strcpy(buf2, "%nick has initiated Trivia! Game will start in %secs seconds...");
	}
	sprintf(buf, "%d", TimeToFirstQuestion);
	str_replace(buf2, sizeof(buf2), "%secs", buf);
	str_replace(buf2, sizeof(buf2), "%nick", StartedBy.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	memset(&GameVars, 0, sizeof(GameVars));
	GameVars.timeStarted = time(NULL);
	status = GS_ANNOUNCE;
}

void TriviaGame::Stop() {
	char buf[1024],buf2[1024];
	if (status != GS_STOPPED) {
		if (!api->LoadMessage("TriviaOver", buf2, sizeof(buf))) {
			strcpy(buf2, "The trivia game has ended, we hope you enjoyed yourself!");
		}
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

		ShowScores(true);

		memset(&GameVars, 0, sizeof(GameVars));
		players.clear();
		status = GS_STOPPED;
	}
}

void TriviaGame::Work() {
	if (status == GS_ANNOUNCE && (time(NULL) - GameVars.timeStarted) >= TimeToFirstQuestion) {
		AskQuestion();
	}
	if (status == GS_WAITING_TO_ASK && (time(NULL) - GameVars.timeAsked) >= TimeBetweenQuestions) {
		AskQuestion();
	}
	if (status == GS_ASKED && (time(NULL) - GameVars.timeAsked) >= QuestionTimeout) {
		TimeoutQuestion();
		PostQuestion();
	}
	if (status == GS_ASKED && HintsEvery && GameVars.CurQuestion && ((time(NULL) - GameVars.timeAsked)%HintsEvery) == 0) {
		if (HintLevel >= 1) {
			ShowHint(NULL, HintLevel, true);
			HintLevel--;
		}
	}
}

void TriviaGame::ProcAnswer(const char * in, char * out, size_t bufSize) {
	//char * tmp = strdup(in);
	//char * outo = out;
	for (const char *p=in; *p != 0; p++) {
		if (isalnum(*p) && *p != ' ') {
			*out = *p;
			out++;
		}
	}
	*out = 0;
	//api->ib_printf("Trivia -> %s => %s\n", in, outo);
	//free(tmp);
}

void TriviaGame::AskQuestion() {
	char buf[1024],buf2[1024];
	status = GS_ASKED;
	GameVars.timeAsked = time(NULL);
	GameVars.QuestionNum++;

	uint32 num = api->genrand_range(0, questions.size() - 1);
	GameVars.CurQuestion = &questions[num];
	GameVars.CurQuestion->lastAsked = time(NULL);
	HintLevel = 3;
	ProcAnswer(GameVars.CurQuestion->answer.c_str(), GameVars.AnswerProcessed, sizeof(GameVars.AnswerProcessed));

	if (!api->LoadMessage("TriviaQuestion", buf2, sizeof(buf))) {
		strcpy(buf2, "Question %num: %question");
	}
	sprintf(buf, "%d", GameVars.QuestionNum);
	str_replace(buf2, sizeof(buf2), "%num", buf);
	str_replace(buf2, sizeof(buf2), "%category", GameVars.CurQuestion->category.c_str());
	str_replace(buf2, sizeof(buf2), "%question", GameVars.CurQuestion->question.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
	//sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), GameVars.CurQuestion->answer.c_str());
	//api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

void TriviaGame::TimeoutQuestion() {
	char buf[1024],buf2[1024];

	if (!api->LoadMessage("TriviaTimeout", buf2, sizeof(buf))) {
		strcpy(buf2, "Question timed out, it seems nobody knew that one.");
	}
	sprintf(buf, "%d", GameVars.QuestionNum);
	str_replace(buf2, sizeof(buf2), "%num", buf);
	str_replace(buf2, sizeof(buf2), "%category", GameVars.CurQuestion->category.c_str());
	str_replace(buf2, sizeof(buf2), "%question", GameVars.CurQuestion->question.c_str());
	sstrcpy(buf, GameVars.CurQuestion->answer.c_str());
	str_replace(buf, sizeof(buf), "#", "");
	str_replace(buf2, sizeof(buf2), "%answer", buf);
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

bool preg_match(const char * pattern, const char * text, int textLen=-1) {
	if (textLen == -1) { textLen = strlen(text); }
	bool ret = false;
	const char * error;
	int errptr = 0;
#if defined(PCRE_NEWLINE_ANYCRLF)
	pcre * pc = pcre_compile(pattern, PCRE_CASELESS|PCRE_BSR_ANYCRLF|PCRE_NEWLINE_ANYCRLF, &error, &errptr, NULL);
#else
	pcre * pc = pcre_compile(pattern, PCRE_CASELESS, &error, &errptr, NULL);
#endif
	if (pc != NULL) {
		int n = pcre_exec(pc, NULL, text, textLen, 0, PCRE_NOTEMPTY, NULL, 0);
		if (n == 0) {
			ret = true;
		}
		pcre_free(pc);
	} else {
		api->ib_printf("Trivia -> Error compiling regexp!\n");
		api->ib_printf("Trivia -> Error desc: %s\n", error);
		api->ib_printf("Trivia -> Error pos: %d in %s\n", errptr, pattern);
	}
	//printf("regex: %d\n", ret);
	return ret;
}

void TriviaGame::CheckAnswer(string acct, string dispnick, string text) {
	char * tmp = zstrdup(text.c_str());
	ProcAnswer(text.c_str(), tmp, text.length());

	if (!stricmp(text.c_str(), GameVars.CurQuestion->answer.c_str()) || !stricmp(tmp, GameVars.AnswerProcessed) || (GameVars.CurQuestion->regexp.length() && preg_match(GameVars.CurQuestion->regexp.c_str(), text.c_str()))) {
		char buf[1024],buf2[1024];
		if (!stricmp(GameVars.last_winner, acct.c_str())) {
			GameVars.win_streak++;
		} else {
			strlcpy(GameVars.last_winner, acct.c_str(), sizeof(GameVars.last_winner));
			GameVars.last_winner[sizeof(GameVars.last_winner)-1] = 0;
			GameVars.win_streak = 1;
		}
		players[acct].acct = acct;
		players[acct].dispnick = dispnick;
		players[acct].points += GameVars.CurQuestion->points;
		AddPlayerPoints(acct, GameVars.CurQuestion->points);

		if (GameVars.win_streak > 1) {
			if (!api->LoadMessage("TriviaCorrectStreak", buf2, sizeof(buf))) {
				strcpy(buf2, "Congratulations %nick, that is %streak in a row! The answer was %answer earning you %points point(s).");
			}
		} else {
			if (!api->LoadMessage("TriviaCorrect", buf2, sizeof(buf))) {
				strcpy(buf2, "%nick got it! The answer was %answer earning you %points point(s).");
			}
		}
		sprintf(buf, "%d", GameVars.CurQuestion->points);
		str_replace(buf2, sizeof(buf2), "%points", buf);
		sprintf(buf, "%d", GameVars.win_streak);
		str_replace(buf2, sizeof(buf2), "%streak", buf);
		str_replace(buf2, sizeof(buf2), "%category", GameVars.CurQuestion->category.c_str());
		str_replace(buf2, sizeof(buf2), "%question", GameVars.CurQuestion->question.c_str());
		sstrcpy(buf, GameVars.CurQuestion->answer.c_str());
		str_replace(buf, sizeof(buf), "#", "");
		str_replace(buf2, sizeof(buf2), "%answer", buf);
		str_replace(buf2, sizeof(buf2), "%nick", dispnick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

		if (PointsToWin && players[acct].points >= PointsToWin) {
			Stop();
		} else {
			PostQuestion();
		}
	}
	zfree(tmp);
}

void TriviaGame::PostQuestion() {
	char buf[1024],buf2[1024];

	//api->ib_printf("Question %d of %d asked\n", GameVars.QuestionNum, QuestionsPerRound);
	if (QuestionsPerRound && GameVars.QuestionNum >= QuestionsPerRound) {
		Stop();
		return;
	}

	if (!api->LoadMessage("TriviaNextQTime", buf2, sizeof(buf))) {
		strcpy(buf2, "Next question is in %secs seconds.");
	}
	sprintf(buf, "%d", TimeBetweenQuestions);
	str_replace(buf2, sizeof(buf2), "%secs", buf);
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	if (GameVars.QuestionNum && (GameVars.QuestionNum%10) == 0) {
		ShowScores(false);
	}

	status = GS_WAITING_TO_ASK;
	GameVars.timeAsked = time(NULL);
	//if (PointsToWin
}

typedef std::multimap<int, PlayerInfo> scoreList;
void TriviaGame::ShowScores(bool announce_winner) {
	if (players.size() == 0) { return; }

	scoreList scores;
	for (playerMap::const_iterator x = players.begin(); x != players.end(); x++) {
		scores.insert(std::pair<int, PlayerInfo>(x->second.points, x->second));
	}

	char buf[1024],buf2[1024];
	if (announce_winner) {
		if (!api->LoadMessage("TriviaScoresOver", buf2, sizeof(buf))) {
			strcpy(buf2, "Trivia Final Scores");
		}
	} else {
		if (!api->LoadMessage("TriviaScoresMid", buf2, sizeof(buf))) {
			strcpy(buf2, "Trivia Scores at Question %num");
		}
	}
	sprintf(buf, "%d", GameVars.QuestionNum);
	str_replace(buf2, sizeof(buf2), "%num", buf);
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

		if (!api->LoadMessage("TriviaScore", buf2, sizeof(buf))) {
			strcpy(buf2, "%rank. %nick: %points point(s)");
		}
		sprintf(buf, "%d", rank);
		str_replace(buf2, sizeof(buf2), "%rank", buf);
		sprintf(buf, "%d", x->second.points);
		str_replace(buf2, sizeof(buf2), "%points", buf);
		str_replace(buf2, sizeof(buf2), "%nick", x->second.dispnick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

		if (announce_winner && rank == 1) {
			AddPlayerWin(x->second.acct);
		}
	}

	if (!api->LoadMessage("TriviaScoresEnd", buf2, sizeof(buf))) {
		strcpy(buf2, "End of list.");
	}
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

}

void TriviaGame::ShowHint(USER_PRESENCE * ut, int level, bool force) {
	if (status == GS_ASKED && GameVars.CurQuestion) {
		if (EnableHints || force) {
			char buf[1024],buf2[1024];
			if (!api->LoadMessage("TriviaHint", buf2, sizeof(buf))) {
				strcpy(buf2, "Here is a hint: %hint");
			}
			sstrcpy(buf, GameVars.CurQuestion->answer.c_str());
			str_replace(buf, sizeof(buf), "#", "");
			int num=level;
			for (int i=0; buf[i] != 0; i++) {
				if (GameVars.CurQuestion->answer.length() <= 5) {
					if (level < 2) { level=2; }
					if ((num%level)!=0) {
						buf[i] = HintHiddenChar;
					}
					num++;
				} else {
					if (buf[i] == ' ') {
						//buf[i] = ' ';
					} else if (buf[i] != 'a' && buf[i] != 'e' && buf[i] != 'i' && buf[i] != 'o' && buf[i] != 'u' && buf[i] != 'y') {
						buf[i] = HintHiddenChar;
					} else {
						if ((num%level)!=0) {
							buf[i] = HintHiddenChar;
						}
						num++;
					}
				}
			}
			if (ut) { str_replace(buf2, sizeof(buf2), "%nick", ut->Nick); }
			str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
			str_replace(buf2, sizeof(buf2), "%hint", buf);
			api->ProcText(buf2, sizeof(buf2));
			sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
			api->irc->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
		} else {
			if (ut) { ut->std_reply(ut, "Hints are not enabled for this round!"); }
		}
	} else {
		if (ut) { ut->std_reply(ut, "There is no active question!"); }
	}
}

bool TriviaGame::PlayerHaveChanStats(string acct) {
	stringstream str;
	str << "SELECT * FROM TriviaChanStats WHERE NetNo=" << NetNo << " AND Channel=\"" << Channel << "\" AND Nick=\"" << acct << "\"";
	sql_rows ret = GetTable(str.str().c_str());
	return ret.size() > 0 ? true:false;
}
bool TriviaGame::PlayerHaveStats(string acct) {
	stringstream str;
	str << "SELECT * FROM TriviaStats WHERE Nick=\"" << acct << "\"";
	sql_rows ret = GetTable(str.str().c_str());
	return ret.size() > 0 ? true:false;
}
void TriviaGame::AddPlayerPoints(string acct, int points) {
	stringstream str,str2;
	if (PlayerHaveChanStats(acct)) {
		str << "UPDATE TriviaChanStats SET Points=Points+" << points << " WHERE NetNo=" << NetNo << " AND Channel=\"" << Channel << "\" AND Nick=\"" << acct << "\"";
	} else {
		str << "INSERT INTO TriviaChanStats (NetNo, Channel, Nick, Points) VALUES (" << NetNo << ", \"" << Channel << "\", \"" << acct << "\", " << points << ")";
	}
	api->db->Query(str.str().c_str(), NULL, NULL, NULL);
	if (PlayerHaveStats(acct)) {
		str2 << "UPDATE TriviaStats SET Points=Points+" << points << " WHERE Nick=\"" << acct << "\"";
	} else {
		str2 << "INSERT INTO TriviaStats (Nick, Points) VALUES (\"" << acct << "\", " << points << ")";
	}
	api->db->Query(str2.str().c_str(), NULL, NULL, NULL);
}
void TriviaGame::AddPlayerWin(string acct) {
	stringstream str,str2;
	if (PlayerHaveChanStats(acct)) {
		str << "UPDATE TriviaChanStats SET Wins=Wins+1 WHERE NetNo=" << NetNo << " AND Channel=\"" << Channel << "\" AND Nick=\"" << acct << "\"";
	} else {
		str << "INSERT INTO TriviaChanStats (NetNo, Channel, Nick, Wins) VALUES (" << NetNo << ", \"" << Channel << "\", \"" << acct << "\", 1)";
	}
	api->db->Query(str.str().c_str(), NULL, NULL, NULL);
	if (PlayerHaveStats(acct)) {
		str2 << "UPDATE TriviaStats SET Wins=Wins+1 WHERE Nick=\"" << acct << "\"";
	} else {
		str2 << "INSERT INTO TriviaStats (Nick, Wins) VALUES (\"" << acct << "\", 1)";
	}
	api->db->Query(str2.str().c_str(), NULL, NULL, NULL);
}

