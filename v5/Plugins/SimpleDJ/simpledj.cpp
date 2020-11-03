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
#include "build.h"

/*
#if defined(WIN32)

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif


#include <wchar.h>
#include <sapi.h>
#include <sphelper.h>
#endif
*/

THREADTYPE PlayThread(VOID *lpData);
THREADTYPE PlaylistThread(VOID *lpData);
THREADTYPE UpdateTitle(VOID *lpData);
int SimpleDJ_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

int pluginnum=0;
BOTAPI_DEF * api=NULL;

Titus_Sockets3 * sockets;
Titus_Mutex * QueueMutex;
SD_CONFIG sd_config;
SD_CONFIG * GetSDConfig() { return &sd_config; }
unsigned long numplays=0;

QUEUE *rQue=NULL; // request queue
QUEUE *prFirst=NULL,*prLast=NULL;
QUEUE *prCur = NULL;

static READER readers[4] = {
	{ read_file_is_my_file, read_file_open },
	{ read_stream_is_my_file, read_stream_open },
#if !defined(WIN32) && defined(BLAHBLAHBLAH)
	{ read_mms_is_my_file, read_mms_open },
#endif
	{ NULL, NULL }
};

static FEEDER feeders[] = {
	{ sc_create, sc_destroy, SS_TYPE_SHOUTCAST, "ShoutCast" },
	{ ultravox_create, ultravox_destroy, SS_TYPE_SHOUTCAST2, "Shoutcast2" },
	{ ic_create, ic_destroy, SS_TYPE_ICECAST, "Icecast2" },
	{ sp2p_create, sp2p_destroy, SS_TYPE_SP2P, "StreamerP2P" },
	{ NULL, NULL, 0, NULL }
};

bool MatchesNoPlayFilter(char * str) {
	StrTokenizer * st = new StrTokenizer(sd_config.Options.NoPlayFilters, ',');
	int num = st->NumTok();
	for (int i=1; i <= num; i++) {
		char * pat = st->GetSingleTok(i);
		if (strlen(pat)) {
			if (wildicmp(str, pat)) {
				st->FreeString(pat);
				delete st;
				return true;
				break;
			}
		}
		st->FreeString(pat);
	}
	delete st;
	return false;
}

bool MatchFilter(QUEUE * Scan, TIMER * Timer, char * pat) {
	if (QueueMutex->LockingThread() != GetCurrentThreadId()) {
		api->ib_printf("SimpleDJ -> QueueMutex should be locked when calling MatchFilter!\n");
	}

	if (Timer->pat_type == TIMER_PAT_TYPE_FILENAME) {
		//api->botapi->ib_printf("SimpleDJ -> Completed Pattern: %s\n", pat);
		//api->botapi->LogToChan(buf);
		if (wildicmp_multi(pat,Scan->fn,"|,")) {
			return true;
		}
	}

	if (Timer->pat_type == TIMER_PAT_TYPE_DIRECTORY || Timer->pat_type == TIMER_PAT_TYPE_GENRE || Timer->pat_type == TIMER_PAT_TYPE_ARTIST || Timer->pat_type == TIMER_PAT_TYPE_ALBUM || Timer->pat_type == TIMER_PAT_TYPE_TITLE) {
		//api->botapi->ib_printf("SimpleDJ -> Completed Pattern: %s\n", pat);
		//api->botapi->LogToChan(buf);

		if (Scan->meta) {
			char * str = NULL;
			switch(Timer->pat_type) {
				case TIMER_PAT_TYPE_ARTIST:
					str = Scan->meta->artist;
					break;
				case TIMER_PAT_TYPE_ALBUM:
					str = Scan->meta->album;
					break;
				case TIMER_PAT_TYPE_GENRE:
					str = Scan->meta->genre;
					break;
				case TIMER_PAT_TYPE_DIRECTORY:
					str = Scan->path;
					break;
				default:
					str = Scan->meta->title;
					break;
			}
			if (wildicmp_multi(pat,str,"|,")) {
				api->ib_printf("SimpleDJ -> Matched Pattern: %s -> %s\n", pat, str);
				return true;
			}
		}
	}

	return false;
}

void AddToQueue(QUEUE * q, QUEUE ** qqFirst, QUEUE ** qqLast) {
	if (QueueMutex->LockingThread() != GetCurrentThreadId()) {
		api->ib_printf("SimpleDJ -> QueueMutex should be locked when calling AddToQueue!\n");
	}
	QUEUE *fQue = *qqFirst;
	QUEUE *lQue = *qqLast;
	q->Next = NULL;
	if (fQue) {
		lQue->Next = q;
		q->Prev = lQue;
		lQue = q;
	} else {
		q->Prev = NULL;
		fQue = lQue = q;
	}
	*qqFirst = fQue;
	*qqLast  = lQue;
}

int ReadMetaData(const char * fn, TITLE_DATA * td) {// retval: 0 title based on FN, 1 metadata read from file
	memset(td, 0, sizeof(TITLE_DATA));
	int ret = 0;
	if (access(fn, 0) == 0) {
		DECODER * dec = GetFileDecoder((char *)fn);
		if (dec) {
			try {
				ret = dec->get_title_data((char *)fn, td, NULL) ? 1:0;
			} catch(...) {
				api->ib_printf("SimpleDJ -> Exception occured while trying to process %s\n", fn);
				ret = 0;
			}
			if (!strlen(td->title)) {
				ret = 0;
			}
		}
	}
	if (ret == 0) {
		memset(td, 0, sizeof(TITLE_DATA));
		const char * p = strrchr(fn, '/');
		if (!p) {
			p = strrchr(fn, '\\');
		}
		if (p) {
			strlcpy(td->title, p+1, sizeof(td->title));
		} else {
			strlcpy(td->title, fn, sizeof(td->title));
		}
	}
	return ret;
}

void ScanDirectory(char * path, QUEUE ** qqFirst, QUEUE ** qqLast, int32 * num_files) {
	if (QueueMutex->LockingThread() != GetCurrentThreadId()) {
		api->ib_printf("SimpleDJ -> QueueMutex should be locked when calling Scan Directory!\n");
	}
	//QUEUE *fQue = *qqFirst;
	//QUEUE *lQue = *qqLast;

	char buf[MAX_PATH],buf2[MAX_PATH];
	//QUEUE *Scan=qFirst;

	Directory * dir = new Directory(path);
	DECODER * dec = NULL;
	while (dir->Read(buf, sizeof(buf), NULL)) {
		sprintf(buf2, "%s%s", path, buf);
		struct stat st;
		//time_t mTime = 0;
		if (stat(buf2, &st)) {// error completing stat
			continue;
		}
		if (!S_ISDIR(st.st_mode)) {
			if ((dec = GetFileDecoder(buf))) {
				QUEUE * Scan=(QUEUE *)zmalloc(sizeof(QUEUE));
				memset(Scan,0,sizeof(QUEUE));
				Scan->fn = zstrdup(buf);
				Scan->path = zstrdup(path);
				//unsigned long id = ;
				//Scan->ID = FileID(buf2);

				TITLE_DATA * td = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
				if (ReadMetaData(buf2, td) == 1) {
					Scan->meta = td;
				} else {
					Scan->meta = NULL;
					zfree(td);
				}

				*num_files = *num_files + 1;
				AddToQueue(Scan, qqFirst, qqLast);
				//sd_config.nofiles++;
			}
		} else if (buf[0] != '.') {
#ifdef WIN32
			sprintf(buf2,"%s%s\\",path,buf);
#else
			sprintf(buf2,"%s%s/",path,buf);
#endif
			ScanDirectory(buf2, qqFirst, qqLast, num_files);
		}
	}
	delete dir;
	//DRIFT_DIGITAL_SIGNATURE();
}

