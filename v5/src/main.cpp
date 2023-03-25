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

#define __I_AM_MAIN_CPP__
#include "ircbot.h"
#if defined(WIN32)
#include <mysql/mysql.h>
#endif

#if defined(WIN32)
#define DISTRO "win32"
#else
#include <getopt.h>
#include "../../distro.h"
#endif

#if defined(WIN32)
#pragma data_seg(push, "Hello")
static char mr_burns[] = "Every bone shattered. Organs leaking vital fluids. Slight headache, loss of appetite. Smithers, I'm going to die.";
#pragma data_seg(pop)
#endif

#define VERSION_MAJOR 5
#define VERSION_MINOR 13

#if defined(DEBUG)
#define VERSION_DEBUG IRCBOT_VERSION_FLAG_DEBUG
#else
#define VERSION_DEBUG 0
#endif
#if defined(IRCBOT_LITE)
#define VERSION_LITE IRCBOT_VERSION_FLAG_LITE
#else
#define VERSION_LITE 0
#endif

uint32 VERSION = MAKE_VERSION(VERSION_MAJOR,VERSION_MINOR,VERSION_LITE|VERSION_DEBUG);

GLOBAL_CONFIG config;

void handle_timers();

#ifdef WIN32
#include <direct.h>
#endif
#include <stdarg.h>

#if defined(WIN32)
/**
 * Handles Ctrl+C, hitting the Close button, etc., on Windows
 */
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) {
	//dwCtrlType == CTRL_CLOSE_EVENT ||
	if (dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
		if (!config.shutdown_now) {
			if (dwCtrlType != CTRL_CLOSE_EVENT) {
				ib_printf(_("Caught Ctrl+C (or similar), setting shutdown_now=true;\n"));
			}
			config.shutdown_now=true;
		}
		return TRUE;
	}
	return TRUE;
}
#else

static void on_error_backtrace(int signo) {
	printf("Caught SIGSEGV\n");
}

/**
 * Catches Ctrl+C, etc. on Linux/Unix
 */
static void catch_sigint(int signo) {
	if (!config.shutdown_now) {
		ib_printf(_("Caught SIGINT, shutting down bot...\n"));
		config.shutdown_reboot=false;
		config.shutdown_now=true;
	}
}

/**
 * Catches SIGUSR1 on Linux/Unix
 */
static void catch_sigusr1(int signo) {
	if (!config.shutdown_now) {
		ib_printf(_("Caught SIGUSR1, restarting bot...\n"));
		config.shutdown_reboot=true;
		config.shutdown_now=true;
	}
}

#endif

void wipe_folder(const char * sDir, bool deldir) {
	char buf[MAX_PATH];
	Directory dir(sDir);
	bool is_dir;
	while (dir.Read(buf, sizeof(buf), &is_dir)) {
		std::string ffn = sDir;
		ffn += "/";
		ffn += buf;
//		struct stat st;
		if (buf[0] != '.') {
			if (is_dir) {
				wipe_folder(ffn.c_str(), true);
			} else {
				//ib_printf("IRCBot: Deleting temp file: %s\n", ffn.c_str());
				if (remove(ffn.c_str()) != 0) {
					ib_printf(_("%s: Error removing temp file: %s (%s)\n"), IRCBOT_NAME, ffn.c_str(), strerror(errno));
				}
			}
		}
	}
	if (deldir) {
		if (rmdir(sDir) != 0) {
			ib_printf(_("%s: Error removing temp folder: %s (%s)\n"), IRCBOT_NAME, sDir, strerror(errno));
		}
	}
}

void wipe_tmp_dir() {
	if (access("." PATH_SEPS "tmp", 0) != 0) {
		dsl_mkdir("." PATH_SEPS "tmp", 0775);
	}
	if (access("." PATH_SEPS "backup",0) != 0) {
		dsl_mkdir("." PATH_SEPS "backup", 0700);
	}
	if (access("." PATH_SEPS "logs", 0) != 0) {
		dsl_mkdir("." PATH_SEPS "logs", 0700);
	}
	if (access("." PATH_SEPS "logs" PATH_SEPS "memchk", 0) != 0) {
		dsl_mkdir("." PATH_SEPS "logs" PATH_SEPS "memchk", 0700);
	}
	wipe_folder("./tmp", false);
}

/**
 * The main() stub for startup
 * @param argc the number of parameters in argv[]
 * @param argv ... :-)
 */

#ifndef UTF7_CODEPAGE
#define UTF7_CODEPAGE 65000
#endif
#ifndef UTF8_CODEPAGE
#define UTF8_CODEPAGE 65001
#endif

