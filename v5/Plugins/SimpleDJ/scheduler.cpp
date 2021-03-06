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

#include "autodj.h"

Titus_Mutex sMutex;
SCHEDULE *fSched=NULL,*lSched=NULL;

void FreeSchedule();
void LoadSchedule();

void FreeSchedule() {
	sMutex.Lock();

	SCHEDULE * Scan = fSched;
	while (Scan) {
		SCHEDULE * toDel = Scan;
		Scan = Scan->Next;
		delete toDel;
	}

	fSched=NULL;
	lSched=NULL;
	sMutex.Release();
}

void AddToSchedule(SCHEDULE * sc) {
	SCHEDULE * Scan = new SCHEDULE;
	memcpy(Scan, sc, sizeof(SCHEDULE));

	Scan->Next = NULL;
	if (fSched) {
		Scan->Prev = lSched;
		lSched->Next = Scan;
		lSched = Scan;
	} else {
		Scan->Prev = NULL;
		fSched = Scan;
		lSched = Scan;
	}
}

void FillMonths(SCHEDULE * sc, char * months, int line) {
	unsigned int i=0;
	for (i=0; i < strlen(months); i++) {
		switch(months[i]) {
			case 'Z':
				sc->timer.months |= MONTH_JAN|MONTH_FEB|MONTH_MAR|MONTH_APR|MONTH_MAY|MONTH_JUN|MONTH_JUL|MONTH_AUG|MONTH_SEP|MONTH_OCT|MONTH_NOV|MONTH_DEC;
				break;
			case 'J':
				sc->timer.months |= MONTH_JAN;
				break;
			case 'F':
				sc->timer.months |= MONTH_FEB;
				break;
			case 'M':
				sc->timer.months |= MONTH_MAR;
				break;
			case 'A':
				sc->timer.months |= MONTH_APR;
				break;
			case 'Y':
				sc->timer.months |= MONTH_MAY;
				break;
			case 'U':
				sc->timer.months |= MONTH_JUN;
				break;
			case 'L':
				sc->timer.months |= MONTH_JUL;
				break;
			case 'G':
				sc->timer.months |= MONTH_AUG;
				break;
			case 'S':
				sc->timer.months |= MONTH_SEP;
				break;
			case 'O':
				sc->timer.months |= MONTH_OCT;
				break;
			case 'N':
				sc->timer.months |= MONTH_NOV;
				break;
			case 'D':
				sc->timer.months |= MONTH_DEC;
				break;
			case ',':
				break;
			default:
				api->ib_printf("SimpleDJ (scheduler) -> Unknown month code '%c' on line %d\n", months[i], line);
				break;
		};
	}
}

void FillDays(SCHEDULE * sc, char * days, int line) {
	unsigned int i=0;
	for (i=0; i < strlen(days); i++) {
		switch(days[i]) {
			case 'Z':
				sc->timer.days |= DAY_SUN|DAY_MON|DAY_TUE|DAY_WED|DAY_THU|DAY_FRI|DAY_SAT;
				break;
			case 'E':
				sc->timer.days |= DAY_MON|DAY_TUE|DAY_WED|DAY_THU|DAY_FRI;
				break;
			case 'K':
				sc->timer.days |= DAY_SUN|DAY_SAT;
				break;
			case 'S':
				sc->timer.days |= DAY_SUN;
				break;
			case 'M':
				sc->timer.days |= DAY_MON;
				break;
			case 'T':
				sc->timer.days |= DAY_TUE;
				break;
			case 'W':
				sc->timer.days |= DAY_WED;
				break;
			case 'H':
				sc->timer.days |= DAY_THU;
				break;
			case 'F':
				sc->timer.days |= DAY_FRI;
				break;
			case 'A':
				sc->timer.days |= DAY_SAT;
				break;
			case ',':
				break;
			default:
				api->ib_printf("SimpleDJ (scheduler) -> Unknown day code '%c' on line %d\n", days[i], line);
				break;
		};
	}
}

