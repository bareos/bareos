/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
 * Kern E. Sibbald, December MMI
 */
/**
 * @file
 * Old style
 *
 * Routines used to keep and match include and exclude
 * filename/pathname patterns.
 *
 * Note, this file is used for the old style include and
 * excludes, so is deprecated. The new style code is found in
 * src/filed/fileset.c.
 *
 * This code is still used for lists in testls and bextract.
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "find.h"
#include "include/ch.h"

#include <sys/types.h>
#include "findlib/match.h"
#include "findlib/find_one.h"
#include "lib/edit.h"

#ifndef FNM_LEADING_DIR
#define FNM_LEADING_DIR 0
#endif

/* Fold case in fnmatch() on Win32 */
#ifdef HAVE_WIN32
static const int fnmode = FNM_CASEFOLD;
#else
static const int fnmode = 0;
#endif

bool MatchFiles(JobControlRecord* jcr,
                FindFilesPacket* ff,
                int FileSave(JobControlRecord*, FindFilesPacket* ff_pkt, bool))
{
  ff->FileSave = FileSave;

  struct s_included_file* inc = NULL;

  /* This is the old deprecated way */
  while (!JobCanceled(jcr) && (inc = get_next_included_file(ff, inc))) {
    /* Copy options for this file */
    bstrncat(ff->VerifyOpts, inc->VerifyOpts, sizeof(ff->VerifyOpts));
    Dmsg1(100, "FindFiles: file=%s\n", inc->fname);
    if (!FileIsExcluded(ff, inc->fname)) {
      if (FindOneFile(jcr, ff, FileSave, inc->fname, (dev_t)-1, 1) == 0) {
        return false; /* error return */
      }
    }
  }
  return true;
}

/**
 * Done doing filename matching, release all
 *  resources used.
 */
void TermIncludeExcludeFiles(FindFilesPacket* ff)
{
  struct s_included_file *inc, *next_inc;
  struct s_excluded_file *exc, *next_exc;

  for (inc = ff->included_files_list; inc;) {
    next_inc = inc->next;
    if (inc->size_match) { free(inc->size_match); }
    free(inc);
    inc = next_inc;
  }
  ff->included_files_list = NULL;

  for (exc = ff->excluded_files_list; exc;) {
    next_exc = exc->next;
    free(exc);
    exc = next_exc;
  }
  ff->excluded_files_list = NULL;

  for (exc = ff->excluded_paths_list; exc;) {
    next_exc = exc->next;
    free(exc);
    exc = next_exc;
  }
  ff->excluded_paths_list = NULL;
}

/**
 * Add a filename to list of included files
 */
