/* =========================================================================
 * File:        sighandling.c
 * Description: Signal handling for the daemon, including the
 *              signal catching daemon
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

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard defines
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <execinfo.h>
#include <syslog.h>

#ifdef SIGSEGV_HANDLER
#include <execinfo.h>
#include <ucontext.h>
#endif

#include "config.h"
#include "g7config.h"
#include "g7ctrl.h"
#include "utils.h"
#include "libxstr/xstr.h"
#include "sighandling.h"
#include "logger.h"
#include "presets.h"

/*
 * received_signal
 * Keep track of signal interrupting the daemon (cause to shutdown)
 * This is recorded by the signal receiving thread and tested in the
 * main thread.
 */
volatile sig_atomic_t received_signal = 0;

/* Protect manipulation of received_signal */
pthread_mutex_t sig_mutex = PTHREAD_MUTEX_INITIALIZER;


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/**
 * This is the signal receiving thread. In order to guarantee that we don't get any
 * deadlocks all signals is masked in all other threads and only this thread can receive
 * signals. The function then sets the global variable handled_signal.
 *
 * The tread is shut down when the daemon is terminated.
 *
 * @param arg Not used
 * @return
 */
void *
sighand_thread(void *arg) {
    sigset_t signal_set;
    int sig;

    while (1) {
        // Start with a clean signal set
        sigemptyset(&signal_set);

        // Here we wait for signals used to terminate the server
        // SIGTERM  = The normal way to stop a process (Or by pressing Ctrl-C)
        // SIGHUP   = Standard way to have the daemon reload its config file
        sigaddset(&signal_set, SIGTERM);
        sigaddset(&signal_set, SIGHUP);

        // FIXME: When running through gdb we should not catch the SIGINT signal
        // since that is used when stepping
        sigaddset(&signal_set, SIGINT);

        // No stop and wait for a signal
        sigwait(&signal_set, &sig);

        if (SIGHUP == sig) {
            // Just reload the config and presets and go back to waiting
            pthread_mutex_lock(&sig_mutex);
            logmsg(LOG_INFO, "Re-reading config file \"%s\" ...", daemon_config_file);
            setup_inifile();
            read_inisettings();
            logmsg(LOG_INFO, "Rereading preset files ...");
            refreshPresets(-1);
            close_inifile();
            pthread_mutex_unlock(&sig_mutex);

        } else {
            pthread_mutex_lock(&sig_mutex);
            received_signal = sig;
            pthread_mutex_unlock(&sig_mutex);
        }
    }
}

#ifdef SIGSEGV_HANDLER

/**
 * Handler for signals indicating serious program errors. The signal we
 * catch here are SIGQUIT, SIGSEGV, SIGBUS, SIGILL, SIGABRT and SIGFPE
 * When a signal is received we gather some general information about the
 * exception and also do a stack walk back to generate a short stack trace.
 * The stack trace is written to "/tmp/g7ctrl_stack-NNNNNN.crash"
 * @param sig_num
 * @param info
 * @param ucontext
 */
