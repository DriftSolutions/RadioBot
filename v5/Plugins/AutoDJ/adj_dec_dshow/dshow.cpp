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
#include <dshow.h>
#include <Qedit.h>

AD_PLUGIN_API * api=NULL;
READER_HANDLE * dshowf;
long frameno=0;
long dshow_time_total=0;
time_t dshow_time_start=0;

bool dshow_is_my_file(char * fn, char * mime_type) {
	//if (mime_type && !stricmp(mime_type, "audio/mpeg")) { return true; }
	char * ext = fn ? strrchr(fn, '.') : NULL;
	if (ext) {
		if (!stricmp(ext, ".avi") || !stricmp(ext, ".ac3") || !stricmp(ext, ".wmv") || !stricmp(ext, ".wma") || !stricmp(ext, ".mpeg") || !stricmp(ext, ".mp4") || !stricmp(ext, ".divx") || !stricmp(ext, ".asf") || !stricmp(ext, ".wav")) {
			return true;
		}
	} else {
		printf("No ext for %s\n", fn);
	}
	return false;
}

bool dshow_get_title_data(TITLE_DATA * td, char * fn) {
	memset(td, 0, sizeof(TITLE_DATA));
	return true;
}

HRESULT GetUnconnectedPin(
    IBaseFilter *pFilter,   // Pointer to the filter.
    PIN_DIRECTION PinDir,   // Direction of the pin to find.
    IPin **ppPin)           // Receives a pointer to the pin.
{
    *ppPin = 0;
    IEnumPins *pEnum = 0;
    IPin *pPin = 0;
    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr))
    {
        return hr;
    }
    while (pEnum->Next(1, &pPin, NULL) == S_OK)
    {
        PIN_DIRECTION ThisPinDir;
        pPin->QueryDirection(&ThisPinDir);
        if (ThisPinDir == PinDir)
        {
            IPin *pTmp = 0;
            hr = pPin->ConnectedTo(&pTmp);
            if (SUCCEEDED(hr))  // Already connected, not the pin we want.
            {
                pTmp->Release();
            }
            else  // Unconnected, this is the pin we want.
            {
                pEnum->Release();
                *ppPin = pPin;
                return S_OK;
            }
        }
        pPin->Release();
    }
    pEnum->Release();
    // Did not find a matching pin.
    return E_FAIL;
}

HRESULT ConnectFilters(
    IGraphBuilder *pGraph, // Filter Graph Manager.
    IPin *pOut,            // Output pin on the upstream filter.
    IBaseFilter *pDest)    // Downstream filter.
{
    if ((pGraph == NULL) || (pOut == NULL) || (pDest == NULL))
    {
        return E_POINTER;
    }
#ifdef debug
        PIN_DIRECTION PinDir;
        pOut->QueryDirection(&PinDir);
        _ASSERTE(PinDir == PINDIR_OUTPUT);
#endif

    // Find an input pin on the downstream filter.
    IPin *pIn = 0;
    HRESULT hr = GetUnconnectedPin(pDest, PINDIR_INPUT, &pIn);
    if (FAILED(hr))
    {
        return hr;
    }
    // Try to connect them.
    hr = pGraph->Connect(pOut, pIn);
    pIn->Release();
    return hr;
}

HRESULT ConnectFilters(
    IGraphBuilder *pGraph,
    IBaseFilter *pSrc,
    IBaseFilter *pDest)
{
    if ((pGraph == NULL) || (pSrc == NULL) || (pDest == NULL))
    {
        return E_POINTER;
    }

    // Find an output pin on the first filter.
    IPin *pOut = 0;
    HRESULT hr = GetUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = ConnectFilters(pGraph, pOut, pDest);
    pOut->Release();
    return hr;
}

