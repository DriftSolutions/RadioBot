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
#if defined(IRCBOT_ENABLE_IRC)
#include "ial.h"
#include "textproc.h"

#undef free
#undef malloc
#undef realloc
#undef new
#undef delete

IAL::IAL() {
}

IAL::~IAL() {
	Clear();
}

bool IAL::IsChan(const char * name) {
	AutoMutex(hMutex);
	ialChannelList::const_iterator x = channels.find(name);
	if (x != channels.end()) {
		return true;
	}
	return false;
}

IAL_CHANNEL * IAL::GetChan(const char * name, bool no_create) {
	AutoMutex(hMutex);
	ialChannelList::iterator x = channels.find(name);
	if (x != channels.end()) {
		return &x->second;
	}

	if (no_create) { return NULL; }

	IAL_CHANNEL Scan;
//	Scan.channel = name;
	channels[name] = Scan;
	return &channels[name];
}

void IAL::DeleteChan(ialChannelList::iterator y) {
	AutoMutex(hMutex);
	IAL_CHANNEL * Scan = &y->second;
	for (ialChannelNickList::iterator x = Scan->nicks.begin(); x != Scan->nicks.end(); x++) {
		DeleteNick(x->Nick);
	}
	Scan->nicks.clear();
	channels.erase(y);
}

void IAL::DeleteChan(IAL_CHANNEL * Scan) {
	AutoMutex(hMutex);
	for (ialChannelList::iterator y = channels.begin(); y != channels.end(); y++) {
		if (&y->second == Scan) {
			DeleteChan(y);
			break;
		}
	}
}

void IAL::DeleteChan(const char * name) {
	AutoMutex(hMutex);
	ialChannelList::iterator y = channels.find(name);
	if (y != channels.end()) {
		DeleteChan(y);
	}
}

void IAL::DeleteNick(IAL_CHANNEL * chan, IAL_NICK * Nick) {
	AutoMutex(hMutex);
	for (ialChannelNickList::iterator x = chan->nicks.begin(); x != chan->nicks.end(); x++) {
		if (Nick == x->Nick) {
			DeleteNick(Nick);
			chan->nicks.erase(x);
			break;
		}
	}
}

void IAL::DeleteNick(IAL_NICK * Nick) {
	AutoMutex(hMutex);
	if (Nick->ref) { Nick->ref--; }
	if (Nick->ref == 0) {
		for (ialNickList::iterator x = nicks.begin(); x != nicks.end(); x++) {
			if (Nick == &(*x)) {
				nicks.erase(x);
				break;
			}
		}
	}
}

void IAL::DeleteNickFromAll(const char * nick) {
	AutoMutex(hMutex);
	IAL_NICK * Nick;// = GetNick(nick, true);
	for (ialChannelList::iterator x = channels.begin(); x != channels.end(); x++) {
		if ((Nick = GetNick(&x->second, nick, NULL, true)) != NULL) {
			DeleteNick(&x->second, Nick);
		}
	}
}

void IAL::ChangeNick(const char * sold, const char * snew) {
	AutoMutex(hMutex);
	for (ialNickList::iterator x = nicks.begin();x != nicks.end(); x++) {
		if (!stricmp(x->nick.c_str(), sold)) {
			x->nick = snew;
			x->hostmask = "";
			break;
		}
	}
}

void IAL::UpdateHostmask(IAL_NICK * Nick, const char * hostmask) {
	AutoMutex(hMutex);
	if (strcmp(Nick->hostmask.c_str(), hostmask)) {
		Nick->hostmask = hostmask;
	}
}

bool IAL::IsNickOnChan(const char * chan, const char * nick) {
	AutoMutex(hMutex);
	IAL_CHANNEL * Chan = GetChan((char *)chan, true);
	if (Chan) {
		if (GetNick(Chan, nick, NULL, true) != NULL) {
			return true;
		}
	}
	return false;
}

IAL_NICK * IAL::GetNick(const char * nick, bool no_create) {
	AutoMutex(hMutex);
	for (ialNickList::iterator x = nicks.begin(); x != nicks.end(); x++) {
		if (!stricmp(x->nick.c_str(), nick)) {
			return &(*x);
		}
	}

	if (no_create) { return NULL; }

	IAL_NICK Scan;
	Scan.nick = nick;
	nicks.push_back(Scan);
	return GetNick(nick, true);
}

IAL_NICK * IAL::GetNickByHostmask(const char * nick) {
	AutoMutex(hMutex);
	for (ialNickList::iterator x = nicks.begin(); x != nicks.end(); x++) {
		if (!wildicmp(x->hostmask.c_str(),nick)) {
			return &(*x);
		}
	}
	return NULL;
}

IAL_CHANNEL_NICKS * IAL::GetNickChanEntry(IAL_CHANNEL * chan, const char * nick, bool no_create) {
	AutoMutex(hMutex);

	for (ialChannelNickList::iterator x = chan->nicks.begin(); x != chan->nicks.end(); x++) {
		if (!stricmp(x->Nick->nick.c_str(),nick)) {
			return &(*x);
		}
	}

	if (no_create) { return NULL; }

	IAL_CHANNEL_NICKS Scan;
	IAL_NICK * Nick = GetNick(nick);
	Nick->ref++;
	Scan.Nick = Nick;
	chan->nicks.push_back(Scan);
	return GetNickChanEntry(chan, nick, true);
}

IAL_NICK * IAL::GetNick(IAL_CHANNEL * chan, const char * nick, const char * hostmask, bool no_create) {
	AutoMutex(hMutex);

	IAL_CHANNEL_NICKS * Scan = GetNickChanEntry(chan, nick, no_create);
	if (Scan) {
		if (hostmask) { UpdateHostmask(Scan->Nick, hostmask); }
		return Scan->Nick;
	}

	return NULL;
}

void IAL::Clear() {
	AutoMutex(hMutex);

	for (ialChannelList::iterator x = channels.begin(); x != channels.end(); x = channels.begin()) {
		DeleteChan(x);
	}
}

void IAL::Dump() {
	AutoMutex(hMutex);
	FILE * fp = fopen("ial.dump.txt", "wb");
	if (fp) {
		for (ialChannelList::iterator x = channels.begin(); x != channels.end(); x++) {
			fprintf(fp, _("Channel %s (%d nicks)\n"), x->first.c_str(), x->second.nicks.size());
			for (ialChannelNickList::iterator y = x->second.nicks.begin(); y != x->second.nicks.end(); y++) {
				fprintf(fp, _("\tNick %s (refcnt: %d) (chumodes: %02X)\n"), y->Nick->nick.c_str(), y->Nick->ref, y->chumode);
			}
		}

		fprintf(fp, "%s", _("\nKnown nicks\n\n"));
		for (ialNickList::iterator y = nicks.begin(); y != nicks.end(); y++) {
			fprintf(fp, _("Nick %s (refcnt: %d) (hostmask: %s)\n"), y->nick.c_str(), y->ref, y->hostmask.c_str());
		}

		fclose(fp);
	}
}

#endif // defined(IRCBOT_ENABLE_SS)
