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

bool OpenLog() {
	bool ret = true;
	LockMutex(sesMutex);
	CloseLog();
	char mon[3], day[3], year[5], date[11];
	time_t t = time(NULL);
	tm tme;
	localtime_r(&t, &tme);
	sprintf(mon, "%02d", tme.tm_mon+1);
	sprintf(day, "%02d", tme.tm_mday);
	sprintf(year, "%04d", tme.tm_year+1900);
	sprintf(date, "%s.%s.%s", mon, day, year);
	if (config.base.log_file.length()) {
		if (strstr(config.base.log_file.c_str(),"%plug%") == NULL) {
			char fn[MAX_PATH]={0};
			if (strchr(config.base.log_file.c_str(), PATH_SEP) == NULL) {
				sstrcpy(fn, "." PATH_SEPS "logs" PATH_SEPS "");
			}
			strlcat(fn, config.base.log_file.c_str(), sizeof(fn));
			str_replace(fn, sizeof(fn), "%mon%", mon);
			str_replace(fn, sizeof(fn), "%day%", day);
			str_replace(fn, sizeof(fn), "%year%", year);
			str_replace(fn, sizeof(fn), "%date%", date);
			if (access(fn, 0) == 0) {
				backup_file(fn);
			}
			config.base.log_fps[0] = fopen(fn, "wb");
			if (config.base.log_fps[0]) {
				ib_printf(_("%s: Log file '%s' opened!\n"), IRCBOT_NAME, fn);
			} else {
				ib_printf(_("%s: Error opening log file '%s': %s!\n"), IRCBOT_NAME, fn, strerror(errno));
				ret = false;
			}
			for (int i=1; i <= MAX_PLUGINS; i++) {
				config.base.log_fps[i] = config.base.log_fps[0];
			}
		} else {
			char buf2[MAX_PATH];
			for (int i=-1; i < MAX_PLUGINS; i++) {
				if (i == -1 || config.plugins[i].fn.length()) {
					char fn[MAX_PATH]={0};
					if (strchr(config.base.log_file.c_str(), PATH_SEP) == NULL) {
						sstrcpy(fn, "." PATH_SEPS "logs" PATH_SEPS "");
					}
					strlcat(fn, config.base.log_file.c_str(), sizeof(fn));
					if (i == -1) {
						str_replace(fn, sizeof(fn), "%plug%", IRCBOT_NAME);
					} else {
						sstrcpy(buf2, nopath(config.plugins[i].fn.c_str()));
						char * p = strrchr(buf2, '.');
						if (p) { *p = 0; }
						str_replace(fn, sizeof(fn), "%plug%", buf2);
					}
					str_replace(fn, sizeof(fn), "%mon%", mon);
					str_replace(fn, sizeof(fn), "%day%", day);
					str_replace(fn, sizeof(fn), "%year%", year);
					str_replace(fn, sizeof(fn), "%date%", date);
					if (access(fn, 0) == 0) {
						backup_file(fn);
					}
					//config.base.log_fns[i+1] = fn;
					if ((config.base.log_fps[i+1] = fopen(fn, "wb")) != NULL) {
						ib_printf(_("%s: Log file '%s' opened!\n"), IRCBOT_NAME, fn);
					} else {
						ib_printf(_("%s: Error opening log file '%s': %s!\n"), IRCBOT_NAME, fn, strerror(errno));
						ret = false;
					}
				}
			}
		}
	}
	RelMutex(sesMutex);
	return ret;
}

void CloseLog() {
	LockMutex(sesMutex);

	if (config.base.log_file.length()) {
		if (config.base.log_fps[0]) {
			ib_printf(_("%s: Log file closed.\n"), IRCBOT_NAME);
		}
		if (strstr(config.base.log_file.c_str(),"%plug%") == NULL) {
			if (config.base.log_fps[0]) {
				fclose(config.base.log_fps[0]);
			}
		} else {
			for (int i=0; i <= MAX_PLUGINS; i++) {
				if (config.base.log_fps[i]) {
					fclose(config.base.log_fps[i]);
				}
			}
		}
	}
	memset(&config.base.log_fps, 0, sizeof(config.base.log_fps));
	RelMutex(sesMutex);
}

static char ib_time[512];
inline void PrintToLog(int pluginnum, const char * buf) {
	LockMutex(sesMutex);
	pluginnum++;
	if (pluginnum >= 0 && pluginnum <= MAX_PLUGINS) {
		if (config.base.log_fps[pluginnum]) {
			time_t tme;
			time(&tme);
			tm tml;
			localtime_r(&tme, &tml);
			strftime(ib_time, sizeof(ib_time), _("<%m/%d/%y@%I:%M:%S%p>"), &tml);
			fprintf(config.base.log_fps[pluginnum], "%s %s", ib_time, buf);
			fflush(config.base.log_fps[pluginnum]);
		}
	}
	RelMutex(sesMutex);
}

static char ib_buf[4096];
void ib_printf2(int pluginnum, const char * fmt, ...) {
	LockMutex(sesMutex);
	//CONSOLE_HEADER * cHead = (CONSOLE_HEADER *)&ib_buf;
	//cHead->comp=0;
	//cHead->decomp_len=0;
	char * buf = (char *)&ib_buf;// + sizeof(CONSOLE_HEADER);

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(ib_buf), fmt, ap);
	ib_buf[sizeof(ib_buf)-1] = 0;
	va_end(ap);

	if (!config.base.Forked) {
		printf("%s", buf);
		fflush(stdout);
	}

	SendMessage(-1, IB_LOG, buf, strlen(buf)+1);
	str_replace(ib_buf, sizeof(ib_buf), "\n", "\r\n");
	PrintToLog(pluginnum, buf);

	RelMutex(sesMutex);
}
