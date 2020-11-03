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

#define __STDC_CONSTANT_MACROS
#include "../adj_plugin.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
#if defined(WIN32)
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#endif

AD_PLUGIN_API * api=NULL;
/*
READER_HANDLE * ffmpegf=NULL;
*/

#ifndef AVERROR
#if EINVAL > 0
#define AVERROR(e) (-(e)) /**< returns a negative error code from a POSIX error code, to return from library functions. */
#define AVUNERROR(e) (-(e)) /**< returns a POSIX error code from a library function error return value. */
#else
/* some platforms have E* and errno already negated. */
#define AVERROR(e) (e)
#define AVUNERROR(e) (e)
#endif
#endif

#undef OLD_AVFORMAT
#if !defined(LIBAVFORMAT_VERSION_MAJOR) || LIBAVFORMAT_VERSION_MAJOR < 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR < 67)
#define OLD_AVFORMAT 1
#endif

int autodj_open(URLContext *h, const char *filename, int flags) {
	if (!strnicmp(filename, "autodj:", 7)) {
		READER_HANDLE * rh = (READER_HANDLE *)atou64(filename+7);
		h->priv_data = rh;
		h->flags |= URL_RDONLY;
		h->flags &= ~URL_WRONLY;
		h->is_streamed = rh->can_seek ? false:true;
		//api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Opened (%d / %X)\n"), h->is_streamed, h->flags);
		return 0;
	}

	return AVERROR(ENOENT);
}

int autodj_read(URLContext *h, unsigned char *buf, int size) {
	READER_HANDLE * rh = (READER_HANDLE *)h->priv_data;
	int ret = rh->read(buf, size, rh);
	return ret;
}

