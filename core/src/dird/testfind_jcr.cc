/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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


#include "filed/filed_utils.h"
#include "filed/filed.h"
#include "filed/filed_jcr_impl.h"
#include "filed/filed_globals.h"
#include "filed/dir_cmd.h"
#include "filed/fileset.h"

#include "dird/dird_conf.h"

#include "dird/testfind_jcr.h"

#include "lib/bsock_testfind.h"
#include "lib/parse_conf.h"


using namespace filedaemon;

static int num_files = 0;
static int max_file_len = 0;
static int max_path_len = 0;
static int trunc_fname = 0;
static int trunc_path = 0;
static int local_attrs = 0;

static int handleFile(JobControlRecord* jcr,
                      FindFilesPacket* ff_pkt,
                      bool top_level)
{
  if (!SaveFile(jcr, ff_pkt, top_level)) { return 0; }
  if (!PrintFile(jcr, ff_pkt, top_level)) { return 0; }
  return 1;
}


void launchFileDaemonLogic(directordaemon::FilesetResource* jcr_fileset,
                           const char* configfile,
                           int attrs)
{
  local_attrs = attrs;
  crypto_cipher_t cipher = CRYPTO_CIPHER_NONE;

  my_config = InitFdConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  me = static_cast<ClientResource*>(my_config->GetNextRes(R_CLIENT, nullptr));
  no_signals = true;
  me->compatible = true;

  if (CheckResources()) {
    BareosSocketTestfind* dird_sock = new BareosSocketTestfind;
    BareosSocketTestfind* stored_sock = new BareosSocketTestfind;
    JobControlRecord* jcr;

    jcr = create_new_director_session(dird_sock);
    jcr->store_bsock = stored_sock;
    stored_sock->message_length = 0;

    GetWantedCryptoCipher(jcr, &cipher);

    setupFileset(jcr->fd_impl->ff, jcr_fileset);
    const char* filename = jcr_fileset->include_items[0]->name_list.get(0);
    AddFileToFileset(jcr, filename, true, jcr->fd_impl->ff->fileset);

    BlastDataToStorageDaemon(jcr, cipher, DEFAULT_NETWORK_BUFFER_SIZE,
                             handleFile);

    printf(_("\n"
             "Total files    : %d\n"
             "Max file length: %d\n"
             "Max path length: %d\n"
             "Files truncated: %d\n"
             "Paths truncated: %d\n"),
           num_files, max_file_len, max_path_len, trunc_fname, trunc_path);

    CleanupFileset(jcr);
    FreeJcr(jcr);
  }
  if (me->secure_erase_cmdline) { FreePoolMemory(me->secure_erase_cmdline); }
}


bool setupFileset(FindFilesPacket* ff,
                  directordaemon::FilesetResource* jcr_fileset)
{
  int num;
  bool include = true;

  findFILESET* fileset;


  findFILESET* fileset_allocation = (findFILESET*)malloc(sizeof(findFILESET));
  fileset = new (fileset_allocation)(findFILESET);
  ff->fileset = fileset;

  fileset->state = state_none;
  fileset->include_list.init(1, true);
  fileset->exclude_list.init(1, true);

  for (;;) {
    if (include) {
      num = jcr_fileset->include_items.size();
    } else {
      num = jcr_fileset->exclude_items.size();
    }
    for (int i = 0; i < num; i++) {
      if (include) {
        /* New include */
        findIncludeExcludeItem* incexe_allocation
            = (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
        fileset->incexe = new (incexe_allocation)(findIncludeExcludeItem);
        fileset->include_list.append(fileset->incexe);
      } else {
        /* New exclude */
        findIncludeExcludeItem* incexe_allocation
            = (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
        fileset->incexe = new (incexe_allocation)(findIncludeExcludeItem);
        fileset->exclude_list.append(fileset->incexe);
      }
    }

    if (!include) { /* If we just did excludes */
      break;        /*   all done */
    }

    include = false; /* Now do excludes */
  }

  return true;
}

void SetOptions(findFOPTS* fo, const char* opts)
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
        [[fallthrough]];
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


int PrintFile(JobControlRecord*, FindFilesPacket* ff, bool)
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
  if (local_attrs) {
    char attr[200];
    //    encode_attribsEx(NULL, attr, ff);
    if (*attr != 0) { printf("AttrEx=%s\n", attr); }
    //    set_attribsEx(NULL, ff->fname, NULL, NULL, ff->type, attr);
  }
  return 1;
}

void CountFiles(FindFilesPacket* ar)
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
