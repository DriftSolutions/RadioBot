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

#include "skype.h"

SKYPE_HANDLE h;

SKYPE_API_PROVIDER providers[] = {
#if defined(WIN32)
	{ skype_win32_connect, skype_win32_send_command, skype_win32_disconnect },
#else
	{ skype_x11_connect, skype_x11_send_command, skype_x11_disconnect },
#if !defined(NO_DBUS)
	{ skype_dbus_connect, skype_dbus_send_command, skype_dbus_disconnect },
#endif
#endif
	{ NULL, NULL, NULL }
};

bool SkypeAPI_Connect(SKYPE_API_CLIENT * client) {
	if (!client || !client->message_cb || !client->status_cb) { return false; }

	memset(&h, 0, sizeof(h));
	h.client = client;

	if (client->protocol_version <= 0) { client->protocol_version = 1; }

	for (int i=0; providers[i].connect != NULL; i++) {
		if (providers[i].connect()) {
			h.provider = &providers[i];
			return true;
		}
	}

	return false;
}

bool SkypeAPI_SendCommand(char * str) {
	if (h.provider == NULL) { return false; }
	return h.provider->send_command(str);
}

void SkypeAPI_Disconnect() {
	if (h.provider != NULL) {
		h.quit = true;
		h.provider->disconnect();
	}
}
