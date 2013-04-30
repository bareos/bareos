/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   scan.c -- scanning routines for Bacula
 *
 *    Kern Sibbald, MM  separated from util.c MMIII
 *
 */

#include "bacula.h"
#include "jcr.h"
#include "findlib/find.h"

/*
 * Strip leading space from command line arguments
 */
void strip_leading_space(char *str)
{
   char *p = str;

   while (B_ISSPACE(*p)) {
      p++;
   }
   if (p != str) {
      strcpy(str, p);
   }
}

/*
 * Strip any trailing junk from the command
 */
void strip_trailing_junk(char *cmd)
{
   char *p;

   /*
    * Strip trailing junk from command
    */
   p = cmd + strlen(cmd) - 1;
   while ((p >= cmd) && (*p == '\n' || *p == '\r' || *p == ' ')) {
      *p-- = 0;
   }
}

/*
 * Strip any trailing newline characters from the string
 */
void strip_trailing_newline(char *cmd)
{
   char *p;

   p = cmd + strlen(cmd) - 1;
   while ((p >= cmd) && (*p == '\n' || *p == '\r')) {
      *p-- = 0;
   }
}

/*
 * Strip any trailing slashes from a directory path
 */
void strip_trailing_slashes(char *dir)
{
   char *p;

   /*
    * Strip trailing slashes
    */
   p = dir + strlen(dir) - 1;
   while (p >= dir && IsPathSeparator(*p)) {
      *p-- = 0;
   }
}

/*
 * Skip spaces
 *  Returns: 0 on failure (EOF)
 *           1 on success
 *           new address in passed parameter
 */
bool skip_spaces(char **msg)
{
   char *p = *msg;
   if (!p) {
      return false;
   }
   while (*p && B_ISSPACE(*p)) {
      p++;
   }
   *msg = p;
   return *p ? true : false;
}

/*
 * Skip nonspaces
 *  Returns: 0 on failure (EOF)
 *           1 on success
 *           new address in passed parameter
 */
bool skip_nonspaces(char **msg)
{
   char *p = *msg;

   if (!p) {
      return false;
   }
   while (*p && !B_ISSPACE(*p)) {
      p++;
   }
   *msg = p;
   return *p ? true : false;
}

/*
 * Folded search for string - case insensitive
 */
int fstrsch(const char *a, const char *b)   /* folded case search */
{
   const char *s1,*s2;
   char c1, c2;

   s1=a;
   s2=b;
   while (*s1) {                      /* do it the fast way */
      if ((*s1++ | 0x20) != (*s2++ | 0x20))
         return 0;                    /* failed */
   }
   while (*a) {                       /* do it over the correct slow way */
      if (B_ISUPPER(c1 = *a)) {
         c1 = tolower((int)c1);
      }
      if (B_ISUPPER(c2 = *b)) {
         c2 = tolower((int)c2);
      }
      if (c1 != c2) {
         return 0;
      }
      a++;
      b++;
   }
   return 1;
}

/*
 * Return next argument from command line.  Note, this
 *   routine is destructive because it stored 0 at the end
 *   of each argument.
 * Called with pointer to pointer to command line. This
 *   pointer is updated to point to the remainder of the
 *   command line.
 * 
 * Returns pointer to next argument -- don't store the result
 *   in the pointer you passed as an argument ...
 *   The next argument is terminated by a space unless within
 *   quotes. Double quote characters (unless preceded by a \) are
 *   stripped.
 *   
 */
char *next_arg(char **s)
{
   char *p, *q, *n;
   bool in_quote = false;

   /* skip past spaces to next arg */
   for (p=*s; *p && B_ISSPACE(*p); ) {
      p++;
   }
   Dmsg1(900, "Next arg=%s\n", p);
   for (n = q = p; *p ; ) {
      if (*p == '\\') {                 /* slash? */
         p++;                           /* yes, skip it */
         if (*p) {
            *q++ = *p++;
         } else {
            *q++ = *p;
         }
         continue;
      }
      if (*p == '"') {                  /* start or end of quote */
         p++;
         in_quote = !in_quote;          /* change state */
         continue;
      }
      if (!in_quote && B_ISSPACE(*p)) { /* end of field */
         p++;
         break;
      }
      *q++ = *p++;
   }
   *q = 0;
   *s = p;
   Dmsg2(900, "End arg=%s next=%s\n", n, p);
   return n;
}

