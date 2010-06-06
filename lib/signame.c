/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/** @file lib/signame.c
 * @brief Signal names
 */
#include "common.h"

#include <signal.h>
#include <stddef.h>

#include "table.h"
#include "signame.h"

static const struct sigtable {
  int signal;
  const char *name;
} signals[] = {
#define S(sig) { sig, #sig }
  /* table must be kept in lexical order */
#ifdef SIGABRT
  S(SIGABRT),
#endif
#ifdef SIGALRM
  S(SIGALRM),
#endif
#ifdef SIGBUS
  S(SIGBUS),
#endif
#ifdef SIGCHLD
  S(SIGCHLD),
#endif
#ifdef SIGCONT
  S(SIGCONT),
#endif
#ifdef SIGFPE
  S(SIGFPE),
#endif
#ifdef SIGHUP
  S(SIGHUP),
#endif
#ifdef SIGILL
  S(SIGILL),
#endif
#ifdef SIGINT
  S(SIGINT),
#endif
#ifdef SIGIO
  S(SIGIO),
#endif
#ifdef SIGIOT
  S(SIGIOT),
#endif
#ifdef SIGKILL
  S(SIGKILL),
#endif
#ifdef SIGPIPE
  S(SIGPIPE),
#endif
#ifdef SIGPOLL
  S(SIGPOLL),
#endif
#ifdef SIGPROF
  S(SIGPROF),
#endif
#ifdef SIGPWR
  S(SIGPWR),
#endif
#ifdef SIGQUIT
  S(SIGQUIT),
#endif
#ifdef SIGSEGV
  S(SIGSEGV),
#endif
#ifdef SIGSTKFLT
  S(SIGSTKFLT),
#endif
#ifdef SIGSTOP
  S(SIGSTOP),
#endif
#ifdef SIGSYS
  S(SIGSYS),
#endif
#ifdef SIGTERM
  S(SIGTERM),
#endif
#ifdef SIGTRAP
  S(SIGTRAP),
#endif
#ifdef SIGTSTP
  S(SIGTSTP),
#endif
#ifdef SIGTTIN
  S(SIGTTIN),
#endif
#ifdef SIGTTOU
  S(SIGTTOU),
#endif
#ifdef SIGURG
  S(SIGURG),
#endif
#ifdef SIGUSR1
  S(SIGUSR1),
#endif
#ifdef SIGUSR2
  S(SIGUSR2),
#endif
#ifdef SIGVTALRM
  S(SIGVTALRM),
#endif
#ifdef SIGWINCH
  S(SIGWINCH),
#endif
#ifdef SIGXCPU
  S(SIGXCPU),
#endif
#ifdef SIGXFSZ
  S(SIGXFSZ),
#endif
#undef S
};

/** @brief Map a signal name to its number
 * @param s Signal name e.g. "SIGINT"
 * @return Signal value or -1 if not found
 */
int find_signal(const char *s) {
  int n;

  if((n = TABLE_FIND(signals, name, s)) < 0)
    return -1;
  return signals[n].signal;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
