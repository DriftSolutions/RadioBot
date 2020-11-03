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

int GetOutputChannels() { return ad_config.Server.Channels; }
int GetOutputSample() { return ad_config.Server.Sample; }
int GetOutputBitrate() { return ad_config.Server.Bitrate; }

AutoDJ_Encoder * GetEncoder() { return ad_config.Encoder; }
//void SetEncoder(ENCODER * Encoder) { ad_config.Encoder = Encoder; }
AutoDJ_Feeder * GetFeeder() { return ad_config.Feeder; }

typedef std::multimap<int, ENCODER *> encoderMap;
void SortEncoders() {
	encoderMap d;
	int i;
	for (i=0; i < MAX_DECODERS; i++) {
		if (ad_config.Encoders[i] != NULL) {
			d.insert(pair<int, ENCODER *>(ad_config.Encoders[i]->priority,ad_config.Encoders[i]));
		}
	}
	i=0;
	for (encoderMap::const_iterator x = d.begin(); x != d.end(); x++) {
		ad_config.Encoders[i] = x->second;
		i++;
	}
}

void RegisterEncoder(ENCODER * enc) {
	for (int i=0; i < MAX_ENCODERS; i++) {
		if (ad_config.Encoders[i] == NULL) {
			ad_config.Encoders[i] = enc;
			SortEncoders();
			//api->ib_printf2(pluginnum,_("AutoDJ -> Registered encoder '%s' in position %d\n"),enc->name,i);
			break;
		}
	}
}

typedef std::multimap<int, DECODER *> decoderMap;
void SortDecoders() {
	decoderMap d;
	int i;
	for (i=0; i < MAX_DECODERS; i++) {
		if (ad_config.Decoders[i] != NULL) {
			d.insert(pair<int, DECODER *>(ad_config.Decoders[i]->priority,ad_config.Decoders[i]));
		}
	}
	i=0;
	for (decoderMap::const_iterator x = d.begin(); x != d.end(); x++) {
		ad_config.Decoders[i] = x->second;
		i++;
	}
}

void RegisterDecoder(DECODER * dec) {
	for (int i=0; i < MAX_DECODERS; i++) {
		if (ad_config.Decoders[i] == NULL) {
			ad_config.Decoders[i] = dec;
			SortDecoders();
			//api->ib_printf2(pluginnum,_("AutoDJ -> Registered decoder in position %d\n"),i);
			break;
		}
	}
}

Titus_Mutex * getQueueMutex() { return &QueueMutex; }

int32 getNumPlaylists() { return ad_config.num_playlists; }
//char * getContentDir(int playlist) { return ad_config.Playlists[playlist].ContentDir; }
int getID3Mode() { return ad_config.Options.ID3_Mode; }
char * getNoPlayFilters() { return ad_config.Options.NoPlayFilters; }
TIMER * getActiveFilter() {
	AutoMutex(QueueMutex);
	return ad_config.Filter;
}
void removeActiveFilter() {
	AutoMutex(QueueMutex);
	if (ad_config.Filter != NULL) {
		zfree(ad_config.Filter);
		ad_config.Filter = NULL;
	}
}
uint32 getNoRepeat() { return ad_config.Options.NoRepeat; }
void setNoRepeat(uint32 nr) {
	ad_config.Options.NoRepeat = nr;
}
uint32 getNoRepeatArtist() { return ad_config.Options.NoRepeatArtist; }
void setNoRepeatArtist(uint32 nr) {
	ad_config.Options.NoRepeatArtist = nr;
}
uint32 getMaxSongDuration() { return ad_config.Options.MaxSongDuration; }
void setMaxSongDuration(uint32 nr) {
	ad_config.Options.MaxSongDuration = nr;
}
bool getOnlyScanNewFiles() { return ad_config.Options.OnlyScanNewFiles; }

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

Deck * GetDeck(AUTODJ_DECKS deck) {
	return ad_config.Decks[deck];
}

int GetPluginNum() { return pluginnum; }

AD_PLUGIN_API plug_api = {
	NULL,
	GetPluginNum,
	QueueTitleUpdate,
	RegisterEncoder,
	RegisterDecoder,

	Timing_Reset,
	Timing_Add,
	Timing_Done,

	GetOutputChannels,
	GetOutputSample,
	GetOutputBitrate,

	GetFeeder,
	//AllocResampler,
	//FreeResampler,
	GetDeck,
	GetDeckName,

	getInstance,

	FileID,
	DoFileScan,
	getOnlyScanNewFiles,

	AllowSongPlayback,
	AddToPlayQueue,

	MatchFilter,
	MatchesNoPlayFilter,
	SchedulerProcText,
	PrettyTime,
	&ad_config.FileLister,
	GetFileDecoder,
//	getContentDir,
	ReadMetaDataEx,
	getNumPlaylists,
	getID3Mode,
	getNoPlayFilters,
	getActiveFilter,
	removeActiveFilter,
	getNoRepeat,
	setNoRepeat,
	getNoRepeatArtist,
	setNoRepeatArtist,
	getMaxSongDuration,
	setMaxSongDuration,
	AllocQueue,
	CopyQueue,
	FreeQueue,
	xmalloc,
	xstrdup,
	xfree,
	AllocAudioBuffer,
	FreeAudioBuffer,
	getQueueMutex
};

