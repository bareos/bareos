/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October MM
 * Extracted from other source files by Marco van Wieringen, September 2012
 */
/**
 * @file
 * Bareos File Daemon Include Exclude pattern handling.
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/filed_jcr_impl.h"
#include "filed/fileset.h"
#include "findlib/match.h"
#include "lib/berrno.h"
#include "lib/edit.h"
#include "include/ch.h"
#include "lib/util.h"
#include "lib/bpipe.h"

#ifdef HAVE_WIN32
#  include "findlib/win32.h"
#endif

namespace filedaemon {

/**
 * callback function for edit_job_codes
 * See ../lib/util.c, function edit_job_codes, for more remaining codes
 *
 * %D = Director
 * %m = Modification time (for incremental and differential)
 */
std::optional<std::string> job_code_callback_filed(JobControlRecord* jcr,
                                                   const char* param)
{
  switch (param[0]) {
    case 'D':
      if (jcr->fd_impl->director) {
        return jcr->fd_impl->director->resource_name_;
      }
      break;
    case 'm':
      return std::to_string(jcr->fd_impl->since_time);
  }

  return std::nullopt;
}

bool InitFileset(JobControlRecord* jcr)
{
  FindFilesPacket* ff;
  findFILESET* fileset;

  if (!jcr->fd_impl->ff) { return false; }
  ff = jcr->fd_impl->ff;
  if (ff->fileset) { return false; }
  fileset = (findFILESET*)malloc(sizeof(findFILESET));
  *fileset = findFILESET{};

  ff->fileset = fileset;
  fileset->state = state_none;
  fileset->include_list.init(1, true);
  fileset->exclude_list.init(1, true);
  return true;
}

static void append_file(JobControlRecord* jcr,
                        findIncludeExcludeItem* incexe,
                        const char* buf,
                        bool IsFile)
{
  if (IsFile) {
    // Sanity check never append empty file patterns.
    if (strlen(buf) > 0) { incexe->name_list.append(new_dlistString(buf)); }
  } else if (me->plugin_directory) {
    GeneratePluginEvent(jcr, bEventPluginCommand, (void*)buf);
    incexe->plugin_list.append(new_dlistString(buf));
  } else {
    Jmsg(jcr, M_FATAL, 0,
         T_("Plugin Directory not defined. Cannot use plugin: \"%s\"\n"), buf);
  }
}

/**
 * Add fname to include/exclude fileset list. First check for
 * | and < and if necessary perform command.
 */
void AddFileToFileset(JobControlRecord* jcr,
                      const char* fname,
                      bool IsFile,
                      findFILESET* fileset)
{
  char* p;
  Bpipe* bpipe;
  POOLMEM* fn;
  FILE* ffd;
  char buf[1000];
  int ch;
  int status;

  p = (char*)fname;
  ch = (uint8_t)*p;
  switch (ch) {
    case '|':
      p++; /* skip over | */
      fn = GetPoolMemory(PM_FNAME);
      fn = edit_job_codes(jcr, fn, p, "", job_code_callback_filed);
      bpipe = OpenBpipe(fn, 0, "r");
      if (!bpipe) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0, T_("Cannot run program: %s. ERR=%s\n"), p,
             be.bstrerror());
        FreePoolMemory(fn);
        return;
      }
      FreePoolMemory(fn);
      while (fgets(buf, sizeof(buf), bpipe->rfd)) {
        StripTrailingJunk(buf);
        append_file(jcr, fileset->incexe, buf, IsFile);
      }
      if ((status = CloseBpipe(bpipe)) != 0) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0,
             T_("Error running program: %s. status=%d: ERR=%s\n"), p,
             be.code(status), be.bstrerror(status));
        return;
      }
      break;
    case '<':
      Dmsg1(100, "Doing < of '%s' include on client.\n", p + 1);
      p++; /* skip over < */
      if ((ffd = fopen(p, "rb")) == NULL) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0,
             T_("Cannot open FileSet input file: %s. ERR=%s\n"), p,
             be.bstrerror());
        return;
      }
      while (fgets(buf, sizeof(buf), ffd)) {
        StripTrailingJunk(buf);
        append_file(jcr, fileset->incexe, buf, IsFile);
      }
      fclose(ffd);
      break;
    default:
      append_file(jcr, fileset->incexe, fname, IsFile);
      break;
  }
}

