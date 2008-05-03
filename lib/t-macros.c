/*
 * This file is part of DisOrder.
 * Copyright (C) 2008 Richard Kettlewell
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
#include "test.h"
#include "macros.h"

static void test_macros(void) {
  const struct mx_node *m;
#define L1 "this is just some\n"
#define L2 "plain text\n"
  static const char plain[] = L1 L2;

  /* As simple as it gets */
  m = mx_parse("plaintext1", 1, "", NULL);
  insist(m == 0);

  /* Almost as simple as that */
  m = mx_parse("plaintext1", 1, plain, NULL);
  insist(m->type == MX_TEXT);
  check_string(m->filename, "plaintext1");
  insist(m->line == 1);
  check_string(m->text, L1 L2);
  insist(m->next == 0);
  check_string(mx_dump(m), plain);

  /* Check that partial parses stop in the right place */
  m = mx_parse("plaintext2", 5, plain, plain + strlen(L1));
  insist(m->type == MX_TEXT);
  check_string(m->filename, "plaintext2");
  insist(m->line == 5);
  check_string(m->text, L1);
  insist(m->next == 0);
  check_string(mx_dump(m), L1);

  /* The simplest possible expansion */
  m = mx_parse("macro1", 1, "@macro@", NULL);
  insist(m->type == MX_EXPANSION);
  check_string(m->filename, "macro1");
  insist(m->line == 1);
  check_string(m->name, "macro");
  insist(m->nargs == 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");

  /* Spacing variants of the above */
  m = mx_parse("macro2", 1, "@    macro@", NULL);
  insist(m->type == MX_EXPANSION);
  check_string(m->filename, "macro2");
  insist(m->line == 1);
  check_string(m->name, "macro");
  insist(m->nargs == 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");
  m = mx_parse("macro3", 1, "@macro    @", NULL);
  insist(m->type == MX_EXPANSION);
  check_string(m->filename, "macro3");
  insist(m->line == 1);
  check_string(m->name, "macro");
  insist(m->nargs == 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");

  /* Unterminated variants */
  m = mx_parse("macro4", 1, "@macro", NULL);
  insist(m->type == MX_EXPANSION);
  check_string(m->filename, "macro4");
  insist(m->line == 1);
  check_string(m->name, "macro");
  insist(m->nargs == 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");
  m = mx_parse("macro5", 1, "@macro   ", NULL);
  insist(m->type == MX_EXPANSION);
  check_string(m->filename, "macro5");
  insist(m->line == 1);
  check_string(m->name, "macro");
  insist(m->nargs == 0);
  insist(m->args == 0);
  insist(m->next == 0);
  check_string(mx_dump(m), "@macro@");

  /* Macros with a :-separated argument */
  m = mx_parse("macro5", 1, "@macro:arg@", NULL);
  insist(m->type == MX_EXPANSION);
  check_string(m->filename, "macro5");
  insist(m->line == 1);
  check_string(m->name, "macro");
  insist(m->nargs == 1);
  insist(m->next == 0);
  
  insist(m->args[0]->type == MX_TEXT);
  check_string(m->args[0]->filename, "macro5");
  insist(m->args[0]->line == 1);
  check_string(m->args[0]->text, "arg");
  insist(m->args[0]->next == 0);

  check_string(mx_dump(m), "@macro{arg}@");

  /* Multiple :-separated arguments, and spacing, and newlines */
  m = mx_parse("macro6", 1, "@macro : \n arg1 : \n arg2@", NULL);
  insist(m->type == MX_EXPANSION);
  check_string(m->filename, "macro6");
  insist(m->line == 1);
  check_string(m->name, "macro");
  insist(m->nargs == 2);
  insist(m->next == 0);
  
  insist(m->args[0]->type == MX_TEXT);
  check_string(m->args[0]->filename, "macro6");
  insist(m->args[0]->line == 2);
  check_string(m->args[0]->text, "arg1");
  insist(m->args[0]->next == 0);
  
  insist(m->args[1]->type == MX_TEXT);
  check_string(m->args[1]->filename, "macro6");
  insist(m->args[1]->line == 3);
  check_string(m->args[1]->text, "arg2");
  insist(m->args[1]->next == 0);

  check_string(mx_dump(m), "@macro{arg1}{arg2}@");

  /* Multiple bracketed arguments */
  

}

TEST(macros);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