bool LoadConfig() {

	DS_CONFIG_SECTION * pSec = api->config->GetConfigSection(NULL, "SimpleDJ");
	if (pSec == NULL) {
		api->ib_printf("SimpleDJ -> No 'SimpleDJ' section found in your config!\n");
		return false;
	}

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(pSec, "Server");
	if (sec == NULL) {
		api->ib_printf("SimpleDJ -> No 'Server' section found in your config!\n");
		return false;
	}
	char * p = NULL;
	int j=0;

	api->config->GetConfigSectionValueBuf(sec, "password", sd_config.Server.Pass, sizeof(sd_config.Server));
	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		strcpy(sd_config.Servers[j].Pass, sd_config.Server.Pass);
	}
	if (strlen(sd_config.Server.Pass)) {
		sd_config.Servers[0].Enabled = true;
	}
	DRIFT_DIGITAL_SIGNATURE();
	if ((p = api->config->GetConfigSectionValue(sec,"description"))) {
		sstrcpy(sd_config.Server.Desc,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			sstrcpy(sd_config.Servers[j].Desc, p);
		}
	}
	if ((p = api->config->GetConfigSectionValue(sec,"genre"))) {
		sstrcpy(sd_config.Server.Genre,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			sstrcpy(sd_config.Servers[j].Genre, p);
		}
	}
	if ((p = api->config->GetConfigSectionValue(sec,"mount"))) {
		sstrcpy(sd_config.Server.Mount,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			sstrcpy(sd_config.Servers[j].Mount, p);
		}
	}
	if ((p = api->config->GetConfigSectionValue(sec,"url"))) {
		sstrcpy(sd_config.Server.URL,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			sstrcpy(sd_config.Servers[j].URL, p);
		}
	}
	if ((p = api->config->GetConfigSectionValue(sec,"name"))) {
		sstrcpy(sd_config.Server.Name,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			sstrcpy(sd_config.Servers[j].Name, p);
		}
	}

	api->config->GetConfigSectionValueBuf(sec, "icq", sd_config.Server.ICQ, sizeof(sd_config.Server.ICQ)-1);
	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		sstrcpy(sd_config.Servers[j].ICQ, sd_config.Server.ICQ);
	}

	if ((p = api->config->GetConfigSectionValue(sec,"aim"))) {
		sstrcpy(sd_config.Server.AIM,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			sstrcpy(sd_config.Servers[j].AIM, p);
		}
	}
	if ((p = api->config->GetConfigSectionValue(sec,"irc"))) {
		sstrcpy(sd_config.Server.IRC,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			sstrcpy(sd_config.Servers[j].IRC, p);
		}
	}

	if ((p = api->config->GetConfigSectionValue(sec,"MimeOverride"))) {
		strcpy(sd_config.Server.MimeType,p);
		for (j=0; j < MAX_SOUND_SERVERS; j++) {
			strcpy(sd_config.Servers[j].MimeType, p);
		}
	}

	if ((p = api->config->GetConfigSectionValue(sec,"Content"))) {
		memset(sd_config.Server.ContentDir,0,sizeof(sd_config.Server.ContentDir));
		char * tmp = zstrdup(p);
		char * p3 = NULL;
		char * p2 = strtok_r(tmp, ";", &p3);
		while (p2) {
			if (strlen(sd_config.Server.ContentDir)) {
				strlcat(sd_config.Server.ContentDir, ";",sizeof(sd_config.Server.ContentDir));
			}
			strlcat(sd_config.Server.ContentDir, p2,sizeof(sd_config.Server.ContentDir));
			#ifdef _WIN32
			if (sd_config.Server.ContentDir[strlen(sd_config.Server.ContentDir) - 1] != '\\') {
				strlcat(sd_config.Server.ContentDir,"\\",sizeof(sd_config.Server.ContentDir));
			}
			#else
			if (sd_config.Server.ContentDir[strlen(sd_config.Server.ContentDir) - 1] != '/') {
				strlcat(sd_config.Server.ContentDir,"/",sizeof(sd_config.Server.ContentDir));
			}
			#endif
			p2 = strtok_r(NULL, ";", &p3);
		}
		zfree(tmp);
	}

	if ((p = api->config->GetConfigSectionValue(sec,"promo"))) {
		sstrcpy(sd_config.Server.PromoDir,p);
		DRIFT_DIGITAL_SIGNATURE();
		#ifdef _WIN32
		if (sd_config.Server.PromoDir[strlen(sd_config.Server.PromoDir) - 1] != '\\') {
			strlcat(sd_config.Server.PromoDir,"\\",sizeof(sd_config.Server.PromoDir));
		}
		#else
		if (sd_config.Server.PromoDir[strlen(sd_config.Server.PromoDir) - 1] != '/') {
			strlcat(sd_config.Server.PromoDir,"/",sizeof(sd_config.Server.PromoDir));
		}
		#endif
	}

	sd_config.Server.Public = api->config->GetConfigSectionLong(sec, "Public") > 0 ? true:false;
	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		sd_config.Servers[j].Public = sd_config.Server.Public;
	}
	sd_config.Server.Reset = api->config->GetConfigSectionLong(sec, "Reset") > 0 ? true:false;
	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		sd_config.Servers[j].Reset = sd_config.Server.Reset;
	}
	sd_config.Server.Bitrate = api->config->GetConfigSectionLong(sec, "Bitrate");
	if (sd_config.Server.Bitrate <= 0) { sd_config.Server.Bitrate = 64; }
	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		sd_config.Servers[j].Bitrate = sd_config.Server.Bitrate;
	}
	sd_config.Server.Channels = api->config->GetConfigSectionLong(sec, "Channels");
	if (sd_config.Server.Channels <= 0) { sd_config.Server.Channels = 1; }
	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		sd_config.Servers[j].Channels = sd_config.Server.Channels;
	}
	sd_config.Server.Sample = api->config->GetConfigSectionLong(sec, "Sample");
	if (sd_config.Server.Sample <= 0) { sd_config.Server.Sample = 44100; }
	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		sd_config.Servers[j].Sample = sd_config.Server.Sample;
	}

	/* Load individual server settings */

	for (j=0; j < MAX_SOUND_SERVERS; j++) {
		char buf[128];

		sprintf(buf, "Server%d", j);
		sec = api->config->GetConfigSection(pSec, buf);
		if (sec == NULL) {
			continue;
		}

		DRIFT_DIGITAL_SIGNATURE();

		if (api->config->IsConfigSectionValue(sec,"password")) {
			api->config->GetConfigSectionValueBuf(sec, "password", sd_config.Servers[j].Pass, sizeof(sd_config.Servers[j].Pass));
		}

		if ((p = api->config->GetConfigSectionValue(sec,"description"))) {
			sstrcpy(sd_config.Servers[j].Desc, p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"genre"))) {
			sstrcpy(sd_config.Servers[j].Genre, p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"mount"))) {
			sstrcpy(sd_config.Servers[j].Mount, p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"url"))) {
			sstrcpy(sd_config.Servers[j].URL, p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"name"))) {
			sstrcpy(sd_config.Servers[j].Name, p);
		}

		if (api->config->IsConfigSectionValue(sec,"icq")) {
			api->config->GetConfigSectionValueBuf(sec, "icq", sd_config.Servers[j].ICQ, sizeof(sd_config.Servers[j].ICQ)-1);
		}

		if ((p = api->config->GetConfigSectionValue(sec,"aim"))) {
			sstrcpy(sd_config.Servers[j].AIM, p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"irc"))) {
			sstrcpy(sd_config.Servers[j].IRC, p);
		}

		if ((p = api->config->GetConfigSectionValue(sec,"MimeOverride"))) {
			sstrcpy(sd_config.Servers[j].MimeType, p);
		}

		if (api->config->IsConfigSectionValue(sec,"Enabled")) {
			sd_config.Servers[j].Enabled = api->config->GetConfigSectionLong(sec, "Enabled") > 0 ? true:false;
		}
		if (api->config->IsConfigSectionValue(sec,"Reset")) {
			sd_config.Servers[j].Reset = api->config->GetConfigSectionLong(sec, "Reset") > 0 ? true:false;
		}

		if (api->config->IsConfigSectionValue(sec,"public")) {
			sd_config.Servers[j].Public = api->config->GetConfigSectionLong(sec, "Public") > 0 ? true:false;
		}
		if (api->config->IsConfigSectionValue(sec,"bitrate")) {
			sd_config.Servers[j].Bitrate = api->config->GetConfigSectionLong(sec, "Bitrate");
			if (sd_config.Servers[j].Bitrate <= 0) { sd_config.Servers[j].Bitrate = 64; }
		}
		if (api->config->IsConfigSectionValue(sec,"Channels")) {
			sd_config.Servers[j].Channels = api->config->GetConfigSectionLong(sec, "Channels");
			if (sd_config.Servers[j].Channels <= 0) { sd_config.Servers[j].Channels = 1; }
		}
		if (api->config->IsConfigSectionValue(sec,"sample")) {
			sd_config.Servers[j].Sample = api->config->GetConfigSectionLong(sec, "Sample");
			if (sd_config.Servers[j].Sample <= 0) { sd_config.Servers[j].Sample = 44100; }
		}
	}

	/* End individual server settings */

	sec = api->config->GetConfigSection(pSec, "Options");
#ifdef WIN32
	strcpy(sd_config.Options.QueuePlugin, ".\\plugins\\sdjq_memory.dll");
#else
	strcpy(sd_config.Options.QueuePlugin, "./plugins/sdjq_memory.so");
#endif

	if (sec == NULL) {
		api->ib_printf("SimpleDJ -> No 'Options' section found in your config!\n");
	} else {
		if ((p = api->config->GetConfigSectionValue(sec,"MoveDir"))) {
			sstrcpy(sd_config.Options.MoveDir,p);
			if (sd_config.Options.MoveDir[strlen(sd_config.Options.MoveDir) - 1] == '\\') {
				sd_config.Options.MoveDir[strlen(sd_config.Options.MoveDir) - 1] = 0;
			}
			if (sd_config.Options.MoveDir[strlen(sd_config.Options.MoveDir) - 1] == '/') {
				sd_config.Options.MoveDir[strlen(sd_config.Options.MoveDir) - 1] = 0;
			}
		}

		if ((p = api->config->GetConfigSectionValue(sec, "VoiceTitle"))) {
			sstrcpy(sd_config.Voice.Title, p);
		} else {
			strcpy(sd_config.Voice.Title, "The Voice of SimpleDJ");
		}

		if ((p = api->config->GetConfigSectionValue(sec, "VoiceArtist"))) {
			sstrcpy(sd_config.Voice.Artist, p);
		} else {
			strcpy(sd_config.Voice.Artist, "www.ShoutIRC.com");
		}

		if ((p = api->config->GetConfigSectionValue(sec,"NoPlayFilters"))) {
			sstrcpy(sd_config.Options.NoPlayFilters,p);
		}

		if ((p = api->config->GetConfigSectionValue(sec,"QueuePlugin")) && strlen(p)) {
			sstrcpy(sd_config.Options.QueuePlugin,p);
		}

		sd_config.Voice.EnableVoice = api->config->GetConfigSectionLong(sec, "EnableVoice") > 0 ? true:false;
		sd_config.Options.EnableFind = api->config->GetConfigSectionLong(sec, "EnableFind") != 0 ? true:false;
		//sd_config.Options.EnableRatings = api->config->GetConfigSectionLong(sec, "EnableRating") > 0 ? true:false;
		sd_config.Options.EnableYP = api->config->GetConfigSectionLong(sec, "EnableYP") != 0 ? true:false;
		sd_config.Options.EnableRequests = api->config->GetConfigSectionLong(sec, "EnableRequests") != 0 ? true:false;
		sd_config.Options.AutoStart = api->config->GetConfigSectionLong(sec, "AutoStart") != 0 ? true:false;
		sd_config.Options.AutoPlayIfNoSource = api->config->GetConfigSectionLong(sec, "AutoPlayIfNoSource") > 0 ? true:false;
		sd_config.Options.DoPromos = api->config->GetConfigSectionLong(sec, "DoPromos") > 0 ? api->config->GetConfigSectionLong(sec, "DoPromos"):0;
		sd_config.Options.NumPromos = api->config->GetConfigSectionLong(sec, "NumPromos") > 0 ? api->config->GetConfigSectionLong(sec, "NumPromos"):1;
		sd_config.Options.NoRepeat = api->config->GetConfigSectionLong(sec, "NoRepeat") > 0 ? api->config->GetConfigSectionLong(sec, "NoRepeat"):0;
		sd_config.Options.MinReqTimePerSong = api->config->GetConfigSectionLong(sec, "MinReqTimePerSong") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerSong"):0;
		sd_config.Options.AutoReload = api->config->GetConfigSectionLong(sec, "AutoReload") > 0 ? api->config->GetConfigSectionLong(sec, "AutoReload"):0;
	}

	sec = api->config->GetConfigSection(pSec, "FileLister");
	if (sec != NULL) {
		if ((p = api->config->GetConfigSectionValue(sec,"List"))) {
			sstrcpy(sd_config.FileLister.List,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"Header"))) {
			sstrcpy(sd_config.FileLister.Header,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"Footer"))) {
			sstrcpy(sd_config.FileLister.Footer,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"NewChar"))) {
			sstrcpy(sd_config.FileLister.NewChar,p);
		}
		sstrcpy(sd_config.FileLister.Line,"%file%");
		if ((p = api->config->GetConfigSectionValue(sec,"Line"))) {
			sstrcpy(sd_config.FileLister.Line,p);
		}
	}

	char buf[128];
	char msgbuf[8192];
	for (int i=0; i < MAX_VOICE_MESSAGES; i++) {
		sprintf(buf, "ADJVoice_%d", i);
		if (api->LoadMessage(buf, msgbuf, sizeof(msgbuf))) {
			sd_config.Voice.Messages[sd_config.Voice.NumMessages] = zstrdup(msgbuf);
			sd_config.Voice.NumMessages++;
		}
	}

	return true;
}

