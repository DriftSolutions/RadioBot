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

#include "ircbot.h"
#if defined(IRCBOT_ENABLE_SS)

int RatingCommands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

struct RSONG_INFO {
	char Song[256];
	uint32 Rating, Votes, AutoDJ_ID;
};

inline void createRatingsTable() {
	Query("CREATE TABLE IF NOT EXISTS Ratings(Song VARCHAR(255) COLLATE NOCASE, SongID UNSIGNED INTEGER DEFAULT 0, Rating INTEGER DEFAULT 0, Votes INTEGER DEFAULT 0);", NULL, NULL, NULL);
	Query("CREATE UNIQUE INDEX IF NOT EXISTS RatingsUnique ON Ratings (Song, SongID)", NULL, NULL, NULL);
}

void RebuildRatings() {
	Query("DROP TABLE Ratings");
	createRatingsTable();

	std::stringstream sstr;
	sstr << "SELECT DISTINCT SongID FROM Votes WHERE SongID>0";
	sql_rows rows = GetTable((char *)sstr.str().c_str(), NULL);
	if (rows.size() > 0) {
		for (sql_rows::iterator x = rows.begin(); x != rows.end(); x++) {
			uint32 id = atoul(x->second["SongID"].c_str());
			RateSong(id, "ratings-rebuild-int", 0);
		}
	}

	std::stringstream sstr2;
	sstr2 << "SELECT DISTINCT Song,SongID FROM Votes WHERE Song<>'' ORDER BY SongID DESC";
	rows = GetTable((char *)sstr2.str().c_str(), NULL);
	if (rows.size() > 0) {
		for (sql_rows::iterator x = rows.begin(); x != rows.end(); x++) {
			RateSong(x->second["Song"].c_str(), "ratings-rebuild-int", 0, atoul(x->second["SongID"].c_str()));
		}
	}
}

bool InitRatings() {
	createRatingsTable();
	Query("CREATE TABLE IF NOT EXISTS Votes(Song VARCHAR(255) COLLATE NOCASE, SongID UNSIGNED INTEGER DEFAULT 0, Rating INTEGER DEFAULT 0, Nick VARCHAR(255));", NULL, NULL, NULL);

	if (config.base.EnableRatings) {
		COMMAND_ACL acl;
		FillACL(&acl, 0, 0, 0);
		RegisterCommand("rate",			&acl, CM_ALLOW_ALL,	RatingCommands,	_("This command will show you to rate the currently playing song"));
		RegisterCommand("rating",		&acl, CM_ALLOW_ALL,	RatingCommands,	_("This command will show you to view the rating of the currently playing song"));
#if defined(DEBUG)
		FillACL(&acl, 0, UFLAG_MASTER, 0);
		RegisterCommand("ratings-rebuild",			&acl, CM_ALLOW_ALL_PRIVATE,	RatingCommands,	_("Rebuilds the rating DB..."));
#endif
	}
	return true;
}

void GetSongInfo(const char * ssong, RSONG_INFO * r) {
	memset(r, 0, sizeof(RSONG_INFO));
	if (config.base.EnableRatings) {
		char * song = MPrintf("%q", ssong);
		std::stringstream sstr;
		sstr << "SELECT * FROM Ratings WHERE Song LIKE '" << song << "'";
		sql_rows rows = GetTable((char *)sstr.str().c_str(), NULL);
		if (rows.size() > 0) {
			r->Rating = atoul(rows[0]["Rating"].c_str());
			r->Votes = atoul(rows[0]["Votes"].c_str());
			r->AutoDJ_ID = atoul(rows[0]["SongID"].c_str());
			sstrcpy(r->Song, rows[0]["Song"].c_str());
		}
		Free(song);
	}
}
void GetSongInfo(uint32 id, RSONG_INFO * r) {
	memset(r, 0, sizeof(RSONG_INFO));
	if (config.base.EnableRatings) {
		std::stringstream sstr;
		sstr << "SELECT * FROM Ratings WHERE SongID=" << id;
		sql_rows rows = GetTable((char *)sstr.str().c_str(), NULL);
		if (rows.size() > 0) {
			r->Rating = atoul(rows[0]["Rating"].c_str());
			r->Votes = atoul(rows[0]["Votes"].c_str());
			r->AutoDJ_ID = atoul(rows[0]["SongID"].c_str());
			sstrcpy(r->Song, rows[0]["Song"].c_str());
		}
	}
}
void GetSongRating(uint32 id, SONG_RATING * r) {
	memset(r, 0, sizeof(SONG_RATING));
	RSONG_INFO ri;
	GetSongInfo(id,&ri);
	r->Rating = ri.Rating;
	r->Votes = ri.Votes;
}
void GetSongRating(const char * song, SONG_RATING * r) {
	memset(r, 0, sizeof(SONG_RATING));
	RSONG_INFO ri;
	GetSongInfo(song,&ri);
	r->Rating = ri.Rating;
	r->Votes = ri.Votes;
}

