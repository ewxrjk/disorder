/* Grotty program to print out the bytes making up filenames in some
 * directory */

#include "config.h"

#include <dirent.h>
#include <stdio.h>
#include <ctype.h>

int main(int attribute((unused)) argc, char **argv) {
  DIR *dp;
  struct dirent *de;
  int n;
  
  if(!(dp = opendir(argv[1]))) return -1;
  while((de = readdir(dp))) {
    for(n = 0; de->d_name[n]; ++n) {
      printf("%02x", (unsigned char)de->d_name[n]);
      if(n) putchar(' ');
    }
    putchar('\n');
    for(n = 0; de->d_name[n]; ++n) {
      if(isprint((unsigned char)de->d_name[n]))
	printf(" %c", (unsigned char)de->d_name[n]);
      else
	printf("  ");
      if(n) putchar(' ');
    }
    putchar('\n');
  }
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