void AddFnameToIncludeList(FindFilesPacket* ff, int prefixed, const char* fname)
{
  int len, j;
  struct s_included_file* inc;
  char* p;
  const char* rp;
  char size[50];

  len = strlen(fname);

  inc =
      (struct s_included_file*)malloc(sizeof(struct s_included_file) + len + 1);
  memset(inc, 0, sizeof(struct s_included_file) + len + 1);
  inc->VerifyOpts[0] = 'V';
  inc->VerifyOpts[1] = ':';
  inc->VerifyOpts[2] = 0;

  /* prefixed = preceded with options */
  if (prefixed) {
    for (rp = fname; *rp && *rp != ' '; rp++) {
      switch (*rp) {
        case 'A':
          SetBit(FO_ACL, inc->options);
          break;
        case 'a': /* alway replace */
        case '0': /* no option */
          break;
        case 'c':
          SetBit(FO_CHKCHANGES, inc->options);
          break;
        case 'd':
          switch (*(rp + 1)) {
            case '1':
              inc->shadow_type = check_shadow_local_warn;
              rp++;
              break;
            case '2':
              inc->shadow_type = check_shadow_local_remove;
              rp++;
              break;
            case '3':
              inc->shadow_type = check_shadow_global_warn;
              rp++;
              break;
            case '4':
              inc->shadow_type = check_shadow_global_remove;
              rp++;
              break;
          }
          break;
        case 'e':
          SetBit(FO_EXCLUDE, inc->options);
          break;
        case 'E':
          switch (*(rp + 1)) {
            case '3':
              inc->cipher = CRYPTO_CIPHER_3DES_CBC;
              rp++;
              break;
            case 'a':
              switch (*(rp + 2)) {
                case '1':
                  inc->cipher = CRYPTO_CIPHER_AES_128_CBC;
                  rp += 2;
                  break;
                case '2':
                  inc->cipher = CRYPTO_CIPHER_AES_192_CBC;
                  rp += 2;
                  break;
                case '3':
                  inc->cipher = CRYPTO_CIPHER_AES_256_CBC;
                  rp += 2;
                  break;
              }
              break;
            case 'b':
              inc->cipher = CRYPTO_CIPHER_BLOWFISH_CBC;
              rp++;
              break;
            case 'c':
              switch (*(rp + 2)) {
                case '1':
                  inc->cipher = CRYPTO_CIPHER_CAMELLIA_128_CBC;
                  rp += 2;
                  break;
                case '2':
                  inc->cipher = CRYPTO_CIPHER_CAMELLIA_192_CBC;
                  rp += 2;
                  break;
                case '3':
                  inc->cipher = CRYPTO_CIPHER_CAMELLIA_256_CBC;
                  rp += 2;
                  break;
              }
              break;
            case 'f':
              SetBit(FO_FORCE_ENCRYPT, inc->options);
              rp++;
              break;
            case 'h':
              switch (*(rp + 2)) {
                case '1':
                  inc->cipher = CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1;
                  rp += 2;
                  break;
                case '2':
                  inc->cipher = CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1;
                  rp += 2;
                  break;
              }
          }
          break;
        case 'f':
          SetBit(FO_MULTIFS, inc->options);
          break;
        case 'H': /* no hard link handling */
          SetBit(FO_NO_HARDLINK, inc->options);
          break;
        case 'h': /* no recursion */
          SetBit(FO_NO_RECURSION, inc->options);
          break;
        case 'i':
          SetBit(FO_IGNORECASE, inc->options);
          break;
        case 'K':
          SetBit(FO_NOATIME, inc->options);
          break;
        case 'k':
          SetBit(FO_KEEPATIME, inc->options);
          break;
        case 'M': /* MD5 */
          SetBit(FO_MD5, inc->options);
          break;
        case 'm':
          SetBit(FO_MTIMEONLY, inc->options);
          break;
        case 'N':
          SetBit(FO_HONOR_NODUMP, inc->options);
          break;
        case 'n':
          SetBit(FO_NOREPLACE, inc->options);
          break;
        case 'p': /* use portable data format */
          SetBit(FO_PORTABLE, inc->options);
          break;
        case 'R': /* Resource forks and Finder Info */
          SetBit(FO_HFSPLUS, inc->options);
          break;
        case 'r': /* read fifo */
          SetBit(FO_READFIFO, inc->options);
          break;
        case 'S':
          switch (*(rp + 1)) {
            case '1':
              SetBit(FO_SHA1, inc->options);
              rp++;
              break;
#ifdef HAVE_SHA2
            case '2':
              SetBit(FO_SHA256, inc->options);
              rp++;
              break;
            case '3':
              SetBit(FO_SHA512, inc->options);
              rp++;
              break;
#endif
            default:
              /*
               * If 2 or 3 is seen here, SHA2 is not configured, so
               *  eat the option, and drop back to SHA-1.
               */
              if (rp[1] == '2' || rp[1] == '3') { rp++; }
              SetBit(FO_SHA1, inc->options);
              break;
          }
          break;
        case 's':
          SetBit(FO_SPARSE, inc->options);
          break;
        case 'V': /* verify options */
          /* Copy Verify Options */
          for (j = 0; *rp && *rp != ':'; rp++) {
            inc->VerifyOpts[j] = *rp;
            if (j < (int)sizeof(inc->VerifyOpts) - 1) { j++; }
          }
          inc->VerifyOpts[j] = 0;
          break;
        case 'W':
          SetBit(FO_ENHANCEDWILD, inc->options);
          break;
        case 'w':
          SetBit(FO_IF_NEWER, inc->options);
          break;
        case 'x':
          SetBit(FO_NO_AUTOEXCL, inc->options);
          break;
        case 'X':
          SetBit(FO_XATTR, inc->options);
          break;
        case 'Z': /* Compression */
          rp++;   /* Skip Z */
          if (*rp >= '0' && *rp <= '9') {
            SetBit(FO_COMPRESS, inc->options);
            inc->algo = COMPRESS_GZIP;
            inc->level = *rp - '0';
          } else if (*rp == 'o') {
            SetBit(FO_COMPRESS, inc->options);
            inc->algo = COMPRESS_LZO1X;
            inc->level = 1; /* Not used with LZO */
          } else if (*rp == 'f') {
            if (rp[1] == 'f') {
              rp++; /* Skip f */
              SetBit(FO_COMPRESS, inc->options);
              inc->algo = COMPRESS_FZFZ;
              inc->level = 1; /* Not used with libfzlib */
            } else if (rp[1] == '4') {
              rp++; /* Skip f */
              SetBit(FO_COMPRESS, inc->options);
              inc->algo = COMPRESS_FZ4L;
              inc->level = 1; /* Not used with libfzlib */
            } else if (rp[1] == 'h') {
              rp++; /* Skip f */
              SetBit(FO_COMPRESS, inc->options);
              inc->algo = COMPRESS_FZ4H;
              inc->level = 1; /* Not used with libfzlib */
            }
          }
          Dmsg2(200, "Compression alg=%d level=%d\n", inc->algo, inc->level);
          break;
        case 'z': /* Min, Max or Approx size or Size range */
          rp++;   /* Skip z */
          for (j = 0; *rp && *rp != ':'; rp++) {
            size[j] = *rp;
            if (j < (int)sizeof(size) - 1) { j++; }
          }
          size[j] = 0;
          if (!inc->size_match) {
            inc->size_match =
                (struct s_sz_matching*)malloc(sizeof(struct s_sz_matching));
          }
          if (!ParseSizeMatch(size, inc->size_match)) {
            Emsg1(M_ERROR, 0, _("Unparseable size option: %s\n"), size);
          }
          break;
        default:
          Emsg1(M_ERROR, 0, _("Unknown include/exclude option: %c\n"), *rp);
          break;
      }
    }
    /* Skip past space(s) */
    for (; *rp == ' '; rp++) {}
  } else {
    rp = fname;
  }

  strcpy(inc->fname, rp);
  p = inc->fname;
  len = strlen(p);
  /* Zap trailing slashes.  */
  p += len - 1;
  while (p > inc->fname && IsPathSeparator(*p)) {
    *p-- = 0;
    len--;
  }
  inc->len = len;
  /* Check for wild cards */
  inc->pattern = 0;
  for (p = inc->fname; *p; p++) {
    if (*p == '*' || *p == '[' || *p == '?') {
      inc->pattern = 1;
      break;
    }
  }
#if defined(HAVE_WIN32)
  /* Convert any \'s into /'s */
  for (p = inc->fname; *p; p++) {
    if (*p == '\\') { *p = '/'; }
  }
#endif
  inc->next = NULL;
  /* Chain this one on the end of the list */
  if (!ff->included_files_list) {
    /* First one, so set head */
    ff->included_files_list = inc;
  } else {
    struct s_included_file* next;
    /* Walk to end of list */
    for (next = ff->included_files_list; next->next; next = next->next) {}
    next->next = inc;
  }
  Dmsg4(100, "add_fname_to_include prefix=%d compres=%d alg= %d fname=%s\n",
        prefixed, BitIsSet(FO_COMPRESS, inc->options), inc->algo, inc->fname);
}

