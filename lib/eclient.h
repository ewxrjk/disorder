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
/** @file lib/eclient.h
 * @brief Client code for event-driven programs
 *
 * See @ref lib/eclient-stubs.h for the (generated) per-command entry points.
 */

#ifndef ECLIENT_H
#define ECLIENT_H

#include "rights.h"

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
  /** @brief Called when a communication error occurs.
   * @param u from disorder_eclient_new()
   * @param msg error message
   *
   * This might be called at any time, and indicates a low-level error,
   * e.g. connection refused by the server.  It does not mean that any requests
   * made of the owning eclient will not be fulfilled at some point.
   */
  void (*comms_error)(void *u, const char *msg);
  
  /** @brief Called when a command fails (including initial authorization).
   * @param u from disorder_eclient_new()
   * @param v from failed command, or NULL if during setup
   * @param msg error message
   *
   * This call is obsolete at least in its current form, in which it is used to
   * report most errors from most requests.  Ultimately requests-specific
   * errors will be reported in a request-specific way rather than via this
   * generic callback.
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

  /** @brief Called when @p track finished playing successfully */
  void (*completed)(void *v, const char *track);

  /** @brief Called when @p track fails for some reason */
  void (*failed)(void *v, const char *track, const char *status);

  /** @brief Called when @p user moves some track or tracks in the queue
   *
   * Fetch the queue again to find out what the new order is - the
   * rearrangement could in principle be arbitrarily complicated.
   */
  void (*moved)(void *v, const char *user);

  /** @brief Called when @p track starts playing
   *
   * @p user might be 0.
   */
  void (*playing)(void *v, const char *track, const char *user/*maybe 0*/);

  /** @brief Called when @p q is added to the queue
   *
   * Fetch the queue again to find out where the in the queue it was added.
   */   
  void (*queue)(void *v, struct queue_entry *q);

  /** @brief Called when @p q is added to the recent list */
  void (*recent_added)(void *v, struct queue_entry *q);

  /** @brief Called when @p id is removed from the recent list */
  void (*recent_removed)(void *v, const char *id);

  /** @brief Called when @p id is removed from the queue
   *
   * @p user might be 0.
   */
  void (*removed)(void *v, const char *id, const char *user/*maybe 0*/);

  /** @brief Called when @p track is scratched */
  void (*scratched)(void *v, const char *track, const char *user);

  /** @brief Called with the current state whenever it changes
   *
   * State bits are:
   * - @ref DISORDER_PLAYING_ENABLED
   * - @ref DISORDER_RANDOM_ENABLED
   * - @ref DISORDER_TRACK_PAUSED
   * - @ref DISORDER_PLAYING
   * - @ref DISORDER_CONNECTED
   */
  void (*state)(void *v, unsigned long state);

  /** @brief Called when the volume changes */
  void (*volume)(void *v, int left, int right);

  /** @brief Called when a rescan completes */
  void (*rescanned)(void *v);

  /** @brief Called when a user is created (admins only) */
  void (*user_add)(void *v, const char *user);

  /** @brief Called when a user is confirmed (admins only) */
  void (*user_confirm)(void *v, const char *user);

  /** @brief Called when a user is deleted (admins only) */
  void (*user_delete)(void *v, const char *user);

  /** @brief Called when a user is edited (admins only) */
  void (*user_edit)(void *v, const char *user, const char *property);

  /** @brief Called when your rights change */
  void (*rights_changed)(void *v, rights_type new_rights);

  /** @brief Called when a track is adopted */
  void (*adopted)(void *v, const char *id, const char *who);

  /** @brief Called when a new playlist is created */
  void (*playlist_created)(void *v, const char *playlist, const char *sharing);

  /** @brief Called when a playlist is modified */
  void (*playlist_modified)(void *v, const char *playlist, const char *sharing);

  /** @brief Called when a new playlist is deleted */
  void (*playlist_deleted)(void *v, const char *playlist);

  /** @brief Called when a global pref is changed or delete */
  void (*global_pref)(void *v, const char *pref, const char *value/*or NULL*/);
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
 * Unlike in earlier releases, these are not allowed to be NULL. */

