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


struct TITUS_VERSION {
	int major;
	int minor;
	int revision;
	int extra;
	char desc[256];
};

#define titus_myver(a,b,c) ( (a > TITUS_MAJOR) || (a == TITUS_MAJOR && b > TITUS_MINOR) || (a == TITUS_MAJOR && b == TITUS_MINOR && c > TITUS_REVISION) )
#define titus_cmpver(d,e,f,a,b,c) ( (a > d) || (a == d && b > e) || (a == d && b == e && c > f) )

extern "C" __declspec(dllexport) TITUS_VERSION TitusGetVersion = {
	TITUS_MAJOR,TITUS_MINOR,TITUS_REVISION,TITUS_EXTRA,TITUS_DESC
};