/**
 * We add an exclude name to either the exclude path
 *  list or the exclude filename list.
 */
void AddFnameToExcludeList(FindFilesPacket* ff, const char* fname)
{
  int len;
  struct s_excluded_file *exc, **list;

  Dmsg1(20, "Add name to exclude: %s\n", fname);

  if (first_path_separator(fname) != NULL) {
    list = &ff->excluded_paths_list;
  } else {
    list = &ff->excluded_files_list;
  }

  len = strlen(fname);

  exc =
      (struct s_excluded_file*)malloc(sizeof(struct s_excluded_file) + len + 1);
  memset(exc, 0, sizeof(struct s_excluded_file) + len + 1);
  exc->next = *list;
  exc->len = len;
  strcpy(exc->fname, fname);
#if defined(HAVE_WIN32)
  /* Convert any \'s into /'s */
  for (char* p = exc->fname; *p; p++) {
    if (*p == '\\') { *p = '/'; }
  }
#endif
  *list = exc;
}


/**
 * Get next included file
 */
struct s_included_file* get_next_included_file(FindFilesPacket* ff,
                                               struct s_included_file* ainc)
{
  struct s_included_file* inc;

  if (ainc == NULL) {
    inc = ff->included_files_list;
  } else {
    inc = ainc->next;
  }
  /*
   * copy inc_options for this file into the ff packet
   */
  if (inc) {
    CopyBits(FO_MAX, inc->options, ff->flags);
    ff->Compress_algo = inc->algo;
    ff->Compress_level = inc->level;
  }
  return inc;
}

