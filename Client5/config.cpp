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

#include "client.h"
#include "../Client3/crypt.h"
#include "client5_savefile.pb.h"

void CryptData(uint8 * ptr, uint32 len) {
	int ind = 0;

	while (len > 4) {
		*(uint32 *)ptr ^= CD_32[ind];
		ptr += 4;
		len -= 4;
		ind++;
		if (CD_32[ind] == 0) {
			ind = 0;
		}
	}

	ind = 0;
	while (len > 2) {
		*(uint16 *)ptr ^= CD_16[ind];
		ptr += 2;
		len -= 2;
		ind++;
		if (CD_16[ind] == 0) {
			ind = 0;
		}
	}

	ind = 0;
	while (len) {
		*(uint8 *)ptr ^= CD_8[ind];
		ptr++;
		len--;
		ind++;
		if (CD_8[ind] == 0) {
			ind = 0;
		}
	}

}

bool LoadOldConfig() {
	char fn[MAX_PATH];
	if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, fn) == S_OK) {
		if (fn[strlen(fn)-1] != PATH_SEP) {
			strcat(fn, PATH_SEPS);
		}
	} else {
		fn[0] = 0;
	}
	strcat(fn, "ircbot_client3.dat");

	FILE * fp = fopen(fn, "rb");
	if (fp == NULL) { fp = fopen("client3.dat", "rb"); }
	if (!fp) { return false; }

	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (len == sizeof(CONFIG_OLD4)) {
		CONFIG_OLD4 cfg;
		fread(&cfg, sizeof(cfg), 1, fp);
		fclose(fp);
		CryptData((uint8 *)&cfg, sizeof(cfg));
		if (cfg.revision != 0x05) {
			MessageBox(config.mWnd, TEXT("Unknown config file revision!"), TEXT("Error"), MB_ICONERROR);
			return false;
		}

		for (int i=0; i < 32; i++) {
			strcpy(config.hosts[i].pass, cfg.hosts[i].pass);
			strcpy(config.hosts[i].username, cfg.hosts[i].username);
			strcpy(config.hosts[i].host, cfg.hosts[i].host);
			strcpy(config.hosts[i].name, cfg.hosts[i].name);
			config.hosts[i].port = cfg.hosts[i].port;
			config.hosts[i].mode = cfg.hosts[i].mode;
		}

		config.host_index = cfg.index;
		config.keep_on_top = cfg.keep_on_top;
		config.beep_on_req = cfg.beep_on_req;
		return true;
	}

	if (len == sizeof(CONFIG_OLD3)) {
		CONFIG_OLD3 cfg;
		fread(&cfg, sizeof(cfg), 1, fp);
		fclose(fp);
		strcpy(config.hosts[0].name, cfg.host);
		strcpy(config.hosts[0].host, cfg.host);
		strcpy(config.hosts[0].pass, cfg.pass);
		strcpy(config.hosts[0].username, cfg.username);
		config.hosts[0].port = cfg.port;
		config.beep_on_req = true;
		return true;
	}

	if (len <= sizeof(CONFIG_OLD2)) {
		CONFIG_OLD2 cfg;
		fread(&cfg, 1, sizeof(cfg), fp);
		fclose(fp);
		for (int i=0; i < 16; i++) {
			strcpy(config.hosts[i].name, cfg.hosts[i].host);
			strcpy(config.hosts[i].pass, cfg.pass);
			strcpy(config.hosts[i].username, cfg.username);
			strcpy(config.hosts[i].host, cfg.hosts[i].host);
			config.hosts[i].port = cfg.hosts[i].port;
		}
		config.host_index = cfg.index;
		config.beep_on_req = true;
		return true;
	}

	if (len == sizeof(CONFIG_OLD)) {
		CONFIG_OLD cfg;
		fread(&cfg, sizeof(cfg), 1, fp);
		fclose(fp);
		CryptData((uint8 *)&cfg, sizeof(CONFIG_OLD));
		if (cfg.revision != 0x04) {
			memset(&cfg, 0, sizeof(cfg));
			MessageBox(config.mWnd, TEXT("Unknown config file revision!"), TEXT("Error"), MB_ICONERROR);
			return false;
		}

		for (int i=0; i < 32; i++) {
			strcpy(config.hosts[i].pass, cfg.hosts[i].pass);
			strcpy(config.hosts[i].username, cfg.hosts[i].username);
			strcpy(config.hosts[i].host, cfg.hosts[i].host);
			strcpy(config.hosts[i].name, cfg.hosts[i].name);
			config.hosts[i].port = cfg.hosts[i].port;
			config.hosts[i].mode = cfg.hosts[i].mode;
		}

		config.host_index = cfg.index;
		config.keep_on_top = false;
		config.beep_on_req = true;
		return true;
	}

	fclose(fp);
	MessageBox(config.mWnd, TEXT("Unknown config file format!"), TEXT("Error"), MB_ICONERROR);
	return false;
}

