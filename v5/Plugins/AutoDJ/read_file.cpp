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

#define READER_USE_OPEN 1
#define READER_USE_FOPEN 2
#define READER_USE_WIN32 3

#ifdef WIN32
#define READER_OPTION READER_USE_WIN32
#else
#define READER_OPTION READER_USE_OPEN
#endif

bool read_file_is_my_file(const char * fn) {
	if (access(fn,0) != -1) {
		return true;
	}
	return false;
}

#if READER_OPTION == READER_USE_WIN32

int64 read_file_seek(READER_HANDLE * fp, int64 offset, int32 mode) {
	DWORD ret = 0;

	LARGE_INTEGER topos, curpos;
	topos.QuadPart = offset;
	curpos.QuadPart = 0;

	switch(mode) {
		case SEEK_SET:
			ret = SetFilePointerEx(fp->data, topos, &curpos, FILE_BEGIN);
			break;
		case SEEK_CUR:
			ret = SetFilePointerEx(fp->data, topos, &curpos, FILE_CURRENT);
			break;
		case SEEK_END:
			ret = SetFilePointerEx(fp->data, topos, &curpos, FILE_END);
			break;
		default:
			api->ib_printf("AutoDJ -> WARNING: Unknown seek mode '%d'!\n", mode);
			break;
	}

	if (ret) {
		return curpos.QuadPart;
	}
	return -1;
}

int64 read_file_tell(READER_HANDLE * fp) {
	LARGE_INTEGER topos, curpos;
	topos.QuadPart = curpos.QuadPart = 0;
	if (SetFilePointerEx(fp->data, topos, &curpos, FILE_CURRENT)) {
		return curpos.QuadPart;
	} else {
		return -1;
	}
}

int32 read_file_read(void * buf, int32 size, READER_HANDLE * fp) {
	if (size == 0) { return 0; }
	DWORD bread = 0;
	if (ReadFile(fp->data,buf,size,&bread,NULL)) {
		if (bread == 0) { fp->is_eof = true; }
		return bread;
	}
	return -1;
}

