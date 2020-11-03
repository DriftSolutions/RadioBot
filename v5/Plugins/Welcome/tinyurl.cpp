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

void DoTinyURL(WEB_QUEUE * Scan) {
	char buf[1024];

	char fn[1024];
	//md5((unsigned char *)Scan->data, strlen(Scan->data), buf);
	hashdata("md5", (unsigned char *)Scan->data, strlen(Scan->data), buf, sizeof(buf));
	sprintf(fn, "./tmp/tinyurl.%s.tmp", buf);
	stringstream url;
	char * tmp = api->curl->escape(Scan->data, strlen(Scan->data));
	url << "http://tinyurl.com/api-create.php?url=" << tmp;
	api->curl->free(tmp);

	CACHED_DOWNLOAD_OPTIONS opts;
	memset(&opts, 0, sizeof(opts));
	opts.cache_time_limit = 3600;
	char * file = get_cached_download(url.str().c_str(), fn, buf, sizeof(buf), &opts);
	if (file) {
		strtrim(file);
		snprintf(buf, sizeof(buf), "Short URL for '%s': %s", Scan->data, file);
		Scan->pres->std_reply(Scan->pres, buf);
		zfree(file);
	} else {
		snprintf(buf, sizeof(buf), "Error shortening URL: %s!", buf);
		Scan->pres->std_reply(Scan->pres, buf);
	}
}
