/*
 * This file is part of DisOrder.
 * Copyright (C) 2006-2008 Richard Kettlewell
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
/** @file lib/eclient.c
 * @brief Client code for event-driven programs
 */

#include "common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <inttypes.h>
#include <stddef.h>
#include <time.h>

#include "log.h"
#include "mem.h"
#include "configuration.h"
#include "queue.h"
#include "eclient.h"
#include "charset.h"
#include "hex.h"
#include "split.h"
#include "vector.h"
#include "inputline.h"
#include "kvp.h"
#include "syscalls.h"
#include "printf.h"
#include "addr.h"
#include "authhash.h"
#include "table.h"
#include "client-common.h"

/* TODO: more commands */

/** @brief How often to send data to the server when receiving logs */
#define LOG_PROD_INTERVAL 10

/* Types *********************************************************************/

/** @brief Client state */
enum client_state {
  state_disconnected,          /**< @brief not connected */
  state_connecting,            /**< @brief waiting for connect() */
  state_connected,             /**< @brief connected but not authenticated */
  state_idle,                  /**< @brief not doing anything */
  state_cmdresponse,           /**< @brief waiting for command resonse */
  state_body,                  /**< @brief accumulating body */
  state_log,                   /**< @brief monitoring log */
};

/** @brief Names for @ref client_state */
static const char *const states[] = {
  "disconnected",
  "connecting",
  "connected",
  "idle",
  "cmdresponse",
  "body",
  "log"
};

struct operation;                       /* forward decl */

/** @brief Type of an operation callback */
typedef void operation_callback(disorder_eclient *c, struct operation *op);

/** @brief A pending operation.
 *
 * This can be either a command or part of the authentication protocol.  In the
 * former case new commands are appended to the list, in the latter case they
 * are inserted at the front. */
struct operation {
  struct operation *next;          /**< @brief next operation */
  char *cmd;                       /**< @brief command to send or 0 */
  char **body;                     /**< @brief command body */
  operation_callback *opcallback;  /**< @brief internal completion callback */
  void (*completed)();             /**< @brief user completion callback or 0 */
  void *v;                         /**< @brief data for COMPLETED */
  disorder_eclient *client;        /**< @brief owning client */

  /** @brief true if sent to server
   *
   * This is cleared by disorder_eclient_close(), forcing all queued
   * commands to be transparently resent.
   */
  int sent;
};

/** @brief Client structure */
struct disorder_eclient {
  char *ident;
  int fd;                               /**< @brief connection to server */
  enum client_state state;              /**< @brief current state */
  int authenticated;                    /**< @brief true when authenicated */
  struct dynstr output;                 /**< @brief output buffer */
  struct dynstr input;                  /**< @brief input buffer */
  int eof;                              /**< @brief input buffer is at EOF */
  const disorder_eclient_callbacks *callbacks; /**< @brief error callbacks */
  void *u;                              /**< @brief user data */
  struct operation *ops;                /**< @brief queue of operations */
  struct operation **opstail;           /**< @brief queue tail */
  /* accumulated response */
  int rc;                               /**< @brief response code */
  char *line;                           /**< @brief complete line */
  struct vector vec;                    /**< @brief body */

  const disorder_eclient_log_callbacks *log_callbacks;
  /**< @brief log callbacks
   *
   * Once disorder_eclient_log() has been issued this is always set.  When we
   * re-connect it is checked to re-issue the log command.
   */
  void *log_v;                          /**< @brief user data */
  unsigned long statebits;              /**< @brief latest state */

  time_t last_prod;
  /**< @brief last time we sent a prod
   *
   * When we are receiving log data we send a "prod" byte to the server from
   * time to time so that we detect broken connections reasonably quickly.  The
   * server just ignores these bytes.
   */

  /** @brief Protocol version */
  int protocol;

  /** @brief True if enabled */
  int enabled;
};

/* Forward declarations ******************************************************/

static int start_connect(disorder_eclient *c);
static void process_line(disorder_eclient *c, char *line);
static void maybe_connected(disorder_eclient *c);
static void authbanner_opcallback(disorder_eclient *c,
                                  struct operation *op);
static void authuser_opcallback(disorder_eclient *c,
                                struct operation *op);
static void complete(disorder_eclient *c);
static void send_output(disorder_eclient *c);
static void put(disorder_eclient *c, const char *s, size_t n);
static void read_input(disorder_eclient *c);
static void stash_command(disorder_eclient *c,
                          int queuejump,
                          operation_callback *opcallback,
                          void (*completed)(),
                          void *v,
                          int nbody,
                          char **body,
                          const char *cmd,
                          ...);
static void log_opcallback(disorder_eclient *c, struct operation *op);
static void logline(disorder_eclient *c, const char *line);
static void logentry_completed(disorder_eclient *c, int nvec, char **vec);
static void logentry_failed(disorder_eclient *c, int nvec, char **vec);
static void logentry_moved(disorder_eclient *c, int nvec, char **vec);
static void logentry_playing(disorder_eclient *c, int nvec, char **vec);
static void logentry_queue(disorder_eclient *c, int nvec, char **vec);
static void logentry_recent_added(disorder_eclient *c, int nvec, char **vec);
static void logentry_recent_removed(disorder_eclient *c, int nvec, char **vec);
static void logentry_removed(disorder_eclient *c, int nvec, char **vec);
static void logentry_scratched(disorder_eclient *c, int nvec, char **vec);
static void logentry_state(disorder_eclient *c, int nvec, char **vec);
static void logentry_volume(disorder_eclient *c, int nvec, char **vec);
static void logentry_rescanned(disorder_eclient *c, int nvec, char **vec);
static void logentry_user_add(disorder_eclient *c, int nvec, char **vec);
static void logentry_user_confirm(disorder_eclient *c, int nvec, char **vec);
static void logentry_user_delete(disorder_eclient *c, int nvec, char **vec);
static void logentry_user_edit(disorder_eclient *c, int nvec, char **vec);
static void logentry_rights_changed(disorder_eclient *c, int nvec, char **vec);
static void logentry_adopted(disorder_eclient *c, int nvec, char **vec);
static void logentry_playlist_created(disorder_eclient *c, int nvec, char **vec);
static void logentry_playlist_deleted(disorder_eclient *c, int nvec, char **vec);
static void logentry_playlist_modified(disorder_eclient *c, int nvec, char **vec);
static void logentry_global_pref(disorder_eclient *c, int nvec, char **vec);

