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

#ifndef __INCLUDE_IRCBOT_PLUGINS_H__
#define __INCLUDE_IRCBOT_PLUGINS_H__

#define IRCBOT_PLUGIN_VERSION			0x0F
#define IRCBOT_VERSION_FLAG_DEBUG		0x00000001
#define IRCBOT_VERSION_FLAG_LITE		0x00000002
//#define IRCBOT_VERSION_FLAG_BASIC		0x00000004
//#define IRCBOT_VERSION_FLAG_FULL		0x00000008
//#define IRCBOT_VERSION_FLAG_STANDALONE	0x00000010

//#define _USE_32BIT_TIME_T
#ifndef NO_DSL
#include "../../Common/ircbot-config.h"
#include <drift/dsl.h>
#else
	// DSL types
	#if defined(WIN32) || defined(XBOX)
		#include <winsock2.h>
		#include <windows.h>
		typedef int socklen_t;
		typedef signed __int64 int64;
		typedef unsigned __int64 uint64;
		typedef signed __int32 int32;
		typedef unsigned __int32 uint32;
		typedef signed __int16 int16;
		typedef unsigned __int16 uint16;
		typedef signed __int8 int8;
		typedef unsigned __int8 uint8;
		#define DL_HANDLE HINSTANCE
		#define DSL_CC __stdcall
	#else
		#define VOID void
		#ifndef SOCKET
			#define SOCKET int
		#endif
		#define BYTE unsigned char
		#ifndef HANDLE
			#define HANDLE void *
		#endif
		#define DL_HANDLE HANDLE
		typedef signed long long int64;
		typedef unsigned long long uint64;
		typedef signed int int32;
		typedef unsigned int uint32;
		typedef signed short int16;
		typedef unsigned short uint16;
		typedef signed char int8;
		typedef unsigned char uint8;
		#define DSL_CC
	#endif
	typedef void * T_SOCKET;
	typedef void * DS_CONFIG_SECTION;
#endif
#define SQLITE3_DLL
#include <sqlite3.h>
#include <curl/curl.h>
//#include <tcl.h>
#include <string>
#include <map>
#include <iostream>
using namespace std;
#undef SendMessage

#if defined(WIN32)
	#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
	#define DEPRECATE __declspec(deprecated)
#else
	#if defined(HAVE_VIS)
		#define PLUGIN_EXPORT extern "C" __attribute__ ((visibility("default")))
	#else
		#define PLUGIN_EXPORT extern "C"
	#endif
	#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
		#define DEPRECATE  __attribute__((__deprecated__))
	#else
		#define DEPRECATE
	#endif /* __GNUC__ */
#endif
#define PLUGIN_EXPORT_NAME GetIRCBotPlugin
#define PLUGIN_EXPORT_FULL PLUGIN_EXPORT PLUGIN_PUBLIC * PLUGIN_EXPORT_NAME() { return &plugin; }

#define MAX_IRC_SERVERS 16
#define MAX_IRC_CHANNELS 16
#define MAX_PLUGINS 32
#define MAX_SOUND_SERVERS 32
#define MAX_HOSTMASKS 16
#define MAX_TIMERS 32

#include "common_messages.h"
#include "user_flags.h"
#include "../../Common/remote_protocol.h"

/*! \mainpage RadioBot v5 Plugin API
 *
 * \section welcome Welcome
 * This site documents the plugin API for RadioBot v5.<br />
 * The modules page groups together some of the more common APIs together for easy reference, but for the bulk of things you should check the Classes page.<br />
 *
 * \section psdk Examples
 * To see an example plugin with source code feel free to contact us and we'll send you a copy of one.
 */

/**
 * \defgroup user User & Presence API
 */
/**
 * \defgroup ss Sound Server/Stream API
 */
/**
 * \defgroup command Command Registration/Handling API
 */
/**
 * \defgroup config Configuration API (ircbot.conf)
 */
/**
 * \defgroup reqapi Find/Request API
 */
/**
 * \defgroup yp ShoutIRC.com Stream List (YP) API
 */
/**
 * \defgroup irc IRC Chat API
 */

/** \enum SS_TYPES
 * \ingroup ss
 * The SS_TYPES identifies the different types of Sound Servers.
 */
enum SS_TYPES {
	SS_TYPE_SHOUTCAST = 0x00, ///< A SHOUTcast v1 server
	SS_TYPE_SHOUTCAST2 = 0x01, ///< A SHOUTcast v2 (Ultravox protocol) server
	SS_TYPE_ICECAST	= 0x02, ///< An icecast2 server
	SS_TYPE_STEAMCAST	= 0x03,
	SS_TYPE_SP2P = 0x04
};

#define REF_TRACKING_DEPTH 16

#if !defined(DEBUG)
#define RefUser(x) (x)->ref(x)
#define UnrefUser(x) (x)->unref(x)
#else
#define RefUser(x) (x)->ref(x, nopathA(__FILE__), __LINE__)
#define UnrefUser(x) (x)->unref(x, nopathA(__FILE__), __LINE__)
#endif

/** \struct USER
 \ingroup user
 The USER structure is the record of an IRC user.
 This structure is only valid at the time it was created, so if someone changes their password or flags later it may not be correct anymore.
 If you are holding on to a USER handle for a long time you should use the username to get an updated copy and see if the user is still valid from time to time.
 @sa API_user
*/
struct USER {
	char Nick[128]; ///< The user's username/nick
	char Pass[128]; ///< The user's password if one exists, otherwise it will be an empty string
	char LastHostmask[256]; ///< The user's last known hostmask, if he's ever been seen

	uint32 Flags; ///< IRC letter-based flag system (+o/-x/etc.)
	bool Temporary; ///< The user was gotten from a remote source (auth plugin). The user account won't be loaded again the next time the bot starts up
	time_t Created; ///< When the user was created (only persistent for non-temporary users)
	time_t Seen; ///< When the user was last seen

	int32 NumHostmasks;
	char Hostmasks[MAX_HOSTMASKS][128]; ///< The hostmasks that belong to the user

#if !defined(DEBUG)
	/**
	 * This command increases the reference count of the USER structure. Use this anytime you need to hang on to a copy of the structure.<br>
	 * Do not call this function directly, instead use RefUser() on the USER structure ptr.
	 * @param user a ptr to a valid USER structure
	 */
	void (*ref)(USER * user);
	/**
	 * This command decreases the reference count of the USER structure and deletes it when it reaches zero. Use this when you are done with a structure you have RefUser'ed (or gotten from a call that RefUser'ed it)<br>
	 * Do not call this function directly, instead use UnrefUser() on the USER structure ptr.
	 * @param user a ptr to a valid USER structure
	 */
	void (*unref)(USER * user);
#else
	void (*ref)(USER *, const char * fn, int line);
	void (*unref)(USER *, const char * fn, int line);

	char * fn[REF_TRACKING_DEPTH];
	int line[REF_TRACKING_DEPTH];
#endif
	uint32 ref_cnt; ///< DO NOT TOUCH!!! Only the ref and unref commands should alter ref_cnt.
};