int main(int argc, char *argv[]) {
#if defined(WIN32)
	//ret = AllocConsole() ? true:false;
	char buf[256];
	sprintf(buf, "%s %s/%s", IRCBOT_NAME, GetBotVersionString(), PLATFORM);
	SetConsoleTitle(buf);
	UINT OldCP = GetConsoleOutputCP();
	if (IsValidCodePage(UTF8_CODEPAGE)) {
		SetConsoleOutputCP(UTF8_CODEPAGE);
		SetConsoleCP(UTF8_CODEPAGE);
	} else if (IsValidCodePage(UTF7_CODEPAGE)) {
		SetConsoleOutputCP(UTF7_CODEPAGE);
		SetConsoleCP(UTF7_CODEPAGE);
	}
	SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
#else
	struct sigaction sa_old;
	struct sigaction sa_new;

	// set up signal handling
	sa_new.sa_handler = catch_sigint;
	sigemptyset(&sa_new.sa_mask);
	sa_new.sa_flags = 0;
	sigaction(SIGINT, &sa_new, &sa_old );

	sa_new.sa_handler = catch_sigusr1;
	sigemptyset(&sa_new.sa_mask);
	sa_new.sa_flags = 0;
	sigaction(SIGUSR1, &sa_new, &sa_old );

	sa_new.sa_handler = on_error_backtrace;
	sigemptyset(&sa_new.sa_mask);
	sa_new.sa_flags = 0;
	sigaction(SIGSEGV, &sa_new, &sa_old );
#endif
	int ret = BotMain(argc, argv);
#if defined(WIN32)
	SetConsoleOutputCP(OldCP);
#endif
#if defined(WIN32) && !defined(DEBUG)
	if (ret != 0) {
		system("pause");
	}
#endif
	return ret;

}

char * align_text_center(char * out, const char * text, int w) {
	char * tmp = zstrdup(text);
	int num = (w - strlen(tmp))/2;
	memset(out, ' ', num);
	strcpy(out+num, tmp);
	zfree(tmp);
	return out;
}
char * align_text_right(char * out, const char * text, int w) {
	char * tmp = zstrdup(text);
	int num = w - strlen(tmp);
	memset(out, ' ', num);
	strcpy(out+num, tmp);
	zfree(tmp);
	return out;
}

TT_DEFINE_THREAD(SendStats) {
	TT_THREAD_START

	CURL * ch = curl_easy_init();
	char errbuf[CURL_ERROR_SIZE];
	char curline[1024];
	memset(&curline, 0, sizeof(curline));
	memset(&errbuf, 0, sizeof(errbuf));
	MEMWRITE stream;
	memset(&stream, 0, sizeof(stream));
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, memWrite);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, &stream);
	curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(ch, CURLOPT_TIMEOUT, 120);
	curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, 30);
	curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 0);

	struct curl_httppost *post=NULL, *last=NULL;
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "name", CURLFORM_COPYCONTENTS, config.base.reg_name.c_str(), CURLFORM_END);
#if defined(IRCBOT_ENABLE_IRC)
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "nick", CURLFORM_COPYCONTENTS, config.base.irc_nick.c_str(), CURLFORM_END);
#endif
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "ver", CURLFORM_COPYCONTENTS, GetBotVersionString(), CURLFORM_END);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "distro", CURLFORM_COPYCONTENTS, DISTRO, CURLFORM_END);
	for (int i=0; i < MAX_PLUGINS; i++) {
		if (config.plugins[i].fn.length() > 0) {
			sprintf(curline, "plugins[%d]", i);
			curl_formadd(&post, &last, CURLFORM_COPYNAME, curline, CURLFORM_COPYCONTENTS, nopath(config.plugins[i].fn.c_str()), CURLFORM_END);
		}
	}
#if defined(WIN32)
	OSVERSIONINFOEX vi;
	memset(&vi, 0, sizeof(vi));
	vi.dwOSVersionInfoSize = sizeof(vi);
	GetVersionEx((OSVERSIONINFO *)&vi);
	snprintf(curline, sizeof(curline), "%d.%d.%d", vi.dwMajorVersion, vi.dwMinorVersion, (vi.wProductType & VER_NT_WORKSTATION) ? 0:1);
	curl_formadd(&post, &last, CURLFORM_COPYNAME, "winver", CURLFORM_COPYCONTENTS, curline, CURLFORM_END);
#endif
	curl_easy_setopt(ch, CURLOPT_HTTPPOST, post);

	curl_easy_setopt(ch, CURLOPT_URL, "https://www.shoutirc.com/getip.php");

	if (curl_easy_perform(ch) == CURLE_OK) {
		if (config.base.myip.length() == 0) {
			char ip[64];
			if (Get_INI_String_Memory(stream.data, "info", "ip", ip, sizeof(ip), NULL) != NULL) {
				strtrim(ip);
				if (ip[0]) {
					ib_printf(_("%s: Found local IP: %s\n"), IRCBOT_NAME, ip);
					config.base.myip = ip;
				}
			}
		}
	}

	curl_easy_cleanup(ch);
	curl_formfree(post);
	zfreenn(stream.data);

	TT_THREAD_END
}

