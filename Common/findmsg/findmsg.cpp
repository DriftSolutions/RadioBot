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
								char * p = strstr(buf, "LoadMessage");
								while (p) {
									char * end = strchr(p, ')');
									if (end) {
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

										strncpy(buf2, p, end-p);
										fprintf(outFP, "%s\r\n", buf2);
									}
									p = strstr(end?end:p+1, "LoadMessage");
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
	strcpy(outFile, "loadmsg.txt");

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

	outFP = fopen(outFile, "wb");
	if (outFP == NULL) {
		printf("Error opening output file!\n");
		return 2;
	}

	scanDirs.push_back(sDir);
	ProcDir();

	return 0;
}
