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

int pluginnum=0;
BOTAPI_DEF * api=NULL;
TTS_CONFIG tts_config;

#if defined(WIN32)
extern TTS_ENGINE tts_engine_win32;
#else
extern TTS_ENGINE tts_engine_espeak;
extern TTS_ENGINE tts_engine_festival;
#endif
extern TTS_ENGINE tts_engine_gtrans;
TTS_ENGINE * tts_engines[] = {
#if defined(WIN32)
	&tts_engine_win32,
#else
	&tts_engine_espeak,
	&tts_engine_festival,
#endif
	&tts_engine_gtrans,
	NULL
};

void LoadConfig() {

#if defined(WIN32)
	strcpy(tts_config.Voice, "");
	tts_config.Engine = VE_WIN32;
#else
	tts_config.Engine = VE_ESPEAK;
	strcpy(tts_config.Voice, "en/en");
	strcpy(tts_config.eSpeakCommand, "speak");
#endif

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "TTS");
	if (sec == NULL) {
		api->ib_printf(_("TTS Services -> No 'TTS' section found in your config, using defaults...\n"));
		return;
	}

	char * p = NULL;

	if ((p = api->config->GetConfigSectionValue(sec, "VoiceEngine"))) {
		if (!stricmp(p, "gtrans")) {
			tts_config.Engine = VE_GTRANS;
		}
#if !defined(WIN32)
		if (!stricmp(p, "festival")) {
			tts_config.Engine = VE_FESTIVAL;
		}
#endif
	}

	if ((p = api->config->GetConfigSectionValue(sec,"Voice"))) {
		sstrcpy(tts_config.Voice, p);
	}
#if !defined(WIN32)
	if ((p = api->config->GetConfigSectionValue(sec,"eSpeakCommand"))) {
		sstrcpy(tts_config.eSpeakCommand, p);
	}
#endif
}

int init(int num, BOTAPI_DEF * BotAPI) {
	memset(&tts_config,0,sizeof(tts_config));
	pluginnum = num;
	api = (BOTAPI_DEF *)BotAPI;

	memset(&tts_config, 0, sizeof(tts_config));
	LoadConfig();

	#if !defined(WIN32)
	signal(SIGSEGV,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	#endif
	return 1;
}

void quit() {
	api->ib_printf(_("TTS Services -> Beginning shutdown...\n"));

	//api->commands->UnregisterCommandByName( "relay" );

	time_t timeo = time(NULL) + 15;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf(_("TTS Services -> Waiting on threads to die (%d) (Will wait %d more seconds...)\n"),TT_NumThreads(),timeo - time(NULL));
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}

	api->ib_printf(_("TTS Services -> Plugin Shut Down!\n"));
}

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif
VOICE_OUTPUT_FORMATS GetPreferredOutputFormat(VOICE_ENGINES Engine) {
	if (Engine == VE_DEFAULT) {
		Engine = tts_config.Engine;
	}
	switch (Engine) {
		case VE_WIN32:
			return VOF_WAV;
			break;
		case VE_FESTIVAL:
			return VOF_WAV;
			break;
		case VE_ESPEAK:
			return VOF_WAV;
			break;
		case VE_GTRANS:
			return VOF_MP3;
			break;
	}
	return VOF_WAV;
}
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#pragma GCC diagnostic pop
#endif

bool MakeMP3FromFile(TTS_JOB * job) {
	VOICE_ENGINES Engine = tts_config.Engine;
	if (job->EngineOverride != VE_NO_OVERRIDE) {
		Engine = job->EngineOverride;
	}
	if (!strlen(job->VoiceOverride)) {
		strcpy(job->VoiceOverride, tts_config.Voice);
	}
	for (int i=0; tts_engines[i] != NULL; i++) {
		if (tts_engines[i]->Engine == Engine) {
			return tts_engines[i]->MakeMP3FromFile(job);
		}
	}
	return false;
}
bool MakeWAVFromFile(TTS_JOB * job) {
	VOICE_ENGINES Engine = tts_config.Engine;
	if (job->EngineOverride != VE_NO_OVERRIDE) {
		Engine = job->EngineOverride;
	}
	if (!strlen(job->VoiceOverride)) {
		strcpy(job->VoiceOverride, tts_config.Voice);
	}
	for (int i=0; tts_engines[i] != NULL; i++) {
		if (tts_engines[i]->Engine == Engine) {
			return tts_engines[i]->MakeWAVFromFile(job);
		}
	}
	return false;
}

