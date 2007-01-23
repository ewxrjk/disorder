/*
 * This file is part of DisOrder
 * Copyright (C) 2004 Richard Kettlewell
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

#define NO_MEMORY_ALLOCATION
/* because byte_snprintf used from log.c */

#include <config.h>
#include "types.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>

#include "printf.h"
#include "sink.h"

enum flags {
  f_thousands = 1,
  f_left = 2,
  f_sign = 4,
  f_space = 8,
  f_hash = 16,
  f_zero = 32,
  f_width = 256,
  f_precision = 512
};

enum lengths {
  l_char = 1,
  l_short,
  l_long,
  l_longlong,
  l_size_t,
  l_intmax_t,
  l_ptrdiff_t,
  l_longdouble
};

struct conversion;

struct state {
  struct sink *output;
  int bytes;
  va_list *ap;
};

struct specifier {
  int ch;
  int (*check)(const struct conversion *c);
  int (*output)(struct state *s, struct conversion *c);
  int base;
  const char *digits;
  const char *xform;
};

struct conversion {
  unsigned flags;
  int width;
  int precision;
  int length;
  const struct specifier *specifier;
};

static const char flags[] = "'-+ #0";

/* write @nbytes@ to the output.  Return -1 on error, 0 on success.
 * Keeps track of the number of bytes written. */
static int do_write(struct state *s,
		    const void *buffer,
		    int nbytes) {
  if(s->bytes > INT_MAX - nbytes) {
#ifdef EOVERFLOW
    errno = EOVERFLOW;
#endif
    return -1;
  }
  if(s->output->write(s->output, buffer, nbytes) < 0) return -1;
  s->bytes += nbytes;
  return 0;
}

/* write character @ch@ @n@ times, reasonably efficiently */
static int do_pad(struct state *s, int ch, unsigned n) {
  unsigned t;
  const char *padding;
  
  switch(ch) {
  case ' ': padding = "                                "; break;
  case '0': padding = "00000000000000000000000000000000"; break;
  default: abort();
  }
  t = n / 32;
  n %= 32;
  while(t-- > 0)
    if(do_write(s, padding, 32) < 0) return -1;
  if(n > 0)
    if(do_write(s, padding, n) < 0) return -1;
  return 0;
}

/* pick up the integer at @ptr@, returning it via @intp@.  Return the
 * number of characters consumed.  Return 0 if there is no integer
 * there and -1 if an error occurred (e.g. too big) */
static int get_integer(int *intp, const char *ptr) {
  long n;
  char *e;

  errno = 0;
  n = strtol(ptr, &e, 10);
  if(errno || n > INT_MAX || n < INT_MIN || e == ptr) return -1;
  *intp = n;
  return e - ptr;
}

/* consistency checks for various conversion specifications */

static int check_integer(const struct conversion *c) {
  switch(c->length) {
  case 0:
  case l_char:
  case l_short:
  case l_long:
  case l_longlong:
  case l_intmax_t:
  case l_size_t:
  case l_longdouble:
    return 0;
  default:
    return -1;
  }
}

static int check_string(const struct conversion *c) {
  switch(c->length) {
  case 0:
    /* XXX don't support %ls, %lc */
    return 0;
  default:
    return -1;
  }
}

static int check_pointer(const struct conversion *c) {
  if(c->length) return -1;
  return 0;
}

static int check_percent(const struct conversion *c) {
  if(c->flags || c->width || c->precision || c->length) return -1;
  return 0;
}

/* output functions for various conversion specifications */

static int output_percent(struct state *s,
			  struct conversion attribute((unused)) *c) {
  return do_write(s, "%", 1);
}

