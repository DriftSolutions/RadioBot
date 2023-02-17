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

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include "../adj_plugin.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

extern AD_PLUGIN_API * adapi;
extern DECODER ffmpeg_decoder;
extern Titus_Mutex ffmpegMutex;

#if (LIBAVCODEC_VERSION_MAJOR < 58) || (LIBAVCODEC_VERSION_MAJOR == 58 && LIBAVCODEC_VERSION_MINOR < 91)
#error "You need FFMpeg version 4.3 or higher; this corresponds to Debian 11 or Ubuntu 22.04 LTS. This plugin has just gotten to be too much of a mess supporting all these older versions."
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 59) || (LIBAVCODEC_VERSION_MAJOR == 59 && LIBAVCODEC_VERSION_MINOR >= 24)
#define FFMPEG_USE_CH_LAYOUT
#endif

string ib_av_strerror(int errnum);
AVIOContext * adj_create_read_handle(READER_HANDLE * fnIn);
void adj_destroy_read_handle(AVIOContext *h);
AVIOContext * adj_create_write_handle();
void adj_destroy_write_handle(AVIOContext *h);
