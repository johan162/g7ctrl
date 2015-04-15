/* =========================================================================
 * File:        g7srvcmd.h
 * Description: Handle all commands relating to the server itself. The
 *              client give these commands with an initial '.'
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2013-2015 Johan Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 * =========================================================================
 */

#ifndef G7SRVCMD_H
#define	G7SRVCMD_H

#ifdef	__cplusplus
extern "C" {
#endif

void
exec_srv_command(struct client_info *cli_info, char *rcmdstr);

void
srvcmd_help(struct client_info *cli_info, char **fields);

#ifdef	__cplusplus
}
#endif

#endif	/* G7SRVCMD_H */

