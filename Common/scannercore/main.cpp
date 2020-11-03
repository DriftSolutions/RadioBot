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

#include "scanner.h"

#if defined(__WXMSW__) && !defined(__WXWINCE__)
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df'\"")
#endif

CONFIG config;
//bool shutdown_now = false;//, debug = false;
//Titus_Mutex hMutex;

PrintFunc debug_printf;
PrintFunc status_printf;

void LoadConfig();
bool SaveConfig();

void ASCII_Remap(unsigned char * str) {
	for (uint32 i=0; str[i] != 0; i++) {
		if (str[i] == 0x8B) {
			str[i] = '<';
		} else if (str[i] == 0x8E) {
			str[i] = 'Z';
		} else if (str[i] == 0x91 || str[i] == 0x92) {
			str[i] = '\'';
		} else if (str[i] == 0x93 || str[i] == 0x94) {
			str[i] = '\"';
		} else if (str[i] == 0x96 || str[i] == 0x97) {
			str[i] = '-';
		} else if (str[i] == 0x9A) {
			str[i] = 's';
		} else if (str[i] == 0x9B) {
			str[i] = '>';
		} else if (str[i] == 0x9E) {
			str[i] = 'z';
		} else if (str[i] == 0x9F) {
			str[i] = 'y';
		} else if (str[i] == 0xA6) {
			str[i] = '|';
		} else if (str[i] == 0xAA) {
			str[i] = 'a';
		} else if (str[i] == 0xB2) {
			str[i] = '2';
		} else if (str[i] == 0xB3) {
			str[i] = '3';
		} else if (str[i] == 0xB5) {
			str[i] = 'u';
		} else if (str[i] == 0xBF) {
			str[i] = '?';
		} else if (str[i] >= 0xC0 && str[i] <= 0xC6) {
			str[i] = 'A';
		} else if (str[i] == 0xC7) {
			str[i] = 'C';
		} else if (str[i] >= 0xC8 && str[i] <= 0xCB) {
			str[i] = 'E';
		} else if (str[i] >= 0xCC && str[i] <= 0xCF) {
			str[i] = 'I';
		} else if (str[i] == 0xD0) {
			str[i] = 'D';
		} else if (str[i] == 0xD1) {
			str[i] = 'N';
		} else if (str[i] >= 0xD2 && str[i] <= 0xD6) {
			str[i] = 'O';
		} else if (str[i] == 0xD7) {
			str[i] = 'X';
		} else if (str[i] == 0xD8) {
			str[i] = 'O';
		} else if (str[i] >= 0xD9 && str[i] <= 0xDC) {
			str[i] = 'U';
		} else if (str[i] == 0xDD) {
			str[i] = 'Y';
		} else if (str[i] == 0xDE) {
			str[i] = 'P';
		} else if (str[i] == 0xDF) {
			str[i] = 'B';
		} else if (str[i] >= 0xE0 && str[i] <= 0xE6) {
			str[i] = 'a';
		} else if (str[i] == 0xE7) {
			str[i] = 'c';
		} else if (str[i] >= 0xE8 && str[i] <= 0xEB) {
			str[i] = 'e';
		} else if (str[i] >= 0xEC && str[i] <= 0xEF) {
			str[i] = 'i';
		} else if (str[i] == 0xF0) {
			str[i] = 'o';
		} else if (str[i] == 0xF1) {
			str[i] = 'n';
		} else if (str[i] >= 0xF2 && str[i] <= 0xF6) {
			str[i] = 'o';
		} else if (str[i] == 0xF8) {
			str[i] = 'o';
		} else if (str[i] >= 0xF9 && str[i] <= 0xFC) {
			str[i] = 'u';
		} else if (str[i] == 0xFD) {
			str[i] = 'y';
		} else if (str[i] == 0xFE) {
			str[i] = 'p';
		} else if (str[i] > 0x7F || str[i] < 0x20) {
			memmove(&str[i], &str[i]+1, strlen((char *)&str[i]));
		}
	}
}

char * EncodeString(char * str) {
	ASCII_Remap((unsigned char *)str);

	int maxlen = (strlen(str)*7)+1;
	char * out = (char *)zmalloc(maxlen);
	memset(out, 0, maxlen);

	char * tmp2 = (char *)zmalloc(maxlen);
	strcpy(tmp2, str);
	str = tmp2;

	//str_replace(str, maxlen, "&",  "&amp;");
	//str_replace(str, maxlen, "\"", "&qout;");
	//str_replace(str, maxlen, "<",  "&lt;");
	//str_replace(str, maxlen, ">",  "&gt;");
	//str_replace(str, maxlen, "'",  "&apos;");

	int len = strlen(str);
//	unsigned char tmp[2];
	char rep[32]={0};
	for (int i=0; i < len; i++) {
		if ((unsigned char)str[i] >= 0x7F || (unsigned char)str[i] < 32) {
			sprintf(rep, "&#x%03u;", (unsigned) ( str[i] & 0xff ) );
			strcat(out, rep);
		} else {
			strncat(out, &str[i], 1);
		}
	}

	zfree(tmp2);
	return out;
}

