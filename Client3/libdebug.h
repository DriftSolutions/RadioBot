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


#if defined(LDC_DLL)
	#if defined(LIBDEBUGCLIENT_EXPORTS)
		#define LDC_API __declspec(dllexport)
	#else
		#define LDC_API __declspec(dllimport)
	#endif
#else
	#define LDC_API
#endif

#if defined(__cplusplus)
extern "C" {
#endif
LDC_API bool ldc_Init();
LDC_API void ldc_SendString(char * str);
LDC_API void ldc_SendStringF(char * str, ...);
LDC_API void ldc_Quit();
#if defined(__cplusplus)
}
#endif
