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

#include "welcome.h"

struct YOUTUBEINFO {
	char title[256];
	char poster[256];
	char views[256];
	char likes[256];
	char dislikes[256];
};

void DoYouTubeInfo(WEB_QUEUE * Scan) {
	char buf[2048], errbuf[1024];

	char fn[1024];
	sprintf(fn, "./tmp/youtube.%s.tmp", Scan->data);
	stringstream url;
	buf[0] = 0;
#define HMAC_KEY "zN!#tbjzQRjsCEll"
	hmacdata("sha256", (uint8 *)HMAC_KEY, strlen(HMAC_KEY), (uint8 *)Scan->data, strlen(Scan->data), buf, sizeof(buf));
	url << "https://www.shoutirc.com/youtubeinfo.php?id=" << Scan->data << "&conf=" << buf;

	char * file = get_cached_download(url.str().c_str(), fn, errbuf, sizeof(errbuf));
	if (file) {
		YOUTUBEINFO y;
		memset(&y, 0, sizeof(y));
		if (!stricmp(Get_INI_String_Memory(file, "result", "error", errbuf, sizeof(errbuf), "Unknown Error"), "ok")) {
			sstrcpy(y.title, Get_INI_String_Memory(file, "result", "title", buf, sizeof(buf), "Unknown Title"));
			sstrcpy(y.poster, Get_INI_String_Memory(file, "result", "poster", buf, sizeof(buf), "Unknown Poster"));
			sstrcpy(y.views, Get_INI_String_Memory(file, "result", "viewCount", buf, sizeof(buf), "-1"));
			sstrcpy(y.likes, Get_INI_String_Memory(file, "result", "numLikes", buf, sizeof(buf), "-1"));
			sstrcpy(y.dislikes, Get_INI_String_Memory(file, "result", "numDislikes", buf, sizeof(buf), "-1"));
			snprintf(buf, sizeof(buf), "YouTube Video %s: '%s' posted by '%s' (Views: %s, Likes: %s, Dislikes: %s)", Scan->data, y.title, y.poster, y.views, y.likes, y.dislikes);
			Scan->pres->std_reply(Scan->pres, buf);
		} else {
			snprintf(buf, sizeof(buf), "Error downloading video info: %s!", errbuf);
			Scan->pres->std_reply(Scan->pres, buf);
		}
		zfree(file);
	} else {
		snprintf(buf, sizeof(buf), "Error downloading video info: %s!", errbuf);
		Scan->pres->std_reply(Scan->pres, buf);
	}
}
