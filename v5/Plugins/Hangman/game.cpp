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

Letter * HangmanGame::getLetter(char c) {
	int i = toupper(c);
	if (i >= 'A' && i <= 'Z') {
		return &letters[i-'A'];
	}
	return NULL;
}

void HangmanGame::resetLetters() {
	for (int i=0; i < HANGMAN_NUM_LETTERS; i++) {
		letters[i].letter = 'A'+i;
		letters[i].tried = false;
	}
}

HangmanGame::HangmanGame() {
	status = UGS_STOPPED;
	player = NULL;
	memset(&GameOptions, 0, sizeof(GameOptions));
	GameOptions.TurnTimeout = hangman_config.TurnTimeout;
	GameOptions.NumTries = hangman_config.NumTries;
	memset(&GameVars, 0, sizeof(GameVars));
	resetLetters();
	wordList.LoadList("hangman.wordlist.txt");
}

void HangmanGame::setPlayer(USER_PRESENCE * ut) {
	if (ut == player) { return; }
	if (player != NULL) {
		UnrefUser(player);
	}
	player = ut;
	if (ut != NULL) {
		RefUser(ut);
	}
}


HangmanGame::~HangmanGame() {
	Stop();
	setPlayer(NULL);
	//UnloadQuestions();
}

void HangmanGame::newWord(bool first) {
	char buf[1024],buf2[1024];

	resetLetters();
	memset(GameVars.tried, 0, sizeof(GameVars.tried));
	GameVars.fail_count = GameVars.good_count = 0;
	GameVars.timeLastMove = time(NULL);
	string nw = wordList.GetRandomWord();
	if (nw.length() == 0) {
		if (player != NULL) {
			player->std_reply(player, _("Error getting a new word!\n"));
		}
		Stop();
	}
	sstrcpy(GameVars.word, nw.c_str());
	for (size_t i=0; i < strlen(GameVars.word); i++) {
		GameVars.wordmasked[i] = '_';
	}
	GameVars.wordmasked[strlen(GameVars.word)] = 0;

	if (player != NULL) {
		if (first) {
			if (!api->LoadMessage("HangmanStart", buf2, sizeof(buf))) {
				strcpy(buf2, "Welcome to Hangman! Your first word is %num% letters long. Type '!letter your_letter' to take a guess...");
			}
		} else {
			if (!api->LoadMessage("HangmanNewWord", buf2, sizeof(buf))) {
				strcpy(buf2, "Your new word is %num% letters long... ");
			}
		}
		snprintf(buf, sizeof(buf), "%u", strlen(GameVars.word));
		str_replace(buf2, sizeof(buf2), "%num%", buf);
		api->ProcText(buf2, sizeof(buf2));
		player->std_reply(player, buf2);
	}
}

void HangmanGame::Start(USER_PRESENCE * ut) {
	setPlayer(ut);
	if (wordList.Count() == 0) {
		ut->std_reply(ut, _("Error: could not load word list!\n"));
		Stop(ut);
		return;
	}
	GameVars.timeLastMove = GameVars.timeGameStarted = time(NULL);
	status = UGS_PLAYING;
	newWord(true);
}

void HangmanGame::gameWon() {
	char buf2[1024];

	if (player != NULL) {
		if (!api->LoadMessage("HangmanWonGame", buf2, sizeof(buf2))) {
			strcpy(buf2, "Congratulations! You guessed the word: %word%");
		}
		str_replace(buf2, sizeof(buf2), "%nick", player->Nick);
		str_replace(buf2, sizeof(buf2), "%word%", GameVars.word);
		api->ProcText(buf2, sizeof(buf2));
		player->std_reply(player, buf2);
		addPlayerWin(player->User ? player->User->Nick:player->Nick);
	}

	//ShowScores();

	newWord(false);
}

void HangmanGame::gameLoss() {
	char buf2[1024];

	if (!api->LoadMessage("HangmanLostGame", buf2, sizeof(buf2))) {
		switch (api->genrand_int32()%3) {
			case 0:
				sstrcpy(buf2, "The executioner throws the switch and down you drop. Better luck next time...");
				break;
			case 1:
				sstrcpy(buf2, "You have failed to guess the word. Your sentence: death by hanging.");
				break;
			default:
				sstrcpy(buf2, "You are dead...");
				break;
		}
	}
	str_replace(buf2, sizeof(buf2), "%nick", player->Nick);
	api->ProcText(buf2, sizeof(buf2));
	player->std_reply(player, buf2);

	addPlayerLoss(player->User ? player->User->Nick:player->Nick);
}


void HangmanGame::Stop(USER_PRESENCE * ut) {
	if (ut != NULL) {
		setPlayer(ut);
	} else {
		ut = player;
	}
	char buf2[1024];

	if (status == UGS_PLAYING) {
		if (GameVars.fail_count > 0 || GameVars.good_count > 0) {
			gameLoss();
		}

		if (!api->LoadMessage("HangmanOver", buf2, sizeof(buf2))) {
			strcpy(buf2, "Hangman has ended, we hope you enjoyed yourself!");
		}
		api->ProcText(buf2, sizeof(buf2));
		if (player != NULL) {
			player->std_reply(player, buf2);
		}
	}

	setPlayer(NULL);
	status = UGS_END;
	memset(&GameVars, 0, sizeof(GameVars));
}

