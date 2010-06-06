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
/** @file lib/queue-rights.c
 * @brief Various rights-checking operations
 */

#include "common.h"

#include "queue.h"
#include "rights.h"

/** @brief Test for scratchability
 * @param rights User rights
 * @param who Username
 * @param q Queue entry or NULL
 * @return non-0 if scratchable, else 0
 */
int right_scratchable(rights_type rights, const char *who,
		      const struct queue_entry *q) {
  rights_type r;
  
  if(!q)
    return 0;
  if(q->submitter)
    if(!strcmp(q->submitter, who))
      r = RIGHT_SCRATCH_MINE|RIGHT_SCRATCH_ANY;
    else
      r = RIGHT_SCRATCH_ANY;
  else
    r = RIGHT_SCRATCH_RANDOM|RIGHT_SCRATCH_ANY;
  return !!(rights & r);
}

/** @brief Test for movability
 * @param rights User rights
 * @param who Username
 * @param q Queue entry or NULL
 * @return non-0 if movable, else 0
 */
int right_movable(rights_type rights, const char *who,
		  const struct queue_entry *q) {
  rights_type r;

  if(!q)
    return 0;
  if(q->submitter)
    if(!strcmp(q->submitter, who))
      r = RIGHT_MOVE_MINE|RIGHT_MOVE_ANY;
    else
      r = RIGHT_MOVE_ANY;
  else
    r = RIGHT_MOVE_RANDOM|RIGHT_MOVE_ANY;
  return !!(rights & r);
}

/** @brief Test for removability
 * @param rights User rights
 * @param who Username
 * @param q Queue entry or NULL
 * @return non-0 if removable, else 0
 */
int right_removable(rights_type rights, const char *who,
		    const struct queue_entry *q) {
  rights_type r;
  
  if(!q)
    return 0;
  if(q->submitter)
    if(!strcmp(q->submitter, who))
      r = RIGHT_REMOVE_MINE|RIGHT_REMOVE_ANY;
    else
      r = RIGHT_REMOVE_ANY;
  else
    r = RIGHT_REMOVE_RANDOM|RIGHT_REMOVE_ANY;
  return !!(rights & r);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
