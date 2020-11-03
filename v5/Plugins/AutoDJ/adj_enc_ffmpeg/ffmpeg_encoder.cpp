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

Titus_Mutex ffmpegMutex;

char mimetype[128]="audio/mpeg";

#if LIBAVFORMAT_VERSION_MAJOR < 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR <= 44)
#define av_guess_format guess_format
#endif

//DL_HANDLE avcDL = NULL;
//DL_HANDLE avfDL = NULL;

class FFMPEG_Encoder: public AutoDJ_Encoder {
private:
	//lame_global_flags *gfp;
#if defined(FFMPEG_USE_URLPROTO)
	bool opened;
#elif defined(FFMPEG_USE_AVIO)
	AVIOContext * avio_handle;
#endif
	bool first_encode;
	Audio_Buffer * ab;

	AVOutputFormat * fmt;
	AVFormatContext * oc;
	AVStream * ast;
	char format[64];
	char codec[64];

	int16 *samples;
	uint8 *audio_outbuf;
	int audio_outbuf_size;
	int audio_input_frame_size;

	int inChannels;

	bool outputBlocks();
	bool addAudioStream(AVCodecID acodec);
	bool initAudioEncoder();
public:
	FFMPEG_Encoder();
	virtual ~FFMPEG_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

FFMPEG_Encoder::FFMPEG_Encoder() {
	/*
	memset(&vi, 0, sizeof(vi));
	memset(&vds, 0, sizeof(vds));
	memset(&vb, 0, sizeof(vb));
	memset(&vc, 0, sizeof(vc));
	memset(&oss, 0, sizeof(oss));
	*/
	//gfp = NULL;
	fmt = NULL;
	oc = NULL;
	ab = NULL;
	ast = NULL;
#if defined(FFMPEG_USE_URLPROTO)
	opened = false;
#elif defined(FFMPEG_USE_AVIO)
	avio_handle = NULL;
#endif
	first_encode = true;
	inChannels = 0;

	strcpy(format, "mp3");
	codec[0] = 0;
	//strcpy(codec, "mp3");
	DS_CONFIG_SECTION * sec = adapi->botapi->config->GetConfigSection(NULL, "AutoDJ");
	if (sec) {
		sec = adapi->botapi->config->GetConfigSection(sec, "FFmpeg_Encoder");
		if (sec) {
			if (adapi->botapi->config->IsConfigSectionValue(sec, "Format")) {
				adapi->botapi->config->GetConfigSectionValueBuf(sec, "Format", format, sizeof(format));
			}
			if (adapi->botapi->config->IsConfigSectionValue(sec, "Encoder")) {
				adapi->botapi->config->GetConfigSectionValueBuf(sec, "Encoder", codec, sizeof(codec));
			}
		}
	}

	AVOutputFormat * fmt = av_guess_format(format, NULL, NULL);
    if (!fmt) {
        adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not find output format for %s, falling back to MP3...\n"), format);
		strcpy(format, "mp3");
		fmt = av_guess_format(format, NULL, NULL);
    }
	if (fmt && fmt->mime_type) {
		strcpy(mimetype, fmt->mime_type);
	}
	adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Using mime type: %s\n"), mimetype);
}

FFMPEG_Encoder::~FFMPEG_Encoder() {
}

bool FFMPEG_Encoder::initAudioEncoder() {
    AVCodecContext *c = ast->codec;
    AVCodec *codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not find encoder!\n"));
		return false;
    }

#if defined(AV_SAMPLE_FMT_S16) || LIBAVCODEC_VERSION_MAJOR >= 54
	c->sample_fmt = AV_SAMPLE_FMT_S16;
#else
	c->sample_fmt = SAMPLE_FMT_S16;
#endif
	LockMutex(ffmpegMutex);
#if FFMPEG_USE_AVCODEC_OPEN2
	if (avcodec_open2(c, codec, NULL) < 0) {
#else
	if (avcodec_open(c, codec) < 0) {
#endif
		RelMutex(ffmpegMutex);
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not open encoder!\n"));
		return false;
    }
	RelMutex(ffmpegMutex);

    audio_outbuf_size = 10000;
    audio_outbuf = (uint8 *)av_malloc(audio_outbuf_size);

    if (c->frame_size <= 1) {
        audio_input_frame_size = audio_outbuf_size / c->channels;
        switch(ast->codec->codec_id) {
        case AV_CODEC_ID_PCM_S16LE:
        case AV_CODEC_ID_PCM_S16BE:
        case AV_CODEC_ID_PCM_U16LE:
        case AV_CODEC_ID_PCM_U16BE:
            audio_input_frame_size >>= 1;
            break;
        default:
            break;
        }
    } else {
        audio_input_frame_size = c->frame_size;
    }
    samples = (int16 *)av_malloc(audio_input_frame_size * 2 * c->channels);
	return true;
}

bool FFMPEG_Encoder::addAudioStream(AVCodecID acodec) {
#ifdef FFMPEG_USE_AV_NEW_STREAM
    AVStream *st = av_new_stream(oc, 0);
#else
    AVStream *st = avformat_new_stream(oc, avcodec_find_encoder(acodec));
#endif
    if (!st) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not add audio stream!\n"));
		return false;
    }