/** \struct USER_PRESENCE
 \ingroup user
 The USER_PRESENCE structure represents a communications handle to a user that is protocol agnostic.
 This allows bot commands to be implemented one time and work for every protocol we support and future protocols without rewriting or reimplementing for each one.
 All pointer fields in this structure should not be modified by the programmer because duplicate presences are shared to reduce memory usage.
*/
struct USER_PRESENCE {
	USER * User; ///< A ptr to a USER structure if the person here matched a hostmask and is an IRCBot user, or NULL if it isn't an IRCBot user.

	/* Some duplication of data here, but necessary for cases where User == NULL and for times when a persons nick != their username */
	const char * Nick; ///< The user's username/nick
	const char * Hostmask; ///< The user's hostmask
	uint32 Flags; ///< The user's flags @sa USER::Flags
	int NetNo; ///< The network # if applicable (for multiple server protocols like IRC), -1 otherwise.
	const char * Channel; ///< If presence was caused by something in a channel/group, it will be here, otherwise it will be NULL

	const char * Desc; ///< Description of the medium this presence is on. ie. IRC, Jabber, etc.

	union {
		T_SOCKET * Sock; ///< Data for the USER_PRESENCE provider, a socket
		void * Ptr1; ///< Data for the USER_PRESENCE provider, ptr to their own data structure
	};

	typedef bool (*userPresSendFunc)(USER_PRESENCE *, const char *); ///< This is the function prototype for std_reply/send_msg/send_chan/etc.
	userPresSendFunc std_reply; ///< This is for a command reply to simplify code for IRC. If the user said something in a channel to activate this USER_PRESENCE, it will NOTICE them, otherwise it will PRIVMSG them.
	userPresSendFunc send_chan; ///< Send a message to the channel the user is in (ONLY valid if Channel != NULL)
	userPresSendFunc send_chan_notice; ///< Send a NOTICE to the channel the user is in (ONLY valid if Channel != NULL)
	userPresSendFunc send_msg; ///< Send a PRIVMSG to the user
	userPresSendFunc send_notice; ///< Send a NOTICE to the user. On protocols where there is no distinction between notices, PMs, etc., it will be the same as send_msg.

#if !defined(DEBUG)
	/**
	 * This command increases the reference count of the USER_PRESENCE structure. Use this anytime you need to hang on to a copy of the structure.<br>
	 * Do not call this function directly, instead use RefUser() on the USER_PRESENCE structure ptr.<br>
	 * Note: It is the responsiblity of this function to RefUser() the User member (if non-NULL), so you don't have to do it seperately.
	 * @param pres a ptr to a valid USER_PRESENCE structure
	 */
	void (*ref)(USER_PRESENCE * pres);
	/**
	 * This command decreases the reference count of the USER_PRESENCE structure and deletes it when it reaches zero. Use this when you are done with a structure you have RefUser'ed (or gotten from a call that RefUser'ed it)<br>
	 * Do not call this function directly, instead use UnrefUser() on the USER_PRESENCE structure ptr.<br>
	 * Note: It is the responsiblity of this function to UnrefUser() the User member (if non-NULL), so you don't have to do it seperately.
	 * @param pres a ptr to a valid USER_PRESENCE structure
	 */
	void (*unref)(USER_PRESENCE * pres);
#else
	void (*ref)(USER_PRESENCE *, const char * fn, int line);
	void (*unref)(USER_PRESENCE *, const char * fn, int line);
#endif
	uint32 ref_cnt; ///< DO NOT TOUCH!!! Only the ref and unref commands should alter ref_cnt.
};

#ifndef SWIG

typedef int (*CommandProcType)(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type); ///< type is one of the CM_ALLOW_* defines below

/** \addtogroup command
 * @{
 */

/** \struct COMMAND_ACL
 * This is the access control method for commands starting in IRCBot v5. It is pretty straightforward and the fields are explained below.
 */
struct COMMAND_ACL {
	uint32 flags_all; ///< user must have all these flags to be allowed
	uint32 flags_any; ///< user must have at least one of these to be allowed
	uint32 flags_not; ///< user cannot have any of these flags and be allowed
};

/** \struct COMMAND
 * The COMMAND structure is the internal record of a command. It contains the command, how to handle it, help text, ACL, what protocols may use it, etc.
 * @sa API_commands
 */
struct COMMAND {
	const char * command; ///< A string containing the command name
#if defined(OLD_COMTRACK)
	COMMAND *Prev,*Next;
#endif
	const char * desc; ///< A string containing the help text for the command
	CommandProcType help_proc; ///< Optional handler to provide extended help beyond the desc field above
	union {
		const char * action; ///< The raw IRC command to send (if proc_type == COMMAND_ACTION)
		CommandProcType proc; ///< The command handler (if proc_type == COMMAND_PROC)
	};
	uint32 proc_type; ///< COMMAND_ACTION or COMMAND_PROC
	COMMAND_ACL acl; ///< The flags needed (and not allowed) to use this command
	uint32 mask; ///< A bitmask of the CM_ALLOW_* defines below, specifies from which contexts a command may be used. Commands loaded from ircbot.text will also have CM_FROM_TEXT set as well, which is used by the Rehash command.
	int plugin; ///< Plugin number that registered this command, or -1 for an IRCBot internal command
	uint32 ref_cnt; ///< DO NOT TOUCH!!!
};
#endif

#define COMMAND_ACTION 0x00
#define COMMAND_PROC 0x01
#define CM_ALLOW_IRC_PUBLIC			0x00000001 ///< This plag means the command can be used via IRC channel message (ie. publically)
#define CM_ALLOW_IRC_PRIVATE		0x00000002 ///< This plag means the command can be used via IRC private message.
#define CM_ALLOW_CONSOLE_PUBLIC		0x00000004 ///< This plag means the command can be used in channels/groups via 'console' interfaces. These include DCC chat, Skype, Jabber, Twitter, SMS, etc.
#define CM_ALLOW_CONSOLE_PRIVATE	0x00000008 ///< This plag means the command can be used in private message via 'console' interfaces. These include DCC chat, Skype, Jabber, Twitter, SMS, etc.
#define CM_FLAG_FROM_TEXT			0x20000000 ///< This flag means the command was loaded from ircbot.text, this is used by the rehash function to wipe out old commands before loading the new text file.
#define CM_FLAG_SLOW				0x10000000 ///< This flag is set on slow interfaces or ones with limited speed, such as SMS. Commands supporting this flag can adjust themselves accordingly.
#define CM_FLAG_ASYNC				0x40000000 ///< This flag is set on functions that take a long time to execute so they will be run in a separate thread.
#define CM_FLAG_NOHANG				0x80000000 ///< This flag is set on interfaces that buffer output until the command is done, so the USER_PRESENCE should not be held on to for a long time because the user won't see any output until the command is done.
#define CM_NO_FLAGS(x) (x & 0x0000000F)

