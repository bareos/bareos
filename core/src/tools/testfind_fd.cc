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

#include "dird/fd_sendfileset.h"
#include "filed/filed_utils.h"
#include "filed/filed.h"
#include "filed/filed_jcr_impl.h"
#include "filed/filed_globals.h"
#include "filed/dir_cmd.h"
#include "filed/fileset.h"
#include "findlib/attribs.h"
#include "lib/parse_conf.h"
#include "tools/dummysockets.h"
#include "tools/testfind_fd.h"

using namespace filedaemon;

static struct TestfindStats {
  int num_files = 0;
  int max_file_len = 0;
  int max_path_len = 0;
  int trunc_fname = 0;
  int trunc_path = 0;
  int hard_links = 0;
  bool print_attributes = false;
} testfind_stats;


static int handleFile(JobControlRecord* jcr,
                      FindFilesPacket* ff_pkt,
                      bool top_level)
{
  if (!SaveFile(jcr, ff_pkt, top_level)) { return 0; }
  if (!PrintFile(jcr, ff_pkt, top_level)) { return 0; }
  return 1;
}

void ProcessFileset(directordaemon::FilesetResource* director_fileset,
                    const char* configfile,
                    bool print_attrs)
{
  testfind_stats.print_attributes = print_attrs;

  my_config = InitFdConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  me = static_cast<ClientResource*>(my_config->GetNextRes(R_CLIENT, nullptr));
  no_signals = true;
  me->compatible = true;

  if (!CheckResources()) {
    printf("Problem checking resources!");
    return;
  }

  JobControlRecord* jcr;
  EmptySocket* dird_sock = new EmptySocket;
  jcr = create_new_director_session(dird_sock);

  EmptySocket* stored_sock = new EmptySocket;
  jcr->store_bsock = stored_sock;
  stored_sock->message_length = 0;

  DummyFdFilesetSocket* filed_sock = new DummyFdFilesetSocket;

  filed_sock->jcr = jcr;
  jcr->file_bsock = filed_sock;

  crypto_cipher_t cipher = CRYPTO_CIPHER_NONE;
  GetWantedCryptoCipher(jcr, &cipher);

  InitFileset(jcr);
  directordaemon::SendIncludeExcludeItems(jcr, director_fileset);
  TermFileset(jcr);

  BlastDataToStorageDaemon(jcr, cipher, DEFAULT_NETWORK_BUFFER_SIZE,
                           handleFile);

  testfind_stats.hard_links = jcr->fd_impl->ff->linkhash->size();
  printf(_("\n"
           "Total files    : %d\n"
           "Max file length: %d\n"
           "Max path length: %d\n"
           "Files truncated: %d\n"
           "Paths truncated: %d\n"
           "Hard links     : %d\n"),
         testfind_stats.num_files, testfind_stats.max_file_len,
         testfind_stats.max_path_len, testfind_stats.trunc_fname,
         testfind_stats.trunc_path, testfind_stats.hard_links);

  jcr->file_bsock->close();
  delete jcr->file_bsock;
  jcr->file_bsock = nullptr;
  CleanupFileset(jcr);
  FreeJcr(jcr);

  TermMsg();
  if (me->secure_erase_cmdline) { FreePoolMemory(me->secure_erase_cmdline); }
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
      UpdateFilestats(ff);
      break;
    case FT_REG:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf(_("Reg: %s\n"), ff->fname);
      }
      UpdateFilestats(ff);
      break;
    case FT_LNK:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Lnk: %s -> %s\n", ff->fname, ff->link);
      }
      UpdateFilestats(ff);
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
      UpdateFilestats(ff);
      break;
    case FT_SPEC:
      if (debug_level == 1) {
        printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
        printf("Spec: %s\n", ff->fname);
      }
      UpdateFilestats(ff);
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
  if (testfind_stats.print_attributes) {
    char attr[200];
    encode_attribsEx(nullptr, attr, ff);
    if (*attr != 0) { printf("AttrEx=%s\n", attr); }
  }
  return 1;
}

void UpdateFilestats(FindFilesPacket* ffp)
{
  int filename_length, path_length;
  char *l, *p;
  PoolMem file(PM_FNAME);
  PoolMem spath(PM_FNAME);

  testfind_stats.num_files++;

  /* Find path without the filename.
   * I.e. everything after the last / is a "filename".
   * OK, maybe it is a directory name, but we treat it like
   * a filename. If we don't find a / then the whole name
   * must be a path name (e.g. c:).
   */
  for (p = l = ffp->fname; *p; p++) {
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
  filename_length = p - l;
  if (filename_length > testfind_stats.max_file_len) {
    testfind_stats.max_file_len = filename_length;
  }
  if (filename_length > 255) {
    printf(_("===== Filename truncated to 255 chars: %s\n"), l);
    filename_length = 255;
    testfind_stats.trunc_fname++;
  }

  if (filename_length > 0) {
    PmStrcpy(file, l); /* copy filename */
  } else {
    PmStrcpy(file, " "); /* blank filename */
  }

  path_length = l - ffp->fname;
  if (path_length > testfind_stats.max_path_len) {
    testfind_stats.max_path_len = path_length;
  }
  if (path_length > 255) {
    printf(_("========== Path name truncated to 255 chars: %s\n"), ffp->fname);
    path_length = 255;
    testfind_stats.trunc_path++;
  }

  PmStrcpy(spath, ffp->fname);
  if (path_length == 0) {
    PmStrcpy(spath, " ");
    printf(_("========== Path length is zero. File=%s\n"), ffp->fname);
  }
  if (debug_level >= 10) {
    printf(_("Path: %s\n"), spath.c_str());
    printf(_("File: %s\n"), file.c_str());
  }
}