    AVCodecContext *c = st->codec;
    c->codec_id = acodec;
#if LIBAVCODEC_VERSION_MAJOR >= 53
    c->codec_type = AVMEDIA_TYPE_AUDIO;
#else
    c->codec_type = CODEC_TYPE_AUDIO;
#endif

    /* put sample parameters */
    c->bit_rate = adapi->GetOutputBitrate()*1000;
    c->sample_rate = adapi->GetOutputSample();
    c->channels = inChannels;
	ast = st;
    return true;
}

bool FFMPEG_Encoder::Init(int channels, int samplerate, float scale) {
	fmt = NULL;
	oc = NULL;
	ast = NULL;
	/*
	memset(&vi, 0, sizeof(vi));
	memset(&vds, 0, sizeof(vds));
	memset(&vb, 0, sizeof(vb));
	memset(&vc, 0, sizeof(vc));
	memset(&oss, 0, sizeof(oss));
	*/

	first_encode = true;
	inChannels = channels == 1 ? 1:2;

	fmt = av_guess_format(format, NULL, NULL);
    if (!fmt) {
        adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not find output format for %s, falling back to MP3...\n"), codec);
		strcpy(format, "mp3");
        fmt = av_guess_format(format, NULL, NULL);
    }
    if (!fmt) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not load output format!\n"));
		Close();
        return false;
    }

    /* allocate the output media context */
#if LIBAVFORMAT_VERSION_MAJOR > 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 26)
    oc = avformat_alloc_context();
#else
    oc = av_alloc_format_context();
#endif
    if (!oc) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not allocate format context!\n"));
		Close();
		return false;
    }
    oc->oformat = fmt;

	AVCodecID acodec = fmt->audio_codec;
	if (codec[0]) {
		AVCodec * c = avcodec_find_encoder_by_name(codec);
		if (c != NULL) {
			acodec = c->id;
		} else {
			adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not find encoder: %s!\n"), codec);
		}
	}
	if (acodec == AV_CODEC_ID_NONE) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> No audio codec detected!\n"));
		Close();
		return false;
	}
	if (!addAudioStream(acodec)) {
		Close();
		return false;
    }

    /* set the output parameters (must be done even if no
       parameters). */
#if LIBAVFORMAT_VERSION_MAJOR < 54
    if (av_set_parameters(oc, NULL) < 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error setting output parameters!\n"));
		Close();
        return false;
    }
#endif

	if (!initAudioEncoder()) {
		Close();
		return false;
	}

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 101)
	av_dump_format(oc, 0, NULL, 1);
#else
	dump_format(oc, 0, NULL, 1);
#endif

