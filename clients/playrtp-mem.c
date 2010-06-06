/*
 * This file is part of DisOrder.
 * Copyright (C) 2007 Richard Kettlewell
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
/** @file clients/playrtp-mem.c
 * @brief RTP player memory management
 */

#include "common.h"

#include <pthread.h>

#include "mem.h"
#include "vector.h"
#include "heap.h"
#include "playrtp.h"

/** @brief Linked list of free packets
 *
 * This is a linked list of formerly used packets.  For preference we re-use
 * packets that have already been used rather than unused ones, to limit the
 * size of the program's working set.  If there are no free packets in the list
 * we try @ref next_free_packet instead.
 *
 * Must hold @ref lock when accessing this.
 */
static union free_packet *free_packets;

/** @brief Array of new free packets 
 *
 * There are @ref count_free_packets ready to use at this address.  If there
 * are none left we allocate more memory.
 *
 * Must hold @ref lock when accessing this.
 */
static union free_packet *next_free_packet;

/** @brief Count of new free packets at @ref next_free_packet
 *
 * Must hold @ref lock when accessing this.
 */
static size_t count_free_packets;

/** @brief Lock protecting packet allocator */
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

/** @brief Return a new packet */
struct packet *playrtp_new_packet(void) {
  struct packet *p;
  
  pthread_mutex_lock(&mem_lock);
  if(free_packets) {
    p = &free_packets->p;
    free_packets = free_packets->next;
  } else {
    if(!count_free_packets) {
      next_free_packet = xcalloc(1024, sizeof (union free_packet));
      count_free_packets = 1024;
    }
    p = &(next_free_packet++)->p;
    --count_free_packets;
  }
  pthread_mutex_unlock(&mem_lock);
  return p;
}

/** @brief Free a packet */
void playrtp_free_packet(struct packet *p) {
  union free_packet *u = (union free_packet *)p;
  pthread_mutex_lock(&mem_lock);
  u->next = free_packets;
  free_packets = u;
  pthread_mutex_unlock(&mem_lock);
}


/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
