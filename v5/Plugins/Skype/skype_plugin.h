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

//#define DEBUG_MUTEX
#include "../../src/plugins.h"
#include "../TTS_Services/tts.h"
#include "../../../Common/libSkypeAPI/skype.h"

#if defined(_WIN32)
//typedef int ssize_t;
#define ssize_t SSIZE_T
#endif
#if defined(WIN32)
//	#define LINK_MPG123_DLL
	#if defined(DEBUG)
	#pragma comment(lib, "libmpg123_d.lib")
	#else
	#pragma comment(lib, "libmpg123.lib")
	#endif
#endif
#include <mpg123.h>

extern BOTAPI_DEF * api;
extern int pluginnum;
extern Titus_Mutex hMutex;
extern Titus_Sockets socks;

enum SKYPE_ANSWER_MODES {
	SAM_DO_NOTHING,
	SAM_AUTO_ANSWER,
	SAM_TRANSFER,
	SAM_ANSWERING_SYSTEM
};

struct SKYPE_CONFIG {
	bool shutdown_now;
	bool connected;
	bool reconnect;
	time_t lastRecv;

	TTS_INTERFACE tts;

	SKYPE_ANSWER_MODES AnswerMode;
	//int MinLevelAnswer, MinLevelMessage;

	bool RequirePrefix;
	char TransferCallsTo[128];
	char MoodTextString[1024];
};

extern SKYPE_CONFIG skype_config;

struct SKYPE_CALL {
	int32 id;
	bool active, onHold, isIncoming;

	char nick[128];
	char hostmask[512];
	uint32  uflags;

	char keyPress;
	char TextToSpeak[4096];
	char LastMessage[4096];

	int32 inPort, outPort;
	T_SOCKET *inSock, *outSock;
};

void SetCallDTMF(SKYPE_CALL * call, char button);

SKYPE_CALL * CreateCallHandle(int32 id);
bool AcceptCall(SKYPE_CALL * call);
SKYPE_CALL * GetCallHandle(int32 id);
void CloseCallHandle(SKYPE_CALL * call);
void CloseAllCalls();
void HangUpCall(SKYPE_CALL * call);
void HangUpAllCalls();
size_t CallCount();