bool MakeMP3FromText(TTS_JOB * job) {
	char fn[1024];
	sprintf(fn, "%stmp/tts_tmp.%u.txt", api->GetBasePath(), GetCurrentThreadId());
	FILE * fp = fopen(fn, "wb");
	if (!fp) {
		return false;
	}
	fwrite(job->text, strlen(job->text), 1, fp);
	fclose(fp);
	const char * old = job->inputFN;
	job->inputFN = fn;
	bool ret = MakeMP3FromFile(job);
	job->inputFN = old;
	remove(fn);
	return ret;
}

bool MakeWAVFromText(TTS_JOB * job) {
	char fn[1024];
	sprintf(fn, "%stmp/tts_tmp.%u.txt", api->GetBasePath(), GetCurrentThreadId());
	FILE * fp = fopen(fn, "wb");
	if (!fp) {
		return false;
	}
	fwrite(job->text, strlen(job->text), 1, fp);
	fclose(fp);
	const char * old = job->inputFN;
	job->inputFN = fn;
	bool ret = MakeWAVFromFile(job);
	job->inputFN = old;
	remove(fn);
	return ret;
}

bool Generic_MakeMP3FromFile(TTS_JOB * job) {
	char fn2[1024];
	bool ret = false;
	sprintf(fn2, "%stmp/tts_tmp.%u.wav", api->GetBasePath(), GetCurrentThreadId());
	const char * old = job->outputFN;
	job->outputFN = fn2;
	if (MakeWAVFromFile(job)) {
		std::ostringstream str;
		str << "lame";
		if (job->bitrate) {
			str << " -b " << job->bitrate;
		}
		if (job->sample) {
			str << " --resample " << job->sample;
		}
		switch(job->channels) {
			case 1:
				str << " -m m";
				break;
			case 2:
				str << " -m j";
				break;
			case 3:
				str << " -m s";
				break;
		}
		str << " \"" << fn2 << "\" \"" << old << "\"";
#if defined(WIN32)
		api->ib_printf(_("TTS Services -> Exec: %s\n"), str.str().c_str());
#endif
		system(str.str().c_str());
		ret = true;
	}
	job->outputFN = old;
	remove(fn2);
	return ret;
}

bool Generic_MakeWAVFromFile(TTS_JOB * job) {
	char fn2[1024];
	bool ret = false;
	sprintf(fn2, "%stmp/tts_tmp.%u.mp3", api->GetBasePath(), GetCurrentThreadId());
	const char * old = job->outputFN;
	job->outputFN = fn2;
	if (MakeMP3FromFile(job)) {
		std::ostringstream str;
		str << "lame --decode \"" << fn2 << "\" \"" << old << "\"";
#if defined(WIN32)
		api->ib_printf(_("TTS Services -> Exec: %s\n"), str.str().c_str());
#endif
		system(str.str().c_str());
		ret = true;
	}
	job->outputFN = old;
	remove(fn2);
	return ret;
}

int MessageHandler(unsigned int msg, char * data, int datalen) {
	//printf("MessageHandler(%u, %X, %d)\n", msg, data, datalen);

	switch(msg) {
		case TTS_IS_LOADED:
			return 1;
			break;

		case TTS_GET_INTERFACE:{
				TTS_INTERFACE * x = (TTS_INTERFACE *)data;
				x->GetPreferredOutputFormat = GetPreferredOutputFormat;
				x->MakeMP3FromFile = MakeMP3FromFile;
				x->MakeMP3FromText = MakeMP3FromText;
				x->MakeWAVFromFile = MakeWAVFromFile;
				x->MakeWAVFromText = MakeWAVFromText;
				return 1;
			}
			break;

	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{5251AEEF-DB89-47fd-BE12-50710D523D6B}",
	"TTS",
	"TTS Services Plugin",
	"Drift Solutions",
	init,
	quit,
	MessageHandler,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
