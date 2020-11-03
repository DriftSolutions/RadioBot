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

#if defined(__EDITUSERS__)
#include "../../editusers/editusers.h"
#else
#include "ircbot.h"
#endif

Titus_Mutex dbMutex;

void Free(char * ptr) {
	dbMutex.Lock();
	sqlite3_free(ptr);
	dbMutex.Release();
}

void FreeTable(char **result) {
	dbMutex.Lock();
	sqlite3_free_table(result);
	dbMutex.Release();
}

char * MPrintf(const char* fmt,...) {
	char * ret = NULL;
	dbMutex.Lock();
	va_list ap;
	va_start(ap, fmt);
	ret = sqlite3_vmprintf(fmt, ap);
	va_end(ap);
	dbMutex.Release();
	return ret;
}

void LockDB() {
	dbMutex.Lock();
}

void ReleaseDB() {
	dbMutex.Release();
}

int64 InsertID() {
	AutoMutex(dbMutex);
	return sqlite3_last_insert_rowid(config.hSQL);
}


int Query(const char * query, sqlite3_callback cb, void * parm, char ** errmsg) {
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	dbMutex.Lock();
	while ((ret = sqlite3_exec(config.hSQL, query, cb, parm, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	dbMutex.Release();
	if (ret != SQLITE_OK) {
		printf(_("Query: %s -> %d (%s)\n"), query, ret, *errmsg2);
	}
	if (errmsg == NULL && *errmsg2) {
		Free(*errmsg2);
	}
	return ret;
}

int GetTable(const char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	int ret = 0, tries = 0;
	char * errmsg2 = NULL;
	dbMutex.Lock();
	while ((ret = sqlite3_get_table(config.hSQL, query, resultp, nrow, ncolumn, &errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	dbMutex.Release();
	if (ret != SQLITE_OK) {
		ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, errmsg2);
	}
	if (errmsg == NULL && errmsg2) {
		Free(errmsg2);
	} else if (errmsg != NULL) {
		*errmsg = errmsg2;
	}
	return ret;
}

const char * GetTableEntry(int row, int col, char **resultp, int nrow, int ncol) {
	if ((col*row) > (nrow*ncol)) {
		ib_printf(_("GetTableEntry: index out of bounds (%d, %d, %d, %d)\n"), row, col, nrow, ncol);
		return NULL;
	}
	return resultp[(row*ncol)+col];
}

sql_rows GetTable(const char * query, char **errmsg) {
//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char * errmsg2 = NULL;
	dbMutex.Lock();
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = sqlite3_get_table(config.hSQL, query, &resultp, &nrow, &ncol, &errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	dbMutex.Release();
	if (ret != SQLITE_OK) {
		ib_printf(_("GetTableSTL: %s -> %d (%s)\n"), query, ret, errmsg2);
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
		dbMutex.Lock();
		FreeTable(resultp);
		dbMutex.Release();
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && errmsg2) {
		Free(errmsg2);
	} else if (errmsg != NULL) {
		*errmsg = errmsg2;
	}
	return rows;
}