static int output_integer(struct state *s, struct conversion *c) {
  uintmax_t u;
  intmax_t l;
  char sign;
  int base, dp, iszero, ndigits, prec, xform, sign_bytes, pad;
  char digits[CHAR_BIT * sizeof (uintmax_t)]; /* overestimate */

  switch(c->specifier->ch) {
  default:
    if(c->specifier->base < 0) {
      switch(c->length) {
      case 0: l = va_arg(*s->ap, int); break;
      case l_char: l = (signed char)va_arg(*s->ap, int); break;
      case l_short: l = (short)va_arg(*s->ap, int); break;
      case l_long: l = va_arg(*s->ap, long); break;
      case l_longlong: l = va_arg(*s->ap, long_long); break;
      case l_intmax_t: l = va_arg(*s->ap, intmax_t); break;
      case l_size_t: l = va_arg(*s->ap, ssize_t); break;
      case l_ptrdiff_t: l = va_arg(*s->ap, ptrdiff_t); break;
      default: abort();
      }
      base = -c->specifier->base;
      if(l < 0) {
	u = -l;
	sign = '-';
      } else {
	u = l;
	sign = 0;
      }
    } else {
      switch(c->length) {
      case 0: u = va_arg(*s->ap, unsigned int); break;
      case l_char: u = (unsigned char)va_arg(*s->ap, unsigned int); break;
      case l_short: u = (unsigned short)va_arg(*s->ap, unsigned int); break;
      case l_long: u = va_arg(*s->ap, unsigned long); break;
      case l_longlong: u = va_arg(*s->ap, u_long_long); break;
      case l_intmax_t: u = va_arg(*s->ap, uintmax_t); break;
      case l_size_t: u = va_arg(*s->ap, size_t); break;
      case l_ptrdiff_t: u = va_arg(*s->ap, ptrdiff_t); break;
      default: abort();
      }
      base = c->specifier->base;
      sign = 0;
    }
    break;
  case 'p':
    u = (uintptr_t)va_arg(*s->ap, void *);
    c->flags |= f_hash;
    base = c->specifier->base;
    sign = 0;
    break;
  }
  /* default precision */
  if(!(c->flags & f_precision))
    c->precision = 1;
  /* enforce sign */
  if((c->flags & f_sign) && !sign) sign = '+';
  /* compute the digits */
  iszero = (u == 0);
  dp = sizeof digits;
  while(u) {
    digits[--dp] = c->specifier->digits[u % base];
    u /= base;
  }
  ndigits = sizeof digits - dp;
  /* alternative form */
  if(c->flags & f_hash) {
    switch(base) {
    case 8:
      if((dp == sizeof digits || digits[dp] != '0')
	 && c->precision <= ndigits)
	c->precision = ndigits + 1;
      break;
    }
    if(!iszero && c->specifier->xform)
      xform = strlen(c->specifier->xform);
    else
      xform = 0;
  } else
    xform = 0;
  /* calculate number of 0s to add for precision */
  if(ndigits < c->precision)
    prec = c->precision - ndigits;
  else
    prec = 0;
  /* bytes occupied by the sign */
  if(sign)
    sign_bytes = 1;
  else
    sign_bytes = 0;
  /* XXX implement the ' ' flag */
  /* calculate number of bytes of padding */
  if(c->flags & f_width) {
    if((pad = c->width - (ndigits + prec + xform + sign_bytes)) < 0)
      pad = 0;
  } else
    pad = 0;
  /* now we are ready to output.  Possibilities are:
   * [space pad][sign][xform][0 prec]digits
   * [sign][xform][0 pad][0 prec]digits
   * [sign][xform][0 prec]digits[space pad]
   *
   * '-' beats '0'.
   */
  if(c->flags & f_left) {
    if(pad && do_pad(s, ' ', pad) < 0) return -1;
    if(sign && do_write(s, &sign, 1)) return -1;
    if(xform && do_write(s, c->specifier->xform, xform)) return -1;
    if(prec && do_pad(s, '0', prec) < 0) return -1;
    if(ndigits && do_write(s, digits + dp, ndigits)) return -1;
  } else if(c->flags & f_zero) {
    if(sign && do_write(s, &sign, 1)) return -1;
    if(xform && do_write(s, c->specifier->xform, xform)) return -1;
    if(pad && do_pad(s, '0', pad) < 0) return -1;
    if(prec && do_pad(s, '0', prec) < 0) return -1;
    if(ndigits && do_write(s, digits + dp, ndigits)) return -1;
  } else {
    if(sign && do_write(s, &sign, 1)) return -1;
    if(xform && do_write(s, c->specifier->xform, xform)) return -1;
    if(prec && do_pad(s, '0', prec) < 0) return -1;
    if(ndigits && do_write(s, digits + dp, ndigits)) return -1;
    if(pad && do_pad(s, ' ', pad) < 0) return -1;
  }
  return 0;
}

static int output_string(struct state *s, struct conversion *c) {
  const char *str, *n;
  int pad, len;

  str = va_arg(*s->ap, const char *);
  if(c->flags & f_precision) {
    if((n = memchr(str, 0, c->precision)))
      len = n - str;
    else
      len = c->precision;
  } else
    len = strlen(str);
  if(c->flags & f_width) {
    if((pad = c->width - len) < 0)
      pad = 0;
  } else
    pad = 0;
  if(c->flags & f_left) {
    if(pad && do_pad(s, ' ', pad) < 0) return -1;
    if(do_write(s, str, len) < 0) return -1;
  } else {
    if(do_write(s, str, len) < 0) return -1;
    if(pad && do_pad(s, ' ', pad) < 0) return -1;
  }
  return 0;
  
}

static int output_char(struct state *s, struct conversion *c) {
  int pad;
  char ch;

  ch = va_arg(*s->ap, int);
  if(c->flags & f_width) {
    if((pad = c->width - 1) < 0)
      pad = 0;
  } else
    pad = 0;
  if(c->flags & f_left) {
    if(pad && do_pad(s, ' ', pad) < 0) return -1;
    if(do_write(s, &ch, 1) < 0) return -1;
  } else {
    if(do_write(s, &ch, 1) < 0) return -1;
    if(pad && do_pad(s, ' ', pad) < 0) return -1;
  }
  return 0;
}

