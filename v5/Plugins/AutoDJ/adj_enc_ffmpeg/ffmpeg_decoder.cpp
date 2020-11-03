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

#include "ffmpeg_codec.h"

#if defined(WIN32)
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")
#endif

AD_PLUGIN_API * adapi=NULL;

char * exts[] = {
	//Music files that other plugins should handle
	".mp3",
	".ogg",
	".cm3",
	".wav",
	".flac",

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
	".pls",
	".m3u",
	".m3u8",
	".sfv",
	".html",
	".htm",
	".txt",
	".log",

	//Subtitles
	".idx",
	".sub",

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

		LockMutex(ffmpegMutex);
#if LIBAVFORMAT_VERSION_MAJOR > 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 45)
		AVOutputFormat * of = av_guess_format(NULL, fn, mime_type);
#else
		AVOutputFormat * of = guess_format(NULL, fn, mime_type);
#endif
		RelMutex(ffmpegMutex);
		if (of != NULL) {
			return true;
		}

		AVFormatContext	* pContext = NULL;
		LockMutex(ffmpegMutex);
#ifdef FFMPEG_USE_AV_OPEN_INPUT
		int err = av_open_input_file(&pContext, fn, NULL, 0, NULL);
#else
		int err = avformat_open_input(&pContext, fn, NULL, NULL);
#endif
		RelMutex(ffmpegMutex);
		if (err != 0) {
			return false;
		}

		LockMutex(ffmpegMutex);
#ifdef FFMPEG_USE_AV_OPEN_INPUT
		av_close_input_file(pContext);
#else
		avformat_close_input(&pContext);
#endif
		RelMutex(ffmpegMutex);
		return true;
	} else if (mime_type) {
		if (!stricmp(mime_type, "audio/mpeg")) {
			return false;
		}
#if LIBAVFORMAT_VERSION_MAJOR > 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 45)
		AVOutputFormat * of = av_guess_format(NULL, NULL, mime_type);
#else
		AVOutputFormat * of = guess_format(NULL, NULL, mime_type);
#endif
		if (of != NULL) {
			return true;
		}
	}

	return true;
}