void RateSong(const char * ssong, const char * nick, uint32 rating) {
	RateSong(ssong, nick, rating, 0);
}
void RateSong(const char * ssong, const char * nick, uint32 rating, uint32 id) {
	if (config.base.EnableRatings) {
		RSONG_INFO ri;
		GetSongInfo(ssong,&ri);
		if (id != 0) {
			// set SongID
			ri.AutoDJ_ID = id;
		}

		char * song = MPrintf("%q", ssong);

		std::stringstream sstr;
		sstr << "DELETE FROM Votes WHERE Song LIKE '" << song << "' AND Nick LIKE '" << nick << "'";
		Query((char *)sstr.str().c_str(), NULL, NULL, NULL);

		if (rating > 0) {
			std::stringstream sstr2;
			sstr2 << "INSERT INTO Votes (`Song`,`SongID`,`Nick`,`Rating`) VALUES (\"" << song << "\", " << ri.AutoDJ_ID << ", '" << nick << "', " << rating << ")";
			Query((char *)sstr2.str().c_str(), NULL, NULL, NULL);
		}

		std::stringstream sstr3;
		sstr3 << "SELECT COUNT(*),SUM(Rating) FROM Votes WHERE Song LIKE '" << song << "'";
		sql_rows rows = GetTable((char *)sstr3.str().c_str(), NULL);
		uint32 num = 0;
		if (rows.size() > 0) {
			num = atoul(rows[0]["COUNT(*)"].c_str());
			rating = atoul(rows[0]["SUM(Rating)"].c_str());
			if (num) {
				rating /= num;
			}
		} else {
			rating = 0;
		}

		std::stringstream sstr4;
		sstr4 << "REPLACE INTO Ratings (`Song`,`SongID`,`Rating`,`Votes`) VALUES (\"" << song << "\", " << ri.AutoDJ_ID << ", " << rating << ", " << num << ")";
		Query((char *)sstr4.str().c_str(), NULL, NULL, NULL);

		IBM_RATING ir;
		ir.AutoDJ_ID = ri.AutoDJ_ID;
		ir.song = ssong;
		ir.rating = rating;
		ir.votes = num;
		SendMessage(-1, IB_ON_SONG_RATING, (char *)&ir, sizeof(ir));
		Free(song);
	}
}
void RateSong(uint32 song, const char * nick, uint32 rating) {
	if (config.base.EnableRatings) {
		RSONG_INFO ri;
		GetSongInfo(song,&ri);
		char * msong = MPrintf("%q", ri.Song);

		std::stringstream sstr;
		sstr << "DELETE FROM Votes WHERE SongID=" << song << " AND Nick LIKE '" << nick << "'";
		Query((char *)sstr.str().c_str(), NULL, NULL, NULL);

		if (rating > 0) {
			std::stringstream sstr2;
			sstr2 << "INSERT INTO Votes (`Song`,`SongID`,`Nick`,`Rating`) VALUES (\"" << msong << "\", '" << song << "', '" << nick << "', " << rating << ")";
			Query((char *)sstr2.str().c_str(), NULL, NULL, NULL);
		}

		std::stringstream sstr3;
		sstr3 << "SELECT COUNT(*),SUM(Rating) FROM Votes WHERE SongID=" << song;
		sql_rows rows = GetTable((char *)sstr3.str().c_str(), NULL);
		uint32 num = 0;
		if (rows.size() > 0) {
			num = atoul(rows[0]["COUNT(*)"].c_str());
			rating = atoul(rows[0]["SUM(Rating)"].c_str());
			if (num) {
				rating /= num;
			}
		} else {
			rating = 0;
		}

		std::stringstream sstr4;
		sstr4 << "REPLACE INTO Ratings (`Song`,`SongID`,`Rating`,`Votes`) VALUES (\"" << msong << "\", " << song << ", " << rating << ", " << num << ")";
		Query((char *)sstr4.str().c_str(), NULL, NULL, NULL);

		IBM_RATING ir;
		ir.AutoDJ_ID = song;
		ir.song = ri.Song;
		ir.rating = rating;
		ir.votes = num;
		SendMessage(-1, IB_ON_SONG_RATING, (char *)&ir, sizeof(ir));
		Free(msong);
	}
}


