/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007 Richard Kettlewell
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
/** @file plugins/tracklength.c
 * @brief Plugin to compute track lengths
 *
 * Currently implements MP3, OGG, FLAC and WAV.
 */
#include "tracklength.h"

static const struct {
  const char *ext;
  long (*fn)(const char *path);
} file_formats[] = {
  { ".FLAC", tl_flac },
  { ".MP3", tl_mp3 },
  { ".OGG", tl_ogg },
  { ".WAV", tl_wav },
  { ".flac", tl_flac },
  { ".mp3", tl_mp3 },
  { ".ogg", tl_ogg },
  { ".wav", tl_wav }
};
#define N_FILE_FORMATS (int)(sizeof file_formats / sizeof *file_formats)

long disorder_tracklength(const char attribute((unused)) *track,
			  const char *path) {
  const char *ext = strrchr(path, '.');
  int l, r, m = 0, c = 0;		/* quieten compiler */

  if(ext) {
    l = 0;
    r = N_FILE_FORMATS - 1;
    while(l <= r && (c = strcmp(ext, file_formats[m = (l + r) / 2].ext)))
      if(c < 0)
	r = m - 1;
      else
	l = m + 1;
    if(!c)
      return file_formats[m].fn(path);
  }
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
