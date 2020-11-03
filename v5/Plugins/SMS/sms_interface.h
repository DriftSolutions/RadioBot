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


//#define SMS_GET_INTERFACE		0x46 ///< data is a pointer to a SMS_INTERFACE structure to be filled in by the SMS plugin. Returns 1 for success
//#define SMS_ON_RECEIVED			0x47 ///< data is a pointer to a SMS_MESSAGE structure containing the message.

struct SMS_MESSAGE {
	USER_PRESENCE * pres;
	const char * phone;
	const char * text;
};

struct SMS_INTERFACE {
	void (*send_sms)(const char * phone, const char * text);
};