/*
void HangmanGame::ShowHand(int player) {
	char buf[1024],buf2[1024];

	stringstream out;
	for (int j=0; j < HANGMAN_NUM_CARDS; j++) {
		Card * c = deck.getCard(j);
		if (c && c->state == UC_INHAND && c->player == player) {
			if (players[player].num_cards <= 7) {
				out << " " << deck.GetCardName(c);
			} else {
				out << " " << deck.GetShortCardName(c);
			}
		}
	}

	if (!api->LoadMessage("HangmanShowHand", buf2, sizeof(buf))) {
		sstrcpy(buf2, "Your Hand (%num cards): %hand");
	}
	str_replace(buf2, sizeof(buf2), "%nick", players[player].nick.c_str());
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	sprintf(buf, "%d", players[player].num_cards);
	str_replace(buf2, sizeof(buf2), "%num", buf);
	str_replace(buf2, sizeof(buf2), "%hand", out.str().c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "NOTICE %s :%s\r\n", players[player].nick.c_str(), buf2);
	api->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
}
*/

void HangmanGame::Timeout() {
	char buf2[1024];

	if (player != NULL) {
		if (!api->LoadMessage("HangmanTimeout", buf2, sizeof(buf2))) {
			sstrcpy(buf2, "Your game has timed out, stopping Hangman...");
		}
		str_replace(buf2, sizeof(buf2), "%nick", player->Nick);
		api->ProcText(buf2, sizeof(buf2));
		player->std_reply(player, buf2);
	}

	Stop();
}

/*
void HangmanGame::ShowHand(USER_PRESENCE * ut) {
	const char * acct = ut->User ? ut->User->Nick:ut->Nick;
	for (size_t i=0; i < players.size(); i++) {
		if (!stricmp(acct, players[i].acct.c_str())) {
			ShowHand(i);
			break;
		}
	}
	ut->std_reply(ut, "You are not in this match!");
}
*/

void HangmanGame::Play(USER_PRESENCE * ut, const char * parms) {
	setPlayer(ut);
	char buf2[1024];

		/*
		if (!api->LoadMessage("HangmanCardNotAllowed", buf2, sizeof(buf))) {
			sstrcpy(buf2, "'%card' can not be played right now!");
		}
		str_replace(buf2, sizeof(buf2), "%nick", players[GameVars.curPlayer].nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		str_replace(buf2, sizeof(buf2), "%card", deck.GetCardName(c).c_str());
		api->ProcText(buf2, sizeof(buf2));
		ut->send_notice(ut, buf2);
		return false;
		*/

	//const char * acct = player->User ? player->User->Nick:player->Nick;
	GameVars.timeLastMove = time(NULL);
	if (parms && parms[0]) {
		char c = toupper(parms[0]);
		Letter * l = getLetter(c);
		if (l != NULL) {
			if (!l->tried) {
				l->tried = true;
				GameVars.tried[strlen(GameVars.tried)] = c;
				GameVars.tried[strlen(GameVars.tried)] = 0;
				bool matched = false;
				for (int i=0; GameVars.word[i] != 0; i++) {
					if (GameVars.word[i] == c) {
						matched = true;
						GameVars.wordmasked[i] = GameVars.word[i];
					}
				}
				if (matched) {
					if (!api->LoadMessage("HangmanMatch", buf2, sizeof(buf2))) {
						sstrcpy(buf2, "That letter is in your word...");
					}
					str_replace(buf2, sizeof(buf2), "%nick", player->Nick);
					api->ProcText(buf2, sizeof(buf2));
					player->std_reply(player, buf2);

					GameVars.good_count++;
					if (!stricmp(GameVars.word, GameVars.wordmasked)) {
						gameWon();
					} else {
						ShowWordStatus(ut);
					}
				} else {
					if (!api->LoadMessage("HangmanNoMatch", buf2, sizeof(buf2))) {
						sstrcpy(buf2, "That letter does not appear in your word...");
					}
					str_replace(buf2, sizeof(buf2), "%nick", player->Nick);
					api->ProcText(buf2, sizeof(buf2));
					player->std_reply(player, buf2);

					GameVars.fail_count++;
					if (GameVars.fail_count == GameOptions.NumTries) {
						gameLoss();
						newWord(false);
					}
				}
			} else {
				if (!api->LoadMessage("HangmanAlreadyTried", buf2, sizeof(buf2))) {
					sstrcpy(buf2, "You have already tried that letter!");
				}
				str_replace(buf2, sizeof(buf2), "%nick", player->Nick);
				api->ProcText(buf2, sizeof(buf2));
				player->std_reply(player, buf2);
			}
		} else {
			player->std_reply(player, _("That is not a valid letter!"));
		}
		/*
				if (players[GameVars.curPlayer].num_cards <= 0) {
					gameWon();
					do_next_turn = false;
				}
				*/
	} else {
		player->std_reply(player, _("You haven't included a letter to play! (Example: !letter A)"));
	}
}

