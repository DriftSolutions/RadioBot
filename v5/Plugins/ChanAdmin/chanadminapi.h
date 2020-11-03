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

/**
 * \defgroup chanadmin ChanAdmin API
 * @{
*/

struct CHANADMIN_INTERFACE {
	/**
	 * @param netno The IRC network number to set the ban on. For global bans use the netno of the network it was ordered from or 0
	 * @param chan The channel to add the ban in or "global" for a global ban
	 * @param hostmask The hostmask to ban
	 * @param set_by Who ordered the ban
	 * @param time_expire A timestamp for the ban to expire at (time()-style) or 0 to never expire.
	 * @param reason You can use reason=NULL for no reason
	*/
	bool (*add_ban)(int netno, const char * chan, const char * hostmask, const char * set_by, int64 time_expire, const char * reason);
	bool (*del_ban)(int netno, const char * chan, const char * hostmask);

	bool (*do_kick)(int netno, const char * chan, const char * nick, const char * reason);
	bool (*do_ban)(int netno, const char * chan, const char * hostmask);
	bool (*do_unban)(int netno, const char * chan, const char * hostmask);
};

/**@}*/