#define CM_ALLOW_ALL_PUBLIC			CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC
#define CM_ALLOW_ALL_PRIVATE			CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE
#define CM_ALLOW_ALL					CM_ALLOW_ALL_PUBLIC|CM_ALLOW_ALL_PRIVATE

/**@}*/

#ifndef SWIG
/** \struct REMOTE_HEADER
 * \ingroup remote
 * This is the packet header for the remote protocol. It consists of a command and the length of data that follows the header (if needed).<br>
 * It is designed to be extremely simple to implement, and is binary-compatible back to IRCBot v2 (of course, new commands that didn't exist back during the v2 days won't work with v2)
 * @sa http://wiki.shoutirc.com/index.php/Remote_Commands
 */
struct REMOTE_HEADER {
	union {
		REMOTE_COMMANDS_C2S ccmd; ///< The command to send to the bot
		REMOTE_COMMANDS_S2C scmd; ///< Reply from bot to client
	};
	uint32 datalen; ///< The length of data following the header (little-endian)
};
#endif

#ifdef SWIG
%immutable;
#endif

/** \struct SOUND_SERVER
 * \ingroup ss
 * This structure lets plugins get the settings for a Sound Server (host/ip, mountpoint, port, etc.)
 * @sa SS_TYPES
 * @sa BOTAPI::GetSSInfo
 */
typedef struct {
	char host[128];
	char mount[128];
	char admin_pass[128];
	int32 port, streamid;
	SS_TYPES type; ///< The sound server type. @sa SS_TYPES
	bool has_source; ///< Does this sound server have a source connected to it? (ie. is there a stream active right now)
} SOUND_SERVER;

/** \struct STATS
 * \ingroup ss
 * This structure contains the latest combined stats for your Sound Servers.
 * @sa BOTAPI::GetStreamInfo
 */
typedef struct {
	bool online; ///< Is your radio online (depends on setting of PullNameFromAnyServer how it is calculated)
	bool title_changed; ///< Did the title change from the last scrape?
	int bitrate;
	int listeners;
	int peak;
	int maxusers;

	char curdj[128]; ///< The current DJ
	char lastdj[128]; ///< The DJ during the previous scrape
	char cursong[512]; ///< The current song/stream title
} STATS;

/** \struct SONG_RATING
 * This structure contains the latest stats combined stats for your Sound Server.
 * @sa BOTAPI::RateSong
 * @sa BOTAPI::RateSongByID
 * @sa BOTAPI::GetSongRating
 * @sa BOTAPI::GetSongRatingByID
 */
struct SONG_RATING {
	uint32 Rating; ///< The song rating from 0-5 (0 = not rated)
	uint32 Votes; ///< The number of votes on this song
};

/**
 * \ingroup reqapi
 */
#define MAX_FIND_RESULTS 100
/** \struct FIND_RESULT
 * \ingroup reqapi
 * This structure contains contains an individual search result of a \@find.
 * @sa FIND_RESULTS
 * @sa REQUEST_INTERFACE
 */
struct FIND_RESULT {
	char * fn; ///< At a minimum fn must be set so the bot has something to display. The bot will automatically show only the filename is a full path is included.
	uint32 id; ///< Optional member the source plugin can use however it wants.
};
/** \struct FIND_RESULTS
 * \ingroup reqapi
 * This structure contains contains the result of a \@find. The bot keeps track of the last \@find result so people can use !request # and not the full filename.
 * @sa REQUEST_INTERFACE
 */
struct FIND_RESULTS {
	char query[128];
	int start;

	FIND_RESULT songs[MAX_FIND_RESULTS];
	int num_songs;
	bool have_more;

	int plugin;
	void (*free)(FIND_RESULTS * res); ///< The bot will call this when it is done with the structure, the source plugin should delete the structure and any associated resources.
};
typedef void (*find_finish_type)(USER_PRESENCE * ut, FIND_RESULTS * qRes, int max);
/** \struct REQUEST_INTERFACE
 * \ingroup reqapi
 * This structure contains contains the result of a \@find. The bot keeps track of the last \@find result so people can use !request # and not the full filename.
 * @sa BOTAPI::RequestsOnHooked
 * @sa FIND_RESULTS
 * @sa FIND_RESULT
 */
struct REQUEST_INTERFACE {
	void (*find)(const char * fn, int max_results, int start, USER_PRESENCE * ut, find_finish_type find_finish); ///< Sent to the source plugin when someone does an \@find
	bool (*request_from_find)(USER_PRESENCE * pres, FIND_RESULT * res, char * reply, size_t replySize); ///< Called when someone does a !request with a # that matches a \@find result
	bool (*request)(USER_PRESENCE * pres, const char * txt, char * reply, size_t replySize); ///< Called when a !request is done with a filename or # after results have expired
};

#ifdef SWIG
%mutable;
#endif

#ifndef SWIG
#ifndef DOXYGEN_SKIP

#endif // DOXYGEN_SKIP

/** \struct API_textfunc
 This structure contains the latest stats combined stats for your Sound Server.
 */
typedef struct {
	/**
		Simple string replacement
		@param str The string to do replacements in
		@param bufsize The buffer size that str is in. If you buffer is the same length as the string you should use strlen(str)+1
		@param findstr The string to find
		@param replstr The string to replace find with
	*/
	int (*replace)(char * str, int32 bufsize, const char *findstr, const char * replstr);
	char * (*trim)(char *buf, const char * strtrim); ///< Trims both ends of a string of any characters in the string strtrim
	char * (*duration)(uint32 num, char * buf); ///< Encodes a number of seconds into this format: 2d 1h 15m and stores in buf (and returns buf)
	int32 (*decode_duration)(const char * buf); ///< Decodes a string of this format: 2d 1h 15m into a number of seconds
	int (*wildcmp)(const char *wild, const char *string); ///< Case-sensitive wildcard matching, returns 0 for not match, other for match
	int (*wildicmp)(const char *wild, const char *string); ///< Case-insensitive wildcard matching, returns 0 for not match, other for match
	bool (*get_range)(const char * buf, int32 * imin, int32 * imax); ///< Parses a string in the form of MIN:MAX or just a number. ie. 1:5, 10, 1:2, 75, etc. In the case of a single number MIN=MAX.
} API_textfunc;

/** \struct API_commands
 * \ingroup command
 * This structure contains functions related to the commands system like registering a command, filling ACLs, etc.
 * @sa COMMAND
 * @sa COMMAND_ACL
 */
