/*
 * This file is part of DisOrder
 * Copyright (C) 2005-2008 Richard Kettlewell
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
/** @file lib/trackname.h
 * @brief Track name calculation
 */
#ifndef TRACKNAME_H
#define TRACKNAME_H

const struct collection *find_track_collection(const char *track);
/* find the collection for @track@ */

const char *find_track_root(const char *track);
/* find the collection root for @track@ */

const char *track_rootless(const char *track);
/* return the rootless part of @track@ (typically starting /) */

const char *trackname_part(const char *track,
			   const char *context,
			   const char *part);
/* compute PART (artist/album/title) for TRACK in CONTEXT (display/sort) */

const char *trackname_transform(const char *type,
				const char *subject,
				const char *context);
/* convert SUBJECT (usually 'track' or 'dir' according to TYPE) for CONTEXT
 * (display/sort) */

int compare_tracks(const char *sa, const char *sb,
		   const char *da, const char *db,
		   const char *ta, const char *tb);
/* Compare tracks A and B, with sort/display/track names S?, D? and T? */

int compare_path_raw(const unsigned char *ap, size_t an,
		     const unsigned char *bp, size_t bn);
/* Comparison function for path names that groups all entries in a directory
 * together */

/** @brief Compare two paths
 * @param ap First path
 * @param bp Second path
 * @return -ve, 0 or +ve for ap <, = or > bp
 *
 * Sorts files within a directory together.
 * A wrapper around compare_path_raw().
 */
static inline int compare_path(const char *ap, const char *bp) {
  return compare_path_raw((const unsigned char *)ap, strlen(ap),
			  (const unsigned char *)bp, strlen(bp));
}

/** @brief Entry in a list of tracks or directories */
struct tracksort_data {
  /** @brief Track name */
  const char *track;
  /** @brief Sort key */
  const char *sort;
  /** @brief Display key */
  const char *display;
};

struct tracksort_data *tracksort_init(int nvec,
                                      char **vec,
                                      const char *type);

#endif /* TRACKNAME_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
