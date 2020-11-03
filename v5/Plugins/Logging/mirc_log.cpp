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

#include "logging.h"

mIRC_Log::mIRC_Log(int n, std::string chan) {
	netno = n;
	channel = chan;
	fp = NULL;
	lastTry = 0;
	lastDay = 0;
}

mIRC_Log::~mIRC_Log() {
	CloseLog();
}

std::string mIRC_Log::CalcFN() {
	hMutex.Lock();
	time_t ts = time(NULL);
	tm tme;
	localtime_r(&ts, &tme);
	char buf[MAX_PATH], buf2[16];
	strftime(buf, sizeof(buf), log_config.log_datemask, &tme);
	std::string date = buf;
	sstrcpy(buf, log_config.log_filemask);
	sprintf(buf2, "%d", netno);
	str_replace(buf, sizeof(buf), "%netno%", buf2);
	str_replace(buf, sizeof(buf), "%chan%", channel.c_str());
	str_replace(buf, sizeof(buf), "%date%", date.c_str());
	std::string ret = log_config.log_dir;
	ret += PATH_SEPS;
	ret += buf;
	hMutex.Release();
	return ret;
}

bool mIRC_Log::OpenLog() {
	hMutex.Lock();
	if (fp) {
		CloseLog();
	}
	if ((time(NULL)-lastTry) < 30) { return false; }
	lastTry = time(NULL);

	time_t ts = time(NULL);
	tm tme;
	localtime_r(&ts, &tme);
	lastDay = tme.tm_mday;

	curFN = CalcFN();

	api->ib_printf(_("Logging Plugin -> Attempting to open log file: %s\n"), curFN.c_str());
	fp = fopen(curFN.c_str(), "ab");
	bool ret = false;
	if (fp) {
		api->ib_printf(_("Logging Plugin -> Log file opened successfully\n"));
		WriteStartHeader();
		ret = true;
	} else {
		int n = errno;
		api->ib_printf(_("Logging Plugin -> Error opening log file: %s -> %s\n"), curFN.c_str(), strerror(n));
	}
	hMutex.Release();
	return ret;
}

void mIRC_Log::WriteLine(std::string line) {
	hMutex.Lock();
	if (!fp) { OpenLog(); }
	time_t ts = time(NULL);
	tm tme;
	localtime_r(&ts, &tme);
	if (fp) {
		if (lastDay != tme.tm_mday) {
			string tfn = CalcFN();
			if (!stricmp(tfn.c_str(),curFN.c_str())) {
				//new day, but no log rotation needed
				WriteMidHeader();
			} else {
				//attempt to rotate log
				CloseLog();
				OpenLog();
			}
		}
		lastDay = tme.tm_mday;
	}
	if (fp) {
		fprintf(fp, "[%02d:%02d] ", tme.tm_hour, tme.tm_min);
		fwrite(line.c_str(), line.length(), 1, fp);
		fflush(fp);
	}
	hMutex.Release();
}

void mIRC_Log::WriteStartHeader() {
	hMutex.Lock();
	if (fp) {
		time_t rawtime;
		time(&rawtime);
		fprintf(fp, "Session Start: %s\r\n", ctime(&rawtime));
		fprintf(fp, "Session Ident: %s\r\n", channel.c_str());
		fflush(fp);
	}
	hMutex.Release();
}

void mIRC_Log::WriteMidHeader() {
	hMutex.Lock();
	if (fp) {
		time_t rawtime;
		time(&rawtime);
		fprintf(fp, "Session Time: %s\r\n", ctime(&rawtime));
		fflush(fp);
	}
	hMutex.Release();
}

void mIRC_Log::WriteEndHeader() {
	hMutex.Lock();
	if (fp) {
		time_t rawtime;
		time(&rawtime);
		fprintf(fp, "Session Close: %s\r\n", ctime(&rawtime));
		fflush(fp);
	}
	hMutex.Release();
}

bool mIRC_Log::CloseLog() {
	hMutex.Lock();
	WriteEndHeader();
	if (fp) {
		api->ib_printf(_("Logging Plugin -> Closing log file: %s\n"), curFN.c_str());
		fclose(fp);
		fp = NULL;
	}
	hMutex.Release();
	return true;
}
