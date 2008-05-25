/*
 * This file is part of DisOrder.
 * Copyright (C) 2007, 2008 Richard Kettlewell
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
/** @file lib/heap.h @brief Binary heap template */

#ifndef HEAP_H
#define HEAP_H

#include "vector.h"

/** @brief Binary heap template.
 * @param NAME name of type to define
 * @param ETYPE element type
 * @param LT comparison function
 *
 * Defines a heap type called @c struct @p NAME and a number of functions to
 * operate on it.
 *
 * The element type of the heap will be @p ETYPE.
 *
 * @p LT will be called with two arguments of type @p ETYPE, and
 * implements a less-than comparison.
 *
 * The functions defined are:
 * - NAME_init(h) which initializes an empty heap at @p h
 * - NAME_count(h) which returns the number of elements in the heap
 * - NAME_insert(h, e) which inserts @p e into @p h
 * - NAME_first(g) which returns the least element of @p h
 * - NAME_remove(g) which removes and returns the least element of @p h
 *
 * The heap is implemented as a vector.  Element 0 is the root.  For any
 * element \f$i\f$, its children are elements \f$2i+1\f$ and \f$2i+2\f$ and
 * consequently its parent (if it is not the root) is
 * \f$\lfloor(i-1)/2\rfloor\f$.
 * 
 * The insert and remove operations maintain two invariants: the @b
 * shape property (all levels of the tree are fully filled except the
 * deepest, and that is filled from the left), and the @b heap
 * property, that every element compares less than or equal to its
 * children.
 *
 * The shape property implies that the array representation has no gaps, which
 * is convenient.  It is preserved by only adding or removing the final element
 * of the array and otherwise only modifying the array by swapping pairs of
 * elements.
 *
 * @b Insertion works by inserting the new element \f$N\f$ at the end and
 * bubbling it up the tree until it is in the right order for its branch.
 * - If, for its parent \f$P\f$, \f$P \le N\f$ then it is already in the right
 * place and the insertion is complete.
 * - Otherwise \f$P > N\f$ and so \f$P\f$ and \f$N\f$ are exchanged.  If
 * \f$P\f$ has a second child, \f$C\f$, then \f$N < P < C\f$ so the heap
 * property is now satisfied from \f$P\f$ down.
 *
 * @b Removal works by first swapping the root with the final element (and then
 * removing it) and then bubbling the new root \f$N\f$ down the tree until it
 * finds its proper place.  At each stage it is compared with its children
 * \f$A\f$ and \f$B\f$.
 * - If \f$N \le A\f$ and \f$N \le B\f$ then it is in the
 * right place already.
 * - Otherwise \f$N > A\f$ or \f$N > B\f$ (or both).  WLOG \f$A \le B\f$.
 * \f$N\f$ and \f$A\f$ are exchanged, so now \f$A\f$ has children \f$N\f$ and
 * \f$B\f$.  \f$A < N\f$ and \f$A \le B\f$.
 */
#define HEAP_TYPE(NAME, ETYPE, LT)                                      \
  typedef ETYPE NAME##_element;                                         \
  VECTOR_TYPE(NAME, NAME##_element, xrealloc);                          \
                                                                        \
  static inline int NAME##_count(struct NAME *heap) {                   \
    return heap->nvec;                                                  \
  }                                                                     \
                                                                        \
  static inline NAME##_element NAME##_first(struct NAME *heap) {        \
    assert(heap->nvec > 0 && "_first");                                 \
    return heap->vec[0];                                                \
  }                                                                     \
                                                                        \
  void NAME##_insert(struct NAME *heap, NAME##_element elt);            \
  NAME##_element NAME##_remove(struct NAME *heap);                      \
                                                                        \
  struct heap_swallow_semicolon

/** @brief External-linkage definitions for @ref HEAP_TYPE */
#define HEAP_DEFINE(NAME, ETYPE, LT)                            \
  void NAME##_insert(struct NAME *heap, NAME##_element elt) {   \
    int n = heap->nvec;                                         \
    NAME##_append(heap, elt);                                   \
    while(n > 0) {                                              \
      const int p = (n-1)/2;                                    \
      if(!LT(heap->vec[n],heap->vec[p]))                        \
        break;                                                  \
      else {                                                    \
        const NAME##_element t = heap->vec[n];                  \
        heap->vec[n] = heap->vec[p];                            \
        heap->vec[p] = t;                                       \
        n = p;                                                  \
      }                                                         \
    }                                                           \
  }                                                             \
                                                                \
  NAME##_element NAME##_remove(struct NAME *heap) {             \
    int n = 0;                                                  \
    NAME##_element r;                                           \
                                                                \
    assert(heap->nvec > 0 && "_remove");                        \
    r = heap->vec[0];                                           \
    heap->vec[0] = heap->vec[--heap->nvec];                     \
    while(2 * n + 1 < heap->nvec) {                             \
      int a = 2 * n + 1;                                        \
      int b = 2 * n + 2;                                        \
                                                                \
      if(b < heap->nvec && LT(heap->vec[b],heap->vec[a])) {     \
        ++a;                                                    \
        --b;                                                    \
      }                                                         \
      if(LT(heap->vec[a], heap->vec[n])) {                      \
        const NAME##_element t = heap->vec[n];                  \
        heap->vec[n] = heap->vec[a];                            \
        heap->vec[a] = t;                                       \
        n = a;                                                  \
      } else                                                    \
        break;                                                  \
    }                                                           \
    return r;                                                   \
  }                                                             \
                                                                \
  struct heap_swallow_semicolon                                 \
  

#endif /* PQUEUE_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
