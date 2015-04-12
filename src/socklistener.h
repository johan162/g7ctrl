/* =========================================================================
 * File:        SOCKLISTENER.C
 * Description: The daemons real work is done here after the initial
 *              daemon householding has been setup.
 *              running to instances of the program at once.
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Ids$
 *
 * Copyright (C) 2013-2015  Johan Persson
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

#ifndef WORKER_H
#define	WORKER_H

#ifdef	__cplusplus
extern "C" {
#endif

int
startupsrv(void);

#ifdef	__cplusplus
}
#endif

#endif	/* WORKER_H */

