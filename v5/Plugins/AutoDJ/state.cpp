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
#if defined(WIN32)
#include "../../../Common/autodj.pb.h"
#if defined(DEBUG)
#pragma comment(lib, "libprotobuf_d.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif
#else
#include "../../src/proto_linux/autodj.pb.h"
#endif

#define STATE_VERSION 2

bool SerializeQueue(QUEUE * q, pbQUEUE &s) {
	if (q == NULL || q->path == NULL || q->fn == NULL) {
		return false;
	}

	//pbQUEUE s;
	s.set_id(q->ID);
	s.set_path(q->path);
	s.set_fn(q->fn);
	if (q->lastPlayed > 0) {
		s.set_lastplayed(q->lastPlayed);
	}
	if (q->mTime > 0) {
		s.set_mtime(q->mTime);
	}
	if (q->songlen > 0) {
		s.set_songlen(q->songlen);
	}
	if (q->flags != 0) {
		s.set_flags(q->flags);
	}
	if (q->meta && q->meta->title[0]) {
		pbTITLE_DATA * td = s.mutable_meta();
		td->set_artist(q->meta->artist);
		td->set_album(q->meta->album);
		td->set_title(q->meta->title);
		td->set_album_artist(q->meta->album_artist);
		td->set_genre(q->meta->genre);

		td->set_url(q->meta->url);
		td->set_comment(q->meta->comment);
		if (q->meta->track_no) {
			td->set_track_no(q->meta->track_no);
		}
		if (q->meta->year > 0) {
			td->set_year(q->meta->year);
		}
	}
	if (q->isreq) {
		pbREQ_INFO * td = s.mutable_isreq();
		td->set_netno(q->isreq->netno);
		td->set_nick(q->isreq->nick);
		td->set_channel(q->isreq->channel);
		if (q->isreq->playAfter > 0) {
			td->set_playafter(q->isreq->playAfter);
		}
	}
	return true;
}

char * SerializeQueue(QUEUE * q, int * size) {
	pbQUEUE s;
	if (!SerializeQueue(q, s)) {
		return NULL;
	}
	*size = s.ByteSize();
	char * ret = (char *)zmalloc(*size);
	s.SerializeToArray(ret, *size);
	return ret;
}

char * SerializeQueueList(QUEUE * q, int * size) {
	pbQUEUE_LIST list;
	QUEUE * Scan = q;
	while (Scan != NULL) {
		pbQUEUE s;
		if (SerializeQueue(Scan, s)) {
			pbQUEUE * x = list.add_list();
			*x = s;
		}
		Scan = Scan->Next;
	}
	*size = list.ByteSize();
	char * ret = (char *)zmalloc(*size);
	list.SerializeToArray(ret, *size);
	return ret;
}

QUEUE * UnserializeQueue(const char * buf, int size) {
	pbQUEUE s;
	if (s.ParseFromArray(buf, size)) {
		QUEUE * ret = AllocQueue();
		memset(ret, 0, sizeof(QUEUE));
		ret->ID = s.id();
		ret->path = zstrdup(s.path().c_str());
		ret->fn = zstrdup(s.fn().c_str());
		if (s.has_mtime()) {
			ret->mTime = s.mtime();
		}
		if (s.has_lastplayed()) {
			ret->lastPlayed = s.lastplayed();
		}
		if (s.has_songlen()) {
			ret->songlen = s.songlen();
		}
		if (s.has_req_count()) {
			ret->req_count = s.req_count();
		}
		if (s.has_flags()) {
			ret->flags = s.flags();
		}
		if (s.has_meta()) {
			pbTITLE_DATA td = s.meta();
			ret->meta = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
			memset(ret->meta, 0, sizeof(TITLE_DATA));
			sstrcpy(ret->meta->artist, td.artist().c_str());
			sstrcpy(ret->meta->album, td.album().c_str());
			sstrcpy(ret->meta->title, td.title().c_str());
			sstrcpy(ret->meta->album_artist, td.album_artist().c_str());
			sstrcpy(ret->meta->genre, td.genre().c_str());
			sstrcpy(ret->meta->url, td.url().c_str());
			sstrcpy(ret->meta->comment, td.comment().c_str());
			if (td.has_track_no()) {
				ret->meta->track_no = td.track_no();
			}
			if (td.has_year()) {
				ret->meta->year = td.year();
			}
		}
		if (s.has_isreq()) {
			pbREQ_INFO td = s.isreq();
			ret->isreq = (REQ_INFO *)zmalloc(sizeof(REQ_INFO));
			memset(ret->isreq, 0, sizeof(REQ_INFO));
			sstrcpy(ret->isreq->nick, td.nick().c_str());
			sstrcpy(ret->isreq->channel, td.channel().c_str());
			if (td.has_netno()) {
				ret->isreq->netno = td.netno();
			}
			if (td.has_playafter()) {
				ret->isreq->playAfter = td.playafter();
			}
		}
		return ret;
	}
	return NULL;
}