typedef struct {
	COMMAND * (*FindCommand)(const char * command); ///< Finds the record of command
	COMMAND * (*RegisterCommand_Action)(int pluginnum, const char * command, const COMMAND_ACL * acl, uint32 mask, const char * action, const char * desc); ///< Register a simple IRC action command, this is the type of command defined in ircbot.text. See RegisterCommand_Proc for more detailed info @sa API_commands::RegisterCommand_Proc
	/**
	 Registers a command with the bot. The bot makes it's own copy of the string pointers and ACL so you don't need to keep copies around or anything.
	 @param pluginnum Your plugin number as given to your plugin in PLUGIN_PUBLIC::init()
	 @param command The command's name (ie. request). This must not have a command prefix (for example: \!request or \@request would be incorrect)
	 @param acl A pointer to an COMMAND_ACL structure defining who can use the command. To allow anyone to use a command just fill it will all zeroes. @sa API_commands::FillACL @sa API_commands::MakeACL
	 @param mask Which protocols can use the commands, one or more of the CM_ALLOW_* constants. @sa CM_ALLOW_IRC_PRIVATE @sa CM_ALLOW_IRC_PUBLIC @sa CM_ALLOW_CONSOLE_PRIVATE @sa CM_ALLOW_CONSOLE_PUBLIC
	 @param proc Your command handler @sa CommandProcType
	 @param desc A short one-line description of the command. This is what users will see if they type !help command
	 */
	COMMAND * (*RegisterCommand_Proc)(int pluginnum, const char * command, const COMMAND_ACL * acl, uint32 mask, CommandProcType proc, const char * desc);
	void (*UnregisterCommandsByPlugin)(int pluginnum); ///< Unregisters all commands that were registered by pluginnum
	void (*UnregisterCommandByName)(const char * command); ///< Unregisters the command command
	void (*UnregisterCommand)(COMMAND * command); ///< Unregisters the commands with handle command (do not make the COMMAND structure yourself, it should be one returned by FindCommand @sa API_commands::FindCommand

	char (*PrimaryCommandPrefix)(); ///< This returns the primary command prefix (for displaying examples to users). Currently ! but may be user-configurable someday
	bool (*IsCommandPrefix)(char ch); ///< This returns true is character ch is a command prefix (!/@/?/etc.). This may be user-configurable someday so it's better to use this function than hardcode
	COMMAND_ACL * (*FillACL)(COMMAND_ACL * acl, uint32 flags_have_all, uint32 flags_have_any, uint32 flags_not); ///< Fills a COMMAND_ACL structure with the given flags
	COMMAND_ACL (*MakeACL)(uint32 flags_have_all, uint32 flags_have_any, uint32 flags_not); ///< Makes a COMMAND_ACL structure with the given flags and returns it

	int (*ExecuteProc)(COMMAND * com, const char * parms, USER_PRESENCE * ut, uint32 type); ///< Executes a command. If you have made a console interface you will need to create and manage the USER_PRESENCE and pass type as CM_ALLOW_CONSOLE_*
	void (*RegisterCommandHook)(const char * command, CommandProcType hook_func); ///< Registers a hook on a command so your handler will be called first. Return 0 to continue to the next hook or main command handler, or 1 if you handled it.
	void (*UnregisterCommandHook)(const char * command, CommandProcType hook_func); ///< Registers a command hook
} API_commands;

#ifndef DOXYGEN_SKIP
// case-independent (ci) string less_than
// returns true if s1 < s2
struct ci_less : binary_function<string, string, bool> {

  // case-independent (ci) compare_less binary function
  struct nocase_compare : public binary_function<unsigned char,unsigned char,bool>
    {
    bool operator() (const unsigned char& c1, const unsigned char& c2) const
      { return tolower (c1) < tolower (c2); }
    };

  bool operator() (const string & s1, const string & s2) const
    {

    return lexicographical_compare
          (s1.begin (), s1.end (),   // source range
           s2.begin (), s2.end (),   // dest range
                nocase_compare ());  // comparison
    }
}; // end of ci_less

typedef std::map<std::string, std::string, ci_less> sql_row;
typedef std::map<int, sql_row> sql_rows;
#endif /* DOXYGEN_SKIP */

/** \struct API_db
 * This structure contains functions related to the bot's internal SQLite DB. Plugins can use this to store any kind of information they want.\n
 * If you create your own tables make sure to prefix them with something unique to make sure it doesn't conflict with anyone else's work.\n
 * Also do not mess with the busy/timeout handlers, by default IRCBot will handle that for you in Query/GetTable and try the query several times to make sure it works.\n
 * I won't document all these functions since they work the same way as sqlite3's API so you can just read up there.
 * @sa http://www.sqlite.org/c3ref/intro.html
 */
typedef struct {
	sqlite3 * (*GetHandle)(); ///< Return's the bot's sqlite3 handle
	int (*Query)(const char * query, sqlite3_callback cb, void * parm, char ** error);
	void (*Free)(char * ptr);
	int (*GetTable)(const char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg);
	const char * (*GetTableEntry)(int row, int col, char **result, int nrow, int ncol);
	void (*FreeTable)(char **result);
	char * (*MPrintf)(const char* fmt,...); ///< Identical to sqlite3_mprintf, free returned string with API_db::Free @sa http://www.sqlite.org/c3ref/mprintf.html
	void (*Lock)(); ///< Lock the database for exclusive access, only use this if you have to (mainly for InsertID)
	void (*Release)(); ///< Release the database lock
	int64 (*InsertID)(); ///< Last insert ID
} API_db;

/** \struct API_curl
 * This structure contains functions related to the bot's instance of libCURL. Plugins can use this to do any kind of file transfers they need.\n
 * I won't document all these functions since they work the same way as libcurl's API so you can just read up there.
 * @sa http://curl.haxx.se/libcurl/c/
 */
typedef struct {
	CURL * (*easy_init)(void);
	CURLcode (*easy_setopt)(CURL *curl, CURLoption option, ...);
	CURLcode (*easy_perform)(CURL *curl);
	void (*easy_cleanup)(CURL *curl);
	const char * (*easy_strerror)(CURLcode);
	CURLcode (*easy_getinfo)(CURL *curl, CURLINFO info, ...);

	CURLM * (*multi_init)(void);
	CURLMcode (*multi_add_handle)(CURLM *multi_handle, CURL *curl_handle);
	CURLMcode (*multi_remove_handle)(CURLM *multi_handle, CURL *curl_handle);
	CURLMcode (*multi_perform)(CURLM *multi_handle, int *running_handles);
	CURLMcode (*multi_cleanup)(CURLM *multi_handle);
	CURLMsg * (*multi_info_read)(CURLM *multi_handle, int *msgs_in_queue);
	const char * (*multi_strerror)(CURLMcode);

	CURLFORMcode (*formadd)(struct curl_httppost **httppost, struct curl_httppost **last_post, ...);
	void (*formfree)(struct curl_httppost *form);

	struct curl_slist * (*slist_append)(struct curl_slist *, const char *);
	void (*slist_free_all)(struct curl_slist *);

	char * (*escape)(const char *string, int length);
	char * (*unescape)(const char *string, int length);

	void (*free)(void *p);
} API_curl;

/** \struct API_memory
 * These functions let you allocate/free memory in the bot's memory instance. Standard malloc-style semantics apply.\n
 * Check out the zmalloc example \#defines at the end of plugins.h for an easy way to use these.
 */
