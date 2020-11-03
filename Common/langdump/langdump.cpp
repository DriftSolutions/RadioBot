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

#define DSL_STATIC
#include <drift/dsl.h>

//#define RANDOM_OUTPUT 1
//#define INCLUDE_PATHS 1

char sDir[MAX_PATH];
FILE * outFP = NULL;

char * excluded[] = {
	"physfs",
	"DemoMode",
	"ui_wx.cpp",
	"unnamed_wdr.cpp",
	"monitor.c",
	NULL
};
bool IsExcluded(const char * fn) {
	for (int i=0; excluded[i] != NULL; i++) {
		if (!stricmp(excluded[i], fn)) {
			return true;
		}
	}
	return false;
}
std::vector<std::string> scanDirs;

class LangItem {
public:
	std::string string, value;
	bool seen;

	LangItem() { string = value = ""; seen = false; }
};

typedef std::map<string, LangItem> langList;
langList lang;

void ResetLang() {
	lang.clear();
}

bool IsStringWritten(string str) {
	langList::iterator x = lang.find(str);
	if (x != lang.end()) {
		if (x->second.seen) { return true; }
	}
	return false;
}

string GetLangStringValue(string str) {
	langList::iterator x = lang.find(str);
	if (x != lang.end()) {
		x->second.seen = true;
		return x->second.value;
	}

	LangItem itm;
	itm.string = str;
#ifdef RANDOM_OUTPUT
	char buf[10];
	for (int i=0; i < 9; i++) {
		buf[i] = (rand()%26) + 'a';
	}
	buf[9] = 0;
	itm.value = buf;
#else
	itm.value = "";
#endif
	itm.seen = true;
	lang[str] = itm;
	return itm.value.c_str();
}

bool GetValue(const char * start, char * buf, size_t bufSize) {
	memset(buf, 0, bufSize);

	const char * p = start;
	const char * end = NULL;
	while (*p) {
		if (*p == '\\') {
			p++;
		} else if (*p == '\"') {
			end = p;
			break;
		}
		p++;
	}

	if (end) {
		strncpy(buf, start, end-start);
		return true;
	}

	return false;
}

bool LoadLangText(const char * fn) {
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		std::string str;
		char buf[1024], buf2[1024];
		while (fgets(buf, sizeof(buf), fp)) {
			if (!strnicmp(buf, "string \"", 8)) {
				if (GetValue(buf+8, buf2, sizeof(buf2))) {
					str = buf2;
				}
			}
			if (!strnicmp(buf, "value \"", 7)) {
				if (GetValue(buf+7, buf2, sizeof(buf2))) {
					if (str.length() && buf2[0]) {
						LangItem itm;
						itm.string = str;
						itm.value = buf2;
						lang[str] = itm;
					} else if (buf2[0]) {
						printf("ERROR: value without string first!\n");
					}
					str = "";
				}
			}
		}
		fclose(fp);
		return true;
	}
	return false;
}

