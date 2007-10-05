/*
 * This file is part of DisOrder.
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file clients/playrtp.h
 * @brief RTP player
 */

#ifndef PLAYRTP_H
#define PLAYRTP_H

/** @brief Maximum samples per packet we'll support
 *
 * NB that two channels = two samples in this program.
 */
#define MAXSAMPLES 2048

/** @brief Number of samples to infill by in one go
 *
 * This is an upper bound - in practice we expect the underlying audio API to
 * only ask for a much smaller number of samples in any one go.
 */
#define INFILL_SAMPLES (44100 * 2)      /* 1s */

/** @brief Received packet
 *
 * Received packets are kept in a binary heap (see @ref pheap) ordered by
 * timestamp.
 */
struct packet {
  /** @brief Next packet in @ref next_free_packet or @ref received_packets */
  struct packet *next;
  
  /** @brief Number of samples in this packet */
  uint32_t nsamples;

  /** @brief Timestamp from RTP packet
   *
   * NB that "timestamps" are really sample counters.  Use lt() or lt_packet()
   * to compare timestamps. 
   */
  uint32_t timestamp;

  /** @brief Flags
   *
   * Valid values are:
   * - @ref IDLE - the idle bit was set in the RTP packet
   */
  unsigned flags;
/** @brief idle bit set in RTP packet*/
#define IDLE 0x0001

  /** @brief Raw sample data
   *
   * Only the first @p nsamples samples are defined; the rest is uninitialized
   * data.
   */
  uint16_t samples_raw[MAXSAMPLES];
};

/** @brief Structure of free packet list */
union free_packet {
  struct packet p;
  union free_packet *next;
};

/** @brief Return true iff \f$a < b\f$ in sequence-space arithmetic
 *
 * Specifically it returns true if \f$(a-b) mod 2^{32} < 2^{31}\f$.
 *
 * See also lt_packet().
 */
static inline int lt(uint32_t a, uint32_t b) {
  return (uint32_t)(a - b) & 0x80000000;
}

/** @brief Return true iff a >= b in sequence-space arithmetic */
static inline int ge(uint32_t a, uint32_t b) {
  return !lt(a, b);
}

/** @brief Return true iff a > b in sequence-space arithmetic */
static inline int gt(uint32_t a, uint32_t b) {
  return lt(b, a);
}

/** @brief Return true iff a <= b in sequence-space arithmetic */
static inline int le(uint32_t a, uint32_t b) {
  return !lt(b, a);
}

/** @brief Ordering for packets, used by @ref pheap */
static inline int lt_packet(const struct packet *a, const struct packet *b) {
  return lt(a->timestamp, b->timestamp);
}

/** @struct pheap 
 * @brief Binary heap of packets ordered by timestamp */
HEAP_TYPE(pheap, struct packet *, lt_packet);

struct packet *new_packet(void);
void free_packet(struct packet *p);

extern const char *device;
extern struct packet *received_packets;
extern struct packet **received_tail;
extern pthread_mutex_t receive_lock;
extern pthread_cond_t receive_cond;
extern uint32_t nreceived;
extern struct pheap packets;
extern volatile uint32_t nsamples;
extern uint32_t next_timestamp;
extern int active;
extern pthread_mutex_t lock;
extern pthread_cond_t cond;


#endif /* PLAYRTP_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
