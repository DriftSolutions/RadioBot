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

#if defined(FREEBSD)
#define stat64 stat
#endif

char VERSION[] = "Drift Solutions' MP3 Sync 1.0";

class FileInfo {
public:
	FileInfo() { mtime = size = mode = 0; }
	string src_fn;
	string dest_fn;
	time_t mtime;
	int64 size;
	int mode;
};

CONFIG config;

struct STATS {
	int tot_files;
	int num_same;
	int num_error;
	int encoded;
	int deleted;
};
STATS stats;

typedef std::list<FileInfo> fileList;
typedef std::list<string> strList;
fileList copyList;
strList delList;
strList dirList;
//Titus_StringList tmpList;

Titus_Mutex dsMutex;
int ds_printf(int level, char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char * tmp = zvmprintf(fmt, va);
	if (tmp) {
		dsMutex.Lock();
		if (config.verbosity >= level) {
			fprintf(stderr, "%s", tmp);
		}
		if (config.log_fp) {
			fprintf(config.log_fp, "%s", tmp);
			fflush(config.log_fp);
		}
		dsMutex.Release();
		zfree(tmp);
	}
	va_end(va);
	return 1;
}

void CreateDirectory(char * dir, int mode=0755) {
	struct stat st;
	if (stat(dir, &st) == 0) {
		if (st.st_mode & S_IFDIR) { return; }
		//directory is a file!
		ds_printf(0, "WARNING: %s should be a directory, but is a file!\n");
		return;
	}
	//if (access(dir, 0) == 0) { return; }
	if (dsl_mkdir(dir, mode) == -1) {
		StrTokenizer st(dir, PATH_SEP);
		for (unsigned long i=1; i <= st.NumTok(); i++) {
			char * p = st.GetTok(1,i);
			if (p) {
				if (access(p, 0) == -1 && dsl_mkdir(p, mode) == -1) {
					ds_printf(0, "Error creating %s\n", p);
				}
			}
			st.FreeString(p);
		}
	}
}

void BuildFileList();

char * bytes(uint64 num, char * buf) {
	#define KB 1024
	#define MB 1048576
	#define GB 1073741824LL
	#define TB 1099511627776LL
	if (num >= TB) {
		uint64 w = num / TB;
		num -= (w * TB);
		double x = num;
		x /= TB;
		x += w;
		sprintf(buf, "%.2fTB", x);
		return buf;
	}
	if (num >= GB) {
		uint64 w = num / GB;
		num -= (w * GB);
		double x = num;
		x /= GB;
		x += w;
		sprintf(buf, "%.2fGB", x);
		return buf;
	}
	if (num >= MB) {
		double x = num;
		x /= MB;
		sprintf(buf, "%.2fMB", x);
		return buf;
	}
	if (num >= KB) {
		double x = num;
		x /= KB;
		sprintf(buf, "%.2fKB", x);
		return buf;
	}

	sprintf(buf, "" U64FMT "B", num);
	return buf;
}

bool EncodeFile(char * src, char * dest, FileInfo * fi) {
	char * dir = zstrdup(dest);
	char * p = strrchr(dir, PATH_SEP);
	if (p) {
		p[0]=0;
		CreateDirectory(dir);
	}
	zfree(dir);

#if defined(WIN32)
	DWORD attr = GetFileAttributes(dest);
	if (attr != INVALID_FILE_ATTRIBUTES) {
		if (attr & FILE_ATTRIBUTE_READONLY) {
			ds_printf(1, "Unsetting read-only attr on file: %s\n", dest);
			SetFileAttributes(dest, attr & ~FILE_ATTRIBUTE_READONLY);
		}
	}
#endif

	/*
	stringstream str;
	str << "lame";
	if (fi->size < (15*1024*1024)) {
		str << " -S";
	}
	str << " -b " << config.bitrate << " --resample " << config.samplerate << " -m " << config.mode << " \"" << src << "\" \"" << dest << "\"";
	ds_printf(2, "Exec: %s\n", str.str().c_str());
	int ret = system(str.str().c_str());
	if (ret != 0) {
		return false;
	}
	*/

#if defined(WIN32) && !defined(DEBUG)
	__try {
#endif
	if (!copy_mp3(src, dest, fi->size)) {
		return false;
	}
#if defined(WIN32) && !defined(DEBUG)
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		ds_printf(1, "\n");
		ds_printf(0, "ERROR: Exception occured while encoding MP3 Data!\n");
		return false;
	}
#endif
	ds_printf(1, "\n");

#if defined(WIN32) && !defined(DEBUG)
	__try {
#endif
	if (!copy_id3(src, dest)) {
		return false;
	}
#if defined(WIN32) && !defined(DEBUG)
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		ds_printf(0, "ERROR: Exception occured while copying ID3 tag(s)!\n");
		return false;
	}
#endif

	utimbuf tme;
	memset(&tme, 0, sizeof(tme));
	tme.actime = time(NULL);
	tme.modtime = fi->mtime;
	utime(dest, &tme);
	chmod(dest, fi->mode);
	utime(dest, &tme);
	return true;
}

