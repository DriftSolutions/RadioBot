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

/*********************************************/
/* Generated by Drift Solutions' make_h 1.00 */
/*      Copyright 2004+ Drift Solutions      */
/*   Generated on Thu Jul 29 03:37:42 2004   */
/*********************************************/

#ifndef __INCLUDE_IRCBOT_TEXTPROC_H__
#define __INCLUDE_IRCBOT_TEXTPROC_H__

bool is_numeric(char ch);
bool is_request_number(const char * p);
//char * strtrim(char *buf, char * trim);
//char * strtrim2(char *buf, char * trim);
char * duration(uint32 num);
int32 decode_duration(const char * buf);
char * duration2(uint32 num, char * buf);
string number_format(uint64 n, char sep = ',');
bool get_range(const char * buf, int32 * imin, int32 * imax);
//int get_rand(int min, int max);
char * GetCurrentDJDisp();

void utprintf(USER_PRESENCE * up, USER_PRESENCE::userPresSendFunc sendfunc, const char * fmt, ...);

bool IsMessage(const char * name);
//int GetMessageOffset(char * name);
bool LoadMessage(const char * name, char * msgbuf, int32 bufSize);
bool LoadMessageChannel(const char * msg, int netno, const char * chan, char * msgbuf, int32 bufSize);
void ProcText(char * text, int32 bufsize);

bool mask(const char * hostmask, char * buf, size_t bufSize, int type);
#if defined(DEBUG)
void mask_test();
#endif


#endif // __INCLUDE_IRCBOT_TEXTPROC_H__