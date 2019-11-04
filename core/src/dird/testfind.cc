/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Test program for find files
 */

#include "include/bareos.h"
#include "dird/dird.h"
#include "findlib/find.h"
#include "lib/mntent_cache.h"
#include "ch.h"

#if defined(HAVE_WIN32)
#define isatty(fd) (fd == 0)
#endif

using namespace directordaemon;

/* Dummy functions */
void GeneratePluginEvent(JobControlRecord* jcr,
                         bEventType eventType,
                         void* value)
{
}
extern bool ParseDirConfig(const char* configfile, int exit_code);

/* Global variables */
static int num_files = 0;
static int max_file_len = 0;
static int max_path_len = 0;
static int trunc_fname = 0;
static int trunc_path = 0;
static int attrs = 0;

static JobControlRecord* jcr;

static int PrintFile(JobControlRecord* jcr, FindFilesPacket* ff, bool);
static void CountFiles(FindFilesPacket* ff);
static bool CopyFileset(FindFilesPacket* ff, JobControlRecord* jcr);
static void SetOptions(findFOPTS* fo, const char* opts);

static void usage()
{
  fprintf(
      stderr,
      _("\n"
        "Usage: testfind [-d debug_level] [-] [pattern1 ...]\n"
        "       -a          print extended attributes (Win32 debug)\n"
        "       -d <nn>     set debug level to <nn>\n"
        "       -dt         print timestamp in debug output\n"
        "       -c          specify config file containing FileSet resources\n"
        "       -f          specify which FileSet to use\n"
        "       -?          print this message.\n"
        "\n"
        "Patterns are used for file inclusion -- normally directories.\n"
        "Debug level >= 1 prints each file found.\n"
        "Debug level >= 10 prints path/file for catalog.\n"
        "Errors are always printed.\n"
        "Files/paths truncated is the number of files/paths with len > 255.\n"
        "Truncation is only in the catalog.\n"
        "\n"));

  exit(1);
}


int main(int argc, char* const* argv)
{
  FindFilesPacket* ff;
  const char* configfile = "bareos-dir.conf";
  const char* fileset_name = "Windows-Full-Set";
  int ch, hard_links;

  OSDependentInit();

  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  while ((ch = getopt(argc, argv, "ac:d:f:?")) != -1) {
    switch (ch) {
      case 'a': /* print extended attributes *debug* */
        attrs = 1;
        break;

      case 'c': /* set debug level */
        configfile = optarg;
        break;

      case 'd': /* set debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 'f': /* exclude patterns */
        fileset_name = optarg;
        break;

      case '?':
      default:
        usage();
    }
  }

  argc -= optind;
  argv += optind;

  my_config = InitDirConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  MessagesResource* msg;

  foreach_res (msg, R_MSGS) {
    InitMsg(NULL, msg);
  }

  jcr = NewDirectorJcr();  // Ueb: null
  jcr->impl_->res.fileset =
      (FilesetResource*)my_config->GetResWithName(R_FILESET, fileset_name);

  if (jcr->impl_->res.fileset == NULL) {
    fprintf(stderr, "%s: Fileset not found\n", fileset_name);

    FilesetResource* var;

    fprintf(stderr, "Valid FileSets:\n");

    foreach_res (var, R_FILESET) {
      fprintf(stderr, "    %s\n", var->resource_name_);
    }

    exit(1);
  }

  ff = init_find_files();

  CopyFileset(ff, jcr);

  FindFiles(jcr, ff, PrintFile, NULL);

  FreeJcr(jcr);
  if (my_config) {
    delete my_config;
    my_config = NULL;
  }

  RecentJobResultsList::Cleanup();
  CleanupJcrChain();

  /* Clean up fileset */
  findFILESET* fileset = ff->fileset;

  if (fileset) {
    int i, j, k;
    /* Delete FileSet Include lists */
    for (i = 0; i < fileset->include_list.size(); i++) {
      findIncludeExcludeItem* incexe =
          (findIncludeExcludeItem*)fileset->include_list.get(i);
      for (j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
        for (k = 0; k < fo->regex.size(); k++) {
          regfree((regex_t*)fo->regex.get(k));
        }
        fo->regex.destroy();
        fo->regexdir.destroy();
        fo->regexfile.destroy();
        fo->wild.destroy();
        fo->wilddir.destroy();
        fo->wildfile.destroy();
        fo->wildbase.destroy();
        fo->fstype.destroy();
        fo->Drivetype.destroy();
      }
      incexe->opts_list.destroy();
      incexe->name_list.destroy();
    }
    fileset->include_list.destroy();

    /* Delete FileSet Exclude lists */
    for (i = 0; i < fileset->exclude_list.size(); i++) {
      findIncludeExcludeItem* incexe =
          (findIncludeExcludeItem*)fileset->exclude_list.get(i);
      for (j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
        fo->regex.destroy();
        fo->regexdir.destroy();
        fo->regexfile.destroy();
        fo->wild.destroy();
        fo->wilddir.destroy();
        fo->wildfile.destroy();
        fo->wildbase.destroy();
        fo->fstype.destroy();
        fo->Drivetype.destroy();
      }
      incexe->opts_list.destroy();
      incexe->name_list.destroy();
    }
    fileset->exclude_list.destroy();
    free(fileset);
  }
  ff->fileset = NULL;
  hard_links = TermFindFiles(ff);

  printf(_("\n"
           "Total files    : %d\n"
           "Max file length: %d\n"
           "Max path length: %d\n"
           "Files truncated: %d\n"
           "Paths truncated: %d\n"
           "Hard links     : %d\n"),
         num_files, max_file_len, max_path_len, trunc_fname, trunc_path,
         hard_links);

  FlushMntentCache();

  TermMsg();

  CloseMemoryPool();
  exit(0);
}

