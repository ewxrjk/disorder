/** @file server/endian.c
 * @brief Expose runtime endianness to makefile for testing
 */
#include <config.h>
#include <stdio.h>

int main(void) {
#if WORDS_BIGENDIAN
  puts("1");
#else
  puts("0");
#endif
  return 0;
}

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
