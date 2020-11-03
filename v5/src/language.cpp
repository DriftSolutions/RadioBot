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

#include "ircbot.h"

typedef std::map<std::string, const char *> langList;
langList lang;
Titus_Mutex langMutex;

void ResetLang() {
	langMutex.Lock();
	for (langList::iterator x = lang.begin(); x != lang.end(); x++) {
		zfree((void *)x->second);
	}
	lang.clear();
	langMutex.Release();
}

const char * GetLangString(const char * str) {
	const char * ret = str;
	langMutex.Lock();
	langList::iterator x = lang.find(str);
	if (x != lang.end()) {
		ret = x->second;
	}
	langMutex.Release();
	return ret;
}

void LoadLang(const char * fn) {
	langMutex.Lock();
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		fclose(fp);
	}
	langMutex.Release();
}

bool GetValue(const char * start, char * buf, size_t bufSize) {
	memset(buf, 0, bufSize);

	const char * p = start;
	const char * end = NULL;
	while (*p) {
		if (*p == '\\') {
			p++;
		} else if (*p == '\"') {
			end = p;
			break;
		}
		p++;
	}

	if (end) {
		strncpy(buf, start, end-start);
		return true;
	}

	return false;
}

void unescape(char * buf, size_t bufSize) {
//\000 /*octal number*/ \xhh /*hexadecimal number*/
	char *p = buf;
	while (*p) {
		if (*p == '\\') {
			memmove(p, p+1, strlen(p));
			switch (*p) {
				case 'n':
					*p = '\n';
					break;
				case 'r':
					*p = '\r';
					break;
				case 't':
					*p = '\t';
					break;
				case 'v':
					*p = '\v';
					break;
				case 'a':
					*p = '\a';
					break;
				case 'b':
					*p = '\b';
					break;
				case 'f':
					*p = '\f';
					break;
				case 'x':
					char buf2[4];
					buf2[0] = p[1];
					buf2[1] = p[2];
					buf2[2] = 0;
					*p = (char)strtoul(buf2, NULL, 16);
					memmove(p, p+3, strlen(p+3)+1);
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					buf2[0] = p[0];
					buf2[1] = p[1];
					buf2[2] = p[2];
					buf2[3] = 0;
					*p = (char)strtoul(buf2, NULL, 8);
					memmove(p, p+4, strlen(p+4)+1);
					break;
			}
		}
		p++;
	}
}

bool LoadLangText(const char * fn) {
	langMutex.Lock();
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		std::string str;
		char buf[1024], buf2[1024], buf3[1024];
		int line=0;
		while (fgets(buf, sizeof(buf), fp)) {
			line++;
			if (!strnicmp(buf, "string \"", 8)) {
				if (GetValue(buf+8, buf2, sizeof(buf2))) {
					str = buf2;
				}
			}
			if (!strnicmp(buf, "value \"", 7)) {
				if (GetValue(buf+7, buf3, sizeof(buf3))) {
					if (buf2[0] && buf3[0]) {
						unescape(buf2, sizeof(buf2));
						unescape(buf3, sizeof(buf3));
						lang[buf2] = zstrdup(buf3);
					} else if (buf3[0]) {
						ib_printf("LoadLangText(): value without string first! (file: %s, line: %d)\n", fn, line);
					}
					buf2[0] = 0;
				}
			}
		}
		fclose(fp);
		langMutex.Release();
		return true;
	} else {
		ib_printf("" IRCBOT_NAME ": Error opening language file: %s\n", fn);
	}
	langMutex.Release();
	return false;
}