/* Tables ********************************************************************/

/** @brief One possible log entry */
struct logentry_handler {
  const char *name;                     /**< @brief Entry name */
  int min;                              /**< @brief Minimum arguments */
  int max;                              /**< @brief Maximum arguments */
  void (*handler)(disorder_eclient *c,
                  int nvec,
                  char **vec);          /**< @brief Handler function */
};

/** @brief Table for parsing log entries */
static const struct logentry_handler logentry_handlers[] = {
#define LE(X, MIN, MAX) { #X, MIN, MAX, logentry_##X }
  LE(adopted, 2, 2),
  LE(completed, 1, 1),
  LE(failed, 2, 2),
  LE(global_pref, 1, 2),
  LE(moved, 1, 1),
  LE(playing, 1, 2),
  LE(playlist_created, 2, 2),
  LE(playlist_deleted, 1, 1),
  LE(playlist_modified, 2, 2),
  LE(queue, 2, INT_MAX),
  LE(recent_added, 2, INT_MAX),
  LE(recent_removed, 1, 1),
  LE(removed, 1, 2),
  LE(rescanned, 0, 0),
  LE(rights_changed, 1, 1),
  LE(scratched, 2, 2),
  LE(state, 1, 1),
  LE(user_add, 1, 1),
  LE(user_confirm, 1, 1),
  LE(user_delete, 1, 1),
  LE(user_edit, 2, 2),
  LE(volume, 2, 2)
};

/* Setup and teardown ********************************************************/

/** @brief Create a new client
 *
 * Does NOT connect the client - connections are made (and re-made) on demand.
 */
disorder_eclient *disorder_eclient_new(const disorder_eclient_callbacks *cb,
                                       void *u) {
  disorder_eclient *c = xmalloc(sizeof *c);
  D(("disorder_eclient_new"));
  c->fd = -1;
  c->callbacks = cb;
  c->u = u;
  c->opstail = &c->ops;
  c->enabled = 1;
  vector_init(&c->vec);
  dynstr_init(&c->input);
  dynstr_init(&c->output);
  return c;
}

/** @brief Disconnect a client
 * @param c Client to disconnect
 *
 * NB that this routine just disconnnects the TCP connection.  It does not
 * destroy the client!  If you continue to use it then it will attempt to
 * reconnect.
 */
void disorder_eclient_close(disorder_eclient *c) {
  struct operation *op;

  D(("disorder_eclient_close"));
  if(c->fd != -1) {
    D(("disorder_eclient_close closing fd %d", c->fd));
    c->callbacks->poll(c->u, c, c->fd, 0);
    xclose(c->fd);
    c->fd = -1;
    c->state = state_disconnected;
    c->statebits = 0;
  }
  c->output.nvec = 0;
  c->input.nvec = 0;
  c->eof = 0;
  c->authenticated = 0;
  /* We'll need to resend all operations */
  for(op = c->ops; op; op = op->next)
    op->sent = 0;
  /* Drop our use a hint that we're disconnected */
  if(c->log_callbacks && c->log_callbacks->state)
    c->log_callbacks->state(c->log_v, c->statebits);
}

/** @brief Permit new connection activity */
void disorder_eclient_enable_connect(disorder_eclient *c) {
  c->enabled = 1;
}

/** @brief Suppress new connection activity */
void disorder_eclient_disable_connect(disorder_eclient *c) {
  c->enabled = 0;
}

/** @brief Return current state */
unsigned long disorder_eclient_state(const disorder_eclient *c) {
  return c->statebits | (c->state > state_connected ? DISORDER_CONNECTED : 0);
}

/* Error reporting ***********************************************************/

/** @brief called when a connection error occurs
 *
 * After this called we will be disconnected (by disorder_eclient_close()),
 * so there will be a reconnection before any commands can be sent.
 */
static int comms_error(disorder_eclient *c, const char *fmt, ...) {
  va_list ap;
  char *s;

  D(("comms_error"));
  va_start(ap, fmt);
  byte_xvasprintf(&s, fmt, ap);
  va_end(ap);
  disorder_eclient_close(c);
  c->callbacks->comms_error(c->u, s);
  return -1;
}

/** @brief called when the server reports an error */
static int protocol_error(disorder_eclient *c, struct operation *op,
                          int code, const char *fmt, ...) {
  va_list ap;
  char *s;

  D(("protocol_error"));
  va_start(ap, fmt);
  byte_xvasprintf(&s, fmt, ap);
  va_end(ap);
  c->callbacks->protocol_error(c->u, op->v, code, s);
  return -1;
}

/* State machine *************************************************************/

/** @brief Send an operation (into the output buffer)
 * @param op Operation to send
 */
static void op_send(struct operation *op) {
  disorder_eclient *const c = op->client;
  put(c, op->cmd, strlen(op->cmd));
  if(op->body) {
    for(int n = 0; op->body[n]; ++n) {
      if(op->body[n][0] == '.')
        put(c, ".", 1);
      put(c, op->body[n], strlen(op->body[n]));
      put(c, "\n", 1);
    }
    put(c, ".\n", 2);
  }
  op->sent = 1;
}

/** @brief Called when there's something to do
 * @param c Client
 * @param mode bitmap of @ref DISORDER_POLL_READ and/or @ref DISORDER_POLL_WRITE.
 *
 * This should be called from by your code when the file descriptor is readable
 * or writable (as requested by the @c poll callback, see @ref
 * disorder_eclient_callbacks) and in any case from time to time (with @p mode
 * = 0) to allow for retries to work.
 */
