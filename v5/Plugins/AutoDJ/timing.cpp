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
bool timingIsDone = true;
Titus_Mutex RepMutex;
countdownList countdowns;

int32 GetTimeLeft(bool secs) {
	int32 ret = sTime - cTime;
	if (secs) {
		ret /= 1000;
	}
	return ret;
}

/*
void SendReportText(REPORT * rep, char * text) {
	char buf[1024];
	switch(rep->type) {
		case REP_TYPE_IRC:
			sprintf(buf, "NOTICE %s :%s\r\n", rep->nick, text);
			api->SendIRC(rep->ircnet, buf, strlen(buf));
			break;
		case REP_TYPE_OUTPUT:
			rep->output(rep->sock, rep->nick, text);
			break;
		case REP_TYPE_REMOTE:
			REMOTE_HEADER rHead;
			rHead.cmd = RCMD_GENERIC_MSG;
			rHead.datalen = strlen(text);
			api->SendRemoteReply(rep->sock, &rHead, text);
			break;
	}
}
*/

void Timing_Reset(int32 SongLen) {
	LockMutex(RepMutex);
	for (countdownList::const_iterator x = countdowns.begin(); x != countdowns.end(); x++) {
		UnrefUser(*x);
	}
	countdowns.clear();
	timingIsDone = false;
	RelMutex(RepMutex);
	sTime = SongLen;
	cTime = 0;
	lTime = -1;
	DRIFT_DIGITAL_SIGNATURE();
#ifdef _DEBUG
	api->ib_printf2(pluginnum,_("AutoDJ -> Timing_Reset(%d) -> %d secs\n"),SongLen,SongLen / 1000);
#endif
}

void Timing_Add(int32 iTime) {
	cTime += iTime;

	int32 left = (sTime - cTime) / 1000;
	if (left == lTime) { return; }

	lTime = left;
#if defined(WIN32)
	if (ad_config.pTaskList) {
		ad_config.pTaskList->SetProgressValue(api->hWnd, cTime, sTime);
	}
#endif
	if ( (left >= 30 && (left % 30) == 0) || (left > 0 && left < 30 && (left % 5) == 0)) {
		char buf[256];
		char timebuf[32];
		PrettyTime(left, timebuf);
		api->ib_printf2(pluginnum,_("AutoDJ -> Time Left in Song: %s (%d)\n"),timebuf,left);

		if (ad_config.countdown) {
			sprintf(buf,_("Song will end in %s..."),timebuf);
			LockMutex(RepMutex);
			ad_config.repto->std_reply(ad_config.repto, buf);
			RelMutex(RepMutex);
		}

		sprintf(buf,_("Song will end in %s..."),timebuf);
		LockMutex(RepMutex);
		for (countdownList::const_iterator x = countdowns.begin(); x != countdowns.end(); x++) {
			(*x)->std_reply(*x, buf);
			//SendReportText(*x, buf);
		}
		RelMutex(RepMutex);
	}
}

void Timing_Done() {
	if (ad_config.countdown) {
		ad_config.playing = false;
		ad_config.countdown = false;
		ad_config.countdown_now = false;
		ad_config.playagainat = time(NULL)+ad_config.Options.DJConnectTime;
		char buf[256];

		sprintf(buf,_("AutoDJ -> Ordering Feeder to disconnect for %s..."), ad_config.repto->Nick);
		if (api->irc) { api->irc->LogToChan(buf); }
		api->ib_printf2(pluginnum,"%s\n", buf);

		sprintf(buf, _("You have %d seconds to connect before I take back over..."), ad_config.Options.DJConnectTime);
		LockMutex(RepMutex);
		ad_config.repto->std_reply(ad_config.repto, buf);
		UnrefUser(ad_config.repto);
		ad_config.repto = NULL;
		RelMutex(RepMutex);
	}
	LockMutex(RepMutex);
	for (countdownList::const_iterator x = countdowns.begin(); x != countdowns.end(); x++) {
		//SendReportText((REPORT *)&(*x), "The current song is over now...");
		(*x)->std_reply(*x, _("The current song is over now..."));
		UnrefUser(*x);
	}
	countdowns.clear();
	timingIsDone = true;
	RelMutex(RepMutex);
	DRIFT_DIGITAL_SIGNATURE();
}

bool Timing_IsDone() { return timingIsDone; }


void timer_init(AUTODJ_TIMER * t, uint64 denominator) {
	memset(t, 0, sizeof(AUTODJ_TIMER));
	t->den = denominator;
}

void timer_set(AUTODJ_TIMER * t, uint64 whole, uint64 numerator) {
	t->whole = whole;
	t->num = numerator;
}

void timer_add(AUTODJ_TIMER * t, uint64 val) {
	t->num += val;
	if (t->num >= t->den) {
		uint64 n = t->num / t->den;
		t->whole += n;
		t->num -= (n*t->den);
	}
}

void timer_zero(AUTODJ_TIMER * t) {
	t->whole = 0;
	t->num = 0;
}

uint64 timer_whole_value(AUTODJ_TIMER * t) {
	return (t->whole * 1000);
}
uint64 timer_value(AUTODJ_TIMER * t) {
	double x = t->num;
	x /= t->den;
	x *= 1000;
	return (t->whole * 1000) + x;
}
