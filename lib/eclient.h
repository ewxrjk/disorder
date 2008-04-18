/*
 * This file is part of DisOrder.
 * Copyright (C) 2006, 2007 Richard Kettlewell
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
/** @file lib/eclient.h
 * @brief Client code for event-driven programs
 */

#ifndef ECLIENT_H
#define ECLIENT_H

/* Asynchronous client interface */

/** @brief Handle type */
typedef struct disorder_eclient disorder_eclient;

struct queue_entry;

/** @brief Set to read from the FD */
#define DISORDER_POLL_READ 1u

/** @brief Set to write to the FD */
#define DISORDER_POLL_WRITE 2u

/** @brief Callbacks for all clients
 *
 * These must all be valid.
 */
typedef struct disorder_eclient_callbacks {
  /** @brief Called when a communication error (e.g. connected refused) occurs.
   * @param u from disorder_eclient_new()
   * @param msg error message
   */
  void (*comms_error)(void *u, const char *msg);
  
  /** @brief Called when a command fails (including initial authorization).
   * @param u from disorder_eclient_new()
   * @param v from failed command, or NULL if during setup
   * @param msg error message
   */
  void (*protocol_error)(void *u, void *v, int code, const char *msg);

  /** @brief Set poll/select flags
   * @param u from disorder_eclient_new()
   * @param c handle
   * @param fd file descriptor
   * @param mode bitmap (@ref DISORDER_POLL_READ and/or @ref DISORDER_POLL_WRITE)
   *
   * Before @p fd is closed you will always get a call with @p mode = 0.
   */
  void (*poll)(void *u, disorder_eclient *c, int fd, unsigned mode);

  /** @brief Report current activity
   * @param u from disorder_eclient_new()
   * @param msg Current activity or NULL
   *
   * Called with @p msg = NULL when there's nothing going on.
   */
  void (*report)(void *u, const char *msg);
} disorder_eclient_callbacks;

/** @brief Callbacks for log clients
 *
 * All of these are allowed to be a null pointers in which case you don't get
 * told about that log event.
 *
 * See disorder_protocol(5) for full documentation.
 */
typedef struct disorder_eclient_log_callbacks {
  /** @brief Called on (re-)connection */
  void (*connected)(void *v);
  
  void (*completed)(void *v, const char *track);
  void (*failed)(void *v, const char *track, const char *status);
  void (*moved)(void *v, const char *user);
  void (*playing)(void *v, const char *track, const char *user/*maybe 0*/);
  void (*queue)(void *v, struct queue_entry *q);
  void (*recent_added)(void *v, struct queue_entry *q);
  void (*recent_removed)(void *v, const char *id);
  void (*removed)(void *v, const char *id, const char *user/*maybe 0*/);
  void (*scratched)(void *v, const char *track, const char *user);
  void (*state)(void *v, unsigned long state);
  void (*volume)(void *v, int left, int right);
  void (*rescanned)(void *v);
} disorder_eclient_log_callbacks;

/* State bits */

/** @brief Play is enabled */
#define DISORDER_PLAYING_ENABLED  0x00000001

/** @brief Random play is enabled */
#define DISORDER_RANDOM_ENABLED   0x00000002

/** @brief Track is paused
 *
 * This is only meaningful if @ref DISORDER_PLAYING is set
 */
#define DISORDER_TRACK_PAUSED     0x00000004

/** @brief Track is playing
 *
 * This can be set even if the current track is paused (in which case @ref
 * DISORDER_TRACK_PAUSED) will also be set.
 */
#define DISORDER_PLAYING    0x00000008

/** @brief Connected to server
 *
 * By connected it is meant that commands have a reasonable chance of being
 * processed soon, not merely that a TCP connection exists - for instance if
 * the client is still authenticating then that does not count as connected.
 */
#define DISORDER_CONNECTED        0x00000010

char *disorder_eclient_interpret_state(unsigned long statebits);

struct queue_entry;
struct kvp;
struct sink;

/* Completion callbacks.  These provide the result of operations to the caller.
 * It is always allowed for these to be null pointers if you don't care about
 * the result. */

typedef void disorder_eclient_no_response(void *v);
/* completion callback with no data */

/** @brief String result completion callback
 * @param v User data
 * @param value or NULL
 *
 * @p value can be NULL for disorder_eclient_get(),
 * disorder_eclient_get_global() and disorder_eclient_userinfo().
 */
typedef void disorder_eclient_string_response(void *v, const char *value);

typedef void disorder_eclient_integer_response(void *v, long value);
/* completion callback with a integer result */

typedef void disorder_eclient_volume_response(void *v, int l, int r);
/* completion callback with a pair of integer results */

typedef void disorder_eclient_queue_response(void *v, struct queue_entry *q);
/* completion callback for queue/recent listing */

typedef void disorder_eclient_list_response(void *v, int nvec, char **vec);
/* completion callback for file listing etc */

disorder_eclient *disorder_eclient_new(const disorder_eclient_callbacks *cb,
                                       void *u);
/* Create a new client */

void disorder_eclient_close(disorder_eclient *c);
/* Close C */

unsigned long disorder_eclient_state(const disorder_eclient *c);

void disorder_eclient_polled(disorder_eclient *c, unsigned mode);
/* Should be called when c's FD is readable and/or writable, and in any case
 * from time to time (so that retries work). */

int disorder_eclient_version(disorder_eclient *c,
                             disorder_eclient_string_response *completed,
                             void *v);
