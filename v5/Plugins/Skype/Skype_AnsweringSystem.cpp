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

#include "skype_plugin.h"

typedef std::map<int32, SKYPE_CALL *> callListType;
callListType callList;

//#define SKYPE_MP3_BUFFER_SIZE 524288

struct buffer {
  unsigned char const *start;
  unsigned long length;
  SKYPE_CALL * call;
  T_SOCKET * sock;
  FILE * fp;
  //AutoDJ_Decoder * dec;
};

//	int n = socks.Send(buf->sock, (char *)&samples[0], pcm->length * sizeof(short));
/*
	if (!buf->call->active || n <= 0 || buf->call->keyPress != 0) {
		return MAD_FLOW_STOP;
	}
	*/

bool PlayMP3ToSocket(SKYPE_CALL * call, T_SOCKET * sock, const char * fn) {
	int err = 0;
	mpg123_handle * mh = mpg123_new(NULL, &err);
	if (mh == NULL) {
		api->ib_printf("Skype -> Error creating audio handle: %s!\n", mpg123_plain_strerror(err));
		return false;
	}

#if defined(MPG123_SKIP_ID3V2)
	mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_SKIP_ID3V2, 0.)
#endif
	if (mpg123_open(mh, (char *)fn) != MPG123_OK) {
		api->ib_printf("Skype -> Error creating audio handle: %s!\n", mpg123_strerror(mh));
		mpg123_delete(mh);
		mh = NULL;
		return false;
	}

	mpg123_format_none(mh);
	mpg123_format(mh, 16000, MPG123_MONO, MPG123_ENC_SIGNED_16);

	long rate;
	int channels,encoding;
	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK || encoding != MPG123_ENC_SIGNED_16 || rate != 16000 || channels != MPG123_MONO) {
		api->ib_printf("Skype -> Skype only supports 16-bit samples, 16kHz samplerate, mono!\n");
		mpg123_close(mh);
		mpg123_delete(mh);
		mh = NULL;
		return false;
	}

	int read_size = mpg123_outblock(mh);
	char * buffer = (char *)zmalloc(read_size);
	//double per = mpg123_tpf(mh);
	size_t done;
	while (1) {
		int n = mpg123_read(mh, (unsigned char *)buffer, read_size, &done);
		if (n == MPG123_NEW_FORMAT) {
			if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK || encoding != MPG123_ENC_SIGNED_16 || rate != 16000 || channels != MPG123_MONO) {
				api->ib_printf(_("Skype -> MP3 parameter change: %d/%d/%d!\n"), channels, rate, encoding);
				break;
			}
		} else if (n == MPG123_OK) {
			if (socks.Send(sock, buffer, done) != done || !call->active || call->keyPress != 0) {
				break;
			}
		} else if (n == MPG123_DONE || n == MPG123_NEED_MORE) {
			break;
		} else {
			api->ib_printf(_("Skype -> Decoder returned: %d (err: %s)\n"), n, mpg123_strerror(mh));
			break;
		}
	}

	zfree(buffer);
	mpg123_close(mh);
	mpg123_delete(mh);
	return true;
}

bool SpeakToSocket(SKYPE_CALL * call, T_SOCKET * sock, const char * txt) {
	TTS_JOB job;
	memset(&job, 0, sizeof(job));
	job.text = txt;
	job.outputFN = "./tmp/skype.mp3";
	job.channels = 1;
	job.sample = 16000;

	if (skype_config.tts.MakeMP3FromText(&job)) {
		return PlayMP3ToSocket(call, sock, job.outputFN);
	}

	return false;
}

T_SOCKET * AcceptSock(T_SOCKET * lSock) {
	T_SOCKET * ret = NULL;
	api->ib_printf(_("Skype -> Waiting for incoming connection...\n"));
	if (socks.Select_Read(lSock, 10000) == 1) {
		api->ib_printf(_("Skype -> Socket accepted...\n"));
		ret = socks.Accept(lSock);
	}
	return ret;
}