void
sigsegv_handler(int sig_num, siginfo_t * info, void * ucontext) {
    void * caller_address;

    ucontext_t *uc = (ucontext_t *) ucontext;
    // NOTE: For x86 we should use REG_EIP but for x86_64 we should use REG_RIP
#ifdef __x86_64__
    caller_address = (void *) uc->uc_mcontext.gregs[REG_RIP];
#else
    caller_address = (void *) uc->uc_mcontext.gregs[REG_EIP];
#endif
    char crashlogfile[256];
    snprintf(crashlogfile, sizeof (crashlogfile), "/tmp/%s_stack-%lu.crash", PACKAGE, (unsigned long) time(NULL));

    int fd = open(crashlogfile,
            O_TRUNC | O_WRONLY | O_CREAT,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (fd >= 0) {
        char *code;
        switch (info->si_signo) {
            case SIGSEGV:
                switch (info->si_code) {
                    case SEGV_MAPERR:
                        code = "addressed not mapped to object";
                        break;
                    case SEGV_ACCERR:
                        code = "invalid permission for mapped object";
                        break;
                    default:
                        code = "no specific exception code";
                        break;
                }
                break;
            case SIGILL:
                switch (info->si_code) {
                    case ILL_ILLOPC:
                        code = "illegal opcode";
                        break;
                    case ILL_ILLOPN:
                        code = "illegal operand";
                        break;
                    case ILL_ILLADR:
                        code = "illegal addressing mode";
                        break;
                    case ILL_ILLTRP:
                        code = "illegal trap";
                        break;
                    case ILL_PRVOPC:
                        code = "privileged opcode";
                        break;
                    case ILL_PRVREG:
                        code = "privileged register";
                        break;
                    case ILL_COPROC:
                        code = "coprocessor error";
                        break;
                    case ILL_BADSTK:
                        code = "internal stack error";
                        break;
                    default:
                        code = "no specific exception code";
                        break;
                }
                break;
            case SIGFPE:
                switch (info->si_code) {
                    case FPE_INTDIV:
                        code = "integer divide by zero";
                        break;
                    case FPE_INTOVF:
                        code = "integer overflow";
                        break;
                    case FPE_FLTDIV:
                        code = "floating-point divide by zero";
                        break;
                    case FPE_FLTOVF:
                        code = "floating-point overflow";
                        break;
                    case FPE_FLTUND:
                        code = "floating-point underflow";
                        break;
                    case FPE_FLTRES:
                        code = "floating-point inexact result";
                        break;
                    case FPE_FLTINV:
                        code = "floating-point invalid operation";
                        break;
                    case FPE_FLTSUB:
                        code = "subscript out of range";
                        break;
                    default:
                        code = "no specific exception code";
                        break;
                }
                break;
            case SIGBUS:
                switch (info->si_code) {
                    case BUS_ADRALN:
                        code = "invalid address alignment";
                        break;
                    case BUS_ADRERR:
                        code = "nonexistent physical address";
                        break;
                    case BUS_OBJERR:
                        code = "object-specific hardware error";
                        break;
                    default:
                        code = "no specific exception code";
                        break;
                }
                break;
            default:
                code = "No specific exception code";
                break;
        }
        char sbuff[1024];
        snprintf(sbuff, sizeof (sbuff), "signal %d : \"%s\" at %p accessing %p (Reason: \"%s\")\n\n"
                "HINT: Use \"addr2line -f -e %s <ADDRESS>\" to find out which function and line is giving this condition.\n"
                "For this to work the command must be run from the same directory as where the program is\n"
                "installed and <ADDRESS> replaced by the last valid address from the stack trace below.\n\n"
                "Stack trace:\n",
                sig_num, strsignal(sig_num), (void *) caller_address, info->si_addr, code,
                PACKAGE);
        int ret = write(fd, sbuff, strlen(sbuff));
        if (-1 < ret) {
            void * bt_lines[100];
            int size = backtrace(bt_lines, sizeof (bt_lines) / sizeof (void *));
            bt_lines[1] = caller_address;
            backtrace_symbols_fd(bt_lines, size, fd);
        }
        close(fd);
    }

    // Re-raise the signal to get the full core dump
    raise(sig_num);

}
#endif

/**
 * Setup signal handler using sigaction(). We catch the following signals:
 * SIGSEGV, SIGBUS, SIGABRT,SIGQUIT,SIGFPE
 * @see sigsegv_handler()
 */
void
setup_sigsegv_handler(void) {
#ifdef SIGSEGV_HANDLER
    struct sigaction sigact;

    CLEAR(sigact);
    sigact.sa_sigaction = sigsegv_handler;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO | SA_RESETHAND;

    if (sigaction(SIGSEGV, &sigact, (struct sigaction *) NULL) != 0) {
        fprintf(stderr, "Cannot create signal handler for signal %d (%s)\n", SIGSEGV, strsignal(SIGSEGV));
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGILL, &sigact, (struct sigaction *) NULL) != 0) {
        fprintf(stderr, "Cannot create signal handler for signal %d (%s)\n", SIGILL, strsignal(SIGSEGV));
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGBUS, &sigact, (struct sigaction *) NULL) != 0) {
        fprintf(stderr, "Cannot create signal handler for signal %d (%s)\n", SIGBUS, strsignal(SIGBUS));
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGABRT, &sigact, (struct sigaction *) NULL) != 0) {
        fprintf(stderr, "Cannot create signal handler for signal %d (%s)\n", SIGABRT, strsignal(SIGABRT));
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGQUIT, &sigact, (struct sigaction *) NULL) != 0) {
        fprintf(stderr, "Cannot create signal handler for signal %d (%s)\n", SIGQUIT, strsignal(SIGQUIT));
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGFPE, &sigact, (struct sigaction *) NULL) != 0) {
        fprintf(stderr, "Cannot create signal handler for signal %d (%s)\n", SIGFPE, strsignal(SIGFPE));
        exit(EXIT_FAILURE);
    }
#endif
}
#pragma GCC diagnostic pop

void
setup_sighandling(void) {
    sigset_t signal_set;
    static pthread_t signal_thread;

    // We use a separate thread to receive all the signals so we must
    // block all signals in all other threads

    // We want to block all signals apart from the OS system signals
    // that indicates a serious error in the program (like a segmentation
    // violation or a very unlikely bus error). We also allow SIGQUIT
    // as a mean to force a core dump in a different way.
    sigfillset(&signal_set);

    // The following signal will be allowed and are caught by our signal
    // handler and causes a stack trace dump if enabled
    sigdelset(&signal_set, SIGSEGV); // Memory overwrite
    sigdelset(&signal_set, SIGILL); // Illegal instruction
    sigdelset(&signal_set, SIGQUIT); // User sent QUIT signal
    sigdelset(&signal_set, SIGBUS); // Bus error (writing to an illegal address)
    sigdelset(&signal_set, SIGFPE); // Floating point exception
    sigdelset(&signal_set, SIGABRT); // Raised by library routines in case of emergency

    // The signal thread will set a volatile global flag that is checked
    // to detect a shutdown and handle that gracefully.
    pthread_sigmask(SIG_BLOCK, &signal_set, NULL);
    pthread_create(&signal_thread, NULL, sighand_thread, NULL);
    setup_sigsegv_handler();

}

/* EOF */
