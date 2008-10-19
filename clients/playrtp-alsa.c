/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
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
/** @file clients/playrtp-alsa.c
 * @brief RTP player - ALSA support
 *
 * This has been rewritten to use the @ref alsabg.h interface and is therefore
 * now closely modelled on @ref playrtp-coreaudio.c.  Given a similar interface
 * wrapping OSS the whole of playrtp could probably be greatly simplified.
 */

#include "common.h"

#if HAVE_ALSA_ASOUNDLIB_H 

#include <poll.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "mem.h"
#include "log.h"
#include "vector.h"
#include "heap.h"
#include "playrtp.h"
#include "alsabg.h"

/** @brief Callback from alsa_bg_collect() */
static int playrtp_alsa_supply(void *dst,
                               unsigned supply_nsamples) {
  unsigned samples_available;
  const struct packet *p;

  pthread_mutex_lock(&lock);
  p = playrtp_next_packet();
  if(p && contains(p, next_timestamp)) {
    /* This packet is ready to play */
    const uint32_t packet_end = p->timestamp + p->nsamples;
    const uint32_t offset = next_timestamp - p->timestamp;
    const uint16_t *src = (void *)(p->samples_raw + offset);
    samples_available = packet_end - next_timestamp;
    if(samples_available > supply_nsamples)
      samples_available = supply_nsamples;
    next_timestamp += samples_available;
    memcpy(dst, src, samples_available * sizeof (int16_t));
    /* We don't bother junking the packet - that'll be dealt with next time
     * round */
  } else {
    /* No packet is ready to play (and there might be no packet at all) */
    samples_available = p ? p->timestamp - next_timestamp : supply_nsamples;
    if(samples_available > supply_nsamples)
      samples_available = supply_nsamples;
    /*info("infill %d", samples_available);*/
    next_timestamp += samples_available;
    /* Unlike Core Audio the buffer is not guaranteed to be 0-filled */
    memset(dst, 0, samples_available * sizeof (int16_t));
  }
  pthread_mutex_unlock(&lock);
  return samples_available;
}

void playrtp_alsa(void) {
  alsa_bg_init(device ? device : "default",
               playrtp_alsa_supply);
  pthread_mutex_lock(&lock);
  for(;;) {
    /* Wait for the buffer to fill up a bit */
    playrtp_fill_buffer();
    /* Start playing now */
    info("Playing...");
    next_timestamp = pheap_first(&packets)->timestamp;
    active = 1;
    alsa_bg_enable();
    /* Wait until the buffer empties out */
    while(nsamples >= minbuffer
	  || (nsamples > 0
	      && contains(pheap_first(&packets), next_timestamp))) {
      pthread_cond_wait(&cond, &lock);
    }
    /* Stop playing for a bit until the buffer re-fills */
    alsa_bg_disable();
    active = 0;
    /* Go back round */
  }
}

#endif

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
