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

#include "autodj.h"

int sTime=0,cTime=0,lTime=0;
countdownList countdowns;

int32 GetTimeLeft(bool secs) {
	int32 ret = sTime - cTime;
	if (secs) {
		ret /= 1000;
	}
	return ret;
}

void SendReportText(REPORT * rep, char * text) {
	//char buf[1024];
	switch(rep->type) {
		case REP_TYPE_PRESENCE:
			rep->pres->std_reply(rep->pres, text);
			break;
		case REP_TYPE_REMOTE:
			REMOTE_HEADER rHead;
			rHead.scmd = RCMD_GENERIC_MSG;
			rHead.datalen = strlen(text);
			api->SendRemoteReply(rep->sock, &rHead, text);
			break;
		case REP_TYPE_NONE:
			break;
	}
}

void Timing_Reset(int32 SongLen) {
	LockMutexPtr(QueueMutex);
	countdowns.clear();
	RelMutexPtr(QueueMutex);
	sTime = SongLen;
	cTime = 0;
	lTime = -1;
	DRIFT_DIGITAL_SIGNATURE();
#ifdef _DEBUG
	api->ib_printf("SimpleDJ -> Timing_Reset(%d) -> %d secs\n",SongLen,SongLen / 1000);
#endif
}

void Timing_Add(int32 iTime) {
	cTime += iTime;

	/*
	REP_TYPE_NONE,
	REP_TYPE_IRC,
	REP_TYPE_OUTPUT,
	REP_TYPE_REMOTE
};

struct REPORT {
	REPORT_TYPES type;
	char nick[64];
	union {
		int32 ircnet;
		CommandProcType output;
	};
	T_SOCKET * sock;
};
*/

	int32 left = (sTime - cTime) / 1000;
	if (left == lTime) { return; }

	lTime = left;
	if ( (left >= 30 && (left % 30) == 0) || (left > 0 && left < 30 && (left % 5) == 0)) {
		char buf[256];
		char timebuf[32];
		PrettyTime(left, timebuf);
		#ifdef _DEBUG
			api->ib_printf("SimpleDJ -> Left: %s (%d)\n",timebuf,left);
		#endif

		if (sd_config.countdown) {
			if (sd_config.repto) { // to a remote link
				sprintf(buf,"Song will end in %s...",timebuf);
				sd_config.repto->std_reply(sd_config.repto, buf);
			}
		}

		sprintf(buf,"Song will end in %s...",timebuf);
		LockMutexPtr(QueueMutex);
		for (countdownList::const_iterator x = countdowns.begin(); x != countdowns.end(); x++) {
			SendReportText((REPORT *)&(*x), buf);
		}
		RelMutexPtr(QueueMutex);
	}
}

void Timing_Done() {
	if (sd_config.countdown) {
		sd_config.playing=false;
		sd_config.countdown = false;
		sd_config.playagainat = time(NULL)+10;
		char buf[256];

		sprintf(buf,"SimpleDJ -> Ordering Feeders to disconnect for %s...\n",sd_config.repto->Nick);
		if (api->irc) { api->irc->LogToChan(buf); }
		api->ib_printf(buf);

		if (sd_config.repto) { // to a remote link
			sd_config.repto->std_reply(sd_config.repto, "You have 10 seconds to connect before I take back over...");
		}
	}
	LockMutexPtr(QueueMutex);
	for (countdownList::const_iterator x = countdowns.begin(); x != countdowns.end(); x++) {
		SendReportText((REPORT *)&(*x), "The current song is over now...");
	}
	countdowns.clear();
	RelMutexPtr(QueueMutex);
	DRIFT_DIGITAL_SIGNATURE();
}

