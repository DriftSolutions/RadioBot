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

#define META_CACHE_EXTERNAL

struct META_CACHE {
	//META_CACHE *Prev,*Next;
	uint32 FileID;
	unsigned char used;
	int64 mTime;
	int32 songLen;
	TITLE_DATA meta;
};

//META_CACHE * AddMetaEntry(META_CACHE * mc);
typedef std::map<unsigned long, META_CACHE> metaCacheList;

class MetaCache {
private:
	sqlite3 * hMetaCache;
	Titus_Mutex hMutex;
	vector<string> queries;
	void commitMeta();
	void freeMetaCache();
public:

	MetaCache();
	~MetaCache();

	META_CACHE * FindFileMeta(unsigned long FileID);
	void SetFileMeta(unsigned long FileID, int64 mTime, int32 songLen, TITLE_DATA * meta);

	//treat as private
	metaCacheList mList;
};
