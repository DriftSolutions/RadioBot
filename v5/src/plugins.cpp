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


#include "ircbot.h"
#include "../../Common/plugin_checksums.h"

typedef PLUGIN_PUBLIC * (*GetPluginType4)();
//typedef IRCBotPlugin * (*GetPluginType5)();

uint32 next_plugin_num=0;

bool LoadPlugin(const char * fn) {
	for (int i=0; i < MAX_PLUGINS; i++) {
		if (config.plugins[i].fn.length() == 0) {//found empty slot
			config.plugins[i].fn = fn;
			if (LoadPlugin(i)) {
				LoadReperm();
				return true;
			}
			return false;
		}
	}
	return false;
}

void LoadPlugins() {
	for (int i=0; i < MAX_PLUGINS; i++) {
		if (config.plugins[i].fn.length()) {
			LoadPlugin(i);
		}
	}
}

bool VerifyChecksum(const char * fn) {
	return true;
#if defined(DEBUG)
	return true;
#endif

	bool ret = false;
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		PLUGIN_CHECKSUM pc;
		fseek(fp, 0, SEEK_END);
		int32 len = ftell(fp) - sizeof(pc);
		fseek(fp, 0, SEEK_SET);
		uint8 * data = (uint8 *)zmalloc(len);
		if (fread(data, len, 1, fp) == 1 && fread(&pc, sizeof(pc), 1, fp) == 1) {
			if (pc.magic == PLUGIN_CHECKSUM_MAGIC) {
				switch (pc.type) {
					case PLUGIN_CHECKSUM_SHA_1:{
							char digest[64],digest2[64];
							hashdata("sha1", data, len, digest, sizeof(digest));
							bin2hex(pc.checksum, 20, digest2, sizeof(digest2));
							//if (!memcmp(digest, pc.checksum, 20)) {
							if (!stricmp(digest, digest2)) {
								ret = true;
							}
						}
						break;
					default:
						ret = false;
						break;
				}
			}
		}
		zfree(data);
		fclose(fp);
	}
	return ret;
}

bool LoadPlugin4(int num, DL_HANDLE hHandle) {
	GetPluginType4 GetPtr;
	PLUGIN * Scan = &config.plugins[num];

	GetPtr = (GetPluginType4)DL_GetAddress(hHandle,"GetIRCBotPlugin");
#ifndef WIN32
	if (GetPtr == NULL) {
		GetPtr = (GetPluginType4)DL_GetAddress(hHandle, "_GetIRCBotPlugin");
	}
	if (GetPtr == NULL) {
		GetPtr = (GetPluginType4)DL_GetAddress(hHandle, "_Z12GetIRCBotPluginv");
	}
	if (GetPtr == NULL) {
		GetPtr = (GetPluginType4)DL_GetAddress(hHandle, "GetIRCBotPlugin");
	}
#endif
	if (GetPtr != NULL) {
		Scan->hHandle = hHandle;
		Scan->plug = GetPtr();
		if (Scan->plug == NULL || Scan->plug->plugin_ver != IRCBOT_PLUGIN_VERSION) {
			ib_printf(_("%s: Error loading plugin: This plugin is not compatible with this version of %s!\n"), IRCBOT_NAME, IRCBOT_NAME);
			//ib_printf("%p / %02X\n", Scan->plug, Scan->plug ? Scan->plug->plugin_ver:0);
			DL_Close(hHandle);
			Scan->hHandle = NULL;
			Scan->plug = NULL;
			return false;
		}
		const char * p = strrchr(Scan->fn.c_str(),'\\');
		if (!p) { p = strrchr(Scan->fn.c_str(),'/'); }
		if (p) {
			Scan->name = (p+1);
		} else {
			Scan->name = Scan->fn;
		}

		Scan->plug->hInstance = hHandle;
		if (Scan->plug->init && !Scan->plug->init(num,GetAPI())) {
			ib_printf(_("%s: Error loading plugin: Plugin init() returned false!\n"), IRCBOT_NAME);
			DL_Close(hHandle);
			Scan->hHandle = NULL;
			Scan->plug = NULL;
			return false;
		}

		Scan->plug->private_struct = Scan;
		return true;
	} else {
		ib_printf(_("%s: Error loading plugin: Failed in DL_GetAddress()!\nError: %s\n"), IRCBOT_NAME, DL_LastError());
	}
	return false;
}

bool LoadPlugin(int num) {
	PLUGIN * Scan = &config.plugins[num];

	DRIFT_DIGITAL_SIGNATURE();
	ib_printf(_("%s: Loading plugin %s ...\n"), IRCBOT_NAME, Scan->fn.c_str());

	if (!VerifyChecksum(Scan->fn.c_str())) {
		ib_printf(_("%s: Error loading plugin: Could not verify plugin checksum!\n"), IRCBOT_NAME);
		return false;
	}

	char tmpfn[MAX_PATH];

#if !defined(DEBUG)
	sprintf(tmpfn, "./tmp/plugin%02d.so", num);

	FILE * fi = fopen(Scan->fn.c_str(), "rb");
	FILE * fo = fopen(tmpfn, "wb");
	if (fi == NULL || fo == NULL) {
		sstrcpy(tmpfn, Scan->fn.c_str());
	} else {
		//copy to temp file
		char * buf = new char[8192];
		size_t n = 0;
		while ((n = fread(buf, 1, 8192, fi)) > 0) {
			fwrite(buf, n, 1, fo);
		}
		delete [] buf;
	}
	if (fi) { fclose(fi); }
	if (fo) { fclose(fo); }
#else
	sstrcpy(tmpfn, Scan->fn.c_str());
#endif

	DL_HANDLE hHandle = DL_Open(tmpfn);
	if (hHandle != NULL) {
		if (LoadPlugin4(num, hHandle)) {// || LoadPlugin5(num, hHandle)
			return true;
		}
		DL_Close(hHandle);
		return false;
	} else {
		ib_printf(_("%s: Error loading plugin: Failed in DL_Open()!\nError: %s\n"), IRCBOT_NAME, DL_LastError());
		return false;
	}
}

void UnloadPlugins() {
	for (int i = 0; i < MAX_PLUGINS; i++) {
		UnloadPlugin(i);
	}
}

void UnloadPlugin(int num) {
	if (config.plugins[num].hHandle == 0) { return; }

#if defined(IRCBOT_ENABLE_SS)
	ExpireFindResults(false, num);
#endif
	if (config.plugins[num].plug && config.plugins[num].plug->quit) {
		config.plugins[num].plug->quit();
	}

	DL_Close(config.plugins[num].hHandle);

	config.plugins[num].plug = NULL;
	config.plugins[num].hHandle = NULL;
}
