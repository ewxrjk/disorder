/*
 * This file is part of DisOrder
 * Copyright (C) 2005, 2006, 2007 Richard Kettlewell
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
/** @file lib/trackorder.c
 * @brief Track ordering
 */
#include "common.h"

#include <fnmatch.h>

#include "trackname.h"
#include "log.h"
#include "unicode.h"

/** @brief Compare two tracks
 * @param sa First sort key
 * @param sb Second sort key
 * @param da First display string
 * @param db Second display string
 * @param ta First raw track
 * @param tb Second raw track
 * @return -ve, 0 or +ve for a <, = or > b
 *
 * Tries the following comparisons until a difference is found:
 * - case-independent comparison of sort keys
 * - case-dependent comparison of sort keys
 * - case-independent comparison of display strings
 * - case-dependent comparison of display strings
 * - case-dependent comparison of paths (see compare_path())
 */
int compare_tracks(const char *sa, const char *sb,
		   const char *da, const char *db,
		   const char *ta, const char *tb) {
  int c;

  if((c = strcmp(utf8_casefold_canon(sa, strlen(sa), 0),
		 utf8_casefold_canon(sb, strlen(sb), 0))))
    return c;
  if((c = strcmp(sa, sb))) return c;
  if((c = strcmp(utf8_casefold_canon(da, strlen(da), 0),
		 utf8_casefold_canon(db, strlen(db), 0))))
    return c;
  if((c = strcmp(da, db))) return c;
  return compare_path(ta, tb);
}

/** @brief Compare two paths
 * @param ap First path
 * @param an Length of @p ap
 * @param bp Second path
 * @param bn Length @p bp
 * @return -ve, 0 or +ve for ap <, = or > bp
 *
 * Sorts files within a directory together.
 *
 * See also compare_path().
 */
int compare_path_raw(const unsigned char *ap, size_t an,
		     const unsigned char *bp, size_t bn) {
  /* Don't change this function!  The database sort order depends on it */
  while(an > 0 && bn > 0) {
    if(*ap == *bp) {
      ap++;
      bp++;
      an--;
      bn--;
    } else if(*ap == '/') {
      return -1;		/* /a/b < /aa/ */
    } else if(*bp == '/') {
      return 1;			/* /aa > /a/b */
    } else
      return *ap - *bp;
  }
  if(an > 0)
    return 1;			/* /a/b > /a and /ab > /a */
  else if(bn > 0)
    return -1;			/* /a < /ab and /a < /a/b */
  else
    return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