void LoadSchedule() {
	FreeSchedule();
	sMutex.Lock();
	FILE * fp = fopen("schedule.conf", "rb");
	char buf[4096];
	if (fp) {
		int line=0;
		memset(buf, 0, sizeof(buf));
		while (!feof(fp) && fgets(buf, sizeof(buf), fp)) {
			line++;
			strtrim(buf, "\n\r\t ");
//			printf("Line: %d, %s\n", line, buf);
			if (strlen(buf) && buf[0] != '#' && buf[0] != '\\') {
//				printf("Line: %s\n", buf);
				StrTokenizer * st = new StrTokenizer(buf, ' ');
				char *type = st->GetSingleTok(1);
				if (!stricmp(type, "Timer")) {
					st->FreeString(type);
					SCHEDULE * sc = new SCHEDULE;
					memset(sc, 0, sizeof(SCHEDULE));
					sc->type = SCHEDULE_TYPE_TIMER;
					if (st->NumTok() >= 8) {
						type = st->GetSingleTok(2);
						if (!stricmp(type, "req")) {
							sc->timer.type = TIMER_TYPE_REQ;
						} else if (!stricmp(type, "override")) {
							sc->timer.type = TIMER_TYPE_OVERRIDE;
						} else if (!stricmp(type, "filter")) {
							sc->timer.type = TIMER_TYPE_FILTER;
						} else if (!stricmp(type, "script")) {
							sc->timer.type = TIMER_TYPE_SCRIPT;
						} else {
							api->ib_printf("SimpleDJ (scheduler) -> Unknown timer type '%s' on line %d\n", type, line);
							st->FreeString(type);
							delete sc;
							delete st;
							continue;
						}
						st->FreeString(type);
						char * months = st->GetSingleTok(3);
						strtrim(months," \t");
						FillMonths(sc, months, line);
						st->FreeString(months);
						if (sc->timer.months) {
							char * days = st->GetSingleTok(4);
							strtrim(days," \t");
							FillDays(sc, days, line);
							st->FreeString(days);
							if (sc->timer.days) {
								char * hours = st->GetSingleTok(5);
								StrTokenizer * stt = new StrTokenizer(hours, ',');
								unsigned int i=0;
								for (i=0; i < stt->NumTok(); i++) {
									char *hr = stt->GetSingleTok(i+1);
									int iHr = atol(hr);
									switch(iHr) {
										case 0:
											sc->timer.hours |= HOUR_00;
											break;
										case 1:
											sc->timer.hours |= HOUR_01;
											break;
										case 2:
											sc->timer.hours |= HOUR_02;
											break;
										case 3:
											sc->timer.hours |= HOUR_03;
											break;
										case 4:
											sc->timer.hours |= HOUR_04;
											break;
										case 5:
											sc->timer.hours |= HOUR_05;
											break;
										case 6:
											sc->timer.hours |= HOUR_06;
											break;
										case 7:
											sc->timer.hours |= HOUR_07;
											break;
										case 8:
											sc->timer.hours |= HOUR_08;
											break;
										case 9:
											sc->timer.hours |= HOUR_09;
											break;
										case 10:
											sc->timer.hours |= HOUR_10;
											break;
										case 11:
											sc->timer.hours |= HOUR_11;
											break;
										case 12:
											sc->timer.hours |= HOUR_12;
											break;
										case 13:
											sc->timer.hours |= HOUR_13;
											break;
										case 14:
											sc->timer.hours |= HOUR_14;
											break;
										case 15:
											sc->timer.hours |= HOUR_15;
											break;
										case 16:
											sc->timer.hours |= HOUR_16;
											break;
										case 17:
											sc->timer.hours |= HOUR_17;
											break;
										case 18:
											sc->timer.hours |= HOUR_18;
											break;
										case 19:
											sc->timer.hours |= HOUR_19;
											break;
										case 20:
											sc->timer.hours |= HOUR_20;
											break;
										case 21:
											sc->timer.hours |= HOUR_21;
											break;
										case 22:
											sc->timer.hours |= HOUR_22;
											break;
										case 23:
											sc->timer.hours |= HOUR_23;
											break;
										case ',':
											break;
										default:
											api->ib_printf("SimpleDJ (scheduler) -> Unknown hour '%s' on line %d\n", hr, line);
											break;
									};
									stt->FreeString(hr);
								}
								delete stt;
								st->FreeString(hours);
								if (sc->timer.hours) {
									char * minutes = st->GetSingleTok(6);
									sc->timer.minutes = atol(minutes);
									st->FreeString(minutes);
									minutes = st->GetSingleTok(7);
									sc->timer.extra = atol(minutes);
									st->FreeString(minutes);

									char * pat_type = st->GetSingleTok(8);
									if (!stricmp(pat_type, "filename")) {
										sc->timer.pat_type = TIMER_PAT_TYPE_FILENAME;
									} else if (!stricmp(pat_type, "genre")) {
										sc->timer.pat_type = TIMER_PAT_TYPE_GENRE;
									} else if (!stricmp(pat_type, "artist")) {
										sc->timer.pat_type = TIMER_PAT_TYPE_ARTIST;
									} else if (!stricmp(pat_type, "album")) {
										sc->timer.pat_type = TIMER_PAT_TYPE_ALBUM;
									} else if (!stricmp(pat_type, "directory") || !stricmp(pat_type, "dir")) {
										sc->timer.pat_type = TIMER_PAT_TYPE_DIRECTORY;
									} else {
										api->ib_printf("SimpleDJ (scheduler) -> Unknown pattern type '%s' on line %d\n", pat_type, line);
									}
									st->FreeString(pat_type);
									pat_type = st->GetTok(9,st->NumTok());
									sstrcpy(sc->timer.pattern, pat_type);
									st->FreeString(pat_type);
									if (sc->timer.pat_type != 0) {
										//printf("AddToSchedule()\n");
										AddToSchedule(sc);
									}
								} else {
									api->ib_printf("SimpleDJ (scheduler) -> No hours are defined for timer on line %d\n", line);
								}
							}
						} else {
							api->ib_printf("SimpleDJ (scheduler) -> No months are defined for timer on line %d\n", line);
						}
					} else {
						api->ib_printf("SimpleDJ (scheduler) -> Not enough tokens for '%s' on line %d\n", type, line);
					}
					delete sc;
				} else {
					api->ib_printf("SimpleDJ (scheduler) -> Unknown entry type '%s' on line %d\n", type, line);
					st->FreeString(type);
				}
				delete st;
			}
			memset(buf, 0, sizeof(buf));
		}
		fclose(fp);
	} else {
		//api->ib_printf("SimpleDJ (scheduler) -> Error opening schedule.conf!\n");
	}
	sMutex.Release();
}