int BotMain(int argc, char *argv[]) {
#if !defined(WIN32)
	signal(SIGSEGV, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
#else
	SetThreadName(GetCurrentThreadId(),"Main Thread");
#endif

	char curline[8192];
	memset(curline, 0, sizeof(curline));
#if defined(WIN32)
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	GetAPI()->hWnd = GetConsoleWindow();
	const HANDLE hbicon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
	if (hbicon) {
		SendMessageA(GetAPI()->hWnd, WM_SETICON, ICON_BIG, (LPARAM)hbicon);//(LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON2)));
	}
	const HANDLE hsicon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
	if (hsicon) {
		SendMessageA(GetAPI()->hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hsicon);//(LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON2)));
	}
	GetModuleFileNameA(NULL, curline, sizeof(curline));
#endif
	if (!strlen(curline)) {
		sstrcpy(curline, argv[0]);
	}
	if (!VerifyChecksum(curline)) {
		printf("%s: Error verifying checksum!\nThis could mean a virus has infected your computer or someone has tampered with your copy of RadioBot!\n", IRCBOT_NAME);
		return 1;
	}

	config.shutdown_now=false;
	while (!config.shutdown_now) { // this is the cheapest hack of them all

#if defined(WIN32)
		int mysql_err = mysql_library_init(0, NULL, NULL);
		if (mysql_err) {
			//printf("%s: Could not initialize libmariadb! (Error: %d)\n", IRCBOT_NAME, mysql_err);
			//return 1;
		}
#endif

		if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
			printf("%s: Error initializing libcurl!\n", IRCBOT_NAME);
			return 1;
		}

		if (!dsl_init()) {
			printf("%s: Error initializing DSL!\n", IRCBOT_NAME);
			curl_global_cleanup();
			return 1;
		}
		//printf("%s\n", dsl_get_version_string());

		ircbot_init_cycles();

		memset(curline, 0, sizeof(curline));
#ifdef WIN32
		config.hInst = GetModuleHandle(NULL);
		GetModuleFileNameA(NULL, curline, sizeof(curline));
		char *p2 = strrchr(curline, '\\');
		if (p2) { p2[1]=0; }
		chdir(curline);
#endif
		config.Reset();


		static struct option ircbot_long_options[] = {
		   {"test",	no_argument,       0, 't'},
		   {"cwd",	required_argument, 0, 'd'},
		   {"conf",	required_argument, 0, 'c'},
		   {"text",	required_argument, 0, 'e'},
		   {0, 0, 0, 0}
		};
		int option_index = 0;
		while (1) {
			int c = getopt_long(argc, argv, "d:c:e:tp:", ircbot_long_options, &option_index);

           /* Detect the end of the options. */
			if (c == -1) {
				break;
			}

			switch (c) {
				case 'd':
					if (chdir(optarg) != 0) {
						printf("Error switching current directory to %s: %s\n", optarg, strerror(errno));
					}
					break;
				case 'c':
					config.base.fnConf = optarg;
					break;
				case 'e':
					config.base.fnText = optarg;
					break;
#if defined(WIN32)
				case 'p':
					config.cPort = clamp<int>(atoi(optarg), 0, 65535);
					break;
#endif
				case 't':
					config.config = new Universal_Config;
					printf("Loading configuration for parse test...\n");
					if (config.config->LoadConfig((char *)config.base.fnConf.c_str())) {
						printf("Done, printing parse tree...\n\n");
						config.config->PrintConfigTree();
					} else {
						printf("Error reading configuration (major structural damage in '%s')!\n", config.base.fnConf.c_str());
					}
					delete config.config;
					dsl_cleanup();
					curl_global_cleanup();
					return 1;
					break;
				case '?':
					/* getopt_long already printed an error message. */
					break;
				default:
					printf("Error parsing command line!\n");
					printf("Options:\n\n");
					printf("--conf=filename.conf - Use alternate %s\n", IRCBOT_CONF);
					printf("--text=filename.text - Use alternate %s\n", IRCBOT_TEXT);
					printf("--cwd=directory - Use alternate working directory\n");
					printf("--test - Dump configuration parse tree (can help you find missing/mismatched opening and closing brackets)\n");
					abort();
					break;
			};
		}

#if defined(WIN32)
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (!GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
			printf("Error getting screen buffer info!\n");
			memset(&csbi, 0, sizeof(csbi));
		}

		int conw = csbi.dwSize.X;
		SetConsoleTextAttribute(hStdOut,FOREGROUND_INTENSITY|FOREGROUND_BLUE|FOREGROUND_GREEN);
#else
		int conw = 80;
		#ifdef TIOCGSIZE
			struct ttysize ts;
			ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
			conw = ts.ts_cols;
		#elif defined(TIOCGWINSZ)
			struct winsize ts;
			ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
			conw = ts.ws_col;
		#endif /* TIOCGSIZE */
#endif
		if (conw <= 0) { conw = 80; }

		printf("%s\n",align_text_center(curline," _____           _ _       ____        _   ",conw));
		printf("%s\n",align_text_center(curline,"|  __ \\         | (_)     |  _ \\      | |  ",conw));
		printf("%s\n",align_text_center(curline,"| |__) |__ _  __| |_  ___ | |_) | ___ | |_ ",conw));
		printf("%s\n",align_text_center(curline,"|  _  // _` |/ _` | |/ _ \\|  _ < / _ \\| __|",conw));
		printf("%s\n",align_text_center(curline,"| | \\ \\ (_| | (_| | | (_) | |_) | (_) | |_ ",conw));
		printf("%s\n",align_text_center(curline,"|_|  \\_\\__,_|\\__,_|_|\\___/|____/ \\___/ \\__|",conw));

		char verline[32];
		sprintf(verline, "v%d.%02d/" PLATFORM "", VERSION_MAJOR, VERSION_MINOR);
		printf("%s\n\n", align_text_right(curline, verline, (conw/2)+23));
		//printf("%s\n\n", align_text_right(curline, "RC1", (conw/2)+23));

		printf("%s\n", align_text_center(curline,"+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+",conw));
		printf("%s\n", align_text_center(curline,"|P|R|O|J|E|C|T| |B|E|C|K|E|T|T|",conw));
		printf("%s\n\n", align_text_center(curline,"+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+",conw));
#if defined(WIN32)
		SetConsoleTextAttribute(hStdOut,FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
#endif

		config.base.argc = argc;
		for (int i=0; i < argc; i++) {
			config.base.argv[i] = argv[i];
		}

		curline[0] = 0;
		if (getcwd(curline, sizeof(curline)) == NULL) {
			printf("WARNING: Could not get current working directory!\n");
			sstrcpy(curline, "." PATH_SEPS "");
		}
		if (curline[strlen(curline) - 1] != PATH_SEP) {
			strlcat(curline,  PATH_SEPS , sizeof(curline));
		}
		config.base.base_path = curline;
#ifdef WIN32
		strcat(curline, "Updater.exe.upd");
#else
		strcat(curline, "updater.upd");
#endif
		if (access(curline, 0) == 0) {
			printf("%s", _("Installing new updater...\n"));
#ifdef WIN32
			remove("Updater.exe");
			rename(curline, "Updater.exe");
#else
			remove("updater");
			rename(curline, "updater");
			//chmod("updater", 0777);
#endif
		}
#ifdef WIN32
		sprintf(curline, "%sPackageManager.exe.upd", config.base.base_path.c_str());
#else
		sprintf(curline, "%spackage_manager.upd", config.base.base_path.c_str());
#endif
		if (access(curline, 0) == 0) {
			printf("%s", _("Installing new updater...\n"));
#ifdef WIN32
			remove("PackageManager.exe");
			rename(curline, "PackageManager.exe");
#else
			remove("package_manager");
			rename(curline, "package_manager");
			//chmod("updater", 0777);
#endif
		}

		//ib_printf("IRCBot: Loading dynamic config...\n");
		//config.config->LoadBinaryConfig(config.base.fnDynamic);
		//SyncConfig();

#if defined(DEBUG)
		//mask_test();
		//system("pause");
#endif

		config.config = new Universal_Config;
		config.sockets = new Titus_Sockets3;

#if defined(WIN32)
		if (config.cPort) {
			config.cSock = config.sockets->Create();
			printf("Connecting to controller socket...\n");
			if (!config.sockets->ConnectWithTimeout(config.cSock, "127.0.0.1", config.cPort, 10000)) {
				printf("Error connecting to controller socket!\n");
				config.sockets->Close(config.cSock);
				config.cSock = NULL;
			}
		}
#endif

		//ib_printf("Starting IRCBot %s%s/%s\n",(VERSION & IRCBOT_VERSION_FLAG_LITE) ? "Lite ":"", GetBotVersionString(), PLATFORM);
		ib_printf("%s: Loading configuration...\n", IRCBOT_NAME);
		if (!config.config->LoadConfig((char *)config.base.fnConf.c_str())) {
			printf("%s: Error reading %s: %s\n", IRCBOT_NAME, nopath(config.base.fnConf.c_str()), strerror(errno));
			delete config.sockets;
			delete config.config;
			dsl_cleanup();
			curl_global_cleanup();
			return 1;
		}

		SyncConfig();

		if (!CheckConfig()) {
			UnregisterCommands();
			delete config.sockets;
			delete config.config;
			dsl_cleanup();
			curl_global_cleanup();
			return 1;
		}

		ib_printf(_("%s: Configuration loaded.\n"), IRCBOT_NAME);

#if defined(WIN32) && WINVER >= 0x0600
		if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SNAME, curline, sizeof(curline)) == 0) {
			strcpy(curline, "en_US");
		}
#else
		char * p = getenv("LC_MESSAGES");
		if (p == NULL) {
			p = getenv("LC_ALL");
		}
		if (p) {
			sstrcpy(curline, p);
		} else {
			strcpy(curline, "en_US");
		}
		p = strchr(curline, '.');
		if (p) {
			*p = 0;
		}
#endif
		str_replaceA(curline, sizeof(curline), "-", "_");

		ib_printf("%s: Detected language code: %s\n", IRCBOT_NAME, curline);

		if (config.base.langCode.length()) {
			sstrcpy(curline, config.base.langCode.c_str());
		}

		ib_printf("%s: Using language code: %s\n", IRCBOT_NAME, curline);
		std::string langFile = "langsrc" PATH_SEPS "";
		langFile += curline;
		langFile +=  PATH_SEPS ;
		langFile += "lang.ldb";
		LoadLangText(langFile.c_str());

		RegisterInternalCommands();

#if !defined(WIN32)
		if (access(config.base.fnText.c_str(), 0) != 0) {
			config.base.fnText = "/usr/share/shoutirc/" IRCBOT_TEXT "";
		}
		if (access(config.base.fnText.c_str(), 0) != 0) {
			config.base.fnText = "/usr/local/share/shoutirc/" IRCBOT_TEXT "";
		}
#endif
		if (!LoadText(config.base.fnText.c_str())) {
			ib_printf(_("%s: Error loading %s!\n"), IRCBOT_NAME, nopath(config.base.fnText.c_str()));
			UnregisterCommands();
			delete config.sockets;
			delete config.config;
			dsl_cleanup();
			curl_global_cleanup();
			return 1;
		}

		wipe_tmp_dir();

		if (config.base.ssl_cert.length()) {
#if defined(ENABLE_GNUTLS)
			ib_printf(_("%s: Enabling GnuTLS...\n"), IRCBOT_NAME);
#elif defined(ENABLE_OPENSSL)
			ib_printf(_("%s: Enabling OpenSSL...\n"), IRCBOT_NAME);
#else
			#error "Unknown TLS engine!"
#endif

			if (access(config.base.ssl_cert.c_str(), 0) == 0 && hashfile("sha1", config.base.ssl_cert.c_str(), curline, sizeof(curline)) && !stricmp(curline, "e28597010f3ddbc667bf496d9b8fd7bcb68bc14f")) {
				remove(config.base.ssl_cert.c_str());
			}

			if (access(config.base.ssl_cert.c_str(), 0) != 0) {
				make_tls_cert(config.base.ssl_cert.c_str());
			}

			if (!config.sockets->EnableSSL(config.base.ssl_cert.c_str(), TS3_SSL_METHOD_TLS)) {
				ib_printf(_("%s: Error enabling TLS! Make sure your TLS_Cert matches an appropriate PEM certificate...\n"), IRCBOT_NAME);
				ib_printf(_("%s: Error returned: %s\n"), IRCBOT_NAME, config.sockets->GetLastErrorString());

				UnregisterCommands();
				delete config.sockets;
				delete config.config;
				dsl_cleanup();
				curl_global_cleanup();
				return 1;
			}
		}

		if (config.base.bind_ip.length()) {
			config.base.myip = config.base.bind_ip;
		}

#if defined(IRCBOT_ENABLE_IRC) && defined(IRCBOT_ENABLE_SS)
		ib_printf(_("%s: IRC Networks: %d, Messages: %d, Sound Servers: %d\n\n"), IRCBOT_NAME, config.num_irc,config.messages.size(),config.num_ss);
#elif defined(IRCBOT_ENABLE_SS)
		ib_printf(_("%s: Messages: %d, Sound Servers: %d\n\n"), IRCBOT_NAME, config.messages.size(),config.num_ss);
#elif defined(IRCBOT_ENABLE_IRC)
		ib_printf(_("%s: IRC Networks: %d, Messages: %d\n\n"), IRCBOT_NAME,config.num_irc,config.messages.size());
#else
		#error Unknown combo of ENABLE options!
#endif

		/*
		ib_printf(" ________\n");
		int i=0;
		for (i=0; i < config.num_irc; i++) {
			ib_printf(_("|\t|-> %s:%d, %d chans\n"),config.ircnets[i].host.c_str(),config.ircnets[i].port,config.ircnets[i].num_chans);
			for (int j=0; j < config.ircnets[i].num_chans; j++) {
				ib_printf(_("|\t|\t|-> %s\n"),config.ircnets[i].channels[j].channel.c_str());
			}
		}
		ib_printf(_("|_______|_______/\n"));

		ib_printf(" IRC Networks\n");
		*/
		int i=0;
#if defined(IRCBOT_ENABLE_IRC)
		if (config.num_irc > 0) {
			ib_printf("__\n");
			for (i=0; i < config.num_irc; i++) {
				ib_printf(_("| %s:%d, %d chan(s)\n"),config.ircnets[i].host.c_str(),config.ircnets[i].port,config.ircnets[i].num_chans);
				for (int j=0; j < config.ircnets[i].num_chans; j++) {
					ib_printf(_("|  |-> %s\n"),config.ircnets[i].channels[j].channel.c_str());
				}
				ib_printf(_("|__/\n"));
			}
			ib_printf("\n");
		}
#endif

		TT_StartThread(SendStats, NULL, "Send Stats");

		safe_sleep(2);

		if (access(IRCBOT_CONF, 0) == 0) {
			backup_file(IRCBOT_CONF);
		}
		if (access(IRCBOT_TEXT, 0) == 0) {
			backup_file(IRCBOT_TEXT);
		}
		if (access(IRCBOT_DB, 0) == 0) {
			backup_file(IRCBOT_DB);
		}
		if (access("schedule.conf", 0) == 0) {
			backup_file("schedule.conf");
		}

		if (config.base.OnlineBackup) {
			config_online_backup();
		}

		ib_printf(_("%s: Using SQLite v%s\n"), IRCBOT_NAME, sqlite3_libversion());
		if (sqlite3_open(IRCBOT_DB, &config.hSQL) != SQLITE_OK) {
			ib_printf(_("%s: ERROR: Could not open %s!\n"), IRCBOT_NAME, IRCBOT_DB);

			UnregisterCommands();
			delete config.sockets;
			delete config.config;
			dsl_cleanup();
			curl_global_cleanup();
			return 1;
		}
		sqlite3_busy_timeout(config.hSQL, 5000);
		Query("PRAGMA case_sensitive_like=OFF;");

#if defined(IRCBOT_ENABLE_SS)
		if (!InitRatings()) {
			if (config.base.EnableRatings) { ib_printf(_("%s: Error enabling ratings, disabled.\n"), IRCBOT_NAME); }
			config.base.EnableRatings = false;
		}
#endif

		if (config.base.Fork) {
#if !defined(WIN32)
			pid_t pid = fork();
			if (pid == (pid_t)-1) {
				ib_printf(_("%s: fork() returned error: \n"), IRCBOT_NAME, strerror(errno));
			} else if (pid == 0) {
				ib_printf(_("%s: Forked to background...\n"), IRCBOT_NAME);
				config.base.Forked = true;
			} else {
				config.base.Forked = true;
				exit(0);
			}
#else
			if (GetAPI()->hWnd != NULL) {
				ib_printf(_("%s: Forked to background...\n"), IRCBOT_NAME);
				ShowWindow(GetAPI()->hWnd, SW_HIDE);
			} else {
				ib_printf(_("%s: Could not fork, could not find my HWND!\n"), IRCBOT_NAME);
			}
#endif
		}

		if (config.base.pid_file.length()) {
			if (access(config.base.pid_file.c_str(), 0) == 0) { remove(config.base.pid_file.c_str()); }
			FILE * fp = fopen(config.base.pid_file.c_str(), "wb");
			if (fp) {
#if defined(WIN32)
#define getpid GetCurrentProcessId
#endif
				fprintf(fp, "%u", getpid());
				fclose(fp);
				ib_printf(_("%s: Wrote PID file.\n"), IRCBOT_NAME);
			} else {
				ib_printf(_("%s: Error opening PID file for output: %s\n"), IRCBOT_NAME, config.base.pid_file.c_str());
			}
		}

		OpenLog();

		InitDatabase();
		LoadOldUsers();

		seed_randomizer();

		ib_printf(_("%s: Loading plugins...\n"), IRCBOT_NAME);
		LoadPlugins();
		SendMessage(-1,IB_INIT,NULL,0);

#if defined(IRCBOT_STANDALONE)
		if (SendMessage(-1, AUTODJ_IS_LOADED, NULL, 0) == 0) {
			ib_printf(_("%s: You must have a source plugin loaded to use the standalone AutoDJ, shutting down...\n"), IRCBOT_NAME);
			SendMessage(-1,IB_SHUTDOWN,NULL,0);
			UnloadPlugins();
			UnregisterCommands();
			delete config.sockets;
			delete config.config;
			dsl_cleanup();
			curl_global_cleanup();
			return 1;
		}
#endif

		if (config.remote.port) {
			ib_printf(_("%s: Starting remote control system...\n"), IRCBOT_NAME);
			TT_StartThread(remoteThreadStarter, NULL, _("remoteThreadStarter"));
		}

		LoadReperm();
		RegisterCommandAliases();

#if defined(IRCBOT_ENABLE_IRC)
		TT_StartThread(SendQ_Thread, NULL, _("SendQ"));

		for (i=0; i < config.num_irc; i++) {
			sprintf(curline, _("ircThread %d"), i);
			TT_StartThread(ircThread,(void *)i,curline);
			//safe_sleep(1);
		}
#endif

#if defined(IRCBOT_ENABLE_SS)
		TT_StartThread(scThread, NULL, _("Sound Server Info Grabber"));
#endif
		TT_StartThread(IdleThread, NULL, _("Idle Thread"));

		ib_printf(_("%s: Ready for action...\n"), IRCBOT_NAME);

		while (!config.shutdown_now) {
			safe_sleep(1);
#if defined(IRCBOT_ENABLE_IRC)
			handle_timers();
#endif
#if defined(WIN32)
			if (config.cSock != NULL && config.sockets->Select_Read(config.cSock, uint32(0)) > 0) {
				int n = config.sockets->Recv(config.cSock, curline, sizeof(curline));
				if (n > 0) {
					curline[n] = 0;
					if (strstr(curline, "exit")) {
						ib_printf(_("%s: Ordered to shut down via control socket...\n"), IRCBOT_NAME);
						config.shutdown_now = true;
						config.shutdown_reboot = false;
					}
					if (strstr(curline, "restart")) {
						ib_printf(_("%s: Ordered to restart via control socket...\n"), IRCBOT_NAME);
						config.shutdown_reboot = true;
						config.shutdown_now = true;
					}
				} else {
					ib_printf(_("%s: Lost connection to control socket!\n"), IRCBOT_NAME);
					config.sockets->Close(config.cSock);
					config.cSock = NULL;
				}
			}
#endif
		}

#if defined(WIN32)
		if (config.cSock != NULL) {
			config.sockets->Close(config.cSock);
			config.cSock = NULL;
		}
#endif

		ib_printf(_("\n%s: Shutting down...\n"), IRCBOT_NAME);
		DRIFT_DIGITAL_SIGNATURE();

		UnregisterCommandAliases();
#if defined(IRCBOT_ENABLE_SS)
		ExpireFindResults(true);
#endif

		ib_printf(_("%s: Shutting down plugins...\n"), IRCBOT_NAME);
		SendMessage(-1,IB_SHUTDOWN,NULL,0);
		UnloadPlugins();

		if (config.remote.port) {
			time_t timeo = time(NULL) + 5;
			while (config.sesFirst != NULL && time(NULL) < timeo) {
				ib_printf(_("%s: Waiting for remote sessions to end...\n"), IRCBOT_NAME);
				safe_sleep(1);
			}
			if (config.sesFirst) {
				ib_printf(_("%s: Warning! Remote sessions did not all end in the time...\n"), IRCBOT_NAME);
			}
		}

#if defined(IRCBOT_ENABLE_IRC)
		if (config.base.quitmsg.length()) {
			sprintf(curline,"QUIT :%s\r\n",config.base.quitmsg.c_str());
		} else {
			sstrcpy(curline,_("QUIT :Shutting down... (ordered at Console)\r\n"));
		}
		bool sent_quit[MAX_IRC_SERVERS];
		memset(&sent_quit, 0, sizeof(sent_quit));
#endif

		time_t timeout=time(NULL)+30;
		while (TT_NumThreadsWithID(-1) && (time(NULL) < timeout) ) {
#if defined(IRCBOT_ENABLE_IRC)
			for (int net=0; net < config.num_irc; net++) {
				if (!sent_quit[net] && config.ircnets[net].ready) {
					bSend(config.ircnets[net].sock, curline, strlen(curline));
					sent_quit[net] = true;
				}
			}
#endif
			ib_printf(_("%s: Waiting on threads to end: %02d (Will wait %02d more seconds...)\n"),IRCBOT_NAME,TT_NumThreadsWithID(-1),(timeout - time(NULL)));
			TT_PrintRunningThreadsWithID(-1);
			safe_sleep(1);
			if (TT_NumThreadsWithID(-1) <= 1) {
				config.shutdown_sendq = true;
			}
		}
		/*
		if (config.shutdown_reboot) {
			TT_THREAD_INFO * Scan = TT_GetFirstThread();
			while (Scan) {
				TT_THREAD_INFO * toKill = Scan;
				ib_printf(_("%s: Forcibly killing thread: %s\n"), IRCBOT_NAME,Scan->desc);
				TT_KillThread(toKill);
				Scan = TT_GetFirstThread();
			}
		}
		*/

		ib_printf(_("%s: Unregistering internal commands...\n"), IRCBOT_NAME);
		UnregisterCommands();

		sesMutex.Lock();
		ib_printf(_("%s: Freeing messages...\n"), IRCBOT_NAME);
		config.messages.clear();

		ib_printf(_("%s: Freeing custom variables...\n"), IRCBOT_NAME);
		config.customVars.clear();
		sesMutex.Release();

		//ib_printf("IRCBot: Saving dynamic configuration...\n");
		//SaveDynamicConfig();
		delete config.config;

		ib_printf(_("%s: Closing sockets...\n"), IRCBOT_NAME);
		delete config.sockets;
		config.sockets = NULL;
		//delete config.sockets;

		ib_printf(_("%s: Closing %s...\n"), IRCBOT_NAME, IRCBOT_DB);
		sqlite3_close(config.hSQL);

		CloseLog();

		dsl_cleanup();
		curl_global_cleanup();
#if defined(WIN32)
		if (mysql_err == 0) {
			mysql_library_end();
		}
#endif

		ib_printf(_("%s: Shutdown complete.\n"), IRCBOT_NAME);

		if (config.shutdown_reboot) {
			safe_sleep(2);
			printf("%s",_("\nSystem Reset\n\n"));
			safe_sleep(2);
			config.shutdown_reboot=false;
			config.shutdown_now=false;
		}

	} // end, cheapest hack of them all

	return 0;
}