/*
 * This routine parses the input command line.
 * It makes a copy in args, then builds an
 *  argc, argk, argv list where:
 *
 *  argc = count of arguments
 *  argk[i] = argument keyword (part preceding =)
 *  argv[i] = argument value (part after =)
 *
 *  example:  arg1 arg2=abc arg3=
 *
 *  argc = c
 *  argk[0] = arg1
 *  argv[0] = NULL
 *  argk[1] = arg2
 *  argv[1] = abc
 *  argk[2] = arg3
 *  argv[2] =
 */
int parse_args(POOLMEM *cmd, POOLMEM **args, int *argc,
               char **argk, char **argv, int max_args)
{
   char *p;

   parse_args_only(cmd, args, argc, argk, argv, max_args);

   /* Separate keyword and value */
   for (int i=0; i < *argc; i++) {
      p = strchr(argk[i], '=');
      if (p) {
         *p++ = 0;                    /* terminate keyword and point to value */
      }
      argv[i] = p;                    /* save ptr to value or NULL */
   }
#ifdef xxx_debug
   for (int i=0; i < *argc; i++) {
      Pmsg3(000, "Arg %d: kw=%s val=%s\n", i, argk[i], argv[i]?argv[i]:"NULL");
   }
#endif
   return 1;
}


/*
 * This routine parses the input command line.
 *   It makes a copy in args, then builds an
 *   argc, argk, but no argv (values).
 *   This routine is useful for scanning command lines where the data 
 *   is a filename and no keywords are expected.  If we scan a filename
 *   for keywords, any = in the filename will be interpreted as the
 *   end of a keyword, and this is not good.
 *
 *  argc = count of arguments
 *  argk[i] = argument keyword (part preceding =)
 *  argv[i] = NULL                         
 *
 *  example:  arg1 arg2=abc arg3=
 *
 *  argc = c
 *  argk[0] = arg1
 *  argv[0] = NULL
 *  argk[1] = arg2=abc
 *  argv[1] = NULL
 *  argk[2] = arg3
 *  argv[2] =
 */
int parse_args_only(POOLMEM *cmd, POOLMEM **args, int *argc,
                    char **argk, char **argv, int max_args)
{
   char *p, *n;

   pm_strcpy(args, cmd);
   strip_trailing_junk(*args);
   p = *args;
   *argc = 0;
   /*
    * Pick up all arguments
    */
   while (*argc < max_args) {
      n = next_arg(&p);
      if (*n) {
         argk[*argc] = n;
         argv[(*argc)++] = NULL;
      } else {
         break;
      }
   }
   return 1;
}

/*
 * Given a full filename, split it into its path
 *  and filename parts. They are returned in pool memory
 *  in the arguments provided.
 */
void split_path_and_filename(const char *fname, POOLMEM **path, int *pnl,
                             POOLMEM **file, int *fnl)
{
   const char *f;
   int slen;
   int len = slen = strlen(fname);

   /*
    * Find path without the filename.
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   f = fname + len - 1;
   /* "strip" any trailing slashes */
   while (slen > 1 && IsPathSeparator(*f)) {
      slen--;
      f--;
   }
   /* Walk back to last slash -- begin of filename */
   while (slen > 0 && !IsPathSeparator(*f)) {
      slen--;
      f--;
   }
   if (IsPathSeparator(*f)) {         /* did we find a slash? */
      f++;                            /* yes, point to filename */
   } else {                           /* no, whole thing must be path name */
      f = fname;
   }
   Dmsg2(200, "after strip len=%d f=%s\n", len, f);
   *fnl = fname - f + len;
   if (*fnl > 0) {
      *file = check_pool_memory_size(*file, *fnl+1);
      memcpy(*file, f, *fnl);    /* copy filename */
   }
   (*file)[*fnl] = 0;

   *pnl = f - fname;
   if (*pnl > 0) {
      *path = check_pool_memory_size(*path, *pnl+1);
      memcpy(*path, fname, *pnl);
   }
   (*path)[*pnl] = 0;

   Dmsg2(200, "pnl=%d fnl=%d\n", *pnl, *fnl);
   Dmsg3(200, "split fname=%s path=%s file=%s\n", fname, *path, *file);
}

/*
 * Extremely simple sscanf. Handles only %(u,d,ld,qd,qu,lu,lld,llu,c,nns)
 */