AD_PLUGIN_API * GetAPI() { return &plug_api; }

typedef AD_PLUGIN * (*GetPluginType)();

bool is_plugin_ok_for_mode(AD_PLUGIN * p) {
	return true;
}

bool LoadPlugin(const char * fn) {
	GetPluginType GetPtr;

	api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),fn);
	DL_HANDLE hHandle = DL_Open(fn);
	AD_PLUGIN * plug = NULL;
	if (hHandle != NULL) {
		GetPtr = (GetPluginType)DL_GetAddress(hHandle,"GetADPlugin");
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle,"_GetADPlugin");
		}
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle,"_Z11GetADPlugin");
		}
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle,"_Z11GetADPluginv");
		}
		if (GetPtr != NULL) {
			plug = GetPtr();
			if (plug && plug->version == AD_PLUGIN_VERSION) {
				if (is_plugin_ok_for_mode(plug)) {
					if (plug->init == NULL || plug->init(GetAPI())) {
						ad_config.Plugins[ad_config.noplugins].hHandle = hHandle;
						ad_config.Plugins[ad_config.noplugins].plug = plug;
						ad_config.Plugins[ad_config.noplugins].plug->hModule = hHandle;
						if (plug->init) {
							api->ib_printf2(pluginnum,_("AutoDJ -> Loaded plugin: %s\n"), plug->plugin_desc);
						}
						ad_config.noplugins++;
						return true;
					} else {
						api->ib_printf2(pluginnum,_("AutoDJ -> Plugin Load Aborted, plugin init() returned false!\n"));
					}
				} else {
					api->ib_printf2(pluginnum,_("AutoDJ -> This plugin is not compatible with the current AutoDJ mode...\n"));
				}
			} else {
				api->ib_printf2(pluginnum,_("AutoDJ -> This plugin is not compatible with this version of AutoDJ...\n"));
			}
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ -> Failed in GetProcAddress(): %s\n"), DL_LastError());
		}
		DL_Close(hHandle);
	} else {
		api->ib_printf2(pluginnum,_("AutoDJ -> Failed in LoadLibrary(): %s\n"), DL_LastError());
	}
	DRIFT_DIGITAL_SIGNATURE();
	return false;
}

void LoadPlugins(const char * path, bool is_subdir) {

	if (!is_subdir) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Loading plugins from: %s\n"),path);
	}
	char buf[MAX_PATH],buf2[MAX_PATH];
	bool is_dir = false;

	Directory dir(path);
	while (dir.Read(buf, sizeof(buf), &is_dir)) {
		if (!is_dir) {
#ifdef WIN32
			if (!strncmp(buf,"adj_",4) && strstr(buf,".dll")) {
#else
			if (!strncmp(buf,"adj_",4) && strstr(buf,".so")) {
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
	if (ad_config.Plugins[num].hHandle == NULL) { return; }

	AD_PLUGIN * blah = ad_config.Plugins[num].plug;

	if (blah && blah->quit) {
		blah->quit();
	}

	DL_Close(ad_config.Plugins[num].hHandle);
	ad_config.Plugins[num].hHandle = NULL;
	ad_config.Plugins[num].plug = NULL;
}

void UnloadPlugins() {
	int x = ad_config.noplugins;
	for (int i = 0; i < x; i++) {
		ad_config.noplugins--;
		UnloadPlugin(ad_config.noplugins);
	}
}

typedef AD_QUEUE_PLUGIN * (*GetQueuePluginType)();

bool LoadQueuePlugin(const char * fn) {
	GetQueuePluginType GetPtr;

	char buf[1024];
	sstrcpy(buf, fn);
	str_replaceA(buf, sizeof(buf), "plugins/adjq_", "plugins/AutoDJ/adjq_");
	str_replaceA(buf, sizeof(buf), "plugins\\adjq_", "plugins\\AutoDJ\\adjq_");

	//api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),buf);
	DL_HANDLE hHandle = DL_Open(buf);
	if (hHandle != NULL) {
		GetPtr = (GetQueuePluginType)DL_GetAddress(hHandle,"GetADQPlugin");
		if (GetPtr != NULL) {
			//ad_config.Plugins[ad_config.noplugins].hHandle = hHandle;
			ad_config.Queue = GetPtr();
			if (ad_config.Queue->version != AD_PLUGIN_VERSION) {
				api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),buf);
				api->ib_printf2(pluginnum,_("AutoDJ -> This plugin is not compatible with this version of AutoDJ...\n"));
				DL_Close(hHandle);
				ad_config.Queue = NULL;
				return false;
			}
			ad_config.Queue->hModule = hHandle;
			if (ad_config.Queue->init != NULL) {
				if (!ad_config.Queue->init(GetAPI())) {
					api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),buf);
					api->ib_printf2(pluginnum,_("AutoDJ -> Plugin init() returned false!\n"));
					DL_Close(hHandle);
					ad_config.Queue = NULL;
					return false;
				}
			}
			//api->ib_printf2(pluginnum,_("AutoDJ -> Loaded plugin: %s\n"),ad_config.Queue->plugin_desc);
			//ad_config.noplugins++;
			return true;
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),buf);
			api->ib_printf2(pluginnum,_("AutoDJ -> Failed in DL_GetAddress(): %s\n"), DL_LastError());
			//load failed
			DL_Close(hHandle);
			return false;
		}
	} else {
		api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),buf);
		api->ib_printf2(pluginnum,_("AutoDJ -> Failed in DL_Open(): %s\n"),DL_LastError());
		return false;
	}
}

