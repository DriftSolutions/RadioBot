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

Wordlist::~Wordlist() {
	wordList.clear();
}

const char valid_word_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

bool Wordlist::LoadList(string fn) {
	wordList.clear();
	FILE * fp = fopen(fn.c_str(), "rb");
	if (fp == NULL) {
		api->ib_printf("Hangman -> Error opening word list for read! (%s)\n", strerror(errno));
		return false;
	}
	char buf[512];
	memset(buf, 0, sizeof(buf));
	while (!feof(fp) && fgets(buf, sizeof(buf)-1, fp)) {
		strtrim(buf);
		if (buf[0] == 0 || buf[0] == '/' || buf[0] == '#') {
			continue;
		}
		if (strspn(buf, valid_word_chars) != strlen(buf)) {
			continue;
		}
		wordList.push_back(buf);
	}
	fclose(fp);
	if (wordList.size() == 0) {
		api->ib_printf("Hangman -> Word list does not have any words!\n");
		return false;
	}
	return true;
}
size_t Wordlist::Count() {
	return wordList.size();
}
string Wordlist::GetRandomWord() {
	if (wordList.size()) {
		return wordList[api->genrand_int32()%wordList.size()];
	}
	return "";
}
