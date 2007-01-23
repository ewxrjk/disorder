#ifndef SPLIT_H
#define SPLIT_H

#define SPLIT_COMMENTS	0001		/* # starts a comment */
#define SPLIT_QUOTES	0002		/* " and ' quote strings */

char **split(const char *s,
	     int *np,
	     unsigned flags,
	     void (*error_handler)(const char *msg, void *u),
	     void *u);
/* split @s@ up into fields.  Return a null-pointer-terminated array
 * of pointers to the fields.  If @np@ is not a null pointer store the
 * number of fields there.  Calls @error_handler@ to report any
 * errors.
 *
 * split() operates on UTF-8 strings.
 */

const char *quoteutf8(const char *s);
/* quote a UTF-8 string.  Might return @s@ if no quoting is required.  */

#endif /* SPLIT_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
End:
*/
/* arch-tag:b1e012fb5925c578672b122a3fc685cb */
