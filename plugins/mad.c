/** @file plugins/mad.c
 * @brief MP3 Length calculation
 *
 * This file is a subset of the debian source tarball of
 * mpg321-0.2.10.3/mad.c - see http://mpg321.sourceforge.net/
 */

/*
    mpg321 - a fully free clone of mpg123.
    Copyright (C) 2001 Joe Drew
    
    Originally based heavily upon:
    plaympeg - Sample MPEG player using the SMPEG library
    Copyright (C) 1999 Loki Entertainment Software
    
    Also uses some code from
    mad - MPEG audio decoder
    Copyright (C) 2000-2001 Robert Leslie
    
    Original playlist code contributed by Tobias Bengtsson <tobbe@tobbe.nu>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <sys/types.h>
#include <string.h>

#include <mad.h>

#include "madshim.h"

/* XING parsing is from the MAD winamp input plugin */

struct xing {
  int flags;
  unsigned long frames;
  unsigned long bytes;
  unsigned char toc[100];
  long scale;
};

enum {
  XING_FRAMES = 0x0001,
  XING_BYTES  = 0x0002,
  XING_TOC    = 0x0004,
  XING_SCALE  = 0x0008
};

# define XING_MAGIC     (('X' << 24) | ('i' << 16) | ('n' << 8) | 'g')

static
int parse_xing(struct xing *xing, struct mad_bitptr ptr, unsigned int bitlen)
{
  if (bitlen < 64 || mad_bit_read(&ptr, 32) != XING_MAGIC)
    goto fail;

  xing->flags = mad_bit_read(&ptr, 32);
  bitlen -= 64;

  if (xing->flags & XING_FRAMES) {
    if (bitlen < 32)
      goto fail;

    xing->frames = mad_bit_read(&ptr, 32);
    bitlen -= 32;
  }

  if (xing->flags & XING_BYTES) {
    if (bitlen < 32)
      goto fail;

    xing->bytes = mad_bit_read(&ptr, 32);
    bitlen -= 32;
  }

  if (xing->flags & XING_TOC) {
    int i;

    if (bitlen < 800)
      goto fail;

    for (i = 0; i < 100; ++i)
      xing->toc[i] = mad_bit_read(&ptr, 8);

    bitlen -= 800;
  }

  if (xing->flags & XING_SCALE) {
    if (bitlen < 32)
      goto fail;

    xing->scale = mad_bit_read(&ptr, 32);
    bitlen -= 32;
  }

  return 1;

 fail:
  xing->flags = 0;
  return 0;
}

/* Following two functions are adapted from mad_timer, from the 
   libmad distribution */
void scan_mp3(void const *ptr, ssize_t len, buffer *buf)
{
    struct mad_stream stream;
    struct mad_header header;
    struct xing xing;
    
    unsigned long bitrate = 0;
    int has_xing = 0;
    int is_vbr = 0;

    memset(&xing, 0, sizeof xing);
    
    mad_stream_init(&stream);
    mad_header_init(&header);

    mad_stream_buffer(&stream, ptr, len);

    buf->num_frames = 0;

    /* There are three ways of calculating the length of an mp3:
      1) Constant bitrate: One frame can provide the information
         needed: # of frames and duration. Just see how long it
         is and do the division.
      2) Variable bitrate: Xing tag. It provides the number of 
         frames. Each frame has the same number of samples, so
         just use that.
      3) All: Count up the frames and duration of each frames
         by decoding each one. We do this if we've no other
         choice, i.e. if it's a VBR file with no Xing tag.
    */

    while (1)
    {
        if (mad_header_decode(&header, &stream) == -1)
        {
            if (MAD_RECOVERABLE(stream.error))
                continue;
            else
                break;
        }

        /* Limit xing testing to the first frame header */
        if (!buf->num_frames++)
        {
            if(parse_xing(&xing, stream.anc_ptr, stream.anc_bitlen))
            {
                is_vbr = 1;
                
                if (xing.flags & XING_FRAMES)
                {
                    /* We use the Xing tag only for frames. If it doesn't have that
                       information, it's useless to us and we have to treat it as a
                       normal VBR file */
                    has_xing = 1;
                    buf->num_frames = xing.frames;
                    break;
                }
            }
        }                

        /* Test the first n frames to see if this is a VBR file */
        if (!is_vbr && !(buf->num_frames > 20))
        {
            if (bitrate && header.bitrate != bitrate)
            {
                is_vbr = 1;
            }
            
            else
            {
                bitrate = header.bitrate;
            }
        }
        
        /* We have to assume it's not a VBR file if it hasn't already been
           marked as one and we've checked n frames for different bitrates */
        else if (!is_vbr)
        {
            break;
        }
            
        mad_timer_add(&buf->duration, header.duration);
    }

    if (!is_vbr)
    {
        double time = (len * 8.0) / (header.bitrate); /* time in seconds */
        double timefrac = (double)time - ((long)(time));
        long nsamples = 32 * MAD_NSBSAMPLES(&header); /* samples per frame */
        
        /* samplerate is a constant */
        buf->num_frames = (long) (time * header.samplerate / nsamples);

        mad_timer_set(&buf->duration, (long)time, (long)(timefrac*100), 100);
    }
        
    else if (has_xing)
    {
        /* modify header.duration since we don't need it anymore */
        mad_timer_multiply(&header.duration, buf->num_frames);
        buf->duration = header.duration;
    }

    else
    {
        /* the durations have been added up, and the number of frames
           counted. We do nothing here. */
    }
    
    mad_header_finish(&header);
    mad_stream_finish(&stream);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
