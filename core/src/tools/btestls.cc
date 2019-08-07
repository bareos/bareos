/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Test program for listing files during regression testing
 * Links have their permissions and time bashed since they cannot
 * be set by Bareos.
 *
 * Kern Sibbald, MM
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "findlib/find.h"
#include "filed/fd_plugins.h"
#include "findlib/match.h"
#include "lib/edit.h"
#include "lib/recent_job_results_list.h"
#include "lib/util.h"

/* Dummy functions */
int GenerateJobEvent(JobControlRecord* jcr, const char* event) { return 1; }
void GeneratePluginEvent(JobControlRecord* jcr,
                         filedaemon::bEventType eventType,
                         void* value)
{
}

/* Global variables */
int attrs = 0;

static JobControlRecord* jcr;
static int num_files = 0;

static int PrintFile(JobControlRecord* jcr, FindFilesPacket* ff, bool);
static void PrintLsOutput(char* fname,
                          char* link,
                          int type,
                          struct stat* statp);
static int CountFiles(JobControlRecord* jcr,
                      FindFilesPacket* ff,
                      bool top_level);

static void usage()
{
  fprintf(stderr,
          _("\n"
            "Usage: btestls [-d debug_level] [-] [pattern1 ...]\n"
            "       -a          print extended attributes (Win32 debug)\n"
            "       -d <nn>     set debug level to <nn>\n"
            "       -dt         print timestamp in debug output\n"
            "       -e          specify file of exclude patterns\n"
            "       -i          specify file of include patterns\n"
            "       -q          quiet, don't print filenames (debug)\n"
            "       -           read pattern(s) from stdin\n"
            "       -?          print this message.\n"
            "\n"
            "Patterns are file inclusion -- normally directories.\n"
            "Debug level >= 1 prints each file found.\n"
            "Debug level >= 10 prints path/file for catalog.\n"
            "Errors always printed.\n"
            "Files/paths truncated is number with len > 255.\n"
            "Truncation is only in catalog.\n"
            "\n"));

  exit(1);
}


int main(int argc, char* const* argv)
{
  FindFilesPacket* ff;
  char name[1000];
  bool quiet = false;
  int i, ch;
  char* inc = NULL;
  char* exc = NULL;
  FILE* fd;

  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  while ((ch = getopt(argc, argv, "ad:e:i:q?")) != -1) {
    switch (ch) {
      case 'a': /* print extended attributes *debug* */
        attrs = 1;
        break;

      case 'd': /* set debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 'e': /* exclude patterns */
        exc = optarg;
        break;

      case 'i': /* include patterns */
        inc = optarg;
        break;

      case 'q':
        quiet = true;
        break;

      case '?':
      default:
        usage();
    }
  }
  argc -= optind;
  argv += optind;

  jcr = new_jcr(sizeof(JobControlRecord), NULL);

  ff = init_find_files();
  if (argc == 0 && !inc) {
    AddFnameToIncludeList(ff, 0, "/"); /* default to / */
  } else {
    for (i = 0; i < argc; i++) {
      if (bstrcmp(argv[i], "-")) {
        while (fgets(name, sizeof(name) - 1, stdin)) {
          StripTrailingJunk(name);
          AddFnameToIncludeList(ff, 0, name);
        }
        continue;
      }
      AddFnameToIncludeList(ff, 0, argv[i]);
    }
  }
  if (inc) {
    fd = fopen(inc, "rb");
    if (!fd) {
      printf(_("Could not open include file: %s\n"), inc);
      exit(1);
    }
    while (fgets(name, sizeof(name) - 1, fd)) {
      StripTrailingJunk(name);
      AddFnameToIncludeList(ff, 0, name);
    }
    fclose(fd);
  }

  if (exc) {
    fd = fopen(exc, "rb");
    if (!fd) {
      printf(_("Could not open exclude file: %s\n"), exc);
      exit(1);
    }
    while (fgets(name, sizeof(name) - 1, fd)) {
      StripTrailingJunk(name);
      AddFnameToExcludeList(ff, name);
    }
    fclose(fd);
  }
  if (quiet) {
    MatchFiles(jcr, ff, CountFiles);
  } else {
    MatchFiles(jcr, ff, PrintFile);
  }
  printf(_("Files seen = %d\n"), num_files);
  TermIncludeExcludeFiles(ff);
  TermFindFiles(ff);

  FreeJcr(jcr);
  TermLastJobsList();
  CleanupJcrChain();
  CloseMemoryPool();
  exit(0);
}

