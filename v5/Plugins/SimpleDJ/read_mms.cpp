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

#include "autodj.h"
#if !defined(WIN32) && defined(BLAHBLAHBLAH)
#include <libmms/mms.h>
/*
uint32_t mms_get_length (mms_t *instance);
*/

bool read_mms_is_my_file(char * fn) {
	if (!strnicmp(fn,"mms://",6)) {
		return true;
	}
	return false;
}

struct READER_HANDLE_MMS {
	/*
	T_SOCKET * sock;
	char host[128];
	int32 port;
	char url[128];
	*/
	int32 pos;
	int32 bufsize;
	mms_t * mms;
};

int32 read_mms_seek(READER_HANDLE * fp, int32  offset, int32 mode) {
	return -1;
}

int32 read_mms_tell(READER_HANDLE * fp) {
	READER_HANDLE_MMS * hs = (READER_HANDLE_MMS *)fp->data;
	return hs->pos;
}

int32 read_mms_read(void * buf, int32 size, READER_HANDLE * fp) {
	READER_HANDLE_MMS * hs = (READER_HANDLE_MMS *)fp->data;
	int n = mms_read(NULL, hs->mms, (char *)buf, size);
	printf("SimpleDJ (read_mms) -> Trying to read %d bytes, got %d\n", size, n);
	FILE * fp2 = fopen("stream.bin", "ab");
	if (fp2) {
		fwrite(buf, n, 1, fp2);
		fclose(fp2);
	}
	if (n <= 0) { fp->is_eof = true; return 0; }
	hs->pos += n;
	return n;
}

int32 read_mms_write(void * buf, int32 objSize, int32 count, READER_HANDLE * fp) {
	return -1;
}

bool read_mms_eof(READER_HANDLE * fp) {
	printf("SimpleDJ (read_mms) -> eof(%d)\n", fp->eof);
	return fp->is_eof;
}

void read_mms_close(READER_HANDLE * fp) {
	printf("SimpleDJ (read_mms) -> close()\n");
	READER_HANDLE_MMS * hs = (READER_HANDLE_MMS *)fp->data;
	mms_close(hs->mms);
	if (fp->desc) { zfree(fp->desc); }
	if (fp->fn) { zfree(fp->fn); }
	zfree(hs);
	zfree(fp);
}

READER_HANDLE * read_mms_open(char * fn, char * mode) {
	if (!stristr(fn,"mms://")) { return NULL; }

	READER_HANDLE_MMS * hs = (READER_HANDLE_MMS *)zmalloc(sizeof(READER_HANDLE_MMS));
	memset(hs, 0, sizeof(READER_HANDLE_MMS));
	hs->bufsize = 8192;
	hs->mms = mms_connect(NULL, NULL, (const char *)fn, 0);

	if (!hs->mms) {
		zfree(hs);
		/*
		char * tmp = (char *)zmalloc(strlen(fn)+16);
		strcpy(tmp, fn);
		api->textfunc->StrReplace(tmp, strlen(fn)+16, "mms://", "mmsh://");
		printf("SimpleDJ -> Attempting to fall back to MMSH protocol...\n");
		READER_HANDLE * ret = NULL;//read_stream_open(tmp, mode);
		zfree(tmp);
		return ret;
		*/
		return NULL;
	}

//	printf("len: %u\n", mms_get_length(hs->mms));

	//sockets->SetKeepAlive(hs->sock, true);

	READER_HANDLE * ret = (READER_HANDLE *)zmalloc(sizeof(READER_HANDLE));
	memset(ret, 0, sizeof(READER_HANDLE));
	strcpy(ret->mime_type, "audio/wma");

	ret->data = hs;
	ret->can_seek = false;
	ret->fileSize = 0;
	ret->read = read_mms_read;
	ret->write = read_mms_write;
	ret->seek = read_mms_seek;
	ret->tell = read_mms_tell;
	ret->close = read_mms_close;
	ret->eof = read_mms_eof;

	ret->fn = zstrdup(fn);
	ret->desc = zstrdup("libmms");
	strcpy(ret->type,"raw");

	return ret;
}

#endif
