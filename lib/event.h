/*
 * This file is part of DisOrder.
 * Copyright (C) 2004 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef EVENT_H
#define EVENT_H

typedef struct ev_source ev_source;

struct rusage;
struct sink;

ev_source *ev_new(void);
/* create a new event loop */

int ev_run(ev_source *ev);
/* run an event loop.  If any callback returns nonzero then that value
 * is returned.  If an error occurs then -1 is returned and @error@ is
 * called. */

/* file descriptors ***********************************************************/

typedef enum {
  ev_read,
  ev_write,
  ev_except,

  ev_nmodes
} ev_fdmode;

typedef int ev_fd_callback(ev_source *ev, int fd, void *u);
/* signature for fd callback functions */

int ev_fd(ev_source *ev,
	  ev_fdmode mode,
	  int fd,
	  ev_fd_callback *callback,
	  void *u);
/* register a callback on a file descriptor */

int ev_fd_cancel(ev_source *ev,
		 ev_fdmode mode,
		 int fd);
/* cancel a callback on a file descriptor */

int ev_fd_disable(ev_source *ev,
		  ev_fdmode mode,
		  int fd);
/* temporarily disable callbacks on a file descriptor */

int ev_fd_enable(ev_source *ev,
		 ev_fdmode mode,
		 int fd);
/* re-enable callbacks on a file descriptor */

/* timeouts *******************************************************************/

typedef int ev_timeout_callback(ev_source *ev,
				const struct timeval *now,
				void *u);
/* signature for timeout callback functions */

typedef void *ev_timeout_handle;

int ev_timeout(ev_source *ev,
	       ev_timeout_handle *handlep,
	       const struct timeval *when,
	       ev_timeout_callback *callback,
	       void *u);
/* register a timeout callback.  If @handlep@ is not a null pointer then a
 * handle suitable for ev_timeout_cancel() below is returned through it. */

int ev_timeout_cancel(ev_source *ev,
		      ev_timeout_handle handle);
/* cancel a timeout callback */

/* signals ********************************************************************/

typedef int ev_signal_callback(ev_source *ev,
			       int sig,
			       void *u);
/* signature for signal callback functions */

int ev_signal(ev_source *ev,
	      int sig,
	      ev_signal_callback *callback,
	      void *u);
/* register a signal callback */

int ev_signal_cancel(ev_source *ev,
		     int sig);
/* cancel a signal callback */

void ev_signal_atfork(ev_source *ev);
/* unhandle and unblock handled signals - call after calling fork and
 * then setting @exitfn@ */

/* child processes ************************************************************/

typedef int ev_child_callback(ev_source *ev,
			      pid_t pid,
			      int status,
			      const struct rusage *rusage,
			      void *u);
/* signature for child wait callbacks */

int ev_child_setup(ev_source *ev);
/* must be called exactly once before @ev_child@ */

int ev_child(ev_source *ev,
	     pid_t pid,
	     int options,
	     ev_child_callback *callback,
	     void *u);
/* register a child callback.  @options@ must be 0 or WUNTRACED. */

int ev_child_cancel(ev_source *ev,
		    pid_t pid);
/* cancel a child callback. */

/* socket listeners ***********************************************************/

typedef int ev_listen_callback(ev_source *ev,
			       int newfd,
			       const struct sockaddr *remote,
			       socklen_t rlen,
			       void *u);
/* callback when a connection arrives. */

int ev_listen(ev_source *ev,
	      int fd,
	      ev_listen_callback *callback,
	      void *u);
/* register a socket listener callback.  @bind@ and @listen@ should
 * already have been called. */

int ev_listen_cancel(ev_source *ev,
		     int fd);
/* cancel a socket listener callback */

/* buffered writer ************************************************************/

typedef struct ev_writer ev_writer;

typedef int ev_error_callback(ev_source *ev,
			      int fd,
			      int errno_value,
			      void *u);
/* called when an error occurs on a writer.  Called with @errno_value@
 * of 0 when finished. */

ev_writer *ev_writer_new(ev_source *ev,
			 int fd,
			 ev_error_callback *callback,
			 void *u);
/* create a new buffered writer, writing to @fd@.  Calls @error@ if an
 * error occurs. */

int ev_writer_close(ev_writer *w);
/* close a writer (i.e. promise not to write to it any more) */

int ev_writer_cancel(ev_writer *w);
/* cancel a writer */

int ev_writer_flush(ev_writer *w);
/* attempt to flush the buffer */

struct sink *ev_writer_sink(ev_writer *w) attribute((const));
/* return a sink for the writer - use this to actually write to it */

/* buffered reader ************************************************************/

typedef struct ev_reader ev_reader;

typedef int ev_reader_callback(ev_source *ev,
			       ev_reader *reader,
			       int fd,
			       void *ptr,
			       size_t bytes,
			       int eof,
			       void *u);
/* Called when data is read or an error occurs.  @ptr@ and @bytes@
 * indicate the amount of data available. @eof@ will be 1 at eof. */

ev_reader *ev_reader_new(ev_source *ev,
			 int fd,
			 ev_reader_callback *callback,
			 ev_error_callback *error_callback,
			 void *u);
/* register a new reader.  @callback@ will be called whenever data is
 * available. */

void ev_reader_buffer(ev_reader *r, size_t nbytes);
/* specify a buffer size *case */

void ev_reader_consume(ev_reader *r, size_t nbytes);
/* consume @nbytes@ bytes. */

int ev_reader_cancel(ev_reader *r);
/* cancel a reader */

int ev_reader_disable(ev_reader *r);
/* disable reading */

int ev_reader_incomplete(ev_reader *r);
/* callback didn't fully process buffer, but would like another
 * callback (used where processing more would block too long) */

int ev_reader_enable(ev_reader *r);
/* enable reading.  If there is unconsumed data then you get a
 * callback next time round the event loop even if nothing new has
 * been read.
 *
 * The idea is in your read callback you come across a line (or
 * whatever) that can't be processed immediately.  So you set up
 * processing and disable reading.  Later when you finish processing
 * you re-enable.  You'll automatically get another callback pretty
 * much direct from the event loop (not from inside ev_reader_enable)
 * so you can handle the next line (or whatever) if the whole thing
 * has in fact already arrived.
 */

#endif /* EVENT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
/* arch-tag:8e6f230cabf206361c14897f1e03b536 */