void FreeQueue(QUEUE * Scan) {
	zfree(Scan->fn);
	zfree(Scan->path);
	if (Scan->meta) {
		zfree(Scan->meta);
	}
	if (Scan->isreq) {
		zfree(Scan->isreq);
	}
	DRIFT_DIGITAL_SIGNATURE();
	zfree(Scan);
}

QUEUE * CopyQueue(QUEUE * Scan) {
	QUEUE * ret = (QUEUE *)zmalloc(sizeof(QUEUE));
	memcpy(ret,Scan,sizeof(QUEUE));
	ret->fn = zstrdup(Scan->fn);
	ret->path = zstrdup(Scan->path);
	if (Scan->isreq) {
		ret->isreq = (REQ_INFO *)zmalloc(sizeof(REQ_INFO));
		memcpy(ret->isreq,Scan->isreq,sizeof(REQ_INFO));
	}
	if (Scan->meta) {
		ret->meta = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
		memcpy(ret->meta,Scan->meta,sizeof(TITLE_DATA));
	}
	return ret;
}

READER_HANDLE * OpenFile(const char * fn, const char * mode) {
	for (int i=0; readers[i].is_my_file != NULL; i++) {
		if (readers[i].is_my_file(fn)) {
			READER_HANDLE * ret = readers[i].open(fn,mode);
			if (ret) {
				api->ib_printf("SimpleDJ -> Opened %s '%s' via %s\n",ret->type,fn,ret->desc);
			}
			return ret;
		}
	}
	return NULL;
}

void QueuePromoFiles(char *path) {
	if (path[0] == 0) { return; }

	LockMutexPtr(QueueMutex);
	//int32 nopromos=0;

	QUEUE *Scan=prFirst;

	prLast=NULL;
	prFirst=NULL;
	prCur=NULL;

	while (Scan != NULL) {
		QUEUE *todel=Scan;
		Scan=Scan->Next;
		FreeQueue(todel);
	}

	ScanDirectory(path, &prFirst, &prLast, &sd_config.nopromos);
	//FreeMetaCache();
	if (sd_config.nopromos > 1) {
		//SortQueue(&prFirst, &prLast);
	}
	DRIFT_DIGITAL_SIGNATURE();

	api->ib_printf("SimpleDJ -> %d Promos Loaded...\n",sd_config.nopromos);
	if (prFirst == NULL) {
		RelMutexPtr(QueueMutex);
		return;
	}

	api->ib_printf("SimpleDJ -> Beginning shuffle...\n");
	Scan=prFirst;
	int cycles = sd_config.nopromos * 3;
	while (cycles--) {
		int go = (SimpleDJ_Rand()%sd_config.nopromos) + 1;
		Scan=prFirst;
		while (go--) {
			Scan=Scan->Next;
			if (Scan == NULL) { Scan=prFirst; } // loop around
		}
		if (Scan == prFirst) {
			continue; //wtf? move it to itself!!!
		}
		if (Scan == prLast) {
			continue; //blah, too much work for this, 3 passes should be enough for all the randomity you need
		}
		if (prFirst->Next != NULL) {
			QUEUE *newFirst=prFirst->Next;
			prFirst->Prev =Scan;
			prFirst->Next = Scan->Next;
			Scan->Next=prFirst;

			prFirst=newFirst;
			prFirst->Prev=NULL;
		}
	}
	api->ib_printf("SimpleDJ -> End shuffle.\n");
	RelMutexPtr(QueueMutex);
}

int init(int num, BOTAPI_DEF * BotAPI) {
	memset(&sd_config,0,sizeof(sd_config));
	pluginnum = num;
	GetAPI()->botapi = api = (BOTAPI_DEF *)BotAPI;
	if (api->ss == NULL) {
		api->ib_printf(_("SimpleDJ -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	api->ib_printf("SimpleDJ -> Version: %s\n", SIMPLEDJ_VERSION);

	QueueMutex = new Titus_Mutex();

	if (!LoadConfig()) {
		delete QueueMutex;
		return 0;
	}

	if (sd_config.Options.AutoStart) {
		sd_config.playing = true;
	}

#ifdef _WIN32
	LoadPlugins(".\\plugins\\");
#else
	LoadPlugins("./plugins/");
	if (sd_config.noplugins == 0){
		LoadPlugins("/usr/lib/shoutirc/plugins/");
	}
	if (sd_config.noplugins == 0){
		LoadPlugins("/usr/local/lib/shoutirc/plugins/");
	}
#endif

	LoadQueuePlugin(sd_config.Options.QueuePlugin);

	if (sd_config.Queue == NULL) {
		api->ib_printf("SimpleDJ -> ERROR: There is no Queue plugin loaded!\n");
		UnloadPlugins();
		delete QueueMutex;
		return 0;
	} else {
		sd_config.nofiles = sd_config.Queue->QueueContentFiles();
	}
	QueuePromoFiles(sd_config.Server.PromoDir);

	sockets = new Titus_Sockets3;
	DRIFT_DIGITAL_SIGNATURE();

	#if !defined(WIN32)
	api->ib_printf("SimpleDJ -> Ignoring some signals...\n");
	signal(SIGSEGV,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	#endif

	if (api->SendMessage(-1, TTS_GET_INTERFACE, (char *)&sd_config.tts, sizeof(&sd_config.tts)) != 1 && sd_config.Voice.EnableVoice) {
		api->ib_printf("SimpleDJ -> Error: TTS Services plugin is not loaded! You must load the TTS Services plugin in order to use the SimpleDJ voice.\n");
		sd_config.Voice.EnableVoice = false;
	}

	TT_StartThread(PlayThread,     NULL, "Playback Control", pluginnum);
	TT_StartThread(UpdateTitle,    NULL, "Title Updater", pluginnum);
	TT_StartThread(ScheduleThread, NULL, "Scheduler", pluginnum);

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-move",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE			, SimpleDJ_Commands, "This tells SimpleDJ to move the currently playing file to a holding folder for review");
	api->commands->RegisterCommand_Proc(pluginnum, "move",				&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC			, SimpleDJ_Commands, "This tells SimpleDJ to move the currently playing file to a holding folder for review");
	api->commands->RegisterCommand_Proc(pluginnum, "relay",				&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC			, SimpleDJ_Commands, "This queues a file to play. Takes a pathname or stream://");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-relay",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE			, SimpleDJ_Commands, "This queues a file to play. Takes a pathname or stream://");

	api->commands->RegisterCommand_Proc(pluginnum, "autodj-chroot",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This tells SimpleDJ to change it's song folder");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-speak",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This tells SimpleDJ to speak the specified text at the end of the current song");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-modules",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This lets you see the plugins loaded in SimpleDJ");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-name",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This command will let you change the name SimpleDJ uses on your sound server");

	api->commands->FillACL(&acl, 0, UFLAG_ADVANCED_SOURCE, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-next",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This tells SimpleDJ to skip the current song and move on to the next one");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-requests",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This command will toggle wether SimpleDJ will take requests or not");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-songtitle",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This command will let you send an updated song title to your sound server");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-reqlist",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This will show you any files in the request queue");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-reqdelete",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This will delete a file from the request queue. You can use a filename or a wildcard pattern");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-reload",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE|CM_FLAG_ASYNC	, SimpleDJ_Commands, "This tells SimpleDJ to re-scan your folders for new songs and reload the schedule (if any)");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-force",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This command makes SimpleDJ stop playing immediately");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-dopromo",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE					, SimpleDJ_Commands, "This command makes SimpleDJ play a promo after the current song");

	api->commands->FillACL(&acl, UFLAG_BASIC_SOURCE, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-stop",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE			, SimpleDJ_Commands, "This command tells SimpleDJ to count you in so you can DJ");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-play",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE			, SimpleDJ_Commands, "This command tells SimpleDJ to start playing");

	api->commands->FillACL(&acl, 0, UFLAG_REQUEST, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "songby",			&acl, CM_ALLOW_ALL, SimpleDJ_Commands, "This will let you request a random song by a certain artist");

	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "next",				&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC			, SimpleDJ_Commands, "This tells SimpleDJ to skip the current song and move on to the next one");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-countdown",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE			, SimpleDJ_Commands, "This will countdown the current song to you until it is over");
	api->commands->RegisterCommand_Proc(pluginnum, "countdown",			&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC			, SimpleDJ_Commands, "This will countdown the current song to you until it is over");

	return 1;
}

