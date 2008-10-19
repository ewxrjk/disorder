/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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
#include "test.h"

static void test_signame(void) {
  insist(find_signal("SIGTERM") == SIGTERM);
  insist(find_signal("SIGHUP") == SIGHUP);
  insist(find_signal("SIGINT") == SIGINT);
  insist(find_signal("SIGQUIT") == SIGQUIT);
  insist(find_signal("SIGKILL") == SIGKILL);
  insist(find_signal("SIGYOURMUM") == -1);
}

TEST(signame);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
