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

#if !defined(DSL_STATIC)
#define DSL_STATIC
#endif
#if !defined(MEMLEAK)
#define MEMLEAK
#endif
#if !defined(ENABLE_ZLIB)
#define ENABLE_ZLIB
#endif
#include <drift/dsl.h>
#include "../tinyxml/tinyxml.h"
#include <sqlite3.h>

#include <tag.h>
#include <fileref.h>
#if defined(DEBUG)
#pragma comment(lib, "taglib_d.lib")
#else
#pragma comment(lib, "taglib.lib")
#endif

#define VERSION "2.1"
#define NUM_FOLDERS 3

typedef struct {
	char out_fn[MAX_PATH];
	char config_fn[MAX_PATH];
	char music_folder[NUM_FOLDERS][MAX_PATH];
	char db_fn[MAX_PATH];
	char error[256];

	sqlite3 * dbHandle;
	bool compressOutput;
	bool useJSON;
} CONFIG;

struct FILEREC {
	time_t mTime;
	int songlen;

	char path[MAX_PATH];
	char fn[256];

	char artist[256];
	char album[256];
	char title[256];
	char genre[256];
};

extern CONFIG config;
extern Universal_Config cfg;
typedef int (*PrintFunc)(char * fmt, ...);
extern PrintFunc debug_printf;
extern PrintFunc status_printf;

bool InitCore(PrintFunc dbg, PrintFunc stat, char * error);
void DoScan();
void WipeDB();
void QuitCore();
int Query(const char * query, sqlite3_callback cb=NULL, void * parm=NULL, char ** errmsg=NULL);
int32 ScanFiles(int32 * num_files, int32 * totlen);