void quit() {
	api->ib_printf("SimpleDJ -> Beginning shutdown...\n");

	api->commands->UnregisterCommandByName( "relay" );
	api->commands->UnregisterCommandByName( "songby" );
	api->commands->UnregisterCommandByName( "move" );
	api->commands->UnregisterCommandByName( "countdown" );
	api->commands->UnregisterCommandByName( "autodj-countdown" );
	api->commands->UnregisterCommandByName( "next" );
	api->commands->UnregisterCommandByName( "autodj-move" );
	api->commands->UnregisterCommandByName( "autodj-next" );
	api->commands->UnregisterCommandByName( "autodj-chroot" );
	api->commands->UnregisterCommandByName( "autodj-name" );
	api->commands->UnregisterCommandByName( "autodj-songtitle" );
	api->commands->UnregisterCommandByName( "autodj-speak" );
	api->commands->UnregisterCommandByName( "autodj-requests" );
	api->commands->UnregisterCommandByName( "autodj-reqlist" );
	api->commands->UnregisterCommandByName( "autodj-reqdelete" );
	api->commands->UnregisterCommandByName( "autodj-modules" );
	api->commands->UnregisterCommandByName( "autodj-reload" );
	api->commands->UnregisterCommandByName( "autodj-force" );
	api->commands->UnregisterCommandByName( "autodj-dopromo" );
	api->commands->UnregisterCommandByName( "autodj-stop" );
	api->commands->UnregisterCommandByName( "autodj-play" );

	sd_config.shutdown_now=true;
	sd_config.playagainat=0;
	sd_config.playing=false;
	sd_config.countdown=false;
	sd_config.force_promo = false;

	time_t timeo = time(NULL) + 15;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf("SimpleDJ -> Waiting on threads to die (%d) (Will wait %d more seconds...)\n",TT_NumThreads(),timeo - time(NULL));
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}

	LockMutexPtr(QueueMutex);

	api->ib_printf("SimpleDJ -> Freeing request queue...\n");
	QUEUE * Scan=rQue;
	while (Scan != NULL) {
		QUEUE * toDel = Scan;
		Scan=Scan->Next;
		FreeQueue(toDel);
	}

	api->ib_printf("SimpleDJ -> Freeing promo queue...\n");
	Scan=prFirst;
	while (Scan != NULL) {
		QUEUE * toDel = Scan;
		Scan=Scan->Next;
		FreeQueue(toDel);
	}

	api->ib_printf("SimpleDJ -> Freeing main file queue...\n");
	if (sd_config.Queue) { sd_config.Queue->FreePlayList(); }

	RelMutexPtr(QueueMutex);

	api->ib_printf("SimpleDJ -> Unloading plugins...\n");
	UnloadQueuePlugin();
	UnloadPlugins();

	api->ib_printf("SimpleDJ -> Freeing sockets...\n");
	delete sockets;

	api->ib_printf("SimpleDJ -> Freeing mutexes...\n");
	delete QueueMutex;

	int i=0;
	for (i=0; i < sd_config.Voice.NumMessages; i++) {
		if (sd_config.Voice.Messages[i]) {
			zfree(sd_config.Voice.Messages[i]);
		}
	}

	api->ib_printf("SimpleDJ -> Plugin Shut Down!\n");
}

char * PrettyTime(int32 seconds, char * buf) {
	int m=0, h=0;
	while (seconds >= 3600) {
		h++;
		seconds -= 3600;
	}
	while (seconds >= 60) {
		m++;
		seconds -= 60;
	}
	if (h) {
		sprintf(buf,"%d:%.2d:%.2d",h,m,seconds);
	} else {
		sprintf(buf,"%.2d:%.2d",m,seconds);
	}
	return buf;
}

sql_rows GetTable(char * query, char **errmsg) {
//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = api->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		api->ib_printf("GetTable: %s -> %d (%s)\n", query, ret, *errmsg2);
	} else {
		if (ncol && nrow) {
			for (int i=0; i < nrow; i++) {
				sql_row row;
				for (int s=0; s < ncol; s++) {
					char * p = resultp[(i*ncol)+ncol+s];
					if (!p) { p=""; }
					row[resultp[s]] = p;
				}
				rows[i] = row;
			}
		}
		api->db->FreeTable(resultp);
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}

void GetSongRating(uint32 FileID, SONG_RATING * r) {
	api->ss->GetSongRatingByID(FileID, r);
}


