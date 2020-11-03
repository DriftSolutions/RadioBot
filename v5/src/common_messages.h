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


#ifndef __IRCBOT_COMMON_MESSAGES_H__
#define __IRCBOT_COMMON_MESSAGES_H__
/** \defgroup ipc IPC Messaging API
	0x00 - 0x0F:	System Messages<br>
	0x10 - 0x1F:	IRC<br>
	0x20 - 0x2F:	User Info/Control<br>
	0x30 - 0x3F:	Source Control (AutoDJ, mp3_source, etc.)<br>
	0x40 - 0x41:	TTS Services Interface<br>
	0x42 - 0x43:	ChanAdmin Interface<br>
	0x44 - 0x45:	Email Interface<br>
	0x46 - 0x4F:	Reserved for official RadioBot implementation<br>
	0x50 - 0x5F:	AutoDJ-specific messages<br>
	0x60 - 0xFF:	Reserved for official RadioBot implementation<br><br>

	0x0100 - 0x7FFFFFFF: Contact me and I will add it here
 * @{
*/

#define IB_INIT					0x01 ///< sent after RadioBot is done reading the config, loading plugins, and is about to connect
									 ///< to IRC and begin scraping sound servers
									 ///< Important note: unlike v3/v4 IB_INIT is only sent when the bot is starting up for real and not from !modload or other plugin loads.
#define IB_SHUTDOWN				0x02 ///< hmm... I wonder what this one is
									 ///< Important note: unlike v3/v4 IB_SHUTDOWN is only sent when the bot is shutting down for real and not from !modunload or other plugin unloads.
#define IB_SS_DRAGCOMPLETE		0x03 ///< sent each time at the end of a stats drag. Data sent is a bool which is set to true if the title has changed since the last drag
#define IB_REHASH				0x04 ///< RadioBot has rehashed, reload your config if you need to
#define IB_LOG					0x05 ///< A line that will be printed to the console/log window. Data is a pointer to a null-terminated string that will be printed.
									 ///< You MUST NOT call ib_printf from within this callback or there will be a thread deadlock on non-Win32 systems.
#define IB_GETMEMINFO			0x06 ///< This is sent to each plugin individually, and you should return 1 if you provided the information, or 0 if you did not. Data is a pointer to an IBM_MEMINFO structure.
#define IB_PROCTEXT				0x07 ///< This is sent to all plugins, and you should return 0 whether your processed the text or not. Data is a pointer to an IBM_PROCTEXT structure.
#define IB_ON_SONG_RATING		0x08 ///< This is sent to all plugins when a song is rated.
#define IB_REMOTE				0x09 ///< This is sent to all plugins when a remote command is received, data is a ptr to a IBM_REMOTE structure
#define IB_REQ_OFF				0x0A ///< This is sent to all plugins when the request system is turned off (!reqlogout, AutoDJ stopped, etc.). Data is a ptr to a IBM_REQONOFF structure
#define IB_REQ_ON				0x0B ///< This is sent to all plugins when the request system is turned on (!reqlogin, AutoDJ playing, etc.). Data is a ptr to a IBM_REQONOFF structure

#define IB_ONJOIN				0x10 ///< sent when a user joins a channel, data is a ptr to a USER_PRESENCE structure
#define IB_ONPART				0x11 ///< sent when a user leaves a channel, data is a ptr to a IBM_USERTEXT structure
#define IB_ONQUIT				0x12 ///< sent when a user disconnects, data is a ptr to a IBM_USERTEXT structure (with channel set to NULL in the userinfo member)
									 ///< this message is also sent with each channel the bot and the nick had in common with the channel set
#define IB_ONTEXT				0x13 ///< sent when a user speaks. data is a ptr to a IBM_USERTEXT structure (channel is set to the ircbot's nick in case of type == IBM_UTT_PRIVMSG)
#define IB_ONNICK				0x14 ///< sent when a user changes nicks. data is a ptr to a IBM_NICKCHANGE structure
									 ///< this message is also sent with each channel the bot and the nick had in common with the channel set
