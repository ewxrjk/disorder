#ifndef STRPTIME_H
#define STRPTIME_H

#include <time.h>

char *my_strptime(const char *buf,
                  const char *format,
                  struct tm *tm);

#endif /* STRPTIME_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