void disorder_eclient_polled(disorder_eclient *c, unsigned mode) {
  struct operation *op;
  time_t now;
  
  D(("disorder_eclient_polled fd=%d state=%s mode=[%s %s]",
     c->fd, states[c->state],
     mode & DISORDER_POLL_READ ? "READ" : "",
     mode & DISORDER_POLL_WRITE ? "WRITE" : ""));
  /* The pattern here is to check each possible state in turn and try to
   * advance (though on error we might go back).  If we advance we leave open
   * the possibility of falling through to the next state, but we set the mode
   * bits to 0, to avoid false positives (which matter more in some cases than
   * others). */

  if(c->state == state_disconnected) {
    D(("state_disconnected"));
    /* If there is no password yet then we cannot connect */
    if(!config->password) {
      comms_error(c, "no password is configured");
      c->enabled = 0;
      return;
    }
    /* Only try to connect if enabled */
    if(c->enabled)
      start_connect(c);
    /* might now be state_disconnected (on error), state_connecting (slow
     * connect) or state_connected (fast connect).  If state_disconnected then
     * we just rely on a periodic callback from the event loop sometime. */
    mode = 0;
  }

  if(c->state == state_connecting && mode) {
    D(("state_connecting"));
    maybe_connected(c);
    /* Might be state_disconnected (on error) or state_connected (on success).
     * In the former case we rely on the event loop for a periodic callback to
     * retry. */
    mode = 0;
  }

  if(c->state == state_connected) {
    D(("state_connected"));
    /* We just connected.  Initiate the authentication protocol. */
    stash_command(c, 1/*queuejump*/, authbanner_opcallback,
                  0/*completed*/, 0/*v*/, -1/*nbody*/, 0/*body*/, 0/*cmd*/);
    /* We never stay is state_connected very long.  We could in principle jump
     * straight to state_cmdresponse since there's actually no command to
     * send, but that would arguably be cheating. */
    c->state = state_idle;
  }

  if(c->state == state_idle) {
    D(("state_idle"));
    /* We are connected, and have finished any command we set off, look for
     * some work to do */
    if(c->ops) {
      D(("have ops"));
      if(c->authenticated) {
        /* Transmit all unsent operations */
        for(op = c->ops; op; op = op->next) {
          if(!op->sent)
            op_send(op);
        }
      } else {
        /* Just send the head operation */
        if(c->ops->cmd && !c->ops->sent)
          op_send(c->ops);
      }
      /* Awaiting response for the operation at the head of the list */
      c->state = state_cmdresponse;
    } else
      /* genuinely idle */
      c->callbacks->report(c->u, 0);
  }

  /* Queue up a byte to send */
  if(c->state == state_log
     && c->output.nvec == 0
     && xtime(&now) - c->last_prod > LOG_PROD_INTERVAL) {
    put(c, "x", 1);
    c->last_prod = now;
  }
  
  if(c->state == state_cmdresponse
     || c->state == state_body
     || c->state == state_log) {
    D(("state_%s", states[c->state]));
    /* We are awaiting a response */
    if(mode & DISORDER_POLL_WRITE) send_output(c);
    if(mode & DISORDER_POLL_READ) read_input(c);
    /* There are a couple of reasons we might want to re-enter the state
     * machine from the top.  state_idle is obvious: there may be further
     * commands to process.  Re-entering on state_disconnected means that we
     * immediately retry connection if a comms error occurs during a command.
     * This is different to the case where a connection fails, where we await a
     * spontaneous call to initiate the retry. */
    switch(c->state) {
    case state_disconnected:            /* lost connection */
    case state_idle:                    /* completed a command */
      D(("retrying"));
      disorder_eclient_polled(c, 0);
      return;
    default:
      break;
    }
  }
  
  /* Figure out what to set the mode to */
  switch(c->state) {
  case state_disconnected:
    D(("state_disconnected (2)"));
    /* Probably an error occurred.  Await a retry. */
    mode = 0;
    break;
  case state_connecting:
    D(("state_connecting (2)"));
    /* Waiting for connect to complete */
    mode = DISORDER_POLL_READ|DISORDER_POLL_WRITE;
    break;
  case state_connected:
    D(("state_connected (2)"));
    assert(!"should never be in state_connected here");
    break;
  case state_idle:
    D(("state_idle (2)"));
    /* Connected but nothing to do. */
    mode = 0;
    break;
  case state_cmdresponse:
  case state_body:
  case state_log:
    D(("state_%s (2)", states[c->state]));
    /* Gathering a response.  Wait for input. */
    mode = DISORDER_POLL_READ;
    /* Flush any pending output. */
    if(c->output.nvec) mode |= DISORDER_POLL_WRITE;
    break;
  }
  D(("fd=%d new mode [%s %s]",
     c->fd,
     mode & DISORDER_POLL_READ ? "READ" : "",
     mode & DISORDER_POLL_WRITE ? "WRITE" : ""));
  if(c->fd != -1) c->callbacks->poll(c->u, c, c->fd, mode);
}

