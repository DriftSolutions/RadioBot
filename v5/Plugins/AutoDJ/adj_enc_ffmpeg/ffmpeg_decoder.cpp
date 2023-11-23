//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2023 Drift Solutions / Indy Sams            |
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

string ib_av_strerror(int errnum) {
	char buf[AV_ERROR_MAX_STRING_SIZE+1] = { 0 };
	if (av_strerror(errnum, buf, sizeof(buf)) != 0) {
		snprintf(buf, sizeof(buf), "Unknown error (%d)", errnum);
	}
	return buf;
}

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
		const AVOutputFormat * of = av_guess_format(NULL, fn, mime_type);
		RelMutex(ffmpegMutex);
		if (of != NULL) {
			return true;
		}

		AVFormatContext	* pContext = NULL;
		LockMutex(ffmpegMutex);
		bool ret = (avformat_open_input(&pContext, fn, NULL, NULL) == 0);
		if (ret) {
			avformat_close_input(&pContext);
		}
		RelMutex(ffmpegMutex);
		return ret;
	} else if (mime_type) {
		if (!stricmp(mime_type, "audio/mpeg")) {
			return false;
		}
		const AVOutputFormat * of = av_guess_format(NULL, NULL, mime_type);
		if (of != NULL) {
			return true;
		}
	}

	return false;
}

bool ffmpeg_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }

	bool ret = false;
	AVFormatContext	* pContext = NULL;
	LockMutex(ffmpegMutex);
	int err = avformat_open_input(&pContext, fn, NULL, NULL);
	RelMutex(ffmpegMutex);
	if (err == 0) {
		// this is needed to load meta tags and stream duration
		err = avformat_find_stream_info(pContext, NULL);
		if (err >= 0) {
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
			adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error getting stream info for %s: %s\n"), fn, ib_av_strerror(err).c_str());
		}
		LockMutex(ffmpegMutex);
		avformat_close_input(&pContext);
		RelMutex(ffmpegMutex);
	} else {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file: %s: %s\n"), fn, ib_av_strerror(err).c_str());
	}

	return ret;
}

class ffmpeg_Decoder: public AutoDJ_Decoder {
private:
	int audioStream = -1;
	uint32 frameno = 0;
	uint64_t layout = 0;

	AVIOContext		* avio_context = NULL;
	AVFormatContext	* ffmpeg_context = NULL;
	AVCodecContext  * ffmpeg_codecctx = NULL;
	const AVCodec * ffmpeg_codec = NULL;
	SwrContext * swr = NULL;
	bool finishOpen();
public:
	ffmpeg_Decoder();
	virtual ~ffmpeg_Decoder(){} // just for compiler warnings
	virtual bool Open(READER_HANDLE * fpp, int64 startpos);
	virtual bool Open_URL(const char * url, int64 startpos);
	virtual int64 GetPosition();
	virtual DECODE_RETURN Decode();
	virtual void Close();
};

ffmpeg_Decoder::ffmpeg_Decoder() {
}

uint64 GetOutputFormat() {
	uint64_t layout = 0;
	int32 ourchans = adapi->GetOutputChannels();
	if (ourchans == 1) {
		layout = AV_CH_LAYOUT_MONO;
	} else if (ourchans == 2) {
		layout = AV_CH_LAYOUT_STEREO;
	} else if (ourchans == 3) {
		layout = AV_CH_LAYOUT_2POINT1;
	} else if (ourchans == 5) {
		layout = AV_CH_LAYOUT_4POINT1;
	} else if (ourchans == 6) {
		layout = AV_CH_LAYOUT_5POINT1;
	} else if (ourchans == 8) {
		layout = AV_CH_LAYOUT_7POINT1;
	}
	return layout;
}