HRESULT AddFilterByCLSID(
    IGraphBuilder *pGraph,  // Pointer to the Filter Graph Manager.
    const GUID& clsid,      // CLSID of the filter to create.
    LPCWSTR wszName,        // A name for the filter.
    IBaseFilter **ppF)      // Receives a pointer to the filter.
{
    if (!pGraph || ! ppF) return E_POINTER;
    *ppF = 0;
    IBaseFilter *pF = 0;
    HRESULT hr = CoCreateInstance(clsid, 0, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, reinterpret_cast<void**>(&pF));
    if (SUCCEEDED(hr))
    {
        hr = pGraph->AddFilter(pF, wszName);
        if (SUCCEEDED(hr))
            *ppF = pF;
        else
            pF->Release();
    }
    return hr;
}

IGraphBuilder	*pGraph = NULL;
IMediaControl	*pControl = NULL;
IMediaEvent		*pEvent = NULL;
IMediaPosition	*pMediaPos = NULL;
IBaseFilter		*pGrabberF = NULL;
IBaseFilter		*pPCM = NULL;
ISampleGrabber	*pGrabber = NULL;
IBaseFilter		*pSrc = NULL;

const CLSID CLSID_PCM = { 0x33D9A761, 0x90C8, 0x11D0, 0xBD, 0x43, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86 };

bool dshow_open(READER_HANDLE * fnIn, TITLE_DATA * meta) {
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr)) {
		api->botapi->ib_printf("AutoDJ (dshow_decoder) -> Could not initialize COM...\n");
		return false;
	}

	if (meta && strlen(meta->title)) {
		api->QueueTitleUpdate(meta);
	} else {
		TITLE_DATA *td = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
		memset(td,0,sizeof(TITLE_DATA));
		if (!stricmp(fnIn->type, "file")) {
			char * p = strrchr(fnIn->fn, '\\');
			if (!p) {
				p = strrchr(fnIn->fn, '/');
			}
			if (p) {
				strcpy(td->title, p+1);
			} else {
				strcpy(td->title, fnIn->fn);
			}
		} else {
			strcpy(td->title, fnIn->fn);
		}
		api->QueueTitleUpdate(td);
		zfree(td);
	}

	dshowf = fnIn;
	frameno=0;
	dshow_time_total=0;
	dshow_time_start=time(NULL);

	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&pGraph);
	if (FAILED(hr)) {
		api->botapi->ib_printf("AutoDJ (dshow_decoder) -> Error creating filter graph instance! (%X)\n", hr);
		return false;
	}

	hr = pGraph->QueryInterface(IID_IMediaControl, (void **)&pControl);
	hr = pGraph->QueryInterface(IID_IMediaEvent, (void **)&pEvent);
	hr = pGraph->QueryInterface(IID_IMediaPosition, (void **)&pMediaPos);

	hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&pGrabberF);
	if (FAILED(hr))	{
		api->botapi->ib_printf("AutoDJ (dshow_decoder) -> Error creating sample grabber instance!\n");
		return false;
	}

	hr = pGraph->AddFilter(pGrabberF, L"Sample Grabber");
	if (FAILED(hr)) {
		api->botapi->ib_printf("AutoDJ (dshow_decoder) -> Error adding sample grabber to filter graph instance!\n");
		return false;
	}

	pGrabberF->QueryInterface(IID_ISampleGrabber, (void**)&pGrabber);

	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
	mt.majortype = MEDIATYPE_Audio;
	mt.subtype = MEDIASUBTYPE_PCM;
	mt.formattype = FORMAT_WaveFormatEx;
	WAVEFORMATEX wf;
	memset(&wf, 0, sizeof(wf));
	wf.nChannels = 2;
	wf.wFormatTag = WAVE_FORMAT_PCM;
	mt.pbFormat = (BYTE *)&wf;
	mt.cbFormat = sizeof(wf);
	hr = pGrabber->SetMediaType(&mt);

	wchar_t fn[MAX_PATH]={0};
	mbstowcs(fn, fnIn->fn, MAX_PATH);
	hr = pGraph->AddSourceFilter(fn, L"Source", &pSrc);
	if (FAILED(hr)) {
		api->botapi->ib_printf("AutoDJ (dshow_decoder) -> Error adding source filter!\n");
		return false;
		// Return an error code.
	}

	if ( SUCCEEDED(AddFilterByCLSID(pGraph, CLSID_PCM, L"PCM Compressor", &pPCM)) ) {
		if ( SUCCEEDED(ConnectFilters(pGraph, pSrc, pPCM)) ) {
			hr = ConnectFilters(pGraph, pPCM, pGrabberF);
		} else {
			hr = ConnectFilters(pGraph, pSrc, pGrabberF);
		}
	} else {
		hr = ConnectFilters(pGraph, pSrc, pGrabberF);
	}
	hr = pGrabber->SetOneShot(TRUE);
	hr = pGrabber->SetBufferSamples(TRUE);

	REFTIME pLen;
	pMediaPos->get_Duration(&pLen);
	api->Timing_Reset(pLen * 1000);

	return true;
}