QUEUE * UnserializeQueueList(const char * buf, int size) {
	pbQUEUE_LIST s;
	QUEUE *ret = NULL, *lQue=NULL;
	if (s.ParseFromArray(buf, size)) {
		for (int i = 0; i < s.list_size(); i++) {
			string str = s.list(i).SerializeAsString();
			QUEUE * x = UnserializeQueue(str.c_str(), str.length());
			if (x != NULL) {
				if (ret == NULL) {
					ret = lQue = x;
				} else {
					lQue->Next = x;
					x->Prev = lQue;
					lQue = x;
				}
			}
		}
	}
	return ret;
}

void LoadState() {
	AutoMutex(QueueMutex);
	uint32 ver1 = Get_UBE32(STATE_VERSION), ver2 = 0;

	QUEUE * lQue = rQue;
	while (lQue && lQue->Next) {
		lQue = lQue->Next;
	}
	FILE * fp = fopen("./backup/adj.requests.state", "rb");
	if (fp != NULL) {
		if (fread(&ver2, sizeof(ver2), 1, fp) == 1 && ver1 == ver2) {
			int size = 0;
			if (fread(&size, sizeof(size), 1, fp) == 1 && size > 0) {
				char * buf = (char *)zmalloc(size);
				if (fread(buf, size, 1, fp) == 1) {
					QUEUE * Scan = UnserializeQueueList(buf, size);
					if (Scan != NULL) {
						api->ib_printf("AutoDJ -> Re-adding %s to request que...\n", Scan->fn);
						if (lQue != NULL) {
							lQue->Next = Scan;
							Scan->Prev = lQue;
							lQue = Scan;
						} else {
							rQue = lQue = Scan;
						}
					}
				}
				zfree(buf);
			}
		}
		fclose(fp);
		remove("./backup/adj.requests.state");
	}

	lQue = pQue;
	while (lQue && lQue->Next) {
		lQue = lQue->Next;
	}
	fp = fopen("./backup/adj.main.state", "rb");
	if (fp != NULL) {
		if (fread(&ver2, sizeof(ver2), 1, fp) == 1 && ver1 == ver2) {
			int size = 0;
			if (fread(&size, sizeof(size), 1, fp) == 1 && size > 0) {
				char * buf = (char *)zmalloc(size);
				if (fread(buf, size, 1, fp) == 1) {
					QUEUE * Scan = UnserializeQueueList(buf, size);
					if (Scan != NULL) {
						api->ib_printf("AutoDJ -> Re-adding %s to main que...\n", Scan->fn);
						if (lQue != NULL) {
							lQue->Next = Scan;
							Scan->Prev = lQue;
							lQue = Scan;
						} else {
							pQue = lQue = Scan;
						}
					}
				}
				zfree(buf);
			}
		}
		fclose(fp);
		remove("./backup/adj.main.state");
	}

	fp = fopen("./backup/adj.nextsong.state", "rb");
	if (fp != NULL) {
		if (fread(&ver2, sizeof(ver2), 1, fp) == 1 && ver1 == ver2) {
			int size = 0;
			if (fread(&size, sizeof(size), 1, fp) == 1 && size > 0) {
				char * buf = (char *)zmalloc(size);
				if (fread(buf, size, 1, fp) == 1) {
					QUEUE * Scan = UnserializeQueue(buf, size);
					if (Scan != NULL) {
						api->ib_printf("AutoDJ -> Re-adding %s as next song...\n", Scan->fn);
						ad_config.nextSong = Scan;
					}
				}
				zfree(buf);
			}
		}
		fclose(fp);
		remove("./backup/adj.nextsong.state");
	}

	fp = fopen("./backup/adj.filter.state", "rb");
	if (fp != NULL) {
		if (fread(&ver2, sizeof(ver2), 1, fp) == 1 && ver1 == ver2) {
			if (ad_config.Filter) {
				zfree(ad_config.Filter);
			}

			ad_config.Filter = (TIMER *)zmalloc(sizeof(TIMER));
			if (fread(ad_config.Filter, sizeof(TIMER), 1, fp) == 1 && time(NULL) < ad_config.Filter->extra) {
				api->ib_printf("AutoDJ -> Re-adding filter...\n");
			} else {
				zfree(ad_config.Filter);
				ad_config.Filter = NULL;
			}
		}
		fclose(fp);
	}

	fp = fopen("./backup/adj.lists.state", "rb");
	if (fp != NULL) {
		char buf[8192];
		int in_section = 0;
		while (!feof(fp) && fgets(buf, sizeof(buf), fp)) {
			strtrim(buf);
			if (buf[0] == 0) { continue; }
			if (buf[0] == '[') { in_section = 0; }
			if (!stricmp(buf, "[songs_requested]")) {
				in_section = 1;
				continue;
			} else if (!stricmp(buf, "[artists_played]")) {
				in_section = 2;
				continue;
			} else if (!stricmp(buf, "[users_requested]")) {
				in_section = 3;
				continue;
			} else if (!stricmp(buf, "[artists_requested]")) {
				in_section = 4;
				continue;
			}

			if (in_section > 0) {
				char * p = strchr(buf, '=');
				if (p) {
					*p = 0;
					p++;

					if (in_section == 1) {
						requestList[atoul(p)] = atoi64(buf);
					} else if (in_section == 2) {
						artistList[p] = atoi64(buf);
					} else if (in_section == 3) {
						requestUserList[p] = atoi64(buf);
					} else if (in_section == 4) {
						requestArtistList[p] = atoi64(buf);
					}
				}
			}
		}
		fclose(fp);
		remove("./backup/adj.lists.state");
	}
}

