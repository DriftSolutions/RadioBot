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


#define SHOUTIRC_BRIDGE_PORT 7343
#define SHOUTIRC_MAX_PACKET 4096

#pragma pack(1)
typedef struct {
	unsigned char cmd;
	unsigned int len;
} BRIDGE_PACKET;
#pragma pack()

#define IMB_WELCOME		0
#define IMB_FULL		1
#define IMB_QUIT		2
#define IMB_AUTH		3
#define IMB_AUTH_OK		4
#define IMB_AUTH_FAILED	5

#define IMB_IM_RECV		16
// ^- data: 64-bit unsigned int uniquely identifying the conversation|protocol name\0|nick\0|message\0
#define IMB_IM_SEND		17
// ^- data: 64-bit unsigned int uniquely identifying the conversation|message\0

#define IMB_CHAT_RECV	18
// ^- data: 64-bit unsigned int uniquely identifying the conversation|protocol name\0|nick\0|channel\0|message\0
#define IMB_CHAT_SEND	19
// ^- data: 64-bit unsigned int uniquely identifying the conversation|message\0