static int dshow_mad_input(void * data) {
	/*
	buffer *buf = (buffer *)data;

	if (mp3f->eof(mp3f) || api->ShouldIStop()) {
		mp3d_ret = AD_DECODE_DONE;
		return MAD_FLOW_STOP;
	}

	char * start = (char *)buf->start;
	int len = dshow_BUFFER_SIZE;
	if (stream->next_frame) {
		len = (buf->start + buf->length) - stream->next_frame;
		start = (char *)(buf->start + len);
		memcpy((void *)buf->start,stream->next_frame,len);
		buf->length = len + mp3f->read((void *)start,1,dshow_BUFFER_SIZE - len,mp3f);
	} else {
		buf->length = mp3f->read((void *)buf->start,1,dshow_BUFFER_SIZE,mp3f);
	}

	if (ctx_tag && (mp3f->tell(mp3f) > ctx_tag->end)) {
		TITLE_DATA td;
		memset(&td,0,sizeof(td));
		strcpy(td.title,ctx_tag->title);
		api->QueueTitleUpdate(&td);
		ctx_tag = ctx_tag->Next;
	}

	mad.mad_stream_buffer(stream, buf->start, buf->length);
	*/
	return 1;
}

static int dshow_mad_output(void *data) {
	/*
	if ((header->flags & MAD_FLAG_INCOMPLETE) || pcm->length < 1152) {
#ifdef DEBUG
		printf("flags: %d, pcm: %d,%d,%d\n",header->flags, pcm->length, pcm->samplerate, pcm->channels);
#endif
	}
	bool bRet = api->GetEncoder()->encode_sint(pcm->length,pcm->samples[0],pcm->samples[1]);
	if (api->ShouldIStop()) {
		mp3d_ret = AD_DECODE_DONE;
		return MAD_FLOW_STOP;
	}
	if (!bRet) {
		mp3d_ret = AD_DECODE_ERROR;
		return MAD_FLOW_BREAK;
	}
	*/
	return 1;
}

static int dshow_mad_decode_header(void *data) {
	/*
  buffer *buf = (buffer *)data;
  if (frameno == 0) {
	int channels = 0;
	if (mp3f->fileSize) {
		dshow_time_total = mp3f->fileSize / (header->bitrate / 8);
	}
	switch(header->mode) {
		case MAD_MODE_SINGLE_CHANNEL:
			channels=1;
			break;
		case MAD_MODE_JOINT_STEREO:
			channels=2;
			break;
		case MAD_MODE_STEREO:
			channels=2;
			break;
		default:
			channels=1;
			break;
	}
	api->Timing_Reset(dshow_time_total * 1000);
	if (!api->GetEncoder()->init(channels, header->samplerate, 12)) {
		api->botapi->ib_printf("AutoDJ (dshow_decoder) -> Encoder init(%d,%d) returned false!\n", channels, header->samplerate);
		mp3d_ret = AD_DECODE_ERROR;
		return MAD_FLOW_BREAK;
	}
  }
  mad.mad_timer_add(&dshow_current,header->duration);
  /*
  while (mad_timer_count(dshow_current,MAD_UNITS_SECONDS) > (time(NULL) - dshow_time_start)) {
	  safe_sleep(100,true);
  }
  */
/*
  api->Timing_Add(mad.mad_timer_count(header->duration,MAD_UNITS_MILLISECONDS));
//  if (mad_timer_count(header->duration,MAD_UNITS_MILLISECONDS) > 0) {
//	  safe_sleep(mad_timer_count(header->duration,MAD_UNITS_MILLISECONDS),true);
//  }

  while (mad.mad_timer_count(dshow_current,MAD_UNITS_MILLISECONDS) > ( ((time(NULL) - dshow_time_start) * 1000) + 1000 ) ) {
	  safe_sleep(100,true);
  }

  frameno++;
  */
  return 1;
}