int SimpleDJ_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[4096], buf2[4096];
	if (!stricmp(command, "autodj-move") || !stricmp(command, "move")) {
		if (sd_config.playing && sd_config.connected && strlen(sd_config.Options.MoveDir)) {
			ut->std_reply(ut, "I will attempt to move the file when playback is complete, watch the console or LogChan for results...");
			sd_config.move = true;
		} else {
			ut->std_reply(ut, "Unable to comply, either I am not playing a song or you did not specify a MoveDir in your SimpleDJ section");
		}
		return 1;
	}

	if (!stricmp(command,"autodj-modules")) {
		sprintf(buf,"Number of SimpleDJ Plugins: %d",sd_config.noplugins);
		ut->std_reply(ut, buf);
		for (int i=0; i < sd_config.noplugins; i++) {
			sprintf(buf,"[%u] [%p] [%02d] [Name: %s]",i,sd_config.Plugins[i].plug,sd_config.Plugins[i].plug->version,sd_config.Plugins[i].plug->plugin_desc);
			ut->std_reply(ut, buf);
		}
		ut->std_reply(ut, "End of SimpleDJ Plugins");
		return 1; //don't interfere with RadioBot's internal modules
	}

	DRIFT_DIGITAL_SIGNATURE();

	if (!stricmp(command, "autodj-reload")) {
		ut->std_reply(ut, "Reloading schedule...");
		LoadSchedule();
		ut->std_reply(ut, "Beginning re-queue...");
		sd_config.nofiles = sd_config.Queue->QueueContentFiles();
		QueuePromoFiles(sd_config.Server.PromoDir);
		prCur=prFirst;
		sprintf(buf,"Re-queue complete! (Songs: %d)",sd_config.nofiles);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "autodj-stop")) {
		ut->std_reply(ut, "Stopping after current song...");
		RefUser(ut);
		sd_config.repto = ut;
		//sd_config.repnet = ut->NetNo;
		sd_config.countdown = true;
		return 1;
	}

	DRIFT_DIGITAL_SIGNATURE();

	if (!stricmp(command, "autodj-force")) {
		ut->std_reply(ut, "Stopping immediately...");
		api->SendMessage(-1, SOURCE_C_STOP, NULL, 0);
		return 1;
	}

	if (!stricmp(command, "autodj-play")) {
		ut->std_reply(ut, "Playing...");
		api->SendMessage(-1, SOURCE_C_PLAY, NULL, 0);
		return 1;
	}

	if (!stricmp(command, "autodj-chroot") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, "Usage: autodj-chroot dir");
		return 1;
	}

	if (!stricmp(command,"autodj-chroot")) {

		sstrcpy(sd_config.Server.ContentDir,parms);
		if (sd_config.Server.ContentDir[strlen(sd_config.Server.ContentDir) - 1] != PATH_SEP) {
			strlcat(sd_config.Server.ContentDir, PATH_SEPS, sizeof(sd_config.Server.ContentDir));
		}
		snprintf(buf,sizeof(buf),"Changed content dir to %s",sd_config.Server.ContentDir);
		ut->std_reply(ut, buf);
		SimpleDJ_Commands("autodj-reload", NULL, ut, type);
		return 1;
	}

	if (!stricmp(command, "autodj-name") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, "Usage: autodj-name NewName");
		return 1;
	}

	if (!stricmp(command,"autodj-name")) {
		strlcpy(sd_config.Server.Name, parms, sizeof(sd_config.Server.Name));
		for (uint32 i=0; i < MAX_SOUND_SERVERS; i++) {
			strcpy(sd_config.Servers[i].Name, sd_config.Server.Name);
		}
		snprintf(buf,sizeof(buf),"Changed SimpleDJ name to %s. (Note: the name will not change until the SimpleDJ reconnects to the sound server)",sd_config.Server.Name);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "autodj-songtitle") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, "Usage: autodj-songtitle Song title to send");
		return 1;
	}

	if (!stricmp(command,"autodj-songtitle")) {

		TITLE_DATA td;
		memset(&td, 0, sizeof(TITLE_DATA));
		strlcpy(td.title, parms, sizeof(td.title));
		QueueTitleUpdate(&td);
		sprintf(buf,"Queued title update....");
		ut->std_reply(ut, buf);
		return 1;
	}

	DRIFT_DIGITAL_SIGNATURE();

	if (!stricmp(command, "autodj-speak") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, "Usage: autodj-speak text");
		return 1;
	}

	if (!stricmp(command,"autodj-speak")) {
		if (sd_config.Voice.EnableVoice) {
			sstrcpy(sd_config.voice_override,parms);
			ut->std_reply(ut, "I will speak at the end of this song...");
		} else {
			ut->std_reply(ut, "The SimpleDJ voice is not enabled!");
		}
		return 1;
	}

	if ((!stricmp(command,"autodj-next") || !stricmp(command,"next")) && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		if (sd_config.playing && sd_config.connected) {
			ut->std_reply(ut, "Skipping song...");
			api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
		} else {
			ut->std_reply(ut, _("I am not playing a song right now!"));
		}
		return 1;
	}

	if (!stricmp(command,"autodj-reqlist")) {
		if (!sd_config.playing || !sd_config.connected) {
			return 0;
		}
		int num = 0;
		ut->std_reply(ut, "Listing current requests...");
		LockMutexPtr(QueueMutex);
		QUEUE * Scan= rQue;
		while (Scan) {
			sprintf(buf, "[%p] %s", Scan, Scan->fn);
			ut->std_reply(ut, buf);
			num++;
			Scan = Scan->Next;
		}
		sprintf(buf, "Number of requests: %d", num);
		ut->std_reply(ut, buf);
		ut->std_reply(ut, "Use !autodj-reqdelete filename to remove the first request that matches that filename (wildcards supported)");
		RelMutexPtr(QueueMutex);
		return 1;
	}

	if (!stricmp(command, "autodj-reqdelete") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, "Usage: autodj-reqdelete filename");
		return 1;
	}

	if (!stricmp(command,"autodj-reqdelete")) {
		if (!sd_config.playing || !sd_config.connected) {
			return 0;
		}
		LockMutexPtr(QueueMutex);
		QUEUE * Scan= rQue;
		bool done = false;
		while (Scan) {
			if (wildicmp(parms, Scan->fn)) {
				sprintf(buf, "Removed request %p", Scan);
				ut->std_reply(ut, buf);
				if (Scan == rQue) {
					rQue = rQue->Next;
					if (rQue) {
						rQue->Prev = NULL;
					}
				} else if (Scan->Next == NULL) {
					Scan->Prev->Next = NULL;
				} else {
					Scan->Prev->Next = Scan->Next;
					Scan->Next->Prev = Scan->Prev;
				}
				FreeQueue(Scan);
				done = true;
				break;
			}
			Scan = Scan->Next;
		}
		if (!done) {
			ut->std_reply(ut, "Error: No request matching that filename or wildcard found!");
		}
		RelMutexPtr(QueueMutex);
		return 1;
	}

	if (!stricmp(command,"autodj-requests")) {
		if (parms && strlen(parms)) {
			if (!stricmp(parms, "1") || !stricmp(parms, "on")) {
				sd_config.Options.EnableRequests = true;
			} else {
				sd_config.Options.EnableRequests = false;
			}
		} else {
			sd_config.Options.EnableRequests = sd_config.Options.EnableRequests ? false:true;
		}
		if (sd_config.playing && sd_config.connected) {
			if (sd_config.Options.EnableRequests) {
				api->ss->EnableRequestsHooked(&sdj_req_iface);
			} else {
				api->ss->EnableRequests(false);
			}
		}
		sprintf(buf, "Requests are now: %s", sd_config.Options.EnableRequests ? "Enabled":"Disabled");
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "autodj-dopromo")) {
		if (sd_config.playing && sd_config.connected) {
			sd_config.force_promo = true;
			sprintf(buf, "I will play a promo next...");
		} else {
			sprintf(buf, "I am not the current DJ!");
		}
		ut->std_reply(ut, buf);
		return 1;
	}


	if (!stricmp(command, "songby") && (!parms || !strlen(parms))) {
		if (!sd_config.playing || !sd_config.connected || !sd_config.Options.EnableRequests || !sd_config.Options.EnableFind) {
			return 0;
		}
		ut->std_reply(ut, "Usage: !songby artist");
		return 1;
	}

	if (!stricmp(command,"songby")) {
		if (!sd_config.playing || !sd_config.connected || !sd_config.Options.EnableRequests || !sd_config.Options.EnableFind) {
			return 0;
		}

		if (strchr(parms,'/') || strchr(parms,'\\')) {
			ut->std_reply(ut, "Sorry, I could not find that file in my playlist!");
			return 1;
		}

		LockMutexPtr(QueueMutex);

		QUEUE_FIND_RESULT * q = sd_config.Queue->FindByMeta(parms, META_ARTIST);
		QUEUE * Found = NULL;
		if (q && q->num) {
			int tries=0;
			while (!Found && tries < 30) {
				Found = q->results[SimpleDJ_Rand()%q->num];
				tries++;
			}
		}
		if (!Found) {
			if (q) { sd_config.Queue->FreeFindResult(q); }
			sprintf(buf,"Sorry, I could not find that file in my playlist!");
			ut->std_reply(ut, buf);
			RelMutexPtr(QueueMutex);
			return 1;
		}

		Found = CopyQueue(Found);
		if (q) { sd_config.Queue->FreeFindResult(q); }

		QUEUE * Scan=NULL;
		if (!(ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
			Scan=rQue;
			while(Scan != NULL) {
				if (Scan->ID == Found->ID) {
					ut->std_reply(ut, "That file has already been requested, it should play soon...");
					FreeQueue(Found);
					RelMutexPtr(QueueMutex);
					return 1;
				}
				Scan = Scan->Next;
			}
		}

		Scan = Found;
		REQ_INFO *req = (REQ_INFO *)zmalloc(sizeof(REQ_INFO));
		if (ut->NetNo >= 0) {
			req->netno=ut->NetNo;
		}
		if (ut->Channel) {
			sstrcpy(req->channel,ut->Channel);
		}
		sstrcpy(req->nick,ut->Nick);
		Scan->isreq = req;
		Scan->Next = NULL;

		//QUEUE *Scan2 = rQue;
		//int num=0;
		if (AllowRequest(Scan) || (ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
			int num = AddRequest(Scan);
			sprintf(buf,"Thank you for your request. I have selected %s by %s. It will play after %d more song(s)...",Scan->meta->title,Scan->meta->artist,num);
		} else {
			sprintf(buf,"Sorry, that song has already been requested recently...");
			FreeQueue(Found);
		}
		ut->std_reply(ut, buf);

		RelMutexPtr(QueueMutex);
		return 1;
	}

	if ((!stricmp(command, "relay") || !stricmp(command, "autodj-relay")) && (!parms || !strlen(parms))) {
		if (!sd_config.playing || !sd_config.connected) {
			return 0;
		}

		sprintf(buf, _("Usage: %s /path/to/file"), command);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "relay") || !stricmp(command, "autodj-relay")) {
		if (!sd_config.playing || !sd_config.connected) {
			return 0;
		}

		QUEUE * ret = (QUEUE *)zmalloc(sizeof(QUEUE));
		memset(ret,0,sizeof(QUEUE));
		ret->fn = zstrdup(parms);
		ret->path = zstrdup("");

		TITLE_DATA * td = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
		if (ReadMetaData(buf2, td) == 1) {
			ret->meta = td;
		} else {
			zfree(td);
		}

		LockMutexPtr(QueueMutex);
		AddRequest(ret, true);
		RelMutexPtr(QueueMutex);

		sprintf(buf, "Set to relay '%s' after song ends...",parms);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"countdown") || !stricmp(command,"autodj-countdown")) {
		if (!sd_config.playing || !sd_config.connected || !sd_config.Options.EnableRequests) {
			return 0;
		}
		REPORT rep;
		memset(&rep, 0, sizeof(rep));
		sstrcpy(rep.nick, ut->Nick);
		rep.type = REP_TYPE_PRESENCE;
		rep.pres = ut;
		RefUser(ut);
		LockMutexPtr(QueueMutex);
		countdowns.push_back(rep);
		RelMutexPtr(QueueMutex);
		ut->std_reply(ut, "I will count down the current song for you...");
		return 1;
	}

	return 0;
}