void ProcText(char * text, time_t tme, unsigned int bufSize) {
	char buf[4096];
	tm * ti = localtime(&tme);

	sprintf(buf, "%.4d", ti->tm_year+1900);
	str_replace(text, bufSize, "%yyyy", buf);
	sprintf(buf, "%02d", ti->tm_year);
	str_replace(text, bufSize, "%yy", buf);
	sprintf(buf, "%02d", ti->tm_mon+1);
	str_replace(text, bufSize, "%mm", buf);
	sprintf(buf, "%d", ti->tm_mon+1);
	str_replace(text, bufSize, "%m", buf);
	sprintf(buf, "%02d", ti->tm_mday);
	str_replace(text, bufSize, "%dd", buf);
	sprintf(buf, "%d", ti->tm_mday);
	str_replace(text, bufSize, "%d", buf);
	sprintf(buf, "%02d", ti->tm_wday);
	str_replace(text, bufSize, "%ww", buf);
	sprintf(buf, "%d", ti->tm_wday);
	str_replace(text, bufSize, "%w", buf);
	sprintf(buf, "%02d", ti->tm_hour);
	str_replace(text, bufSize, "%hh", buf);
	sprintf(buf, "%d", ti->tm_hour);
	str_replace(text, bufSize, "%h", buf);
	sprintf(buf, "%02d", ti->tm_min);
	str_replace(text, bufSize, "%nn", buf);
	sprintf(buf, "%d", ti->tm_min);
	str_replace(text, bufSize, "%n", buf);
	sprintf(buf, "%02d", ti->tm_sec);
	str_replace(text, bufSize, "%ss", buf);
	sprintf(buf, "%d", ti->tm_sec);
	str_replace(text, bufSize, "%s", buf);

	char *p=NULL;
	if ((p = strstr(text, "%lw("))) {
		char * ptr = strdup(p);
		char * end = strstr(ptr, ")");
		end[1]=0;
		str_replace(text, bufSize, ptr, "");
		end[0]=0;
		int count = atol(ptr+4);
		free(ptr);

		time_t tmp = tme;

		// lw(0)
		while (ti->tm_wday == 0 || ti->tm_wday == 6) {
			tmp -= (24*60*60);
			ti = localtime(&tmp);
		}

		while (count) {
			count--;
			tmp -= (24*60*60);
			ti = localtime(&tmp);
			while (ti->tm_wday == 0 || ti->tm_wday == 6) {
				tmp -= (24*60*60);
				ti = localtime(&tmp);
			}
		}

		sprintf(buf, "%04d", ti->tm_year+1900);
		str_replace(text, bufSize, "%lwyyyy", buf);
		sprintf(buf, "%02d", ti->tm_year);
		str_replace(text, bufSize, "%lwyy", buf);
		sprintf(buf, "%02d", ti->tm_mon+1);
		str_replace(text, bufSize, "%lwmm", buf);
		sprintf(buf, "%d", ti->tm_mon+1);
		str_replace(text, bufSize, "%lwm", buf);
		sprintf(buf, "%02d", ti->tm_mday);
		str_replace(text, bufSize, "%lwdd", buf);
		sprintf(buf, "%d", ti->tm_mday);
		str_replace(text, bufSize, "%lwd", buf);
		sprintf(buf, "%02d", ti->tm_wday);
		str_replace(text, bufSize, "%lwww", buf);
		sprintf(buf, "%d", ti->tm_wday);
		str_replace(text, bufSize, "%lww", buf);
		sprintf(buf, "%02d", ti->tm_hour);
		str_replace(text, bufSize, "%lwhh", buf);
		sprintf(buf, "%d", ti->tm_hour);
		str_replace(text, bufSize, "%lwh", buf);
		sprintf(buf, "%02d", ti->tm_min);
		str_replace(text, bufSize, "%lwnn", buf);
		sprintf(buf, "%d", ti->tm_min);
		str_replace(text, bufSize, "%lwn", buf);
		sprintf(buf, "%02d", ti->tm_sec);
		str_replace(text, bufSize, "%lwss", buf);
		sprintf(buf, "%d", ti->tm_sec);
		str_replace(text, bufSize, "%lws", buf);
	}

	api->ProcText(text, bufSize);
}