#ifdef FFMPEG_USE_URLPROTO
	char filename[64];
	if (sizeof(char *) == 4) {
		sprintf(filename, "ffmpegenc:%u", this);
	} else {
		sprintf(filename, "ffmpegenc:"U64FMT"", this);
	}
	if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not open write handle!\n"));
		Close();
		return false;
    }
	opened = true;
#elif defined(FFMPEG_USE_AVIO)
	oc->pb = avio_handle = adj_create_write_handle();
#else
#error What kind of I/O to use???
#endif

#if LIBAVFORMAT_VERSION_MAJOR > 53 || (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_MINOR >= 4)
    if (avformat_write_header(oc, NULL)) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not write stream header!\n"));
		Close();
		return false;
	}
#else
    if (av_write_header(oc)) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not write stream header!\n"));
		Close();
		return false;
	}
#endif

	ab = adapi->AllocAudioBuffer();
	return true;
}

bool FFMPEG_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}

bool FFMPEG_Encoder::outputBlocks() {
//	int out_size;
    AVCodecContext * c = ast->codec;
    AVPacket pkt;
    av_init_packet(&pkt);

	int frame_size = c->frame_size;

#if defined(FFMPEG_USE_AV_FRAME)
	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error allocating AVFrame!\n"));
		av_free_packet(&pkt);
		return false;
	}


	frame->data[0] = audio_outbuf;
	frame->nb_samples = audio_outbuf_size / sizeof(int16) / inChannels;
	if (!(c->flags & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) {
		frame_size = frame->nb_samples = min(frame->nb_samples, c->frame_size);
	}

	int got_packet_ptr;
	int ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet_ptr);
	if (ret != 0 || got_packet_ptr == 0) {
		if (ret != 0) { adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error encoding audio data!\n")); }
		av_free_packet(&pkt);
		av_frame_free(&frame);
		return (ret == 0) ? true:false;
	}
#else
    pkt.size = avcodec_encode_audio(c, audio_outbuf, audio_outbuf_size, ab->buf);

    pkt.pts = c->coded_frame->pts;
#if defined(AV_PKT_FLAG_KEY)
    pkt.flags |= AV_PKT_FLAG_KEY;
#endif
    pkt.stream_index = ast->index;
    pkt.data = audio_outbuf;
#endif

    /* write the compressed frame in the media file */
    if (av_write_frame(oc, &pkt) != 0) {
        adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error writing audio data!\n"));
		av_free_packet(&pkt);
#ifdef FFMPEG_USE_AV_FRAME
		av_frame_free(&frame);
#endif
        return false;
    }

	memmove(ab->buf, ab->buf + (ast->codec->frame_size * inChannels), (ab->len - (ast->codec->frame_size * inChannels)) * sizeof(short));
	ab->realloc(ab, ab->len - (ast->codec->frame_size * inChannels));
	av_free_packet(&pkt);
#ifdef FFMPEG_USE_AV_FRAME
	av_frame_free(&frame);
#endif
	return true;
}

bool FFMPEG_Encoder::Encode(int samples, const short buf2[]) {
	if (first_encode) {
		first_encode = false;
	}

	int32 olen = ab->len;
	ab->realloc(ab, ab->len + (samples * inChannels));
	memcpy(ab->buf+olen, buf2, samples * inChannels * sizeof(short));

	while (ab->len >= (ast->codec->frame_size*inChannels)) {
		if (!outputBlocks()) {
			return false;
		}
	}

	return true;
}