static int PrintFile(JobControlRecord* jcr, FindFilesPacket* ff, bool top_level)
{
  switch (ff->type) {
    case FT_LNKSAVED:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Lnka: %s -> %s\n", ff->fname, ff->link);
      }
      break;
    case FT_REGE:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Empty: %s\n", ff->fname);
      }
      CountFiles(ff);
      break;
    case FT_REG:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf(_("Reg: %s\n"), ff->fname);
      }
      CountFiles(ff);
      break;
    case FT_LNK:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Lnk: %s -> %s\n", ff->fname, ff->link);
      }
      CountFiles(ff);
      break;
    case FT_DIRBEGIN:
      return 1;
    case FT_NORECURSE:
    case FT_NOFSCHG:
    case FT_INVALIDFS:
    case FT_INVALIDDT:
    case FT_DIREND:
      if (debug_level) {
        char errmsg[100] = "";
        if (ff->type == FT_NORECURSE) {
          bstrncpy(errmsg, _("\t[will not descend: recursion turned off]"),
                   sizeof(errmsg));
        } else if (ff->type == FT_NOFSCHG) {
          bstrncpy(errmsg,
                   _("\t[will not descend: file system change not allowed]"),
                   sizeof(errmsg));
        } else if (ff->type == FT_INVALIDFS) {
          bstrncpy(errmsg, _("\t[will not descend: disallowed file system]"),
                   sizeof(errmsg));
        } else if (ff->type == FT_INVALIDDT) {
          bstrncpy(errmsg, _("\t[will not descend: disallowed drive type]"),
                   sizeof(errmsg));
        }
        printf("%s%s%s\n", (debug_level > 1 ? "Dir: " : ""), ff->fname, errmsg);
      }
      ff->type = FT_DIREND;
      CountFiles(ff);
      break;
    case FT_SPEC:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Spec: %s\n", ff->fname);
      }
      CountFiles(ff);
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
    case FT_NOOPEN:
      printf(_("Err: Could not open directory %s: %s\n"), ff->fname,
             strerror(errno));
      break;
    default:
      printf(_("Err: Unknown file ff->type %d: %s\n"), ff->type, ff->fname);
      break;
  }
  if (attrs) {
    char attr[200];
    encode_attribsEx(NULL, attr, ff);
    if (*attr != 0) { printf("AttrEx=%s\n", attr); }
    //    set_attribsEx(NULL, ff->fname, NULL, NULL, ff->type, attr);
  }
  return 1;
}

