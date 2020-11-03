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
#include "../ChanAdmin/chanadminapi.h"

extern BOTAPI_DEF * api;
extern int pluginnum;

struct SAM_CONFIG {
	char mysql_host[128];
	char mysql_user[128];
	char mysql_pass[128];
	char mysql_db[128];
	int mysql_port;

	char SAM_Host[128];
	int SAM_Port;

	CHANADMIN_INTERFACE ca;

	char MatchDJ[256];
	char FindTrigger[256];
	bool EnableRequests;
	bool EnableFind;
	bool AuxData;
	int WildMode;
	int MinReqTimePerSong;
	int MinReqTimePerUser;
	int MinReqTimePerUserKick;
	int MinReqTimePerUserBanTime;
	int MaxNextResults;
	char NoRequestFilters[1024];
	int NoReqFiltersKick;
	int NoReqFiltersBanTime;

	int AdjustWeight;
	int AdjustWeightOnRating[10];
	int AdjustWeightOnRatingTime;

	bool EnableYP;
	YP_HANDLE yp_handle;
	time_t yp_last_try;
	char yp_genre[64];

	int32 nofiles;
	bool playing, lastPlaying;

	int requestNumber;
};

extern SAM_CONFIG config;
extern DB_MySQL conx;
extern Titus_Mutex reqMutex;

struct SAM_SONG {
	int32 ID;
	char * fn;
	int32 duration;

	char * artist;
	char * title;
};

#define MAX_RESULTS 256
struct SONG_FIND_RESULT {
	int32 num;
	bool not_enough;
	SAM_SONG * results[MAX_RESULTS];
};

SONG_FIND_RESULT * FindWild(char * pat);
void FreeFindResult(SONG_FIND_RESULT * qRes);

SAM_SONG * GetSongFromID(int32 id);
SAM_SONG * FindFile(const char * fn);
void FreeSong(SAM_SONG * Scan);

void AdjustWeight(int id, int amount);
double GetNextSortID();
bool AllowRequest(SAM_SONG * Scan, USER_PRESENCE * ut, char * buf, int bufSize);

extern REQUEST_INTERFACE sam_req_iface;
