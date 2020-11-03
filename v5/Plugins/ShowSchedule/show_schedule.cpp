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

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
SHOWSCHED_DB * showsched_db = NULL;
SHOWSCHED_CONFIG showsched_config;
Titus_Mutex showsched_mutex;

void addschedule_usage(USER_PRESENCE * ut) {
	char buf[1024];
	snprintf(buf, sizeof(buf), _("Usage: %caddschedule [weekly] YYYY-MM-DD HH:MM HH:MM Show Description"), api->commands->PrimaryCommandPrefix());
	ut->std_reply(ut, buf);
}

int ShowSchedule_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];
	if (!stricmp(command, "delschedule") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %cdelchedule number (use '!schedule all' to get the number)"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "delschedule")) {
		LockMutex(showsched_mutex);
		showsched_db->ping();
		showsched_db->del_entry(strtoul(parms, NULL, 10));
		ut->std_reply(ut, _("Entry deleted!"));
		RelMutex(showsched_mutex);
	}

	if (!stricmp(command, "addschedule") && (!parms || !strlen(parms))) {
		addschedule_usage(ut);
		return 1;
	}

	if (!stricmp(command, "addschedule")) {
		StrTokenizer st((char *)parms, ' ');
		ScheduleEntry e;
		e.weekly = false;
		int ind = 1;
		if (!stricmp(st.stdGetSingleTok(1).c_str(), "weekly")) {
			ind++;
			e.weekly = true;
		}
		unsigned int shave = e.weekly ? 5:4;
		if (st.NumTok() < shave) {
			addschedule_usage(ut);
			return 1;
		}
		{
			StrTokenizer st2((char *)st.stdGetSingleTok(ind).c_str(), '-');
			if (st2.NumTok() != 3) {
				addschedule_usage(ut);
				return 1;
			}
			int year = atoi(st2.stdGetSingleTok(1).c_str());
			int mon = atoi(st2.stdGetSingleTok(2).c_str());
			int day = atoi(st2.stdGetSingleTok(3).c_str());
			if (year < 1900 || year > 9999) {
				ut->std_reply(ut, "Invalid year!");
				return 1;
			}
			if (mon < 1 || mon > 12) {
				ut->std_reply(ut, "Invalid month!");
				return 1;
			}
			if (day < 1 || day > 31) {
				ut->std_reply(ut, "Invalid day!");
				return 1;
			}
			e.date = (year*10000) + (mon*100) + day;
			tm t;
			memset(&t, 0, sizeof(t));
			t.tm_hour = 12;
			t.tm_mday = day;
			t.tm_mon = mon-1;
			t.tm_year = year-1900;
			time_t tme = mktime(&t);
			localtime_r(&tme, &t);
			e.day_of_week = t.tm_wday;
			ind++;
		}
		{
			StrTokenizer st2((char *)st.stdGetSingleTok(ind).c_str(), ':');
			if (st2.NumTok() != 2) {
				addschedule_usage(ut);
				return 1;
			}
			int hour = atoi(st2.stdGetSingleTok(1).c_str());
			int min = atoi(st2.stdGetSingleTok(2).c_str());
			if (hour < 0 || hour > 23) {
				ut->std_reply(ut, "Invalid start hour!");
				return 1;
			}
			if (min < 0 || min > 59) {
				ut->std_reply(ut, "Invalid start minute!");
				return 1;
			}
			e.hour_start = hour;
			e.min_start = min;
			ind++;
		}
		{
			StrTokenizer st2((char *)st.stdGetSingleTok(ind).c_str(), ':');
			if (st2.NumTok() != 2) {
				addschedule_usage(ut);
				return 1;
			}
			int hour = atoi(st2.stdGetSingleTok(1).c_str());
			int min = atoi(st2.stdGetSingleTok(2).c_str());
			if (hour < 0 || hour > 23) {
				ut->std_reply(ut, "Invalid end hour!");
				return 1;
			}
			if (min < 0 || min > 59) {
				ut->std_reply(ut, "Invalid end minute!");
				return 1;
			}
			e.hour_end = hour;
			e.min_end = min;
			ind++;
		}
		e.name = st.stdGetTok(ind, st.NumTok());
		LockMutex(showsched_mutex);
		showsched_db->ping();
		showsched_db->add_entry(&e);
		RelMutex(showsched_mutex);
		ut->std_reply(ut, _("Schedule entry added!"));
		return 1;
	}

	if (!stricmp(command, "schedule") || !stricmp(command, "schedule_public")) {
		bool all = false;
		if ((parms && !stricmp(parms, "all")) && api->user->uflag_have_any_of(ut->Flags, UFLAG_MASTER|UFLAG_OP)) {
			all = true;
		}
		USER_PRESENCE::userPresSendFunc func = (stricmp(command, "schedule_public") == 0) ? ut->send_chan:ut->std_reply;

		ScheduleEntryList list;
		LockMutex(showsched_mutex);
		showsched_db->ping();
		bool res = all ? showsched_db->get_all_shows(&list):showsched_db->get_todays_shows(&list);
		RelMutex(showsched_mutex);
		if (res && list.size()) {
			func(ut, _("(** Today's Schedule **)"));
			for (size_t i=0; i < list.size(); i++) {
				if (all) {
					snprintf(buf, sizeof(buf), "[%d] %02d:%02d - %02d:%02d %s - %s", list[i].id, list[i].hour_start, list[i].min_start, list[i].hour_end, list[i].min_end, showsched_config.timezone, list[i].name.c_str());
				} else {
					snprintf(buf, sizeof(buf), "%02d:%02d - %02d:%02d %s - %s", list[i].hour_start, list[i].min_start, list[i].hour_end, list[i].min_end, showsched_config.timezone, list[i].name.c_str());
				}
				func(ut, buf);
				//19:00 - 21:00 CET - Top 10 Chart /w DJ Azkme
			}
			func(ut, _("(** End of list **)"));
		} else {
			func(ut, _("There are no shows scheduled for today..."));
		}
		return 1;
	}

	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = (BOTAPI_DEF *)BotAPI;
	pluginnum = num;
	memset(&showsched_config, 0, sizeof(showsched_config));
	sstrcpy(showsched_config.timezone, "EST");
	LockMutex(showsched_mutex);
	showsched_config.db_type = SSD_SQLITE;
	showsched_db = &ssd_sqlite;
	RelMutex(showsched_mutex);

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "ShowSchedule");
	if (sec) {
		const char * p = api->config->GetConfigSectionValue(sec, "DBType");
		if (p && !stricmp(p, "mysql")) {
			LockMutex(showsched_mutex);
			showsched_config.db_type = SSD_MYSQL;
			showsched_db = &ssd_mysql;
			RelMutex(showsched_mutex);
		}
		p = api->config->GetConfigSectionValue(sec, "TimeZone");
		if (p && p[0]) {
			sstrcpy(showsched_config.timezone, p);
		}
		DS_CONFIG_SECTION * ssec = api->config->GetConfigSection(sec, "MySQL");
		if (sec) {
			api->config->GetConfigSectionValueBuf(ssec, "Host", showsched_config.mysql_host, sizeof(showsched_config.mysql_host));
			api->config->GetConfigSectionValueBuf(ssec, "User", showsched_config.mysql_user, sizeof(showsched_config.mysql_user));
			api->config->GetConfigSectionValueBuf(ssec, "Pass", showsched_config.mysql_pass, sizeof(showsched_config.mysql_pass));
			api->config->GetConfigSectionValueBuf(ssec, "DBName", showsched_config.mysql_db, sizeof(showsched_config.mysql_db));
			if (api->config->GetConfigSectionLong(ssec, "Port") > 0) {
				showsched_config.mysql_port = api->config->GetConfigSectionLong(ssec, "Port");
			}
		} else {
			api->ib_printf(_("Show Schedule -> No 'MySQL' section found in your 'ShowSchedule' section, using pure defaults\n"));
		}
	}

	LockMutex(showsched_mutex);
	if (!showsched_db->connect()) {
		RelMutex(showsched_mutex);
		return 0;
	}
	RelMutex(showsched_mutex);

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_DJ, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "addschedule", &acl, CM_ALLOW_ALL_PRIVATE, ShowSchedule_Commands, "This command will let you add a show to the schedule.");
	api->commands->RegisterCommand_Proc(pluginnum, "schedule_public", &acl, CM_ALLOW_IRC_PUBLIC, ShowSchedule_Commands, "This command will show you any shows scheduled for today in the current channel.");

	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "delschedule", &acl, CM_ALLOW_ALL_PRIVATE, ShowSchedule_Commands, "This command will let you delete a show from the schedule.");

	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "schedule", &acl, CM_ALLOW_ALL_PUBLIC|CM_ALLOW_ALL_PRIVATE, ShowSchedule_Commands, "This command will show you any shows scheduled for today.");

	return 1;
}

void quit() {
	api->ib_printf(_("Show Schedule -> Shutting down...\n"));
	api->commands->UnregisterCommandsByPlugin(pluginnum);
	LockMutex(showsched_mutex);
	showsched_db->disconnect();
	RelMutex(showsched_mutex);
	api->ib_printf(_("Show Schedule -> Shut down.\n"));
}

int message(unsigned int msg, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{8F39A711-57A4-4B42-92DA-3BD0A1DFB5C9}",
	"Show Schedule",
	"Show Schedule Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