/*
int script_output(T_SOCKET * sock, char * dest, char * str) {
	GetADConfig()->api->ib_printf(_("SimpleDJ (scheduler) -> Script Ouput: %s\n"), str);
	return 1;
}
*/

static const char scheduler_desc[] = "Scheduler";

#if !defined(DEBUG)
void scheduler_up_ref(USER_PRESENCE * u) {
#else
void scheduler_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("Scheduler_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	RefUser(u->User);
	u->ref_cnt++;
}

#if !defined(DEBUG)
void scheduler_up_unref(USER_PRESENCE * u) {
#else
void scheduler_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("Scheduler_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	UnrefUser(u->User);
	u->ref_cnt--;
	#if defined(DEBUG)
	api->ib_printf("Scheduler_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
	#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u);
	}
}

bool send_scheduler_pm(USER_PRESENCE * ut, const char * str) {
	api->ib_printf("SimpleDJ Script: %s\n", str);
	return true;
}

USER_PRESENCE * alloc_scheduler_presence(USER * user) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = user;
	ret->Sock = NULL;
	ret->Nick = zstrdup(ret->User->Nick);
	ret->Hostmask = zstrdup(ret->User->Hostmasks[0]);
	ret->NetNo = -1;
	ret->Flags = ret->User->Flags;

	ret->send_chan_notice = send_scheduler_pm;
	ret->send_chan = send_scheduler_pm;
	ret->send_msg = send_scheduler_pm;
	ret->send_notice = send_scheduler_pm;
	ret->std_reply = send_scheduler_pm;
	ret->Desc = scheduler_desc;

	ret->ref = scheduler_up_ref;
	ret->unref = scheduler_up_unref;

	RefUser(ret);
	return ret;
};

void ExecuteScript(char * fn, time_t tme) {
	USER * user = api->user->GetUser("SimpleDJ!SimpleDJ@ShoutIRC.com");
	if (user == NULL || !api->user->uflag_have_any_of(user->Flags, UFLAG_BASIC_SOURCE|UFLAG_ADVANCED_SOURCE)) {
		api->ib_printf("SimpleDJ (scheduler) -> ALERT: To use scripts, you must create an SimpleDJ user! To do this PM the bot !adduser SimpleDJ flags random_password SimpleDJ!SimpleDJ@ShoutIRC.com\n");
		if (api->irc) { api->irc->LogToChan("ALERT: To use scripts, you must create an SimpleDJ user! To do this PM the bot !adduser SimpleDJ flags random_password SimpleDJ!SimpleDJ@ShoutIRC.com"); }
		if (user) { UnrefUser(user); }
		return;
	}

	USER_PRESENCE * pres = alloc_scheduler_presence(user);
	UnrefUser(user);

	/*
	IBM_USER ui;
	memset(&ui, 0, sizeof(ui));
	ui.nick = "SimpleDJ";
	ui.hostmask = "SimpleDJ!SimpleDJ@ShoutIRC.com";
	ui.flags = user->Flags;
	ui.netno = -1;
	*/

	char buf[4096];
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		int line=0;
		while (!feof(fp) && fgets(buf, sizeof(buf), fp) != NULL) {
			line++;
			strtrim(buf);
			if (strlen(buf) && buf[0] != '#' && buf[0] != '/' && buf[0] != ';') {
				ProcText(buf, tme, sizeof(buf));
				char * p = strchr(buf, ' ');
				if (p) {
					p[0]=0;
					p++;
				}
				if (!stricmp(buf, "wait") || !stricmp(buf, "sleep")) {
					if (p) {
						int n = atoi(p);
						api->ib_printf("SimpleDJ (scheduler) -> Sleeping for %d milliseconds...\n", n);
						safe_sleep(n, true);
					} else {
						api->ib_printf("SimpleDJ (scheduler) -> Syntax: wait/sleep #\n");
					}
				} else {
					COMMAND * com = api->commands->FindCommand(buf);
					if (com) {
						if (com->proc_type == COMMAND_PROC) {
							if (com->mask & CM_ALLOW_CONSOLE_PRIVATE) {
								api->commands->ExecuteProc(com, p, pres, CM_ALLOW_CONSOLE_PRIVATE);
							} else {
								api->ib_printf("SimpleDJ (scheduler) -> Command '%s' on %s:%d does not allow Console use!\n", buf, fn, line);
							}
						} else {
							api->ib_printf("SimpleDJ (scheduler) -> Command '%s' on %s:%d is not a PROC command!\n", buf, fn, line);
						}
					} else {
						api->ib_printf("SimpleDJ (scheduler) -> I could not find a command called '%s' on %s:%d\n", buf, fn, line);
					}
				}
			}
		}
		fclose(fp);
	} else {
		api->ib_printf("SimpleDJ (scheduler) -> Error opening script: %s\n", fn);
	}
}

