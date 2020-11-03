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

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include "../adj_plugin.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

extern AD_PLUGIN_API * adapi;
extern DECODER ffmpeg_decoder;
extern Titus_Mutex ffmpegMutex;

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

#undef FFMPEG_USE_AVCODEC_OPEN2
#if defined(FF_API_AVCODEC_OPEN) || LIBAVCODEC_VERSION_MAJOR >= 54
#define FFMPEG_USE_AVCODEC_OPEN2 1
#endif

#undef FFMPEG_USE_AV_OPEN_INPUT
#if (LIBAVFORMAT_VERSION_MAJOR < 52) || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR < 112)
#define FFMPEG_USE_AV_OPEN_INPUT 1
#endif

#undef FFMPEG_USE_AV_NEW_STREAM
#if (LIBAVFORMAT_VERSION_MAJOR < 53) || (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_MINOR < 11)
#define FFMPEG_USE_AV_NEW_STREAM 1
#endif

#if (LIBAVCODEC_VERSION_MAJOR < 56)
#define av_free_packet av_destruct_packet
#endif

#if (LIBAVFORMAT_VERSION_MAJOR > 55) || (LIBAVFORMAT_VERSION_MAJOR == 55 && LIBAVFORMAT_VERSION_MINOR > 38)
typedef struct AVFrac {
    int64_t val, num, den;
} AVFrac;
#endif

#undef FFMPEG_USE_AV_FRAME
#if LIBAVUTIL_VERSION_MAJOR >= 53
#if LIBAVCODEC_VERSION_MAJOR >= 54
#define FFMPEG_USE_AV_FRAME 1
extern "C" {
	#include <libswresample/swresample.h>
	#include <libavutil/opt.h>
}
#endif
#endif

#if (LIBAVCODEC_VERSION_MAJOR < 55) && !defined(FF_API_CODEC_ID)
#define AVCodecID CodecID
#define AV_CODEC_ID_NONE CODEC_ID_NONE
#define AV_CODEC_ID_PCM_S16LE CODEC_ID_PCM_S16LE
#define AV_CODEC_ID_PCM_S16BE CODEC_ID_PCM_S16BE
#define AV_CODEC_ID_PCM_U16LE CODEC_ID_PCM_U16LE
#define AV_CODEC_ID_PCM_U16BE CODEC_ID_PCM_U16BE
#endif

#if defined(AV_CODEC_CAP_TRUNCATED) && !defined(CODEC_CAP_TRUNCATED)
#define CODEC_CAP_TRUNCATED AV_CODEC_CAP_TRUNCATED
#endif
#if defined(AV_CODEC_FLAG_TRUNCATED) && !defined(CODEC_FLAG_TRUNCATED)
#define CODEC_FLAG_TRUNCATED AV_CODEC_FLAG_TRUNCATED
#endif
#if defined(AV_CODEC_CAP_VARIABLE_FRAME_SIZE) && !defined(CODEC_CAP_VARIABLE_FRAME_SIZE)
#define CODEC_CAP_VARIABLE_FRAME_SIZE AV_CODEC_CAP_VARIABLE_FRAME_SIZE
#endif

#undef FFMPEG_USE_AVIO
#undef FFMPEG_USE_URLPROTO
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFILTER_VERSION_MINOR > 102)
#define FFMPEG_USE_AVIO 1
AVIOContext * adj_create_read_handle(READER_HANDLE * fnIn);
void adj_destroy_read_handle(AVIOContext *h);
AVIOContext * adj_create_write_handle();
void adj_destroy_write_handle(AVIOContext *h);
#else
#define FFMPEG_USE_URLPROTO 1
void adj_register_protocols();
#endif
