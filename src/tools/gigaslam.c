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
    
*/

#define HOW_BIG   1000000000ll

#ifdef __GNUC__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *const *argv)
{
    FILE *fp = fopen("gigaslam.gif", "w");
    char header[] = "<html>\n<table>\n<tr><td>\n";
    char trailer[] = "</html>\n";
    off_t howBig = HOW_BIG;
    
    fwrite(header, sizeof header, 1, fp);
    fseeko(fp, howBig - strlen(trailer), 0);
    fwrite(trailer, strlen(trailer), 1, fp);
    fclose(fp);
    return 0;
    
}
