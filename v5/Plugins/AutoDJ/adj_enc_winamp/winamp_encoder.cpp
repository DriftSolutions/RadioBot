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

#include "../adj_plugin.h"
#include "enc_if.h"
#include "resource.h"

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin();

// unsigned int GetAudioTypes3(int idx, char *desc);
// AudioCoder *CreateAudio3(int nch, int srate, int bps, unsigned int srct, unsigned int *outt, char *configfile);
// HWND ConfigAudio3(HWND hwndParent, HINSTANCE hinst, unsigned int outt, char *configfile);
//mmioFOURCC('P','C','M','4')

extern "C" {
	typedef unsigned int (*GetAudioTypes3Type)(int idx, char *desc);
	typedef AudioCoder * (*CreateAudio3Type)(int nch, int srate, int bps, unsigned int srct, unsigned int *outt, char *configfile);
	typedef void (*FinishAudio3Type)(char* filename, AudioCoder* ac);
	typedef HWND (*ConfigAudio3Type)(HWND hwndParent, HINSTANCE hinst, unsigned int outt, char *configfile);
}

struct ENCODER_INTERFACE {
	GetAudioTypes3Type GetAudioTypes3;
	CreateAudio3Type CreateAudio3;
	FinishAudio3Type FinishAudio3;
	ConfigAudio3Type ConfigAudio3;
};

AD_PLUGIN_API * adapi=NULL;

DL_HANDLE hDLL=NULL;
char EncoderFourCC[5]={0,0,0,0,0};
int EncoderIndex=0;
ENCODER_INTERFACE encoder;
char config_file[MAX_PATH]={0};

class Winamp_Encoder: public AutoDJ_Encoder {
private:
	int chans;
	AudioCoder * enc;
	bool fake_stereo;
public:
	Winamp_Encoder();
	~Winamp_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

Winamp_Encoder::Winamp_Encoder() {
	chans = 0;
	enc = NULL;
	fake_stereo = false;
}

Winamp_Encoder::~Winamp_Encoder() {
}

bool Winamp_Encoder::Init(int channels, int samplerate, float scale) {
	chans = channels;
	unsigned int in = mmioFOURCC('P','C','M',' ');
	unsigned int out = mmioFOURCC(EncoderFourCC[0],EncoderFourCC[1],EncoderFourCC[2],EncoderFourCC[3]);

	enc = encoder.CreateAudio3(channels, samplerate, 16, in, &out, config_file);
	if (enc) {
		adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Encoding object created\n"));
		return true;
	}

	adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Error creating encoding object!\n"));
	return false;
}

/*
short int2short(int x) {
	double mult = (x < 0) ? -1:1;
	double per = abs(x*2);
	per /= 2147483647;
	return per * 32767 * mult;
}

bool winampenc_encode_short_interleaved(int samples, short buffer[]);
bool winampenc_encode_signed_int(int samples, const int left[], const int right[]) {
	if (enc == NULL) { return true; }
	short * buf = new short[samples*chans];
	int ind=0;
	//int samples = (chans == 1) ? bread/2:bread/4;
	for (int i=0; i < samples; i++) {
			buf[ind] = left[i] * (1.0 / ( 1L << (8 * sizeof(long) - 16)));//int2short(left[i]);
			//32768
			//2147483648
			ind++;
			if (chans == 2) {
				buf[ind] = right[i] * (1.0 / ( 1L << (8 * sizeof(long) - 16)));//int2short(right[i]);
				ind++;
			}
	}

		//for (int i=0; i < samples; i++) {
		//buf[i] = left[i];
	//*chans
	bool ret = winampenc_encode_short_interleaved(samples, buf);
	delete buf;
	//printf("signed int not supported\n");
	return ret;
}

bool winampenc_encode_short(int samples, short left[], short right[]) {
	if (enc == NULL) { return true; }
	short * buf = new short[samples*chans];
	int ind=0;
	//int samples = (chans == 1) ? bread/2:bread/4;
	for (int i=0; i < samples; i++) {
			buf[ind] = left[i];
			ind++;
			if (chans == 2) {
				buf[ind] = right[i];
				ind++;
			}
	}

		//for (int i=0; i < samples; i++) {
		//buf[i] = left[i];
	bool ret = winampenc_encode_short_interleaved(samples, buf);
	delete buf;
	//printf("signed int not supported\n");
	return ret;
}
*/

bool Winamp_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}


