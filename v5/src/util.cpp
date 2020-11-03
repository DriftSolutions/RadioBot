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

#if defined(WIN32)
int64 tv_start = 0;
#include <VersionHelpers.h>
#else
#include <sys/time.h>
timeval tv_start = {0,0};

inline uint64 tv_2_ms(struct timeval * tv) {
    return ((tv->tv_sec * 1000) + (tv->tv_usec / 1000));
}
#endif

#if defined(WIN32)
typedef unsigned long long (WINAPI * GetTickCount64Type)(void);
static GetTickCount64Type ibGetTickCount64 = NULL;
#endif

void ircbot_init_cycles() {
#if defined(WIN32)
	static bool didCheck = false;
	if (!didCheck) {
		if (IsWindowsVistaOrGreater()) {
			HMODULE hMod = LoadLibraryA("kernel32");
			if (hMod != NULL) {
				ibGetTickCount64 = (GetTickCount64Type)GetProcAddress(hMod, "GetTickCount64");
			}
		}
		didCheck = true;
	}
	if (ibGetTickCount64 != NULL) {
		tv_start = ibGetTickCount64();
	} else {
		tv_start = GetTickCount();
	}
#else
	gettimeofday(&tv_start, NULL);
	tv_start.tv_usec = 0;
#endif
}

int64 ircbot_get_cycles() {
#if defined(WIN32)
	if (ibGetTickCount64 != NULL) {
		return ibGetTickCount64()-tv_start;
	}
	return GetTickCount()-tv_start;
#else
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64 ret = tv_2_ms(&tv) - tv_2_ms(&tv_start);
	return ret;
#endif
}

char * ircbot_cycles_ts(char * durbuf, size_t bufSize) {
	if (bufSize < 16) { return "<insufbuf>"; }
	int64 secs = ircbot_get_cycles();
	snprintf(durbuf, bufSize, "%011d", int(secs));
	memmove(&durbuf[9],&durbuf[8],4);
	durbuf[8] = '.';
	return durbuf;
}

