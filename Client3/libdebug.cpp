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

#ifndef DSL_STATIC
#define DSL_STATIC
#endif
#include <drift/dsl.h>
#include "libdebug.h"

Titus_Sockets * socks = NULL;
T_SOCKET * sock = NULL;

LDC_API bool ldc_Init() {
	socks = new Titus_Sockets();
	sock = socks->Create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socks->SetBroadcast(sock, true);
	return true;
}

LDC_API void ldc_SendString(char * str) {
	socks->SendTo(sock, "255.255.255.255", 3434, str);
}

LDC_API void ldc_Quit() {
	socks->Close(sock);
	delete socks;
}