findIncludeExcludeItem* get_incexe(JobControlRecord* jcr)
{
  if (jcr->fd_impl->ff && jcr->fd_impl->ff->fileset) {
    return jcr->fd_impl->ff->fileset->incexe;
  }

  return NULL;
}

void SetIncexe(JobControlRecord* jcr, findIncludeExcludeItem* incexe)
{
  findFILESET* fileset = jcr->fd_impl->ff->fileset;
  fileset->incexe = incexe;
}

// Add a regex to the current fileset
int AddRegexToFileset(JobControlRecord* jcr, const char* item, int type)
{
  findFOPTS* current_opts = start_options(jcr->fd_impl->ff);
  regex_t* preg;
  int rc;
  char prbuf[500];

  preg = (regex_t*)malloc(sizeof(regex_t));
  if (BitIsSet(FO_IGNORECASE, current_opts->flags)) {
    rc = regcomp(preg, item, REG_EXTENDED | REG_ICASE);
  } else {
    rc = regcomp(preg, item, REG_EXTENDED);
  }
  if (rc != 0) {
    regerror(rc, preg, prbuf, sizeof(prbuf));
    regfree(preg);
    free(preg);
    Jmsg(jcr, M_FATAL, 0, T_("REGEX %s compile error. ERR=%s\n"), item, prbuf);
    return state_error;
  }
  if (type == ' ') {
    current_opts->regex.append(preg);
  } else if (type == 'D') {
    current_opts->regexdir.append(preg);
  } else if (type == 'F') {
    current_opts->regexfile.append(preg);
  } else {
    return state_error;
  }

  return state_options;
}

// Add a wild card to the current fileset
int AddWildToFileset(JobControlRecord* jcr, const char* item, int type)
{
  findFOPTS* current_opts = start_options(jcr->fd_impl->ff);

  if (type == ' ') {
    current_opts->wild.append(strdup(item));
  } else if (type == 'D') {
    current_opts->wilddir.append(strdup(item));
  } else if (type == 'F') {
    current_opts->wildfile.append(strdup(item));
  } else if (type == 'B') {
    current_opts->wildbase.append(strdup(item));
  } else {
    return state_error;
  }

  return state_options;
}

/**
 * As an optimization, we should do this during
 *  "compile" time in filed/job.c, and keep only a bit mask
 *  and the Verify options.
 */
