/*
    By John Walker written ages ago.

    Create a sparse file.

    Beat denial of service floggers to death by persuading
    them to download a HOW_BIG pseudo GIF file which is actually
    a holey file occupying trivial space on our server.

    Make:  make gigaslam
    Run:   ./gigaslam
    Output: a file named gigaslam.gif that contains something like
            16K bytes (i.e. 2-8K blocks), but appears to be 1GB in
            length because the second block is written at a 1GB
            address.

    Be careful what you do with this file as not all programs know
    how to deal with sparse files.

    Tweaked by Kern Sibbald, July 2007 to grow a file to a specified
    size.

*/

#ifdef __GNUC__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif

#include "bareos.h"
#include "lib/edit.h"

int main(int argc, char *argv[])
{
   off_t howBig;
   FILE *fp;

   if (argc != 3) {
      Pmsg0(0, "Calling sequence: grow <filename> <size>\n");
      exit(1);
   }
   howBig = str_to_int64(argv[2]);
   fp = fopen(argv[1], "r+");
   if (!fp) {
      berrno be;
      Pmsg2(0, "Could not open %s for write. ERR=%s\n", argv[1], be.bstrerror());
      exit(1);
   }
   char trailer[] = "xxxxxxx\n";

   fseeko(fp, howBig - strlen(trailer), SEEK_SET);
   fwrite(trailer, strlen(trailer), 1, fp);
   fclose(fp);
   return 0;
}
