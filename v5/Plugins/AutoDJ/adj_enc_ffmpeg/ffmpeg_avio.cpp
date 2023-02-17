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

#define AVIO_BUFFER_SIZE 32768

int autodj_read(void *h, uint8_t *buf, int size) {
	READER_HANDLE * rh = (READER_HANDLE *)h;
	return rh->read(buf, size, rh);
}

int autodj_write(void *h, uint8_t *buf, int size) {
	READER_HANDLE * rh = (READER_HANDLE *)h;
	return rh->write((void *)buf, size, rh);
}

int64_t autodj_seek(void *h, int64_t pos, int whence) {
	READER_HANDLE * rh = (READER_HANDLE *)h;
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

AVIOContext * adj_create_read_handle(READER_HANDLE * fnIn) {
	unsigned char * buf = (unsigned char *)av_mallocz(AVIO_BUFFER_SIZE);
	memset(buf, 0, AVIO_BUFFER_SIZE);
	AVIOContext * ret = avio_alloc_context(buf, AVIO_BUFFER_SIZE, 0, fnIn, autodj_read, autodj_write, autodj_seek);
	ret->seekable = (fnIn->can_seek) ? AVIO_SEEKABLE_NORMAL:0;
	return ret;
}

void adj_destroy_read_handle(AVIOContext *h) {
	av_free(h->buffer);
	av_free(h);
}


int ffmpegenc_read(void *h, unsigned char *buf, int size) {
	return -1;
}

int ffmpegenc_write(void *h, unsigned char *buf, int size) {
	//READER_HANDLE * rh = (READER_HANDLE *)h;
	if (adapi->GetFeeder()->Send((unsigned char *)buf, size)) {
		return size;
	}
	return -1;
}

int64_t ffmpegenc_seek(void *h, int64_t pos, int whence) {
	return -1;
}

AVIOContext * adj_create_write_handle() {
	unsigned char * buf = (unsigned char *)av_mallocz(4096);
	memset(buf, 0, 4096);
	AVIOContext * ret = avio_alloc_context(buf, 4096, 1, NULL, ffmpegenc_read, ffmpegenc_write, ffmpegenc_seek);
	return ret;
}

void adj_destroy_write_handle(AVIOContext *h) {
	av_free(h->buffer);
	av_free(h);
}

