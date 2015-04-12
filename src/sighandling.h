/* =========================================================================
 * File:        sighandling.h
 * Description: Signal handling for the daemon, including the
 *              signal catching daemon
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: sighandling.h 761 2015-03-03 10:18:20Z ljp $
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

#ifndef SIGHANDLING_H
#define	SIGHANDLING_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * received_signal
 * The last signal received
 */
extern volatile sig_atomic_t received_signal;

void *
sighand_thread(void *arg);

void
setup_sigsegv_handler(void);

void
setup_sighandling(void);


#ifdef	__cplusplus
}
#endif

#endif	/* SIGHANDLING_H */

