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

static void test_selection(void) {
  hash *h;

  insist((h = selection_new()) != 0);
  selection_set(h, "one", 1);
  selection_set(h, "two", 1);
  selection_set(h, "three", 0);
  selection_set(h, "four", 1);
  insist(selection_selected(h, "one") == 1);
  insist(selection_selected(h, "two") == 1);
  insist(selection_selected(h, "three") == 0);
  insist(selection_selected(h, "four") == 1);
  insist(selection_selected(h, "five") == 0);
  insist(hash_count(h) == 3);
  selection_flip(h, "one"); 
  selection_flip(h, "three"); 
  insist(selection_selected(h, "one") == 0);
  insist(selection_selected(h, "three") == 1);
  insist(hash_count(h) == 3);
  selection_live(h, "one");
  selection_live(h, "two");
  selection_live(h, "three");
  selection_cleanup(h);
  insist(selection_selected(h, "one") == 0);
  insist(selection_selected(h, "two") == 1);
  insist(selection_selected(h, "three") == 1);
  insist(selection_selected(h, "four") == 0);
  insist(selection_selected(h, "five") == 0);
  insist(hash_count(h) == 2);
  selection_empty(h);
  insist(selection_selected(h, "one") == 0);
  insist(selection_selected(h, "two") == 0);
  insist(selection_selected(h, "three") == 0);
  insist(selection_selected(h, "four") == 0);
  insist(selection_selected(h, "five") == 0);
  insist(hash_count(h) == 0);
}

TEST(selection);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
