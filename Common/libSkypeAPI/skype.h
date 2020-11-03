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

/*** libSkypeAPI: Skype API Library ***

Written by Indy Sams (indy@driftsolutions.com)

This file is part of the libSkypeAPI library, for licensing see the LICENSE file that came with this package.

***/

#ifndef __LIBSKYPE_H__
#define __LIBSKYPE_H__

#if defined(WIN32)
#include "skype_win32.h"

#if defined(LIBSKYPEAPI_DLL)
	#if defined(LIBSKYPEAPI_EXPORTS)
		#define SKYPEAPI_API __declspec(dllexport)
	#else
		#define SKYPEAPI_API __declspec(dllimport)
	#endif
#else
	#define SKYPEAPI_API
#endif

#elif defined(__linux__) || defined(__LINUX__) || defined(FREEBSD) || defined(__FreeBSD__) || defined(__APPLE__)
#include "skype_x11.h"
#include "skype_dbus.h"

#ifndef UNIX
#define UNIX
#endif

#define SKYPEAPI_API

#define SKYPE_HANDLE_PLATFORM	DBusConnection * dConx; \
								Display * disp; \
								Window win; \
								Window skype_win; \
								Atom message_start, message_continue; \
								unsigned char x11_error_code; \
								bool loop_running, pending;

#else
#error Unknown/unsupported platform!
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum SKYPE_API_STATUS {
	SAPI_STATUS_ERROR,
	SAPI_STATUS_DISCONNECTED,
	SAPI_STATUS_CONNECTING,
	SAPI_STATUS_CONNECTED,

	SAPI_STATUS_PENDING_AUTHORIZATION,
	SAPI_STATUS_REFUSED,
	SAPI_STATUS_NOT_AVAILABLE,
	SAPI_STATUS_AVAILABLE
};

enum SKYPE_PROTOCOL_VERSIONS {
	SKYPE_PROTOCOL_DEFAULT, // just like using SKYPE_PROTOCOL_1
	SKYPE_PROTOCOL_1,
	SKYPE_PROTOCOL_2,
	SKYPE_PROTOCOL_3,
	SKYPE_PROTOCOL_4,
	SKYPE_PROTOCOL_5,
	SKYPE_PROTOCOL_6,
	SKYPE_PROTOCOL_7,
	SKYPE_PROTOCOL_8
};

struct SKYPE_API_CLIENT {
#if defined(WIN32)
	HINSTANCE hInstance;
#else
	char * app_name;
#endif
	int protocol_version; // you can use the number, or one of SKYPE_PROTOCOL_* constants, whichever you like
	bool (*status_cb)(SKYPE_API_STATUS status);
	bool (*message_cb)(char * msg);
};

struct SKYPE_API_PROVIDER {
	bool (*connect)();
	bool (*send_command)(char * str);
	void (*disconnect)();
};

struct SKYPE_HANDLE {
	SKYPE_API_CLIENT * client;
	SKYPE_API_PROVIDER * provider;
	bool quit;
	SKYPE_HANDLE_PLATFORM
};

extern SKYPE_HANDLE h;

#ifdef __cplusplus
extern "C" {
#endif

bool SKYPEAPI_API SkypeAPI_Connect(SKYPE_API_CLIENT * client);
bool SKYPEAPI_API SkypeAPI_SendCommand(char * str);
void SKYPEAPI_API SkypeAPI_Disconnect();

#ifdef __cplusplus
}
#endif

#endif // __LIBSKYPE_H__
