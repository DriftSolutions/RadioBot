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


#if !defined(__DSLCORE_H__)
#if (defined(WIN32) && _MSC_VER < 1400)
	typedef signed __int32 int32;
	typedef unsigned __int32 uint32;
	typedef signed __int8 int8;
	typedef unsigned __int8 uint8;
#else
	typedef signed int int32;
	typedef unsigned int uint32;
	typedef signed char int8;
	typedef unsigned char uint8;
#endif
#endif

#if defined(WIN32) && defined(LIBSSMT_DLL)
	#ifdef LIBSSMT_EXPORTS
		#define SSMT_API extern "C" __declspec(dllexport)
	#else
		#define SSMT_API extern "C" __declspec(dllimport)
	#endif
#else
	#define SSMT_API
#endif

struct SSMT_VERSION {
	int major;
	int minor;
};

#define SSMT_MAGIC 0x53534D54

enum SSMT_ITEMS {
	SSMT_TITL = 0x5449544C,
	SSMT_ALBM = 0x414C424D,
	SSMT_ARTS =	0x41525453,
	SSMT_GNRE =	0x474E5245,
	SSMT_WURL =	0x5755524C,

	SSMT_STIT =	0x53544954,
	SSMT_SNAM =	0x534E414D,
	SSMT_SGNR =	0x53474E52,
	SSMT_SURL =	0x5355524C
};

struct SSMT_ITEM {
	uint32 name;
	uint32 length;
	union {
		char * str;
		uint8 * data;
		uint32 * uval;
		int32 * ival;
	};
};

struct SSMT_TAG {
	SSMT_ITEM * items; //array of SSMT_ITEM
	int num_items;
	int64 filepos; // file position of tag
	SSMT_TAG *Prev,*Next;
};

struct SSMT_CTX {
	FILE * fp;
	int64 filesize;
	int32 bufsize;// you can set this to something besides default before you call ScanFile, should be a minimum of 4096

	int num_tags;// filled in after ScanFile, tells if any SSMTs have been found
	SSMT_TAG *first_tag, *last_tag;
};

SSMT_API void LibSSMT_GetVersion(SSMT_VERSION * ver);

/**
 * Opens a file for reading and/or writing with LibSSMT.
 *
 * @param fn The file you want to open or create.
 * @param mode The file mode, this is passed directly to fopen. I would definitely recommended including b in the mode to make sure there is no text translation going on.
 * @return Returns a SSMT_CTX handle or NULL. When you are done with the file it should be closed with LibSSMT_CloseFile().
 */
SSMT_API SSMT_CTX * LibSSMT_OpenFile(const char * fn, const char *mode="rb");

/**
 * Scans a file for SSMT tags and fills the SSMT_CTX with tag info.
 *
 * @param ctx The handle to the file you want to scan.
 * @return Returns true if it found any tags, false otherwise.
 */
SSMT_API bool LibSSMT_ScanFile(SSMT_CTX * ctx);

/**
 * Searches a tag for an item with certain name.
 *
 * @param ctx The handle to the file you want to scan.
 * @param name Name of a section to look for
 * @return Returns a pointer to a SSMT_ITEM structure if found or NULL otherwise. This structure is owned by LibSSMT, so do not modify it or use LibSSMT_FreeTag on it.
 * @sa SSMT_ITEMS
 */
SSMT_API SSMT_ITEM * LibSSMT_GetItem(SSMT_TAG * tag, uint32 name);

/**
 * Closes a file previously opened with LibSSMT_OpenFile().
 *
 * @param ctx The handle to the file you want to close.
 */
SSMT_API void LibSSMT_CloseFile(SSMT_CTX * ctx);

/**
 * Create a new (empty) SSMT tag.
 */
SSMT_API SSMT_TAG * LibSSMT_NewTag();

/**
 * Deletes a SSMT tag created with LibSSMT_NewTag().
 *
 * @param tag The tag you want to delete.
 */
SSMT_API void LibSSMT_FreeTag(SSMT_TAG * tag);

/**
 * Adds an item to a SSMT tag created with LibSSMT_NewTag().
 *
 * @param tag The tag you want to delete.
 * @param name The name of the item.
 * @param str The string to set.
 * @sa SSMT_ITEMS
 */
SSMT_API void LibSSMT_AddItem(SSMT_TAG * tag, uint32 name, const char * str);

/**
 * Writes a tag to the file with fwrite
 *
 * @param ctx The handle to the file you want to write to.
 * @param tag The tag you want to write.
 * @return Returns true if tag was written successfully, false otherwise.
 */
SSMT_API bool LibSSMT_WriteTag(SSMT_CTX * ctx, SSMT_TAG * tag);

/**
 * Writes arbitrary data to the file with fwrite
 *
 * @param ctx The handle to the file you want to write to.
 * @param data The data you want to write.
 * @param len The length of data you want to write.
 * @return Returns true if all bytes were written successfully, false otherwise.
 */
SSMT_API bool LibSSMT_WriteFile(SSMT_CTX * ctx, void * data, size_t len);

/**
 * Parses the contents of an SSMT tag and stores the result in a given tag structure
 *
 * @param Scan The empty tag to fill in.
 * @param tag The contents of the SSMT tag
 * @param len The length of the SSMT tag data
 */
SSMT_API void LibSSMT_ParseTagBuffer(SSMT_TAG * Scan, uint8 * tag, uint32 len);

SSMT_API uint32 LibSSMT_UnsynchInt(uint32 in);