/** @brief Called to start connecting */
static int start_connect(disorder_eclient *c) {
  struct sockaddr *sa;
  socklen_t len;

  D(("start_connect"));
  if((len = find_server(config, &sa, &c->ident)) == (socklen_t)-1)
    return comms_error(c, "cannot look up server"); /* TODO better error */
  if(c->fd != -1) {
    xclose(c->fd);
    c->fd = -1;
  }
  if((c->fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0)
    return comms_error(c, "socket: %s", strerror(errno));
  c->eof = 0;
  nonblock(c->fd);
  cloexec(c->fd);
  if(connect(c->fd, sa, len) < 0) {
    switch(errno) {
    case EINTR:
    case EINPROGRESS:
      c->state = state_connecting;
      /* We are called from _polled so the state machine will get to do its
       * thing */
      return 0;
    default:
      /* Signal the error to the caller. */
      return comms_error(c, "connecting to %s: %s", c->ident, strerror(errno));
    }
  } else
    c->state = state_connected;
  return 0;
}

/** @brief Called when poll triggers while waiting for a connection */
static void maybe_connected(disorder_eclient *c) {
  /* We either connected, or got an error. */
  int err;
  socklen_t len = sizeof err;
  
  D(("maybe_connected"));
  /* Work around over-enthusiastic error slippage */
  if(getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
    err = errno;
  if(err) {
    /* The connection failed */
    comms_error(c, "connecting to %s: %s", c->ident, strerror(err));
    /* sets state_disconnected */
  } else {
    char *r;
    
    /* The connection succeeded */
    c->state = state_connected;
    byte_xasprintf(&r, "connected to %s", c->ident);
    c->callbacks->report(c->u, r);
    /* If this is a log client we expect to get a bunch of updates from the
     * server straight away */
  }
}

/* Authentication ************************************************************/

/** @brief Called with the greeting from the server */
static void authbanner_opcallback(disorder_eclient *c,
                                  struct operation *op) {
  size_t nonce_len;
  const unsigned char *nonce;
  const char *res;
  char **rvec;
  int nrvec;
  const char *protocol, *algorithm, *challenge;
  
  D(("authbanner_opcallback"));
  if(c->rc / 100 != 2
     || !(rvec = split(c->line + 4, &nrvec, SPLIT_QUOTES, 0, 0))
     || nrvec < 1) {
    /* Banner told us to go away, or was malformed.  We cannot proceed. */
    protocol_error(c, op, c->rc, "%s [%s]", c->line, c->ident);
    disorder_eclient_close(c);
    return;
  }
  switch(nrvec) {
  case 1:
    protocol = "1";
    algorithm = "sha1";
    challenge = *rvec++;
    break;
  case 2:
    protocol = "1";
    algorithm = *rvec++;
    challenge = *rvec++;
    break;
  case 3:
    protocol = *rvec++;
    algorithm = *rvec++;
    challenge = *rvec++;
    break;
  default:
    protocol_error(c, op, c->rc, "%s [%s]", c->line, c->ident);
    disorder_eclient_close(c);
    return;
  }
  c->protocol = atoi(protocol);
  if(c->protocol < 1 || c->protocol > 2) {
    protocol_error(c, op, c->rc, "%s [%s]", c->line, c->ident);
    disorder_eclient_close(c);
    return;
  }
  nonce = unhex(challenge, &nonce_len);
  res = authhash(nonce, nonce_len, config->password, algorithm);
  if(!res) {
    protocol_error(c, op, c->rc, "unknown authentication algorithm '%s' [%s]",
                   algorithm, c->ident);
    disorder_eclient_close(c);
    return;
  }
  stash_command(c, 1/*queuejump*/, authuser_opcallback, 0/*completed*/, 0/*v*/,
                -1/*nbody*/, 0/*body*/,
                "user", quoteutf8(config->username), quoteutf8(res),
                (char *)0);
}

/** @brief Called with the response to the @c user command */
static void authuser_opcallback(disorder_eclient *c,
                                struct operation *op) {
  char *r;

  D(("authuser_opcallback"));
  if(c->rc / 100 != 2) {
    /* Wrong password or something.  We cannot proceed. */
    protocol_error(c, op, c->rc, "%s [%s]", c->line, c->ident);
    c->enabled = 0;
    disorder_eclient_close(c);
    return;
  }
  /* OK, we're authenticated now. */
  c->authenticated = 1;
  byte_xasprintf(&r, "authenticated with %s", c->ident);
  c->callbacks->report(c->u, r);
  if(c->log_callbacks && !(c->ops && c->ops->opcallback == log_opcallback))
    /* We are a log client, switch to logging mode */
    stash_command(c, 0/*queuejump*/, log_opcallback, 0/*completed*/, c->log_v,
                  -1/*nbody*/, 0/*body*/,
                  "log", (char *)0);
}

/* Output ********************************************************************/

/* Chop N bytes off the front of a dynstr */
static void consume(struct dynstr *d, int n) {
  D(("consume %d", n));
  assert(d->nvec >= n);
  memmove(d->vec, d->vec + n, d->nvec - n);
  d->nvec -= n;
}

/* Write some bytes */
static void put(disorder_eclient *c, const char *s, size_t n) {
  D(("put %d %.*s", c->fd, (int)n, s));
  dynstr_append_bytes(&c->output, s, n);
}

/* Called when we can write to our FD, or at any other time */
static void send_output(disorder_eclient *c) {
  int n;

  D(("send_output %d bytes pending", c->output.nvec));
  if(c->state > state_connecting && c->output.nvec) {
    n = write(c->fd, c->output.vec, c->output.nvec);
    if(n < 0) {
      switch(errno) {
      case EINTR:
      case EAGAIN:
        break;
      default:
        comms_error(c, "writing to %s: %s", c->ident, strerror(errno));
        break;
      }
    } else
      consume(&c->output, n);
  }
}

/* Input *********************************************************************/

/* Called when c->fd might be readable, or at any other time */
static void read_input(disorder_eclient *c) {
  char *nl;
  int n;
  char buffer[512];

  D(("read_input in state %s", states[c->state]));
  if(c->state <= state_connected) return; /* ignore bogus calls */
  /* read some more input */
  n = read(c->fd, buffer, sizeof buffer);
  if(n < 0) {
    switch(errno) {
    case EINTR:
    case EAGAIN:
      break;
    default:
      comms_error(c, "reading from %s: %s", c->ident, strerror(errno));
      break;
    }
    return;                             /* no new input to process */
  } else if(n) {
    D(("read %d bytes: [%.*s]", n, n, buffer));
    dynstr_append_bytes(&c->input, buffer, n);
  } else
    c->eof = 1;
  /* might have more than one line to process */
  while(c->state > state_connecting
        && (nl = memchr(c->input.vec, '\n', c->input.nvec))) {
    process_line(c, xstrndup(c->input.vec, nl - c->input.vec));
    /* we might have disconnected along the way, which zogs the input buffer */
    if(c->state > state_connecting)
      consume(&c->input, (nl - c->input.vec) + 1);
  }
  if(c->eof) {
    comms_error(c, "reading from %s: server disconnected", c->ident);
    c->authenticated = 0;
  }
}

/* called with a line that has just been read */
static void process_line(disorder_eclient *c, char *line) {
  D(("process_line %d [%s]", c->fd, line));
  switch(c->state) {
  case state_cmdresponse:
    /* This is the first line of a response */
    if(!(line[0] >= '0' && line[0] <= '9'
         && line[1] >= '0' && line[1] <= '9'
         && line[2] >= '0' && line[2] <= '9'
         && line[3] == ' '))
      disorder_fatal(0, "invalid response from server: %s", line);
    c->rc = (line[0] * 10 + line[1]) * 10 + line[2] - 111 * '0';
    c->line = line;
    switch(c->rc % 10) {
    case 3:
      /* We need to collect the body. */
      c->state = state_body;
      vector_init(&c->vec);
      break;
    case 4:
      assert(c->log_callbacks != 0);
      if(c->log_callbacks->connected)
        c->log_callbacks->connected(c->log_v);
      c->state = state_log;
      break;
    default:
      /* We've got the whole response.  Go into the idle state so the state
       * machine knows we're done and then call the operation callback. */
      complete(c);
      break;
    }
    break;
  case state_body:
    if(strcmp(line, ".")) {
      /* A line from the body */
      vector_append(&c->vec, line + (line[0] == '.'));
    } else {
      /* End of the body. */
      vector_terminate(&c->vec);
      complete(c);
    }
    break;
  case state_log:
    if(strcmp(line, ".")) {
      logline(c, line + (line[0] == '.'));
    } else 
      complete(c);
    break;
  default:
    assert(!"wrong state for location");
    break;
  }
}

/* Called when an operation completes */
static void complete(disorder_eclient *c) {
  struct operation *op;

  D(("complete"));
  /* Pop the operation off the queue */
  op = c->ops;
  c->ops = op->next;
  if(c->opstail == &op->next)
    c->opstail = &c->ops;
  /* If we've pipelined a command ahead then we go straight to cmdresponser.
   * Otherwise we go to idle, which will arrange further sends. */
  c->state = c->ops && c->ops->sent ? state_cmdresponse : state_idle;
  op->opcallback(c, op);
  /* Note that we always call the opcallback even on error, though command
   * opcallbacks generally always do the same error handling, i.e. just call
   * protocol_error().  It's the auth* opcallbacks that have different
   * behaviour. */
}

/* Operation setup ***********************************************************/

static void stash_command_vector(disorder_eclient *c,
                                 int queuejump,
                                 operation_callback *opcallback,
                                 void (*completed)(),
                                 void *v,
                                 int nbody,
                                 char **body,
                                 int ncmd,
                                 char **cmd) {
  struct operation *op = xmalloc(sizeof *op);
  struct dynstr d;
  int n;

  if(cmd) {
    dynstr_init(&d);
    for(n = 0; n < ncmd; ++n) {
      if(n)
        dynstr_append(&d, ' ');
      dynstr_append_string(&d, quoteutf8(cmd[n]));
    }
    dynstr_append(&d, '\n');
    dynstr_terminate(&d);
    op->cmd = d.vec;
  } else
    op->cmd = 0;                        /* usually, awaiting challenge */
  if(nbody >= 0) {
    op->body = xcalloc(nbody + 1, sizeof (char *));
    for(n = 0; n < nbody; ++n)
      op->body[n] = xstrdup(body[n]);
    op->body[n] = 0;
  } else
    op->body = NULL;
  op->opcallback = opcallback;
  op->completed = completed;
  op->v = v;
  op->next = 0;
  op->client = c;
  assert(op->sent == 0);
  if(queuejump) {
    /* Authentication operations jump the queue of useful commands */
    op->next = c->ops;
    c->ops = op;
    if(c->opstail == &c->ops)
      c->opstail = &op->next;
    for(op = c->ops; op; op = op->next)
      assert(!op->sent);
  } else {
    *c->opstail = op;
    c->opstail = &op->next;
  }
}

static void vstash_command(disorder_eclient *c,
                           int queuejump,
                           operation_callback *opcallback,
                           void (*completed)(),
                           void *v,
                           int nbody,
                           char **body,
                           const char *cmd, va_list ap) {
  char *arg;
  struct vector vec;

  D(("vstash_command %s", cmd ? cmd : "NULL"));
  if(cmd) {
    vector_init(&vec);
    vector_append(&vec, (char *)cmd);
    while((arg = va_arg(ap, char *))) {
      if(arg == disorder__list) {
	char **list = va_arg(ap, char **);
	int nlist = va_arg(ap, int);
	if(nlist < 0) {
	  for(nlist = 0; list[nlist]; ++nlist)
	    ;
	}
        vector_append_many(&vec, list, nlist);
      } else if(arg == disorder__body) {
	body = va_arg(ap, char **);
	nbody = va_arg(ap, int);
      } else if(arg == disorder__integer) {
        long n = va_arg(ap, long);
        char buffer[16];
        snprintf(buffer, sizeof buffer, "%ld", n);
        vector_append(&vec, xstrdup(buffer));
      } else if(arg == disorder__time) {
        time_t n = va_arg(ap, time_t);
        char buffer[16];
        snprintf(buffer, sizeof buffer, "%lld", (long long)n);
        vector_append(&vec, xstrdup(buffer));
      } else
        vector_append(&vec, arg);
    }
    stash_command_vector(c, queuejump, opcallback, completed, v, 
                         nbody, body, vec.nvec, vec.vec);
  } else
    stash_command_vector(c, queuejump, opcallback, completed, v,
                         nbody, body,
                         0, 0);
}

static void stash_command(disorder_eclient *c,
                          int queuejump,
                          operation_callback *opcallback,
                          void (*completed)(),
                          void *v,
                          int nbody,
                          char **body,
                          const char *cmd,
                          ...) {
  va_list ap;

  va_start(ap, cmd);
  vstash_command(c, queuejump, opcallback, completed, v, nbody, body, cmd, ap);
  va_end(ap);
}

/* Command support ***********************************************************/

static const char *errorstring(disorder_eclient *c) {
  char *s;

  byte_xasprintf(&s, "%s [%s]", c->line, c->ident);
  return s;
}

/* for commands with a quoted string response */ 
static void string_response_opcallback(disorder_eclient *c,
                                       struct operation *op) {
  disorder_eclient_string_response *completed
    = (disorder_eclient_string_response *)op->completed;
    
  D(("string_response_callback"));
  if(completed) {
    if(c->rc / 100 == 2 || c->rc == 555) {
      if(c->rc == 555)
        completed(op->v, NULL, NULL);
      else if(c->protocol >= 2) {
        char **rr = split(c->line + 4, 0, SPLIT_QUOTES, 0, 0);
        
        if(rr && *rr)
          completed(op->v, NULL, *rr);
        else
          /* TODO error message a is bit lame but generally indicates a server
           * bug rather than anything the user can address */
          completed(op->v, "error parsing response", NULL);
      } else
        completed(op->v, NULL, c->line + 4);
    } else
      completed(op->v, errorstring(c), NULL);
  }
}

/* for commands with a simple integer response */ 
static void integer_response_opcallback(disorder_eclient *c,
                                        struct operation *op) {
  disorder_eclient_integer_response *completed
    = (disorder_eclient_integer_response *)op->completed;

  D(("string_response_callback"));
  if(c->rc / 100 == 2) {
    long n;
    int e;

    e = xstrtol(&n, c->line + 4, 0, 10);
    if(e)
      completed(op->v, strerror(e), 0);
    else
      completed(op->v, 0, n);
  } else
    completed(op->v, errorstring(c), 0);
}

/* for commands with no response */
static void no_response_opcallback(disorder_eclient *c,
                                   struct operation *op) {
  disorder_eclient_no_response *completed
    = (disorder_eclient_no_response *)op->completed;

  D(("no_response_callback"));
  if(completed) {
    if(c->rc / 100 == 2)
      completed(op->v, NULL);
    else
      completed(op->v, errorstring(c));
  }
}

/* error callback for queue_unmarshall */
static void eclient_queue_error(const char *msg,
                                void *u) {
  struct operation *op = u;

  /* TODO don't use protocol_error here */
  protocol_error(op->client, op, -1, "error parsing queue entry: %s", msg);
}

/* for commands that expect a queue dump */
static void queue_response_opcallback(disorder_eclient *c,
                                      struct operation *op) {
  disorder_eclient_queue_response *const completed
    = (disorder_eclient_queue_response *)op->completed;
  int n;
  int parse_failed = 0;
  struct queue_entry *q, *qh = 0, **qtail = &qh, *qlast = 0;
  
  D(("queue_response_callback"));
  if(c->rc / 100 == 2) {
    /* parse the queue */
    for(n = 0; n < c->vec.nvec; ++n) {
      q = xmalloc(sizeof *q);
      D(("queue_unmarshall %s", c->vec.vec[n]));
      if(!queue_unmarshall(q, c->vec.vec[n], NULL, op)) {
        q->prev = qlast;
        *qtail = q;
        qtail = &q->next;
        qlast = q;
      } else
        parse_failed = 1;
    }
    /* Currently we pass the partial queue to the callback along with the
     * error.  This might not be very useful in practice... */
    if(parse_failed)
      completed(op->v, "cannot parse result", qh);
    else
      completed(op->v, 0, qh);
  } else
    completed(op->v, errorstring(c), 0);
} 

/* for 'playing' */
static void playing_response_opcallback(disorder_eclient *c,
                                        struct operation *op) {
  disorder_eclient_queue_response *const completed
    = (disorder_eclient_queue_response *)op->completed;
  struct queue_entry *q;

  D(("playing_response_callback"));
  if(c->rc / 100 == 2) {
    switch(c->rc % 10) {
    case 2:
      if(queue_unmarshall(q = xmalloc(sizeof *q), c->line + 4,
                          NULL, c))
        completed(op->v, "cannot parse result", 0);
      else
        completed(op->v, 0, q);
      break;
    case 9:
      completed(op->v, 0, 0);
      break;
    default:
      completed(op->v, errorstring(c), 0);
      break;
    }
  } else
    completed(op->v, errorstring(c), 0);
}

/* for commands that expect a list of some sort */
static void list_response_opcallback(disorder_eclient *c,
                                     struct operation *op) {
  disorder_eclient_list_response *const completed =
    (disorder_eclient_list_response *)op->completed;

  D(("list_response_callback"));
  if(c->rc / 100 == 2)
    completed(op->v, NULL, c->vec.nvec, c->vec.vec);
  else if(c->rc == 555)
    completed(op->v, NULL, -1, NULL);
  else
    completed(op->v, errorstring(c), 0, 0);
}

/* for volume */
static void pair_integer_response_opcallback(disorder_eclient *c,
                                             struct operation *op) {
  disorder_eclient_pair_integer_response *completed
    = (disorder_eclient_pair_integer_response *)op->completed;
  long l, r;

  D(("volume_response_callback"));
  if(c->rc / 100 == 2) {
    if(op->completed) {
      if(sscanf(c->line + 4, "%ld %ld", &l, &r) != 2 || l < 0 || r < 0)
        completed(op->v, "cannot parse volume response", 0, 0);
      else
        completed(op->v, 0, l, r);
    }
  } else
    completed(op->v, errorstring(c), 0, 0);
}

static int simple(disorder_eclient *c,
                  operation_callback *opcallback,
                  void (*completed)(),
                  void *v,
                  const char *cmd, ...) {
  va_list ap;

  va_start(ap, cmd);
  vstash_command(c, 0/*queuejump*/, opcallback, completed, v, -1, 0, cmd, ap);
  va_end(ap);
  /* Give the state machine a kick, since we might be in state_idle */
  disorder_eclient_polled(c, 0);
  return 0;
}

/* Commands ******************************************************************/

int disorder_eclient_scratch_playing(disorder_eclient *c,
                                     disorder_eclient_no_response *completed,
                                     void *v) {
  return disorder_eclient_scratch(c, completed, 0, v);
}

static void rtp_response_opcallback(disorder_eclient *c,
                                    struct operation *op) {
  disorder_eclient_list_response *const completed =
    (disorder_eclient_list_response *)op->completed;
  D(("rtp_response_opcallback"));
  if(c->rc / 100 == 2) {
    int nvec;
    char **vec = split(c->line + 4, &nvec, SPLIT_QUOTES, 0, 0);

    if(vec)
      completed(op->v, NULL, nvec, vec);
    else
      completed(op->v, "error parsing response", 0, 0);
  } else
    completed(op->v, errorstring(c), 0, 0);
}

/** @brief Determine the RTP target address
 * @param c Client
 * @param completed Called with address details
 * @param v Passed to @p completed
 *
 * The address details will be two elements, the first being the hostname and
 * the second the service (port).
 */
int disorder_eclient_rtp_address(disorder_eclient *c,
                                 disorder_eclient_list_response *completed,
                                 void *v) {
  return simple(c, rtp_response_opcallback, (void (*)())completed, v,
                "rtp-address", (char *)0);
}

/* Log clients ***************************************************************/

/** @brief Monitor the server log
 * @param c Client
 * @param callbacks Functions to call when anything happens
 * @param v Passed to @p callbacks functions
 *
 * Once a client is being used for logging it cannot be used for anything else.
 * There is magic in authuser_opcallback() to re-submit the @c log command
 * after reconnection.
 *
 * NB that the @c state callback may be called from within this function,
 * i.e. not solely later on from the event loop callback.
 */
int disorder_eclient_log(disorder_eclient *c,
                         const disorder_eclient_log_callbacks *callbacks,
                         void *v) {
  if(c->log_callbacks) return -1;
  c->log_callbacks = callbacks;
  c->log_v = v;
  /* Repoort initial state */
  if(c->log_callbacks->state)
    c->log_callbacks->state(c->log_v, c->statebits);
  stash_command(c, 0/*queuejump*/, log_opcallback, 0/*completed*/, v,
                -1, 0, "log", (char *)0);
  disorder_eclient_polled(c, 0);
  return 0;
}

/* If we get here we've stopped being a log client */
static void log_opcallback(disorder_eclient *c,
                           struct operation attribute((unused)) *op) {
  D(("log_opcallback"));
  c->log_callbacks = 0;
  c->log_v = 0;
}

/* error callback for log line parsing */
static void logline_error(const char *msg, void *u) {
  disorder_eclient *c = u;
  /* TODO don't use protocol_error here */
  protocol_error(c, c->ops, -1, "error parsing log line: %s", msg);
}

/* process a single log line */
static void logline(disorder_eclient *c, const char *line) {
  int nvec, n;
  char **vec;
  uintmax_t when;

  D(("logline [%s]", line));
  vec = split(line, &nvec, SPLIT_QUOTES, logline_error, c);
  if(nvec < 2) return;                  /* probably an error, already
                                         * reported */
  if(sscanf(vec[0], "%"SCNxMAX, &when) != 1) {
    /* probably the wrong side of a format change */
    /* TODO don't use protocol_error here */
    protocol_error(c, c->ops, -1, "invalid log timestamp '%s'", vec[0]);
    return;
  }
  /* TODO: do something with the time */
  //fprintf(stderr, "log key: %s\n", vec[1]);
  n = TABLE_FIND(logentry_handlers, name, vec[1]);
  if(n < 0) {
    //fprintf(stderr, "...not found\n");
    return;                     /* probably a future command */
  }
  vec += 2;
  nvec -= 2;
  if(nvec < logentry_handlers[n].min || nvec > logentry_handlers[n].max) {
    //fprintf(stderr, "...wrong # args\n");
    return;
  }
  logentry_handlers[n].handler(c, nvec, vec);
}

static void logentry_completed(disorder_eclient *c,
                               int attribute((unused)) nvec, char **vec) {
  c->statebits &= ~DISORDER_PLAYING;
  if(c->log_callbacks->completed)
    c->log_callbacks->completed(c->log_v, vec[0]);
  if(c->log_callbacks->state)
    c->log_callbacks->state(c->log_v, c->statebits | DISORDER_CONNECTED);
}

static void logentry_failed(disorder_eclient *c,
                            int attribute((unused)) nvec, char **vec) {
  c->statebits &= ~DISORDER_PLAYING;
  if(c->log_callbacks->failed)
    c->log_callbacks->failed(c->log_v, vec[0], vec[1]);
  if(c->log_callbacks->state)
    c->log_callbacks->state(c->log_v, c->statebits | DISORDER_CONNECTED);
}

static void logentry_moved(disorder_eclient *c,
                           int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->moved)
    c->log_callbacks->moved(c->log_v, vec[0]);
}

static void logentry_playing(disorder_eclient *c,
                             int attribute((unused)) nvec, char **vec) {
  c->statebits |= DISORDER_PLAYING;
  if(c->log_callbacks->playing)
    c->log_callbacks->playing(c->log_v, vec[0], vec[1]);
  if(c->log_callbacks->state)
    c->log_callbacks->state(c->log_v, c->statebits | DISORDER_CONNECTED);
}

static void logentry_queue(disorder_eclient *c,
                           int attribute((unused)) nvec, char **vec) {
  struct queue_entry *q;

  if(!c->log_callbacks->queue) return;
  q = xmalloc(sizeof *q);
  if(queue_unmarshall_vec(q, nvec, vec, eclient_queue_error, c))
    return;                             /* bogus */
  c->log_callbacks->queue(c->log_v, q);
}

static void logentry_recent_added(disorder_eclient *c,
                                  int attribute((unused)) nvec, char **vec) {
  struct queue_entry *q;

  if(!c->log_callbacks->recent_added) return;
  q = xmalloc(sizeof *q);
  if(queue_unmarshall_vec(q, nvec, vec, eclient_queue_error, c))
    return;                           /* bogus */
  c->log_callbacks->recent_added(c->log_v, q);
}

static void logentry_recent_removed(disorder_eclient *c,
                                    int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->recent_removed)
    c->log_callbacks->recent_removed(c->log_v, vec[0]);
}