/*
void SaveDynamicConfig() {
	char buf[MAX_PATH],buf2[MAX_PATH];
	sprintf(buf, "%s.b04", config.base.fnDynamic);
	sprintf(buf2, "%s.b05", config.base.fnDynamic);
	remove(buf2);
	rename(buf, buf2);
	sprintf(buf, "%s.b03", config.base.fnDynamic);
	sprintf(buf2, "%s.b04", config.base.fnDynamic);
	rename(buf, buf2);
	sprintf(buf, "%s.b02", config.base.fnDynamic);
	sprintf(buf2, "%s.b03", config.base.fnDynamic);
	rename(buf, buf2);
	sprintf(buf, "%s.b01", config.base.fnDynamic);
	sprintf(buf2, "%s.b02", config.base.fnDynamic);
	rename(buf, buf2);
	sprintf(buf, "%s.b01", config.base.fnDynamic);
	rename(config.base.fnDynamic, buf);
	config.config->WriteBinaryConfig(config.base.fnDynamic);
}
*/

#if defined(IRCBOT_ENABLE_IRC)
void handle_timers() {
	char buf[1024*2];
	for (int i=0; i < MAX_TIMERS && !config.shutdown_now; i++) {
		TIMERS *timer = &config.timers[i];
		if (timer->delaymin > 0 && (timer->action.length() || timer->lines.size() > 0)) {
			timer->nextFire--;
			if (timer->nextFire == 0) {
				timer->nextFire = genrand_range(timer->delaymin, timer->delaymax);
				if (config.dospam) {
					for (int net=0; net < config.num_irc; net++) {
						if ((timer->network == net || timer->network == -1) && config.ircnets[net].ready) {
							if (timer->lines.size() > 0) {
								snprintf(buf,sizeof(buf), "%s\r\n", timer->lines[genrand_range(0, timer->lines.size() - 1)].c_str());
							} else {
								snprintf(buf,sizeof(buf), "%s\r\n", timer->action.c_str());
							}
							ProcText(buf,sizeof(buf));
							bSend(config.ircnets[net].sock,buf,strlen(buf));
						}
					}
				}
			}
		}
	}
}
#endif