static void CountFiles(FindFilesPacket* ar)
{
  int fnl, pnl;
  char *l, *p;
  PoolMem file(PM_FNAME);
  PoolMem spath(PM_FNAME);

  num_files++;

  /* Find path without the filename.
   * I.e. everything after the last / is a "filename".
   * OK, maybe it is a directory name, but we treat it like
   * a filename. If we don't find a / then the whole name
   * must be a path name (e.g. c:).
   */
  for (p = l = ar->fname; *p; p++) {
    if (IsPathSeparator(*p)) { l = p; /* set pos of last slash */ }
  }
  if (IsPathSeparator(*l)) { /* did we find a slash? */
    l++;                     /* yes, point to filename */
  } else {                   /* no, whole thing must be path name */
    l = p;
  }

  /* If filename doesn't exist (i.e. root directory), we
   * simply create a blank name consisting of a single
   * space. This makes handling zero length filenames
   * easier.
   */
  fnl = p - l;
  if (fnl > max_file_len) { max_file_len = fnl; }
  if (fnl > 255) {
    printf(_("===== Filename truncated to 255 chars: %s\n"), l);
    fnl = 255;
    trunc_fname++;
  }

  if (fnl > 0) {
    PmStrcpy(file, l); /* copy filename */
  } else {
    PmStrcpy(file, " "); /* blank filename */
  }

  pnl = l - ar->fname;
  if (pnl > max_path_len) { max_path_len = pnl; }
  if (pnl > 255) {
    printf(_("========== Path name truncated to 255 chars: %s\n"), ar->fname);
    pnl = 255;
    trunc_path++;
  }

  PmStrcpy(spath, ar->fname);
  if (pnl == 0) {
    PmStrcpy(spath, " ");
    printf(_("========== Path length is zero. File=%s\n"), ar->fname);
  }
  if (debug_level >= 10) {
    printf(_("Path: %s\n"), spath.c_str());
    printf(_("File: %s\n"), file.c_str());
  }
}