/* fetch the server version */

int disorder_eclient_play(disorder_eclient *c,
                          const char *track,
                          disorder_eclient_no_response *completed,
                          void *v);
/* add a track to the queue */

int disorder_eclient_pause(disorder_eclient *c,
                           disorder_eclient_no_response *completed,
                           void *v);
/* add a track to the queue */

int disorder_eclient_resume(disorder_eclient *c,
                            disorder_eclient_no_response *completed,
                            void *v);
/* add a track to the queue */

int disorder_eclient_scratch(disorder_eclient *c,
                             const char *id,
                             disorder_eclient_no_response *completed,
                             void *v);
/* scratch a track by ID */

int disorder_eclient_scratch_playing(disorder_eclient *c,
                                     disorder_eclient_no_response *completed,
                                     void *v);
/* scratch the playing track whatever it is */

int disorder_eclient_remove(disorder_eclient *c,
                            const char *id,
                            disorder_eclient_no_response *completed,
                            void *v);
/* remove a track from the queue */

int disorder_eclient_moveafter(disorder_eclient *c,
                               const char *target,
                               int nids,
                               const char **ids,
                               disorder_eclient_no_response *completed,
                               void *v);
/* move tracks within the queue */

int disorder_eclient_playing(disorder_eclient *c,
                             disorder_eclient_queue_response *completed,
                             void *v);
/* find the currently playing track (0 for none) */

int disorder_eclient_queue(disorder_eclient *c,
                           disorder_eclient_queue_response *completed,
                           void *v);
/* list recently played tracks */

int disorder_eclient_recent(disorder_eclient *c,
                            disorder_eclient_queue_response *completed,
                            void *v);
/* list recently played tracks */

int disorder_eclient_files(disorder_eclient *c,
                           disorder_eclient_list_response *completed,
                           const char *dir,
                           const char *re,
                           void *v);
/* list files in a directory, matching RE if not a null pointer */

int disorder_eclient_dirs(disorder_eclient *c,
                          disorder_eclient_list_response *completed,
                          const char *dir,
                          const char *re,
                          void *v);
/* list directories in a directory, matching RE if not a null pointer */

int disorder_eclient_namepart(disorder_eclient *c,
                              disorder_eclient_string_response *completed,
                              const char *track,
                              const char *context,
                              const char *part,
                              void *v);
/* look up a track name part */

int disorder_eclient_length(disorder_eclient *c,
                            disorder_eclient_integer_response *completed,
                            const char *track,
                            void *v);
/* look up a track name length */

int disorder_eclient_volume(disorder_eclient *c,
                            disorder_eclient_volume_response *callback,
                            int l, int r,
                            void *v);
/* If L and R are both -ve gets the volume.
 * If neither are -ve then sets the volume.
 * Otherwise asserts!
 */

int disorder_eclient_enable(disorder_eclient *c,
                            disorder_eclient_no_response *callback,
                            void *v);
int disorder_eclient_disable(disorder_eclient *c,
                             disorder_eclient_no_response *callback,
                             void *v);
int disorder_eclient_random_enable(disorder_eclient *c,
                                   disorder_eclient_no_response *callback,
                                   void *v);
int disorder_eclient_random_disable(disorder_eclient *c,
                                    disorder_eclient_no_response *callback,
                                    void *v);
/* Enable/disable play/random play */

int disorder_eclient_resolve(disorder_eclient *c,
                             disorder_eclient_string_response *completed,
                             const char *track,
                             void *v);
/* Resolve aliases */

int disorder_eclient_log(disorder_eclient *c,
                         const disorder_eclient_log_callbacks *callbacks,
                         void *v);
/* Make this a log client (forever - it automatically becomes one again upon
 * reconnection) */

int disorder_eclient_get(disorder_eclient *c,
                         disorder_eclient_string_response *completed,
                         const char *track, const char *pref,
                         void *v);
int disorder_eclient_set(disorder_eclient *c,
                         disorder_eclient_no_response *completed,
                         const char *track, const char *pref, 
                         const char *value,
                         void *v);
int disorder_eclient_unset(disorder_eclient *c,
                           disorder_eclient_no_response *completed,
                           const char *track, const char *pref, 
                           void *v);
/* Get/set preference values */

int disorder_eclient_search(disorder_eclient *c,
                            disorder_eclient_list_response *completed,
                            const char *terms,
                            void *v);

int disorder_eclient_nop(disorder_eclient *c,
                         disorder_eclient_no_response *completed,
                         void *v);

int disorder_eclient_new_tracks(disorder_eclient *c,
                                disorder_eclient_list_response *completed,
                                int max,
                                void *v);

int disorder_eclient_rtp_address(disorder_eclient *c,
                                 disorder_eclient_list_response *completed,
                                 void *v);

int disorder_eclient_users(disorder_eclient *c,
                           disorder_eclient_list_response *completed,
                           void *v);
int disorder_eclient_deluser(disorder_eclient *c,
                             disorder_eclient_no_response *completed,
                             const char *user,
                             void *v);
int disorder_eclient_userinfo(disorder_eclient *c,
                              disorder_eclient_string_response *completed,
                              const char *user,
                              const char *property,
                              void *v);
int disorder_eclient_edituser(disorder_eclient *c,
                              disorder_eclient_no_response *completed,
                              const char *user,
                              const char *property,
                              const char *value,
                              void *v);

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
