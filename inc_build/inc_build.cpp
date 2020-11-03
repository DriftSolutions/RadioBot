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

int main(int argc, char * argv[]) {
	char file[256],ini[256];
	if (argc == 3) {
		strcpy(ini,argv[1]);
		strcpy(file,argv[2]);
	} else if (argc == 2) {
		strcpy(ini,argv[1]);
		strcpy(file,"build.h");
	} else {
		strcpy(ini,"build.ini");
		strcpy(file,"build.h");
	}

	int n = Get_INI_Int(ini, "build", "num", -1);
	if (n <= -1) {
		printf("Error reading INI: %s\n", ini);
		return 1;
	}

	FILE * fp = fopen(file,"wb");
	if (fp) {
		n++;
		printf("New Build #: %d\n",n);
		fprintf(fp,"#define BUILD %d\n",n);
		fprintf(fp,"#define BUILD_STRING \"%d\"\n",n);
		fclose(fp);
		Write_INI_Int(ini, "build", "num", n);
	} else {
		printf("Error opening %s for write.\n",file);
		return 1;
	}
	return 0;
}

