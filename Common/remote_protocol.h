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

#ifndef __INCLUDE_IRCBOT_REMOTE_PROTOCOL_H__
#define __INCLUDE_IRCBOT_REMOTE_PROTOCOL_H__

/** \defgroup remote Remote Connection/Protocol API
 * The Remote Protocol is much better documented on the wiki, so you may want to check it out for more information.
 * @sa http://wiki.shoutirc.com/index.php/Remote_Commands
 * @{
 */

#define MAX_REMOTE_PACKET_SIZE 4096

typedef struct {
	char title[64]; //stream title
	char dj[64];
	int32 listeners;
	int32 peak;
	int32 max;
} STREAM_INFO;

#pragma pack(1)
typedef struct {
	uint8 xferid;
	uint32 offset;
} REMOTE_UPLOAD_DATA;

typedef struct {
	uint32 size; // 4GB - 1 byte file size limit, you really shouldn't be sending files that big through RadioBot anyway
	uint8 hash[40];
	char filename[1];
} REMOTE_UPLOAD_START;
#pragma pack()

#ifdef __DSL_H__
COMPILE_TIME_ASSERT(sizeof(REMOTE_UPLOAD_START) == 45)
COMPILE_TIME_ASSERT(sizeof(REMOTE_UPLOAD_DATA) == 5)
COMPILE_TIME_ASSERT(sizeof(STREAM_INFO) == 140)
#endif

enum REMOTE_COMMANDS_C2S {
	// Client to Server Commands
	// - Login/Control Commands
	RCMD_LOGIN = 0x00,
	RCMD_QUERY_STREAM = 0x02,
	RCMD_ENABLE_SSL = 0x03,
	RCMD_GET_VERSION = 0x04,

	// - Request System Commands
	RCMD_REQ_LOGOUT = 0x10,
	RCMD_REQ_LOGIN = 0x11,
	RCMD_REQ_CURRENT = 0x12,
	RCMD_SEND_REQ = 0x13,
	RCMD_REQ = 0x14,
	RCMD_SEND_DED = 0x15,
	RCMD_FIND_RESULTS = 0x16,

	// - Misc Commands
	RCMD_DOSPAM = 0x20,
	RCMD_DIE = 0x21,
	RCMD_BROADCAST_MSG = 0x22,
	RCMD_RESTART = 0x23,
	RCMD_RCONS_OPEN = 0x24,
	RCMD_RCONS_CLOSE = 0x25,
	RCMD_REHASH = 0x26,
	RCMD_UPLOAD_START = 0x27,
	RCMD_UPLOAD_DATA = 0x28,
	RCMD_UPLOAD_DONE = 0x29,

	// - Source Control Commands
	RCMD_SRC_COUNTDOWN = 0x30,
	RCMD_SRC_FORCE_OFF = 0x31,
	RCMD_SRC_FORCE_ON = 0x32,
	RCMD_SRC_NEXT = 0x33,
	RCMD_SRC_RELOAD = 0x34,
	RCMD_SRC_GET_SONG = 0x35,
	RCMD_SRC_RATE_SONG = 0x36,
	RCMD_SRC_STATUS = 0x37,
	RCMD_SRC_GET_NAME = 0x38,
	RCMD_SRC_RELAY = 0x39,
	RCMD_SRC_GET_SONG_INFO = 0x3A,

	// - User Control Commands
	RCMD_GETUSERINFO = 0x40,
};

enum REMOTE_COMMANDS_S2C {
	// Server to Client Commands
	RCMD_LOGIN_FAILED	= 0x00,
	RCMD_LOGIN_OK		= 0x01,
	RCMD_ENABLE_SSL_ACK	= 0x03,
	RCMD_IRCBOT_VERSION = 0x04,

	RCMD_REQ_LOGOUT_ACK	= 0x10,
	RCMD_REQ_LOGIN_ACK	= 0x11,
	RCMD_REQ_INCOMING	= 0x12,
	RCMD_STREAM_INFO	= 0x13,
	RCMD_FIND_QUERY		= 0x14,

	RCMD_RCONS_OPEN_ACK	= 0x20,
	RCMD_RCONS_LINE		= 0x21,
	RCMD_RCONS_CLOSE_ACK= 0x22,
	RCMD_UPLOAD_FAILED	= 0x23,
	RCMD_UPLOAD_OK		= 0x24,
	RCMD_UPLOAD_DATA_ACK= 0x25,
	RCMD_UPLOAD_DONE_ACK= 0x26,

	RCMD_SONG_INFO      = 0x30,

	RCMD_USERINFO		= 0x40,
	RCMD_USERNOTFOUND	= 0x41,
	//RCMD_USERINFO_V5	= 0x42,

	RCMD_GENERIC_MSG	= 0xFE,
	RCMD_GENERIC_ERROR	= 0xFF
};

/**@}*/

#endif // __INCLUDE_IRCBOT_REMOTE_PROTOCOL_H__