const int BIG = 1000;
int bsscanf(const char *buf, const char *fmt, ...)
{
   va_list ap;
   int count = 0;
   void *vp;
   char *cp;
   int l = 0;
   int max_len = BIG;
   uint64_t value;
   bool error = false;
   bool negative;

   va_start(ap, fmt);
   while (*fmt && !error) {
//    Dmsg1(000, "fmt=%c\n", *fmt);
      if (*fmt == '%') {
         fmt++;
//       Dmsg1(000, "Got %% nxt=%c\n", *fmt);
switch_top:
         switch (*fmt++) {
         case 'u':
            value = 0;
            while (B_ISDIGIT(*buf)) {
               value = B_TIMES10(value) + *buf++ - '0';
            }
            vp = (void *)va_arg(ap, void *);
//          Dmsg2(000, "val=%lld at 0x%lx\n", value, (long unsigned)vp);
            if (l == 0) {
               *((int *)vp) = (int)value;
            } else if (l == 1) {
               *((uint32_t *)vp) = (uint32_t)value;
//             Dmsg0(000, "Store 32 bit int\n");
            } else {
               *((uint64_t *)vp) = (uint64_t)value;
//             Dmsg0(000, "Store 64 bit int\n");
            }
            count++;
            l = 0;
            break;
         case 'd':
            value = 0;
            if (*buf == '-') {
               negative = true;
               buf++;
            } else {
               negative = false;
            }
            while (B_ISDIGIT(*buf)) {
               value = B_TIMES10(value) + *buf++ - '0';
            }
            if (negative) {
               value = -value;
            }
            vp = (void *)va_arg(ap, void *);
//          Dmsg2(000, "val=%lld at 0x%lx\n", value, (long unsigned)vp);
            if (l == 0) {
               *((int *)vp) = (int)value;
            } else if (l == 1) {
               *((int32_t *)vp) = (int32_t)value;
//             Dmsg0(000, "Store 32 bit int\n");
            } else {
               *((int64_t *)vp) = (int64_t)value;
//             Dmsg0(000, "Store 64 bit int\n");
            }
            count++;
            l = 0;
            break;
         case 'l':
//          Dmsg0(000, "got l\n");
            l = 1;
            if (*fmt == 'l') {
               l++;
               fmt++;
            }
            if (*fmt == 'd' || *fmt == 'u') {
               goto switch_top;
            }
//          Dmsg1(000, "fmt=%c !=d,u\n", *fmt);
            error = true;
            break;
         case 'q':
            l = 2;
            if (*fmt == 'd' || *fmt == 'u') {
               goto switch_top;
            }
//          Dmsg1(000, "fmt=%c !=d,u\n", *fmt);
            error = true;
            break;
         case 's':
//          Dmsg1(000, "Store string max_len=%d\n", max_len);
            cp = (char *)va_arg(ap, char *);
            while (*buf && !B_ISSPACE(*buf) && max_len-- > 0) {
               *cp++ = *buf++;
            }
            *cp = 0;
            count++;
            max_len = BIG;
            break;
         case 'c':
            cp = (char *)va_arg(ap, char *);
            *cp = *buf++;
            count++;
            break;
         case '%':
            if (*buf++ != '%') {
               error = true;
            }
            break;
         default:
            fmt--;
            max_len = 0;
            while (B_ISDIGIT(*fmt)) {
               max_len = B_TIMES10(max_len) + *fmt++ - '0';
            }
//          Dmsg1(000, "Default max_len=%d\n", max_len);
            if (*fmt == 's') {
               goto switch_top;
            }
//          Dmsg1(000, "Default c=%c\n", *fmt);
            error = true;
            break;                    /* error: unknown format */
         }
         continue;

      /* White space eats zero or more whitespace */
      } else if (B_ISSPACE(*fmt)) {
         fmt++;
         while (B_ISSPACE(*buf)) {
            buf++;
         }
      /* Plain text must match */
      } else if (*buf++ != *fmt++) {
//       Dmsg2(000, "Mismatch buf=%c fmt=%c\n", *--buf, *--fmt);
         error = true;
         break;
      }
   }
   va_end(ap);
// Dmsg2(000, "Error=%d count=%d\n", error, count);
   if (error) {
      count = -1;
   }
   return count;
}

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
   char buf[100];
   uint32_t val32;
   uint64_t val64;
   uint32_t FirstIndex, LastIndex, StartFile, EndFile, StartBlock, EndBlock;
   char Job[200];
   int cnt;
   char *helloreq= "Hello *UserAgent* calling\n";
   char *hello = "Hello %127s calling\n";
   char *catreq =