#if defined(OLD_AVFORMAT)
int autodj_write(URLContext *h, unsigned char *buf, int size) {
#else
int autodj_write(URLContext *h, const unsigned char *buf, int size) {
#endif
	READER_HANDLE * rh = (READER_HANDLE *)h->priv_data;
	return rh->write((void *)buf, size, rh);
}

int64_t autodj_seek(URLContext *h, int64_t pos, int whence) {
	READER_HANDLE * rh = (READER_HANDLE *)h->priv_data;
	//api->botapi->ib_printf("AutoDJ (ffmpeg_decoder) -> Seek: "I64FMT" / %d\n", pos, whence);

	if (whence == AVSEEK_SIZE) {
		if (rh->fileSize > 0) {
			return rh->fileSize;
		} else {
			return -1;
		}
	} else if (whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END) {
		if (rh->can_seek) {
			return rh->seek(rh, pos, whence);
		}
	}
	return -1;
}

int autodj_close(URLContext *h) {
	return 0;
}

char * exts[] = {
	//Music files that other plugins should handle
	".mp3",
	".ogg",
	".cm3",

	//Image (picture) files
	".jpg",
	".gif",
	".bmp",
	".png",
	".tga",

	//Text files
	".ini",
	".php",
	".cue",
	".nfo",
	".m3u",
	".sfv",
	".html",
	".htm",
	".txt",

	//Other files it shouldn't try to play / binary files
	".db",
	".torrent",
	".pdf",
	".nes",
	".zip",
	".rar",
	".par2",
	".doc",
	".exe",

	//NULL terminator
	NULL
};

bool ffmpeg_is_my_file(const char * fn, const char * mime_type) {
	if (fn) {
		//first, check the list of known bad file extensions so they don't have to be probed
		const char * ext = strrchr(fn, '.');
		if (ext) {
			for (int i=0; exts[i]; i++) {
				if (!stricmp(exts[i],ext)) {
					return false;
				}
			}
		}

		AVFormatContext	* pContext = NULL;
#if defined(FF_API_FORMAT_PARAMETERS)
		int err = avformat_open_input(&pContext, fn, NULL, NULL);
#else
		int err = av_open_input_file(&pContext, fn, NULL, 0, NULL);
#endif
		if (err != 0) {
			return false;
		}

		/*
		// this is needed to load meta tags and stream duration
		if (av_find_stream_info(pContext) < 0) {
			api->botapi->ib_printf(_("AutoDJ (ffmpeg_is_my_file) -> Error getting stream info! (err: %d)\n"), err);
			av_close_input_file(pContext);
			return false; // Couldn't find stream information
		}
		*/

		av_close_input_file(pContext);
		return true;
	}

	if (mime_type && !stricmp(mime_type, "audio/mpeg")) {
		return false;
	}

	return true;
}

bool ffmpeg_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=NULL; }

	bool ret = false;
	#ifdef WIN32
	__try {
	#endif
		AVFormatContext	* pContext = NULL;
		int err = av_open_input_file(&pContext, fn, NULL, 0, NULL);
		if (err == 0) {
			// this is needed to load meta tags and stream duration
			if (av_find_stream_info(pContext) >= 0) {
#if LIBAVFORMAT_VERSION_INT < (53<<16)
				strlcpy(td->title, pContext->title, sizeof(td->title));
				strlcpy(td->artist, pContext->author, sizeof(td->artist));
				strlcpy(td->album, pContext->album, sizeof(td->album));
				strlcpy(td->genre, pContext->genre, sizeof(td->genre));
#else
				AVDictionaryEntry * p = av_dict_get(pContext->metadata, "title", NULL, 0);
				if (p) {
					strlcpy(td->title, p->value, sizeof(td->title));
				}
				p = av_dict_get(pContext->metadata, "author", NULL, 0);
				if (p) {
					strlcpy(td->artist, p->value, sizeof(td->artist));
				}
				p = av_dict_get(pContext->metadata, "album", NULL, 0);
				if (p) {
					strlcpy(td->album, p->value, sizeof(td->album));
				}
				p = av_dict_get(pContext->metadata, "author", NULL, 0);
				if (p) {
					strlcpy(td->genre, p->value, sizeof(td->genre));
				}
#endif
				if (songlen) {
					int64_t tmp = pContext->duration / (AV_TIME_BASE / 1000);
					*songlen =  tmp;
				}
				ret = true;
			} else {
				api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error getting stream info! (err: %d)\n"), err);
			}
			av_close_input_file(pContext);
		} else {
			api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file: %s!\n"), fn);
		}
	#ifdef WIN32
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> ERROR: Exception occured while getting title info from: %s!\n"), fn);
		ret = false;
	}
	#endif

	return ret;
}

class ffmpeg_Decoder: public AutoDJ_Decoder {
private:
	int audioStream;
	//time_t ffmpeg_time_start;
	uint32 frameno;
	int last_channels, last_samplerate;
	AVFrac curpos;
	AVFormatContext	* ffmpeg_context;
	AVCodecContext  * ffmpeg_codecctx;
	AVCodec         * ffmpeg_codec;
public:
	ffmpeg_Decoder();
	virtual bool Open(READER_HANDLE * fpp, int64 startpos);
	virtual bool Open_URL(const char * url, int64 startpos);
	virtual int64 GetPosition();
	virtual int32 Decode();
	virtual void Close();
};

ffmpeg_Decoder::ffmpeg_Decoder() {
	audioStream = 0;
	frameno = 0;
	last_channels = last_samplerate = 0;
	ffmpeg_context = NULL;
	ffmpeg_codecctx = NULL;
	ffmpeg_codec = NULL;
}

