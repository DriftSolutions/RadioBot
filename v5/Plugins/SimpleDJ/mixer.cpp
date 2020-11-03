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

#include "autodj.h"
#include <math.h>

bool encode_raw(unsigned char * data, int32 len);

MIXER Mixer = {
	encode_raw
};

MIXER * GetMixer() { return &Mixer; }

bool encode_raw(unsigned char * data, int32 len) {

	bool ret = true;
	for (int i=0; ret && i < MAX_SOUND_SERVERS; i++) {
		if (sd_config.Servers[i].Enabled && sd_config.Servers[i].Feeder) {
			if (!sd_config.Servers[i].Feeder->Send(data, len)) {
				ret = false;
			}
		}
	}

	return ret;
}
