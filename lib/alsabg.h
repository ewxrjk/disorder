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
/** @file alsabg.h
 * @brief Background-thread interface to ALSA
 *
 * This wraps ALSA with an interface which calls back to the client from a
 * thread.  It's not intended for completely general use, just what DisOrder
 * needs.
 */

#ifndef ALSABG_H
#define ALSABG_H

/** @brief Supply audio callback
 * @param dst Where to write audio data
 * @param nsamples Number of samples to write
 *
 * This function should write up to @p *nsamples samples of data at
 * @p dst, and return the number of samples written, or -1 if some error
 * occurred.  It will be called in a background thread.
 */
typedef int alsa_bg_supply(void *dst,
			   unsigned nsamples_max);

void alsa_bg_init(const char *dev,
		  alsa_bg_supply *supply);

void alsa_bg_enable(void);

void alsa_bg_disable(void);

void alsa_bg_close(void);

#endif /* ALSABG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
