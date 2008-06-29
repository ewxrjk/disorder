/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007, 2008 Richard Kettlewell
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
/** @file disobedience/client.c
 * @brief GLIB integration for @ref lib/eclient.c client
 */

#include "disobedience.h"

/** @brief GSource subclass for disorder_eclient */
struct eclient_source {
  GSource gsource;
  disorder_eclient *client;
  time_t last_poll;
  GPollFD pollfd;
};

/** @brief Called before FDs are polled to choose a timeout.
 *
 * We ask for a 3s timeout and every 10s or so we force a dispatch. 
 */
static gboolean gtkclient_prepare(GSource *source,
				  gint *timeout) {
  const struct eclient_source *esource = (struct eclient_source *)source;
  D(("gtkclient_prepare"));
  if(time(0) > esource->last_poll + 10)
    return TRUE;		/* timed out */
  *timeout = 3000/*milliseconds*/;
  return FALSE;			/* please poll */
}

/** @brief Check whether we're ready to dispatch */
static gboolean gtkclient_check(GSource *source) {
  const struct eclient_source *esource = (struct eclient_source *)source;
  D(("gtkclient_check fd=%d events=%x revents=%x",
     esource->pollfd.fd, esource->pollfd.events, esource->pollfd.revents));
  return esource->pollfd.fd != -1 && esource->pollfd.revents != 0;
}

/** @brief Called after poll() (or otherwise) to dispatch an event */
static gboolean gtkclient_dispatch(GSource *source,
				   GSourceFunc attribute((unused)) callback,
				   gpointer attribute((unused)) user_data) {
  struct eclient_source *esource = (struct eclient_source *)source;
  unsigned revents,  mode;

  D(("gtkclient_dispatch"));
  revents = esource->pollfd.revents & esource->pollfd.events;
  mode = 0;
  if(revents & (G_IO_IN|G_IO_HUP|G_IO_ERR))
    mode |= DISORDER_POLL_READ;
  if(revents & (G_IO_OUT|G_IO_HUP|G_IO_ERR))
    mode |= DISORDER_POLL_WRITE;
  time(&esource->last_poll);
  disorder_eclient_polled(esource->client, mode);
  return TRUE;                          /* ??? not documented */
}

/** @brief Table of callbacks for GSource subclass */
static const GSourceFuncs sourcefuncs = {
  gtkclient_prepare,
  gtkclient_check,
  gtkclient_dispatch,
  0,
  0,
  0,
};

/** @brief Tell the mainloop what we need */
static void gtkclient_poll(void *u,
                           disorder_eclient attribute((unused)) *c,
                           int fd, unsigned mode) {
  struct eclient_source *esource = u;
  GSource *source = u;

  D(("gtkclient_poll fd=%d mode=%x", fd, mode));
  /* deconfigure the source if currently configured */
  if(esource->pollfd.fd != -1) {
    D(("calling g_source_remove_poll"));
    g_source_remove_poll(source, &esource->pollfd);
    esource->pollfd.fd = -1;
    esource->pollfd.events = 0;
  }
  /* install new settings */
  if(fd != -1 && mode) {
    esource->pollfd.fd = fd;
    esource->pollfd.events = 0;
    if(mode & DISORDER_POLL_READ)
      esource->pollfd.events |= G_IO_IN | G_IO_HUP | G_IO_ERR;
    if(mode & DISORDER_POLL_WRITE)
      esource->pollfd.events |= G_IO_OUT | G_IO_ERR;
    /* reconfigure the source */
    D(("calling g_source_add_poll"));
    g_source_add_poll(source, &esource->pollfd);
  }
}

/** @brief Report a communication-level error
 *
 * Any operations still outstanding are automatically replied by the underlying
 * @ref lib/eclient.c code.
 */
static void gtkclient_comms_error(void attribute((unused)) *u,
				  const char *msg) {
  D(("gtkclient_comms_error %s", msg));
  gtk_label_set_text(GTK_LABEL(report_label), msg);
}

/** @brief Report a protocol-level error
 *
 * The error will not be retried.  We offer a callback to the submitter of the
 * original command and if none is supplied we drop the error message in the
 * status bar.
 */
static void gtkclient_protocol_error(void attribute((unused)) *u,
				     void attribute((unused)) *v,
                                     int code,
				     const char *msg) {
  D(("gtkclient_protocol_error %s", msg));
  gtk_label_set_text(GTK_LABEL(report_label), msg);
}

/** @brief Report callback from eclient */
static void gtkclient_report(void attribute((unused)) *u,
                             const char *msg) {
  if(!msg)
    /* We're idle - clear the report line */
    gtk_label_set_text(GTK_LABEL(report_label), "");
}

/** @brief Repoort an unhandled protocol-level error to the user */
void popup_protocol_error(int attribute((unused)) code,
                          const char *msg) {
  gtk_label_set_text(GTK_LABEL(report_label), msg);
  popup_msg(GTK_MESSAGE_ERROR, msg);
}

/** @brief Table of eclient callbacks */
static const disorder_eclient_callbacks gtkclient_callbacks = {
  gtkclient_comms_error,
  gtkclient_protocol_error,
  gtkclient_poll,
  gtkclient_report
};

/** @brief Create a @ref disorder_eclient using the GLib main loop */
disorder_eclient *gtkclient(void) {
  GSource *source;
  struct eclient_source *esource;

  D(("gtkclient"));
  source = g_source_new((GSourceFuncs *)&sourcefuncs,
			sizeof (struct eclient_source));
  esource = (struct eclient_source *)source;
  esource->pollfd.fd = -1;
  esource->client = disorder_eclient_new(&gtkclient_callbacks, source);
  if(!esource->client) {
    g_source_destroy(source);
    return 0;
  }
  g_source_attach(source, 0);
  return esource->client;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
