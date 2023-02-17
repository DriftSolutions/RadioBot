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

Titus_Mutex ffmpegMutex;

char mimetype[128]="audio/mpeg";

class FFMPEG_Encoder: public AutoDJ_Encoder {
private:
	AVIOContext * avio_handle = NULL;
	bool first_encode = true;
	Audio_Buffer * ab = NULL;

	const AVOutputFormat * fmt = NULL;
	AVFormatContext * oc = NULL;
	AVStream * ast = NULL;
	AVCodecContext * codec_ctx = NULL;
	char format[64] = "mp3";
	char codec[64] = { 0 };

	int16 *samples = NULL;
	uint8 *audio_outbuf = NULL;
	int audio_outbuf_size=0;
	int audio_input_frame_size=0;

	int inChannels=0;

	bool outputBlocks();
public:
	FFMPEG_Encoder();
	virtual ~FFMPEG_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

FFMPEG_Encoder::FFMPEG_Encoder() {
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

	const AVOutputFormat * fmt = av_guess_format(format, NULL, NULL);
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

bool FFMPEG_Encoder::Init(int channels, int samplerate, float scale) {
	inChannels = (channels == 1) ? 1 : 2;

	fmt = av_guess_format(format, NULL, NULL);
	if (fmt == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not find output format for %s, falling back to MP3...\n"), codec);
		strcpy(format, "mp3");
		fmt = av_guess_format(format, NULL, NULL);
	}
	if (fmt == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not load output format!\n"));
		Close();
		return false;
	}

	/* allocate the output media context */
	oc = avformat_alloc_context();
	if (oc == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not allocate format context!\n"));
		Close();
		return false;
	}
	oc->oformat = fmt;

	AVCodecID acodec = fmt->audio_codec;
	if (codec[0]) {
		const AVCodec * c = avcodec_find_encoder_by_name(codec);
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

	const AVCodec * codec = avcodec_find_encoder(acodec);
	if (codec == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error finding codec encoder!\n"));
		Close();
		return false;
	}

#ifdef FFMPEG_USE_CH_LAYOUT
	const AVChannelLayout * best = NULL;
	const AVChannelLayout * p = codec->ch_layouts;
	while (p != NULL && p->nb_channels) {
		if (p->nb_channels == channels) {
			best = p;
		}
		p++;
	}
	if (best == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error finding matching channel layout!\n"));
		Close();
		return false;
	}
#endif

	AVStream *st = avformat_new_stream(oc, codec);
	if (st == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not create audio stream!\n"));
		Close();
		return false;
	}
	ast = st;
	AVCodecContext * c = codec_ctx = avcodec_alloc_context3(codec);
	if (c == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not create codec context!\n"));
		Close();
		return false;
	}

	c->codec_id = acodec;
	c->codec_type = AVMEDIA_TYPE_AUDIO;
	/* put audio parameters */
	c->bit_rate = adapi->GetOutputBitrate() * 1000;
	c->sample_rate = adapi->GetOutputSample();
	c->sample_fmt = AV_SAMPLE_FMT_S16;
#ifdef FFMPEG_USE_CH_LAYOUT
	av_channel_layout_copy(&c->ch_layout, best);
#else
	uint64_t layout = (channels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	c->channel_layout = layout;
	c->channels = channels;
#endif

	LockMutex(ffmpegMutex);
	if (avcodec_open2(c, NULL, NULL) < 0) {
		RelMutex(ffmpegMutex);
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not open encoder!\n"));
		return false;
	}
	RelMutex(ffmpegMutex);

	audio_outbuf_size = 10000;
	audio_outbuf = (uint8 *)av_malloc(audio_outbuf_size);

	if (c->frame_size <= 1) {
#ifdef FFMPEG_USE_CH_LAYOUT
		audio_input_frame_size = audio_outbuf_size / c->ch_layout.nb_channels;
#else
		audio_input_frame_size = audio_outbuf_size / c->channels;
#endif
		switch (codec->id) {
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
#ifdef FFMPEG_USE_CH_LAYOUT
	samples = (int16 *)av_malloc(audio_input_frame_size * sizeof(int16) * c->ch_layout.nb_channels);
#else
	samples = (int16 *)av_malloc(audio_input_frame_size * sizeof(int16) * c->channels);
#endif
	return true;


#ifdef DEBUG
	av_dump_format(oc, 0, NULL, 1);
#endif
	oc->pb = avio_handle = adj_create_write_handle();

    if (avformat_write_header(oc, NULL)) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Could not write stream header!\n"));
		Close();
		return false;
	}

	ab = adapi->AllocAudioBuffer();
	return true;
}

bool FFMPEG_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}

bool FFMPEG_Encoder::outputBlocks() {
//	int out_size;
	AVFrame *frame = av_frame_alloc();

	AVCodecContext * c = codec_ctx;
	int frame_size = c->frame_size;

	frame->data[0] = audio_outbuf;
	frame->nb_samples = audio_outbuf_size / sizeof(int16) / inChannels;
	if (!(c->flags & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) {
		frame_size = frame->nb_samples = min(frame->nb_samples, c->frame_size);
	}

	if (avcodec_send_frame(c, frame) != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error encoding audio data!\n"));
		av_frame_free(&frame);
		return false;
	}

	AVPacket * pkt = av_packet_alloc();
	while (avcodec_receive_packet(c, pkt) == 0) {
		/* write the compressed frame in the media file */
		if (av_write_frame(oc, pkt) != 0) {
			adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Error writing audio data!\n"));
			av_packet_unref(pkt);
			av_packet_free(&pkt);
			av_frame_free(&frame);
			return false;
		}
		av_packet_unref(pkt);
	}
	av_packet_free(&pkt);

	memmove(ab->buf, ab->buf + (frame_size * inChannels), (ab->len - (frame_size * inChannels)) * sizeof(short));
	ab->realloc(ab, ab->len - (frame_size * inChannels));
	av_frame_free(&frame);
	return true;
}

bool FFMPEG_Encoder::Encode(int samples, const short buf2[]) {
	if (first_encode) {
		first_encode = false;
	}

	int32 olen = ab->len;
	ab->realloc(ab, ab->len + (samples * inChannels));
	memcpy(ab->buf+olen, buf2, samples * inChannels * sizeof(short));

	while (ab->len >= (codec_ctx->frame_size*inChannels)) {
		if (!outputBlocks()) {
			return false;
		}
	}

	return true;
}

void FFMPEG_Encoder::Close() {
	if (ast) {
		LockMutex(ffmpegMutex);
		avcodec_close(codec_ctx);
		RelMutex(ffmpegMutex);
		avcodec_free_context(&codec_ctx);
		ast = NULL;
	}

	if (samples != NULL) {
		av_free(samples);
	}
	if (audio_outbuf != NULL) {
		av_free(audio_outbuf);
	}

	if (oc) {
		if (avio_handle) {
			av_write_trailer(oc);
		}
		avformat_free_context(oc);
		if (avio_handle) {
			adj_destroy_write_handle(avio_handle);
			avio_handle = NULL;
		}
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
		FILE * fp = fopen("oformats.txt", "wb");
		if (fp) {
			void *fmt_iter = NULL;
			const AVOutputFormat * cur = NULL;
			while (cur = av_muxer_iterate(&fmt_iter)) {
				fprintf(fp, "\r\n%s : %s\r\n", cur->name, cur->long_name);
				if (cur->mime_type) {
					fprintf(fp, "\tMime Type: %s\r\n", cur->mime_type);
				}
				if (cur->audio_codec != AV_CODEC_ID_NONE) {
					const AVCodec * c = avcodec_find_encoder(cur->audio_codec);
					if (c) {
						fprintf(fp, "\tDefault Audio Encoder: %s : %s\r\n", c->name, c->long_name);
					}
				}
				if (cur->video_codec != AV_CODEC_ID_NONE) {
					const AVCodec * c = avcodec_find_encoder(cur->video_codec);
					if (c) {
						fprintf(fp, "\tDefault Video Encoder: %s : %s\r\n", c->name, c->long_name);
					}
				}
			}
			fclose(fp);
			ut->std_reply(ut, "List of formats has been written to 'oformats.txt' in your ircbot folder.");
		} else {
			ut->std_reply(ut, "Error opening oformats.txt for writing!");
		}
		return 1;
	}
	if (!stricmp(command, "ffmpeg-encoders")) {
		FILE * fp = fopen("encoders.txt", "wb");
		if (fp) {
			void *fmt_iter = NULL;
			const AVCodec * cur = NULL;
			while (cur = av_codec_iterate(&fmt_iter)) {
				if (av_codec_is_encoder(cur)) {
					fprintf(fp, "%s : %s\r\n", cur->name, cur->long_name);
				}
			}
			fclose(fp);
			ut->std_reply(ut, "List of encoders has been written to 'encoders.txt' in your ircbot folder.");
		} else {
			ut->std_reply(ut, "Error opening encoders.txt for writing!");
		}
		return 1;
	}
	return 0;
}

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	av_log_set_level(AV_LOG_QUIET);

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
