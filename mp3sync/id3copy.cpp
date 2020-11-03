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

#include "mp3sync.h"

#define FULL_MPEG_COPY 1

bool copy_id3(const char * src, const char * dest) {
#ifdef FULL_MPEG_COPY
	TagLib::MPEG::File iTag(src);
	if (iTag.isValid() && (iTag.ID3v1Tag(false) || iTag.ID3v2Tag(false))) {
		TagLib::MPEG::File iTag2(dest);
		if (iTag2.isOpen()) {
			//iTag.tag()->duplicate(iTag.tag(), iTag2.tag(), true);
			TagLib::ID3v1::Tag * tag1 = iTag.ID3v1Tag(false);
			if (tag1) {
				ds_printf(2, "Source has ID3v1...\n");
				TagLib::ID3v1::Tag * ntag = iTag2.ID3v1Tag(true);
				ntag->setAlbum(tag1->album());
				ntag->setArtist(tag1->artist());
				ntag->setComment(tag1->comment());
				ntag->setGenre(tag1->genre());
				ntag->setTitle(tag1->title());
				ntag->setTrack(tag1->track());
				ntag->setYear(tag1->year());
			}
			TagLib::ID3v2::Tag * tag2 = iTag.ID3v2Tag(false);
			if (tag2) {
				ds_printf(2, "Source has ID3v2...\n");
				TagLib::ID3v2::Tag * ntag = iTag2.ID3v2Tag(true);
				tag2->duplicate(tag2, ntag);
				/*
				int frames = tag2->frameList().size();
				for (int i=0; !tag2->isEmpty() && i < frames; i++) {
					TagLib::ID3v2::FrameList::ConstIterator x = tag2->frameList().begin();
					ntag->addFrame(*x);
					tag2->removeFrame(*x, false);
				}
				*/
			}
			if (iTag2.save()) {
				ds_printf(1, "ID3 tag(s) copied!\n");
			} else {
				ds_printf(0, "Error copying ID3 tag(s)!\n");
				return false;
			}
		} else {
			ds_printf(0, "Error opening output to copy ID3 tag(s)!\n");
			return false;
		}
	} else {
		ds_printf(1, "Source file does not have an ID3 tag, could not copy...\n");
	}
#else
	TagLib::FileRef iTag(src);
	if (!iTag.isNull() && !iTag.tag()->isEmpty()) {
		TagLib::MPEG::File iTag2(dest);
		if (iTag2.isOpen()) {
			//iTag.tag()->duplicate(iTag.tag(), iTag2.tag(), true);
			TagLib::Tag * tag1 = iTag.tag();
			if (tag1) {
				ds_printf(2, "Source has ID3v1...\n");
				TagLib::Tag * ntag = iTag2.tag();
				ntag->setAlbum(tag1->album());
				ntag->setArtist(tag1->artist());
				ntag->setComment(tag1->comment());
				ntag->setGenre(tag1->genre());
				ntag->setTitle(tag1->title());
				ntag->setTrack(tag1->track());
				ntag->setYear(tag1->year());
			}
			if (iTag2.save()) {
				ds_printf(1, "ID3 tag(s) copied!\n");
			} else {
				ds_printf(0, "Error copying ID3 tag(s)!\n");
				return false;
			}
		} else {
			ds_printf(0, "Error opening output to copy ID3 tag(s)!\n");
			return false;
		}
	} else {
		ds_printf(1, "Source file does not have an ID3 tag, could not copy...\n");
	}
#endif
	return true;
}
