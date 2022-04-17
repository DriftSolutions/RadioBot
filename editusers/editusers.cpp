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

#include "editusers.h"
#ifdef WIN32
#include <conio.h>
#if _MSC_VER >= 1900
char * gets(char * buf, size_t bufSize=1024) {
	return gets_s(buf, bufSize);
}
#endif
#else
#include <termios.h>
int getch() {
	struct termios oldt, newt;
	int ch;
	tcgetattr(STDIN_FILENO, &oldt );
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	ch = getchar();
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	return ch;
}
#endif

CONFIG config;

typedef std::vector<USER *> userList;
userList users;

void showmenu() {
	printf("RadioBot User Editor Menu:\n\t1. List users\n\t2. Edit user\n\t3. Add new user\n\t4. Delete user\n\t5. List all registered hostmasks\n\tM. Redisplay this menu\n\tQ. Quit\n");
}

void showeditmenu() {
	printf("Edit User Menu:\n\t1. View user info\n\t2. Add hostmask\n\t3. Delete hostmask\n\t4. Delete all hostmasks\n\t5. Change user flags\n\t6. Change password\n\tM. Redisplay this menu\n\tQ. Quit to main menu\n");
}

uint32 GetDefaultFlags() {
	return UFLAG_REQUEST;
}

bool list_cb(const char * nick, void * ptr) {
	int * count = (int *)ptr;
	*count = *count + 1;
	printf("%s\n", nick);
	return true;
}

bool listhm_cb(const char * nick, const char * hm, void * ptr) {
	int * count = (int *)ptr;
	*count = *count + 1;
	printf("%s: %s\n", nick, hm);
	return true;
}

