/*
 * This file is part of DisOrder
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
/** @file lib/rights.h
 * @brief User rights
 */

#ifndef RIGHTS_H
#define RIGHTS_H

struct queue_entry;

/** @brief User can perform read-only operations */
#define RIGHT_READ            0x00000001

/** @brief User can add tracks to the queue */
#define RIGHT_PLAY            0x00000002

/** @brief User can move any track */
#define RIGHT_MOVE_ANY        0x00000004

/** @brief User can move their own tracks */
#define RIGHT_MOVE_MINE       0x00000008

/** @brief User can move randomly chosen tracks */
#define RIGHT_MOVE_RANDOM     0x00000010

#define RIGHT_MOVE__MASK      0x0000001c

/** @brief User can remove any track */
#define RIGHT_REMOVE_ANY      0x00000020

/** @brief User can remove their own tracks */
#define RIGHT_REMOVE_MINE     0x00000040

/** @brief User can remove randomly chosen tracks */
#define RIGHT_REMOVE_RANDOM   0x00000080

#define RIGHT_REMOVE__MASK    0x000000e0

/** @brief User can scratch any track */
#define RIGHT_SCRATCH_ANY     0x00000100

/** @brief User can scratch their own tracks */
#define RIGHT_SCRATCH_MINE    0x00000200

/** @brief User can scratch randomly chosen tracks */
#define RIGHT_SCRATCH_RANDOM  0x00000400

#define RIGHT_SCRATCH__MASK   0x00000700

/** @brief User can change the volume */
#define RIGHT_VOLUME          0x00000800

/** @brief User can perform admin operations */
#define RIGHT_ADMIN           0x00001000

/** @brief User can initiate a rescan */
#define RIGHT_RESCAN          0x00002000

/** @brief User can register new users */
#define RIGHT_REGISTER        0x00004000

/** @brief User can edit their own userinfo */
#define RIGHT_USERINFO        0x00008000

/** @brief User can modify track preferences */
#define RIGHT_PREFS           0x00010000

/** @brief User can modify global preferences */
#define RIGHT_GLOBAL_PREFS    0x00020000

/** @brief User can pause/resume */
#define RIGHT_PAUSE           0x00040000

/** @brief Current rights mask */
#define RIGHTS__MASK          0x0007ffff

/** @brief Connection is local
 *
 * This isn't a rights bit, it's used in @file server.c to limit
 * certain commands to local connections.
 */
#define RIGHT__LOCAL          0x80000000

/** @brief Unsigned type big enough for rights */
typedef uint32_t rights_type;

char *rights_string(rights_type r);
int parse_rights(const char *s, rights_type *rp, int report);
int right_scratchable(rights_type rights, const char *who,
		      const struct queue_entry *q);
int right_movable(rights_type rights, const char *who,
		  const struct queue_entry *q);
int right_removable(rights_type rights, const char *who,
		    const struct queue_entry *q);

#endif /* RIGHTS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
