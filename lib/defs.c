/*
 * This file is part of DisOrder
 * Copyright (C) 2005 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
/** @file lib/defs.c @brief Definitions chosen by configure
 *
 * The binary directories are included so that they can be appended to the path
 * (see fix_path()), not so that the path can be ignored.
 */

#include <config.h>
#include "types.h"

#include "defs.h"
#include "definitions.h"

/** @brief Software version number */
const char disorder_version_string[] = VERSION;

/** @brief Package library directory */
const char pkglibdir[] = PKGLIBDIR;

/** @brief Package configuration directory */
const char pkgconfdir[] = PKGCONFDIR;

/** @brief Package variable state directory */
const char pkgstatedir[] = PKGSTATEDIR;

/** @brief Package fixed data directory */
const char pkgdatadir[] = PKGDATADIR;

/** @brief Binary directory */
const char bindir[] = BINDIR;

/** @brief System binary directory */
const char sbindir[] = SBINDIR;

/** @brief Fink binary directory
 *
 * Meaningless if not on a Mac.
 */
const char finkbindir[] = FINKBINDIR;

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
End:
*/
