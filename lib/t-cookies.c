/*
 * This file is part of DisOrder.
 * Copyright (C) 2005, 2007, 2008 Richard Kettlewell
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

void test_cookies(void) {
  struct cookiedata cd[1];

  fprintf(stderr, "test_cookies\n");

  /* These are the examples from RFC2109 */
  insist(!parse_cookie("$Version=\"1\"; Customer=\"WILE_E_COYOTE\"; $Path=\"/acme\"", cd));
  insist(!strcmp(cd->version, "1"));
  insist(cd->ncookies == 1);
  insist(find_cookie(cd, "Customer") == &cd->cookies[0]);
  check_string(cd->cookies[0].value, "WILE_E_COYOTE");
  check_string(cd->cookies[0].path, "/acme");
  insist(cd->cookies[0].domain == 0);
  insist(!parse_cookie("$Version=\"1\";\n"
                       "Customer=\"WILE_E_COYOTE\"; $Path=\"/acme\";\n"
                       "Part_Number=\"Rocket_Launcher_0001\"; $Path=\"/acme\"",
                       cd));
  insist(cd->ncookies == 2);
  insist(find_cookie(cd, "Customer") == &cd->cookies[0]);
  insist(find_cookie(cd, "Part_Number") == &cd->cookies[1]);
  check_string(cd->cookies[0].value, "WILE_E_COYOTE");
  check_string(cd->cookies[0].path, "/acme");
  insist(cd->cookies[0].domain == 0);
  check_string(cd->cookies[1].value, "Rocket_Launcher_0001");
  check_string(cd->cookies[1].path, "/acme");
  insist(cd->cookies[1].domain == 0);
  insist(!parse_cookie("$Version=\"1\";\n"
                       "Customer=\"WILE_E_COYOTE\"; $Path=\"/acme\";\n"
                       "Part_Number=\"Rocket_Launcher_0001\"; $Path=\"/acme\";\n"
                       "Shipping=\"FedEx\"; $Path=\"/acme\"",
                       cd));
  insist(cd->ncookies == 3);
  insist(find_cookie(cd, "Customer") == &cd->cookies[0]);
  insist(find_cookie(cd, "Part_Number") == &cd->cookies[1]);
  insist(find_cookie(cd, "Shipping") == &cd->cookies[2]);
  check_string(cd->cookies[0].value, "WILE_E_COYOTE");
  check_string(cd->cookies[0].path, "/acme");
  insist(cd->cookies[0].domain == 0);
  check_string(cd->cookies[1].value, "Rocket_Launcher_0001");
  check_string(cd->cookies[1].path, "/acme");
  insist(cd->cookies[1].domain == 0);
  check_string(cd->cookies[2].value, "FedEx");
  check_string(cd->cookies[2].path, "/acme");
  insist(cd->cookies[2].domain == 0);

  insist(!parse_cookie("BX=brqn3il3r9jro&b=3&s=vv", cd));
  insist(cd->ncookies == 1);
  insist(find_cookie(cd, "BX") == &cd->cookies[0]);
  check_string(cd->cookies[0].value, "brqn3il3r9jro&b=3&s=vv");
  insist(cd->cookies[0].path == 0);
  insist(cd->cookies[0].domain == 0);
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