int saveXMLRec(void * ptr, int nCols, char ** vals, char ** fields) {

	uint32 id = 0;
	FILEREC ret;
	memset(&ret, 0, sizeof(FILEREC));

	int num = 0;
	char ** f = fields;
	char ** v = vals;

	while (num < nCols) {
		if (!stricmp(*f, "mTime")) {
			ret.mTime = atoi64(*v);
		} else if (!stricmp(*f, "ID")) {
			id = atoul(*v);
		} else if (!stricmp(*f, "Path")) {
			strcpy(ret.path, *v);
		} else if (!stricmp(*f, "FN")) {
			strcpy(ret.fn, *v);
		} else if (!stricmp(*f, "Artist")) {
			strcpy(ret.artist, *v);
		} else if (!stricmp(*f, "Album")) {
			strcpy(ret.album, *v);
		} else if (!stricmp(*f, "Title")) {
			strcpy(ret.title, *v);
		} else if (!stricmp(*f, "Genre")) {
			strcpy(ret.genre, *v);
		} else if (!stricmp(*f, "SongLen")) {
			ret.songlen = atoi(*v);
		}
		f++;
		v++;
		num++;
	}

	if (id != 0) {
		TiXmlElement * songs = (TiXmlElement *)ptr;

		TiXmlElement * song = new TiXmlElement("Song");
		char buf[64];
		sprintf(buf, "%u", id);
		song->SetAttribute("ID", buf);
		song->SetAttribute("SongLen", ret.songlen);
		sprintf(buf, I64FMT, ret.mTime);
		song->SetAttribute("mTime", buf);
		char * p = EncodeString(ret.path);
		song->SetAttribute("Path", p);
		zfree(p);
		p = EncodeString(ret.fn);
		song->SetAttribute("FN", p);
		zfree(p);
		p = EncodeString(ret.artist);
		song->SetAttribute("Artist", p);
		zfree(p);
		p = EncodeString(ret.title);
		song->SetAttribute("Title", p);
		zfree(p);
		p = EncodeString(ret.album);
		song->SetAttribute("Album", p);
		zfree(p);
		p = EncodeString(ret.genre);
		song->SetAttribute("Genre", p);
		zfree(p);

		songs->LinkEndChild(song);
	}

	return 0;
}

THREADTYPE ScanThread(void * lpData) {
	TT_THREAD_START

	int32 num_files=0, totlen = 0;
	config.error[0] = 0;
	ScanFiles(&num_files, &totlen);
	//num_files = 4563;
	//totlen = 100;

	if (num_files > 0) {
		TiXmlDocument * doc = new TiXmlDocument();
		doc->LinkEndChild(new TiXmlDeclaration("1.0", "UTF-8", "yes"));

		TiXmlElement * root = new TiXmlElement("SMD");
		root->SetAttribute("Version", 1);
		root->SetAttribute("URL", "http://wiki.shoutirc.com/index.php/ShoutIRC_Music_Database");
		doc->LinkEndChild(root);

		TiXmlElement * info = new TiXmlElement("Info");
		info->SetAttribute("NumSongs", num_files);
		info->SetAttribute("TotalLength", totlen);
		root->LinkEndChild(info);

		TiXmlElement * songs = new TiXmlElement("Songs");

//		int n;
		Query("SELECT * FROM Songs WHERE Seen=1", saveXMLRec, songs, NULL);

		root->LinkEndChild(songs);
		if (config.compressOutput) {
			char * fn = GetUserConfigFile("ShoutIRC", "music.scanner.tmp");
			FILE * fp = fopen(fn, "w+b");
			if (fp) {
				doc->SaveFile(fp);
				long len = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				char * data = new char[len];
				if (fread(data, len, 1, fp) == 1) {
					uLongf len2 = compressBound(len);
					char * comp = new char[len2];
					if (compress2((Bytef *)comp, &len2, (const Bytef *)data, len, 9) == Z_OK) {
						FILE * fp2 = fopen(config.out_fn, "wb");
						if (fp2) {
							fwrite("SMDZ", 4, 1, fp2);
							fwrite(&len2, sizeof(len2), 1, fp2);
							fwrite(&len, sizeof(len), 1, fp2);
							len = 0;
							fwrite(&len, sizeof(len), 1, fp2);
							fwrite(comp, len2, 1, fp2);
							fclose(fp2);
						} else {
							strcpy(config.error, "Error opening output file!");
						}
					} else {
						strcpy(config.error, "Error compressing data!");
					}
					delete comp;
				} else {
					strcpy(config.error, "Error reading temp file data!");
				}
				delete data;
				fclose(fp);
			} else {
				strcpy(config.error, "Error opening temp file!");
			}
			remove(fn);
			zfree(fn);
		} else {
			if (!doc->SaveFile(config.out_fn)) {
				strcpy(config.error, "Error saving file! Make sure it is not in use...");
			}
		}
		delete doc;
	} else {
		strcpy(config.error, "No files found! Double-check your Music folders!");
	}
	TT_THREAD_END
}

void DoScan() {
	TT_StartThread(ScanThread, NULL);
}

void WipeDB() {
	status_printf("Wiping data cache...");
	Query("DROP TABLE Songs");
	status_printf("...done.");
}

bool InitCore(PrintFunc dbg, PrintFunc stat, char * error) {
	status_printf = stat;
	debug_printf = dbg;

	char * tmp = GetUserConfigFile("ShoutIRC", "scanner.conf");
	strcpy(config.config_fn, tmp);
	zfree(tmp);
	tmp = GetUserConfigFile("ShoutIRC", "music.db");
	strcpy(config.db_fn, tmp);
	zfree(tmp);
	tmp = GetUserConfigFile("ShoutIRC", "music.smd");
	strcpy(config.out_fn, tmp);
	zfree(tmp);

	LoadConfig();
	if (sqlite3_open(config.db_fn, &config.dbHandle) != SQLITE_OK) {
		if (error) { strcpy(error, sqlite3_errmsg(config.dbHandle)); }
		if (config.dbHandle) { sqlite3_close(config.dbHandle); }
		return false;
	}
	return true;
}

void QuitCore() {
	while (TT_NumThreads()) { safe_sleep(100, true); }
	SaveConfig();
	if (config.dbHandle) { sqlite3_close(config.dbHandle); }
}
