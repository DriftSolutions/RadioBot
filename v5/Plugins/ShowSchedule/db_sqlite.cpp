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

sql_rows GetTable(const char * query, char **errmsg=NULL) {
//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = api->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		api->ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
	} else {
		if (ncol && nrow) {
			for (int i=0; i < nrow; i++) {
				sql_row row;
				for (int s=0; s < ncol; s++) {
					char * p = resultp[(i*ncol)+ncol+s];
					if (!p) { p=""; }
					row[resultp[s]] = p;
				}
				rows[i] = row;
			}
		}
		api->db->FreeTable(resultp);
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}


bool sqlite_connect() {
	//api->db->Query("DROP TABLE ShowSchedule", NULL, NULL, NULL);
	api->db->Query("CREATE TABLE IF NOT EXISTS ShowSchedule(ID INTEGER PRIMARY KEY, Name VARCHAR(255), Date INT, HourStart INT, HourEnd INT, MinStart INT, MinEnd INT, DayOfWeek INT, Repetition TINYINT);", NULL, NULL, NULL);
	return true;
}
void sqlite_disconnect() {
}
bool sqlite_ping() {
	return true;
}

bool sqlite_get_finish(const char * query, ScheduleEntryList * list) {
	sql_rows res = GetTable(query);
	for (size_t i=0; i < res.size(); i++) {
		ScheduleEntry e;
		e.id = strtoul(res[i].find("ID")->second.c_str(), NULL, 10);
		e.name = res[i].find("Name")->second.c_str();
		e.date = strtoul(res[i].find("Date")->second.c_str(), NULL, 10);
		e.hour_start = atoi(res[i].find("HourStart")->second.c_str());
		e.hour_end = atoi(res[i].find("HourEnd")->second.c_str());
		e.min_start = atoi(res[i].find("MinStart")->second.c_str());
		e.min_end = atoi(res[i].find("MinEnd")->second.c_str());
		e.day_of_week = atoi(res[i].find("DayOfWeek")->second.c_str());
		e.weekly = atoi(res[i].find("Repetition")->second.c_str()) ? true:false;
		list->push_back(e);
	}
	return true;
}

bool sqlite_get_todays_shows(ScheduleEntryList * list) {
	stringstream query;
	char buf[128];
	time_t tme = time(NULL);
	struct tm t;
	localtime_r(&tme, &t);
	snprintf(buf, sizeof(buf), "%04d%02d%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday);
	query << "SELECT * FROM ShowSchedule WHERE Date='" << buf << "' OR DayOfWeek=" << t.tm_wday << " ORDER BY HourStart ASC, MinStart ASC";
	return sqlite_get_finish(query.str().c_str(), list);
}
bool sqlite_get_all_shows(ScheduleEntryList * list) {
	stringstream query;
	query << "SELECT * FROM ShowSchedule ORDER BY Date ASC, HourStart ASC, MinStart ASC";
	return sqlite_get_finish(query.str().c_str(), list);
}
bool sqlite_add_entry(ScheduleEntry * entry) {
	stringstream query;
	char * tmp = api->db->MPrintf("%q", entry->name.c_str());
	query << "INSERT INTO ShowSchedule (ID,Name,Date,HourStart,HourEnd,MinStart,MinEnd,DayOfWeek,Repetition) VALUES (NULL, '" << tmp << "', " << entry->date << ", " << entry->hour_start << ", " << entry->hour_end << ", " << entry->min_start << ", " << entry->min_end;
	query << ", " << entry->day_of_week << ", " << entry->weekly << ");";
	api->db->Query(query.str().c_str(), NULL, NULL, NULL);
	api->db->Free(tmp);
	return true;
}
bool sqlite_del_entry(uint32 id) {
	stringstream query;
	query << "DELETE FROM ShowSchedule WHERE ID=" << id;
	api->db->Query(query.str().c_str(), NULL, NULL, NULL);
	return true;
}

SHOWSCHED_DB ssd_sqlite = {
	sqlite_connect,
	sqlite_disconnect,
	sqlite_ping,
	sqlite_get_todays_shows,
	sqlite_get_all_shows,
	sqlite_add_entry,
	sqlite_del_entry
};
