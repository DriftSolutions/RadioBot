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
#ifdef _WIN32
#include <io.h>
#endif

//#if !defined(access) && defined(_access)
//#define access _access
//#endif

#ifdef _DEBUG
Titus_Mutex * ReadFileMutex = new Titus_Mutex;
//static int32 ReadFileCount=0;
#endif


bool read_file_is_my_file(const char * fn) {
	if (access(fn,0) != -1) {
		return true;
	}
	return false;
}

#ifdef _WIN32

int32 read_file_seek(READER_HANDLE * fp, int32  offset, int32 mode) {
	DWORD ret = INVALID_SET_FILE_POINTER;

	switch(mode) {
		case SEEK_SET:
			ret = SetFilePointer(fp->data, offset, NULL, FILE_BEGIN);
			break;
		case SEEK_CUR:
			ret = SetFilePointer(fp->data, offset, NULL, FILE_CURRENT);
			break;
		case SEEK_END:
			ret = SetFilePointer(fp->data, offset, NULL, FILE_END);
			break;
	}

	return (ret != INVALID_SET_FILE_POINTER) ? 0:1;
}

int32 read_file_tell(READER_HANDLE * fp) {
	return SetFilePointer(fp->data, 0, NULL, FILE_CURRENT);
}

int32 read_file_read(void * buf, int32 size, READER_HANDLE * fp) {
	DWORD bread=0;
	if (size == 0) { return 0; }
	unsigned long pos = fp->tell(fp);
	if (ReadFile(fp->data,buf,size,&bread,NULL)) {
		if (bread == 0) { fp->is_eof = true; return 0; }
		return bread;
	}
	return 0;
}

int32 read_file_write(void * buf, int32 objSize, int32 count, READER_HANDLE * fp) {
	DWORD bread=0;
	if (objSize == 0 || count == 0) { return 0; }
	if (WriteFile(fp->data,buf,objSize * count,&bread,NULL)) {
		if (bread == 0) { return 0; }
		return bread / objSize;
	}
	return 0;
}

bool read_file_eof(READER_HANDLE * fp) {
	return fp->is_eof;
}

void read_file_close(READER_HANDLE * fp) {
	CloseHandle(fp->data);
	if (fp->desc) { zfree(fp->desc); }
	if (fp->fn) { zfree(fp->fn); }
	zfree(fp);
}

READER_HANDLE * read_file_open(const char * fn, const char * mode) {
	DWORD bMode = 0;
	bool create = false;
	if (strchr(mode,'r')) {
		bMode |= GENERIC_READ;
		if (strchr(mode, '+')) {
			bMode |= GENERIC_WRITE;
		}
	}
	if (strchr(mode,'w')) {
		create = true;
		bMode |= GENERIC_WRITE;
		if (strchr(mode, '+')) {
			bMode |= GENERIC_READ;
		}
	}

	HANDLE fp = NULL;
	if (create) {
		fp = CreateFile(fn,bMode,FILE_SHARE_READ|FILE_SHARE_READ,NULL,CREATE_ALWAYS,NULL,NULL);
	} else {
		fp = CreateFile(fn,bMode,FILE_SHARE_READ|FILE_SHARE_READ,NULL,OPEN_EXISTING,NULL,NULL);
	}
	if (fp == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	READER_HANDLE * ret = (READER_HANDLE *)zmalloc(sizeof(READER_HANDLE));
	ret->data = fp;
	ret->can_seek = true;
	ret->fileSize = GetFileSize(fp,NULL);

	ret->read = read_file_read;
	ret->write = read_file_write;
	ret->seek = read_file_seek;
	ret->tell = read_file_tell;
	ret->close = read_file_close;
	ret->eof = read_file_eof;

	ret->fn = zstrdup(fn);
	ret->desc = zstrdup("CreateFile");
	strcpy(ret->type,"file");

	return ret;
}

#else // non-Windows systems

int32 read_file_seek(READER_HANDLE * fp, int32  offset, int32 mode) {
	return fseek(fp->fp,offset,mode);
}

int32 read_file_tell(READER_HANDLE * fp) {
	return ftell(fp->fp);
}

int32 read_file_read(void * buf, int32 size, READER_HANDLE * fp) {
	return fread(buf,1,size,fp->fp);
}

int32 read_file_write(void * buf, int32 objSize, int32 count, READER_HANDLE * fp) {
	return fwrite(buf,objSize,count,fp->fp);
}

bool read_file_eof(READER_HANDLE * fp) {
	return feof(fp->fp) ? true:false;
}

void read_file_close(READER_HANDLE * fp) {
	fclose(fp->fp);
	if (fp->desc) { zfree(fp->desc); }
	if (fp->fn) { zfree(fp->fn); }
	zfree(fp);
}

READER_HANDLE * read_file_open(const char * fn, const char * mode) {
	FILE * fp = fopen(fn,mode);
	if (!fp) {
		return NULL;
	}

	READER_HANDLE * ret = (READER_HANDLE *)zmalloc(sizeof(READER_HANDLE));
	ret->fp = fp;
	ret->can_seek = true;
	fseek(fp,0,SEEK_END);
	ret->fileSize = ftell(fp);
	fseek(fp,0,SEEK_SET);

	ret->read = read_file_read;
	ret->write = read_file_write;
	ret->seek = read_file_seek;
	ret->tell = read_file_tell;
	ret->close = read_file_close;
	ret->eof = read_file_eof;

	ret->fn = zstrdup(fn);
	ret->desc = zstrdup("fopen");
	strcpy(ret->type,"file");

	return ret;
}

#endif

