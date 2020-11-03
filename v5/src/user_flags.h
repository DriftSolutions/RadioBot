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

#ifndef __USER_FLAGS_H__
#define __USER_FLAGS_H__

/** \defgroup uflags User Flags List
 * @{
 */

#define UFLAG_a 0x00000001
#define UFLAG_b 0x00000002
#define UFLAG_c 0x00000004
#define UFLAG_d 0x00000008
#define UFLAG_e 0x00000010
#define UFLAG_f 0x00000020
#define UFLAG_g 0x00000040
#define UFLAG_h 0x00000080
#define UFLAG_i 0x00000100
#define UFLAG_j 0x00000200
#define UFLAG_k 0x00000400
#define UFLAG_l 0x00000800
#define UFLAG_m 0x00001000
#define UFLAG_n 0x00002000
#define UFLAG_o 0x00004000
#define UFLAG_p 0x00008000
#define UFLAG_q 0x00010000
#define UFLAG_r 0x00020000
#define UFLAG_s 0x00040000
#define UFLAG_t 0x00080000
#define UFLAG_u 0x00100000
#define UFLAG_v 0x00200000
#define UFLAG_w 0x00400000
#define UFLAG_x 0x00800000
#define UFLAG_y 0x01000000
#define UFLAG_z 0x02000000
#define UFLAG_A 0x04000000
#define UFLAG_B 0x08000000
#define UFLAG_C 0x10000000
#define UFLAG_D 0x20000000
#define UFLAG_E 0x40000000
#define UFLAG_F 0x80000000 //- DO NOT USE!

#define UFLAG_MASTER			UFLAG_m
#define UFLAG_OP				UFLAG_o
#define UFLAG_HOP				UFLAG_h
#define UFLAG_DJ				UFLAG_d

#define UFLAG_DIE				UFLAG_i
#define UFLAG_REMOTE			UFLAG_r
#define UFLAG_RATE				UFLAG_n
#define UFLAG_REQUEST			UFLAG_q
#define UFLAG_USER_RECORDS		UFLAG_u
#define UFLAG_BASIC_SOURCE		UFLAG_s
#define UFLAG_ADVANCED_SOURCE	UFLAG_a
#define UFLAG_CHANADMIN			UFLAG_c
#define UFLAG_SKYPE				UFLAG_k
#define UFLAG_TWEET				UFLAG_t
#define UFLAG_SOURCE_NO_REQ_BY	UFLAG_b

#define UFLAG_DCC_CHAT			UFLAG_e
#define UFLAG_DCC_FSERV			UFLAG_f
#define UFLAG_DCC_XFER			UFLAG_g
#define UFLAG_DCC_ADMIN			UFLAG_j

#define UFLAG_IGNORE			UFLAG_x

/**@}*/

#endif // __USER_FLAGS_H__
