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

#include "Queue_MySQL.h"

#undef zmalloc
#undef zstrdup
#undef zrealloc
#undef znew
#undef zfreenn
#undef zfree

//META_CACHE *mFirst=NULL, *mLast=NULL;
//bool mChanged=false;

extern AD_PLUGIN_API * adapi;

typedef std::map<unsigned long, QUEUE *> metaCacheList;

metaCacheList mList;
unsigned int mcFound = 0;
Titus_Mutex mcMutex;

void FreeMetaCache() {
#if 1
	for (metaCacheList::iterator x = mList.begin(); x != mList.end(); x = mList.begin()) {
		adapi->FreeQueue(x->second);
		mList.erase(x);
	}
#else
	for (metaCacheList::iterator x = mList.begin(); x != mList.end(); x++) {
		adapi->FreeQueue(x->second);
	}
	mList.clear();
#endif
}

void LoadMetaCache() {
	FreeMetaCache();

#if defined(WIN32) && defined(DEBUG)
	uint32 ticks = GetTickCount();
#endif
	MYSQL_RES * res = LoadMetaCacheQuery();
	QUEUE * Scan = NULL;
	while ((Scan = GetRowToQueue(res)) != NULL) {
		mList[Scan->ID] = Scan;
	}
	conx.FreeResult(res);
#if defined(WIN32) && defined(DEBUG)
	ticks = GetTickCount() - ticks;
	adapi->botapi->ib_printf(_("AutoDJ -> Loaded meta DB in %u ticks\n"), ticks);
#endif
	mcFound = 0;
	conx.Query("BEGIN");
}

void CommitMetaCache() {
	conx.Query("COMMIT");
	conx.Query("BEGIN");
	mcFound = 0;
}

void SaveMetaCache() {
	//adapi->botapi->ib_printf("AutoDJ (adjq_mysql) -> Committing meta cache changes...\n");
	conx.Query("COMMIT");
	//adapi->botapi->ib_printf("AutoDJ (adjq_mysql) -> Done!\n");
}

QUEUE * FindFileMeta(unsigned long FileID) {
	mcMutex.Lock();
	mcFound++;
	if (mcFound >= 1000) {
		CommitMetaCache();
	}
	mcMutex.Release();
	metaCacheList::iterator x = mList.find(FileID);
	if (x != mList.end()) {
		return x->second;
	}
	return NULL;
}
