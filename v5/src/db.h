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


int Query(const char * query, sqlite3_callback cb=NULL, void * parm=NULL, char ** errmsg=NULL);
char * MPrintf(const char* fmt,...);
void Free(char * ptr);//use on any errmsg parms & MPrintf

void LockDB();
void ReleaseDB();
int64 InsertID();

int GetTable(const char * query, char *** resultp, int *nrow, int *ncolumn, char **errmsg=NULL);
const char * GetTableEntry(int row, int col, char **resultp, int nrow, int ncol);
void FreeTable(char ** result);

sql_rows GetTable(const char * query, char **errmsg=NULL);