/**
 * Walk through the included list to see if this
 *  file is included possibly with wild-cards.
 */
bool FileIsIncluded(FindFilesPacket* ff, const char* file)
{
  struct s_included_file* inc = ff->included_files_list;
  int len;

  for (; inc; inc = inc->next) {
    if (inc->pattern) {
      if (fnmatch(inc->fname, file, fnmode | FNM_LEADING_DIR) == 0) {
        return true;
      }
      continue;
    }
    /*
     * No wild cards. We accept a match to the
     *  end of any component.
     */
    Dmsg2(900, "pat=%s file=%s\n", inc->fname, file);
    len = strlen(file);
    if (inc->len == len && bstrcmp(inc->fname, file)) { return true; }
    if (inc->len < len && IsPathSeparator(file[inc->len]) &&
        bstrncmp(inc->fname, file, inc->len)) {
      return true;
    }
    if (inc->len == 1 && IsPathSeparator(inc->fname[0])) { return true; }
  }
  return false;
}

/**
 * This is the workhorse of excluded_file().
 * Determine if the file is excluded or not.
 */
static bool FileInExcludedList(struct s_excluded_file* exc, const char* file)
{
  if (exc == NULL) { Dmsg0(900, "exc is NULL\n"); }
  for (; exc; exc = exc->next) {
    if (fnmatch(exc->fname, file, fnmode | FNM_PATHNAME) == 0) {
      Dmsg2(900, "Match exc pat=%s: file=%s:\n", exc->fname, file);
      return true;
    }
    Dmsg2(900, "No match exc pat=%s: file=%s:\n", exc->fname, file);
  }
  return false;
}

/**
 * Walk through the excluded lists to see if this
 *  file is excluded, or if it matches a component
 *  of an excluded directory.
 */
bool FileIsExcluded(FindFilesPacket* ff, const char* file)
{
  const char* p;

#if defined(HAVE_WIN32)
  /*
   *  ***NB*** this removes the drive from the exclude
   *  rule.  Why?????
   */
  if (file[1] == ':') { file += 2; }
#endif

  if (FileInExcludedList(ff->excluded_paths_list, file)) { return true; }

  /* Try each component */
  for (p = file; *p; p++) {
    /* Match from the beginning of a component only */
    if ((p == file || (!IsPathSeparator(*p) && IsPathSeparator(p[-1]))) &&
        FileInExcludedList(ff->excluded_files_list, p)) {
      return true;
    }
  }
  return false;
}

/**
 * Parse a size matching fileset option.
 */
bool ParseSizeMatch(const char* size_match_pattern,
                    struct s_sz_matching* size_matching)
{
  bool retval = false;
  char *private_copy, *bp;

  /*
   * Make a private copy of the input string.
   * As we manipulate the input and size_to_uint64
   * eats its input.
   */
  private_copy = strdup(size_match_pattern);

  /*
   * Empty the matching arguments.
   */
  static const s_sz_matching empty_s_sz_matching{};
  *size_matching = empty_s_sz_matching;

  /*
   * See if the size is a range e.g. there is a - in the
   * match pattern. As a size of a file can never be negative
   * this is a workable solution.
   */
  if ((bp = strchr(private_copy, '-')) != NULL) {
    *bp++ = '\0';
    size_matching->type = size_match_range;
    if (!size_to_uint64(private_copy, &size_matching->begin_size)) {
      goto bail_out;
    }
    if (!size_to_uint64(bp, &size_matching->end_size)) { goto bail_out; }
  } else {
    switch (*private_copy) {
      case '<':
        size_matching->type = size_match_smaller;
        if (!size_to_uint64(private_copy + 1, &size_matching->begin_size)) {
          goto bail_out;
        }
        break;
      case '>':
        size_matching->type = size_match_greater;
        if (!size_to_uint64(private_copy + 1, &size_matching->begin_size)) {
          goto bail_out;
        }
        break;
      default:
        size_matching->type = size_match_approx;
        if (!size_to_uint64(private_copy, &size_matching->begin_size)) {
          goto bail_out;
        }
        break;
    }
  }

  retval = true;

bail_out:
  free(private_copy);
  return retval;
}