bool LoadConfig() {
	sstrcpy(config.sam.mysql_host, "localhost");
	sstrcpy(config.sam.mysql_user, "root");
	sstrcpy(config.sam.mysql_db, "samdb");
	config.sam.mysql_port = 3306;

	bool ret = false;
	char * fn = GetUserConfigFile("ShoutIRC", "client5.cfg");
	FILE * fp = fopen(fn, "rb");
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		char * buf = (char *)zmalloc(len);
		if (fread(buf, len, 1, fp) == 1) {
			ConfigFile c;
			if (c.ParseFromArray(buf, len) && c.IsInitialized()) {
				//config.find_method = c.find_method();
				config.host_index = c.host_index();
				config.beep_on_req = c.beep_on_req();
				config.keep_on_top = c.keep_on_top();
				config.connect_on_startup = c.connect_on_startup();
				if (c.has_sam()) {
					sstrcpy(config.sam.mysql_host, c.sam().host().c_str());
					sstrcpy(config.sam.mysql_user, c.sam().user().c_str());
					sstrcpy(config.sam.mysql_pass, c.sam().pass().c_str());
					sstrcpy(config.sam.mysql_db, c.sam().db().c_str());
					config.sam.mysql_port = c.sam().port();
				}
				if (c.has_musicdb()) {
					sstrcpy(config.music_db.folder, c.musicdb().folder().c_str());
				}
				for (int i=0; i < c.connections_size(); i++) {
					SaveHost h = c.connections(i);
					sstrcpy(config.hosts[i].name, h.name().c_str());
					sstrcpy(config.hosts[i].host, h.host().c_str());
					sstrcpy(config.hosts[i].username, h.user().c_str());
					sstrcpy(config.hosts[i].pass, h.pass().c_str());
					config.hosts[i].port = h.port();
					config.hosts[i].mode = h.mode();
				}
				ret = true;
			}
		} else {
			MessageBox(config.mWnd, TEXT("Error reading config file!"), TEXT("Error"), MB_ICONERROR);
		}
		zfree(buf);
		fclose(fp);
	}
	zfree(fn);

	if (ret) { return true; }
	return LoadOldConfig();
}

void SaveConfig() {
	char * fn = GetUserConfigFile("ShoutIRC", "client5.cfg");
	FILE * fp = fopen(fn, "wb");
	if (fp != NULL) {
		ConfigFile c;
//		c.set_find_method(config.find_method);
		c.set_host_index(config.host_index);
		c.set_beep_on_req(config.beep_on_req);
		c.set_keep_on_top(config.keep_on_top);
		c.set_connect_on_startup(config.connect_on_startup);

		SaveSAM * s = c.mutable_sam();
		s->set_host(config.sam.mysql_host);
		s->set_user(config.sam.mysql_user);
		s->set_pass(config.sam.mysql_pass);
		s->set_db(config.sam.mysql_db);
		s->set_port(config.sam.mysql_port);

		if (config.music_db.folder[0]) {
			SaveMusicDB * m = c.mutable_musicdb();
			m->set_folder(config.music_db.folder);
		}

		for (int i=0; i < MAX_CONNECTIONS; i++) {
			if (config.hosts[i].name[0]) {
				SaveHost * h = c.add_connections();
				h->set_name(config.hosts[i].name);
				h->set_host(config.hosts[i].host);
				h->set_user(config.hosts[i].username);
				h->set_pass(config.hosts[i].pass);
				h->set_port(config.hosts[i].port);
				h->set_mode(config.hosts[i].mode);
			}
		}
		string str = c.SerializeAsString();
		if (fwrite(str.c_str(), str.length(), 1, fp) != 1) {
			MessageBox(NULL, TEXT("Error writing config file!"), TEXT("Error"), MB_ICONERROR);
		}
		fclose(fp);
	} else {
		MessageBox(NULL, TEXT("Error opening config file for write!"), TEXT("Error"), MB_ICONERROR);
	}
	zfree(fn);
}
