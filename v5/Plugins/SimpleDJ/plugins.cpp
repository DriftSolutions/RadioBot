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

int GetOutputChannels(int num) { return sd_config.Servers[num].Channels; }
int GetOutputSample(int num) { return sd_config.Servers[num].Sample; }
int GetOutputBitrate(int num) { return sd_config.Servers[num].Bitrate; }

SimpleDJ_Feeder * GetFeeder(int num) { return sd_config.Servers[num].Feeder; }

void RegisterDecoder(DECODER * dec) {
	for (int i=0; i < MAX_DECODERS; i++) {
		if (sd_config.Decoders[i] == NULL) {
			sd_config.Decoders[i] = dec;
			api->ib_printf("SimpleDJ -> Registered decoder in position %d\n",i);
			break;
		}
	}
}

Titus_Mutex * getQueueMutex() { return QueueMutex; }

char * getContentDir() { return sd_config.Server.ContentDir; }
char * getNoPlayFilters() { return sd_config.Options.NoPlayFilters; }
TIMER * getActiveFilter() { return sd_config.Filter; }
void removeActiveFilter() {
	QueueMutex->Lock();
	zfree(sd_config.Filter);
	sd_config.Filter = NULL;
	QueueMutex->Release();
}
uint32 getNoRepeat() { return sd_config.Options.NoRepeat; }
void setNoRepeat(uint32 nr) {
	sd_config.Options.NoRepeat = nr;
}

//QUEUE * AllocQueue() { return znew(QUEUE); }
void * xmalloc(size_t sz) {
	return zmalloc(sz);
}
char * xstrdup(const char * str) {
	return zstrdup((char *)str);
}
void xfree(void * ptr) {
	zfree(ptr);
}

int GetPluginNum() { return pluginnum; }

SD_PLUGIN_API plug_api = {
	NULL,
	GetPluginNum,
	QueueTitleUpdate,
	ShouldIStop,
	GetMixer,
	RegisterDecoder,

	Timing_Reset,
	Timing_Add,
	Timing_Done,

	GetOutputChannels,
	GetOutputSample,
	GetOutputBitrate,

	GetFeeder,
	getInstance,

	MatchFilter,
	MatchesNoPlayFilter,
	ProcText,
	PrettyTime,
	&sd_config.FileLister,
	GetFileDecoder,
	getContentDir,
	getNoPlayFilters,
	getActiveFilter,
	removeActiveFilter,
	getNoRepeat,
	setNoRepeat,
	CopyQueue,
	xmalloc,
	xstrdup,
	xfree,
	FreeQueue,
	getQueueMutex
};

SD_PLUGIN_API * GetAPI() { return &plug_api; }

typedef SD_PLUGIN * (*GetPluginType)();

bool is_plugin_ok_for_mode(SD_PLUGIN * p) {
	return true;
}

bool LoadPlugin(char * fn) {
	GetPluginType GetPtr;

	api->ib_printf("SimpleDJ -> Attempting to load plugin %s: ", fn);
	DL_HANDLE hHandle = DL_Open(fn);
	SD_PLUGIN * plug = NULL;
	if (hHandle != NULL) {
		GetPtr = (GetPluginType)DL_GetAddress(hHandle,"GetSDPlugin");
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle,"_GetSDPlugin");
		}
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle,"_Z11GetSDPlugin");
		}
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle,"_Z11GetSDPluginv");
		}
		if (GetPtr != NULL) {
			plug = GetPtr();
			if (plug && plug->version == SD_PLUGIN_VERSION) {
				if (is_plugin_ok_for_mode(plug)) {
					if (plug->init == NULL) {
						api->ib_printf("OK\nSimpleDJ -> Loaded plugin: %s\n", plug->plugin_desc);
					} else {
						api->ib_printf("OK\n");
					}
					if (plug->init == NULL || plug->init(GetAPI())) {
						sd_config.Plugins[sd_config.noplugins].hHandle = hHandle;
						sd_config.Plugins[sd_config.noplugins].plug = plug;
						sd_config.Plugins[sd_config.noplugins].plug->hModule = hHandle;
						if (plug->init) {
							api->ib_printf("SimpleDJ -> Loaded plugin: %s\n", plug->plugin_desc);
						}
						sd_config.noplugins++;
						return true;
					} else {
						api->ib_printf("SimpleDJ -> Plugin Load Aborted, plugin init() returned false!\n");
					}
				} else {
					api->ib_printf("This plugin is not compatible with the current SimpleDJ mode...\n");
				}
			} else {
				api->ib_printf("This plugin is not compatible with this version of SimpleDJ...\n");
			}
		} else {
			api->ib_printf("Failed in GetProcAddress(): %s\n", DL_LastError());
		}
		DL_Close(hHandle);
	} else {
		api->ib_printf("Failed in LoadLibrary(): %s\n", DL_LastError());
	}
	DRIFT_DIGITAL_SIGNATURE();
	return false;
}

