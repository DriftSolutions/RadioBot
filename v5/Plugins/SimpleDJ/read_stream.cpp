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

bool read_stream_is_my_file(const char * fn) {
	if (!strncmp(fn,"stream://",9) || !strncmp(fn,"http://",7) || !strncmp(fn,"mmsh://",7)) {
		return true;
	}
	return false;
}

struct READER_HANDLE_STREAM {
	T_SOCKET * sock;
	char host[128];
	int32 port;
	char url[128];
	int32 pos;

	int32 bufsize;
	int32 meta_ind;
	bool has_meta;

	bool mmsh;
	Titus_Buffer * buffer;
};

int32 read_stream_seek(READER_HANDLE * fp, int32  offset, int32 mode) {
	return -1;
}

int32 read_stream_tell(READER_HANDLE * fp) {
	READER_HANDLE_STREAM * hs = (READER_HANDLE_STREAM *)fp->data;
	return hs->pos;
}

#pragma pack(1)
struct MMSH_PACKET {
	uint16 id;
	uint16 len;
};

struct MMSH_TYPEOBJ {
	uint32 seq;
	uint8 id, flags;
	uint16 len;
};
#pragma pack()

int32 read_stream_read(void * buf, int32 size, READER_HANDLE * fp) {
	READER_HANDLE_STREAM * hs = (READER_HANDLE_STREAM *)fp->data;
	int n = 0;

	if (hs->has_meta) {

		long i = size;
		long p = hs->bufsize - hs->meta_ind;
		if (p < i) {
			i = p;
		}
		n = sockets->Recv(hs->sock,(char *)buf,i);
		//printf("Attempt: %d, act: %d\n",i,n);

		if (n <= 0) { fp->is_eof = true; return 0; }
		hs->pos += n;
		hs->meta_ind += n;
		if ((hs->meta_ind % hs->bufsize) == 0) {
			unsigned char l;
			sockets->Recv(hs->sock,(char *)&l,1);
			if (l) {
				char * str = new char[(l*16)+1];
				memset(str,0,(l*16)+1);
				sockets->Recv(hs->sock,str,l*16);
				char *p = strstr(str,"StreamTitle=");
				if (p) {
					p = strchr(p,'\'');
					if (p) {
						p++;
						char *q = strchr(p,'\'');
						if (q) {
							q[0]=0;
							TITLE_DATA td;
							memset(&td,0,sizeof(td));
							strlcpy(td.title,p,sizeof(td.title));
							QueueTitleUpdate(&td);
						}
					}
				}
				delete str;
			}
			hs->meta_ind=0;
		}

	} else {
		if (hs->mmsh) {
			if (hs->buffer->GetLen() > 0) {
				n = hs->buffer->GetLen() >= size ? size : hs->buffer->GetLen();
				hs->buffer->Get((char *)buf, n);
				hs->buffer->RemoveFromBeginning(n);
			}
			if (n < size) {
				MMSH_PACKET pack;
				//MMSH_TYPEOBJ * to = NULL;
				while (n < size && sockets->Recv(hs->sock, (char *)&pack, sizeof(pack)) == 4) {
					pack.id = Get_UBE16(pack.id);
					//pack.len = Get_UBE16(pack.len);
					if (pack.len) {
						char * buf2 = (char *)zmalloc(pack.len);
						int n2=0, ind=0;
						int left = pack.len;
						while (left && (n2 = sockets->Recv(hs->sock, buf2 + ind, left)) > 0) {
							ind += n2;
							left -= n2;
						}
						if (n2 >= 0) {
							if (pack.id == 0x2443) {
							}
							if (pack.id == 0x2444 || pack.id == 0xA444) {
								//to = (MMSH_TYPEOBJ *)buf2;
								buf2 += sizeof(MMSH_TYPEOBJ);
								pack.len -= sizeof(MMSH_TYPEOBJ);
								hs->buffer->Append(buf2, pack.len);
								FILE * fp2 = fopen("stream.bin", "ab");
								if (fp2) {
									fwrite(buf2, pack.len, 1, fp2);
									fclose(fp2);
								}
								int toRead = hs->buffer->GetLen() >= size ? size : hs->buffer->GetLen();
								hs->buffer->Get((char *)buf + n, toRead);
								hs->buffer->RemoveFromBeginning(toRead);
								n += toRead;
								buf2 -= sizeof(MMSH_TYPEOBJ);
							}
						}
						zfree(buf2);
					}
				}
			}
			if (n <= 0) { fp->is_eof = true; return 0; }
			hs->pos += n;
		} else {
			n = sockets->Recv(hs->sock,(char *)buf,size);
			if (n <= 0) { fp->is_eof = true; return 0; }
			hs->pos += n;
		}
	}

	return n;
}

int32 read_stream_write(void * buf, int32 objSize, int32 count, READER_HANDLE * fp) {
	READER_HANDLE_STREAM * hs = (READER_HANDLE_STREAM *)fp->data;
	int n = sockets->Send(hs->sock,(char *)buf,objSize * count);
	if (n <= 0) { fp->is_eof = true; return 0; }
	return n / objSize;
}

bool read_stream_eof(READER_HANDLE * fp) {
	return fp->is_eof;
}

void read_stream_close(READER_HANDLE * fp) {
	READER_HANDLE_STREAM * hs = (READER_HANDLE_STREAM *)fp->data;
	sockets->Close(hs->sock);
	if (hs->buffer) { delete hs->buffer; }
	if (fp->desc) { zfree(fp->desc); }
	if (fp->fn) { zfree(fp->fn); }
	delete hs;
	zfree(fp);
}

READER_HANDLE * read_stream_open(const char * fn, const char * mode) {
	if (!strstr(fn,"stream://") && !strstr(fn,"http://") && !strstr(fn,"mmsh://")) { return NULL; }

	bool mmsh = strstr(fn,"mmsh://") ? true:false;

	char buf[4096];
	sstrcpy(buf,fn);
	str_replace(buf,sizeof(buf),"stream://","");
	str_replace(buf,sizeof(buf),"http://","");
	str_replace(buf,sizeof(buf),"mmsh://","");

	char *p = strchr(buf,'/');
	if (p) {
		p[0]=0;
		p++;
	} else {
		p="";;
	}

	READER_HANDLE_STREAM * hs = new READER_HANDLE_STREAM;
	memset(hs,0,sizeof(READER_HANDLE_STREAM));

	sstrcpy(hs->host,buf);
	if (strchr(hs->host,':')) {
		char * q = strchr(hs->host,':');
		q[0]=0;
		q++;
		hs->port = atoi(q);
	}
	if (!hs->port) { hs->port = 80; }
	strlcpy(hs->url,p,sizeof(hs->url));

	hs->sock = sockets->Create();
	if (!hs->sock) {
		delete hs;
		return NULL;
	}

	sockets->SetSendTimeout(hs->sock, 5000);
	sockets->SetRecvTimeout(hs->sock, 30000);
	if (!sockets->Connect(hs->sock,hs->host,hs->port)) {
		sockets->Close(hs->sock);
		delete hs;
		return NULL;
	}

	sockets->SetKeepAlive(hs->sock, true);
	if (mmsh) {
		//,stream-offset=4294967295:4294967295,packet-num=4294967295,
		//Supported: com.microsoft.wm.srvppair, com.microsoft.wm.sswitch, com.microsoft.wm.predstrm, com.microsoft.wm.startupprofile\r\n
		//Pragma: stream-switch-count=1\r\nPragma: stream-switch-entry=ffff:1:0\r\n
		sprintf(buf,"GET /%s HTTP/1.0\r\nAccept: */*\r\nHost: %s\r\nUser-Agent: NSPlayer/7.0.0.3700\r\nX-Accept-Authentication: Negotiate, NTLM, Digest, Basic\r\nPragma: no-cache,rate=1.000,stream-time=0,max-duration=0\r\nPragma: xPlayStrm=1\r\nPragma: client-id=4025837599\r\nPragma: LinkBW=1106891, AccelBW=2147483647, AccelDuration=18000\r\nPragma: xClientGUID={3300AD50-2C39-46c0-AE0A-53817C79AA7B}\r\nAccept-Language: en-us, *;q=0.1\r\n\r\n", hs->url, hs->host);
	} else {
		sprintf(buf,"GET /%s HTTP/1.0\r\nConnection: close\r\nHost: %s:%d\r\nUser-Agent: Drift SimpleDJ (Winamp 2.81)\r\nIcy-MetaData: 1\r\n\r\n", hs->url, hs->host, hs->port);
	}
	sockets->Send(hs->sock,buf);

	READER_HANDLE * ret = (READER_HANDLE *)zmalloc(sizeof(READER_HANDLE));
	memset(ret, 0, sizeof(READER_HANDLE));
	strcpy(ret->mime_type, "audio/mpeg");

	int n=0, line=0;
	while ((n = sockets->RecvLine(hs->sock,buf,sizeof(buf))) >= RL3_NOLINE) {
		if (n == RL3_NOLINE) {
			api->safe_sleep_milli(100);
			continue;
		}
		if (n == 0) {
			break;
		}

		buf[n]=0;
		strtrim(buf,"\r\n");
		if (!strlen(buf)) { break; }

		line++;

		if (line == 1) {
			api->ib_printf("Response: %s\n",buf);
			if (!strstr(buf,"200")) {
				sockets->Close(hs->sock);
				delete hs;
				zfree(ret);
				return NULL;
			}
		} else {
			p = strchr(buf,':');
			if (p) {
				p[0]=0;
				strtrim(buf,"  ");
				p++;
				while (p[0] == ' ' && p[0]) { p++; }
				if (!stricmp(buf,"icy-metaint")) {
					hs->bufsize = atol(p);
					if (hs->bufsize) {
						hs->has_meta = true;
						api->ib_printf("SimpleDJ (read_stream) -> Has Icy MetaData every %d bytes...\n",hs->bufsize);
					}
				}
				if (!stricmp(buf,"content-type")) {
					sstrcpy(ret->mime_type, p);
					api->ib_printf("SimpleDJ (read_stream) -> Mime Type: %s\n", p);
				}
			}
			str_replace(buf,sizeof(buf),"stream://","");
			str_replace(buf,sizeof(buf),"http://","");
			str_replace(buf,sizeof(buf),"mmsh://","");
		}
	}

	if (n == RL3_ERROR) {
		sockets->Close(hs->sock);
		delete hs;
		zfree(ret);
		return NULL;
	}

	if (!hs->bufsize) { hs->bufsize = 8192; }

	hs->mmsh = mmsh;
	hs->buffer = new Titus_Buffer;
	ret->data = hs;

	ret->can_seek = false;
	ret->fileSize = 0;

	ret->read = read_stream_read;
	ret->write = read_stream_write;
	ret->seek = read_stream_seek;
	ret->tell = read_stream_tell;
	ret->close = read_stream_close;
	ret->eof = read_stream_eof;

	ret->fn = zstrdup(fn);
	ret->desc = zstrdup("Sockets");
	strcpy(ret->type,"www");

	return ret;
}

