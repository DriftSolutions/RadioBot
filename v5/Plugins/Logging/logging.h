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

extern BOTAPI_DEF *api;
extern int pluginnum; // the number we were assigned

struct LOG_CONFIG {
	char log_datemask[MAX_PATH];
	char log_filemask[MAX_PATH];
	char log_dir[MAX_PATH];
};

extern LOG_CONFIG log_config;

class mIRC_Log {
private:
	Titus_Mutex hMutex;
	time_t lastTry;
	int lastDay;
	FILE * fp;
	std::string curFN;
public:
	int netno;
	std::string channel;
public:
	mIRC_Log(int netno, std::string chan);
	~mIRC_Log();

	std::string CalcFN();

	bool OpenLog();

	void WriteLine(std::string line);
	void WriteStartHeader();
	void WriteMidHeader();
	void WriteEndHeader();

	bool CloseLog();
};