void HangmanGame::ShowWordStatus(USER_PRESENCE * ut) {
	setPlayer(ut);
	char buf[1024],buf2[1024];
	if (!api->LoadMessage("HangmanStatus", buf2, sizeof(buf))) {
		sstrcpy(buf2, "Letters tried so far: %tried%. Current word: %word%. Bad guesses: %bad%/%limit%.");
	}
	str_replace(buf2, sizeof(buf2), "%nick", player->Nick);
	snprintf(buf, sizeof(buf), "%s", GameVars.tried);
	str_replace(buf2, sizeof(buf2), "%tried%", buf);
	snprintf(buf, sizeof(buf), "%s", GameVars.wordmasked);
	str_replace(buf2, sizeof(buf2), "%word%", buf);
	snprintf(buf, sizeof(buf), "%d", GameVars.good_count);
	str_replace(buf2, sizeof(buf2), "%good%", buf);
	snprintf(buf, sizeof(buf), "%d", GameVars.fail_count);
	str_replace(buf2, sizeof(buf2), "%bad%", buf);
	snprintf(buf, sizeof(buf), "%d", GameOptions.NumTries);
	str_replace(buf2, sizeof(buf2), "%limit%", buf);
	api->ProcText(buf2, sizeof(buf2));
	player->std_reply(player, buf2);
}

void HangmanGame::Work() {
	if (status == UGS_PLAYING && (time(NULL) - GameVars.timeLastMove) >= GameOptions.TurnTimeout) {
		Timeout();
	}
}

/*
typedef std::multimap<int, PlayerInfo> scoreList;
void HangmanGame::ShowScores() {
	if (players.size() == 0) { return; }

	scoreList scores;
	for (playerMap::const_iterator x = players.begin(); x != players.end(); x++) {
		scores.insert(std::pair<int, PlayerInfo>(x->points, *x));
	}

	char buf[1024],buf2[1024];
	if (!api->LoadMessage("HangmanScores", buf2, sizeof(buf))) {
		strcpy(buf2, "Hangman Scoreboard");
	}
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);

	int rank = 0;
	int lastscore = 0;
	for (scoreList::reverse_iterator x = scores.rbegin(); x != scores.rend() && rank <= 10; x++) {
		if (x->second.points != lastscore) {
			rank++;
			lastscore = x->second.points;
		}
		if (rank == 11) { break; }

		if (!api->LoadMessage("HangmanScore", buf2, sizeof(buf))) {
			strcpy(buf2, "%nick: %points point(s)");
		}
		sprintf(buf, "%d", x->second.points);
		str_replace(buf2, sizeof(buf2), "%points", buf);
		str_replace(buf2, sizeof(buf2), "%nick", x->second.nick.c_str());
		str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
		api->ProcText(buf2, sizeof(buf2));
		sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
		api->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
	}

	if (!api->LoadMessage("HangmanScoresEnd", buf2, sizeof(buf))) {
		strcpy(buf2, "End of list.");
	}
	str_replace(buf2, sizeof(buf2), "%chan", Channel.c_str());
	api->ProcText(buf2, sizeof(buf2));
	sprintf(buf, "PRIVMSG %s :%s\r\n", Channel.c_str(), buf2);
	api->SendIRC_Priority(NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
}

bool HangmanGame::playerHaveChanStats(string acct) {
	stringstream str;
	str << "SELECT * FROM HangmanChanStats WHERE NetNo=" << NetNo << " AND Channel=\"" << Channel << "\" AND Nick=\"" << acct << "\"";
	sql_rows ret = GetTable(str.str().c_str());
	return ret.size() > 0 ? true:false;
}
*/
bool HangmanGame::playerHaveStats(string acct) {
	stringstream str;
	str << "SELECT * FROM HangmanStats WHERE Nick=\"" << acct << "\"";
	sql_rows ret = GetTable(str.str().c_str());
	return ret.size() > 0 ? true:false;
}
void HangmanGame::addPlayerWin(string acct) {
	stringstream str2;
	if (playerHaveStats(acct)) {
		str2 << "UPDATE HangmanStats SET Wins=Wins+1 WHERE Nick=\"" << acct << "\"";
	} else {
		str2 << "INSERT INTO HangmanStats (Nick, Wins) VALUES (\"" << acct << "\", 1)";
	}
	api->db->Query(str2.str().c_str(), NULL, NULL, NULL);
}
void HangmanGame::addPlayerLoss(string acct) {
	stringstream str2;
	if (playerHaveStats(acct)) {
		str2 << "UPDATE HangmanStats SET Losses=Losses+1 WHERE Nick=\"" << acct << "\"";
	} else {
		str2 << "INSERT INTO HangmanStats (Nick, Losses) VALUES (\"" << acct << "\", 1)";
	}
	api->db->Query(str2.str().c_str(), NULL, NULL, NULL);
}
