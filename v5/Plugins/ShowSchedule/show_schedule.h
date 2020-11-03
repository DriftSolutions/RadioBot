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

extern BOTAPI_DEF * api;
extern int pluginnum;

class ScheduleEntry {
private:
public:
	ScheduleEntry() { id=0; day_of_week = 0; weekly = false; }
	uint32 id; //database_id
	string name;
	uint32 date;
	int hour_start;
	int hour_end;
	int min_start;
	int min_end;
	int day_of_week;
	bool weekly;
};
typedef std::vector<ScheduleEntry> ScheduleEntryList;

enum SHOWSCHED_DBTYPES {
	SSD_SQLITE,
	SSD_MYSQL
};

struct SHOWSCHED_DB {
	bool (*connect)();
	void (*disconnect)();
	bool (*ping)();
	bool (*get_todays_shows)(ScheduleEntryList * list);
	bool (*get_all_shows)(ScheduleEntryList * list);
	bool (*add_entry)(ScheduleEntry * entry);
	bool (*del_entry)(uint32 id);
};
extern SHOWSCHED_DB * showsched_db;
extern SHOWSCHED_DB ssd_sqlite;
extern SHOWSCHED_DB ssd_mysql;

struct SHOWSCHED_CONFIG {
	SHOWSCHED_DBTYPES db_type;
	char timezone[128];

	char mysql_host[128];
	char mysql_user[128];
	char mysql_pass[128];
	char mysql_db[128];
	int mysql_port;
};
extern SHOWSCHED_CONFIG showsched_config;
