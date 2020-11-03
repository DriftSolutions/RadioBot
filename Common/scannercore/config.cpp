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

#include "scanner.h"

#define SUBKEY "Software\\ShoutIRC\\Music_Scanner"

void LoadConfig() {
	Universal_Config cfg;
	if (cfg.LoadConfig(config.config_fn)) {
		char buf[256];
		const char * p = NULL;

		p = cfg.GetValueString("Main", "OutFile");
		if (p) { strcpy(config.out_fn, p); }
		config.compressOutput = cfg.GetValueLong("Main", "Compress") > 0 ? true:false;
		config.useJSON = cfg.GetValueLong("Main", "useJSON") > 0 ? true:false;

		for (int i=0; i < NUM_FOLDERS; i++) {
			sprintf(buf, "MusicFolder%d", i);
			p = cfg.GetValueString("Main", buf);
			if (p && strlen(p)) {
				strcpy(config.music_folder[i], p);
				if (config.music_folder[i][strlen(config.music_folder[i])-1] != PATH_SEP) { strcat(config.music_folder[i], PATH_SEPS); }
			}
		}
	}
}

bool SaveConfig() {
	Universal_Config cfg;
	char buf[256];
	const char * p = NULL;

	cfg.SetValueString("Main", "OutFile", config.out_fn);
	cfg.SetValueLong("Main", "Compress", config.compressOutput ? 1:0);
	cfg.SetValueLong("Main", "useJSON", config.useJSON ? 1:0);

	for (int i=0; i < NUM_FOLDERS; i++) {
		if (strlen(config.music_folder[i])) {
			sprintf(buf, "MusicFolder%d", i);
			cfg.SetValueString("Main", buf, config.music_folder[i]);
		}
	}

	return cfg.WriteConfig(config.config_fn);
}
