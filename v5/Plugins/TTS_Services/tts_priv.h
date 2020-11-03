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

#include "../../src/plugins.h"
#include "tts.h"

extern int pluginnum;
extern BOTAPI_DEF * api;

struct TTS_ENGINE {
	VOICE_ENGINES Engine;
	bool (*MakeMP3FromFile)(TTS_JOB * job);
	bool (*MakeWAVFromFile)(TTS_JOB * job);
};

struct TTS_CONFIG {
	VOICE_ENGINES Engine; /// non-Win32 only
	TTS_ENGINE * pEngine;
	char eSpeakCommand[128];
	char Voice[128];
};

extern TTS_CONFIG tts_config;

bool Generic_MakeMP3FromFile(TTS_JOB * job);
bool Generic_MakeWAVFromFile(TTS_JOB * job);