void usage_and_exit(int code) {
	printf("Options:\n");
	printf("\t-i dir - Set source directory\n");
	printf("\t-o dir - Set dest directory\n");
	printf("\t-b bitrate - Encode files with bitrate (default: 64)\n");
	printf("\t-s samplerate - Encode files with samplerate (default: 44100)\n");
	printf("\t-c channels - 1 = mono, 2 = joint stereo, 3 = stereo (default: 1)\n");
	printf("\t-l file - Log to file\n");
	printf("\t-d - Delete files in dest that don't exist in src\n");
	printf("\t-v - Increase verbosity\n");
	printf("\t-h - This help text\n");
	//printf("\t* Note: due to a limitation in lame, if you try to encode a mono source file to stereo output the resulting file will be mono!\n");
	exit(code);
}

int main(int argc, char * argv[]) {
	printf("%s\n",VERSION);
	printf("Check out www.titus-dev.com for more FREE tools and source codes!\n");

	memset(&config, 0, sizeof(config));
	config.bitrate = 64;
	config.samplerate = 44100;
	config.channels = 1;

	static option opts[] = {
		{ "in", required_argument, NULL, 'i' },
		{ "log", required_argument, NULL, 'l' },
		{ "out", required_argument, NULL, 'o' },
		{ "delete", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ 0, 0, 0, 0 }
	};

	bool do_delete = false;
	memset(&stats, 0, sizeof(stats));

	int c=0;
	while ((c = getopt_long(argc, argv, "i:l:o:dvb:s:c:", opts, NULL)) != -1) {
		switch (c) {
			case 'i':
				strcpy(config.src_dir, optarg);
				//cvalue = optarg;
				break;
			case 'l':
				config.log_fp = fopen(optarg, "wb");
				if (config.log_fp == NULL) {
					ds_printf(0, "Error opening %s for log output!\n", optarg);
				}
				break;
			case 'o':
				strcpy(config.dest_dir, optarg);
				//cvalue = optarg;
				break;
			case 'd':
				do_delete = true;
				break;
			case 'c':
				config.channels = atoi(optarg);
				break;
			case 'b':
				config.bitrate = atoi(optarg);
				break;
			case 's':
				config.samplerate = atoi(optarg);
				break;
			case 'v':
				config.verbosity++;
				break;
			case '?':
				if (isprint(optopt)) {
					ds_printf(0, "Unknown option `-%c'.\n\n", optopt);
				} else {
					ds_printf(0, "Unknown option character `\\x%x'.\n\n", optopt);
				}
			case 'h':
			default:
			    usage_and_exit(1);
				return 1;
				break;
		}
	}

	if (!strlen(config.src_dir)) {
		ds_printf(0, "Error: No input directory specified! (-i)\n");
		return 1;
	}

	if (!strlen(config.dest_dir)) {
		ds_printf(0, "Error: No output directory specified! (-o)\n");
		return 1;
	}

	if (config.channels < 1 || config.channels > 3) { config.channels = 1; }
	if (config.samplerate < 16000) {
		config.samplerate = 16000;
		ds_printf(0, "MP3 versions 1 and 2 only support a minimum sample rate of 16000, I have set your sample rate to that.\n");
	} else if (config.samplerate > 48000) {
		config.samplerate = 48000;
		ds_printf(0, "MP3 versions 1 and 2 only support a maximum sample rate of 48000, I have set your sample rate to that.\n");
	}
	if (config.src_dir[strlen(config.src_dir) - 1] != PATH_SEP) {
		strcat(config.src_dir,PATH_SEPS);
	}
	if (config.dest_dir[strlen(config.dest_dir) - 1] != PATH_SEP) {
		strcat(config.dest_dir,PATH_SEPS);
	}

	int err = mpg123_init();
	if ( err != MPG123_OK) {
		ds_printf(0, "Error initializing mp3 decoder: %s\n", mpg123_plain_strerror(err));
		mpg123_exit();
		return 1;
	}

	time_t t = time(NULL);
	ds_printf(0, "Source Path Root: %s\n",config.src_dir);
	ds_printf(0, "Destination Path Root: %s\n",config.dest_dir);
	CreateDirectory(config.dest_dir);

	ds_printf(0, "Building file list...\n");
	BuildFileList();

	if (copyList.size()) {
		int num_encode = copyList.size();
		int ind = 1;
		char bytebuf[32];
		//ds_printf(0, "Number of files to encode: %d out of %d total files (%d unchanged)\n", num_encode, stats.tot_files, stats.num_same);
		ds_printf(0, "Encoding files...\n");
		FileInfo * fi = NULL;
		for (fileList::iterator x = copyList.begin(); x != copyList.end(); x++) {
			fi = &*x;
			char * src = zstrdup(fi->src_fn.c_str());
			char * dest = zstrdup(fi->dest_fn.c_str());

			ds_printf(0, "Encoding file %d/%d: %s (%s)\n", ind++, num_encode, nopath(fi->src_fn.c_str()), bytes(fi->size, bytebuf));
			if (EncodeFile(src, dest, fi)) {
				stats.encoded++;
			} else {
				ds_printf(0, "Error encoding file!\n");
				stats.num_error++;
			}

			zfree(src);
			zfree(dest);
		}
		ds_printf(0, "Encoding complete!\n\n");
		copyList.clear();
	}

	if (delList.size() && do_delete) {
		if (stats.num_error == 0) {
			ds_printf(0, "Deleting files...\n");
			for (strList::iterator x = delList.begin(); x != delList.end(); x++) {
				ds_printf(0, "Deleting: %s\n", (*x).c_str());
				if (remove((*x).c_str()) == 0) {
					stats.deleted++;
				} else {
					ds_printf(0, "Error deleting file: %s\n", strerror(errno));
				}
			}
		} else {
			ds_printf(0, "Skipping file deletion, errors occured...\n");
		}
	}
	delList.clear();

	ds_printf(0, "Files Encoded: %d\nFiles Not Encoded (unchanged): %d\nFiles Not Encoded (due to errors): %d\nOld Files Deleted: %d\n",stats.encoded,stats.num_same,stats.num_error,stats.deleted);
	ds_printf(0, "Took: %d seconds...\n",time(NULL) - t);

	mpg123_exit();
	return 0;
}