THREADTYPE CallThreadOut(void * lpData) {
	TT_THREAD_START
	SKYPE_CALL * call = (SKYPE_CALL *)tt->parm;

	T_SOCKET * sock = AcceptSock(call->outSock);

	char buf[16000*1*sizeof(short)];
	while (sock && call->active) {
		if (socks.Select_Read(sock, 1000) == 1) {
			int n = socks.Recv(sock, buf, sizeof(buf));
			if (n > 0) {
				//api->ib_printf("Received %d bytes on call %d\n", n, call->id);
			} else if (n == 0) {
				api->ib_printf(_("Call %d outsocket closed\n"), call->id);
				socks.Close(sock);
				sock = NULL;
			} else {
				api->ib_printf(_("Call %d outsocket error: %s\n"), call->id, socks.GetLastErrorString());
				socks.Close(sock);
				sock = NULL;
			}
		}
	}

	if (sock) {
		socks.Close(sock);
		sock = NULL;
	}

	if (call->active) { HangUpCall(call); }
	TT_THREAD_END
}

THREADTYPE CallThreadIn(void * lpData) {
	TT_THREAD_START
	SKYPE_CALL * call = (SKYPE_CALL *)tt->parm;

	T_SOCKET * sock = AcceptSock(call->inSock);

	char buf[16000*1*sizeof(short)];
	while (sock && call->active) {
		if (strlen(call->TextToSpeak)) {
			if (SpeakToSocket(call, sock, call->TextToSpeak)) {
				sstrcpy(call->LastMessage, call->TextToSpeak);
				memset(call->TextToSpeak,0,sizeof(call->TextToSpeak));
			} else {
				api->ib_printf(_("Error speaking to socket: %s\n"), socks.GetLastErrorString());
				socks.Close(sock);
				sock = NULL;
				break;
			}
		}

		if (call->keyPress == '*') {
			sstrcpy(call->TextToSpeak, call->LastMessage);
			call->keyPress = 0;
		}
		if (call->keyPress == '#') {
			HangUpCall(call);
			call->keyPress = 0;
		}

#if !defined(IRCBOT_LITE)
		if (call->keyPress == '1') {
			STATS * s = api->ss->GetStreamInfo();
			if (s->online) {
				snprintf(call->TextToSpeak, sizeof(call->TextToSpeak), _("%s is currently playing %s"), s->curdj, s->cursong);
			} else {
				strcpy(call->TextToSpeak, _("The radio stream is currently Offline"));
			}
			call->keyPress = 0;
		}

		if (call->keyPress == '2') {
			if (call->uflags & UFLAG_ADVANCED_SOURCE) {
				if (api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0)) {
					strcpy(call->TextToSpeak, _("Skipping to next song"));
				} else {
					strcpy(call->TextToSpeak, _("Error skipping to next song, Auto DJ is not currently playing"));
				}
			} else {
				strcpy(call->TextToSpeak, _("You are not a high enough level to use call transfers"));
			}
			call->keyPress = 0;
		}
#endif

		if (call->keyPress == '3') {
			if (call->uflags & (UFLAG_MASTER|UFLAG_OP)) {
				SpeakToSocket(call, sock, _("Attempting to transfer call"));
				safe_sleep(4);
				sprintf(buf, "GET CALL %d CAN_TRANSFER shoutirc", call->id);
				SkypeAPI_SendCommand(buf);
			} else {
				strcpy(call->TextToSpeak, _("You are not a high enough level to use call transfers"));
			}
			call->keyPress = 0;
		}
//#endif

		safe_sleep(100,true);
	}

	if (sock) {
		socks.Close(sock);
		sock = NULL;
	}

	if (call->active) { HangUpCall(call); }
	TT_THREAD_END
}

SKYPE_CALL * GetCallHandle(int32 id) {
	LockMutex(hMutex);
	callListType::const_iterator x = callList.find(id);
	if (x != callList.end()) {
		RelMutex(hMutex);
		return x->second;
	}
	RelMutex(hMutex);
	return NULL;
}

struct LSOCKET {
	int32 port;
	T_SOCKET * sock;
};

