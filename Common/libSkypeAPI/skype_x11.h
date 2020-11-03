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

#if !defined(NO_X11)

#include <unistd.h>
#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

bool skype_x11_connect();
bool skype_x11_send_command(char * str);
void skype_x11_disconnect();

#endif