inline void AddFile(char * sfn, char * dfn,time_t mtime, time_t atime, int mode, int64 size) {
	FileInfo fi;// = (FILEINFO *)zmalloc(sizeof(FILEINFO));

	fi.src_fn = sfn;
	fi.dest_fn = dfn;
	fi.mode = mode;
	fi.mtime = mtime;
	fi.size = size;
	copyList.push_back(fi);
}

inline bool want_ext(const char * ext) {
	if (!stricmp(ext, "mp3") || !stricmp(ext, "sm3")) {
		return true;
	}
	return false;
}

void ProcDir(const char * sub) {
	static char buf[4096];
	//bool is_dir;
	//int64 size;

	char *src = NULL, *dest=NULL;
	if (sub == NULL) {
		src = zmprintf("%s", config.src_dir);
		dest = zmprintf("%s", config.dest_dir);
	} else {
		src = zmprintf("%s%s", config.src_dir, sub, PATH_SEP);
		dest = zmprintf("%s%s", config.dest_dir, sub, PATH_SEP);
	}

	Directory dir(src);
	//ds_printf(3, "Scanning: %s\n", src);
	while (dir.Read(buf, sizeof(buf))) {
		if (!strcmp(buf, ".") || !strcmp(buf, "..")) { continue; }
		char * sFile = zmprintf("%s%s", src, buf);
		struct stat64 st, st2;
		if (stat64(sFile, &st) == 0) {
			if (S_ISDIR(st.st_mode)) {
				char * tmp = zmprintf("%s%s%c", sub ? sub:"", buf, PATH_SEP);
				dirList.push_back(tmp);
				zfree(tmp);
			} else {
				char * ext = strrchr(buf, '.');
				if (ext && want_ext(ext+1)) {
					stats.tot_files++;
					char * dFile = zmprintf("%s%s", dest, buf);
					char * sFile = zmprintf("%s%s", src, buf);

					if (stat64(dFile, &st2) != 0) {
						ds_printf(1, "Adding %s... (new)\n", sFile);
						AddFile(sFile, dFile, st.st_mtime, st.st_atime, st.st_mode, st.st_size);
					} else if (st.st_mtime != st2.st_mtime) {
						ds_printf(1, "Adding %s... (mtime)\n", sFile);
						AddFile(sFile, dFile, st.st_mtime, st.st_atime, st.st_mode, st.st_size);
					} else {
						stats.num_same++;
					}

					zfree(dFile);
					zfree(sFile);
				}
			}
		} else {
			ds_printf(0, "Error stat()ing source file %s: %s\n", sFile, strerror(errno));
		}
		zfree(sFile);
	}

	dir.Close();
	dir.Open(dest);
	while (dir.Read(buf, sizeof(buf))) {
		if (!strcmp(buf, ".") || !strcmp(buf, "..")) { continue; }
		char * sFile = zmprintf("%s%s", src, buf);

		char * ext = strrchr(buf, '.');
		if (ext && want_ext(ext+1)) {
			if (access(sFile, 0) != 0) {//if file does not exist in src dir, delete from dest
				char * dFile = zmprintf("%s%s", dest, buf);
				delList.push_back(dFile);
				zfree(dFile);
			}
		}

		zfree(sFile);
	}

	zfree(src); zfree(dest);
}

void BuildFileList() {
	ProcDir(NULL);
	strList::iterator x = dirList.begin();
	while (x != dirList.end()) {
		ProcDir((*x).c_str());
		dirList.erase(x);
		x = dirList.begin();
	}
}