static int SetOptionsAndFlags(findFOPTS* fo, const char* opts)
{
  int j;
  const char* p;
  char strip[21];
  char size[50];

  for (p = opts; *p; p++) {
    switch (*p) {
      case 'A':
        SetBit(FO_ACL, fo->flags);
        break;
      case 'a': /* Alway replace */
      case '0': /* No option */
        break;
      case 'C': /* Accurate options */
        // Copy Accurate Options
        for (j = 0; *p && *p != ':'; p++) {
          fo->AccurateOpts[j] = *p;
          if (j < (int)sizeof(fo->AccurateOpts) - 1) { j++; }
        }
        fo->AccurateOpts[j] = 0;
        // skip ':' but do not skip '\0'
        // p currently points at one of them
        if (*p == '\0') {
          // we need to undo the increment, so that the for loop does not
          // advance past the string
          p -= 1;
        }
        break;
      case 'c':
        SetBit(FO_CHKCHANGES, fo->flags);
        break;
      case 'd':
        switch (*(p + 1)) {
          case '1':
            fo->shadow_type = check_shadow_local_warn;
            p++;
            break;
          case '2':
            fo->shadow_type = check_shadow_local_remove;
            p++;
            break;
          case '3':
            fo->shadow_type = check_shadow_global_warn;
            p++;
            break;
          case '4':
            fo->shadow_type = check_shadow_global_remove;
            p++;
            break;
        }
        break;
      case 'e':
        SetBit(FO_EXCLUDE, fo->flags);
        break;
      case 'E': /* Encryption */
        switch (*(p + 1)) {
          case '3':
            fo->Encryption_cipher = CRYPTO_CIPHER_3DES_CBC;
            p++;
            break;
          case 'a':
            switch (*(p + 2)) {
              case '1':
                fo->Encryption_cipher = CRYPTO_CIPHER_AES_128_CBC;
                p += 2;
                break;
              case '2':
                fo->Encryption_cipher = CRYPTO_CIPHER_AES_192_CBC;
                p += 2;
                break;
              case '3':
                fo->Encryption_cipher = CRYPTO_CIPHER_AES_256_CBC;
                p += 2;
                break;
            }
            break;
          case 'b':
            fo->Encryption_cipher = CRYPTO_CIPHER_BLOWFISH_CBC;
            p++;
            break;
          case 'c':
            switch (*(p + 2)) {
              case '1':
                fo->Encryption_cipher = CRYPTO_CIPHER_CAMELLIA_128_CBC;
                p += 2;
                break;
              case '2':
                fo->Encryption_cipher = CRYPTO_CIPHER_CAMELLIA_192_CBC;
                p += 2;
                break;
              case '3':
                fo->Encryption_cipher = CRYPTO_CIPHER_CAMELLIA_256_CBC;
                p += 2;
                break;
            }
            break;
          case 'f':
            SetBit(FO_FORCE_ENCRYPT, fo->flags);
            p++;
            break;
          case 'h':
            switch (*(p + 2)) {
              case '1':
                fo->Encryption_cipher = CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1;
                p += 2;
                break;
              case '2':
                fo->Encryption_cipher = CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1;
                p += 2;
                break;
            }
        }
        break;
      case 'f':
        SetBit(FO_MULTIFS, fo->flags);
        break;
      case 'H': /* No hard link handling */
        SetBit(FO_NO_HARDLINK, fo->flags);
        break;
      case 'h': /* No recursion */
        SetBit(FO_NO_RECURSION, fo->flags);
        break;
      case 'i':
        SetBit(FO_IGNORECASE, fo->flags);
        break;
      case 'K':
        SetBit(FO_NOATIME, fo->flags);
        break;
      case 'k':
        SetBit(FO_KEEPATIME, fo->flags);
        break;
      case 'M': /* MD5 */
        SetBit(FO_MD5, fo->flags);
        break;
      case 'm':
        SetBit(FO_MTIMEONLY, fo->flags);
        break;
      case 'N':
        SetBit(FO_HONOR_NODUMP, fo->flags);
        break;
      case 'n':
        SetBit(FO_NOREPLACE, fo->flags);
        break;
      case 'P': /* Strip path */
        // Get integer
        p++; /* skip P */
        for (j = 0; *p && *p != ':'; p++) {
          strip[j] = *p;
          if (j < (int)sizeof(strip) - 1) { j++; }
        }
        strip[j] = 0;
        fo->StripPath = atoi(strip);
        SetBit(FO_STRIPPATH, fo->flags);
        Dmsg2(100, "strip=%s StripPath=%d\n", strip, fo->StripPath);
        // skip ':' but do not skip '\0'
        // p currently points at one of them
        if (*p == '\0') {
          // we need to undo the increment, so that the for loop does not
          // advance past the string
          p -= 1;
        }
        break;
      case 'p': /* Use portable data format */
        SetBit(FO_PORTABLE, fo->flags);
        break;
      case 'R': /* Resource forks and Finder Info */
        SetBit(FO_HFSPLUS, fo->flags);
        break;
      case 'r': /* Read fifo */
        SetBit(FO_READFIFO, fo->flags);
        break;
      case 'S':
        switch (*(p + 1)) {
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
          case '4':
            SetBit(FO_XXH128, fo->flags);
            p++;
            break;
          default:
            /* If 2 or 3 is seen here, SHA2 is not configured, so eat the
             * option, and drop back to SHA-1. */
            if (p[1] == '2' || p[1] == '3') { p++; }
            SetBit(FO_SHA1, fo->flags);
            break;
        }
        break;
      case 's':
        SetBit(FO_SPARSE, fo->flags);
        break;
      case 'V': /* verify options */
        // Copy Verify Options
        for (j = 0; *p && *p != ':'; p++) {
          fo->VerifyOpts[j] = *p;
          if (j < (int)sizeof(fo->VerifyOpts) - 1) { j++; }
        }
        fo->VerifyOpts[j] = 0;

        // skip ':' but do not skip '\0'
        // p currently points at one of them
        if (*p == '\0') {
          // we need to undo the increment, so that the for loop does not
          // advance past the string
          p -= 1;
        }
        break;
      case 'W':
        SetBit(FO_ENHANCEDWILD, fo->flags);
        break;
      case 'w':
        SetBit(FO_IF_NEWER, fo->flags);
        break;
      case 'x':
        SetBit(FO_NO_AUTOEXCL, fo->flags);
        break;
      case 'X':
        SetBit(FO_XATTR, fo->flags);
        break;
      case 'Z': /* Compression */
        p++;    /* Skip Z */
        if (*p >= '0' && *p <= '9') {
          SetBit(FO_COMPRESS, fo->flags);
          fo->Compress_algo = COMPRESS_GZIP;
          fo->Compress_level = *p - '0';
        } else if (*p == 'o') {
          SetBit(FO_COMPRESS, fo->flags);
          fo->Compress_algo = COMPRESS_LZO1X;
          fo->Compress_level = 1; /* not used with LZO */
        } else if (*p == 'f') {
          p++;
          if (*p == 'f') {
            SetBit(FO_COMPRESS, fo->flags);
            fo->Compress_algo = COMPRESS_FZFZ;
            fo->Compress_level = 1; /* not used with FZFZ */
          } else if (*p == '4') {
            SetBit(FO_COMPRESS, fo->flags);
            fo->Compress_algo = COMPRESS_FZ4L;
            fo->Compress_level = 1; /* not used with FZ4L */
          } else if (*p == 'h') {
            SetBit(FO_COMPRESS, fo->flags);
            fo->Compress_algo = COMPRESS_FZ4H;
            fo->Compress_level = 1; /* not used with FZ4H */
          }
        }
        break;
      case 'z': /* Min, max or approx size or size range */
        p++;    /* Skip z */
        for (j = 0; *p && *p != ':'; p++) {
          size[j] = *p;
          if (j < (int)sizeof(size) - 1) { j++; }
        }
        size[j] = 0;
        if (!fo->size_match) {
          fo->size_match
              = (struct s_sz_matching*)malloc(sizeof(struct s_sz_matching));
        }
        if (!ParseSizeMatch(size, fo->size_match)) {
          Emsg1(M_ERROR, 0, T_("Unparseable size option: %s\n"), size);
        }

        // skip ':' but do not skip '\0'
        // p currently points at one of them
        if (*p == '\0') {
          // we need to undo the increment, so that the for loop does not
          // advance past the string
          p -= 1;
        }
        break;
      default:
        Emsg1(M_ERROR, 0, T_("Unknown include/exclude option: %c\n"), *p);
        break;
    }
  }

  return state_options;
}