int main(int argc, char * argv[]) {
	char buf[1024];

	memset(&config, 0, sizeof(config));

	printf("WARNING: DO NOT RUN THIS WHILE RADIOBOT IS RUNNING!!!\n");
	printf("IF YOU DO, RADIOBOT MAY OVERWRITE ANY CHANGES YOU MAKE\n\n");

	string fn = "ircbot.db";
	if (argc > 1) {
		fn = argv[1];
	}

	int err=0;
#if SQLITE_VERSION_NUMBER >= 3005000
	if ((err = sqlite3_open_v2(fn.c_str(), &config.hSQL, SQLITE_OPEN_READWRITE, NULL)) != SQLITE_OK) {
#else
	if ((err = sqlite3_open(fn.c_str(), &config.hSQL)) != SQLITE_OK) {
#endif
		if (config.hSQL) {
			printf("ERROR: Could not open ircbot.db: %s!\n", sqlite3_errmsg(config.hSQL));
			sqlite3_close(config.hSQL);
		} else {
			printf("ERROR: Could not open ircbot.db!\n");
		}
		return 1;
	}
	sqlite3_busy_timeout(config.hSQL, 5000);

	InitDatabase();
	LoadOldUsers();

	bool quitsub = false;
//	bool haschanged = false;
	showmenu();
	while (!config.quit) {
		printf("Enter your selection (m for menu): ");
		char ch = getch();
		printf("%c\n", ch);
		switch(ch) {
			case 27:
			case 'q':
			case 'Q':
				config.quit = true;
				break;

			case 'm':
			case 'M':
				showmenu();
				break;

			case '1':{
					int count=0;
					printf("Listing users...\n");
					EnumUsers(list_cb, &count);
					printf("%d users found.\n", count);
				}
				break;

			case '2':{
					printf("Which user? ");
					gets(buf);
					strtrim(buf);
					USER * User = GetUserByNick(buf);
					if (User) {
						showeditmenu();
						quitsub = false;
						while (!quitsub) {
							printf("Enter your selection: ");
							char ch = getch();
							printf("%c\n", ch);
							switch(ch) {
								case 27:
								case 'q':
								case 'Q':
									quitsub = true;
									break;

								case 'm':
								case 'M':
									showeditmenu();
									break;

								case '1':
									uflag_to_string(User->Flags, buf, sizeof(buf));
									printf("User Information for %s\nUser Flags: %s\nPassword: %s\n",User->Nick,buf,User->Pass);
									sstrcpy(buf, ctime(&User->Created));
									strtrim(buf);
									printf("User Created: %s\n", buf);
									sstrcpy(buf, ctime(&User->Seen));
									strtrim(buf);
									printf("Last Seen: %s\n", buf);
									printf("Hostmasks: %d\n", User->NumHostmasks);
									for (int i=0; i < User->NumHostmasks; i++) {
										printf("Hostmask: %s\n", User->Hostmasks[i]);
									}
									break;

								case '2':
									printf("Which hostmask should I add? ");
									gets(buf);
									strtrim(buf);

									if (strlen(buf)) {
										if (AddHostmask(User, buf)) {
											printf("Successfully added hostmask...\n");
										} else {
											printf("Error adding hostmask (user may already have the maximum of %d hostmasks)!\n", MAX_HOSTMASKS);
										}
									} else {
										printf("Error: You must specify a hostmask!\n");
									}
									break;

								case '3':
									printf("Which hostmask? (copy/paste the full hostmask) ");
									gets(buf);
									strtrim(buf);

									if (ClearHostmask(User, buf)) {
										printf("Removed hostmask...\n");
								} else {
										printf("Error removing hostmask!\n");
									}
									break;

								case '4':
									if (ClearAllHostmasks(User)) {
										printf("Removed all hostmasks...\n");
									} else {
										printf("Error removing hostmasks!\n");
									}
									break;

								case '5':{
										printf("New flags? (mode string) ");
										gets(buf);
										strtrim(buf);

										SetUserFlags(User, string_to_uflag(buf, User->Flags));
										uflag_to_string(User->Flags, buf, sizeof(buf));
										printf("User flags are now: %s\n", buf);
									}
									break;

								case '6':
									printf("New password? ");
									gets(buf);
									strtrim(buf);

									SetUserPass(User, buf);
									printf("User password changed!\n");
									break;

								default:
									printf("Unknown entry! (Press M to redisplay the menu)\n");
									break;
							}
						}
						UnrefUser(User);
						showmenu();
					} else {
						printf("Unable to find a user by that name!\n");
					}
				}
				break;

			case '3':{
					printf("New user's nickname? ");
					gets(buf);
					strtrim(buf);
					if (strlen(buf)) {
						USER * Scan = AddUser(buf, "", GetDefaultFlags());
						if (Scan) {
							//haschanged = true;
							uflag_to_string(Scan->Flags, buf, sizeof(buf));
							printf("User added with no password and flags: %s\n", buf);
							sprintf(buf,"*%s*!*@*", Scan->Nick);
							if (AddHostmask(Scan, buf)) {
								printf("Added hostmask %s\n", buf);
							} else {
								printf("Error adding default hostmask!\n");
							}
							UnrefUser(Scan);
						} else {
							printf("Error adding user!\n");
						}
					} else {
						printf("Error: No nickname specified!\n");
					}
				}
				break;

			case '4':{
					printf("Which user? ");
					gets(buf);
					strtrim(buf);
					USER * User = GetUserByNick(buf);
					if (User) {
						//DelUser does an UnrefUser() for you
						DelUser(User);
					} else {
						printf("Unable to find a user by that name!\n");
					}
				}
				break;

			case '5':{
					int count=0;
					printf("Listing hostmasks...\n");
					EnumHostmasks(listhm_cb, &count);
					printf("%d hostmasks found.\n", count);
				}
				break;

			default:
				printf("Unknown entry! (Press M to redisplay the menu)\n");
				break;
		}
	}

	printf("Closing ircbot.db ...\n");
	sqlite3_close(config.hSQL);
	config.hSQL = NULL;
	printf("Goodbye.\n");

	return 0;
}