void FFMPEG_Encoder::Close() {
	if (ast) {
		LockMutex(ffmpegMutex);
		avcodec_close(ast->codec);
		RelMutex(ffmpegMutex);
		av_free(samples);
		av_free(audio_outbuf);
		ast = NULL;
	}

	if (oc) {
#if defined(FFMPEG_USE_URLPROTO)
		if (opened) {
#elif defined(FFMPEG_USE_AVIO)
		if (avio_handle) {
#endif
			av_write_trailer(oc);
		}

		for(unsigned int i = 0; i < oc->nb_streams; i++) {
			av_freep(&oc->streams[i]);
		}

#if defined(FFMPEG_USE_URLPROTO)
		if (opened) {
			url_fclose(oc->pb);
			opened = false;
		}
#elif defined(FFMPEG_USE_AVIO)
		if (avio_handle) {
			adj_destroy_write_handle(avio_handle);
			avio_handle = NULL;
		}
#endif

		av_free(oc);
		oc = NULL;
	}
	if (ab) {
		adapi->FreeAudioBuffer(ab);
		ab = NULL;
	}
}


AutoDJ_Encoder * ffmpeg_enc_create() { return new FFMPEG_Encoder; }
void ffmpeg_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<FFMPEG_Encoder *>(enc);
}

ENCODER ffmpeg_encoder = {
	"ffmpeg",
	mimetype,
	99,

	ffmpeg_enc_create,
	ffmpeg_enc_destroy
};

int FFmpegEncoderCommands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "ffmpeg-oformats")) {
		AVOutputFormat * cur = av_oformat_next(NULL);
		FILE * fp = fopen("oformats.txt", "wb");
		if (fp) {
			ut->std_reply(ut, "List of formats has been written to 'oformats.txt' in your ircbot folder.");
			while (cur) {
				fprintf(fp, "\r\n%s : %s\r\n", cur->name, cur->long_name);
				if (cur->mime_type) {
					fprintf(fp, "\tMime Type: %s\r\n", cur->mime_type);
				}
				if (cur->audio_codec != AV_CODEC_ID_NONE) {
					AVCodec * c = avcodec_find_encoder(cur->audio_codec);
					if (c) {
						fprintf(fp, "\tDefault Audio Encoder: %s : %s\r\n", c->name, c->long_name);
					}
				}
				if (cur->video_codec != AV_CODEC_ID_NONE) {
					AVCodec * c = avcodec_find_encoder(cur->video_codec);
					if (c) {
						fprintf(fp, "\tDefault Video Encoder: %s : %s\r\n", c->name, c->long_name);
					}
				}
				cur = av_oformat_next(cur);
			}
			fclose(fp);
		} else {
			ut->std_reply(ut, "Error opening oformats.txt for writing!");
		}
		return 1;
	}
	if (!stricmp(command, "ffmpeg-encoders")) {
		AVCodec * cur = av_codec_next(NULL);
		FILE * fp = fopen("encoders.txt", "wb");
		if (fp) {
			ut->std_reply(ut, "List of encoders has been written to 'encoders.txt' in your ircbot folder.");
			while (cur) {
#if LIBAVCODEC_VERSION_MAJOR < 54
				if (cur->encode != NULL) {
#else
				if (cur->encode2 != NULL) {
#endif
					fprintf(fp, "%s : %s\r\n", cur->name, cur->long_name);
				}
				cur = av_codec_next(cur);
			}
			fclose(fp);
		} else {
			ut->std_reply(ut, "Error opening oformats.txt for writing!");
		}
		return 1;
	}
	return 0;
}

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	av_register_all();
	av_log_set_level(AV_LOG_QUIET);

#ifdef FFMPEG_USE_URLPROTO
	adj_register_protocols();
#endif

	adapi->RegisterEncoder(&ffmpeg_encoder);
	adapi->RegisterDecoder(&ffmpeg_decoder);

	COMMAND_ACL acl;
	adapi->botapi->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "ffmpeg-oformats",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, FFmpegEncoderCommands, "Shows the available output formats supported by FFmpeg");
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "ffmpeg-encoders",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, FFmpegEncoderCommands, "Shows the available encoders supported by FFmpeg");
	return true;
};

void quit() {
	adapi->botapi->commands->UnregisterCommandByName("ffmpeg-oformats");
	adapi->botapi->commands->UnregisterCommandByName("ffmpeg-encoders");
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"FFmpeg Decoder/Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