void ProcDir() {
	char buf[1024],buf2[1024];
	while (scanDirs.size()) {
		std::vector<std::string>::iterator x = scanDirs.begin();
		std::string ssDir = *x;
		scanDirs.erase(x);
		Directory dir(ssDir.c_str());
		bool is_dir = false;
		bool first_d = true;
		while (dir.Read(buf, sizeof(buf), &is_dir)) {
			if (buf[0] != '.' && !IsExcluded(buf)) {
				if (is_dir) {
					std::string ffn = ssDir;
					ffn += buf;
					ffn += PATH_SEPS;
					scanDirs.push_back(ffn);
				} else {
					char * ext = strrchr(buf, '.');
					if (ext && (!stricmp(ext, ".cpp") || !stricmp(ext, ".c") || !stricmp(ext, ".cxx"))) {
						std::string ffn = ssDir;
						ffn += buf;
						FILE * fp = fopen(ffn.c_str(), "r");
						if (fp) {
							printf("Processing %s ...\n", ffn.c_str());
							memset(buf, 0, sizeof(buf));
							bool first = true;
							while (fgets(buf, sizeof(buf), fp)) {
								char * p = strstr(buf, "_(\"");
								while (p) {
									p += 3;
									char * start = p;
									char * end = NULL;
									while (*p) {
										if (*p == '\\') {
											p++;
										} else if (*p == '\"' && p[1] == ')') {
											end = p;
											break;
										}
										p++;
									}

									if (end) {
#ifdef INCLUDE_PATHS
										if (first) {
											if (first_d) {
												if (!stricmp(sDir, ssDir.c_str())) {
													fprintf(outFP, "# Entering root directory ...\r\n");
												} else {
													strcpy(buf2, ssDir.c_str());
													str_replace(buf2, sizeof(buf2), sDir, "");
													str_replace(buf2, sizeof(buf2), "\\", "/");
													fprintf(outFP, "# Entering directory: %s ...\r\n", buf2);
												}
												first_d = false;
											}
											strcpy(buf2, ffn.c_str());
											str_replace(buf2, sizeof(buf2), sDir, "");
											str_replace(buf2, sizeof(buf2), "\\", "/");
											fprintf(outFP, "# Begin %s\r\n\r\n", buf2);
											first = false;
										}
#endif
										memset(buf2, 0, sizeof(buf2));
										strncpy(buf2, start, end-start);
										if (!IsStringWritten(buf2)) {
											fprintf(outFP, "string \"%s\"\r\n", buf2);
											fprintf(outFP, "value \"%s\"\r\n\r\n", GetLangStringValue(buf2).c_str());
										}
									}

									p = strstr(end ? end:p, "_(\"");
								}
							}
							fclose(fp);
						} else {
							printf("Error opening %s!\n", ffn.c_str());
						}
					}
				}
			}
		}
	}
}

int main(int argc, char * argv[]) {
	char outFile[MAX_PATH];//,buf[1024];
	getcwd(sDir, sizeof(sDir));
	strcpy(outFile, "lang.ldb");

	int c = 0;
	while ((c = getopt (argc, argv, "d:o:")) != -1) {
		switch (c) {
			case 'd':
				strcpy(sDir, optarg);
				break;
			case 'o':
				strcpy(outFile, optarg);
				break;
			case '?':
				if (isprint(optopt)) {
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				} else {
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
			default:
				printf("Command line options:\n");
				printf("-d directory - Set source directory\n");
				printf("-o filename - Set output filename\n");
				return 1;
				break;
		}
	}

	if (sDir[strlen(sDir)-1] != PATH_SEP) { strcat(sDir, PATH_SEPS); }

	printf("Drift Solutions' Language File Maker\n* INTERNAL USE ONLY *\n\n");
	printf("Source Dir: %s\n", sDir);
	printf("Output File: %s\n", outFile);

	if (access(outFile, 0) == 0) {
		printf("Loading existing strings...\n");
		if (!LoadLangText(outFile)) {
			printf("Error loading existing strings, aborting...\n");
			return 3;
		}
	}

	outFP = fopen(outFile, "wb");
	if (outFP == NULL) {
		printf("Error opening output file!\n");
		return 2;
	}

	time_t tme = time(NULL);
	tm t;
	localtime_r(&tme, &t);
	fprintf(outFP, "# RadioBot Language File\r\n");
	fprintf(outFP, "# Copyright 2003-%d ShoutIRC.com. All rights reserved.\r\n", t.tm_year + 1900);
	fprintf(outFP, "# Compiled on " __DATE__ " at " __TIME__ "\r\n\r\n");

#ifdef RANDOM_OUTPUT
	srand(tme ^ 0xDEADBEEB);
#endif

	scanDirs.push_back(sDir);
	ProcDir();

	fprintf(outFP, "# Number of Entries: %u\r\n", lang.size());

	ResetLang();

	return 0;
}