static void logentry_removed(disorder_eclient *c,
                             int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->removed)
    c->log_callbacks->removed(c->log_v, vec[0], vec[1]);
}

static void logentry_rescanned(disorder_eclient *c,
                               int attribute((unused)) nvec,
                               char attribute((unused)) **vec) {
  if(c->log_callbacks->rescanned)
    c->log_callbacks->rescanned(c->log_v);
}

static void logentry_scratched(disorder_eclient *c,
                               int attribute((unused)) nvec, char **vec) {
  c->statebits &= ~DISORDER_PLAYING;
  if(c->log_callbacks->scratched)
    c->log_callbacks->scratched(c->log_v, vec[0], vec[1]);
  if(c->log_callbacks->state)
    c->log_callbacks->state(c->log_v, c->statebits | DISORDER_CONNECTED);
}

static void logentry_user_add(disorder_eclient *c,
                              int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->user_add)
    c->log_callbacks->user_add(c->log_v, vec[0]);
}

static void logentry_user_confirm(disorder_eclient *c,
                              int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->user_confirm)
    c->log_callbacks->user_confirm(c->log_v, vec[0]);
}

static void logentry_user_delete(disorder_eclient *c,
                              int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->user_delete)
    c->log_callbacks->user_delete(c->log_v, vec[0]);
}