void UnloadQueuePlugin() {
	if (ad_config.Queue) {
		if (ad_config.Queue->hModule) {
			DL_Close(ad_config.Queue->hModule);
		}
		ad_config.Queue = NULL;
	}
}

typedef AD_ROTATION_PLUGIN * (*GetRotationPluginType)();

bool LoadRotationPlugin(const char * fn) {
	GetRotationPluginType GetPtr;

	api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),fn);
	DL_HANDLE hHandle = DL_Open(fn);
	if (hHandle != NULL) {
		GetPtr = (GetRotationPluginType)DL_GetAddress(hHandle,"GetADRPlugin");
		if (GetPtr != NULL) {
			//ad_config.Plugins[ad_config.noplugins].hHandle = hHandle;
			ad_config.Rotation = GetPtr();
			if (ad_config.Rotation->version != AD_PLUGIN_VERSION) {
				api->ib_printf2(pluginnum,_("AutoDJ -> This plugin is not compatible with this version of AutoDJ...\n"));
				DL_Close(hHandle);
				ad_config.Rotation = NULL;
				return false;
			}
			ad_config.Rotation->hModule = hHandle;
			if (ad_config.Rotation->init != NULL) {
				if (!ad_config.Rotation->init(GetAPI())) {
					api->ib_printf2(pluginnum,_("AutoDJ -> Plugin init() returned false!\n"));
					DL_Close(hHandle);
					ad_config.Rotation = NULL;
					return false;
				}
			}
			api->ib_printf2(pluginnum,_("AutoDJ -> Loaded plugin: %s\n"),ad_config.Rotation->plugin_desc);
			//ad_config.noplugins++;
			return true;
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ -> Failed in DL_GetAddress(): %s\n"), DL_LastError());
			//load failed
			DL_Close(hHandle);
			return false;
		}
	} else {
		api->ib_printf2(pluginnum,_("AutoDJ -> Failed in DL_Open(%): %s\n"),DL_LastError());
		return false;
	}
}

void UnloadRotationPlugin() {
	if (ad_config.Rotation) {
		if (ad_config.Rotation->hModule) {
			DL_Close(ad_config.Rotation->hModule);
		}
		ad_config.Rotation = NULL;
	}
}

typedef AD_RESAMPLER_PLUGIN * (*GetResamplerPluginType)();

bool LoadResamplerPlugin(const char * fn) {
	GetResamplerPluginType GetPtr;

	api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s ...\n"),fn);
	DL_HANDLE hHandle = DL_Open(fn);
	if (hHandle != NULL) {
		GetPtr = (GetResamplerPluginType)DL_GetAddress(hHandle,"GetADResampler");
		if (GetPtr != NULL) {
			ad_config.Resampler = GetPtr();
			if (ad_config.Resampler->version != AD_PLUGIN_VERSION) {
				api->ib_printf2(pluginnum,_("AutoDJ -> This plugin is not compatible with this version of AutoDJ...\n"));
				DL_Close(hHandle);
				ad_config.Resampler = NULL;
				return false;
			}
			ad_config.Resampler->hModule = hHandle;
			if (ad_config.Resampler->init != NULL) {
				if (!ad_config.Resampler->init(GetAPI())) {
					api->ib_printf2(pluginnum,_("AutoDJ -> Plugin init() returned false!\n"));
					DL_Close(hHandle);
					ad_config.Resampler = NULL;
					return false;
				}
			}
			api->ib_printf2(pluginnum,_("AutoDJ -> Loaded plugin: %s\n"),ad_config.Resampler->plugin_desc);
			//ad_config.noplugins++;
			return true;
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ -> Failed in DL_GetAddress(): %s\n"), DL_LastError());
			//load failed
			DL_Close(hHandle);
			return false;
		}
	} else {
		api->ib_printf2(pluginnum,_("AutoDJ -> Failed in DL_Open(%): %s\n"),DL_LastError());
		return false;
	}
}

void UnloadResamplerPlugin() {
	if (ad_config.Resampler) {
		if (ad_config.Resampler->hModule) {
			DL_Close(ad_config.Resampler->hModule);
		}
		ad_config.Resampler = NULL;
	}
}