typedef struct {
	void * (DSL_CC * alloc)(size_t lSize);
	void * (DSL_CC *realloc)(void * ptr, size_t lSize);
	char * (DSL_CC *strdup)(const char * ptr);
	wchar_t * (DSL_CC *wcsdup)(const wchar_t * ptr);
	void (DSL_CC *free)(void * ptr);
	char * (DSL_CC *mprintf)(const char * ptr, ...); ///< Like vsprintf, free returned pointer with API_memory::free
} API_memory;

/** \struct API_ial
 * These provide an interface to the bot's internal address list (IAL). It maintains the list of users in each channel and their modes (op, hop, etc.)\n
 * Pretty basic so far, let me know if you want anything added because I just add APIs as I need them.
 */
typedef struct {
	bool (*is_nick_in_chan)(int netno, const char * chan, const char * nick);
	bool (*is_bot_in_chan)(int netno, const char * chan);
	int (*num_nicks_in_chan)(int netno, const char * chan);
	bool (*get_nick_in_chan)(int netno, const char * chan, int num, char * nick, int nickSize);
} API_ial;


typedef bool (*EnumUsersByLastHostmaskCallback)(USER * user, void * ptr);

/** \struct API_user
 * \ingroup user
 * These functions are related to IRCBot user accounts and user flags.
 * @sa USER
 * @sa COMMAND_ACL
 */
typedef struct {
	/**
	 * Get a handle to the user matching a certain hostmask.
	 * @param hostmask The hostmask to search for
	 * @return A pointer to a USER structure if a user was found, NULL otherwise. Make sure to UnrefUser() the returned pointer if it's not NULL when you are done with it!
	 */
	USER * (*GetUser)(const char * hostmask);
	/**
	 * Get a handle to the user with a certain IRCBot username. (Remember: their bot username isn't always the same as their IRC nick, so if at all possible it's better to use API_user::GetUser with a hostmask!)
	 * @param nick The username to search for
	 * @return A pointer to a USER structure if a user was found, NULL otherwise. Make sure to UnrefUser() the returned pointer if it's not NULL when you are done with it!
	 */
	USER * (*GetUserByNick)(const char * nick);
	bool (*IsValidUserName)(const char * nick); ///< Check to see if a user with the username 'nick' exists.
	uint32 (*GetUserCount)(); ///< Only returns users in your ircbot.db, not auth plugins

	void (*EnumUsersByLastHostmask)(EnumUsersByLastHostmaskCallback callback, const char * hostmask, void * ptr);

	uint32 (*GetUserFlags)(const char * hostmask); ///< Get the user flags for a particular hostmask. If it matches a user it will return their flags, if not it will return the user-defined default flags.
	USER * (*AddUser)(const char * nick, const char * pass, uint32 flags, bool temp);
	void (*SetUserFlags)(USER * user, uint32 flags); ///< Change a user's flags
	void (*AddUserFlags)(USER * user, uint32 flags); ///< Add flags to a user
	void (*DelUserFlags)(USER * user, uint32 flags); ///< Remove flags from a user

	uint32 (*GetDefaultFlags)(); ///< Returns the default flags for a user without a bot account.

	/**
	 * Check if any of some wanted flag are in flags.
	 * @param flags A user's flags
	 * @param wanted The flags you want them to have
	 * @return true if they have them, false otherwise.
	 */
	bool (*uflag_have_any_of)(uint32 flags, uint32 wanted);
	/**
	 * This function gives a general comparison of user rank based on their flags.
	 * @param flags1 User 1's flags.
	 * @param flags2 User 2's flags.
	 * @return 1 if User 1 is a higher rank, 0 if they are equal, -1 if User 1 is a lower rank.
	 */
	int (*uflag_compare)(uint32 flags1, uint32 flags2);
	/**
	 * Given numeric user flags, it will create a string version for display (ie. +abcdefg)
	 * @param flags A user's flags
	 * @param buf The char buffer to output to.
	 * @param bufSize The size of buf
	 */
	void (*uflag_to_string)(uint32 flags, char * buf, size_t bufSize);
	/**
	 * Modifies user flags with a flag string and returns numerical user flags.\n
	 * Strings must start with an operator: +, -, or =. As you would imagine + adds any following flags to the user's flags, - removes them, and = sets the flags obliterating any existing flags.\n
	 * You can also mix and match operators inline.\n
	 * Example: +abcdefg (adds abcdefg to user's flags)\n
	 * Example: -abcdefg (removes abcdefg from user's flags)\n
	 * Example: =abcdefg (sets user's flags to abcdefg, removing all others)\n
	 * Example: +abc-def (adds abc to user's flags and removes def)\n
	 * @param buf The flag modification string
	 * @param cur The user's current flags.
	 * @return The new user flags
	 */
	uint32 (*string_to_uflag)(const char * buf, uint32 cur);

	/**
	 * Given a full hostmask, alter into a wildcard matching hostmask\n
	 *	Type 0: *!*user@*.host\n
	 *	Type 1: *!*@*.host\n
	 *	Type 2: *nick*!*user@*.host\n
	 *	Type 3: *nick*!*@*.host
	 * @param hostmask A full hostmask in nick!user\@host form
	 * @param msgbuf The buffer to load the altered hostmask in to
	 * @param bufSize The size of your buffer
	 * @param type The type of mask to apply, see list above.
	 * @return true if the mask was altered successfully, false otherwise
	 */
	bool (*Mask)(const char * hostmask, char * msgbuf, size_t bufSize, int type);

	uint32 (*GetLevelFlags)(int level); ///< Returns the default flags for an old IRCBot user level. This is used by the LevelEmul plugin and shouldn't be used by other plugins.
	void (*SetLevelFlags)(int level, uint32  flags); ///< Sets the default flags for an old IRCBot user level. This is used by the LevelEmul plugin and shouldn't be used by other plugins.
} API_user;

/** \struct API_config
 * \ingroup config
 * These functions give you access to the configuration in ircbot.conf.\n
 * As you probably know from your own dealings with ircbot.conf, the bot's configuration is based on hierarchical sections with values inside them.
 */
