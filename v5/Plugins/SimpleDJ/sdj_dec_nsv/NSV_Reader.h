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


#ifndef _NSV_READER_H_
#define _NSV_READER_H_

#include "../adj_plugin.h"
extern SD_PLUGIN_API * api;

#define MAX_FRAME_SIZE 1179768

#pragma pack(1)
struct NSV_Header {
	uint32 nsv_sig;
	uint32 header_size;
	uint32 file_size;
	uint32 file_len_ms;
	uint32 metadata_len;
	uint32 toc_alloc;
	uint32 toc_size;
};

struct NSV_Sync_Header {
	uint32 sync_hdr;
	uint32 vidfmt; // video format (see Appendix C for valid formats)
	uint32 audfmt; // audio format (see Appendix C for valid formats)
	unsigned short width;
	unsigned short height;
	unsigned char framerate_idx;
	short syncoffs;
};

struct NSV_Frame {
	unsigned char num_aux_plus_len[3];
	unsigned short audio_len;
};
#pragma pack()

struct NSV_File {
	NSV_Header header;
	char * metadata;
	uint32 * toc;
	NSV_Sync_Header info;

	double framerate;
	char vidfmt_str[5],audfmt_str[5];
};

struct NSV_Buffer {
	unsigned char * data;
	unsigned long len;
	unsigned long ind;
};

class NSV_Reader {
	private:
		READER_HANDLE * fp;
		NSV_File file;
	public:
		NSV_Reader();
		~NSV_Reader();

		bool Open(READER_HANDLE * ffp, char * error, size_t errSize);
		void Close();
		NSV_File * GetFileInformation();

		bool PopFrame(NSV_Buffer * buf);
		void AddData(NSV_Buffer * buf, unsigned char * data, unsigned long len);
		void ClearBuffer(NSV_Buffer * buf);
};

#endif // _NSV_READER_H_
