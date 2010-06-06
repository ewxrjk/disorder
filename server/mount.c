/*
 * This file is part of DisOrder.
 * Copyright (C) 2010 Richard Kettlewell
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
/** @file server/mount.c
 * @brief Periodically check for devices being mounted and unmounted
 */
#include "disorder-server.h"
#if HAVE_GETFSSTAT
# include <sys/param.h>
# include <sys/ucred.h>
# include <sys/mount.h>
#endif

#if HAVE_GETFSSTAT
static int compare_fsstat(const void *av, const void *bv) {
  const struct statfs *a = av, *b = bv;
  int c;
 
  c = memcmp(&a->f_fsid, &b->f_fsid, sizeof a->f_fsid);
  if(c)
    return c;
  c = strcmp(a->f_mntonname, b->f_mntonname);
  if(c)
    return c;
  return 0;
}
#endif

void periodic_mount_check(ev_source *ev_) {
#if HAVE_GETFSSTAT
  /* On OS X, we keep track of the hash of the kernel's mounted
   * filesystem list */
  static int first = 1;
  static unsigned char last[20];
  unsigned char *current;
  int nfilesystems, space;
  struct statfs *buf;
  gcrypt_hash_handle h;
  gcry_error_t e;

  space = getfsstat(NULL, 0, MNT_NOWAIT);
  buf = xcalloc(space, sizeof *buf);
  nfilesystems = getfsstat(buf, space * sizeof *buf, MNT_NOWAIT);
  if(nfilesystems > space)
    // The array grew between check and use!  We just give up and try later.
    return;
  // Put into order so we get a bit of consistency
  qsort(buf, nfilesystems, sizeof *buf, compare_fsstat);
  if((e = gcry_md_open(&h, GCRY_MD_SHA1, 0))) {
    disorder_error(0, "gcry_md_open: %s", gcry_strerror(e));
    return;
  }
  for(int n = 0; n < nfilesystems; ++n) {
    gcry_md_write(h, &buf[n].f_fsid, sizeof buf[n].f_fsid);
    gcry_md_write(h, buf[n].f_mntonname, 1 + strlen(buf[n].f_mntonname));
  }
  current = gcry_md_read(h, GCRY_MD_SHA1);
  if(!first && memcmp(current, last, sizeof last))
    trackdb_rescan(ev_, 1/*check*/, 0, 0);
  memcpy(last, current, sizeof last);
  first = 0;
#elif defined PATH_MTAB
  /* On Linux we keep track of the modification time of /etc/mtab */
  static time_t last_mount;
  struct stat sb;
  
  if(stat(PATH_MTAB, &sb) >= 0) {
    if(last_mount != 0 && last_mount != sb.st_mtime)
      trackdb_rescan(ev_, 1/*check*/, 0, 0);
    last_mount = sb.st_mtime;
  }
#endif
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
