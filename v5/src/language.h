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
/*          Generated on Mar 2, 2009         */
/*********************************************/

#ifndef __INCLUDE_IRCBOT_LANGUAGE_H__
#define __INCLUDE_IRCBOT_LANGUAGE_H__

void ResetLang();
const char * GetLangString(const char * str);
bool LoadLangText(const char * fn);
void LoadLang(const char * fn);
#define _(x) GetLangString(x)

#endif // __INCLUDE_IRCBOT_LANGUAGE_H__
