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


#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

//type month(s) day(s) hour(s) minute(s) second(s) pattern
//req 0 0 0

#define SCHEDULE_TYPE_TIMER 1

enum TIMER_TYPES {
	TIMER_TYPE_REQ				= 1,
	TIMER_TYPE_OVERRIDE			= 2,
	TIMER_TYPE_FILTER			= 3,
	TIMER_TYPE_SCRIPT			= 4,
	TIMER_TYPE_RELAY			= 5
};

enum TIMER_PAT_TYPES {
	TIMER_PAT_TYPE_FILENAME		= 1,
	TIMER_PAT_TYPE_DIRECTORY	= 2,
	TIMER_PAT_TYPE_GENRE		= 3,
	TIMER_PAT_TYPE_ARTIST		= 4,
	TIMER_PAT_TYPE_ALBUM		= 5,
	TIMER_PAT_TYPE_TITLE		= 6,
	TIMER_PAT_TYPE_YEAR			= 7,
	TIMER_PAT_TYPE_REQ_COUNT	= 8,
};

#define MONTH_JAN 0x00000001
#define MONTH_FEB 0x00000002
#define MONTH_MAR 0x00000004
#define MONTH_APR 0x00000008
#define MONTH_MAY 0x00000010
#define MONTH_JUN 0x00000020
#define MONTH_JUL 0x00000040
#define MONTH_AUG 0x00000080
#define MONTH_SEP 0x00000100
#define MONTH_OCT 0x00000200
#define MONTH_NOV 0x00000400
#define MONTH_DEC 0x00000800

#define DAY_SUN   0x00000001
#define DAY_MON   0x00000002
#define DAY_TUE   0x00000004
#define DAY_WED   0x00000008
#define DAY_THU   0x00000010
#define DAY_FRI   0x00000020
#define DAY_SAT   0x00000040

#define HOUR_00	  0x00000001
#define HOUR_01	  0x00000002
#define HOUR_02	  0x00000004
#define HOUR_03	  0x00000008
#define HOUR_04	  0x00000010
#define HOUR_05	  0x00000020
#define HOUR_06	  0x00000040
#define HOUR_07	  0x00000080
#define HOUR_08	  0x00000100
#define HOUR_09	  0x00000200
#define HOUR_10	  0x00000400
#define HOUR_11	  0x00000800
#define HOUR_12	  0x00001000
#define HOUR_13	  0x00002000
#define HOUR_14	  0x00004000
#define HOUR_15	  0x00008000
#define HOUR_16	  0x00010000
#define HOUR_17	  0x00020000
#define HOUR_18	  0x00040000
#define HOUR_19	  0x00080000
#define HOUR_20	  0x00100000
#define HOUR_21	  0x00200000
#define HOUR_22	  0x00400000
#define HOUR_23	  0x00800000

struct TIMER {
	TIMER_TYPES type;
	uint32 months;
	uint32 days;
	uint32 hours;
	uint32 minutes;
	int32 extra;
	TIMER_PAT_TYPES pat_type;
	char pattern[256];
};

struct SCHEDULE {
	unsigned char type;
	union {
		TIMER timer;
	};
	//SCHEDULE *Prev, *Next;
};

#endif // _SCHEDULER_H_