#define IB_ONNOTICE				0x15 ///< sent when a user notices. data is a ptr to a IBM_USERTEXT structure (channel is set to the ircbot's nick in case of type == IBM_UTT_PRIVMSG)
#define IB_ONTOPIC				0x16 ///< sent when a channel topic changes. data is a ptr to a IBM_USERTEXT structure
#define IB_ONKICK				0x17 ///< sent when a person is kicked from a channel. data is a ptr to a IBM_ON_KICK structure
#define IB_ONMODE				0x18 ///< sent when a mode changes. data is a ptr to a IBM_USERTEXT structure. channel is set to the target username's nick in case of type == IBM_UTT_PRIVMSG, text is the raw mode line
#define IB_ONBAN				0x19 ///< sent when a ban is added in a channel. data is a ptr to a IBM_ON_BAN structure.
#define IB_ONUNBAN				0x1A ///< sent when a ban is removed from a channel. data is a ptr to a IBM_ON_BAN structure.
#define IB_BROADCAST_MSG		0x1B ///< sent when a user sends a broadcast message to IRC (!bcast/!dedicate/etc.). data is a ptr to a IBM_USERTEXT structure. userpres may == NULL if a message was generated by a plugin

#define IB_GETUSERINFO			0x20 ///< sent when a user's level/info is needed, sent as a IBM_GETUSERINFO structure (the nick/hostmask for you to find will be in the IBM_USERFULL/ui member, remember the hostmask may be an empty string in case of GetUserByNick())
#define IB_ADD_USER				0x21 ///< sent when a user needs to be added, sent as a IBM_USERFULL structure
									 ///< return values are: 0 = I don't support this, 1 = Addition failed, 2 = Addition success
// For all the 0x2X messages beyond this point you should always return 0 in case other plugins need the notification as well
#define IB_DEL_USER				0x22 ///< sent when a user needs to be deleted, sent as a IBM_USERFULL structure
#define IB_ADD_HOSTMASK			0x23 ///< sent when a user adds a hostmask, sent as a IBM_USERFULL structure with hostmask as the new hostmask to be added
#define IB_DEL_HOSTMASK			0x24 ///< sent when a user deletes a hostmask, sent as a IBM_USERFULL structure with hostmask as the hostmask to be deleted
#define IB_DEL_HOSTMASKS		0x25 ///< sent when a user deletes all of their hostmasks, sent as a IBM_USERFULL structure with hostmask empty
#define IB_CHANGE_FLAGS			0x26 ///< sent when a user's flags change, sent as a IBM_USERFULL structure with flags as the new flags and hostmask empty
#define IB_CHANGE_PASS			0x27 ///< sent when a user's password changes, sent as a IBM_USERFULL structure with pass as the new pass and hostmask empty

// _C_ == Command message source control should respond to
// _I_ == Information message source control should send out
#define SOURCE_C_STOP			0x30 ///< commands any source control plugins to stop feeding the stream
#define SOURCE_C_PLAY			0x31 ///< commands any source control plugins to start feeding the stream
#define SOURCE_C_NEXT			0x32 ///< commands any source control plugins to skip the current song
#define SOURCE_I_STOPPING		0x33 ///< broadcast sent by a source plugin when it's stopping playback
#define SOURCE_I_STOPPED		0x34 ///< broadcast sent by a source plugin when playback stops
#define SOURCE_I_PLAYING		0x35 ///< broadcast sent by a source plugin when playback starts
#define SOURCE_IS_PLAYING		0x36 ///< if a source plugin is currently connected and playing returns 1
#define SOURCE_GET_SONGINFO		0x37 ///< gets information about the song a source plugin is currently playing, sent as a IBM_SONGINFO structure and returns 1 on success
#define SOURCE_GET_SONGID		0x38 ///< gets the ID number of the song a source plugin is currently playing, sent as a uint32 and returns 1 on success
#define SOURCE_GET_FILE_PATH	0x39 ///< gets the full path and filename of a specified file. data is a buffer with the filename to get info on and where the result will be stored with datalen = the size of the buffer. Returns 1 on success
#define SOURCE_QUEUE_FILE		0x3A ///< Queues a file for playback, sent as a IBM_QUEUE_FILE structure. Returns 1 on success