void LoadPlugins(char * path, bool quiet) {

	if (!quiet) {
		api->ib_printf("SimpleDJ -> Loading plugins from: %s\n",path);
	}
	char buf[MAX_PATH],buf2[MAX_PATH];
	bool is_dir = false;

	Directory dir(path);
	while (dir.Read(buf, sizeof(buf), &is_dir)) {
		if (!is_dir) {
#ifdef WIN32
			if (!strncmp(buf,"sdj_",4) && strstr(buf,".dll")) {
#else
			if (!strncmp(buf,"sdj_",4) && strstr(buf,".so")) {
#endif
				sprintf(buf2,"%s%s", path, buf);
				LoadPlugin(buf2);
			}
		} else {
			if (buf[0] != '.') {
				sprintf(buf2,"%s%s%c", path, buf, PATH_SEP);
				LoadPlugins(buf2, true);
			}
		}
	}

	DRIFT_DIGITAL_SIGNATURE();
}

void UnloadPlugin(int num) {
	if (sd_config.Plugins[num].hHandle == NULL) { return; }

	SD_PLUGIN * blah = sd_config.Plugins[num].plug;

	if (blah && blah->quit) {
		blah->quit();
	}

	DL_Close(sd_config.Plugins[num].hHandle);
	sd_config.Plugins[num].hHandle = NULL;
	sd_config.Plugins[num].plug = NULL;
}

void UnloadPlugins() {
	int x = sd_config.noplugins;
	for (int i = 0; i < x; i++) {
		sd_config.noplugins--;
		UnloadPlugin(sd_config.noplugins);
	}
}

typedef SD_QUEUE_PLUGIN * (*GetQueuePluginType)();

bool LoadQueuePlugin(char * fn) {
	GetQueuePluginType GetPtr;

	char buf[1024];
	sstrcpy(buf, fn);
	str_replaceA(buf, sizeof(buf), "plugins/sdjq_", "plugins/SimpleDJ/sdjq_");
	str_replaceA(buf, sizeof(buf), "plugins\\sdjq_", "plugins\\SimpleDJ\\sdjq_");

	api->ib_printf("SimpleDJ -> Attempting to load plugin %s: ",buf);
	DL_HANDLE hHandle = DL_Open(buf);
	if (hHandle != NULL) {
		GetPtr = (GetQueuePluginType)DL_GetAddress(hHandle,"GetSDQPlugin");
		if (GetPtr != NULL) {
			//sd_config.Plugins[sd_config.noplugins].hHandle = hHandle;
			sd_config.Queue = GetPtr();
			if (sd_config.Queue->version != SD_PLUGIN_VERSION) {
				api->ib_printf("Failed!\nSimpleDJ -> This plugin is not compatible with this version of SimpleDJ...\n");
				DL_Close(hHandle);
				sd_config.Queue = NULL;
				return false;
			}
			sd_config.Queue->hModule = hHandle;
			if (sd_config.Queue->init != NULL) {
				if (!sd_config.Queue->init(GetAPI())) {
					api->ib_printf("ERROR\nSimpleDJ -> Plugin init() returned false!\n");
					DL_Close(hHandle);
					sd_config.Queue = NULL;
					return false;
				}
			}
			api->ib_printf("OK\nSimpleDJ -> Loaded plugin: %s\n",sd_config.Queue->plugin_desc);
			//sd_config.noplugins++;
			return true;
		} else {
			api->ib_printf("Failed in DL_GetAddress(): %s\n", DL_LastError());
			//load failed
			DL_Close(hHandle);
			return false;
		}
	} else {
		api->ib_printf("Failed in DL_Open(%): %s\n",DL_LastError());
		return false;
	}
}

void UnloadQueuePlugin() {
	if (sd_config.Queue) {
		if (sd_config.Queue->hModule) {
			DL_Close(sd_config.Queue->hModule);
		}
		sd_config.Queue = NULL;
	}
}
