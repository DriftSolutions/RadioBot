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
Based on the Skype API Plugin for Pidgin/libpurple/Adium by Eion Robb <http://myjobspace.co.nz/images/pidgin/>

This file is part of the libSkypeAPI library, for licensing see the LICENSE file that came with this package.

***/

#if !defined(NO_DBUS)

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>

#define SKYPE_SERVICE   "com.Skype.API"
#define SKYPE_INTERFACE "com.Skype.API"
#define SKYPE_PATH      "/com/Skype"

bool skype_dbus_connect();
bool skype_dbus_send_command(char * str);
void skype_dbus_disconnect();

#else
//this is so we don't break the SKYPE_HANDLE's binary compatibility or have to use 2 definitions for it
typedef char DBusConnection;
#endif
