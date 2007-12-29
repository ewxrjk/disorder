/*
 * This file is part of DisOrder
 * Copyright (C) 2007 Richard Kettlewell
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

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <gcrypt.h>

#include "syscalls.h"
#include "log.h"
#include "configuration.h"
#include "inputline.h"
#include "addr.h"
#include "hostname.h"
#include "sendmail.h"
#include "base64.h"

/** @brief Get a server response
 * @param tag Server name
 * @param in Input stream
 * @return Response code 0-999 or -1 on error
 */
static int getresponse(const char *tag, FILE *in) {
  char *line;

  while(!inputline(tag, in, &line, CRLF)) {
    if(line[0] >= '0' && line[0] <= '9'
       && line[1] >= '0' && line[1] <= '9'
       && line[2] >= '0' && line[2] <= '9') {
      const int rc = 10 * (10 * line[0] + line[1]) + line[2] - 111 * '0';
      if(rc >= 400 && rc <= 599)
	error(0, "%s: %s", tag, line);
      if(line[3] != '-') {
	return rc;
      }
      /* else go round for further response lines */
    } else {
      error(0, "%s: malformed response: %s", tag, line);
      return -1;
    }
  }
  if(ferror(in))
    error(errno, "%s: read error", tag);
  else
    error(0, "%s: server closed connection", tag);
  return -1;
}

/** @brief Send a command to the server
 * @param tag Server name
 * @param out stream to send commands to
 * @param fmt Format string
 * @return 0 on success, non-0 on error
 */
static int sendcommand(const char *tag, FILE *out, const char *fmt, ...) {
  va_list ap;
  int rc;

  va_start(ap, fmt);
  rc = vfprintf(out, fmt, ap);
  va_end(ap);
  if(rc >= 0)
    rc = fputs("\r\n", out);
  if(rc >= 0)
    rc = fflush(out);
  if(rc >= 0)
    return 0;
  error(errno, "%s: write error", tag);
  return -1;
}

/** @brief Send a mail message
 * @param tag Server name
 * @param in Stream to read responses from
 * @param out Stream to send commands to
 * @param sender Sender address (can be "")
 * @param pubsender Visible sender address (must not be "")
 * @param recipient Recipient address
 * @param subject Subject string
 * @param encoding Body encoding
 * @param body_type Content-type of body
 * @param body Text of body (encoded, but \n for newline)
 * @return 0 on success, non-0 on error
 */
static int sendmailfp(const char *tag, FILE *in, FILE *out,
		      const char *sender,
		      const char *pubsender,
		      const char *recipient,
		      const char *subject,
		      const char *encoding,
		      const char *content_type,
		      const char *body) {
  int rc, sol = 1;
  const char *ptr;
  uint8_t idbuf[20];
  char *id;

  gcry_create_nonce(idbuf, sizeof idbuf);
  id = mime_to_base64(idbuf, sizeof idbuf);
  if((rc = getresponse(tag, in)) / 100 != 2)
    return -1;
  if(sendcommand(tag, out, "HELO %s", local_hostname()))
    return -1;
  if((rc = getresponse(tag, in)) / 100 != 2)
    return -1;
  if(sendcommand(tag, out, "MAIL FROM:<%s>", sender))
    return -1;
  if((rc = getresponse(tag, in)) / 100 != 2)
    return -1;
  if(sendcommand(tag, out, "RCPT TO:<%s>", recipient))
    return -1;
  if((rc = getresponse(tag, in)) / 100 != 2)
    return -1;
  if(sendcommand(tag, out, "DATA", sender))
    return -1;
  if((rc = getresponse(tag, in)) / 100 != 3)
    return -1;
  if(fprintf(out, "From: %s\r\n", pubsender) < 0
     || fprintf(out, "To: %s\r\n", recipient) < 0
     || fprintf(out, "Subject: %s\r\n", subject) < 0
     || fprintf(out, "Message-ID: <%s@%s>\r\n", id, local_hostname()) < 0
     || fprintf(out, "MIME-Version: 1.0\r\n") < 0
     || fprintf(out, "Content-Type: %s\r\n", content_type) < 0
     || fprintf(out, "Content-Transfer-Encoding: %s\r\n", encoding) < 0
     || fprintf(out, "\r\n") < 0) {
  write_error:
    error(errno, "%s: write error", tag);
    return -1;
  }
  for(ptr = body; *ptr; ++ptr) {
    if(sol && *ptr == '.')
      if(fputc('.', out) < 0)
	goto write_error;
    if(*ptr == '\n') {      
      if(fputc('\r', out) < 0)
	goto write_error;
      sol = 1;
    } else
      sol = 0;
    if(fputc(*ptr, out) < 0)
      goto write_error;
  }
  if(!sol)
    if(fputs("\r\n", out) < 0)
      goto write_error;
  if(fprintf(out, ".\r\n") < 0
     || fflush(out) < 0)
    goto write_error;
  if((rc = getresponse(tag, in)) / 100 != 2)
    return -1;
  return 0;
}

/** @brief Send a mail message
 * @param sender Sender address (can be "")
 * @param pubsender Visible sender address (must not be "")
 * @param recipient Recipient address
 * @param subject Subject string
 * @param encoding Body encoding
 * @param content_type Content-type of body
 * @param body Text of body (encoded, but \n for newline)
 * @return 0 on success, non-0 on error
 *
 * See mime_encode_text() for encoding of text bodies.
 */
int sendmail(const char *sender,
	     const char *pubsender,
	     const char *recipient,
	     const char *subject,
	     const char *encoding,
	     const char *content_type,
	     const char *body) {
  struct stringlist a;
  char *s[2];
  struct addrinfo *ai;
  char *tag;
  int fdin, fdout, rc;
  FILE *in, *out;
   
  static const struct addrinfo pref = {
    0,
    PF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
    0,
    0,
    0,
    0
  };

  /* Find the SMTP server */
  a.n = 2;
  a.s = s;
  s[0] = config->smtp_server;
  s[1] = (char *)"smtp";
  if(!(ai = get_address(&a, &pref, &tag)))
    return -1;
  fdin = xsocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if(connect(fdin, ai->ai_addr, ai->ai_addrlen) < 0) {
    error(errno, "error connecting to %s", tag);
    xclose(fdin);
    return -1;
  }
  if((fdout = dup(fdin)) < 0)
    fatal(errno, "error calling dup2");
  if(!(in = fdopen(fdin, "rb")))
    fatal(errno, "error calling fdopen");
  if(!(out = fdopen(fdout, "wb")))
    fatal(errno, "error calling fdopen");
  rc = sendmailfp(tag, in, out, sender, pubsender, recipient, subject,
		  encoding, content_type, body);
  fclose(in);
  fclose(out);
  return rc;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/