int dshow_decode() {
//	dshowd_ret = AD_DECODE_CONTINUE;
//	int res = mad.mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
	api->botapi->ib_printf("AutoDJ (dshow_decoder) -> dshow_decode(): mad_decoder_run() -> %d\n", 0);
	HRESULT hr = 0;

	while (!FAILED(pControl->Run()) && !api->ShouldIStop()) { // Run the graph.
		long evCode = 0;
		pEvent->WaitForCompletion(INFINITE, &evCode);

		long cbBuffer = 0;
		HRESULT hr = pGrabber->GetCurrentBuffer(&cbBuffer, NULL);

		char *pBuffer = new char[cbBuffer];
		if (!pBuffer) {
			// Out of memory. Return an error code.
		}
		hr = pGrabber->GetCurrentBuffer(&cbBuffer, (long*)pBuffer);

		AM_MEDIA_TYPE mt;
		hr = pGrabber->GetConnectedMediaType(&mt);
		if (!FAILED(hr)) {
			if (mt.majortype == MEDIATYPE_Audio) {
				if (mt.formattype == FORMAT_WaveFormatEx) {
					if (mt.pbFormat) {
						WAVEFORMATEX * wf = (WAVEFORMATEX *)mt.pbFormat;
						wf = wf;
					}
					//mt.subtype
				} else {
					printf("Unknown formattype for MEDIATYPE_Audio\n");
				}
			} else if (mt.majortype == MEDIATYPE_Video) {
				if (mt.formattype == FORMAT_VideoInfo) {
					printf("Video: ss: %d fbsize: %u\n", mt.lSampleSize, mt.cbFormat);
				} else if (mt.formattype == FORMAT_VideoInfo2) {
					printf("FORMAT_VideoInfo2 (unsupported/untested)\n");
				} else {
					printf("Unknown/unsupported format type\n");
				}
			} else {
				printf("Unknown/unsupported major type\n");
			}
		}
		delete pBuffer;
	}

	api->Timing_Done();
	return AD_DECODE_DONE;
}

void dshow_close() {

	api->botapi->ib_printf("AutoDJ (dshow_decoder) -> dshow_close()\n");

	if (pSrc) {
		pSrc->Release();
		pSrc = NULL;
	}
	if (pGrabber) {
		pGrabber->Release();
		pGrabber = NULL;
	}
	if (pGrabberF) {
		pGrabberF->Release();
		pGrabberF = NULL;
	}
	if (pControl) {
		pControl->Release();
		pControl = NULL;
	}
	if (pEvent) {
		pEvent->Release();
		pEvent = NULL;
	}
	if (pGraph) {
		pGraph->Release();
		pGraph = NULL;
	}
	if (dshowf) {
		dshowf->close(dshowf);
		dshowf = NULL;
	}
	api->GetEncoder()->close();
	api->Timing_Done();
	CoUninitialize();
}

DECODER dshow_decoder = {
	dshow_is_my_file,
	dshow_get_title_data,
	dshow_open,
	dshow_decode,
	dshow_close
};

bool init(AD_PLUGIN_API * pApi) {
	api = pApi;
	api->RegisterDecoder(&dshow_decoder);
	return true;
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"DirectShow Decoder",

	init,
	NULL
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
