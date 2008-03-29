/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005, 2007, 2008 Richard Kettlewell
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
/** @file lib/vector.h @brief Dynamic array template */

#ifndef VECTOR_H
#define VECTOR_H

/** @brief Dynamic array template
 * @param NAME type name
 * @param ETYPE element type
 * @param REALLOC realloc function
 *
 * Defines @c struct @p NAME as a dynamic array with element type @p
 * ETYPE.  @p REALLOC should have the same signature as realloc() and
 * will be used for all memory allocation.  Typically it would be
 * xrealloc() for pointer-containing element types and
 * xrealloc_noptr() for pointer-free element types.
 *
 * Clients are inspected to read the @p vec member of the structure,
 * which points to the first element, and the @p nvec member, which is
 * the number of elements.  It is safe to reduce @p nvec.  Do not
 * touch any other members.
 *
 * The functions defined are:
 * - NAME_init(struct NAME *v) which initializes @p v
 * - NAME_append(struct NAME *v, ETYPE value) which appends @p value to @p v
 * - NAME_terminate(struct NAME *v) which zeroes out the element beyond the last
 */
#define VECTOR_TYPE(NAME,ETYPE,REALLOC)				\
								\
struct NAME {							\
  /** @brief Pointer to elements */ 				\
  ETYPE *vec;							\
  /** @brief Number of elements */				\
  int nvec;							\
  /** @brief Number of slots */					\
  int nslots;							\
};								\
								\
static inline void NAME##_init(struct NAME *v) {		\
  memset(v, 0, sizeof *v);					\
}								\
								\
static inline void NAME##_append(struct NAME *v, ETYPE val) {	\
  if(v->nvec >= v->nslots) {					\
    v->nslots = v->nslots ? 2 * v->nslots : 16;			\
    v->vec = REALLOC(v->vec, v->nslots * sizeof(ETYPE));	\
  }								\
  v->vec[v->nvec++] = val;					\
}								\
								\
static inline void NAME##_terminate(struct NAME *v) {		\
  if(v->nvec >= v->nslots)					\
    v->vec = REALLOC(v->vec, ++v->nslots * sizeof(ETYPE));	\
  memset(&v->vec[v->nvec], 0, sizeof (ETYPE));			\
}								\
								\
struct vector_swallow_semicolon

/** @brief A dynamic array of pointers to strings */
VECTOR_TYPE(vector, char *, xrealloc);
/** @brief A dynamic string */
VECTOR_TYPE(dynstr, char, xrealloc_noptr);
/** @brief A dynamic unicode string */
VECTOR_TYPE(dynstr_ucs4, uint32_t, xrealloc_noptr);
/** @brief A dynamic array of pointers to unicode string */
VECTOR_TYPE(vector32, uint32_t *, xrealloc);

/** @brief Append many strings to a @ref vector */
void vector_append_many(struct vector *v, char **vec, int nvec);

/** @brief Append @p n bytes to a @ref dynstr */
void dynstr_append_bytes(struct dynstr *v, const char *ptr, size_t n);

/** @brief Append a string to a @ref dynstr */
static inline void dynstr_append_string(struct dynstr *v, const char *ptr) {
  dynstr_append_bytes(v, ptr, strlen(ptr));
}

#endif /* VECTOR_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