void SaveState() {
	AutoMutex(QueueMutex);

	uint32 ver1 = Get_UBE32(STATE_VERSION);

	FILE * fp = fopen("./backup/adj.requests.state", "wb");
	if (fp != NULL) {
		fwrite(&ver1, sizeof(ver1), 1, fp);
		int size = 0;
		char * txt = SerializeQueueList(rQue, &size);
		if (txt && size > 0) {
			fwrite(&size, sizeof(size), 1, fp);
			fwrite(txt, size, 1, fp);
		}
		fclose(fp);
	} else {
		api->ib_printf(_("AutoDJ -> Error opening ./backup/adj.requests.state\n"));
	}

	fp = fopen("./backup/adj.main.state", "wb");
	if (fp != NULL) {
		fwrite(&ver1, sizeof(ver1), 1, fp);
		int size = 0;
		char * txt = SerializeQueueList(pQue, &size);
		if (txt && size > 0) {
			fwrite(&size, sizeof(size), 1, fp);
			fwrite(txt, size, 1, fp);
		}
		fclose(fp);
	} else {
		api->ib_printf(_("AutoDJ -> Error opening ./backup/adj.main.state\n"));
	}

	fp = fopen("./backup/adj.nextsong.state", "wb");
	if (fp != NULL) {
		fwrite(&ver1, sizeof(ver1), 1, fp);
		int size = 0;
		char * txt = SerializeQueue(ad_config.nextSong, &size);
		if (txt && size > 0) {
			fwrite(&size, sizeof(size), 1, fp);
			fwrite(txt, size, 1, fp);
		}
		fclose(fp);
	} else {
		api->ib_printf(_("AutoDJ -> Error opening ./backup/adj.nextsong.state\n"));
	}

	fp = fopen("./backup/adj.lists.state", "wb");
	if (fp != NULL) {
		fprintf(fp, "[songs_requested]\r\n");
		for (RequestListType::const_iterator x = requestList.begin(); x != requestList.end(); x++) {
			fprintf(fp, "" I64FMT "=%u\r\n", x->second, x->first);
		}
		fprintf(fp, "\r\n[artists_played]\r\n");
		for (ArtistListType::const_iterator x = artistList.begin(); x != artistList.end(); x++) {
			fprintf(fp, "" I64FMT "=%s\r\n", x->second, x->first.c_str());
		}
		fprintf(fp, "\r\n[users_requested]\r\n");
		for (RequestUserListType::const_iterator x = requestUserList.begin(); x != requestUserList.end(); x++) {
			fprintf(fp, "" I64FMT "=%s\r\n", x->second, x->first.c_str());
		}
		fprintf(fp, "\r\n[artists_requested]\r\n");
		for (RequestUserListType::const_iterator x = requestArtistList.begin(); x != requestArtistList.end(); x++) {
			fprintf(fp, "" I64FMT "=%s\r\n", x->second, x->first.c_str());
		}
		fclose(fp);
	} else {
		api->ib_printf(_("AutoDJ -> Error opening ./backup/adj.lists.state\n"));
	}

	fp = fopen("./backup/adj.filter.state", "wb");
	if (fp != NULL) {
		fwrite(&ver1, sizeof(ver1), 1, fp);
		if (ad_config.Filter != NULL && time(NULL) < ad_config.Filter->extra) {
			fwrite(ad_config.Filter, sizeof(TIMER), 1, fp);
			//COMPILE_TIME_ASSERT(sizeof(*ad_config.Filter) == sizeof(TIMER));
		}
		fclose(fp);
	} else {
		api->ib_printf(_("AutoDJ -> Error opening ./backup/adj.filter.state\n"));
	}
}