static int output_count(struct state *s, struct conversion *c) {
  switch(c->length) {
  case 0: *va_arg(*s->ap, int *) = s->bytes; break;
  case l_char: *va_arg(*s->ap, signed char *) = s->bytes; break;
  case l_short: *va_arg(*s->ap, short *) = s->bytes; break;
  case l_long: *va_arg(*s->ap, long *) = s->bytes; break;
  case l_longlong: *va_arg(*s->ap, long_long *) = s->bytes; break;
  case l_intmax_t: *va_arg(*s->ap, intmax_t *) = s->bytes; break;
  case l_size_t: *va_arg(*s->ap, ssize_t *) = s->bytes; break;
  case l_ptrdiff_t: *va_arg(*s->ap, ptrdiff_t *) = s->bytes; break;
  default: abort();
  }
  return 0;
}

/* table of conversion specifiers */
static const struct specifier specifiers[] = {
  /* XXX don't support floating point conversions */
  { '%', check_percent, output_percent,  0,  0,                  0    },
  { 'X', check_integer, output_integer,  16, "0123456789ABCDEF", "0X" },
  { 'c', check_string,  output_char,     0,  0,                  0    },
  { 'd', check_integer, output_integer, -10, "0123456789",       0    },
  { 'i', check_integer, output_integer, -10, "0123456789",       0    },
  { 'n', check_integer, output_count,    0,  0,                  0    },
  { 'o', check_integer, output_integer,  8,  "01234567",         0    },
  { 'p', check_pointer, output_integer,  16, "0123456789abcdef", "0x" },
  { 's', check_string,  output_string,   0,  0,                  0    },
  { 'u', check_integer, output_integer,  10, "0123456789",       0    },
  { 'x', check_integer, output_integer,  16, "0123456789abcdef", "0x" },
};

/* collect and check information about a conversion specification */
static int parse_conversion(struct conversion *c, const char *ptr) {
  int n, ch, l, r, m;
  const char *q, *start = ptr;
    
  memset(c, 0, sizeof *c);
  /* flags */
  while(*ptr && (q = strchr(flags, *ptr))) {
    c->flags |= (1 << (q - flags));
    ++ptr;
  }
  /* minimum field width */
  if(*ptr >= '0' && *ptr <= '9') {
    if((n = get_integer(&c->width, ptr)) < 0) return -1;
    ptr += n;
    c->flags |= f_width;
  } else if(*ptr == '*') {
    ++ptr;
    c->width = -1;
    c->flags |= f_width;
  }
  /* precision */
  if(*ptr == '.') {
    ++ptr;
    if(*ptr >= '0' && *ptr <= '9') {
      if((n = get_integer(&c->precision, ptr)) < 0) return -1;
      ptr += n;
    } else if(*ptr == '*') {
      ++ptr;
      c->precision = -1;
    } else
      return -1;
    c->flags |= f_precision;
  }
  /* length modifier */
  switch(ch = *ptr++) {
  case 'h':
    if((ch = *ptr++) == 'h') { c->length = l_char; ch = *ptr++; }
    else c->length = l_short;
    break;
  case 'l':
    if((ch = *ptr++) == 'l') { c->length = l_longlong; ch = *ptr++; }
    else c->length = l_long;
    break;
  case 'q': c->length = l_longlong; ch = *ptr++; break;
  case 'j': c->length = l_intmax_t; ch = *ptr++; break;
  case 'z': c->length = l_size_t; ch = *ptr++; break;
  case 't': c->length = l_ptrdiff_t; ch = *ptr++; break;
  case 'L': c->length = l_longdouble; ch = *ptr++; break;
  }
  /* conversion specifier */
  l = 0;
  r = sizeof specifiers / sizeof *specifiers;
  while(l <= r && (specifiers[m = (l + r) / 2].ch != ch))
    if(ch < specifiers[m].ch) r = m - 1;
    else l = m + 1;
  if(specifiers[m].ch != ch) return -1;
  if(specifiers[m].check(c)) return -1;
  c->specifier = &specifiers[m];
  return ptr - start;
}

/* ISO/IEC 9899:1999 7.19.6.1 */
/* http://www.opengroup.org/onlinepubs/009695399/functions/fprintf.html */

int byte_vsinkprintf(struct sink *output,
		     const char *fmt,
		     va_list ap) {
  int n;
  const char *ptr;
  struct state s;
  struct conversion c;

  memset(&s, 0, sizeof s);
  s.output = output;
  s.ap = &ap;
  while(*fmt) {
    /* output text up to next conversion specification */
    for(ptr = fmt; *fmt && *fmt != '%'; ++fmt)
      ;
    if((n = fmt - ptr))
      if(do_write(&s, ptr, n) < 0) return -1;
    if(!*fmt)
      break;
    ++fmt;
    /* parse conversion */
    if((n = parse_conversion(&c, fmt)) < 0) return -1;
    fmt += n;
    /* fill in width and precision */
    if((c.flags & f_width) && c.width == -1)
      if((c.width = va_arg(*s.ap, int)) < 0) {
	c.width = -c.width;
	c.flags |= f_left;
      }
    if((c.flags & f_precision) && c.precision == -1)
      if((c.precision = va_arg(*s.ap, int)) < 0)
	c.flags ^= f_precision;
    /* generate the output */
    if(c.specifier->output(&s, &c) < 0) return -1;
  }
  return s.bytes;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:e6ce806ce060f1d992eed14d9e5f0a6f */