static void logentry_user_edit(disorder_eclient *c,
                              int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->user_edit)
    c->log_callbacks->user_edit(c->log_v, vec[0], vec[1]);
}

static void logentry_rights_changed(disorder_eclient *c,
                                    int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->rights_changed) {
    rights_type r;
    if(!parse_rights(vec[0], &r, 0/*report*/))
      c->log_callbacks->rights_changed(c->log_v, r);
  }
}

static void logentry_playlist_created(disorder_eclient *c,
                                      int attribute((unused)) nvec,
                                      char **vec) {
  if(c->log_callbacks->playlist_created)
    c->log_callbacks->playlist_created(c->log_v, vec[0], vec[1]);
}

static void logentry_playlist_deleted(disorder_eclient *c,
                                      int attribute((unused)) nvec,
                                      char **vec) {
  if(c->log_callbacks->playlist_deleted)
    c->log_callbacks->playlist_deleted(c->log_v, vec[0]);
}

static void logentry_playlist_modified(disorder_eclient *c,
                                      int attribute((unused)) nvec,
                                      char **vec) {
  if(c->log_callbacks->playlist_modified)
    c->log_callbacks->playlist_modified(c->log_v, vec[0], vec[1]);
}

static const struct {
  unsigned long bit;
  const char *enable;
  const char *disable;
} statestrings[] = {
  { DISORDER_PLAYING_ENABLED, "enable_play", "disable_play" },
  { DISORDER_RANDOM_ENABLED, "enable_random", "disable_random" },
  { DISORDER_TRACK_PAUSED, "pause", "resume" },
  { DISORDER_PLAYING, "playing", "completed" },
  { DISORDER_PLAYING, 0, "scratched" },
  { DISORDER_PLAYING, 0, "failed" },
};
#define NSTATES (int)(sizeof statestrings / sizeof *statestrings)

