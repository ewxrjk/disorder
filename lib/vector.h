/*
 * This file is part of DisOrder.
 * Copyright (C) 2004, 2005 Richard Kettlewell
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

#ifndef VECTOR_H
#define VECTOR_H

#define VECTOR_TYPE(NAME,ETYPE,REALLOC)				\
								\
struct NAME {							\
  ETYPE *vec;							\
  int nvec, nslots;						\
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

VECTOR_TYPE(vector, char *, xrealloc)
VECTOR_TYPE(dynstr, char, xrealloc_noptr)
VECTOR_TYPE(dynstr_ucs4, uint32_t, xrealloc_noptr)

void vector_append_many(struct vector *v, char **vec, int nvec);

void dynstr_append_bytes(struct dynstr *v, const char *ptr, size_t n);

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
/* arch-tag:a57474a59d7ebd67cda96bf05bd89049 */
