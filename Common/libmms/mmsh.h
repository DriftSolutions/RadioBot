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

/*
 * Copyright (C) 2002-2003 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: mmsh.h,v 1.4 2006/06/06 09:52:28 shawarma Exp $
 *
 * libmmsh public header
 */

#ifndef HAVE_MMSH_H
#define HAVE_MMSH_H

#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include "mmsio.h"

#ifdef WIN32
#ifdef LIBMMS_EXPORTS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllimport)
#endif
#else
#define DLLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct mmsh_s mmsh_t;

DLLEXPORT char*    mmsh_connect_common(int *s ,int *port, char *url, char **host, char **path, char **file);
DLLEXPORT mmsh_t*   mmsh_connect (mms_io_t *io, void *data, const char *url_, int bandwidth);

DLLEXPORT int      mmsh_read (mms_io_t *io, mmsh_t *instance, char *data, int len);
DLLEXPORT uint32_t mmsh_get_length (mmsh_t *instance);
DLLEXPORT off_t    mmsh_get_current_pos (mmsh_t *instance);
DLLEXPORT void     mmsh_close (mmsh_t *instance);

DLLEXPORT int      mmsh_peek_header (mmsh_t *instance, char *data, int maxsize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
