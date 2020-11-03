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

#include "tts_priv.h"
#if !defined(WIN32)

bool MakeMP3FromFile_eSpeak(TTS_JOB * job) {
	return Generic_MakeMP3FromFile(job);
}

bool MakeWAVFromFile_eSpeak(TTS_JOB * job) {
	char msgbuf[8192];
	sprintf(msgbuf, "%s -v %s -w %s -f %s", tts_config.eSpeakCommand, job->VoiceOverride, job->outputFN, job->inputFN);
	system(msgbuf);
	return true;
}

TTS_ENGINE tts_engine_espeak = {
	VE_ESPEAK,
	MakeMP3FromFile_eSpeak,
	MakeWAVFromFile_eSpeak
};

#endif
