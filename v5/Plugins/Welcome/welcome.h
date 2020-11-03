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
#define TIXML_USE_STL
#include "../../../Common/tinyxml/tinyxml.h"

extern BOTAPI_DEF *api;
extern int pluginnum;

struct WELCOME_CONFIG {
	int nothreads;
	bool shutdown_now;
	char RegKey[128];

	bool pingDefaultPublic;
	int weatherLimit;
	char BingAccountKey[128];
	char WeatherKey[128];
};
extern WELCOME_CONFIG wel_config;
extern Titus_Sockets3 * sockets;

enum WEB_TYPES {
	Weather,
	Horoscope,
	SpinTheBottle,
	YouTubeInfo,
	TinyURL,
	WebSearch
	//Ping
};

struct WEB_QUEUE {
	void (*handler)(WEB_QUEUE * Scan);
	USER_PRESENCE * pres;
	USER_PRESENCE::userPresSendFunc send_func;

	char data[256];
	char data2[128];

	int stage;
	bool nofree;
	int64 req_time;
	/*
//	char nick[128];
	//int netno;

	union {
		bool doPublic;
	};
	*/
	bool getCelcius;

	//CommandOutputType output;
	//T_SOCKET * sock;

	WEB_QUEUE *Next;
};

struct PING_QUEUE {
	USER_PRESENCE * pres;
	char data[128];
	int64 req_time;
	bool doPublic;

	bool nofree;
	PING_QUEUE *Next;
};

void DoWeather(WEB_QUEUE * Scan);
void DoSpinTheBottle(WEB_QUEUE * Scan);
void DoYouTubeInfo(WEB_QUEUE * Scan);
void DoTinyURL(WEB_QUEUE * Scan);
void DoWebSearch(WEB_QUEUE * Scan);
void DoWordnik(WEB_QUEUE * Scan);

void strcpy_safe(char * dest, const char * src, size_t len);
void strcat_safe(char * dest, const char * src, size_t len);
int atoi_safe(const char * p, int iDefault=0);

struct CACHED_DOWNLOAD_OPTIONS {
	const char * userpwd;
	time_t cache_time_limit;
};

char * get_cached_download(const char * url, const char * fn, char * error, size_t errSize, CACHED_DOWNLOAD_OPTIONS * opts=NULL);
