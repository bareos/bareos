#include <windows.h>
#include <stdio.h>

int
main(int argc, const char ** argv)
{
   int   nsecs;

   if (argc != 2)
   {
      fputs("usage: bsleep <n>\n    n = number of seconds\n", stderr);
      exit(1);
   }

   if (sscanf(argv[1], "%d", &nsecs) != 1)
   {
      fputs("sleep: incorrect argument, must be number of seconds to sleep\n", stderr);
      exit(1);
   }

   Sleep(nsecs * 1000);
   exit(0);
}