/** @brief Trivial completion callback
 * @param v User data
 * @param err Error string or NULL on succes
 */
typedef void disorder_eclient_no_response(void *v,
                                          const char *err);

/** @brief String result completion callback
 * @param v User data
 * @param err Error string or NULL on succes
 * @param value Result or NULL
 *
 * @p error will be NULL on success.  In this case @p value will be the result
 * (which might be NULL for disorder_eclient_get(),
 * disorder_eclient_get_global(), disorder_eclient_userinfo() and
 * disorder_eclient_playlist_get_share()).
 *
 * @p error will be non-NULL on failure.  In this case @p value is always NULL.
 */
typedef void disorder_eclient_string_response(void *v,
                                              const char *err,
                                              const char *value);

/** @brief Integer result completion callback
 * @param v User data
 * @param err Error string or NULL on succes
 * @param value Result or 0
 *
 * @p error will be NULL on success.  In this case @p value will be the result.
 *
 * @p error will be non-NULL on failure.  In this case @p value is always 0.
 */
typedef void disorder_eclient_integer_response(void *v,
                                               const char *err,
                                               long value);
/** @brief Volume completion callback
 * @param v User data
 * @param err Error string or NULL on success
 * @param l Left channel volume
 * @param r Right channel volume
 *
 * @p error will be NULL on success.  In this case @p l and @p r will be the
 * result.
 *
 * @p error will be non-NULL on failure.  In this case @p l and @p r are always
 * 0.
 */
typedef void disorder_eclient_pair_integer_response(void *v,
                                                    const char *err,
                                                    long l, long r);

/** @brief Queue request completion callback
 * @param v User data
 * @param err Error string or NULL on success
 * @param q Head of queue data list
 *
 * @p error will be NULL on success.  In this case @p q will be the (head of
 * the) result.
 *
 * @p error will be non-NULL on failure.  In this case @p q may be NULL but
 * MIGHT also be some subset of the queue.  For consistent behavior it should
 * be ignored in the error case.
 */
typedef void disorder_eclient_queue_response(void *v,
                                             const char *err,
                                             struct queue_entry *q);

#define disorder_eclient_playing_response disorder_eclient_queue_response

/** @brief List request completion callback
 * @param v User data
 * @param err Error string or NULL on success
 * @param nvec Number of elements in response list
 * @param vec Pointer to response list
 *
 * @p error will be NULL on success.  In this case @p nvec and @p vec will give
 * the result, or be -1 and NULL respectively e.g. from
 * disorder_eclient_playlist_get() if there is no such playlist.
 *
 * @p error will be non-NULL on failure.  In this case @p nvec and @p vec will
 * be 0 and NULL.
 */
typedef void disorder_eclient_list_response(void *v,
                                            const char *err,
                                            int nvec, char **vec);

disorder_eclient *disorder_eclient_new(const disorder_eclient_callbacks *cb,
                                       void *u);
/* Create a new client */

void disorder_eclient_close(disorder_eclient *c);
/* Close C */

unsigned long disorder_eclient_state(const disorder_eclient *c);

void disorder_eclient_polled(disorder_eclient *c, unsigned mode);
/* Should be called when c's FD is readable and/or writable, and in any case
 * from time to time (so that retries work). */

int disorder_eclient_scratch_playing(disorder_eclient *c,
                                     disorder_eclient_no_response *completed,
                                     void *v);
/* scratch the playing track whatever it is */

int disorder_eclient_log(disorder_eclient *c,
                         const disorder_eclient_log_callbacks *callbacks,
                         void *v);
/* Make this a log client (forever - it automatically becomes one again upon
 * reconnection) */

int disorder_eclient_rtp_address(disorder_eclient *c,
                                 disorder_eclient_list_response *completed,
                                 void *v);

void disorder_eclient_enable_connect(disorder_eclient *c);
void disorder_eclient_disable_connect(disorder_eclient *c);

#include "eclient-stubs.h"

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