typedef struct {
	/**
	  This function finds and returns a config section.\n
	  Example getting the AutoDJ section: DS_CONFIG_SECTION * sec = GetConfigSection(NULL, "AutoDJ");\n
	  Example getting the AutoDJ/Options section after getting AutoDJ above: DS_CONFIG_SECTION * options = GetConfigSection(sec, "Options");
	  @param parent The parent section to search in, NULL to search the root section.
	  @param name The name of the section to find
	 */
	DS_CONFIG_SECTION * (*GetConfigSection)(DS_CONFIG_SECTION * parent, const char * name);
	/**
	  This function finds a section value and if it exists fills buf with it.\n
	  If the setion value is a number it will be converted to a string with snprintf
	  @param section The section to search in. This must not be NULL!
	  @param name The name of the value to find
	  @param buf The buffer to store the value in
	  @param bufSize The size of buf
	  @return true if the value was found successfully, false otherwise
	 */
	bool (*GetConfigSectionValueBuf)(DS_CONFIG_SECTION * section, const char * name, char * buf, size_t bufSize);
	/**
	  This function finds a string section value and if it exists returns a const pointer to the value.\n
	  The returned pointer should not be modified or free'd, it belongs to the configuration parser.\n
	  In the value is a number the function will return NULL
	  @param section The section to search in. This must not be NULL!
	  @param name The name of the value to find
	  @return A pointer to the string value if found successfully, NULL otherwise
	 */
	char * (*GetConfigSectionValue)(DS_CONFIG_SECTION * section, const char * name);
	/**
	  This function finds a numerical section value and if it exists returns the number.\n
	  In the value is a string it will be converted to a number with atol()
	  @param section The section to search in. This must not be NULL!
	  @param name The name of the value to find
	  @return The number if found, -1 otherwise. (note: if you have an option where -1 might be a valid setting you should use API_config::GetConfigSectionValueBuf or API_config::IsConfigSectionValue to make sure it actually exists)
	 */
	long (*GetConfigSectionLong)(DS_CONFIG_SECTION * section, const char * name);
	/**
	  This function finds a floating point section value and if it exists returns the number.\n
	  In the value is a string it will be converted to a number with atof()
	  @param section The section to search in. This must not be NULL!
	  @param name The name of the value to find
	  @return The number if found, -1 otherwise. (note: if you have an option where -1 might be a valid setting you should use API_config::GetConfigSectionValueBuf or API_config::IsConfigSectionValue to make sure it actually exists)
	 */
	double (*GetConfigSectionFloat)(DS_CONFIG_SECTION * section, const char * name);
	/**
	  This function checks to see if a section value exists\n
	  In the value is a string it will be converted to a number with atof()
	  @param section The section to search in. This must not be NULL!
	  @param name The name of the value to find
	  @return true if the value was found, false otherwise.
	 */
	bool (*IsConfigSectionValue)(DS_CONFIG_SECTION * section, const char * name);
} API_config;

/** \struct YP_HANDLE
 * \ingroup yp
 * This holds your YP listing ID and the key to edit it.
 * @sa API_yp
 */
typedef struct {
	uint32 yp_id;
	char key[68];
} YP_HANDLE;

/** \struct YP_CREATE_INFO
 * \ingroup yp
 * This struct holds your initial information for your YP listing. You can free/delete/whatever this structure and it's contents after the AddToYP() call.
 * @sa API_yp
 */
typedef struct {
	const char * source_name;
	const char * mime_type;
	const char * cur_playing;
	const char * genre;
} YP_CREATE_INFO;

/** \struct API_yp
 * \ingroup yp
 * These functions let you create/update/delete a YP listing in the ShoutIRC.com Stream List.
 * @sa YP_CREATE_INFO
 */
typedef struct {
	/**
		Called by your source plugin when it starts playing with listing information.\n
		@param yp An empty YP_HANDLE struct for AddToYP to fill in.
		@param sc A pointer to the SOUND_SERVER the plugin is feeding.
		@param ypinfo A pointer to a YP_CREATE_INFO structure you have filled out.
		@return false if the listing failed, true for success.
		@sa SOUND_SERVER
		@sa YP_CREATE_INFO
		@sa YP_HANDLE
	*/
	bool (*AddToYP)(YP_HANDLE * yp, SOUND_SERVER * sc, YP_CREATE_INFO * ypinfo);
	/**
		Update your YP listing with the current song.\n
		@param yp A YP_HANDLE struct previously filled in by a successful call to AddToYP.
		@param cur_playing A pointer to the title of the current song the source plugin is playing.
		@return 0 on success, -1 on general error (but should be tried again in the future), -2 for a hard error where you should call AddToYP again and start over
		@sa YP_HANDLE
	*/
	int (*TouchYP)(YP_HANDLE * yp, const char * cur_playing);
	/**
		Delete/deactivate your YP listing.\n
		@param yp A YP_HANDLE struct previously filled in by a successful call to AddToYP.
		@sa YP_HANDLE
	*/
	void (*DelFromYP)(YP_HANDLE * yp);
} API_yp;

/**
 * \ingroup irc
 * Priorities for SendSock_Priority/SendIRC_Priority/SendIRC_Delay
 */
enum {
	PRIORITY_LOWEST			=  0,
	PRIORITY_SPAM			=  1,
	PRIORITY_DEFAULT		= 10,
	PRIORITY_INTERACTIVE	= 60,
	PRIORITY_IMMEDIATE		= 99
};

/** \struct API_irc
 * \ingroup irc
 * These functions let directly interact with IRC networks/connections as well as log things to the LogChan if one is set.
 */
typedef struct {
	bool (*LogToChan)(const char * buf); ///< Logs a string to LogChan (if defined)
	/**
	 * Queues a string to be sent to IRC connection netno's socket with the specified priority (see the PRIORITY_* defines).\n
	 * IMPORTANT: All IRC socket output should be done through one of the API_irc::SendIRC_* functions and/or API_irc::SendSock_Priority!!!
	 * @param netno The IRC connection number
	 * @param buf The string to send
	 * @param datalen Length of string to send. If datalen <= 0 then it will strlen() it for you.
	 * @param priority The priority of the string from 0-99, with 0 being least important and 99 being most important. @sa PRIORITY_IMMEDIATE @sa PRIORITY_INTERACTIVE @sa PRIORITY_DEFAULT @sa PRIORITY_SPAM @sa PRIORITY_LOWEST
	 * @return The length of the string queued, or -1 on error
	 */
	int (*SendIRC_Priority)(int32 netno, const char * buf, int32 datalen, uint8 priority);
	/**
	 * Queues a string to be sent to IRC connection netno's socket with the specified priority (see the PRIORITY_* defines).\n
	 * IMPORTANT: All IRC socket output should be done through one of the API_irc::SendIRC_* functions and/or API_irc::SendSock_Priority!!!
	 * @param netno The IRC connection number
	 * @param buf The string to send
	 * @param datalen Length of string to send. If datalen <= 0 then it will strlen() it for you.
	 * @param priority The priority of the string from 0-99, with 0 being least important and 99 being most important.
	 * @param delay the number of seconds to to wait before sending the data
	 * @return The length of the string queued, or -1 on error
	 */
	int (*SendIRC_Delay)(int32 netno, const char * buf, int32 datalen, uint8 priority, uint32 delay);
	/**
	 * Queues a string to be sent to a socket with the specified priority (see the PRIORITY_* defines).\n
	 * IMPORTANT: All IRC socket output should be done through one of the API_irc::SendIRC_* functions and/or API_irc::SendSock_Priority!!!
	 * @param sock The socket
	 * @param buf The string to send
	 * @param datalen Length of string to send. If datalen <= 0 then it will strlen() it for you.
	 * @param priority The priority of the string from 0-99, with 0 being least important and 99 being most important. @sa PRIORITY_IMMEDIATE @sa PRIORITY_INTERACTIVE @sa PRIORITY_DEFAULT @sa PRIORITY_SPAM @sa PRIORITY_LOWEST
	 * @return The length of the string queued, or -1 on error
	 */
	int DEPRECATE (*SendSock_Priority)(T_SOCKET * sock, const char * buf, int32 datalen, uint8 priority, uint32 delay);
	void DEPRECATE (*ClearSockEntries)(T_SOCKET * sock); ///< Removes any queued sends for the specified socket.
	// ^- I don't think anything uses these 2 anymore in plugins, but they could potentially be used in command handlers as long as you verify they are IRC connections.

	bool (*GetDoSpam)(); ///< Gets the value of the global DoSpam flag
	void (*SetDoSpam)(bool dospam); ///< Sets the global DoSpam flag
	void (*SetDoSpamChannel)(bool dospam, int netno, const char * chan); ///< Sets the DoSpam flag for a particular channel
	void (*SetDoOnJoin)(bool doonjoin);
	void (*SetDoOnJoinChannel)(bool doonjoin, int netno, const char * chan);

	/**
	 * Get's the bot's current nickname on a particular IRC network. If the network is not connected it will return the default nick for that network\n
	 * This is a return from a STL string's .c_str() so make sure you copy it because it may not be valid long
	 * @param net The network number
	 */
	const char *(*GetCurrentNick)(int32 net);
	/**
	 * Get's the bot's default nickname on a particular IRC network or the global default nickname\n
	 * This is a return from a STL string's .c_str() so make sure you copy it because it may not be valid long
	 * @param net The network number, or -1 for the global default nickname
	 */
	const char *(*GetDefaultNick)(int32 net);
	int (*NumNetworks)(); ///< Returns the number of IRC networks that have been defined in ircbot.conf
	bool (*IsNetworkReady)(int32 num); ///< Check to see if a particular IRC network is connected and logged in
} API_irc;

