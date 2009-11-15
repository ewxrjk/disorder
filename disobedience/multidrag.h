/*
 * Copyright (C) 2009 Richard Kettlewell
 *
 * Note that this license ONLY applies to multidrag.c and multidrag.h, not to
 * the rest of DisOrder.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/** @file disobedience/multidrag.h
 * @brief Drag multiple rows of a GtkTreeView
 */
#ifndef MULTIDRAG_H
#define MULTIDRAG_H

/** @brief Predicate type for rows to drag
 * @param path Path to row
 * @param iter Iterator pointing at row
 * @return TRUE if row is draggable else FALSE
 */
typedef gboolean multidrag_row_predicate(GtkTreePath *path,
					 GtkTreeIter *iter);

void make_treeview_multidrag(GtkWidget *w,
			     multidrag_row_predicate *predicate);

#endif /* MULTIDRAG_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
