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
#ifdef FFMPEG_USE_URLPROTO

#undef FFMPEG_OLD_AVFORMAT
#if !defined(LIBAVFORMAT_VERSION_MAJOR) || LIBAVFORMAT_VERSION_MAJOR < 52 || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR < 67)
#define FFMPEG_OLD_AVFORMAT 1
#endif

int autodj_open(URLContext *h, const char *filename, int flags) {
	if (!strnicmp(filename, "autodj:", 7)) {
		READER_HANDLE * rh = (READER_HANDLE *)atou64(filename+7);
		h->priv_data = rh;
		h->flags |= URL_RDONLY;
		h->flags &= ~URL_WRONLY;
		h->is_streamed = rh->can_seek ? false:true;
		//adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_decoder) -> Opened (%d / %X)\n"), h->is_streamed, h->flags);
		return 0;
	}

	return AVERROR(ENOENT);
}

int autodj_read(URLContext *h, unsigned char *buf, int size) {
	READER_HANDLE * rh = (READER_HANDLE *)h->priv_data;
	int ret = rh->read(buf, size, rh);
	return ret;
}

#if defined(FFMPEG_OLD_AVFORMAT)
int autodj_write(URLContext *h, unsigned char *buf, int size) {
#else
int autodj_write(URLContext *h, const unsigned char *buf, int size) {
#endif
	READER_HANDLE * rh = (READER_HANDLE *)h->priv_data;
	return rh->write((void *)buf, size, rh);
}

int64_t autodj_seek(URLContext *h, int64_t pos, int whence) {
	READER_HANDLE * rh = (READER_HANDLE *)h->priv_data;
	//adapi->botapi->ib_printf("AutoDJ (ffmpeg_decoder) -> Seek: "I64FMT" / %d\n", pos, whence);

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



int ffmpegenc_open(URLContext *h, const char *filename, int flags) {
	if (flags & URL_RDONLY) {
		return AVERROR(ENOENT);
	}
	if (!strnicmp(filename, "ffmpegenc:", 10)) {
		//FFMPEG_Encoder * rh = (FFMPEG_Encoder *)atou64(filename+10);
		h->priv_data = (void *)atou64(filename+10);
		h->flags |= URL_WRONLY;
		h->is_streamed = true;
		adapi->botapi->ib_printf(_("AutoDJ (ffmpeg_encoder) -> Opened handle...\n"));
		return 0;
	}

	return AVERROR(ENOENT);
}

int ffmpegenc_read(URLContext *h, unsigned char *buf, int size) {
	return -1;
}

#if defined(FFMPEG_OLD_AVFORMAT)
int ffmpegenc_write(URLContext *h, unsigned char *buf, int size) {
#else
int ffmpegenc_write(URLContext *h, const unsigned char *buf, int size) {
#endif
	if (adapi->GetFeeder()->Send((unsigned char *)buf, size)) {
		return size;
	}
	return -1;
}

int64_t ffmpegenc_seek(URLContext *h, int64_t pos, int whence) {
	return -1;
}

int ffmpegenc_close(URLContext *h) {
	return 0;
}

static const char reader_protocol_name[] = "autodj";
URLProtocol reader_protocol;
static const char writer_protocol_name[] = "ffmpegenc";
URLProtocol writer_protocol;

void adj_register_protocols() {
	memset(&writer_protocol, 0, sizeof(writer_protocol));
	writer_protocol.name = writer_protocol_name;
	writer_protocol.url_open = ffmpegenc_open;
	writer_protocol.url_read = ffmpegenc_read;
	writer_protocol.url_write = ffmpegenc_write;
	writer_protocol.url_seek = ffmpegenc_seek;
	writer_protocol.url_close = ffmpegenc_close;
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 69)
	av_register_protocol2(&writer_protocol, sizeof(writer_protocol));
#elif (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 29)
	av_register_protocol(&writer_protocol);
#else
	register_protocol(&writer_protocol);
#endif

	memset(&reader_protocol, 0, sizeof(reader_protocol));
	reader_protocol.name = reader_protocol_name;
	reader_protocol.url_open = autodj_open;
	reader_protocol.url_read = autodj_read;
	reader_protocol.url_write = autodj_write;
	reader_protocol.url_seek = autodj_seek;
	reader_protocol.url_close = autodj_close;
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 69)
	av_register_protocol2(&reader_protocol, sizeof(reader_protocol));
#elif (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_MINOR >= 29)
	av_register_protocol(&reader_protocol);
#else
	register_protocol(&reader_protocol);
#endif
}

#endif
