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

#include "show_schedule.h"

DB_MySQL conx;

bool mysql_connect() {
	int tries=5;
	bool suc = false;
	while (!suc && tries--) {
		api->ib_printf(_("Show Schedule -> Connecting to MySQL...\n"));
		suc = conx.Connect(showsched_config.mysql_host, showsched_config.mysql_user, showsched_config.mysql_pass, showsched_config.mysql_db, showsched_config.mysql_port);
		if (!suc && tries) {
			api->ib_printf(_("Show Schedule -> Error connecting to MySQL, trying again in 5 seconds...\n"));
			safe_sleep(5);
		}
	}
	if (suc) {
		conx.Query("CREATE TABLE IF NOT EXISTS `ShowSchedule`(ID INTEGER PRIMARY KEY AUTO_INCREMENT, Name VARCHAR(255) DEFAULT '', Date INT DEFAULT 0, HourStart INT DEFAULT 0, HourEnd INT DEFAULT 0, MinStart INT DEFAULT 0, MinEnd INT DEFAULT 0, DayOfWeek INT DEFAULT 0, Repetition TINYINT DEFAULT 0)");
	}
	return suc;
}
void mysql_disconnect() {
	conx.Disconnect();
}
bool mysql_ping() {
	return conx.Ping() == 0 ? true:false;
}

bool mysql_get_finish(const char * query, ScheduleEntryList * list) {
	MYSQL_RES * res = conx.Query(query);
	if (res && conx.NumRows(res) > 0) {
		SC_Row row;
		while (conx.FetchRow(res, row)) {
			ScheduleEntry e;
			e.id = strtoul(row.Get("ID").c_str(), NULL, 10);
			e.name = row.Get("Name").c_str();
			e.date = strtoul(row.Get("Date").c_str(), NULL, 10);
			e.hour_start = atoi(row.Get("HourStart").c_str());
			e.hour_end = atoi(row.Get("HourEnd").c_str());
			e.min_start = atoi(row.Get("MinStart").c_str());
			e.min_end = atoi(row.Get("MinEnd").c_str());
			e.day_of_week = atoi(row.Get("DayOfWeek").c_str());
			e.weekly = atoi(row.Get("Repetition").c_str()) ? true:false;
			list->push_back(e);
		}
	}
	conx.FreeResult(res);
	return true;
}

bool mysql_get_todays_shows(ScheduleEntryList * list) {
	stringstream query;
	char buf[128];
	time_t tme = time(NULL);
	struct tm t;
	localtime_r(&tme, &t);
	snprintf(buf, sizeof(buf), "%04d%02d%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday);
	query << "SELECT * FROM `ShowSchedule` WHERE Date='" << buf << "' OR DayOfWeek=" << t.tm_wday << " ORDER BY HourStart ASC, MinStart ASC";
	return mysql_get_finish(query.str().c_str(), list);
}
bool mysql_get_all_shows(ScheduleEntryList * list) {
	stringstream query;
	query << "SELECT * FROM `ShowSchedule` ORDER BY Date ASC, HourStart ASC, MinStart ASC";
	return mysql_get_finish(query.str().c_str(), list);
}
bool mysql_add_entry(ScheduleEntry * entry) {
	stringstream query;
	query << "INSERT INTO `ShowSchedule` (`Name`,`Date`,`HourStart`,`HourEnd`,`MinStart`,`MinEnd`,`DayOfWeek`,`Repetition`) VALUES (\"" << conx.EscapeString(entry->name.c_str()) << "\", " << entry->date << ", " << entry->hour_start << ", " << entry->hour_end << ", " << entry->min_start << ", " << entry->min_end;
	query << ", " << entry->day_of_week << ", " << entry->weekly << ");";
	conx.Query(query.str().c_str());
	return true;
}
bool mysql_del_entry(uint32 id) {
	stringstream query;
	query << "DELETE FROM `ShowSchedule` WHERE `ID`=" << id;
	conx.Query(query.str().c_str());
	return true;
}

SHOWSCHED_DB ssd_mysql = {
	mysql_connect,
	mysql_disconnect,
	mysql_ping,
	mysql_get_todays_shows,
	mysql_get_all_shows,
	mysql_add_entry,
	mysql_del_entry
};