#define TTS_IS_LOADED			0x40 ///< send to see if TTS is loaded, anything but 0 means TTS is loaded \ingroup tts
#define TTS_GET_INTERFACE		0x41 ///< data is a pointer to a TTS_INTERFACE structure to be filled in by the TTS plugin. Returns 1 for success \ingroup tts
#define CHANADMIN_IS_LOADED		0x42 ///< send to see if ChanAdmin is loaded, anything but 0 means ChanAdmin is loaded \ingroup chanadmin
#define CHANADMIN_GET_INTERFACE	0x43 ///< data is a pointer to a CHANADMIN_INTERFACE structure to be filled in by the ChanAdmin plugin. Returns 1 for success \ingroup chanadmin
#define EMAIL_IS_LOADED			0x44 ///< send to see if Email is loaded, anything but 0 means Email is loaded
#define EMAIL_GET_INTERFACE		0x45 ///< data is a pointer to a EMAIL_INTERFACE structure to be filled in by the Email plugin. Returns 1 for success

#define SMS_GET_INTERFACE		0x46 ///< data is a pointer to a SMS_INTERFACE structure to be filled in by the SMS plugin. Returns 1 for success
#define SMS_ON_RECEIVED			0x47 ///< data is a pointer to a SMS_MESSAGE structure containing the message.

#define AUTODJ_IS_LOADED		0x50 ///< returns 1 if AutoDJ or SimpleDJ are loaded
#define AUTODJ_GET_FILE_PATH	0x51 ///< use SOURCE_GET_FILE_PATH instead

#ifndef SWIG

struct USER_PRESENCE;
struct REMOTE_HEADER;

struct IBM_NICKCHANGE {
	const char * old_nick;
	const char * new_nick;
	int32 netno;
	const char * channel;
};

struct IBM_ON_KICK {
	USER_PRESENCE * kicker;
	const char * kickee;
	const char * reason;
	const char * kickee_hostmask;
};

struct IBM_ON_BAN {
	USER_PRESENCE * banner;
	const char * banmask;
};

enum IBM_USERTEXT_TYPE {
	IBM_UTT_CHANNEL=0,
	IBM_UTT_PRIVMSG=1
};

struct IBM_USERTEXT {
	USER_PRESENCE * userpres;
	IBM_USERTEXT_TYPE type;
	const char * text;
};

struct IBM_USERFULL {
	char nick[128];
	char pass[128];
	char hostmask[128];
	uint32 flags;
};

struct IBM_USEREXTENDED {
	char nick[128];
	char pass[128];
	int32 ulevel_old_for_v4_binary_compat;
	uint32 flags;
	int32 numhostmasks;
	char hostmasks[MAX_HOSTMASKS][128];
};

enum IBM_GETUSERINFO_TYPES {
	IBM_UT_FULL		= 0,
	IBM_UT_EXTENDED	= 1
};

struct IBM_GETUSERINFO {
	IBM_GETUSERINFO_TYPES type;
	union {
		IBM_USERFULL ui;
		IBM_USEREXTENDED ue;
	};
};

struct IBM_MEMINFO {
	int64 num_pointers;
	int64 bytes_allocated;
};

struct IBM_PROCTEXT {
	char * buf;
	int32 bufSize;
};

struct IBM_SONGINFO {
	uint32 file_id;
	char fn[1024];
	char artist[256];
	char album[256];
	char title[256];
	char genre[128];
	int32 songLen;
	bool is_request;
	char requested_by[64];

	int64 playback_position;
	int64 playback_length;
};
#ifdef __DSL_H__
COMPILE_TIME_ASSERT(sizeof(IBM_SONGINFO) == 2016)
#endif

struct IBM_QUEUE_FILE {
	const char * fn; // or URL
	const char * requesting_nick; // should either be NULL or a nickname, no empty strings ""
	bool to_front; // add to front of request queue instead of back
};

struct IBM_RATING {
	const char * song;
	uint32 rating, votes;
	uint32 AutoDJ_ID;
};

struct IBM_REMOTE {
	USER_PRESENCE * userpres;
	unsigned char cliversion;
	REMOTE_HEADER * head;
	char * data;
};

struct IBM_REQONOFF {
	USER_PRESENCE * userpres; ///< Can be NULL if it isn't a Live DJ
};

#endif
// ^- SWIG

/**@}*/

#endif // __IRCBOT_COMMON_MESSAGES_H__