enum REQUEST_MODES {
	REQ_MODE_OFF		= 0,
	REQ_MODE_DJ_NOREQ	= 1,
	REQ_MODE_NORMAL		= 2,
	REQ_MODE_REMOTE		= 3,
	REQ_MODE_HOOKED		= 4
};
#define REQUESTS_ON(x) (x >= REQ_MODE_NORMAL)
#define HAVE_DJ(x) (x >= REQ_MODE_DJ_NOREQ)

typedef struct {
	/**
	 * \ingroup ss
	 * Get's information on one of the sound servers in ircbot.conf
	 * @param ssnum The sound server number. If you use -1 the function will return the number of servers defined in ircbot.conf (in this case the ss param can be NULL).
	 * @param ss A SOUND_SERVER structure to be filled in
	 * @sa SOUND_SERVER
	 * @sa SS_TYPES
	 */
	int (*GetSSInfo)(int32 ssnum, SOUND_SERVER * ss);
	STATS * (*GetStreamInfo)(); ///< Get's information on the stream status, do not modify/free/etc. the pointer. \ingroup ss
	REQUEST_MODES (*GetRequestMode)();
	void (*EnableRequests)(bool activate); ///< If activate is true, it activates requests and sets the mode to hooked. This is used for source plugins that don't provide a REQUEST_INTERFACE. If activate is false, disables requests. @sa REQUEST_INTERFACE \ingroup reqapi
	void (*EnableRequestsHooked)(REQUEST_INTERFACE *); ///< Activates requests in hooked mode. This is used for source plugins like AutoDJ, SimpleDJ, SAM, etc., to take requests. If activate is false, disables requests. @sa REQUEST_INTERFACE \ingroup reqapi

	bool (*AreRatingsEnabled)();
	unsigned int (*GetMaxRating)();
	void (*RateSongByID)(uint32 id, const char * nick, uint32 rating); ///< @param id id should only be gotten from a successful SOURCE_GET_SONGID call.
	void (*RateSong)(const char * song, const char * nick, uint32 rating);
	void (*GetSongRatingByID)(uint32 id, SONG_RATING * r); ///< @param id id should only be gotten from a successful SOURCE_GET_SONGID call.
	void (*GetSongRating)(const char * song, SONG_RATING * r);
} API_ss;

struct BOTAPI;
typedef struct {
	unsigned char plugin_ver; ///< The bot binary ABI identifier, you should always set this to IRCBOT_PLUGIN_VERSION instead of setting it manually.

	char * guid; ///< The unique GUID for your plugin. It should be in "registry" format, and please use a real GUID generator and don't try to freehand it.
	char * plugin_name_short; ///< The short name of your plugin, it should be 3 or less words and NOT have a version number or anything in it.
	char * plugin_name_long; ///< The long name of your plugin with version info (if you want).
	char * plugin_author; ///< The name of the author of this plugin. (Can be a person, company, URL, whatever).

	/**
		Called when the bot is loading your plugin.\n
		@param num Your plugin number
		@param BotAPI A pointer to the bot's plugin API.
		@return 0 if your plugin load failed, 1 for success.
	*/
	int (*init)(int num, BOTAPI * BotAPI);

	/**
		Called when the bot is unloading your plugin
	*/
	void (*quit)();

	/**
		This function handles messages from the bot and other plugins ranging from incoming text, stats, etc.\n
		See common_messages.h for information on individual messages.

		@param msg The message ID
		@param data The data sent with the message
		@param datalen The length of the data sent with the message. If datalen == 0 then data may or may not be a NULL ptr
		@return 0 to let other plugins handle the message, 1 otherwise.
	*/
	int (*message)(unsigned int msg, char * data, int datalen);

	/**
		Called when either:\n
			a) The bot is PMed with any text, OR\n
			b) In-channel messages that begin with a command prefix (!, @, etc.)
		@param ut USER_PRESENCE handle
		@param msg the message from IRC
		@return 0 to let the bot and/or other plugins handle the message, 1 otherwise.
	*/
	int (*on_text)(USER_PRESENCE * ut, const char * msg);

	/**
		Called when either:\n
			a) The bot is noticed with any text, OR\n
			b) In-channel notices are received regadless of what they are prefixed with
		@param ut USER_PRESENCE handle
		@param msg the notice from IRC
		@return 0 to let the bot and/or other plugins handle the notice, 1 otherwise.
	*/
	int (*on_notice)(USER_PRESENCE * ut, const char * msg);

	/**
		Called on each incoming raw IRC line.

		@param netno The network number the line was received from
		@param msg The line
		@return 0 to let the bot and/or other plugins handle the message, 1 otherwise.
	*/
	int (*raw)(int netno, char * msg);

	/**
		Called when a packet is received on a remote connection.

		@param ut USER_PRESENCE handle
		@param cliversion The client's protocol version
		@param head The REMOTE_HEADER containing the command and data length
		@param data The data sent with the command. If data len is 0 data may or may not be a NULL ptr
		@return 0 to let the bot and/or other plugins handle the packet, 1 otherwise.
	*/
	int (*remote)(USER_PRESENCE * ut, unsigned char cliversion, REMOTE_HEADER * head, char * data);

	//filled in by IRCBot for you
	DL_HANDLE hInstance;

	//for use only by IRCBot
	void * private_struct;
} PLUGIN_PUBLIC;

