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

#ifndef DSL_STATIC
#define DSL_STATIC
#endif
#include <drift/dslcore.h>
#include <drift/GenLib.h>
#include <drift/buffer.h>
#include "libssmt.h"

#define SSMT_MAJOR 1
#define SSMT_MINOR 0

#pragma pack(1)
struct SSMT_HEADER {
	uint32 name;
	uint32 len;
};
#pragma pack()

inline uint32 SynchInt(uint32 in) {
	int out, mask = 0x7F;

	while (mask ^ 0x7FFFFFFF) {
		out = in & ~mask;
		out <<= 1;
		out |= in & mask;
		mask = ((mask + 1) << 8) - 1;
		in = out;
	}

	return out;
}

uint32 LibSSMT_UnsynchInt(uint32 in) {
	int out = 0, mask = 0x7F000000;

	while (mask) {
		out >>= 1;
		out |= in & mask;
		mask >>= 8;
	}

	return out;
}

void LibSSMT_GetVersion(SSMT_VERSION * v) {
	v->major = SSMT_MAJOR;
	v->minor = SSMT_MINOR;
}

SSMT_API SSMT_CTX * LibSSMT_OpenFile(const char * fn, const char * mode) {
	FILE * fp = fopen(fn,mode);
	if (!fp) {
		return NULL;
	}

	SSMT_CTX * ret = new SSMT_CTX;
	memset(ret,0,sizeof(SSMT_CTX));

	ret->bufsize = 65536;
	fseek64(fp,0,SEEK_END);
	ret->filesize = ftell64(fp);
	if (ret->filesize == -1) {
		fclose(fp);
		delete ret;
		return NULL;
	}
	ret->fp = fp;
	return ret;
}

SSMT_API SSMT_ITEM * LibSSMT_GetItem(SSMT_TAG * tag, uint32 name) {
	if (tag == NULL) { return NULL; }
	for (int i=0; i < tag->num_items; i++) {
		if (tag->items[i].name == name) {
			return &tag->items[i];
		}
	}
	return NULL;
}

void LibSSMT_ParseTagBuffer(SSMT_TAG * Scan, uint8 * tag, uint32 len) {
	uint32 left = len;
	while (left) {
		SSMT_HEADER * h = (SSMT_HEADER *)tag;
		uint32 ilen = LibSSMT_UnsynchInt(Get_UBE32(h->len));
		if (ilen) {
			Scan->num_items++;
			Scan->items = (SSMT_ITEM *)realloc(Scan->items, sizeof(SSMT_ITEM)*Scan->num_items);
			SSMT_ITEM * item = &Scan->items[Scan->num_items-1];

			memset(item, 0, sizeof(SSMT_ITEM));
			item->name = Get_UBE32(h->name);
			item->length = LibSSMT_UnsynchInt(Get_UBE32(h->len));
			item->data = new uint8[item->length+1];
			memcpy(item->data, tag+8, item->length);
			item->data[item->length] = 0;
		}
		tag += (ilen + 8);
		left -= (ilen + 8);
	}
}

void LibSSMT_ParseTag(SSMT_CTX * ctx, int64 filepos, uint8 * tag, uint32 len) {
	SSMT_TAG * Scan = LibSSMT_NewTag();
	Scan->filepos = filepos;
	if (ctx->first_tag) {
		ctx->last_tag->Next = Scan;
		Scan->Prev = ctx->last_tag;
		ctx->last_tag = Scan;
	} else {
		ctx->first_tag = ctx->last_tag = Scan;
	}
	ctx->num_tags++;
	LibSSMT_ParseTagBuffer(Scan, tag, len);
}