static int CountFiles(JobControlRecord* jcr,
                      FindFilesPacket* ff,
                      bool top_level)
{
  num_files++;
  return 1;
}

static int PrintFile(JobControlRecord* jcr, FindFilesPacket* ff, bool top_level)
{
  switch (ff->type) {
    case FT_LNKSAVED:
    case FT_REGE:
    case FT_REG:
    case FT_LNK:
    case FT_DIREND:
    case FT_SPEC:
      PrintLsOutput(ff->fname, ff->link, ff->type, &ff->statp);
      break;
    case FT_DIRBEGIN:
      break;
    case FT_NOACCESS:
      printf(_("Err: Could not access %s: %s\n"), ff->fname, strerror(errno));
      break;
    case FT_NOFOLLOW:
      printf(_("Err: Could not follow ff->link %s: %s\n"), ff->fname,
             strerror(errno));
      break;
    case FT_NOSTAT:
      printf(_("Err: Could not stat %s: %s\n"), ff->fname, strerror(errno));
      break;
    case FT_NOCHG:
      printf(_("Skip: File not saved. No change. %s\n"), ff->fname);
      break;
    case FT_ISARCH:
      printf(_("Err: Attempt to backup archive. Not saved. %s\n"), ff->fname);
      break;
    case FT_NORECURSE:
      printf(_("Recursion turned off. Directory not entered. %s\n"), ff->fname);
      break;
    case FT_NOFSCHG:
      printf(
          _("Skip: File system change prohibited. Directory not entered. %s\n"),
          ff->fname);
      break;
    case FT_NOOPEN:
      printf(_("Err: Could not open directory %s: %s\n"), ff->fname,
             strerror(errno));
      break;
    default:
      printf(_("Err: Unknown file ff->type %d: %s\n"), ff->type, ff->fname);
      break;
  }
  num_files++;
  return 1;
}

static void PrintLsOutput(char* fname, char* link, int type, struct stat* statp)
{
  char buf[2000];
  char ec1[30];
  char *p, *f;
  int n;

  if (type == FT_LNK) {
    statp->st_mtime = 0;
    statp->st_mode |= 0777;
  }
  p = encode_mode(statp->st_mode, buf);
  n = sprintf(p, " %2d ", (uint32_t)statp->st_nlink);
  p += n;
  n = sprintf(p, "%-4d %-4d", (int)statp->st_uid, (int)statp->st_gid);
  p += n;
  n = sprintf(p, "%10.10s ", edit_uint64(statp->st_size, ec1));
  p += n;
  if (S_ISCHR(statp->st_mode) || S_ISBLK(statp->st_mode)) {
    n = sprintf(p, "%4x ", (int)statp->st_rdev);
  } else {
    n = sprintf(p, "     ");
  }
  p += n;
  p = encode_time(statp->st_mtime, p);
  *p++ = ' ';
  /* Copy file name */
  for (f = fname; *f && (p - buf) < (int)sizeof(buf);) *p++ = *f++;
  if (type == FT_LNK) {
    *p++ = '-';
    *p++ = '>';
    *p++ = ' ';
    /* Copy link name */
    for (f = link; *f && (p - buf) < (int)sizeof(buf);) *p++ = *f++;
  }
  *p++ = '\n';
  *p = 0;
  fputs(buf, stdout);
}