struct BOTAPI {
	uint32 (*GetVersion)(); ///< Returns the bot version in the form 0x00AAIIFF where AA = Major Version, II = Minor Version, FF = Flags @sa IRCBOT_VERSION_FLAG_BASIC @sa IRCBOT_VERSION_FLAG_FULL
	const char * (*GetVersionString)(); ///< Returns the bot version as a string
	const char * (*GetIP)(); ///< Returns the (hopefully) real IP of the bot's computer. It gets it via userhost on IRC just like most IRC clients. This is a return from a STL string's .c_str() so make sure you copy it because it may not be valid long
	const char * platform; ///< The platform the bot is running on
#if defined(WIN32)
	HWND hWnd; ///< HWND of the bot's console (Windows-only of course)
#else
	void * unused;
#endif

	const char * (*GetBasePath)(); ///< Returns the folder the bot is running from. This is a return from a STL string's .c_str() so make sure you copy it because it may not be valid long
	char * (*GetBindIP)(char * buf, size_t bufSize); ///< Copies the global bind IP into buf (if set, will be empty string if not set). Return is buf if a bind IP is set or NULL otherwise
	int (*get_argc)(); ///< Returns the number of arguments that were passed on the bot's command line.
	const char * (*get_argv)(int i); ///< Returns argument i from the bot's command line. This is a return from a STL string's .c_str() so make sure you copy it because it may not be valid long
	int64 (*get_cycles)(); ///< This is similar to GetTickCount(), it increases by one every millisecond (depending on system timer). It's starting value has no meaning unlike GetTickCount, so don't try to infer meaning into it.
	void (*ib_printf2)(int pluginnum, const char * fmt, ...); ///< Prints to the bot console. All bot console output should be done through this function, not raw printfs or or other methods.
	void (*Rehash)(const char * fn); ///< Reloads ircbot.text, optionally a different file. Use NULL to reload the last used ircbot.text

	int (*NumPlugins)(); ///< Returns the number of loaded plugins.
	PLUGIN_PUBLIC * (*GetPlugin)(int32 plugnum); ///< Returns plugin number plugnum
	bool (*LoadPlugin)(const char * fn); ///< This will attempt to load a plugin in the first empty plugin slot.

	/**
	 * Loads a message defined in ircbot.text
	 * @param name The name of the message (ie. RadioOn, Song, etc.)
	 * @param msgbuf The buffer to load it in to
	 * @param bufSize The size of your buffer
	 * @return true if the message was loaded successfully, false otherwise
	 */
	bool (*LoadMessage)(const char * name, char * msgbuf, int32 bufSize);
	/**
	 * Does variable replacement in a string with IRCBot internal variables and those provided by plugins\n
	 * If your plugin wants to provide variables you can handle IB_PROCTEXT in your message handler
	 * @param text The string to process
	 * @param bufSize The size of your buffer that text is in
	 * @sa IBM_PROCTEXT
	 * @sa IB_PROCTEXT
	 */
	void (*ProcText)(char * text, int32 bufSize);
	void (*SetCustomVar)(const char * name, const char * val);
	void (*DeleteCustomVar)(const char * name);
	void (*BroadcastMsg)(USER_PRESENCE * ut, const char * buf); ///< Broadcasts a message to all channels the bot is on (respecting the DoSpam flag). Some plugins also respond to this such as Twitter.
	/**
	 * This function is how translation to other languages is done in IRCBot\n
	 * If you want a string to be able to be translated, you can use this function or the _() macro at the end of the file for more convenience\n
	 * This is a return from a STL string's .c_str() so make sure you copy it because it may not be valid long
	 * @param str The string to translate
	 */
	const char * (*GetLangString)(const char * str);

	void (*safe_sleep_seconds)(int32 sleepfor); ///< Sleeps for the specified number of seconds. This is a thread-safe sleep that won't interfere with other running threads.
	void (*safe_sleep_milli)(int32 sleepfor); ///< Sleeps for the specified number of milliseconds. This is a thread-safe sleep that won't interfere with other running threads.
	uint32 (*genrand_int32)(); ///< Generates a random number from the Mersenne Twister
	uint32(*genrand_range)(uint32 iMin, uint32 iMax); ///< Generates a random number from min to max (inclusive), untested with negative numbers

	void (*SendRemoteReply)(T_SOCKET * socket, REMOTE_HEADER * rHead, const char * data); ///< Sends a reply/command to a remote connection
	int (*SendMessage)(int toplug, uint32 MsgID, char * data, int32 datalen); ///< Sends a message to a plugin or all plugins.

	API_irc			* irc; ///< @sa API_irc
	API_ial			* ial; ///< @sa API_ial
	API_textfunc	* textfunc; ///< @sa API_textfunc
	API_commands	* commands; ///< @sa API_commands
	API_user		* user; ///< @sa API_user
	API_config		* config; ///< @sa API_config
	API_db			* db; ///< @sa API_db
	API_curl		* curl; ///< @sa API_curl
	API_memory		* mem; ///< @sa API_memory

#if !defined(IRCBOT_LITE)
	API_yp			* yp; ///< @sa API_yp
	API_ss			* ss; /// < @sa API_ss
#else
	void * unused_lite[2];
#endif
};

/**
 * \ingroup irc
 * Convenience define for SendIRC_Priority so you don't have to put in a priority every time
 */
#define SendIRC(x,y,z) SendIRC_Priority(x,y,z,PRIORITY_DEFAULT)
/**
 * \ingroup irc
 * Convenience define for SendSock_Priority so you don't have to put in a priority every time
 */
#define SendSock(x,y,z) SendSock_Priority(x,y,z,PRIORITY_DEFAULT)

#if !defined(__INCLUDE_IRCBOT_MAIN_H__)
	#define BOTAPI_DEF BOTAPI
	#define API_SS SOUND_SERVER
	#if defined(_ADJ_PLUGIN_H_) && !defined(_AUTODJ_)
		#define _(x) adapi->botapi->GetLangString(x)
		#define ib_printf(x, ...) ib_printf2(adapi->GetPluginNum(), x, ##__VA_ARGS__)
	#elif defined(_SDJ_PLUGIN_H_) && !defined(_SIMPLEDJ_)
		#define _(x) api->botapi->GetLangString(x)
		#define ib_printf(x, ...) ib_printf2(api->GetPluginNum(), x, ##__VA_ARGS__)
	#else
		#define _(x) api->GetLangString(x)
		#define ib_printf(x, ...) ib_printf2(pluginnum, x, ##__VA_ARGS__)
	#endif

	/*
	#define zmalloc(x) api->mem->alloc(x)
	#define zrealloc(x,y) api->mem->realloc(x,y)
	#define zfree(x) api->mem->free(x)
	#define zstrdup(x) api->mem->strdup(x)
	#define znew(x) (x *)api->mem->alloc(sizeof(x))
	#define zfreenn(ptr) if (ptr) { api->mem->free(ptr); }
	#define zmprintf api->mem->mprintf
	*/
#endif

#endif // SWIG

#endif // __INCLUDE_IRCBOT_PLUGINS_H__
