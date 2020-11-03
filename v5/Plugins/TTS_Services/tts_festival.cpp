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

bool MakeMP3FromFile_Festival(TTS_JOB * job) {
	return Generic_MakeMP3FromFile(job);
}

bool MakeWAVFromFile_Festival(TTS_JOB * job) {
	char msgbuf[8192];
	sprintf(msgbuf, "text2wave -scale 2 -o %s %s", job->outputFN, job->inputFN);
	system(msgbuf);
	return true;
}

TTS_ENGINE tts_engine_festival = {
	VE_FESTIVAL,
	MakeMP3FromFile_Festival,
	MakeWAVFromFile_Festival
};

#endif