bool CreateListeningSocket(LSOCKET * l) {
	l->sock = socks.Create();
	l->port = 1024;
	while (!socks.Bind(l->sock, l->port) && l->port <= 65535) {
		l->port++;
	}
	if (l->port == 65536) {
		socks.Close(l->sock);
		memset(l,0,sizeof(LSOCKET));
		return false;
	}
	if (!socks.Listen(l->sock)) {
		socks.Close(l->sock);
		memset(l,0,sizeof(LSOCKET));
		return false;
	}
	return true;
}

bool AcceptCall(SKYPE_CALL * call) {
	LockMutex(hMutex);

	LSOCKET l;
	if (!CreateListeningSocket(&l)) {
		RelMutex(hMutex);
		return false;
	}
	call->inSock = l.sock;
	call->inPort = l.port;

	if (!CreateListeningSocket(&l)) {
		socks.Close(call->inSock);
		call->inSock = NULL;
		RelMutex(hMutex);
		return false;
	}
	call->outSock = l.sock;
	call->outPort = l.port;

	TT_StartThread(CallThreadIn, call, _("Call Thread"), call->id);
	TT_StartThread(CallThreadOut, call, _("Call Thread"), call->id);

	char buf[1024];
	sprintf(buf, "ALTER CALL %d SET_INPUT PORT=\"%d\"", call->id, call->inPort);
	SkypeAPI_SendCommand(buf);
	sprintf(buf, "ALTER CALL %d SET_OUTPUT PORT=\"%d\"", call->id, call->outPort);
	SkypeAPI_SendCommand(buf);
	sprintf(buf, "ALTER CALL %d ANSWER", call->id);
	SkypeAPI_SendCommand(buf);
	RelMutex(hMutex);
	return true;
}

SKYPE_CALL * CreateCallHandle(int32 id) {
	LockMutex(hMutex);
	SKYPE_CALL * ret = (SKYPE_CALL *)zmalloc(sizeof(SKYPE_CALL));
	memset(ret, 0, sizeof(SKYPE_CALL));
	ret->id = id;
	strcpy(ret->TextToSpeak, _("Hello, this is I R C Bot. Press # to hang up, or press * to have me repeat my last message."));

	callList[id] = ret;
	ret->active = true;
	ret->uflags = api->user->GetDefaultFlags();

	RelMutex(hMutex);
	return ret;
}

void SetCallDTMF(SKYPE_CALL * call, char button) {
	LockMutex(hMutex);
	call->keyPress = button;
	RelMutex(hMutex);
}

void CloseCallHandle(SKYPE_CALL * ret) {
	LockMutex(hMutex);
	callListType::iterator x = callList.find(ret->id);
	if (x != callList.end()) {
		callList.erase(x);
	}
	ret->active = false;
	while (TT_NumThreadsWithID(ret->id)) {
		safe_sleep(100,true);
	}
	if (ret->inSock) { socks.Close(ret->inSock); }
	if (ret->outSock) { socks.Close(ret->outSock); }
	zfree(ret);
	RelMutex(hMutex);
}

void CloseAllCalls() {
	LockMutex(hMutex);
	if (callList.size()) { api->ib_printf(_("Skype -> Closing all call handles...\n")); }
	for (callListType::const_iterator x = callList.begin(); x != callList.end(); x = callList.begin()) {
		CloseCallHandle(x->second);
	}
	RelMutex(hMutex);
}

void HangUpCall(SKYPE_CALL * call) {
	LockMutex(hMutex);
	char buf[1024];
	call->active = false;
	sprintf(buf, "ALTER CALL %d HANGUP", call->id);
	SkypeAPI_SendCommand(buf);
	RelMutex(hMutex);
}


void HangUpAllCalls() {
	LockMutex(hMutex);
	if (callList.size()) { api->ib_printf(_("Skype -> Hanging up all call handles...\n")); }
	char buf[1024];
	for (callListType::const_iterator x = callList.begin(); x != callList.end(); x++) {
		sprintf(buf, "ALTER CALL %d HANGUP", x->first);
		SkypeAPI_SendCommand(buf);
	}
	RelMutex(hMutex);
}

size_t CallCount() { return callList.size(); }