void backup_file(const char * ofn) {
	char buf[32768];
	time_t last_user_backup = time(NULL);
	struct tm tm;
	localtime_r(&last_user_backup, &tm);
	char * fn = zmprintf("./backup/%s.%02d.%02d.%04d.00", nopath(ofn), tm.tm_mon + 1, tm.tm_mday, tm.tm_year+1900);
	int n = 0;
	while (access(fn,0) == 0) {
		n++;
		zfree(fn);
		fn = zmprintf("./backup/%s.%02d.%02d.%04d.%02d", nopath(ofn), tm.tm_mon + 1, tm.tm_mday, tm.tm_year+1900, n);
	}

	FILE * fi = fopen(ofn, "rb");
	if (fi) {
		FILE * fo = fopen(fn, "wb");
		if (fo) {
			int n = 0;
			int tot = 0;
			while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
				tot += n;
				if (fwrite(buf, n, 1, fo) != 1) {
					ib_printf(_("%s: Error writing %s!\n"), IRCBOT_NAME, fn);
				}
			}
			ib_printf(_("%s: Created auto-backup file %s (%d bytes)\n"), IRCBOT_NAME, fn, tot);
			fclose(fo);
		} else {
			ib_printf(_("%s: Error opening %s for output\n"), IRCBOT_NAME, fn);
		}
		fclose(fi);
	}
	zfree(fn);
}

GLOBAL_CONFIG * GetConfig() { return &config; }
unsigned int GetBotVersion() { return VERSION; }

const char * GetBotVersionString() {
	static char version[8]={0};
	//                    MAMI D
	//#define VERSION 0x00030002
	if (version[0]==0) {
		sprintf(version, "%u.%02u", VERSION_MAJOR, VERSION_MINOR);
		if (VERSION & IRCBOT_VERSION_FLAG_DEBUG) {
			strcat(version, "d");
		}
	}
	return version;
}

int voidptr2int(void * ptr) {
#if defined(__x86_64__)
	int64 i64tmp = (int64)ptr;
	return i64tmp;
#else
	return int(ptr);
#endif
}
