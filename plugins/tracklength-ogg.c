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
/** @file plugins/tracklength-ogg.c
 * @brief Compute track lengths for OGG files
 */
#include "tracklength.h"
#include <vorbis/vorbisfile.h>

long tl_ogg(const char *path) {
  OggVorbis_File vf;
  FILE *fp = 0;
  double length;

  if(!path) goto error;
  if(!(fp = fopen(path, "rb"))) goto error;
  if(ov_open(fp, &vf, 0, 0)) goto error;
  fp = 0;
  length = ov_time_total(&vf, -1);
  ov_clear(&vf);
  return ceil(length);
error:
  if(fp) fclose(fp);
  return -1;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
