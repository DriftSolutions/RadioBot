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

#ifndef __TWITTER_H__
#define __TWITTER_H__

#include "../../src/plugins.h"
#include "twitcurl/twitcurl.h"

extern BOTAPI_DEF *api;
extern int pluginnum;

#define CONSUMER_KEY "RMWPLtiOgUhhigSR7VtCTA"
#define CONSUMER_SECRET "m76+oZ2Pm7aBrZvKiLe9lbegk5a+mauvlaiWm4Gcyq+ispKCvMCx"
//"cFFYewcNyUc2pOEmOXknFaSWmPncyd2WZJjzD8I"
//"m76+oZ2Pm7aBrZvKiLe9lbegk5a+mauvlaiWm4Gcyq+ispKCvMCx" // "cFFYewcNyUc2pOEmOXknFaSWmPncyd2WZJjzD8I"

//#define CLIENT	"src"
#define MAX_ACCOUNTS 16
#define MAX_TWITTER_TIMERS 16
#define VERSION "1.0"

#ifdef TWITTER_PLUGIN
class TWITTER_TIMERS {
	public:
		TWITTER_TIMERS() {
			account = -1;
			delaymin = delaymax = 0;
			nextFire = 0;
		}
		std::string action;
		std::vector<std::string> lines;
		int account;
		int32 delaymin, delaymax;
		int32 nextFire;
};

struct TWITTER_CONFIG {
	twitCurl * twitter;
	bool enabled, source_only, enable_interactive;
	time_t lastTweet;
	int SongInterval, SongIntervalSource, ReTweet;

	uint64 user_id;
	char user[128];
	char token_key[128];
	char token_secret[128];

	//time_t lastReadTweet, lastReadDM;
	uint64 lastReadTweetID, lastReadDMID;

	time_t watchdog;
	TT_THREAD_INFO * hThread;
};
extern TWITTER_CONFIG tw_config[MAX_ACCOUNTS];
extern TWITTER_TIMERS tw_timers[MAX_TWITTER_TIMERS];
#endif

extern Titus_Mutex TwitterMutex;
extern bool twitter_shutdown_now;
void PostTweet(int i, string msg, bool trunc = true);
bool GetScreenName(int ind, uint64 user_id, string& screen_name);
bool GetUserID(int ind, const string screen_name, uint64& user_id);

TT_DEFINE_THREAD(TwitterCheck);

sql_rows GetTable(const char * query, char **errmsg = NULL);

//bool lfm_handshake();
//bool lfm_scrobble(const char * artist, const char * album, const char * title, int songLen, time_t startTime);
//extern twitCurl * twitter;

#endif // __TWITTER_H__