static void logentry_state(disorder_eclient *c,
                           int attribute((unused)) nvec, char **vec) {
  int n;

  for(n = 0; n < NSTATES; ++n)
    if(statestrings[n].enable && !strcmp(vec[0], statestrings[n].enable)) {
      c->statebits |= statestrings[n].bit;
      break;
    } else if(statestrings[n].disable && !strcmp(vec[0], statestrings[n].disable)) {
      c->statebits &= ~statestrings[n].bit;
      break;
    }
  if(c->log_callbacks->state) 
    c->log_callbacks->state(c->log_v, c->statebits | DISORDER_CONNECTED);
}

static void logentry_volume(disorder_eclient *c,
                            int attribute((unused)) nvec, char **vec) {
  long l, r;

  if(!c->log_callbacks->volume) return;
  if(xstrtol(&l, vec[0], 0, 10)
     || xstrtol(&r, vec[1], 0, 10)
     || l < 0 || l > INT_MAX
     || r < 0 || r > INT_MAX)
    return;                             /* bogus */
  c->log_callbacks->volume(c->log_v, (int)l, (int)r);
}

/** @brief Convert @p statebits to a string */
char *disorder_eclient_interpret_state(unsigned long statebits) {
  struct dynstr d[1];
  size_t n;

  static const struct {
    unsigned long bit;
    const char *name;
  } bits[] = {
    { DISORDER_PLAYING_ENABLED, "playing_enabled" },
    { DISORDER_RANDOM_ENABLED, "random_enabled" },
    { DISORDER_TRACK_PAUSED, "track_paused" },
    { DISORDER_PLAYING, "playing" },
    { DISORDER_CONNECTED, "connected" },
  };
#define NBITS (sizeof bits / sizeof *bits)

  dynstr_init(d);
  if(!statebits)
    dynstr_append(d, '0');
  for(n = 0; n < NBITS; ++n)
    if(statebits & bits[n].bit) {
      if(d->nvec)
        dynstr_append(d, '|');
      dynstr_append_string(d, bits[n].name);
      statebits ^= bits[n].bit;
    }
  if(statebits) {
    char s[20];

    if(d->nvec)
      dynstr_append(d, '|');
    sprintf(s, "%#lx", statebits);
    dynstr_append_string(d, s);
  }
  dynstr_terminate(d);
  return d->vec;
}

static void logentry_adopted(disorder_eclient *c,
                             int attribute((unused)) nvec, char **vec) {
  if(c->log_callbacks->adopted) 
    c->log_callbacks->adopted(c->log_v, vec[0], vec[1]);
}

static void logentry_global_pref(disorder_eclient *c,
                                 int nvec, char **vec) {
  if(c->log_callbacks->global_pref) 
    c->log_callbacks->global_pref(c->log_v, vec[0], nvec > 1 ? vec[1] : 0);
}

#include "eclient-stubs.c"

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
