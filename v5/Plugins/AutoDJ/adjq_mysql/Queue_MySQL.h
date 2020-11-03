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

//#include <drift/dsl.h>
#include "../adj_plugin.h"
#undef zmalloc
#undef zstrdup
#undef zrealloc
#undef znew
#undef zfreenn
#undef zfree
#include <assert.h>
//#include "meta_cache.h"

enum QM_FIND_SORT {
	QMFS_NONE,
	QMFS_RANDOM,
	QMFS_FILENAME,
	QMFS_TITLE
};

struct QM_CONFIG {
	char mysql_host[128];
	char mysql_user[128];
	char mysql_pass[128];
	char mysql_db[128];
	char mysql_table[128];
	uint16 mysql_port;
	int32 nofiles;
	int32 keep_history;

	QM_FIND_SORT find_sort;
};

extern DB_MySQL conx;
MYSQL_RES * LoadMetaCacheQuery();
QUEUE * GetRowToQueue(MYSQL_RES * res);

void FreeMetaCache();
void LoadMetaCache();
void SaveMetaCache();
QUEUE * FindFileMeta(unsigned long FileID);

//void ScanDirectory(const char * path, int32 * num_files);