bool ffmpeg_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }

	bool ret = false;
	#ifdef WIN32
	__try {
	#endif
		AVFormatContext	* pContext = NULL;
		LockMutex(ffmpegMutex);
#ifdef FFMPEG_USE_AV_OPEN_INPUT
		int err = av_open_input_file(&pContext, fn, NULL, 0, NULL);
#else
		int err = avformat_open_input(&pContext, fn, NULL, NULL);
#endif
		RelMutex(ffmpegMutex);
		if (err == 0) {
			// this is needed to load meta tags and stream duration
#if (LIBAVFORMAT_VERSION_MAJOR > 53) || (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_MINOR >= 6)
			if (avformat_find_stream_info(pContext, NULL) >= 0) {
#else
			if (av_find_stream_info(pContext) >= 0) {
#endif
#if LIBAVFORMAT_VERSION_MAJOR < 53
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
				p = av_dict_get(pContext->metadata, "album_artist", NULL, 0);
				if (p) {
					strlcpy(td->album_artist, p->value, sizeof(td->album_artist));
				}
				p = av_dict_get(pContext->metadata, "author", NULL, 0);
				if (p) {
					strlcpy(td->genre, p->value, sizeof(td->genre));
				}
				p = av_dict_get(pContext->metadata, "url", NULL, 0);
				if (p) {
					strlcpy(td->url, p->value, sizeof(td->url));
				}
				p = av_dict_get(pContext->metadata, "comment", NULL, 0);
				if (p) {
					strlcpy(td->comment, p->value, sizeof(td->comment));
				}
				p = av_dict_get(pContext->metadata, "year", NULL, 0);
				if (p) {
					td->year = atoi(p->value);
				}
#endif
				if (songlen) {
					if (pContext->duration != 0x8000000000000000LL) {
						*songlen = pContext->duration / AV_TIME_BASE;
					}
				}
				strtrim(td->title);
				strtrim(td->artist);
				strtrim(td->album_artist);
				strtrim(td->album);
				strtrim(td->genre);
				strtrim(td->comment);
				ret = true;
			} else {
				adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error getting stream info! (err: %d)\n"), err);
			}
			LockMutex(ffmpegMutex);
#ifdef FFMPEG_USE_AV_OPEN_INPUT
			av_close_input_file(pContext);
#else
			avformat_close_input(&pContext);
#endif
			RelMutex(ffmpegMutex);
		} else {
			adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file: %s!\n"), fn);
		}
	#ifdef WIN32
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> ERROR: Exception occured while getting title info from: %s!\n"), fn);
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
//#if LIBAVCODEC_VERSION_MAJOR >= 58
//	AVCodecParameters  * ffmpeg_codecctx;
//#else
	AVCodecContext  * ffmpeg_codecctx;
//#endif
	AVCodec         * ffmpeg_codec;
#ifdef FFMPEG_USE_AVIO
	AVIOContext		* avio_context;
#endif
	bool finishOpen();
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
#ifdef FFMPEG_USE_AVIO
	avio_context = NULL;
#endif
}

bool ffmpeg_Decoder::finishOpen() {
#if (LIBAVFORMAT_VERSION_MAJOR > 53) || (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_MINOR >= 6)
	int err = avformat_find_stream_info(ffmpeg_context, NULL);
#else
	int err = av_find_stream_info(ffmpeg_context);
#endif
	if (err < 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error getting stream info! (err: %d)\n"), err);
#ifdef FFMPEG_USE_AVIO
		avformat_free_context(ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
#endif
		return false; // Couldn't find stream information
	}

#if defined(DEBUG)
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 101)
	av_dump_format(ffmpeg_context, 0, NULL, 0);
#else
	dump_format(ffmpeg_context, 0, NULL, 0);
#endif
#endif

	audioStream=-1;
	int firstAudioStream=-1;
	for(unsigned int i=0; i < ffmpeg_context->nb_streams; i++) {
#if LIBAVCODEC_VERSION_MAJOR >= 58
		if (ffmpeg_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
#elif LIBAVCODEC_VERSION_MAJOR >= 53
        if (ffmpeg_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
#else
        if (ffmpeg_context->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
#endif
			if (firstAudioStream == -1) { firstAudioStream = i; }
#if (LIBAVUTIL_VERSION_MAJOR > 51) || (LIBAVUTIL_VERSION_MAJOR == 51 && LIBAVUTIL_VERSION_MINOR > 5)
			if (ffmpeg_context->streams[i]->metadata) {
				AVDictionaryEntry * scan = av_dict_get(ffmpeg_context->streams[i]->metadata, "language", NULL, 0);
				if (scan) {
					adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Stream %d language: %s\n"), i, scan->value);
					if (strcmp(scan->value, "eng")) {
						adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Not preferred language, continuing scan...\n"));
						continue;
					}
				}
			}
#endif
            audioStream = i;
            break;
        }
	}
	if (audioStream == -1) {
		audioStream = firstAudioStream;
	}
	if (audioStream == -1) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> No audio stream in file!\n"));
#ifdef FFMPEG_USE_AVIO
		avformat_free_context(ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
#endif
        return false; // Couldn't find stream information
	}

	adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Using audio stream %d...\n"), audioStream);

//#if LIBAVCODEC_VERSION_MAJOR >= 58
//	ffmpeg_codecctx = ffmpeg_context->streams[audioStream]->codecpar;
//#else
    ffmpeg_codecctx = ffmpeg_context->streams[audioStream]->codec;
//#endif

    // Find the decoder for the video stream
    ffmpeg_codec = avcodec_find_decoder(ffmpeg_codecctx->codec_id);
	if(ffmpeg_codec == NULL) {
		ffmpeg_codecctx = NULL;
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error finding decoder for audio stream!\n"));
#ifdef FFMPEG_USE_AVIO
		avformat_free_context(ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
#endif
        return false; // Couldn't find stream information
	}

//#if LIBAVCODEC_VERSION_MAJOR < 58
    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
	if(ffmpeg_codec->capabilities & AV_CODEC_CAP_TRUNCATED) { ffmpeg_codecctx->flags |= AV_CODEC_FLAG_TRUNCATED; }
//#endif

    // Open codec
	LockMutex(ffmpegMutex);
#if FFMPEG_USE_AVCODEC_OPEN2
	err = avcodec_open2(ffmpeg_codecctx, ffmpeg_codec, NULL);
#else
	err = avcodec_open(ffmpeg_codecctx, ffmpeg_codec);
#endif
	RelMutex(ffmpegMutex);
	if(err < 0) {
		ffmpeg_codecctx = NULL;
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening decoder for audio stream!\n"));
#ifdef FFMPEG_USE_AVIO
		avformat_free_context(ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
#endif
        return false; // Couldn't find stream information
	}

	adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Opened with decoder '%s/%s'...\n"), ffmpeg_context->iformat->long_name, ffmpeg_codec->name);

	if (ffmpeg_context->duration == 0x8000000000000000LL) {
		song_length = 0;
	} else {
		double tmp = double(ffmpeg_context->duration);
		tmp /= double(AV_TIME_BASE)/1000.0;
		song_length = tmp;
	}

	memset(&curpos, 0, sizeof(curpos));
	curpos.den = ffmpeg_context->streams[audioStream]->time_base.den;
	last_channels = last_samplerate = 0;

	return true;
}

bool ffmpeg_Decoder::Open(READER_HANDLE * fnIn, int64 startpos) {

	audioStream = 0;
	frameno = 0;
	ffmpeg_context = NULL;
	ffmpeg_codecctx = NULL;
	ffmpeg_codec = NULL;
	fp = fnIn;

	/*
	AVInputFormat	iformat;
	AVFormatParameters ap;
	memset(&iformat, 0, sizeof(iformat));
	memset(&ap, 0, sizeof(ap));
	ap.initial_pause = 1;
	*/

#ifdef FFMPEG_USE_URLPROTO

	char buf[128];
	if (sizeof(char *) == 4) {
		sprintf(buf, "autodj:%u", fnIn);
	} else {
		sprintf(buf, "autodj:"U64FMT"", fnIn);
	}

	LockMutex(ffmpegMutex);
#ifdef FFMPEG_USE_AV_OPEN_INPUT
	int err = av_open_input_file(&ffmpeg_context, buf, NULL, 0, NULL);
#else
	int err = avformat_open_input(&ffmpeg_context, buf, NULL, NULL);
#endif
	RelMutex(ffmpegMutex);

#elif defined(FFMPEG_USE_AVIO)
	ffmpeg_context = avformat_alloc_context();
	avio_context = adj_create_read_handle(fnIn);

	LockMutex(ffmpegMutex);
#ifdef FFMPEG_USE_AV_OPEN_INPUT
	int err = av_open_input_stream(&ffmpeg_context, avio_context, fnIn->fn, NULL, NULL);
#else
	ffmpeg_context->pb = avio_context;
	int err = avformat_open_input(&ffmpeg_context, fnIn->fn, NULL, NULL);
#endif
	RelMutex(ffmpegMutex);

#else
#error What I/O method to use???
#endif // FFMPEG_USE_URLPROTO || FFMPEG_USE_AVIO
    if (err != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file for input! (err: %d)\n"), err);
#ifdef FFMPEG_USE_AVIO
		adj_destroy_read_handle(avio_context);
		avio_context = NULL;
#endif
		return false;
	}

	return finishOpen();

}

bool ffmpeg_Decoder::Open_URL(const char * fn, int64 startpos) {
	audioStream = 0;
	frameno = 0;
	ffmpeg_context = NULL;
	ffmpeg_codecctx = NULL;
	ffmpeg_codec = NULL;

	//ffmpegf = fnIn;
	//frameno=0;

	/*
	AVInputFormat	iformat;
	AVFormatParameters ap;
	//memset(&ic, 0, sizeof(ic));
	memset(&iformat, 0, sizeof(iformat));
	memset(&ap, 0, sizeof(ap));
	ap.initial_pause = 1;
	*/

	//char buf[128];
	//sprintf(buf, "autodj:%u", fnIn);
#if defined(FFMPEG_USE_AVIO)
	ffmpeg_context = avformat_alloc_context();
	avio_context = NULL;
#endif
	LockMutex(ffmpegMutex);
#ifdef FFMPEG_USE_AV_OPEN_INPUT
    int err = av_open_input_file(&ffmpeg_context, fn, NULL, 0, NULL);
#else
	int err = avformat_open_input(&ffmpeg_context, fn, NULL, NULL);
#endif
	RelMutex(ffmpegMutex);
    if (err != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file for input! (err: %d)\n"), err);
		return false;
	}

	return finishOpen();
}

int64 ffmpeg_Decoder::GetPosition() { return fp->tell(fp); }

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif

#ifdef FFMPEG_USE_AV_FRAME

int32 ffmpeg_Decoder::Decode() {
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(16)    /* set alignment to 16 byte boundary */
	static int16_t audiobuf[AVCODEC_MAX_AUDIO_FRAME_SIZE/sizeof(int16_t)];
#pragma pack(pop)   /* restore original alignment from stack */

	AVPacket pkt;
	memset(&pkt, 0, sizeof(pkt));
	int ret=0, n=0;

	AVFrame *frame = av_frame_alloc();
	if (frame == NULL) {
		av_free_packet(&pkt);
		return 0;
	}

	SwrContext *swr = swr_alloc();
	av_opt_set_int(swr, "in_channel_layout",  ffmpeg_codecctx->channel_layout, 0);
	av_opt_set_int(swr, "out_channel_layout", ffmpeg_codecctx->channel_layout,  0);
	av_opt_set_int(swr, "in_sample_rate",     ffmpeg_codecctx->sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate",    ffmpeg_codecctx->sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt",  ffmpeg_codecctx->sample_fmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
	if ((ret = swr_init(swr)) < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE]={0};
		av_strerror(ret, errbuf, sizeof(errbuf));
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error initializing swresample: %d -> %s\n"), ret, errbuf);
		swr_free(&swr);
		av_frame_free(&frame);
		av_free_packet(&pkt);
		return 0;
	}

	while (ret == 0 && (n = av_read_frame(ffmpeg_context, &pkt)) >= 0) {
		//if (n!=0) { adapi->botapi->ib_printf(_("av_read_frame() -> %d / %d\n"), n, pkt.stream_index); }
		if (pkt.data == NULL) { av_free_packet(&pkt); continue; }

		if (pkt.stream_index == audioStream) {
			int got_frame = 0;
			//int err = avcodec_receive_frame(ffmpeg_codecctx, frame);
			int err = avcodec_decode_audio4(ffmpeg_codecctx, frame, &got_frame, &pkt);
			if (err < 0) {
				av_free_packet(&pkt);
				adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error decoding audio packet!\n"));
				continue;
			}

			if (got_frame == 0) {
				adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error decoding audio frame!\n"));
				av_free_packet(&pkt);
				continue;
			}

			uint8_t * outbufs[] = { (uint8_t *)&audiobuf[0] };
			int size = swr_convert(swr, outbufs, sizeof(audiobuf) / sizeof(int16_t) / ffmpeg_codecctx->channels, (const uint8_t **)frame->extended_data, frame->nb_samples);
			if (size <= 0) {
				adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error resampling audio frame!\n"));
				av_free_packet(&pkt);
				continue;
			}
			size = size * ffmpeg_codecctx->channels;

			if (ffmpeg_codecctx->channels < 1 || ffmpeg_codecctx->channels > 2) {
				//av_free_packet(&pkt);
				//adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> AutoDJ only supports 1 or 2 channel audio!\n"));
				//return 0;
			}
			if (frameno == 0 || last_channels != ffmpeg_codecctx->channels || last_samplerate != ffmpeg_codecctx->sample_rate) {
				last_channels	= ffmpeg_codecctx->channels;
				last_samplerate	= ffmpeg_codecctx->sample_rate;
				if (!adapi->GetDeck(deck)->SetInAudioParams(ffmpeg_codecctx->channels, ffmpeg_codecctx->sample_rate)) {
					adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> setInAudioParam(%d,%d) returned false!\n"), ffmpeg_codecctx->channels, ffmpeg_codecctx->sample_rate);
					av_free_packet(&pkt);
					break;
				}
			}
			frameno++;

			ret = adapi->GetDeck(deck)->AddSamples(audiobuf, size / ffmpeg_codecctx->channels);

			curpos.num += pkt.duration;
			while (curpos.num >= curpos.den && curpos.den) {
				curpos.val++;
				curpos.num -= curpos.den;
			}
		}

		av_free_packet(&pkt);
	}
#ifdef FFMPEG_USE_AV_FRAME
	swr_free(&swr);
	av_frame_free(&frame);
#endif
	return ret;
}

#else // FFMPEG_USE_AV_FRAME

int32 ffmpeg_Decoder::Decode() {
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(16)    /* set alignment to 16 byte boundary */
	static int16_t audiobuf[AVCODEC_MAX_AUDIO_FRAME_SIZE/sizeof(int16_t)];
#pragma pack(pop)   /* restore original alignment from stack */

	AVPacket pkt;
	memset(&pkt, 0, sizeof(pkt));
	int ret=0, n=0;

	while (ret == 0 && (n = av_read_frame(ffmpeg_context, &pkt)) >= 0) {
		if (n!=0) { adapi->botapi->ib_printf(_("av_read_frame() -> %d / %d\n"), n, pkt.stream_index); }
		if (pkt.data == NULL) { av_free_packet(&pkt); continue; }

		if (pkt.stream_index == audioStream) {
			int size = sizeof(audiobuf);//AVCODEC_MAX_AUDIO_FRAME_SIZE
#if LIBAVCODEC_VERSION_MAJOR >= 53
			int err = avcodec_decode_audio3(ffmpeg_codecctx, (int16_t *)&audiobuf, &size, &pkt);
#else
			int err = avcodec_decode_audio2(ffmpeg_codecctx, (int16_t *)&audiobuf, &size, pkt.data, pkt.size);
#endif
			if (err < 0) {
				av_free_packet(&pkt);
				adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error decoding audio packet!\n"));
				continue;
			}

			if (size == 0) {
				av_free_packet(&pkt);
				continue;
			}

			if (ffmpeg_codecctx->channels < 1 || ffmpeg_codecctx->channels > 2) {
				//av_free_packet(&pkt);
				//adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> AutoDJ only supports 1 or 2 channel audio!\n"));
				//return 0;
			}
			if (frameno == 0 || last_channels != ffmpeg_codecctx->channels || last_samplerate != ffmpeg_codecctx->sample_rate) {
				last_channels	= ffmpeg_codecctx->channels;
				last_samplerate	= ffmpeg_codecctx->sample_rate;
				if (!adapi->GetDeck(deck)->SetInAudioParams(ffmpeg_codecctx->channels, ffmpeg_codecctx->sample_rate)) {
					av_free_packet(&pkt);
					adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> setInAudioParam(%d,%d) returned false!\n"), ffmpeg_codecctx->channels, ffmpeg_codecctx->sample_rate);
					return 0;
				}
			}
			frameno++;

			int slen = size / sizeof(int16_t);
			ret = adapi->GetDeck(deck)->AddSamples(audiobuf, slen / ffmpeg_codecctx->channels);

			curpos.num += pkt.duration;
			while (curpos.num >= curpos.den && curpos.den) {
				curpos.val++;
				curpos.num -= curpos.den;
			}
		}

		av_free_packet(&pkt);
	}
	return ret;
}

#endif // FFMPEG_USE_AV_FRAME

void ffmpeg_Decoder::Close() {
	//adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> ffmpeg_close()\n"));
	if (ffmpeg_codecctx) {
		LockMutex(ffmpegMutex);
		avcodec_close(ffmpeg_codecctx);
		RelMutex(ffmpegMutex);
		ffmpeg_codecctx = NULL;
	}
	if (ffmpeg_context) {
		LockMutex(ffmpegMutex);
#if defined(FFMPEG_USE_AV_OPEN_INPUT) && defined(FFMPEG_USE_AVIO)
		if (avio_context) {
			av_close_input_stream(ffmpeg_context);
		} else {
			av_close_input_file(ffmpeg_context);
		}
#elif (LIBAVFORMAT_VERSION_MAJOR > 53) || (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_MINOR >= 17)
		avformat_close_input(&ffmpeg_context);
#else
		av_close_input_file(ffmpeg_context);
#endif
		RelMutex(ffmpegMutex);
		ffmpeg_context = NULL;
	}
#ifdef FFMPEG_USE_AVIO
	if (avio_context) {
		adj_destroy_read_handle(avio_context);
		avio_context = NULL;
	}
#endif
	//adapi->Timing_Done();
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
