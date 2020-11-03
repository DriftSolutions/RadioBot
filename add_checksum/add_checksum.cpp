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
#define ENABLE_OPENSSL
#include <drift/dsl.h>
#include "../Common/plugin_checksums.h"

int main(int argc, char * argv[]) {
	if (argc < 2) {
		printf("Usage: %s filename.exe/dll\n", nopath(argv[0]));
		return 1;
	}

	dsl_init();

	int ret = 1;
	FILE * fp = fopen(argv[1], "r+b");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		int len = ftell(fp);

		if (len >= (int)sizeof(PLUGIN_CHECKSUM)) {
			PLUGIN_CHECKSUM cs;
			fseek(fp, -1 * long(sizeof(PLUGIN_CHECKSUM)), SEEK_END);
			if (fread(&cs, sizeof(PLUGIN_CHECKSUM), 1, fp)) {
				if (cs.magic == PLUGIN_CHECKSUM_MAGIC) {
					printf("This file already has a checksum, recalculating...\n");
					//fclose(fp);
					//return 0;
					len -= sizeof(PLUGIN_CHECKSUM);
				}
			}
		}

		fseek(fp, 0, SEEK_SET);
		uint8 * data = (uint8 *)malloc(len);
		if (fread(data, len, 1, fp) == 1) {
			PLUGIN_CHECKSUM pc;
			memset(&pc, 0, sizeof(pc));
			pc.magic	= PLUGIN_CHECKSUM_MAGIC;
			pc.type		= PLUGIN_CHECKSUM_SHA_256;

			HASH_CTX * ctx = hash_init("sha256");
			if (ctx == NULL) {
				printf("Error initializing sha256, falling back to sha1...\n");
				pc.type = PLUGIN_CHECKSUM_SHA_1;
				ctx = hash_init("sha1");
			}

			hash_update(ctx, data, len);
			hash_finish(ctx, pc.checksum, sizeof(pc.checksum));

			/*
			DSL_SHA1 sha1;
			sha1.addBytes(data, len);
			sha1.finish();
			unsigned char digest[20];
			sha1.getDigest(digest);
	#if defined(DEBUG)
			printf("SHA-1 Checksum: ");
			sha1.hexPrinter(digest, 20);
			printf("\n");
	#endif
			memcpy(pc.checksum, digest, 20);
			*/

			fseek(fp, len, SEEK_SET);
			size_t n = fwrite(&pc, sizeof(pc), 1, fp);
			if (n == 1) {
				printf("Added checksum to file...\n");
				ret = 0;
			} else {
				printf("Error writing checksum to file! (%u, %s)\n", n, strerror(errno));
				ret = 3;
			}
		} else {
			printf("Error reading %s! (%s)\n", argv[1], strerror(errno));
			ret = 4;
		}
		free(data);
		fclose(fp);
	} else {
		printf("Error opening %s! (%s)\n", argv[1], strerror(errno));
	}

	dsl_cleanup();
	return ret;
}

