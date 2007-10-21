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
/** @file lib/html.c
 * @brief Noddy HTML parser
 */

#ifndef HTML_H
#define HTML_H

/** @brief HTML parser callbacks */
struct html_parser_callbacks {
  /** @brief Called for an open tag
   * @param tag Name of tag, normalized to lower case
   * @param attrs Hash containing attributes
   * @param u User data pointer
   */
  void (*open)(const char *tag,
	       hash *attrs,
	       void *u);

  /** @brief Called for a close tag
   * @param tag Name of tag, normalized to lower case
   * @param u User data pointer
   */
  void (*close)(const char *tag,
		void *u);

  /** @brief Called for text
   * @param text Pointer to text
   * @param u User data pointer
   */
  void (*text)(const char *text,
	       void *u);
};

int html_parse(const struct html_parser_callbacks *callbacks,
	       const char *input,
	       void *u);

#endif /* HTML_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