THREADTYPE ScheduleThread(void * lpData) {
	TT_THREAD_START

	LoadSchedule();
	time_t tme;
	time_t lastReload=0;
	tm * ti;
	char buf[4096],buf2[4096];


	while (!sd_config.shutdown_now) {
		sMutex.Lock();
		time(&tme);
		ti = localtime(&tme);

		if (sd_config.Options.AutoReload > 0) {
			if ((tme - lastReload) >= sd_config.Options.AutoReload) {
				api->ib_printf("SimpleDJ -> Reloading content directories (due to AutoReload)...\n");
				sd_config.nofiles = sd_config.Queue->QueueContentFiles();
				lastReload = tme;
			}
		}

		unsigned long mMonth=0, mDay=0, mHour=0, mMin=ti->tm_min;

		switch(ti->tm_mon) {
			case 0:
				mMonth = MONTH_JAN;
				break;
			case 1:
				mMonth = MONTH_FEB;
				break;
			case 2:
				mMonth = MONTH_MAR;
				break;
			case 3:
				mMonth = MONTH_APR;
				break;
			case 4:
				mMonth = MONTH_MAY;
				break;
			case 5:
				mMonth = MONTH_JUN;
				break;
			case 6:
				mMonth = MONTH_JUL;
				break;
			case 7:
				mMonth = MONTH_AUG;
				break;
			case 8:
				mMonth = MONTH_SEP;
				break;
			case 9:
				mMonth = MONTH_OCT;
				break;
			case 10:
				mMonth = MONTH_NOV;
				break;
			case 11:
				mMonth = MONTH_DEC;
				break;
		}
		switch(ti->tm_wday) {
			case 0:
				mDay = DAY_SUN;
				break;
			case 1:
				mDay = DAY_MON;
				break;
			case 2:
				mDay = DAY_TUE;
				break;
			case 3:
				mDay = DAY_WED;
				break;
			case 4:
				mDay = DAY_THU;
				break;
			case 5:
				mDay = DAY_FRI;
				break;
			case 6:
				mDay = DAY_SAT;
				break;
		}
		switch(ti->tm_hour) {
			case 0:
				mHour = HOUR_00;
				break;
			case 1:
				mHour = HOUR_01;
				break;
			case 2:
				mHour = HOUR_02;
				break;
			case 3:
				mHour = HOUR_03;
				break;
			case 4:
				mHour = HOUR_04;
				break;
			case 5:
				mHour = HOUR_05;
				break;
			case 6:
				mHour = HOUR_06;
				break;
			case 7:
				mHour = HOUR_07;
				break;
			case 8:
				mHour = HOUR_08;
				break;
			case 9:
				mHour = HOUR_09;
				break;
			case 10:
				mHour = HOUR_10;
				break;
			case 11:
				mHour = HOUR_11;
				break;
			case 12:
				mHour = HOUR_12;
				break;
			case 13:
				mHour = HOUR_13;
				break;
			case 14:
				mHour = HOUR_14;
				break;
			case 15:
				mHour = HOUR_15;
				break;
			case 16:
				mHour = HOUR_16;
				break;
			case 17:
				mHour = HOUR_17;
				break;
			case 18:
				mHour = HOUR_18;
				break;
			case 19:
				mHour = HOUR_19;
				break;
			case 20:
				mHour = HOUR_20;
				break;
			case 21:
				mHour = HOUR_21;
				break;
			case 22:
				mHour = HOUR_22;
				break;
			case 23:
				mHour = HOUR_23;
				break;
		};

		SCHEDULE * Scan = fSched;
		while (Scan) {
			if (Scan->type == SCHEDULE_TYPE_TIMER) {
				//printf("Timer: %u, %u, %u, %u, %u, %u, %s\n", Scan->timer.type, Scan->timer.months, Scan->timer.days, Scan->timer.hours, Scan->timer.minutes,  Scan->timer.pat_type, Scan->timer.pattern);
				if (mMonth & Scan->timer.months) {
					if (mDay & Scan->timer.days) {
						if (mHour & Scan->timer.hours) {
							if (mMin == Scan->timer.minutes) {
								sprintf(buf, "Time: %X / %X", (uint32)mHour, (uint32)Scan->timer.hours);
								if (api->irc) { api->irc->LogToChan(buf); }

								switch(Scan->timer.type) {
									case TIMER_TYPE_REQ:
									case TIMER_TYPE_OVERRIDE:
										if (Scan->timer.pat_type == TIMER_PAT_TYPE_FILENAME) {
											if ((tme - lastReload) > 300) {
												api->ib_printf("SimpleDJ -> Reloading content directories...\n");
												sd_config.nofiles = sd_config.Queue->QueueContentFiles();
												lastReload = tme;
											}
											sstrcpy(buf, Scan->timer.pattern);
											ProcText(buf, tme, sizeof(buf));
											sprintf(buf2, "SimpleDJ (scheduler) -> Pattern: %s -> %s", Scan->timer.pattern, buf);
											api->ib_printf("%s\n", buf2);
											if (api->irc) { api->irc->LogToChan(buf2); }

											QueueMutex->Lock();
											QUEUE_FIND_RESULT * ret = sd_config.Queue->FindWild(buf);
											QueueMutex->Release();
											char used[MAX_RESULTS];
											memset(&used, 0, sizeof(used));
											if (ret->num) {
												int max = Scan->timer.extra;
												if (max > MAX_RESULTS) {
													max = MAX_RESULTS;
												}
												if (!max) { max = ret->num; }
												for (int i=0; i < max && i < ret->num; i++) {
													int ind = SimpleDJ_Rand()%ret->num;
													while (used[ind]) {
														ind = SimpleDJ_Rand()%ret->num;
													}
													used[ind] = 1;
													sprintf(buf, "SimpleDJ (scheduler) -> Queued '%s'", ret->results[ind]->fn);
													api->ib_printf("%s\n", buf);
													if (api->irc) { api->irc->LogToChan(buf); }
													AddRequest(CopyQueue(ret->results[ind]));
												}
												if (Scan->timer.type == TIMER_TYPE_OVERRIDE) {
													sd_config.next = true;
												}
											} else {
												sprintf(buf, "SimpleDJ (scheduler) -> Could not find '%s'\n", Scan->timer.pattern);
												api->ib_printf("%s\n", buf);
												if (api->irc) { api->irc->LogToChan(buf); }
											}
											sd_config.Queue->FreeFindResult(ret);
										}

										if (Scan->timer.pat_type == TIMER_PAT_TYPE_GENRE || Scan->timer.pat_type == TIMER_PAT_TYPE_ARTIST || Scan->timer.pat_type == TIMER_PAT_TYPE_ALBUM) {
											if ((tme - lastReload) > 300) {
												api->ib_printf("SimpleDJ -> Reloading content directories...\n");
												sd_config.nofiles = sd_config.Queue->QueueContentFiles();
												lastReload = tme;
											}
											sstrcpy(buf, Scan->timer.pattern);
											ProcText(buf, tme, sizeof(buf));
											sprintf(buf2, "SimpleDJ (scheduler) -> Pattern: %s -> %s", Scan->timer.pattern, buf);
											api->ib_printf("%s\n", buf2);
											if (api->irc) { api->irc->LogToChan(buf2); }

											META_TYPES type = META_GENRE;
											switch(Scan->timer.pat_type) {
												case TIMER_PAT_TYPE_ARTIST:
													type = META_ARTIST;
													break;
												case TIMER_PAT_TYPE_ALBUM:
													type = META_ALBUM;
													break;
												case TIMER_PAT_TYPE_GENRE:
													type = META_GENRE;
													break;
												case TIMER_PAT_TYPE_DIRECTORY:
													type = META_DIRECTORY;
													break;
												default:
													type = META_TITLE;
													break;
											}
											QueueMutex->Lock();
											QUEUE_FIND_RESULT * ret = sd_config.Queue->FindByMeta(buf, type);
											QueueMutex->Release();
											char used[MAX_RESULTS];
											memset(&used, 0, sizeof(used));
											if (ret->num) {
												int max = Scan->timer.extra;
												if (max > MAX_RESULTS) {
													max = MAX_RESULTS;
												}
												if (!max) { max = ret->num; }
												for (int i=0; i < max && i < ret->num; i++) {
													int ind = SimpleDJ_Rand()%ret->num;
													while (used[ind]) {
														ind = SimpleDJ_Rand()%ret->num;
													}
													used[ind] = 1;
													sprintf(buf, "SimpleDJ (scheduler) -> Queued '%s'", ret->results[ind]->fn);
													api->ib_printf("%s\n", buf);
													if (api->irc) { api->irc->LogToChan(buf); }
													AddRequest(CopyQueue(ret->results[ind]));
												}
												if (Scan->timer.type == TIMER_TYPE_OVERRIDE) {
													sd_config.next = true;
												}
											} else {
												sprintf(buf, "SimpleDJ (scheduler) -> Could not find '%s'", Scan->timer.pattern);
												api->ib_printf("%s\n", buf);
												if (api->irc) { api->irc->LogToChan(buf); }
											}
											sd_config.Queue->FreeFindResult(ret);
										}
										break;

									case TIMER_TYPE_FILTER:
										QueueMutex->Lock();
										if (sd_config.Filter) {
											zfree(sd_config.Filter);
										}

										sd_config.Filter = (TIMER *)zmalloc(sizeof(TIMER));
										memcpy(sd_config.Filter, &Scan->timer, sizeof(TIMER));
										sd_config.Filter->extra += time(NULL);
										QueueMutex->Release();

										sprintf(buf, "SimpleDJ (scheduler) -> New filter active '%s'", Scan->timer.pattern);
										api->ib_printf("%s\n", buf);
										if (api->irc) { api->irc->LogToChan(buf); }

										break;

									case TIMER_TYPE_SCRIPT:
										sprintf(buf, "SimpleDJ (scheduler) -> Executing script '%s'", Scan->timer.pattern);
										api->ib_printf("%s\n", buf);
										if (api->irc) { api->irc->LogToChan(buf); }
										char * fn = zstrdup(Scan->timer.pattern);
										sMutex.Release();
										ExecuteScript(fn, tme);
										sMutex.Lock();
										zfree(fn);
										Scan = NULL;
										break;
								}

							}
						}
					}
				}
			}
			if (Scan) {
				Scan = Scan->Next;
			}
		}


		//printf ("Current date and time are: %s", asctime (timeinfo) );
		sMutex.Release();
		tme = tme+60;
		for (int i=0; time(NULL) < tme && !sd_config.shutdown_now; i++) {
			api->safe_sleep_milli(500);
		}
	}

	FreeSchedule();
	api->ib_printf("SimpleDJ -> Scheduler exiting...\n");
	TT_THREAD_END
};