static bool CopyFileset(FindFilesPacket* ff, JobControlRecord* jcr)
{
  FilesetResource* jcr_fileset = jcr->impl_->res.fileset;
  int num;
  bool include = true;

  findFILESET* fileset;
  findFOPTS* current_opts;

  fileset = (findFILESET*)malloc(sizeof(findFILESET));
  memset(fileset, 0, sizeof(findFILESET));
  ff->fileset = fileset;

  fileset->state = state_none;
  fileset->include_list.init(1, true);
  fileset->exclude_list.init(1, true);

  for (;;) {
    if (include) {
      num = jcr_fileset->num_includes;
    } else {
      num = jcr_fileset->num_excludes;
    }
    for (int i = 0; i < num; i++) {
      IncludeExcludeItem* ie;
      int j, k;

      if (include) {
        ie = jcr_fileset->include_items[i];

        /* New include */
        fileset->incexe =
            (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
        memset(fileset->incexe, 0, sizeof(findIncludeExcludeItem));
        fileset->incexe->opts_list.init(1, true);
        fileset->incexe->name_list.init(0, 0);
        fileset->include_list.append(fileset->incexe);
      } else {
        ie = jcr_fileset->exclude_items[i];

        /* New exclude */
        fileset->incexe =
            (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
        memset(fileset->incexe, 0, sizeof(findIncludeExcludeItem));
        fileset->incexe->opts_list.init(1, true);
        fileset->incexe->name_list.init(0, 0);
        fileset->exclude_list.append(fileset->incexe);
      }

      for (j = 0; j < ie->num_opts; j++) {
        FileOptions* fo = ie->opts_list[j];

        current_opts = (findFOPTS*)malloc(sizeof(findFOPTS));
        memset(current_opts, 0, sizeof(findFOPTS));
        fileset->incexe->current_opts = current_opts;
        fileset->incexe->opts_list.append(current_opts);

        current_opts->regex.init(1, true);
        current_opts->regexdir.init(1, true);
        current_opts->regexfile.init(1, true);
        current_opts->wild.init(1, true);
        current_opts->wilddir.init(1, true);
        current_opts->wildfile.init(1, true);
        current_opts->wildbase.init(1, true);
        current_opts->fstype.init(1, true);
        current_opts->Drivetype.init(1, true);

        SetOptions(current_opts, fo->opts);

        for (k = 0; k < fo->regex.size(); k++) {
          // fd->fsend("R %s\n", fo->regex.get(k));
          current_opts->regex.append(strdup((const char*)fo->regex.get(k)));
        }
        for (k = 0; k < fo->regexdir.size(); k++) {
          // fd->fsend("RD %s\n", fo->regexdir.get(k));
          current_opts->regexdir.append(
              strdup((const char*)fo->regexdir.get(k)));
        }
        for (k = 0; k < fo->regexfile.size(); k++) {
          // fd->fsend("RF %s\n", fo->regexfile.get(k));
          current_opts->regexfile.append(
              strdup((const char*)fo->regexfile.get(k)));
        }
        for (k = 0; k < fo->wild.size(); k++) {
          current_opts->wild.append(strdup((const char*)fo->wild.get(k)));
        }
        for (k = 0; k < fo->wilddir.size(); k++) {
          current_opts->wilddir.append(strdup((const char*)fo->wilddir.get(k)));
        }
        for (k = 0; k < fo->wildfile.size(); k++) {
          current_opts->wildfile.append(
              strdup((const char*)fo->wildfile.get(k)));
        }
        for (k = 0; k < fo->wildbase.size(); k++) {
          current_opts->wildbase.append(
              strdup((const char*)fo->wildbase.get(k)));
        }
        for (k = 0; k < fo->fstype.size(); k++) {
          current_opts->fstype.append(strdup((const char*)fo->fstype.get(k)));
        }
        for (k = 0; k < fo->Drivetype.size(); k++) {
          current_opts->Drivetype.append(
              strdup((const char*)fo->Drivetype.get(k)));
        }
      }

      for (j = 0; j < ie->name_list.size(); j++) {
        fileset->incexe->name_list.append(
            strdup((const char*)ie->name_list.get(j)));
      }
    }

    if (!include) { /* If we just did excludes */
      break;        /*   all done */
    }

    include = false; /* Now do excludes */
  }

  return true;
}

static void SetOptions(findFOPTS* fo, const char* opts)
{
  int j;
  const char* p;

  for (p = opts; *p; p++) {
    switch (*p) {
      case 'a': /* alway replace */
      case '0': /* no option */
        break;
      case 'e':
        SetBit(FO_EXCLUDE, fo->flags);
        break;
      case 'f':
        SetBit(FO_MULTIFS, fo->flags);
        break;
      case 'h': /* no recursion */
        SetBit(FO_NO_RECURSION, fo->flags);
        break;
      case 'H': /* no hard link handling */
        SetBit(FO_NO_HARDLINK, fo->flags);
        break;
      case 'i':
        SetBit(FO_IGNORECASE, fo->flags);
        break;
      case 'M': /* MD5 */
        SetBit(FO_MD5, fo->flags);
        break;
      case 'n':
        SetBit(FO_NOREPLACE, fo->flags);
        break;
      case 'p': /* use portable data format */
        SetBit(FO_PORTABLE, fo->flags);
        break;
      case 'R': /* Resource forks and Finder Info */
        SetBit(FO_HFSPLUS, fo->flags);
      case 'r': /* read fifo */
        SetBit(FO_READFIFO, fo->flags);
        break;
      case 'S':
        switch (*(p + 1)) {
          case ' ':
            /* Old director did not specify SHA variant */
            SetBit(FO_SHA1, fo->flags);
            break;
          case '1':
            SetBit(FO_SHA1, fo->flags);
            p++;
            break;
#ifdef HAVE_SHA2
          case '2':
            SetBit(FO_SHA256, fo->flags);
            p++;
            break;
          case '3':
            SetBit(FO_SHA512, fo->flags);
            p++;
            break;
#endif
          default:
            /* Automatically downgrade to SHA-1 if an unsupported
             * SHA variant is specified */
            SetBit(FO_SHA1, fo->flags);
            p++;
            break;
        }
        break;
      case 's':
        SetBit(FO_SPARSE, fo->flags);
        break;
      case 'm':
        SetBit(FO_MTIMEONLY, fo->flags);
        break;
      case 'k':
        SetBit(FO_KEEPATIME, fo->flags);
        break;
      case 'A':
        SetBit(FO_ACL, fo->flags);
        break;
      case 'V': /* verify options */
        /* Copy Verify Options */
        for (j = 0; *p && *p != ':'; p++) {
          fo->VerifyOpts[j] = *p;
          if (j < (int)sizeof(fo->VerifyOpts) - 1) { j++; }
        }
        fo->VerifyOpts[j] = 0;
        break;
      case 'w':
        SetBit(FO_IF_NEWER, fo->flags);
        break;
      case 'W':
        SetBit(FO_ENHANCEDWILD, fo->flags);
        break;
      case 'Z': /* compression */
        p++;    /* skip Z */
        if (*p >= '0' && *p <= '9') {
          SetBit(FO_COMPRESS, fo->flags);
          fo->Compress_algo = COMPRESS_GZIP;
          fo->Compress_level = *p - '0';
        } else if (*p == 'o') {
          SetBit(FO_COMPRESS, fo->flags);
          fo->Compress_algo = COMPRESS_LZO1X;
          fo->Compress_level = 1; /* not used with LZO */
        }
        Dmsg2(200, "Compression alg=%d level=%d\n", fo->Compress_algo,
              fo->Compress_level);
        break;
      case 'x':
        SetBit(FO_NO_AUTOEXCL, fo->flags);
        break;
      case 'X':
        SetBit(FO_XATTR, fo->flags);
        break;
      default:
        Emsg1(M_ERROR, 0, _("Unknown include/exclude option: %c\n"), *p);
        break;
    }
  }
}