bool Winamp_Encoder::Encode(int samples, const short buffer[]) {
	if (enc == NULL) { return true; }
	static unsigned char buf[512*1024];
	int blen = sizeof(short)*samples*chans;
	char * sPtr = (char *)buffer;
	bool ret = true;

	for(;blen > 0 && ret;) {
		int used = 0;
		int len = enc->Encode(0, sPtr, blen, &used, buf, sizeof(buf));

		//int enclen = g->aacpEncoder->Encode(in_used, bufcounter, len, &in_used, outbuffer, sizeof(outbuffer));

		if(len > 0) {
			ret = adapi->GetFeeder()->Send(buf,len);
			if (!ret) { break; }
		} else {
			break;
		}

		if (used > 0) {
			sPtr += used;
			blen -= used;
		}
	}

	/*
	if (blen != used) {
		adapi->botapi->ib_printf("AutoDJ (winamp_encoder) -> Warning: used != blen (used: %d / blen: %d / len: %d)\n", used, blen, len);
	}
	*/

	return ret;
}

void Winamp_Encoder::Close() {
	if (enc) {
		if (encoder.FinishAudio3) { encoder.FinishAudio3("test.bin", enc); }
		delete enc;
		enc = NULL;
	}
}

static char aacp_desc[] = "audio/aacp";
static char mp3_desc[]  = "audio/mpeg";
static char flac_desc[] = "application/x-flac";
static char wma_desc[]  = "audio/x-ms-wma";

AutoDJ_Encoder * winamp_enc_create() { return new Winamp_Encoder; }
void winamp_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<Winamp_Encoder *>(enc);
}

ENCODER winamp_encoder = {
	"winamp",
	"",
	50,

	winamp_enc_create,
	winamp_enc_destroy
};

INT_PTR CALLBACK ConfigProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:{
				unsigned int out = mmioFOURCC(EncoderFourCC[0],EncoderFourCC[1],EncoderFourCC[2],EncoderFourCC[3]);
#ifdef WIN32
	__try {
#endif
				HWND hCfg = encoder.ConfigAudio3(hWnd, hDLL, out, config_file);
				ShowWindow(hCfg, SW_SHOW);
				RECT rc,rc2;
				GetWindowRect(hCfg, &rc);
				ShowWindow(hWnd, SW_SHOW);
				GetWindowRect(hWnd, &rc2);
				MoveWindow(hWnd, rc2.left, rc2.top, rc.right - rc.left + 10, rc.bottom - rc.top + 30, TRUE);
#ifdef WIN32
	} __except(EXCEPTION_EXECUTE_HANDLER) {
				adapi->botapi->ib_printf(_("ERROR: Exception occured while trying to open codec configuration window!\n"));
				EndDialog(hWnd, 0);
	}
#endif
			}
			return TRUE;
			break;
		case WM_CLOSE:
			EndDialog(hWnd, 0);
			return TRUE;
			break;
	}
	return FALSE;
}

THREADTYPE ConfigWindowThread(void * lpData) {
	TT_THREAD_START
	DialogBox(GetADPlugin()->hModule, MAKEINTRESOURCE(IDD_DIALOG1), GetDesktopWindow(), ConfigProc);
	TT_THREAD_END
}

int ConfigCommand(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "autodj-winamp-config")) {
		TT_StartThread(ConfigWindowThread, NULL, "cwt");
		ut->std_reply(ut, _("A window should have popped up on your desktop. (or if it is running on a different computer, on it's desktop)"));
		ut->std_reply(ut, _("If a window didn't pop up, it may mean that your current encoder doesn't support configuration"));
		return 1;
	}
	return 0;
}


bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;

	char file[MAX_PATH]={0};
	memset(&encoder, 0, sizeof(encoder));

	DS_CONFIG_SECTION * sec = adapi->botapi->config->GetConfigSection(NULL, "AutoDJ");
	if (sec) {
		sec = adapi->botapi->config->GetConfigSection(sec, "Winamp_Encoder");
		if (sec) {
			adapi->botapi->config->GetConfigSectionValueBuf(sec, "Plugin", file, sizeof(file));
			int tmp = adapi->botapi->config->GetConfigSectionLong(sec, "EncoderIndex");
			if (tmp >= 0) {
				EncoderIndex = tmp;
			}
		} else {
			return false;
		}
	} else {
		return false;
	}

	if (strlen(file)) {
		hDLL = DL_Open(file);
		if (!hDLL) {
			adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Error loading '%s' (%s)\n"), file, DL_LastError());
			return false;
		}
	} else { return false; }

	encoder.GetAudioTypes3 = (GetAudioTypes3Type)DL_GetAddress(hDLL, "GetAudioTypes3");
	encoder.ConfigAudio3 = (ConfigAudio3Type)DL_GetAddress(hDLL, "ConfigAudio3");
	encoder.CreateAudio3 = (CreateAudio3Type)DL_GetAddress(hDLL, "CreateAudio3");
	encoder.FinishAudio3 = (FinishAudio3Type)DL_GetAddress(hDLL, "FinishAudio3");

	if (!encoder.ConfigAudio3 || !encoder.CreateAudio3 || !encoder.GetAudioTypes3) {
		adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Error loading '%s' (unable to resolve all symbols)\n"), file);
		DL_Close(hDLL);
		hDLL = NULL;
		return false;
	}

	unsigned int f = encoder.GetAudioTypes3(EncoderIndex, file);
	EncoderFourCC[0] = f & 0xFF;
	EncoderFourCC[1] = (f >> 8) & 0xFF;
	EncoderFourCC[2] = (f >> 16) & 0xFF;
	EncoderFourCC[3] = (f >> 24) & 0xFF;

	while (strlen(EncoderFourCC) < 4) {
		strcat(EncoderFourCC, " ");
	}

	adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Using encoder: %s (FourCC: %s)\n"), file, EncoderFourCC);
	if (!stricmp(EncoderFourCC, "AACP")) {
		adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Detected AACP, automatically setting mime type to audio/aacp (MimeOverride will still override this)\n"));
		winamp_encoder.mimetype = aacp_desc;
	}
	if (!stricmp(EncoderFourCC, "FLAk")) {
		adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Detected FLAC, automatically setting mime type to application/x-flac (MimeOverride will still override this)\n"));
		winamp_encoder.mimetype = flac_desc;
	}
	if (!stricmp(EncoderFourCC, "WMA ")) {
		adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Detected WMA, automatically setting mime type to audio/x-ms-wma (MimeOverride will still override this)\n"));
		winamp_encoder.mimetype = wma_desc;
	}
	if (!stricmp(EncoderFourCC, "MP3l")) {
		adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Detected MP3, automatically setting mime type to audio/mpeg (MimeOverride will still override this)\n"));
		winamp_encoder.mimetype = mp3_desc;
	}

	GetModuleFileName(hDLL, config_file, sizeof(config_file));
	char * p = strrchr(config_file, PATH_SEP);
	if (p) {
		p++;
		*p = 0;
		strlcat(config_file, "winamp_ircbot.cfg", sizeof(config_file));
	} else {
		sstrcpy(config_file, "./winamp_ircbot.cfg");
	}

	//TT_StartThread(ConfigWindowThread, NULL, "cwt");

	COMMAND_ACL acl;
	adapi->botapi->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "autodj-winamp-config", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, ConfigCommand, _("This command lets you configure your Winamp encoder settings"));

	adapi->RegisterEncoder(&winamp_encoder);
	return true;
};

void quit() {
	while (TT_NumThreads()) {
		adapi->botapi->ib_printf(_("AutoDJ (winamp_encoder) -> Waiting for configuration window(s) to close...\n"));
		safe_sleep(1);
	}
	adapi->botapi->commands->UnregisterCommandByName("autodj-winamp-config");
	if (hDLL) { DL_Close(hDLL); hDLL = NULL; }
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Winamp Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