"CatReq Job=NightlySave.2004-06-11_19.11.32 CreateJobMedia FirstIndex=1 LastIndex=114 StartFile=0 EndFile=0 StartBlock=208 EndBlock=2903248";
static char Create_job_media[] = "CatReq Job=%127s CreateJobMedia "
  "FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u "
  "StartBlock=%u EndBlock=%u\n";
static char OK_media[] = "1000 OK VolName=%127s VolJobs=%u VolFiles=%u"
   " VolBlocks=%u VolBytes=%" lld " VolMounts=%u VolErrors=%u VolWrites=%u"
   " MaxVolBytes=%" lld " VolCapacityBytes=%" lld " VolStatus=%20s"
   " Slot=%d MaxVolJobs=%u MaxVolFiles=%u InChanger=%d"
   " VolReadTime=%" lld " VolWriteTime=%" lld;
   char *media =
"1000 OK VolName=TestVolume001 VolJobs=0 VolFiles=0 VolBlocks=0 VolBytes=1 VolMounts=0 VolErrors=0 VolWrites=0 MaxVolBytes=0 VolCapacityBytes=0 VolStatus=Append Slot=0 MaxVolJobs=0 MaxVolFiles=0 InChanger=1 VolReadTime=0 VolWriteTime=0";
struct VOLUME_CAT_INFO {
   /* Media info for the current Volume */
   uint32_t VolCatJobs;               /* number of jobs on this Volume */
   uint32_t VolCatFiles;              /* Number of files */
   uint32_t VolCatBlocks;             /* Number of blocks */
   uint64_t VolCatBytes;              /* Number of bytes written */
   uint32_t VolCatMounts;             /* Number of mounts this volume */
   uint32_t VolCatErrors;             /* Number of errors this volume */
   uint32_t VolCatWrites;             /* Number of writes this volume */
   uint32_t VolCatReads;              /* Number of reads this volume */
   uint64_t VolCatRBytes;             /* Number of bytes read */
   uint32_t VolCatRecycles;           /* Number of recycles this volume */
   int32_t  Slot;                     /* Slot in changer */
   bool     InChanger;                /* Set if vol in current magazine */
   uint32_t VolCatMaxJobs;            /* Maximum Jobs to write to volume */
   uint32_t VolCatMaxFiles;           /* Maximum files to write to volume */
   uint64_t VolCatMaxBytes;           /* Max bytes to write to volume */
   uint64_t VolCatCapacityBytes;      /* capacity estimate */
   uint64_t VolReadTime;              /* time spent reading */
   uint64_t VolWriteTime;             /* time spent writing this Volume */
   char VolCatStatus[20];             /* Volume status */
   char VolCatName[MAX_NAME_LENGTH];  /* Desired volume to mount */
};
   struct VOLUME_CAT_INFO vol;

#ifdef xxx
   bsscanf("Hello_world 123 1234", "%120s %ld %lld", buf, &val32, &val64);
   printf("%s %d %lld\n", buf, val32, val64);

   *Job=0;
   cnt = bsscanf(catreq, Create_job_media, &Job,
      &FirstIndex, &LastIndex, &StartFile, &EndFile,
      &StartBlock, &EndBlock);
   printf("cnt=%d Job=%s\n", cnt, Job);
   cnt = bsscanf(helloreq, hello, &Job);
   printf("cnt=%d Agent=%s\n", cnt, Job);
#endif
   cnt = bsscanf(media, OK_media,
               vol.VolCatName,
               &vol.VolCatJobs, &vol.VolCatFiles,
               &vol.VolCatBlocks, &vol.VolCatBytes,
               &vol.VolCatMounts, &vol.VolCatErrors,
               &vol.VolCatWrites, &vol.VolCatMaxBytes,
               &vol.VolCatCapacityBytes, vol.VolCatStatus,
               &vol.Slot, &vol.VolCatMaxJobs, &vol.VolCatMaxFiles,
               &vol.InChanger, &vol.VolReadTime, &vol.VolWriteTime);
   printf("cnt=%d Vol=%s\n", cnt, vol.VolCatName);

}

#endif