int remote(USER_PRESENCE * user, unsigned char cliversion, REMOTE_HEADER * head, char * data) {
	char buf[1024];
	T_SOCKET * socket = user->Sock;

	if (head->ccmd == RCMD_SRC_COUNTDOWN && (user->Flags & UFLAG_BASIC_SOURCE)) {
		if (sd_config.playing && sd_config.connected) {
			sstrcpy(buf,_("Stopping after current song..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(socket,head,buf);

			RefUser(user);
			sd_config.repto = user;
			sd_config.countdown=true;
		} else {
			sstrcpy(buf,_("SimpleDJ is not currently playing..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(socket,head,buf);
		}
		return 1;
	}

	if (head->ccmd == RCMD_SRC_FORCE_OFF && (user->Flags & UFLAG_ADVANCED_SOURCE)) {
		if (sd_config.playing) {
			sstrcpy(buf,_("Stopping immediately..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(socket,head,buf);
			api->SendMessage(-1, SOURCE_C_STOP, NULL, 0);
		} else {
			sstrcpy(buf,_("SimpleDJ is not currently playing..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(socket,head,buf);
		}
		return 1;
	}

	if (head->ccmd == RCMD_SRC_FORCE_ON && (user->Flags & UFLAG_BASIC_SOURCE)) {
		sstrcpy(buf,_("Playing..."));
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(socket,head,buf);
		api->SendMessage(-1, SOURCE_C_PLAY, NULL, 0);
		sd_config.playing=true;
		return 1;
	}

	if (head->ccmd == RCMD_SRC_NEXT && (user->Flags & UFLAG_ADVANCED_SOURCE)) {
		sstrcpy(buf,_("Skipping song..."));
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(socket,head,buf);
		api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_RELOAD && (user->Flags & UFLAG_ADVANCED_SOURCE)) {
		sstrcpy(buf,_("Reloading SimpleDJ..."));
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(socket,head,buf);

		LoadSchedule();
		sd_config.nofiles = sd_config.Queue->QueueContentFiles();
		QueuePromoFiles(sd_config.Server.PromoDir);
		prCur=prFirst;

		//api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_STATUS) {
		if (sd_config.playing && sd_config.connected) {
			sstrcpy(buf, "playing");
		} else if (sd_config.playing) {
			sstrcpy(buf, "connecting");
		} else {
			sstrcpy(buf, "stopped");
		}
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(socket,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_GET_NAME) {
		if (sd_config.playing && sd_config.connected) {
			sstrcpy(buf, "simpledj");
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(socket,head,buf);
		}
		return 1;
	}

	if (head->ccmd == RCMD_SRC_GET_SONG && (user->Flags & UFLAG_ADVANCED_SOURCE)) {
		LockMutexPtr(QueueMutex);
		if (sd_config.connected && sd_config.playing && sd_config.cursong) {
			head->scmd = RCMD_GENERIC_MSG;
			strlcpy(buf, sd_config.cursong->path, sizeof(buf));
			strlcat(buf, sd_config.cursong->fn, sizeof(buf));
		} else {
			head->scmd = RCMD_GENERIC_ERROR;
			sstrcpy(buf,_("Error: I'm not currently playing any song"));
		}
		RelMutexPtr(QueueMutex);
		head->datalen = strlen(buf);
		api->SendRemoteReply(socket,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_RELAY && (user->Flags & (UFLAG_MASTER|UFLAG_OP))) {
		if (!sd_config.playing || !sd_config.connected || data == NULL || data[0] == 0) {
			return 0;
		}
		QUEUE * ret = znew(QUEUE);
		memset(ret, 0, sizeof(QUEUE));
		ret->fn = zstrdup(data);
		ret->path = zstrdup("");

		LockMutexPtr(QueueMutex);
		AddRequest(ret, true);
		RelMutexPtr(QueueMutex);

		sprintf(buf, _("Set to relay '%s' after song ends..."),data);
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(user->Sock,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_RATE_SONG && (user->Flags & UFLAG_ADVANCED_SOURCE)) {
		head->scmd = RCMD_GENERIC_ERROR;
		if (api->ss->AreRatingsEnabled()) {
			char * p2 = NULL;
			char * nick = strtok_r(data, "\xFE", &p2);
			if (nick) {
				char * srating = strtok_r(NULL, "\xFE", &p2);
				if (srating) {
					uint32 rating = atoul(srating);
					if (rating >= 0 && rating <= api->ss->GetMaxRating()) {
						char * fn = strtok_r(NULL, "\xFE", &p2);
						if (fn) {
							LockMutexPtr(QueueMutex);
							QUEUE * song = sd_config.Queue->FindFile(fn);
							if (song) {
								api->ss->RateSongByID(song->ID, nick, rating);
								strcpy(buf, "Thank you for your song rating.");
								head->scmd = RCMD_GENERIC_MSG;
								FreeQueue(song);
							} else {
								sstrcpy(buf, "Error: I could not find that file in my playlist!");
							}
							RelMutexPtr(QueueMutex);
						} else {
							sstrcpy(buf, "Error: Data should be nick\\xFErating\\xFEfilename");
						}
					} else {
						snprintf(buf, sizeof(buf), "Error: Rating should be a string representing 0-%d", api->ss->GetMaxRating());
					}
				} else {
					sstrcpy(buf, "Error: Data should be nick\\xFErating\\xFEfilename");
				}
			} else {
				sstrcpy(buf, "Error: Data should be nick\\xFErating\\xFEfilename");
			}
		} else {
			sstrcpy(buf, "Error: Song Ratings are not enabled in RadioBot!");
		}
		head->datalen = strlen(buf);
		api->SendRemoteReply(socket,head,buf);
	}

	return 0;
}

bool ShouldIStop() {
	if (sd_config.shutdown_now) { return true; }
	if (sd_config.playing == false) { return true; }
	if (sd_config.next == true) {
		sd_config.next = false;
		return true;
	}
	return false;
}

typedef std::map<uint32, time_t> RequestListType;
RequestListType requestList;

bool AllowRequest(QUEUE * Scan) {
	if (QueueMutex->LockingThread() != GetCurrentThreadId()) {
		api->ib_printf("SimpleDJ -> QueueMutex should be locked when calling AllowRequest!\n");
	}
	if (sd_config.Options.MinReqTimePerSong == 0) { return true; }
	RequestListType::iterator s = requestList.begin();
	while (s != requestList.end()) {
		time_t tmp = s->second;
		if (tmp <= (time(NULL)-sd_config.Options.MinReqTimePerSong)) {
			requestList.erase(s);
			s = requestList.begin();
		} else {
			s++;
		}
	}

	RequestListType::const_iterator x = requestList.find(Scan->ID);
	if (x != requestList.end()) {
		return false;
	}

	return true;
}

int AddRequest(QUEUE * Scan, bool to_front) {
	if (QueueMutex->LockingThread() != GetCurrentThreadId()) {
		api->ib_printf("SimpleDJ -> QueueMutex should be locked when calling AddRequest!\n");
	}

	int ret=0;
	Scan->Next = NULL;
	if (rQue == NULL) {
		rQue = Scan;
		Scan->Prev=NULL;
	} else {
		ret++;
		if (to_front) {
			Scan->Next = rQue;
			rQue->Prev = Scan;
			rQue = Scan;
			Scan->Prev = NULL;
		} else {
			QUEUE *Scan2 = rQue;
			while (Scan2->Next != NULL) {
				ret++;
				Scan2=Scan2->Next;
			}
			Scan2->Next=Scan;
			Scan->Prev = Scan2;
		}
	}

	requestList[Scan->ID] = time(NULL);

	if (sd_config.Queue->on_song_request) { sd_config.Queue->on_song_request(Scan); }
	return ret;
}

QUEUE * GetNextSong() {
	QUEUE * ret = NULL;

	// Promo System
	LockMutexPtr(QueueMutex);
	if (sd_config.Options.DoPromos > 0 && sd_config.nopromos && !sd_config.cur_promo && (numplays % sd_config.Options.DoPromos) == 0) {
		sd_config.cur_promo = sd_config.Options.NumPromos;
		numplays++;
	}
	if (sd_config.force_promo && sd_config.nopromos) {
		sd_config.cur_promo = 1;
		sd_config.force_promo = false;
		numplays++;
	}
	if (sd_config.cur_promo > 0) {
		if (prFirst == NULL) {
			RelMutexPtr(QueueMutex);
			QueuePromoFiles(sd_config.Server.PromoDir);
			LockMutexPtr(QueueMutex);
		}
		if (prFirst != NULL) {
			ret = prFirst;
			DRIFT_DIGITAL_SIGNATURE();
			prFirst=prFirst->Next;
			if (prFirst) {
				prFirst->Prev=NULL;
			} else {
				prLast=NULL;
			}
			RelMutexPtr(QueueMutex);
			sd_config.cur_promo--;
			api->ib_printf("SimpleDJ -> Playing promo %d of %d: %s\n",sd_config.cur_promo,sd_config.Options.NumPromos,ret->fn);
			return ret;
		}
		api->ib_printf("SimpleDJ -> Error playing promos, no promos found!\n");
		sd_config.cur_promo=0;
	}

	// Requested song(s)
	if (rQue) {
		ret = rQue;
		rQue = rQue->Next;
		api->ib_printf("SimpleDJ -> Playing request: %s\n",ret->fn);
	}

	if (!ret) {
		ret = sd_config.Queue->GetRandomFile();
	}
	if (numplays != 4000000) {
		numplays++;
	} else {
		numplays=1;
	}
	RelMutexPtr(QueueMutex);
	return ret;
}

DECODER * GetFileDecoder(const char * fn) {
	for (int dsel=0; sd_config.Decoders[dsel] != NULL; dsel++) {
		if (sd_config.Decoders[dsel]->is_my_file(fn, NULL)) {
			return sd_config.Decoders[dsel];
		}
	}
	DRIFT_DIGITAL_SIGNATURE();
	return NULL;
}

int PlaySongToSock(const char * buf, TITLE_DATA * meta=NULL, int64 fromPos=0) {
	char buf2[1024];
	sprintf(buf2,"SimpleDJ -> Current File: '%s'\n",buf);
	if (api->irc) { api->irc->LogToChan(buf2); }
	api->ib_printf(buf2);

	sd_config.resumePos = 0;
	READER_HANDLE * fp = OpenFile(buf, "rb");

	for (int dsel=0; sd_config.Decoders[dsel] != NULL; dsel++) {
		if (fp && sd_config.Decoders[dsel]->is_my_file(buf, fp ? fp->mime_type : NULL)) {
			if (fp) { fp->seek(fp, 0, SEEK_SET); }

			if (sd_config.Decoders[dsel]->create) {
				SimpleDJ_Decoder * dec = sd_config.Decoders[dsel]->create();
				if (dec->Open(fp, meta, fromPos) || dec->Open_URL(buf, meta, fromPos)) {
					int ret = dec->Decode();
					while (ret == SD_DECODE_CONTINUE) {
						ret = dec->Decode();
					}
					if (ret == SD_DECODE_ERROR) {
						sd_config.resumePos = dec->GetPosition();
					}
					dec->Close();
					sd_config.Decoders[dsel]->destroy(dec);
					fp->close(fp);
					return ret;
				}
				sd_config.Decoders[dsel]->destroy(dec);
			}

			/*
			if (fp && sd_config.Decoders[dsel]->open && sd_config.Decoders[dsel]->open(fp,meta)) {
				int ret = sd_config.Decoders[dsel]->decode();
				while (ret == SD_DECODE_CONTINUE) {
					ret = sd_config.Decoders[dsel]->decode();
				}
				sd_config.Decoders[dsel]->close();
				fp->close(fp);
				return ret;
			} else if (sd_config.Decoders[dsel]->open_url && sd_config.Decoders[dsel]->open_url(buf,meta)) {
				int ret = sd_config.Decoders[dsel]->decode();
				while (ret == SD_DECODE_CONTINUE) {
					ret = sd_config.Decoders[dsel]->decode();
				}
				sd_config.Decoders[dsel]->close();
				if (fp) { fp->close(fp); }
				return ret;
			} else {
				if (sd_config.Decoders[dsel]->close != NULL) { sd_config.Decoders[dsel]->close(); }
			}
			*/
			sprintf(buf2,"SimpleDJ -> Error opening decoder for '%s'\n",buf);
			if (api->irc) { api->irc->LogToChan(buf2); }
			api->ib_printf(buf2);
		}
	}

	if (fp) { fp->close(fp); }
	sprintf(buf2,"SimpleDJ -> Could not find decoder for '%s'\n",buf);
	if (api->irc) { api->irc->LogToChan(buf2); }
	api->ib_printf(buf2);
	return SD_DECODE_DONE;
}

bool ConnectFeeders() {
	API_SS ss;
	bool ret = true;
	for (int j=0; j < MAX_SOUND_SERVERS; j++) {
		if (sd_config.Servers[j].Enabled) {
			api->ss->GetSSInfo(j,&ss);
			if (ss.type == SS_TYPE_ICECAST && !strlen(sd_config.Servers[j].Mount)) {
				sstrcpy(sd_config.Servers[j].Mount, ss.mount);
				if (!strlen(sd_config.Servers[j].Mount)) {
					api->ib_printf("SimpleDJ -> Error: No Mountpoint specified for icecast! Falling back to /\n");
					strcpy(sd_config.Servers[j].Mount, "/");
				}
			}
			if (!sd_config.Servers[j].Feeder) {
				for (int i=0; feeders[i].create != NULL; i++) {
					if (ss.type == feeders[i].type) {
						sd_config.Servers[j].SelFeeder = &feeders[i];
						sd_config.Servers[j].Feeder = feeders[i].create();
						if (sd_config.Servers[j].Feeder) {
							sd_config.Servers[j].Feeder->SetNum(j);
						}
						break;
					}
				}
			}
			if (sd_config.Servers[j].Feeder) {
				if (!sd_config.Servers[j].Feeder->IsConnected()) {
					if (!sd_config.Servers[j].Feeder->Connect()) { ret = false; }
				}
			}
		}
	}
	return ret;
}

void DisconnectFeeders() {
	for (int j=0; j < MAX_SOUND_SERVERS; j++) {
		if (sd_config.Servers[j].Enabled && sd_config.Servers[j].Feeder) {
			if (sd_config.Servers[j].Feeder->IsConnected()) {
				sd_config.Servers[j].Feeder->Close();
			}
		}
	}
}

THREADTYPE PlayThread(VOID *lpData) {
	TT_THREAD_START

	char buf[1024*2],buf2[1024*2];

//	printf("SimpleDJ -> Play Thread Started\n");
	#if defined(WIN32)
	bool have_com = true;
	if (FAILED(CoInitialize(NULL))) {
		have_com = false;
	    api->ib_printf("Error intiliazing COM!\n");
    }
	#else
	signal(SIGSEGV,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	#endif

	for (;!sd_config.shutdown_now;) {
		api->safe_sleep_seconds(1);
		if (sd_config.shutdown_now) { break; }
		api->safe_sleep_seconds(1);
		if (sd_config.shutdown_now) { break; }
		api->safe_sleep_seconds(1);
		if (sd_config.shutdown_now) { break; }
		api->safe_sleep_seconds(1);
		if (sd_config.shutdown_now) { break; }

		if (sd_config.playagainat > 0) {
			if (sd_config.playagainat < time(NULL)) {
				sd_config.playing=true;
				sd_config.playagainat=0;
			}
		}

		if (sd_config.playing && !sd_config.shutdown_now && sd_config.nofiles > 0) {

			api->ib_printf("SimpleDJ -> Attempting to connect to Sound Server(s)...\n");

			if (ConnectFeeders()) {
				sd_config.connected = true;
				api->SendMessage(-1, SOURCE_I_PLAYING, NULL, 0);
				api->ss->EnableRequestsHooked(&sdj_req_iface);

				while (sd_config.connected && sd_config.playing) {
					QUEUE * que = NULL;
					if (sd_config.resumeSong) {
						que = sd_config.resumeSong;
						sd_config.resumeSong = NULL;
					} else {
						que = GetNextSong();
					}
					if (que == NULL) {
						// this is a situation that will not be resolved without an autodj-reload
						// so not going to set playagainat
						api->ib_printf("SimpleDJ -> I can not find any more files to play!\n");
						sd_config.next = false;
						sd_config.move = false;
						sd_config.playagainat = 0;
						sd_config.playing = false;
						break;
					}
					LockMutexPtr(QueueMutex);
					sd_config.cursong = que;
					RelMutexPtr(QueueMutex);

					sprintf(buf,"%s%s",que->path,que->fn);

					char *msgbuf = NULL;
					char *msgbuf2 = NULL;
					if (que->isreq != NULL && api->irc) {
						msgbuf = (char *)zmalloc(8192);
						msgbuf2 = (char *)zmalloc(8192);
						if (api->LoadMessage("ReqToChans",msgbuf, 8192)) {
							str_replace(msgbuf,8192,"%request",que->isreq->nick);
							api->ProcText(msgbuf,8192);
							sprintf(msgbuf2,"PRIVMSG %s :%s\r\n",que->isreq->channel,msgbuf);
							api->irc->SendIRC(que->isreq->netno,msgbuf2,strlen(msgbuf2));
						}
						zfree(msgbuf2);
						zfree(msgbuf);
						msgbuf = NULL;
						msgbuf2 = NULL;
					}

					if (sd_config.Voice.EnableVoice) {
						msgbuf = (char *)zmalloc(8192);
						msgbuf2 = (char *)zmalloc(8192);

						TITLE_DATA td;
						memset(&td, 0, sizeof(td));

						if (que->meta && strlen(que->meta->title)) {
							if (strlen(que->meta->artist)) {
								sprintf(msgbuf2, "%s by %s", que->meta->title, que->meta->artist);
							} else {
								strlcpy(msgbuf2, que->meta->title, 8192);
							}
						} else {
							strlcpy(msgbuf2, que->fn, 8192);
							char *ep = strrchr(msgbuf2, '.');
							if (ep) { ep[0] = 0; }
						}
						str_replace(msgbuf2, 8192, "_", " ");
						str_replace(msgbuf2, 8192, "-", ".");
						str_replace(msgbuf2, 8192, "[", "");
						str_replace(msgbuf2, 8192, "]", "");
						str_replace(msgbuf2, 8192, "(", "");
						str_replace(msgbuf2, 8192, ")", "");
						str_replace(msgbuf2, 8192, "feat.", "featuring");
						str_replace(msgbuf2, 8192, "Feat.", "featuring");
						str_replace(msgbuf2, 8192, "ft.", "featuring");
						str_replace(msgbuf2, 8192, "Ft.", "featuring");
						str_replace(msgbuf2, 8192, "pt.", "part");
						str_replace(msgbuf2, 8192, "Pt.", "part");
						while (strstr(msgbuf2, "  ")) {
							str_replace(msgbuf2, 8192, "  ", " ");
						}

						if (sd_config.Voice.NumMessages > 0) {
							int n = rand()%sd_config.Voice.NumMessages;
							if (n < 0) { n = 0; }
							if (n >= sd_config.Voice.NumMessages) { n = sd_config.Voice.NumMessages - 1; }
							strlcpy(msgbuf, sd_config.Voice.Messages[n], 8192);
							str_replace(msgbuf, 8192, "%song", msgbuf2);
						} else {
							switch(rand()%8) {
								case 0:
									sprintf(msgbuf, "You are listening to Shout I R C Radio. Coming up next: %s", msgbuf2);
									break;
								case 1:
									sprintf(msgbuf, "This is I R C Bot, the Auto DJ with Style. Coming up next: %s", msgbuf2);
									break;
								case 2:
									sprintf(msgbuf, "This is I R C Bot spinning. Next up is: %s", msgbuf2);
									break;
								case 3:
									sprintf(msgbuf, "Coming up next: %s", msgbuf2);
									break;
								case 4:
									sprintf(msgbuf, "This is I R C Bot, the Auto DJ with Style. Next up is: %s", msgbuf2);
									break;
								case 5:
									sprintf(msgbuf, "You are listening to Shout I R C Radio. Next up is: %s", msgbuf2);
									break;
								case 6:
									sprintf(msgbuf, "This is I R C Bot spinning. Coming up next: %s", msgbuf2);
									break;
								default:
									sprintf(msgbuf, "Now playing: %s", msgbuf2);
									break;
							}
						}

						if (strlen(sd_config.voice_override)) {
							strlcpy(msgbuf,sd_config.voice_override, 8192);
							sd_config.voice_override[0] = 0;
						}

						if (api->irc) { api->irc->LogToChan(msgbuf); }

						if (que->isreq) {
							if ((strlen(msgbuf)+strlen("\nThis song was requested by ")+strlen(que->isreq->nick)) < 8192) {
								strlcat(msgbuf, "\nThis song was requested by ", 8192);
								strlcat(msgbuf, que->isreq->nick, 8192);
							}
						}

						char * txtfn = zmprintf("%sautodj_script.txt", api->GetBasePath());
						FILE * fp = fopen(txtfn, "wb");
						zfree(txtfn);
						if (fp) {
							fwrite(msgbuf, strlen(msgbuf), 1, fp);
							fclose(fp);

							TITLE_DATA vd;
							memset(&vd, 0, sizeof(vd));
							sstrcpy(vd.title,  sd_config.Voice.Title);
							sstrcpy(vd.artist, sd_config.Voice.Artist);

							TTS_JOB job;
							memset(&job, 0, sizeof(job));
							job.inputFN = zmprintf("%sautodj_script.txt", api->GetBasePath());
							int ret = SD_DECODE_DONE;
							if (GetFileDecoder("autodj_script.wav") != NULL) {
								job.outputFN = zmprintf("%sautodj_script.wav", api->GetBasePath());
								if (sd_config.tts.MakeWAVFromFile(&job)) {
									ret = PlaySongToSock(job.outputFN, &vd);
									remove(job.outputFN);
								}
							} else {
								job.bitrate = sd_config.Servers[0].Bitrate;
								job.channels = sd_config.Servers[0].Channels;
								job.sample = sd_config.Servers[0].Sample;
								job.outputFN = zmprintf("%sautodj_script.mp3", api->GetBasePath());
								if (sd_config.tts.MakeMP3FromFile(&job)) {
									ret = PlaySongToSock(job.outputFN, &vd);
									remove(job.outputFN);
								}
							}
							zfreenn((void *)job.inputFN);
							zfreenn((void *)job.outputFN);
							if (ret == SD_DECODE_ERROR) {
								sprintf(buf, "SimpleDJ -> Decode error, reconnecting to SS and reinitializing encoder/feeder...");
								if (api->irc) { api->irc->LogToChan(buf); }
								api->ib_printf("SimpleDJ -> Decode error, reconnecting to SS and reinitializing encoder/feeder...\n");
								sd_config.playing = false;
								sd_config.playagainat = time(NULL) + 1;
								LockMutexPtr(QueueMutex);
								sd_config.cursong = NULL;
								RelMutexPtr(QueueMutex);
								FreeQueue(que);
								zfree(msgbuf);
								zfree(msgbuf2);
								break;
							}
						}
						zfree(msgbuf);
						zfree(msgbuf2);
					}

					if (sd_config.Queue->on_song_play) { sd_config.Queue->on_song_play(que); }
					int ret = PlaySongToSock(buf,que->meta,sd_config.resumePos);
					if (sd_config.Queue->on_song_play_over) { sd_config.Queue->on_song_play_over(que); }

					DRIFT_DIGITAL_SIGNATURE();
					if (ret == SD_DECODE_ERROR) {
						if (sd_config.resumePos > 0) {
							sd_config.resumeSong = que;
						}
						sprintf(buf,"SimpleDJ -> Decode error, reconnecting to SS and reinitializing encoder/feeder...");
						if (api->irc) { api->irc->LogToChan(buf); }
						api->ib_printf("SimpleDJ -> Decode error, reconnecting to SS and reinitializing encoder/feeder...\n");
						sd_config.playing = false;
						sd_config.move = false;
						sd_config.playagainat = time(NULL) + 1;
					}

					if (sd_config.move) {
						sprintf(buf,"%s%s",que->path,que->fn);
						struct stat st;
						if (stat(sd_config.Options.MoveDir, &st) != 0) {
							dsl_mkdir(sd_config.Options.MoveDir, 0777);
						}
						sprintf(buf2,"%s%s%s",sd_config.Options.MoveDir,PATH_SEPS,que->fn);
						int ret = rename(buf,buf2);
						if (ret == 0) {
							sprintf(buf,"File '%s' moved out of main rotation...",que->fn);
							api->ib_printf("File '%s' moved out of main rotation...\n",que->fn);
						} else {
							char * tmp = zstrdup(buf);
							sprintf(buf,"Error moving '%s' out of main rotation! (errno: %d) (%s -> %s)",que->fn,errno,tmp,buf2);
							zfree(tmp);
							api->ib_printf("Error moving '%s' out of main rotation! (errno: %d)\n",que->fn,errno);
						}
						if (api->irc) { api->irc->LogToChan(buf); }
						sd_config.move = false;
					}

					LockMutexPtr(QueueMutex);
					sd_config.cursong = NULL;
					RelMutexPtr(QueueMutex);
					if (!sd_config.resumeSong) {
						FreeQueue(que);
					}
				} // while (sd_config.connected)

				api->SendMessage(-1, SOURCE_I_STOPPING, NULL, 0);

				api->ib_printf("SimpleDJ -> Disconnecting from SS...\n",buf);

				api->ss->EnableRequests(false);
				sd_config.connected=false;
				DisconnectFeeders();
				api->SendMessage(-1, SOURCE_I_STOPPED, NULL, 0);
			} else {
				api->ib_printf("SimpleDJ -> Error connecting to Sound Server as source!\n");
			}
		}
	}

	#if defined(WIN32)
	if (have_com) {
		CoUninitialize();
	}
	#endif
	api->ib_printf("SimpleDJ -> Play Thread Stopped\n");
	TT_THREAD_END
}

int MessageHandler(unsigned int msg, char * data, int datalen) {
	//printf("MessageHandler(%u, %X, %d)\n", msg, data, datalen);

	for (int i = 0; i < sd_config.noplugins; i++) {
		if (sd_config.Plugins[i].plug && sd_config.Plugins[i].plug->message) {
			sd_config.Plugins[i].plug->message(msg, data, datalen);
		}
	}

	switch(msg) {
		case SOURCE_C_PLAY:
			sd_config.playagainat=time(NULL);
			sd_config.repto= NULL;
			sd_config.next = false;
			sd_config.move = false;
			sd_config.countdown=false;
			sd_config.playing=true;
			return 1;
			break;

		case SOURCE_C_NEXT:
			sd_config.next = true;
			//sd_config.move = false;
			return 1;
			break;

		case SOURCE_C_STOP:
			sd_config.playagainat=0;
			sd_config.repto=NULL;
			sd_config.next = false;
			//sd_config.move = false;
			sd_config.countdown=false;
			sd_config.playing=false;
			return 1;
			break;

		case SOURCE_IS_PLAYING:
			if (sd_config.playing && sd_config.connected) {
				return 1;
			}
			break;

		case AUTODJ_IS_LOADED:
			return 1;
			break;

		case SOURCE_GET_FILE_PATH:
		case AUTODJ_GET_FILE_PATH:
			if (strlen(data)) {
				LockMutexPtr(QueueMutex);
				QUEUE * Scan=NULL;
				Scan = sd_config.Queue->FindFile(data);
				RelMutexPtr(QueueMutex);
				int ret = 0;
				if (Scan) {
					ret = 1;
					memset(data, 0, datalen);
					snprintf(data, datalen, "%s%s", Scan->path, Scan->fn);
					FreeQueue(Scan);
				}
				return ret;
			}
			break;

		case SOURCE_GET_SONGINFO:{
				LockMutexPtr(QueueMutex);
				IBM_SONGINFO * si = (IBM_SONGINFO *)data;
				if (sd_config.playing && sd_config.connected && sd_config.cursong) {
					snprintf(si->fn, sizeof(si->fn), "%s%s", sd_config.cursong->path, sd_config.cursong->fn);
					si->songLen = sd_config.cursong->songlen;
					si->file_id = sd_config.cursong->ID;
					if (sd_config.cursong->meta) {
						strlcpy(si->title, sd_config.cursong->meta->title, sizeof(si->title));
						strlcpy(si->artist, sd_config.cursong->meta->artist, sizeof(si->artist));
						strlcpy(si->album, sd_config.cursong->meta->album, sizeof(si->album));
						strlcpy(si->genre, sd_config.cursong->meta->genre, sizeof(si->genre));
					}
					si->is_request = sd_config.cursong->isreq ? true:false;
					if (sd_config.cursong->isreq && sd_config.cursong->isreq->nick[0]) {
						sstrcpy(si->requested_by, sd_config.cursong->isreq->nick);
					}
					RelMutexPtr(QueueMutex);
					return 1;
				}
				RelMutexPtr(QueueMutex);
			}
			break;

		case SOURCE_GET_SONGID:{
				LockMutexPtr(QueueMutex);
				if (sd_config.playing && sd_config.connected && sd_config.cursong) {
					*((uint32 *)data) = sd_config.cursong->ID;
					RelMutexPtr(QueueMutex);
					return 1;
				}
				RelMutexPtr(QueueMutex);
			}
			break;

		case IB_ON_SONG_RATING:{
				IBM_RATING * ir = (IBM_RATING *)data;
				LockMutexPtr(QueueMutex);
				if (ir->AutoDJ_ID && sd_config.Queue && sd_config.Queue->on_song_rating) {
					sd_config.Queue->on_song_rating(ir->AutoDJ_ID, ir->rating, ir->votes);
				}
				RelMutexPtr(QueueMutex);
			}
			break;

		case IB_PROCTEXT:{
				IBM_PROCTEXT * pt = (IBM_PROCTEXT *)data;
				bool done = false;
				if (TryLockMutexPtr(QueueMutex, 10000)) {
					char buf[128];
					if (sd_config.connected && sd_config.playing && sd_config.cursong) {
						sprintf(buf, "%d", GetTimeLeft(false));
						str_replace(pt->buf, pt->bufSize, "%timeleft_milli%", buf);
						sprintf(buf, "%d", GetTimeLeft(true));
						str_replace(pt->buf, pt->bufSize, "%timeleft_secs%", buf);
						done = true;
					}
					RelMutexPtr(QueueMutex);
				}
				if (!done) {
					str_replace(pt->buf, pt->bufSize, "%timeleft_milli%", "0");
					str_replace(pt->buf, pt->bufSize, "%timeleft_secs%", "0");
				}
				return 0;
			}
			break;

		case IB_SS_DRAGCOMPLETE:{
				STATS * s = api->ss->GetStreamInfo();
				static time_t lastOffline = 0;
				if (sd_config.Options.AutoPlayIfNoSource && !s->online && !sd_config.playing && time(NULL) >= sd_config.playagainat) {
					if (lastOffline == 0) {
						lastOffline = time(NULL);
					} else if ((time(NULL) - lastOffline) > sd_config.Options.AutoPlayIfNoSource){
						api->ib_printf(_("SimpleDJ -> Auto-starting, stream has no source...\n"));
						if (api->irc) { api->irc->LogToChan(_("SimpleDJ -> Auto-starting, stream has no source...")); }
						sd_config.playing = true;
					}
				} else if (s->online) {
					lastOffline = 0;
				}
			 }
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{9E8E8536-0142-4950-9892-F14FCCB1AF65}",
	"SimpleDJ",
	"SimpleDJ Plugin " SIMPLEDJ_VERSION "",
	"Drift Solutions",
	init,
	quit,
	MessageHandler,
	NULL,
	NULL,
	NULL,
	remote,
};

PLUGIN_EXPORT_FULL

DL_HANDLE getInstance() {
	return plugin.hInstance;
}