SSMT_API bool LibSSMT_ScanFile(SSMT_CTX * ctx) {
	if (!ctx->fp) { return false; }
	if (ctx->num_tags) { return true; }

	fseek64(ctx->fp,0,SEEK_SET);
	char * buf = new char[ctx->bufsize];

	int64 left = ctx->filesize;
	ctx->num_tags = 0;

	while (!feof(ctx->fp) && left > 8) {
		int64 cp = ftell64(ctx->fp);
		left = ctx->filesize - cp;

		int toRead = (left >= ctx->bufsize) ? ctx->bufsize:left;
		if (left < 8) { break; }

		if (!fread(buf,toRead,1,ctx->fp)) {
			// unable to read file
			delete [] buf;
			return false;
		}

		char * start = buf;
		for (int i=0; i < (toRead-4); i++, start++) {
			SSMT_HEADER * h = (SSMT_HEADER *)start;
			if (Get_UBE32(h->name) == SSMT_MAGIC) {
				uint32 tsize = LibSSMT_UnsynchInt(Get_UBE32(h->len));
				fseek64(ctx->fp, cp+i+8, SEEK_SET);
				uint8 * tag = new uint8[tsize];
				if (fread(tag, tsize, 1, ctx->fp) == 1) {
					LibSSMT_ParseTag(ctx, cp+i, tag, tsize);
					//parse tag
				} else {
					delete [] tag;
					delete [] buf;
					return false;
				}
				delete [] tag;
			}
		}

		fseek64(ctx->fp, cp + toRead - 7,SEEK_SET);
	}

	delete [] buf;
	return ctx->num_tags > 0 ? true:false;
}

SSMT_API void LibSSMT_CloseFile(SSMT_CTX * ctx) {
	if (ctx->fp) {
		fclose(ctx->fp);
	}

	SSMT_TAG * Scan = ctx->first_tag;
	SSMT_TAG * toDel = NULL;
	while (Scan) {
		toDel = Scan;
		Scan = Scan->Next;
		LibSSMT_FreeTag(toDel);
	}

	delete ctx;
}


SSMT_API SSMT_TAG * LibSSMT_NewTag() {
	SSMT_TAG * ret = new SSMT_TAG;
	memset(ret, 0, sizeof(SSMT_TAG));
	return ret;
}

SSMT_API void LibSSMT_FreeTag(SSMT_TAG * toDel) {
	for (int i=0; i < toDel->num_items; i++) {
		if (toDel->items[i].data) {
			delete [] toDel->items[i].data;
		}
	}
	if (toDel->items) {
		free(toDel->items);
	}
	delete toDel;
}

SSMT_API void LibSSMT_AddItem(SSMT_TAG * Scan, uint32 name, const char * str) {
	SSMT_ITEM * item = LibSSMT_GetItem(Scan, name);
	if (item == NULL) {
		Scan->num_items++;
		Scan->items = (SSMT_ITEM *)realloc(Scan->items, sizeof(SSMT_ITEM)*Scan->num_items);
		item = &Scan->items[Scan->num_items-1];
	} else {
		if (item->data) { delete [] item->data; }
	}

	memset(item, 0, sizeof(SSMT_ITEM));
	item->name = name;
	item->length = strlen(str)+1;
	item->str = new char[item->length];
	strcpy(item->str, str);
}

SSMT_API bool LibSSMT_WriteTag(SSMT_CTX * ctx, SSMT_TAG * tag) {
	Titus_Buffer buf;
	for (int i=0; i < tag->num_items; i++) {
		SSMT_HEADER h;
		h.name = Get_UBE32(tag->items[i].name);
		h.len = Get_UBE32(SynchInt(tag->items[i].length));
		buf.Append((const char *)&h, sizeof(h));
		buf.Append(tag->items[i].str, tag->items[i].length);
	}
	SSMT_HEADER h;
	h.name = Get_UBE32(SSMT_MAGIC);
	h.len = Get_UBE32(SynchInt(buf.GetLen()));
	buf.Prepend((const char *)&h, sizeof(h));
	return (fwrite(buf.Get(), buf.GetLen(), 1, ctx->fp) == 1) ? true:false;
}

SSMT_API bool LibSSMT_WriteFile(SSMT_CTX * ctx, void * data, size_t len) {
	if (len == 0) { return true; }
	return (fwrite(data, len, 1, ctx->fp) == 1) ? true:false;
}
