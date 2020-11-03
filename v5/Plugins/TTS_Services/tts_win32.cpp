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
#if defined(WIN32)

#include <wchar.h>
#include <sapi.h>
#include <sphelper.h>

SPSTREAMFORMAT GetNearestAudioFormat(TTS_JOB * job) {
	switch(job->sample) {
		case 8:
			switch (job->channels) {
				case 1:
					return SPSF_8kHz16BitMono;
				case 2:
					return SPSF_8kHz16BitStereo;
			}
			break;
		case 11:
			switch (job->channels) {
				case 1:
					return SPSF_11kHz16BitMono;
				case 2:
					return SPSF_11kHz16BitStereo;
			}
			break;
		case 12:
			switch (job->channels) {
				case 1:
					return SPSF_12kHz16BitMono;
				case 2:
					return SPSF_12kHz16BitStereo;
			}
			break;
		case 16:
			switch (job->channels) {
				case 1:
					return SPSF_16kHz16BitMono;
				case 2:
					return SPSF_16kHz16BitStereo;
			}
			break;
		case 22:
			switch (job->channels) {
				case 1:
					return SPSF_22kHz16BitMono;
				case 2:
					return SPSF_22kHz16BitStereo;
			}
			break;
		case 24:
			switch (job->channels) {
				case 1:
					return SPSF_24kHz16BitMono;
				case 2:
					return SPSF_24kHz16BitStereo;
			}
			break;
		case 32:
			switch (job->channels) {
				case 1:
					return SPSF_32kHz16BitMono;
				case 2:
					return SPSF_32kHz16BitStereo;
			}
			break;
		case 44:
			switch (job->channels) {
				case 1:
					return SPSF_44kHz16BitMono;
				case 2:
					return SPSF_44kHz16BitStereo;
			}
			break;
		case 48:
			switch (job->channels) {
				case 1:
					return SPSF_48kHz16BitMono;
				case 2:
					return SPSF_48kHz16BitStereo;
			}
			break;
	}

	return SPSF_22kHz16BitMono;
}

bool MakeWAVFromFile_Win32(TTS_JOB * job) {
	if (FAILED(CoInitialize(NULL))) {
	    api->ib_printf(_("TTS Services -> Error initializing COM!\n"));
		return false;
    }

	HRESULT	hr = S_OK;
	CComPtr <ISpVoice> cpVoice;

	wchar_t outFN[MAX_PATH];
	mbstowcs(outFN, job->outputFN, MAX_PATH);
	wchar_t inFN[MAX_PATH];
	if (!strchr(job->inputFN, '\\') && !strchr(job->inputFN, '/')) {
		char inFNtmp[MAX_PATH];
		sprintf(inFNtmp, "%s%s", api->GetBasePath(), job->inputFN);
		mbstowcs(inFN, inFNtmp, MAX_PATH);
	} else {
		mbstowcs(inFN, job->inputFN, MAX_PATH);
	}

	//Create a SAPI voice
	hr = cpVoice.CoCreateInstance( CLSID_SpVoice );

	//Set the audio format
	if(SUCCEEDED(hr)) {
		CSpStreamFormat			cAudioFmt;
		hr = cAudioFmt.AssignFormat(GetNearestAudioFormat(job));
		//Call SPBindToFile, a SAPI helper method,  to bind the audio stream to the file
		if(SUCCEEDED(hr)) {
			CComPtr <ISpStream>		cpStream;
			hr = SPBindToFile( outFN,  SPFM_CREATE_ALWAYS, &cpStream, &cAudioFmt.FormatId(), cAudioFmt.WaveFormatExPtr() );
			//set the output to cpStream so that the output audio data will be stored in cpStream
			if(SUCCEEDED(hr)) {
				hr = cpVoice->SetOutput( cpStream, TRUE );
				if (SUCCEEDED(hr) && strlen(job->VoiceOverride)) {
					CComPtr <ISpObjectToken>	cpToken;
					CComPtr <IEnumSpObjectTokens>	cpEnum;
					//Enumerate voice tokens with attribute "Name=Microsoft Sam"
					if(SUCCEEDED(hr)) {
						wchar_t wfind[sizeof(job->VoiceOverride)+1];
						mbstowcs(wfind, job->VoiceOverride, sizeof(job->VoiceOverride));
						hr = SpEnumTokens(SPCAT_VOICES, wfind, NULL, &cpEnum);
					}

					//Get the closest token
					if(SUCCEEDED(hr))
					{
						hr = cpEnum ->Next(1, &cpToken, NULL);
					}

					//set the voice
					if(SUCCEEDED(hr))
					{
						hr = cpVoice->SetVoice(cpToken);
					}
					cpToken.Release();
					cpEnum.Release();
				}

				//Speak the text file (assumed to exist)
				/*
				L"autodj_script.txt", SPF_IS_FILENAME
				*/
				//wchar_t msgbufw[8192 * sizeof(wchar_t)];
				//mbstowcs(msgbufw, fn, 8191);
				//msgbufw, SPF_DEFAULT
				hr = cpVoice->Speak(inFN, SPF_IS_FILENAME, NULL );
				hr = cpStream->Close();
			}
			cpStream.Release();
		}
	}

	//Release objects
	cpVoice.Release ();
	CoUninitialize();
	return true;
}

bool MakeMP3FromFile_Win32(TTS_JOB * job) {
	return Generic_MakeMP3FromFile(job);
}

TTS_ENGINE tts_engine_win32 = {
	VE_WIN32,
	MakeMP3FromFile_Win32,
	MakeWAVFromFile_Win32
};

#endif