bool ffmpeg_Decoder::Open(READER_HANDLE * fnIn, int64 startpos) {
	audioStream = 0;
	frameno = 0;
	ffmpeg_context = NULL;
	ffmpeg_codecctx = NULL;
	ffmpeg_codec = NULL;
	fp = fnIn;

	AVInputFormat	iformat;
	AVFormatParameters ap;
	memset(&iformat, 0, sizeof(iformat));
	memset(&ap, 0, sizeof(ap));
	ap.initial_pause = 1;

	char buf[128];
	if (sizeof(char *) == 4) {
		sprintf(buf, "autodj:%u", fnIn);
	} else {
		sprintf(buf, "autodj:"U64FMT"", fnIn);
	}

    int err = av_open_input_file(&ffmpeg_context, buf, NULL, 0, NULL);
    if (err != 0) {
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file for input! (err: %d)\n"), err);
		return false;
	}

	if (av_find_stream_info(ffmpeg_context) < 0) {
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error getting stream info! (err: %d)\n"), err);
        return false; // Couldn't find stream information
	}

    audioStream=-1;
	for(unsigned int i=0; i < ffmpeg_context->nb_streams; i++) {
#if LIBAVCODEC_VERSION_MAJOR >= 53
        if (ffmpeg_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
#else
        if (ffmpeg_context->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
#endif
            audioStream = i;
#if defined(DEBUG)
#if defined(FF_API_DUMP_FORMAT)
			av_dump_format(ffmpeg_context, i, fnIn->fn, 0);
#else
			dump_format(ffmpeg_context, i, fnIn->fn, 0);
#endif
#endif
            break;
        }
	}
	if (audioStream == -1) {
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> No audio stream in file!\n"));
        return false; // Couldn't find stream information
	}

    ffmpeg_codecctx = ffmpeg_context->streams[audioStream]->codec;

    // Find the decoder for the video stream
    ffmpeg_codec = avcodec_find_decoder(ffmpeg_codecctx->codec_id);
	if(ffmpeg_codec == NULL) {
		ffmpeg_codecctx = NULL;
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error finding decoder for audio stream!\n"));
        return false; // Couldn't find stream information
	}

    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
	if(ffmpeg_codec->capabilities & CODEC_CAP_TRUNCATED) { ffmpeg_codecctx->flags |= CODEC_FLAG_TRUNCATED; }

    // Open codec
	if(avcodec_open(ffmpeg_codecctx, ffmpeg_codec) < 0) {
		ffmpeg_codecctx = NULL;
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening decoder for audio stream!\n"));
        return false; // Couldn't find stream information
	}

	api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Opened '%s' with decoder '%s/%s'...\n"), fnIn->fn, ffmpeg_context->iformat->long_name, ffmpeg_codec->name);

	//dump_format(ffmpeg_context, 0, fnIn->fn, false);
	double tmp = double(ffmpeg_context->duration) / double(AV_TIME_BASE/100000);
	//int64 len = (1000 * ffmpeg_context->duration) / AV_TIME_BASE;// / 1000);
	tmp=tmp;
	song_length = tmp;//*100000;
	//song_length = len*1000;

	memset(&curpos, 0, sizeof(curpos));
	curpos.den = ffmpeg_context->streams[audioStream]->time_base.den;
	last_channels = last_samplerate = 0;

	return true;
}

bool ffmpeg_Decoder::Open_URL(const char * fn, int64 startpos) {
	audioStream = 0;
	frameno = 0;
	ffmpeg_context = NULL;
	ffmpeg_codecctx = NULL;
	ffmpeg_codec = NULL;

	//ffmpegf = fnIn;
	//frameno=0;

	AVInputFormat	iformat;
	AVFormatParameters ap;
	//memset(&ic, 0, sizeof(ic));
	memset(&iformat, 0, sizeof(iformat));
	memset(&ap, 0, sizeof(ap));
	ap.initial_pause = 1;

	//char buf[128];
	//sprintf(buf, "autodj:%u", fnIn);

    int err = av_open_input_file(&ffmpeg_context, fn, NULL, 0, NULL);
    if (err != 0) {
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file for input! (err: %d)\n"), err);
		return false;
	}

	if (av_find_stream_info(ffmpeg_context) < 0) {
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error getting stream info! (err: %d)\n"), err);
        return false; // Couldn't find stream information
	}

    audioStream=-1;
	for(unsigned int i=0; i < ffmpeg_context->nb_streams; i++) {
#if LIBAVCODEC_VERSION_MAJOR >= 53
        if (ffmpeg_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
#else
        if (ffmpeg_context->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
#endif
            audioStream = i;
#if defined(DEBUG)
#if defined(FF_API_DUMP_FORMAT)
			av_dump_format(ffmpeg_context, i, fn, 0);
#else
			dump_format(ffmpeg_context, i, fn, 0);
#endif
#endif
            break;
        }
	}
	if (audioStream == -1) {
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> No audio stream in file!\n"));
        return false; // Couldn't find stream information
	}

    ffmpeg_codecctx = ffmpeg_context->streams[audioStream]->codec;

    // Find the decoder for the video stream
    ffmpeg_codec = avcodec_find_decoder(ffmpeg_codecctx->codec_id);
	if(ffmpeg_codec == NULL) {
		ffmpeg_codecctx = NULL;
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error finding decoder for audio stream!\n"));
        return false; // Couldn't find stream information
	}

    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
	if(ffmpeg_codec->capabilities & CODEC_CAP_TRUNCATED) { ffmpeg_codecctx->flags |= CODEC_FLAG_TRUNCATED; }

    // Open codec
	if(avcodec_open(ffmpeg_codecctx, ffmpeg_codec) < 0) {
		ffmpeg_codecctx = NULL;
		api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening decoder for audio stream!\n"));
        return false; // Couldn't find stream information
	}

	api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Opened '%s' with decoder '%s/%s'...\n"), fn, ffmpeg_context->iformat->long_name, ffmpeg_codec->name);

	uint64_t len = ffmpeg_context->duration;
	if (len == 0x8000000000000000LL) {
		len = 0;
	}
	len /= (AV_TIME_BASE/1000);
	song_length = len;

	memset(&curpos, 0, sizeof(curpos));
	curpos.den = ffmpeg_context->streams[audioStream]->time_base.den;
	last_channels = last_samplerate = 0;

	return true;
}

int64 ffmpeg_Decoder::GetPosition() { return fp->tell(fp); }

int32 ffmpeg_Decoder::Decode() {
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(16)    /* set alignment to 16 byte boundary */
	static int16_t audiobuf[1474560];
#pragma pack(pop)   /* restore original alignment from stack */

	AVPacket pkt;
	memset(&pkt, 0, sizeof(pkt));
	int ret=0, n=0;

	while (ret == 0 && (n = av_read_frame(ffmpeg_context, &pkt)) >= 0) {
		if (n!=0) { api->botapi->ib_printf(_("av_read_frame() -> %d / %d\n"), n, pkt.stream_index); }
		if (pkt.data == NULL) { av_free_packet(&pkt); continue; }

		if (pkt.stream_index == audioStream) {
			int size = sizeof(audiobuf);
#if LIBAVCODEC_VERSION_MAJOR >= 53
			int err = avcodec_decode_audio3(ffmpeg_codecctx, (int16_t *)&audiobuf, &size, &pkt);
#else
			int err = avcodec_decode_audio2(ffmpeg_codecctx, (int16_t *)&audiobuf, &size, pkt.data, pkt.size);
#endif
			if (err < 0) {
				av_free_packet(&pkt);
				api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error decoding audio packet!\n"));
				continue;
			}
			if (size == 0) {
				av_free_packet(&pkt);
				continue;
			}

			if (ffmpeg_codecctx->channels < 1 || ffmpeg_codecctx->channels > 2) {
				//av_free_packet(&pkt);
				//api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> AutoDJ only supports 1 or 2 channel audio!\n"));
				//return 0;
			}
			if (frameno == 0 || last_channels != ffmpeg_codecctx->channels || last_samplerate != ffmpeg_codecctx->sample_rate) {
				last_channels	= ffmpeg_codecctx->channels;
				last_samplerate	= ffmpeg_codecctx->sample_rate;
				if (!api->GetDeck(deck)->SetInAudioParams((ffmpeg_codecctx->channels < 3) ? ffmpeg_codecctx->channels:2, ffmpeg_codecctx->sample_rate)) {
					av_free_packet(&pkt);
					api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> setInAudioParam(%d,%d) returned false!\n"), ffmpeg_codecctx->channels, ffmpeg_codecctx->sample_rate);
					return 0;
				}
			}
			frameno++;

			int slen = size / sizeof(int16_t);
			ret = slen / ffmpeg_codecctx->channels;
			Audio_Buffer * buffer = api->AllocAudioBuffer();
			if (ffmpeg_codecctx->channels < 3) {
				buffer->realloc(buffer, slen);
				memcpy(buffer->buf, audiobuf, size);
			} else {
				buffer->realloc(buffer, ret*2);
				for (int i=0; i < (ret*2); i++) {
					buffer->buf[i++] = audiobuf[i*ffmpeg_codecctx->channels];
					buffer->buf[i] = audiobuf[(i*ffmpeg_codecctx->channels)+1];
				}
			}
			ret = api->GetDeck(deck)->AddSamples(buffer);
			api->FreeAudioBuffer(buffer);

			/*
			if (!api->GetMixer()->encode_short_interleaved(size / sizeof(int16_t) / ffmpeg_codecctx->channels, audiobuf)) {
				av_free_packet(&pkt);
				api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Encoder returned false!\n"));
				return AD_DECODE_ERROR;
			}
			*/

			curpos.num += pkt.duration;
			while (curpos.num >= curpos.den && curpos.den) {
				curpos.val++;
				curpos.num -= curpos.den;
				//api->Timing_Add(1000);
			}
		}

		av_free_packet(&pkt);
	}

	//api->botapi->ib_printf(_("av_read_frame() -> %d / %d\n"), n, ret);
	//api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> ffmpeg_decode() -> done\n"), 0);
	return ret;
}

void ffmpeg_Decoder::Close() {
	api->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> ffmpeg_close()\n"));
	if (ffmpeg_codecctx) {
		avcodec_close(ffmpeg_codecctx);
		ffmpeg_codecctx = NULL;
	}
	if (ffmpeg_context) {
		av_close_input_file(ffmpeg_context);
		ffmpeg_context = NULL;
	}
	//api->Timing_Done();
}

AutoDJ_Decoder * ffmpeg_create() { return new ffmpeg_Decoder; }
void ffmpeg_destroy(AutoDJ_Decoder * dec) { delete dynamic_cast<ffmpeg_Decoder *>(dec); }

DECODER ffmpeg_decoder = {
	999,
	ffmpeg_is_my_file,
	ffmpeg_get_title_data,
	ffmpeg_create,
	ffmpeg_destroy
};

static const char protocol_name[] = "autodj";
URLProtocol reader_protocol;

bool init(AD_PLUGIN_API * pApi) {
	api = pApi;
	av_register_all();
	memset(&reader_protocol, 0, sizeof(reader_protocol));
	reader_protocol.name = protocol_name;
	reader_protocol.url_open = autodj_open;
	reader_protocol.url_read = autodj_read;
	reader_protocol.url_write = autodj_write;
	reader_protocol.url_seek = autodj_seek;
	reader_protocol.url_close = autodj_close;
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 69)
	av_register_protocol2(&reader_protocol, sizeof(reader_protocol));
#elif (LIBAVCODEC_VERSION_MAJOR >= 52)
	av_register_protocol(&reader_protocol);
#else
	register_protocol(&reader_protocol);
#endif
	api->RegisterDecoder(&ffmpeg_decoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	AD_PLUGINFLAG_SUPPORTS_AUTODJ,
	"ffmpeg Decoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }

