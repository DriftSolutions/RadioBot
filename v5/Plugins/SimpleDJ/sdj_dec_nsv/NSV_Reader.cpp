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

#include "NSV_Reader.h"

#define NSV_HEADER_MAGIC	0x6656534E
#define NSV_SYNC_MAGIC	0x7356534E
#define NSV_NONSYNC_MAGIC(x) (x == 0xBEEF)

const double fratetab[4] = {
	30.0,
	30.0*1000.0/1001.0,
    25.0,
	24.0*1000.0/1001.0,
};

inline double GetFrameRate(unsigned char fr) {
  if (!(fr&0x80)) return (double)fr;

  double sc;
  int d = (fr & 0x7F)>>2;
  if (d < 16) {
	  sc = 1.0/(double)(d+1);
  } else {
	  sc = d - 15;
  }
  return fratetab[fr&3]*sc;
}

void fmt2str(char * buf, uint32 fmt) {
	sprintf(buf,"%c%c%c%c",fmt & 0xFF,(fmt >> 8) & 0xFF,(fmt >> 16) & 0xFF,(fmt >> 24) & 0xFF);
}

NSV_Reader::NSV_Reader() {
	fp = NULL;
	memset(&file,0,sizeof(NSV_File));
}

#define BAIL_MACRO(x) strlcpy(errBuf, x, errSize); goto bail;
#define NSV_BUFFER_SIZE 4096

bool NSV_Reader::Open(READER_HANDLE * ffp, char * errBuf, size_t errSize) {
	fp = ffp;
	memset(&file,0,sizeof(NSV_File));

	uint8 * buf = new uint8[NSV_BUFFER_SIZE];
	fp->seek(fp, 0, SEEK_SET);
	int origpos = fp->tell(fp);
	int32 fpos = 0;

	int len = fp->read(buf,NSV_BUFFER_SIZE,fp);
	if (len <= 0) {
		BAIL_MACRO("Error reading NSV file!");
	}

	int i;
	for (i=0; i < len-4; i++) {
		uint32 * chk = (uint32 *)&buf[i];
		if (*chk == NSV_HEADER_MAGIC) {
			fp->seek(fp,origpos+i,SEEK_SET);
			if (fp->read(&file.header,sizeof(NSV_Header),fp) != sizeof(NSV_Header)) {
				BAIL_MACRO("Error reading NSVf header!");
			}
			if (file.header.metadata_len > 0 && file.header.metadata_len <= 65535) { //just as a check against bad files
				file.metadata = new char[file.header.metadata_len+1];
				if (fp->read(file.metadata,file.header.metadata_len,fp) != file.header.metadata_len) {
					BAIL_MACRO("Error reading NSVf metadata!");
				}
				file.metadata[file.header.metadata_len]=0;
			}

			if (file.header.toc_alloc > 0 && file.header.toc_alloc >= file.header.toc_size) {
				file.toc = new uint32[file.header.toc_alloc];
				if (fp->read(file.toc,file.header.toc_alloc * 4,fp) != (file.header.toc_alloc * 4)) {
					BAIL_MACRO("Error reading NSVf TOC!");
				}
			}
			break;
		}
	}

	fpos = fp->tell(fp);
	//should be in a position now to get some frames

	while (!fp->eof(fp)) {
		fp->seek(fp,origpos,SEEK_SET);
		uint32 chk=0;
		if (fp->read(&chk,4,fp) == 4) {
			if (chk == NSV_SYNC_MAGIC) {
				fp->seek(fp,origpos,SEEK_SET);
				if (fp->read(&file.info,sizeof(NSV_Sync_Header),fp) != sizeof(NSV_Sync_Header)) {
					BAIL_MACRO("Error reading NSV sync frame!");
				}
				if ((file.info.height%16) != 0 || (file.info.width%16) != 0) {
					api->botapi->ib_printf(_("AutoDJ (nsv_decoder) -> Invalid NSV sync frame, continuing search..."));
				} else {
					break;
				}
			}
		} else {
			BAIL_MACRO("Reached EOF without finding a sync frame!");
		}
		origpos++;
	}

	fp->seek(fp,fpos,SEEK_SET);

	file.framerate = GetFrameRate(file.info.framerate_idx);
	fmt2str(file.vidfmt_str,file.info.vidfmt);
	fmt2str(file.audfmt_str,file.info.audfmt);
	return true;
bail:
	Close();
	return false;
}

NSV_Reader::~NSV_Reader() {
	Close();
}

void NSV_Reader::Close() {
	if (file.metadata) { delete file.metadata; }
	if (file.toc) { delete file.toc; }
	memset(&file, 0, sizeof(file));
	fp = NULL;
}

NSV_File * NSV_Reader::GetFileInformation() {
	return &file;
}

bool NSV_Reader::PopFrame(NSV_Buffer * buf) {
	if (fp == NULL) { return false; }
	if (fp->eof(fp)) { return false; }

	int32 pos = fp->tell(fp);
	while (!fp->eof(fp)) {
		fp->seek(fp,pos,SEEK_SET);
		unsigned short chk=0;
		//unsigned long chk=0;
		if (fp->read(&chk,2,fp) == 2) {
			if (chk == (NSV_SYNC_MAGIC & 0xFFFF) || NSV_NONSYNC_MAGIC(chk)) {
				//printf("%X / %X / %X / %d\n",chk,(NSV_SYNC_MAGIC & 0xFFFF),NSV_SYNC_MAGIC,NSV_NONSYNC_MAGIC(chk));

				if (chk == (NSV_SYNC_MAGIC & 0xFFFF)) {
					NSV_Sync_Header head;
					fp->seek(fp,pos,SEEK_SET);
					if (fp->read(&head,sizeof(NSV_Sync_Header),fp) != sizeof(NSV_Sync_Header)) {
						api->botapi->ib_printf("AutoDJ (nsv_decoder) -> Error reading NSV sync header @ %X!\n",fp->tell(fp));
						return false;
					}
					if (head.sync_hdr != NSV_SYNC_MAGIC) {
						pos++;
						api->botapi->ib_printf("AutoDJ (nsv_decoder) -> Bad Magic in header @ %X\n",fp->tell(fp));
						continue;
					}
					this->AddData(buf,(unsigned char *)&head,sizeof(NSV_Sync_Header));
				} else {
					//fp->seek(fp,pos,SEEK_SET);
					//unsigned char ns[2];
					//fp->read(&ns,sizeof(ns),fp);
					//this->AddData(buf,(unsigned char *)&ns,sizeof(ns));
					this->AddData(buf,(unsigned char *)&chk,sizeof(chk));
				}

				NSV_Frame frame;
				fp->read(&frame,sizeof(frame),fp);
				this->AddData(buf,(unsigned char *)&frame,sizeof(frame));

				/*
				unsigned long lTest = 0x00000000;
				memcpy(&lTest,&frame.num_aux_plus_len[0],3);
				unsigned char cTest[4];
				memcpy(&cTest,&lTest,4);
				cTest[1] = cTest[1];
				*/

				unsigned int num_aux = (frame.num_aux_plus_len[0] & 0x0F);
				unsigned int aux_plus_video_len = (frame.num_aux_plus_len[0] & 0xF0) >> 4;
				aux_plus_video_len |= (frame.num_aux_plus_len[1] & 0xFF) << 4;
				aux_plus_video_len |= (frame.num_aux_plus_len[2] & 0xFF) << 12;

				if (aux_plus_video_len > (524288 + (num_aux *(32768+6))) || num_aux > 15) {
					api->botapi->ib_printf("AutoDJ (nsv_decoder) -> Oversize Payload @ %X\n",fp->tell(fp));
					pos++;
					continue;
				}

				unsigned long llen = aux_plus_video_len;// + (6*num_aux);
				//printf("Video/Aux Len: %u, Audio Len: %u, nu_aux: %u\n",aux_plus_video_len,frame.audio_len,num_aux);
				fp->read(buf->data+buf->ind,llen,fp);
				buf->ind += llen;

				fp->read(buf->data+buf->ind,frame.audio_len,fp);
				buf->ind += frame.audio_len;

				return true;
				break;
			}
		} else {
			return false;
		}
		pos++;
	}
	return false;
}

void NSV_Reader::AddData(NSV_Buffer * buf, unsigned char * data, unsigned long len) {
	memcpy(buf->data+buf->ind,data,len);
	buf->ind += len;
}

void NSV_Reader::ClearBuffer(NSV_Buffer * buf) {
	memset(buf->data,0,buf->len);
	buf->ind=0;
}