bool ffmpeg_Decoder::finishOpen() {
	int32 ourchans = adapi->GetOutputChannels();
	layout = GetOutputFormat();
	if (layout == 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Don\'t know what audio format we are?\n"));
		avformat_close_input(&ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
		return false; // Couldn't find stream information
	}

	int err = avformat_find_stream_info(ffmpeg_context, NULL);
	if (err < 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error getting stream info! (err: %d)\n"), err);
		avformat_close_input(&ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
		return false; // Couldn't find stream information
	}

#if defined(DEBUG)
	av_dump_format(ffmpeg_context, 0, NULL, 0);
#endif

	audioStream=-1;
	int firstAudioStream=-1;
	for(unsigned int i=0; i < ffmpeg_context->nb_streams; i++) {
		if (ffmpeg_context->streams[i]->codecpar != NULL && ffmpeg_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (firstAudioStream == -1) { firstAudioStream = i; }
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
            audioStream = i;
            break;
        }
	}
	if (audioStream == -1) {
		audioStream = firstAudioStream;
	}
	if (audioStream == -1) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> No audio stream in file!\n"));
		avformat_close_input(&ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
        return false; // Couldn't find stream information
	}

	adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Using audio stream %d...\n"), audioStream);

    //ffmpeg_codecctx = ffmpeg_context->streams[audioStream]->codec;

    // Find the decoder for the audio stream
    ffmpeg_codec = avcodec_find_decoder(ffmpeg_context->streams[audioStream]->codecpar->codec_id);
	if(ffmpeg_codec == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error finding decoder for audio stream!\n"));
		avformat_close_input(&ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
        return false; // Couldn't find stream information
	}

	ffmpeg_codecctx = avcodec_alloc_context3(ffmpeg_codec);
	// Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
	//if(ffmpeg_codec->capabilities & AV_CODEC_CAP_TRUNCATED) { ffmpeg_codecctx->flags |= AV_CODEC_FLAG_TRUNCATED; }

		/* Copy codec parameters from input stream to output codec context */
	if ((err = avcodec_parameters_to_context(ffmpeg_codecctx, ffmpeg_context->streams[audioStream]->codecpar)) < 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Failed to copy %s codec parameters to decoder context\n"), av_get_media_type_string(ffmpeg_codec->type));
		avcodec_free_context(&ffmpeg_codecctx);
		avformat_close_input(&ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
		return false; // Couldn't find stream information
	}

    // Open codec
	LockMutex(ffmpegMutex);
	err = avcodec_open2(ffmpeg_codecctx, ffmpeg_codec, NULL);
	RelMutex(ffmpegMutex);
	if(err != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening decoder for audio stream!\n"));
		avcodec_free_context(&ffmpeg_codecctx);
		avformat_close_input(&ffmpeg_context);
		if (avio_context) {
			adj_destroy_read_handle(avio_context);
		}
        return false; // Couldn't find stream information
	}

	adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Opened with decoder '%s/%s'\n"), ffmpeg_context->iformat->long_name, ffmpeg_codec->name);

	if (ffmpeg_context->duration == 0x8000000000000000LL) {
		song_length = 0;
	} else {
		double tmp = double(ffmpeg_context->duration);
		tmp /= double(AV_TIME_BASE)/1000.0;
		song_length = tmp;
	}


	return true;
}

bool ffmpeg_Decoder::Open(READER_HANDLE * fnIn, int64 startpos) {

	audioStream = 0;
	frameno = 0;
	ffmpeg_context = NULL;
	ffmpeg_codecctx = NULL;
	ffmpeg_codec = NULL;
	fp = fnIn;

	ffmpeg_context = avformat_alloc_context();
	avio_context = adj_create_read_handle(fnIn);
	ffmpeg_context->pb = avio_context;

	LockMutex(ffmpegMutex);
	int err = avformat_open_input(&ffmpeg_context, fnIn->fn, NULL, NULL);
	RelMutex(ffmpegMutex);

    if (err != 0) {
		// On error avformat_open_input frees ffmpeg_context
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file %s for input: %s\n"), fnIn->fn, ib_av_strerror(err).c_str());
		adj_destroy_read_handle(avio_context);
		avio_context = NULL;
		ffmpeg_context = NULL;
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
	avio_context = NULL;

	ffmpeg_context = avformat_alloc_context();
	LockMutex(ffmpegMutex);
	int err = avformat_open_input(&ffmpeg_context, fn, NULL, NULL);
	RelMutex(ffmpegMutex);
    if (err != 0) {
		// On error avformat_open_input frees ffmpeg_context
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error opening file %s for input: %s\n"), fn, ib_av_strerror(err).c_str());
		ffmpeg_context = NULL;
		return false;
	}

	return finishOpen();
}

int64 ffmpeg_Decoder::GetPosition() { return fp->tell(fp); }

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif

DECODE_RETURN ffmpeg_Decoder::Decode() {
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(16)    /* set alignment to 16 byte boundary */
	static int16_t audiobuf[AVCODEC_MAX_AUDIO_FRAME_SIZE/sizeof(int16_t)];
#pragma pack(pop)   /* restore original alignment from stack */

	int ret=0, n=0;
	bool had_error = false;

	AVFrame * frame = av_frame_alloc();
	AVPacket * pkt = av_packet_alloc();
	uint8_t * outbufs[] = { (uint8_t *)&audiobuf[0] };
	while (ret == 0 && !had_error && (n = av_read_frame(ffmpeg_context, pkt)) >= 0) {
		//if (n!=0) { adapi->botapi->ib_printf(_("av_read_frame() -> %d / %d\n"), n, pkt.stream_index); }
		if (pkt->stream_index != audioStream) { av_packet_unref(pkt); continue; }

		n = avcodec_send_packet(ffmpeg_codecctx, pkt);
		if (n != 0) {
			adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error decoding audio packet: %s\n"), ib_av_strerror(n).c_str());
			av_packet_unref(pkt);
			break;
		}

		while ((n = avcodec_receive_frame(ffmpeg_codecctx, frame)) >= 0) {
			if (frameno == 0) {
#ifdef FFMPEG_USE_CH_LAYOUT
				if (ffmpeg_codecctx->sample_fmt != AV_SAMPLE_FMT_S16 || adapi->GetOutputChannels() != ffmpeg_codecctx->ch_layout.nb_channels || ffmpeg_codecctx->sample_rate != adapi->GetOutputSample()) {
#else
				if (ffmpeg_codecctx->sample_fmt != AV_SAMPLE_FMT_S16 || layout != ffmpeg_codecctx->channel_layout || ffmpeg_codecctx->sample_rate != adapi->GetOutputSample()) {
#endif
					swr = swr_alloc();
#if defined(FFMPEG_USE_CH_LAYOUT)
					av_opt_set_chlayout(swr, "in_channel_layout", &ffmpeg_codecctx->ch_layout, 0);
					AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_STEREO;
					if (adapi->GetOutputChannels() == 1) {
						ch_layout = AV_CHANNEL_LAYOUT_MONO;
					}
					av_opt_set_chlayout(swr, "out_channel_layout", &ch_layout, 0);
#else
					av_opt_set_int(swr, "in_channel_layout", ffmpeg_codecctx->channel_layout, 0);
					av_opt_set_int(swr, "out_channel_layout", layout, 0);
#endif
					av_opt_set_int(swr, "in_sample_rate", ffmpeg_codecctx->sample_rate, 0);
					av_opt_set_int(swr, "out_sample_rate", adapi->GetOutputSample(), 0);
					av_opt_set_sample_fmt(swr, "in_sample_fmt", ffmpeg_codecctx->sample_fmt, 0);
					av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
					if ((n = swr_init(swr)) < 0) {
						adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error initializing swresample: %d -> %s\n"), n, ib_av_strerror(n).c_str());
						had_error = true;
						av_packet_unref(pkt);
						break;
					}
#ifdef DEBUG
					char buf[64] = { 0 };
#ifdef FFMPEG_USE_CH_LAYOUT
					av_channel_layout_describe(&ffmpeg_codecctx->ch_layout, buf, sizeof(buf));
#else
					av_get_channel_layout_string(buf, sizeof(buf), ffmpeg_codecctx->channels, ffmpeg_codecctx->channel_layout);
#endif
					adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Resampling from %dkHz %s to %dkHz %d channels\n"), ffmpeg_codecctx->sample_rate, buf, adapi->GetOutputSample(), adapi->GetOutputChannels());
#endif
				}
			}

			int size = frame->nb_samples;
			if (swr != NULL) {
				size = swr_convert(swr, outbufs, sizeof(audiobuf) / sizeof(int16_t) / adapi->GetOutputChannels(), (const uint8_t **)frame->extended_data, frame->nb_samples);
				if (size <= 0) {
					adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Error resampling audio frame!\n"));
					av_packet_unref(pkt);
					had_error = true;
					break;
				}
			}

			if (size > 0) {
				if (frameno == 0) {
					if (!adapi->GetDeck(deck)->SetInAudioParams(adapi->GetOutputChannels(), adapi->GetOutputSample())) {
						adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> setInAudioParam(%d,%d) returned false!\n"), adapi->GetOutputChannels(), adapi->GetOutputSample());
						av_packet_unref(pkt);
						had_error = true;
						break;
					}
				}
				frameno++;

				if ((n = adapi->GetDeck(deck)->AddSamples(audiobuf, size)) <= 0) {
					av_packet_unref(pkt);
					had_error = true;
					break;
				}
				ret += n;
			}
		}

		av_packet_unref(pkt);
	}
	av_packet_free(&pkt);
	av_frame_free(&frame);
	return (ret > 0) ? AD_DECODE_CONTINUE : AD_DECODE_DONE;
}

void ffmpeg_Decoder::Close() {
	//adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> ffmpeg_close()\n"));
	if (swr != NULL) {
		swr_free(&swr);
	}
	if (ffmpeg_codecctx) {
		LockMutex(ffmpegMutex);
		avcodec_close(ffmpeg_codecctx);
		avcodec_free_context(&ffmpeg_codecctx);
		RelMutex(ffmpegMutex);
		ffmpeg_codecctx = NULL;
	}
	if (ffmpeg_context) {
		LockMutex(ffmpegMutex);
		avformat_close_input(&ffmpeg_context);
		RelMutex(ffmpegMutex);
	}
	if (avio_context) {
		adj_destroy_read_handle(avio_context);
		avio_context = NULL;
	}
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