int32 read_file_write(void * buf, int32 size, READER_HANDLE * fp) {
	DWORD bread=0;
	if (size == 0) { return 0; }
	if (WriteFile(fp->data,buf,size,&bread,NULL)) {
		return bread;
	}
	return -1;
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

READER_HANDLE * read_file_open(const char * fn, int32 mode) {
	DWORD bMode = 0;
	DWORD cMode = 0;
	if (mode & READER_RDONLY) {
		bMode |= GENERIC_READ;
	}
	if (mode & READER_WRONLY) {
		bMode |= GENERIC_WRITE;
	}
	if ((mode & READER_CREATE) && (mode & READER_TRUNC)) {
		cMode = CREATE_ALWAYS;
	} else if (mode & READER_CREATE) {
		cMode = OPEN_ALWAYS;
	} else if (mode & READER_TRUNC) {
		cMode = TRUNCATE_EXISTING;
	} else {
		cMode = OPEN_EXISTING;
	}

	HANDLE fp = INVALID_HANDLE_VALUE;
	int len = MultiByteToWideChar(CP_UTF8, 0, fn, -1, NULL, 0);
	if (len > 0) {
		wchar_t * tmp = new wchar_t[len];
		memset(tmp, 0, sizeof(wchar_t)*len);
		MultiByteToWideChar(CP_UTF8, 0, fn, -1, tmp, len);
		fp = CreateFileW(tmp, bMode, FILE_SHARE_READ | FILE_SHARE_READ, NULL, cMode, NULL, NULL);
		delete[] tmp;
	}
	if (fp == INVALID_HANDLE_VALUE) {
		fp = CreateFile(fn, bMode, FILE_SHARE_READ | FILE_SHARE_READ, NULL, cMode, NULL, NULL);
	}
	if (fp == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	READER_HANDLE * ret = (READER_HANDLE *)zmalloc(sizeof(READER_HANDLE));
	memset(ret, 0, sizeof(READER_HANDLE));
	ret->data = fp;
	ret->can_seek = true;
	LARGE_INTEGER fs;
	memset(&fs,0,sizeof(fs));
	if (GetFileSizeEx(fp, &fs)) {
		ret->fileSize = fs.QuadPart;
	} else {
		api->ib_printf("AutoDJ -> Error getting file size for '%s', falling back to GetFileSize...\n", fn);
		ret->fileSize = GetFileSize(fp,NULL);
	}

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

#elif READER_OPTION == READER_USE_OPEN

int64 read_file_seek(READER_HANDLE * fp, int64 offset, int32 mode) {
#if !defined(WIN32)
	fp->is_eof = false;
#endif
	return lseek64(fp->fd,offset,mode);
}

int64 read_file_tell(READER_HANDLE * fp) {
	return tell64(fp->fd);
}

int32 read_file_read(void * buf, int32 size, READER_HANDLE * fp) {
	if (size == 0) { return 0; }
	int ret = read(fp->fd, buf, size);
#if !defined(WIN32)
	if (ret == 0) { fp->is_eof = true; }
#endif
	return ret;
}

int32 read_file_write(void * buf, int32 size, READER_HANDLE * fp) {
	return write(fp->fd, buf, size);
}

bool read_file_eof(READER_HANDLE * fp) {
#ifdef WIN32
	return eof(fp->fd) ? true:false;
#else
	return fp->is_eof;
#endif
}

void read_file_close(READER_HANDLE * fp) {
	close(fp->fd);
	if (fp->desc) { zfree(fp->desc); }
	if (fp->fn) { zfree(fp->fn); }
	zfree(fp);
}

READER_HANDLE * read_file_open(const char * fn, int32 mode) {
#ifdef WIN32
	int bMode = O_BINARY;
#else
	int bMode = 0;
#endif
	if ((mode & READER_RDWR) == READER_RDWR) {
		bMode |= O_RDWR;
	} else if (mode & READER_RDONLY) {
		bMode |= O_RDONLY;
	} else if (mode & READER_WRONLY) {
		bMode |= O_WRONLY;
	}
	if (mode & READER_CREATE) {
		bMode |= O_CREAT;
	}
	if (mode & READER_TRUNC) {
		bMode |= O_TRUNC;
	}

	int fd = open(fn, bMode, S_IREAD|S_IWRITE);

	READER_HANDLE * ret = (READER_HANDLE *)zmalloc(sizeof(READER_HANDLE));
	ret->fd = fd;
	ret->can_seek = true;
	ret->fileSize = lseek64(fd, 0, SEEK_END);
	lseek64(fd, 0, SEEK_SET);

	ret->read = read_file_read;
	ret->write = read_file_write;
	ret->seek = read_file_seek;
	ret->tell = read_file_tell;
	ret->close = read_file_close;
	ret->eof = read_file_eof;

	ret->fn = zstrdup(fn);
	ret->desc = zstrdup("open");
	strcpy(ret->type,"file");

	return ret;
}

#elif READER_OPTION == READER_USE_FOPEN
// fopen-based I/O

int64 read_file_seek(READER_HANDLE * fp, int64 offset, int32 mode) {
	if (fseek64(fp->fp,offset,mode) == 0) {
		return ftell64(fp->fp);
	}
	return -1;
}

int64 read_file_tell(READER_HANDLE * fp) {
	return ftell64(fp->fp);
}

int32 read_file_read(void * buf, int32 size, READER_HANDLE * fp) {
	return fread(buf,1,size,fp->fp);
}

int32 read_file_write(void * buf, int32 size, READER_HANDLE * fp) {
	return (fwrite(buf,size,1,fp->fp) == 1) ? size:-1;
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

READER_HANDLE * read_file_open(const char * fn, int32 mode) {
	FILE * fp = fopen(fn,fmode);
	if (!fp) {
		return NULL;
	}

	READER_HANDLE * ret = (READER_HANDLE *)zmalloc(sizeof(READER_HANDLE));
	ret->fp = fp;
	ret->can_seek = true;
	fseek64(fp,0,SEEK_END);
	ret->fileSize = ftell64(fp);
	fseek64(fp,0,SEEK_SET);

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

#else
#error No I/O reader set in AutoDJ
#endif
