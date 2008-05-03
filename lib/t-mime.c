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

static int test_multipart_callback(const char *s, void *u) {
  struct vector *parts = u;

  vector_append(parts, (char *)s);
  return 0;
}

static int header_callback(const char *name, const char *value,
			   void *u) {
  hash *const h = u;

  hash_add(h, name, &value, HASH_INSERT);
  return 0;
}

static void test_mime(void) {
  char *t, *n, *v;
  struct vector parts[1];
  struct kvp *k;
  const char *s, *cs, *enc;
  hash *h;

  t = 0;
  k = 0;
  insist(!mime_content_type("text/plain", &t, &k));
  check_string(t, "text/plain");
  insist(k == 0);

  insist(mime_content_type("TEXT ((broken) comment", &t, &k) < 0);
  insist(mime_content_type("TEXT ((broken) comment\\", &t, &k) < 0);
  
  t = 0;
  k = 0;
  insist(!mime_content_type("TEXT ((nested)\\ comment) /plain", &t, &k));
  check_string(t, "text/plain");
  insist(k == 0);

  t = 0;
  k = 0;
  insist(!mime_content_type(" text/plain ; Charset=\"utf-\\8\"", &t, &k));
  check_string(t, "text/plain");
  insist(k != 0);
  insist(k->next == 0);
  check_string(k->name, "charset");
  check_string(k->value, "utf-8");

  t = 0;
  k = 0;
  insist(!mime_content_type("text/plain;charset = ISO-8859-1 ", &t, &k));
  insist(k != 0);
  insist(k->next == 0);
  check_string(t, "text/plain");
  check_string(k->name, "charset");
  check_string(k->value, "ISO-8859-1");

  t = n = v = 0;
  insist(!mime_rfc2388_content_disposition("form-data; name=\"field1\"", &t, &n, &v));
  check_string(t, "form-data");
  check_string(n, "name");
  check_string(v, "field1");

  insist(!mime_rfc2388_content_disposition("inline", &t, &n, &v));
  check_string(t, "inline");
  insist(n == 0);
  insist(v == 0);

  /* Current versions of the code only understand a single arg to these
   * headers.  This is a bug at the level they work at but suffices for
   * DisOrder's current purposes. */

  insist(!mime_rfc2388_content_disposition(
              "attachment; filename=genome.jpeg;\n"
              "modification-date=\"Wed, 12 Feb 1997 16:29:51 -0500\"",
         &t, &n, &v));
  check_string(t, "attachment");
  check_string(n, "filename");
  check_string(v, "genome.jpeg");

  vector_init(parts);
  insist(mime_multipart("--outer\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Disposition: inline\r\n"
                        "Content-Description: text-part-1\r\n"
                        "\r\n"
                        "Some text goes here\r\n"
                        "\r\n"
                        "--outer\r\n"
                        "Content-Type: multipart/mixed; boundary=inner\r\n"
                        "Content-Disposition: attachment\r\n"
                        "Content-Description: multipart-2\r\n"
                        "\r\n"
                        "--inner\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Disposition: inline\r\n"
                        "Content-Description: text-part-2\r\n"
                        "\r\n"
                        "Some more text here.\r\n"
                        "\r\n"
                        "--inner\r\n"
                        "Content-Type: image/jpeg\r\n"
                        "Content-Disposition: attachment\r\n"
                        "Content-Description: jpeg-1\r\n"
                        "\r\n"
                        "<jpeg data>\r\n"
                        "--inner--\r\n"
                        "--outer--\r\n",
                        test_multipart_callback,
                        "outer",
                        parts) == 0);
  check_integer(parts->nvec, 2);
  check_string(parts->vec[0],
               "Content-Type: text/plain\r\n"
               "Content-Disposition: inline\r\n"
               "Content-Description: text-part-1\r\n"
               "\r\n"
               "Some text goes here\r\n");
  check_string(parts->vec[1],
               "Content-Type: multipart/mixed; boundary=inner\r\n"
               "Content-Disposition: attachment\r\n"
               "Content-Description: multipart-2\r\n"
               "\r\n"
               "--inner\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Disposition: inline\r\n"
               "Content-Description: text-part-2\r\n"
               "\r\n"
               "Some more text here.\r\n"
               "\r\n"
               "--inner\r\n"
               "Content-Type: image/jpeg\r\n"
               "Content-Disposition: attachment\r\n"
               "Content-Description: jpeg-1\r\n"
               "\r\n"
               "<jpeg data>\r\n"
               "--inner--");
  /* No trailing CRLF is _correct_ - see RFC2046 5.1.1 note regarding CRLF
   * preceding the boundary delimiter line.  An implication of this is that we
   * must cope with partial lines at the end of the input when recursively
   * decomposing a multipart message. */
  vector_init(parts);
  insist(mime_multipart("--inner\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Disposition: inline\r\n"
                        "Content-Description: text-part-2\r\n"
                        "\r\n"
                        "Some more text here.\r\n"
                        "\r\n"
                        "--inner\r\n"
                        "Content-Type: image/jpeg\r\n"
                        "Content-Disposition: attachment\r\n"
                        "Content-Description: jpeg-1\r\n"
                        "\r\n"
                        "<jpeg data>\r\n"
                        "--inner--",
                        test_multipart_callback,
                        "inner",
                        parts) == 0);
  check_integer(parts->nvec, 2);
  check_string(parts->vec[0],
               "Content-Type: text/plain\r\n"
               "Content-Disposition: inline\r\n"
               "Content-Description: text-part-2\r\n"
               "\r\n"
               "Some more text here.\r\n");
  check_string(parts->vec[1],
               "Content-Type: image/jpeg\r\n"
               "Content-Disposition: attachment\r\n"
               "Content-Description: jpeg-1\r\n"
               "\r\n"
               "<jpeg data>");
 
  /* XXX mime_parse */

  check_string(mime_qp(""), "");
  check_string(mime_qp("foobar"), "foobar");
  check_string(mime_qp("foo=20bar"), "foo bar");
  check_string(mime_qp("x \r\ny"), "x\r\ny");
  check_string(mime_qp("x=\r\ny"), "xy");
  check_string(mime_qp("x= \r\ny"), "xy");
  check_string(mime_qp("x =\r\ny"), "x y");
  check_string(mime_qp("x = \r\ny"), "x y");

  check_string(mime_to_qp(""), "");
  check_string(mime_to_qp("foobar\n"), "foobar\n");
  check_string(mime_to_qp("foobar \n"), "foobar=20\n");
  check_string(mime_to_qp("foobar\t\n"), "foobar=09\n"); 
  check_string(mime_to_qp("foobar \t \n"), "foobar=20=09=20\n");
  check_string(mime_to_qp(" foo=bar"), " foo=3Dbar\n");
  check_string(mime_to_qp("copyright \xC2\xA9"), "copyright =C2=A9\n");
  check_string(mime_to_qp("foo\nbar\nbaz\n"), "foo\nbar\nbaz\n");
  check_string(mime_to_qp("wibble wobble wibble wobble wibble wobble wibble wobble wibble wobble wibble"), "wibble wobble wibble wobble wibble wobble wibble wobble wibble wobble wibb=\nle\n");
 
  /* from RFC2045 */
  check_string(mime_qp("Now's the time =\r\n"
"for all folk to come=\r\n"
" to the aid of their country."),
	       "Now's the time for all folk to come to the aid of their country.");

#define check_base64(encoded, decoded) do {                     \
    size_t ns;                                                  \
    check_string(mime_base64(encoded, &ns), decoded);           \
    insist(ns == (sizeof decoded) - 1);                         \
    check_string(mime_to_base64((const uint8_t *)decoded,       \
                                         (sizeof decoded) - 1), \
                 encoded);                                      \
  } while(0)
    
  
  check_base64("",  "");
  check_base64("BBBB", "\x04\x10\x41");
  check_base64("////", "\xFF\xFF\xFF");
  check_base64("//BB", "\xFF\xF0\x41");
  check_base64("BBBB//BB////",
             "\x04\x10\x41" "\xFF\xF0\x41" "\xFF\xFF\xFF");
  check_base64("BBBBBA==",
	       "\x04\x10\x41" "\x04");
  check_base64("BBBBBBA=",
	       "\x04\x10\x41" "\x04\x10");

  /* Check that decoding handles various kinds of rubbish OK */
  check_string(mime_base64("B B B B  / / B B / / / /", 0),
             "\x04\x10\x41" "\xFF\xF0\x41" "\xFF\xFF\xFF");
  check_string(mime_base64("B\r\nBBB.// B-B//~//", 0),
	       "\x04\x10\x41" "\xFF\xF0\x41" "\xFF\xFF\xFF");
  check_string(mime_base64("BBBB BB==", 0),
	       "\x04\x10\x41" "\x04");
  check_string(mime_base64("BBBB BB = =", 0),
	       "\x04\x10\x41" "\x04");
  check_string(mime_base64("BBBB BBB=", 0),
	       "\x04\x10\x41" "\x04\x10");
  check_string(mime_base64("BBBB BBB = ", 0),
	       "\x04\x10\x41" "\x04\x10");
  check_string(mime_base64("BBBB=", 0),
	       "\x04\x10\x41");
  check_string(mime_base64("BBBBBB==", 0),
	       "\x04\x10\x41" "\x04");
  check_string(mime_base64("BBBBBBB=", 0),
	       "\x04\x10\x41" "\x04\x10");
  /* Not actually valid base64 */
  check_string(mime_base64("BBBBx=", 0),
	       "\x04\x10\x41");

  h = hash_new(sizeof (char *));
  s = mime_parse("From: sender@example.com\r\n"
		 "To: rcpt@example.com\r\n"
		 "Subject: test #1\r\n"
		 "\r\n"
		 "body\r\n",
		 header_callback, h);
  insist(s != 0);
  check_string(*(char **)hash_find(h, "from"), "sender@example.com");
  check_string(*(char **)hash_find(h, "to"), "rcpt@example.com");
  check_string(*(char **)hash_find(h, "subject"), "test #1");
  check_string(s, "body\r\n");

  h = hash_new(sizeof (char *));
  s = mime_parse("FROM: sender@example.com\r\n"
		 "TO: rcpt@example.com\r\n"
		 "SUBJECT: test #1\r\n"
		 "CONTENT-TRANSFER-ENCODING: 7bit\r\n"
		 "\r\n"
		 "body\r\n",
		 header_callback, h);
  insist(s != 0);
  check_string(*(char **)hash_find(h, "from"), "sender@example.com");
  check_string(*(char **)hash_find(h, "to"), "rcpt@example.com");
  check_string(*(char **)hash_find(h, "subject"), "test #1");
  check_string(*(char **)hash_find(h, "content-transfer-encoding"), "7bit");
  check_string(s, "body\r\n");

  h = hash_new(sizeof (char *));
  s = mime_parse("From: sender@example.com\r\n"
		 "To:    \r\n"
		 "     rcpt@example.com\r\n"
		 "Subject: test #1\r\n"
		 "MIME-Version: 1.0\r\n"
		 "Content-Type: text/plain\r\n"
		 "Content-Transfer-Encoding: BASE64\r\n"
		 "\r\n"
		 "d2liYmxlDQo=\r\n",
		 header_callback, h);
  insist(s != 0); 
  check_string(*(char **)hash_find(h, "from"), "sender@example.com");
  check_string(*(char **)hash_find(h, "to"), "rcpt@example.com");
  check_string(*(char **)hash_find(h, "subject"), "test #1");
  check_string(*(char **)hash_find(h, "mime-version"), "1.0");
  check_string(*(char **)hash_find(h, "content-type"), "text/plain");
  check_string(*(char **)hash_find(h, "content-transfer-encoding"), "BASE64");
  check_string(s, "wibble\r\n");

#define CHECK_QUOTE(INPUT, EXPECT) do {                 \
  s = quote822(INPUT, 0);                               \
  insist(s != 0);                                       \
  check_string(s, EXPECT);                              \
  s = mime_parse_word(s, &t, mime_http_separator);      \
  check_string(t, INPUT);                               \
} while(0)
  CHECK_QUOTE("wibble", "wibble");
  CHECK_QUOTE("wibble spong", "\"wibble spong\"");
  CHECK_QUOTE("wibble\\spong", "\"wibble\\\\spong\"");
  CHECK_QUOTE("wibble\"spong", "\"wibble\\\"spong\"");
  CHECK_QUOTE("(wibble)", "\"(wibble)\"");

  s = mime_encode_text("wibble\n", &cs, &enc);
  insist(s != 0);
  check_string(s, "wibble\n");
  check_string(cs, "us-ascii");
  check_string(enc, "7bit");

  s = mime_encode_text("wibble\xC3\xB7\n", &cs, &enc);
  insist(s != 0);
  check_string(s, "wibble=C3=B7\n");
  check_string(cs, "utf-8");
  check_string(enc, "quoted-printable");
}

TEST(mime);

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