int RatingCommands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
#if defined(DEBUG)
	if (!stricmp(command,"ratings-rebuild")) {
		ut->std_reply(ut, "Rebuilding...");
		RebuildRatings();
		return 1;
	}
#endif

	if (!stricmp(command,"rating")) {
		if (!config.stats.online || !config.stats.cursong[0]) {
			ut->std_reply(ut, _("Error: Nothing is playing right now..."));
		} else {
			char buf[1024], buf2[1024];
			SONG_RATING r;
			GetSongRating(config.stats.cursong, &r);
			if (r.Votes == 0) {
				uint32 id=0;
				if (SendMessage(-1, SOURCE_GET_SONGID, (char *)&id, sizeof(id)) == 1 && id != 0) {
					GetSongRating(id, &r);
				}
			}
			if (r.Votes > 0) {
				if (!LoadMessage("Rating", buf, sizeof(buf))) {
					sstrcpy(buf, "The current song has a score of %rating% with %votes% votes.");
				}
				sprintf(buf2, "%u", r.Rating);
				str_replaceA(buf, sizeof(buf), "%rating%", buf2);
				sprintf(buf2, "%u", r.Votes);
				str_replaceA(buf, sizeof(buf), "%votes%", buf2);
				ut->std_reply(ut, buf);
			} else {
				ut->std_reply(ut, _("This song has not been rated."));
			}
		}
		return 1;
	}

	if (!stricmp(command, "rate") && (!parms || !strlen(parms))) {
		char buf[512];
		snprintf(buf, sizeof(buf), _("Usage: rate 0-%d (0 to remove your rating, 1 for worst, %d for best)"), config.base.MaxRating, config.base.MaxRating);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"rate")) {
		if (!config.stats.online || !config.stats.cursong[0]) {
			ut->std_reply(ut, _("Error: Nothing is playing right now..."));
		} else {
			uint32 rating = atoul(parms);
			if (rating >= 0 && rating <= config.base.MaxRating) {
				const char * nick = ut->Nick;
				if (ut->User) {
					nick = ut->User->Nick;
				}
				uint32 id=0;
				if (SendMessage(-1, SOURCE_GET_SONGID, (char *)&id, sizeof(id)) != 1) {
					id = 0;
				}
				RateSong(config.stats.cursong, nick, rating, id);
				ut->std_reply(ut, _("Thank you for your song rating."));
			} else {
				char buf[512];
				snprintf(buf, sizeof(buf), _("Usage: rate 0-%d (0 to remove your rating, 1 for worst, %d for best)"), config.base.MaxRating, config.base.MaxRating);
				ut->std_reply(ut, buf);
			}
		}
		return 1;
	}
	return 0;
}

#endif // defined(IRCBOT_ENABLE_SS)