// Add options to the current fileset
int AddOptionsFlagsToFileset(JobControlRecord* jcr, const char* item)
{
  findFOPTS* current_opts = start_options(jcr->fd_impl->ff);

  SetOptionsAndFlags(current_opts, item);

  return state_options;
}

void AddFileset(JobControlRecord* jcr, const char* item)
{
  FindFilesPacket* ff = jcr->fd_impl->ff;
  findFILESET* fileset = ff->fileset;
  int code, subcode;
  int state = fileset->state;
  findFOPTS* current_opts;

  // Get code, optional subcode, and position item past the dividing space
  Dmsg1(100, "%s\n", item);
  code = item[0];
  if (code != '\0') { ++item; }

  subcode = ' '; /* A space is always a valid subcode */
  if (item[0] != '\0' && item[0] != ' ') {
    subcode = item[0];
    ++item;
  }

  if (*item == ' ') { ++item; }

  // Skip all lines we receive after an error
  if (state == state_error) {
    Dmsg0(100, "State=error return\n");
    return;
  }

  /* The switch tests the code for validity.
   * The subcode is always good if it is a space, otherwise we must confirm.
   * We set state to state_error first assuming the subcode is invalid,
   * requiring state to be set in cases below that handle subcodes. */
  if (subcode != ' ') {
    state = state_error;
    Dmsg0(100, "Set state=error or double code.\n");
  }
  switch (code) {
    case 'I':
      (void)new_include(jcr->fd_impl->ff->fileset);
      break;
    case 'E':
      (void)new_exclude(jcr->fd_impl->ff->fileset);
      break;
    case 'N': /* Null */
      state = state_none;
      break;
    case 'F': /* File */
      state = state_include;
      AddFileToFileset(jcr, item, true, jcr->fd_impl->ff->fileset);
      break;
    case 'P': /* Plugin */
      state = state_include;
      AddFileToFileset(jcr, item, false, jcr->fd_impl->ff->fileset);
      break;
    case 'R': /* Regex */
      state = AddRegexToFileset(jcr, item, subcode);
      break;
    case 'X': /* Filetype or Drive type */
      current_opts = start_options(ff);
      state = state_options;
      if (subcode == ' ') {
        current_opts->fstype.append(strdup(item));
      } else if (subcode == 'D') {
        current_opts->Drivetype.append(strdup(item));
      } else {
        state = state_error;
      }
      break;
    case 'W': /* Wild cards */
      state = AddWildToFileset(jcr, item, subcode);
      break;
    case 'O': /* Options */
      state = AddOptionsFlagsToFileset(jcr, item);
      break;
    case 'Z': /* Ignore dir */
      state = state_include;
      fileset->incexe->ignoredir.append(strdup(item));
      break;
    case 'D':
      current_opts = start_options(ff);
      state = state_options;
      break;
    case 'T':
      current_opts = start_options(ff);
      state = state_options;
      break;
    case 'G': /* Plugin command for this Option block */
      current_opts = start_options(ff);
      current_opts->plugin = strdup(item);
      state = state_options;
      break;
    default:
      Jmsg(jcr, M_FATAL, 0, T_("Invalid FileSet command: %s\n"), item);
      state = state_error;
      break;
  }
  ff->fileset->state = state;
}

bool TermFileset(JobControlRecord* jcr)
{
  findFILESET* fileset;

  fileset = jcr->fd_impl->ff->fileset;
#ifdef HAVE_WIN32
  /* Expand the fileset to include all drive letters when the fileset includes a
   * File = / entry. */
  if (!expand_win32_fileset(jcr->fd_impl->ff->fileset)) { return false; }

  // Exclude entries in NotToBackup registry key
  if (!exclude_win32_not_to_backup_registry_entries(jcr, jcr->fd_impl->ff)) {
    return false;
  }
#endif

  // Generate bEventPluginCommand events for each Options Plugin.
  for (int i = 0; i < fileset->include_list.size(); i++) {
    findIncludeExcludeItem* incexe
        = (findIncludeExcludeItem*)fileset->include_list.get(i);

    for (int j = 0; j < incexe->opts_list.size(); j++) {
      findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);

      if (fo->plugin) {
        GeneratePluginEvent(jcr, bEventPluginCommand, (void*)fo->plugin);
      }
    }
  }

  return jcr->fd_impl->ff->fileset->state != state_error;
}
} /* namespace filedaemon */
