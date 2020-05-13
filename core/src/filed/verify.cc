/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2020 Bareos GmbH & Co. KG

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
 */
/**
 * @file
 * Verify files.
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/jcr_private.h"
#include "findlib/find.h"
#include "findlib/attribs.h"
#include "lib/attribs.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/util.h"

namespace filedaemon {

#ifdef HAVE_DARWIN_OS
const bool have_darwin_os = true;
#else
const bool have_darwin_os = false;
#endif

static int VerifyFile(JobControlRecord* jcr, FindFilesPacket* ff_pkt, bool);
static int ReadDigest(BareosWinFilePacket* bfd,
                      DIGEST* digest,
                      JobControlRecord* jcr);
static bool calculate_file_chksum(JobControlRecord* jcr,
                                  FindFilesPacket* ff_pkt,
                                  DIGEST** digest,
                                  int* digest_stream,
                                  char** digest_buf,
                                  const char** digest_name);

/**
 * Find all the requested files and send attributes
 * to the Director.
 *
 */
void DoVerify(JobControlRecord* jcr)
{
  jcr->setJobStatus(JS_Running);
  jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
  if ((jcr->impl->big_buf = (char*)malloc(jcr->buf_size)) == NULL) {
    Jmsg1(jcr, M_ABORT, 0, _("Cannot malloc %d network read buffer\n"),
          DEFAULT_NETWORK_BUFFER_SIZE);
  }
  SetFindOptions((FindFilesPacket*)jcr->impl->ff, jcr->impl->incremental,
                 jcr->impl->since_time);
  Dmsg0(10, "Start find files\n");
  /* Subroutine VerifyFile() is called for each file */
  FindFiles(jcr, (FindFilesPacket*)jcr->impl->ff, VerifyFile, NULL);
  Dmsg0(10, "End find files\n");

  if (jcr->impl->big_buf) {
    free(jcr->impl->big_buf);
    jcr->impl->big_buf = NULL;
  }
  jcr->setJobStatus(JS_Terminated);
}

/**
 * Called here by find() for each file.
 *
 *  Find the file, compute the MD5 or SHA1 and send it back to the Director
 */
static int VerifyFile(JobControlRecord* jcr,
                      FindFilesPacket* ff_pkt,
                      bool top_level)
{
  PoolMem attribs(PM_NAME), attribsEx(PM_NAME);
  int status;
  BareosSocket* dir;

  if (JobCanceled(jcr)) { return 0; }

  dir = jcr->dir_bsock;
  jcr->impl->num_files_examined++; /* bump total file count */

  switch (ff_pkt->type) {
    case FT_LNKSAVED: /* Hard linked, file already saved */
      Dmsg2(30, "FT_LNKSAVED saving: %s => %s\n", ff_pkt->fname, ff_pkt->link);
      break;
    case FT_REGE:
      Dmsg1(30, "FT_REGE saving: %s\n", ff_pkt->fname);
      break;
    case FT_REG:
      Dmsg1(30, "FT_REG saving: %s\n", ff_pkt->fname);
      break;
    case FT_LNK:
      Dmsg2(30, "FT_LNK saving: %s -> %s\n", ff_pkt->fname, ff_pkt->link);
      break;
    case FT_DIRBEGIN:
      jcr->impl->num_files_examined--; /* correct file count */
      return 1;                        /* ignored */
    case FT_REPARSE:
    case FT_JUNCTION:
    case FT_DIREND:
      Dmsg1(30, "FT_DIR saving: %s\n", ff_pkt->fname);
      break;
    case FT_SPEC:
      Dmsg1(30, "FT_SPEC saving: %s\n", ff_pkt->fname);
      break;
    case FT_RAW:
      Dmsg1(30, "FT_RAW saving: %s\n", ff_pkt->fname);
      break;
    case FT_FIFO:
      Dmsg1(30, "FT_FIFO saving: %s\n", ff_pkt->fname);
      break;
    case FT_NOACCESS: {
      BErrNo be;
      be.SetErrno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not access %s: ERR=%s\n"),
           ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
    }
    case FT_NOFOLLOW: {
      BErrNo be;
      be.SetErrno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not follow link %s: ERR=%s\n"),
           ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
    }
    case FT_NOSTAT: {
      BErrNo be;
      be.SetErrno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not stat %s: ERR=%s\n"),
           ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
    }
    case FT_DIRNOCHG:
    case FT_NOCHG:
      Jmsg(jcr, M_SKIPPED, 1, _("     Unchanged file skipped: %s\n"),
           ff_pkt->fname);
      return 1;
    case FT_ISARCH:
      Jmsg(jcr, M_SKIPPED, 1, _("     Archive file skipped: %s\n"),
           ff_pkt->fname);
      return 1;
    case FT_NORECURSE:
      Jmsg(jcr, M_SKIPPED, 1,
           _("     Recursion turned off. Directory skipped: %s\n"),
           ff_pkt->fname);
      ff_pkt->type = FT_DIREND; /* directory entry was backed up */
      break;
    case FT_NOFSCHG:
      Jmsg(jcr, M_SKIPPED, 1,
           _("     File system change prohibited. Directory skipped: %s\n"),
           ff_pkt->fname);
      return 1;
    case FT_PLUGIN_CONFIG:
    case FT_RESTORE_FIRST:
      return 1; /* silently skip */
    case FT_NOOPEN: {
      BErrNo be;
      be.SetErrno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not open directory %s: ERR=%s\n"),
           ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
    }
    default:
      Jmsg(jcr, M_NOTSAVED, 0, _("     Unknown file type %d: %s\n"),
           ff_pkt->type, ff_pkt->fname);
      jcr->JobErrors++;
      return 1;
  }

  /* Encode attributes and possibly extend them */
  EncodeStat(attribs.c_str(), &ff_pkt->statp, sizeof(ff_pkt->statp),
             ff_pkt->LinkFI, 0);
  encode_attribsEx(jcr, attribsEx.c_str(), ff_pkt);

  jcr->lock();
  jcr->JobFiles++; /* increment number of files sent */
  PmStrcpy(jcr->impl->last_fname, ff_pkt->fname);
  jcr->unlock();

  /*
   * Send file attributes to Director
   *   File_index
   *   Stream
   *   Verify Options
   *   Filename (full path)
   *   Encoded attributes
   *   Link name (if type==FT_LNK)
   * For a directory, link is the same as fname, but with trailing
   * slash. For a linked file, link is the link.
   */
  /*
   * Send file attributes to Director (note different format than for Storage)
   */
  Dmsg2(400, "send Attributes inx=%d fname=%s\n", jcr->JobFiles, ff_pkt->fname);
  if (ff_pkt->type == FT_LNK || ff_pkt->type == FT_LNKSAVED) {
    status = dir->fsend("%d %d %s %s%c%s%c%s%c", jcr->JobFiles,
                        STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts,
                        ff_pkt->fname, 0, attribs.c_str(), 0, ff_pkt->link, 0);
  } else if (ff_pkt->type == FT_DIREND || ff_pkt->type == FT_REPARSE ||
             ff_pkt->type == FT_JUNCTION) {
    /*
     * Here link is the canonical filename (i.e. with trailing slash)
     */
    status =
        dir->fsend("%d %d %s %s%c%s%c%c", jcr->JobFiles, STREAM_UNIX_ATTRIBUTES,
                   ff_pkt->VerifyOpts, ff_pkt->link, 0, attribs.c_str(), 0, 0);
  } else {
    status =
        dir->fsend("%d %d %s %s%c%s%c%c", jcr->JobFiles, STREAM_UNIX_ATTRIBUTES,
                   ff_pkt->VerifyOpts, ff_pkt->fname, 0, attribs.c_str(), 0, 0);
  }
  Dmsg2(20, "filed>dir: attribs len=%d: msg=%s\n", dir->message_length,
        dir->msg);
  if (!status) {
    Jmsg(jcr, M_FATAL, 0, _("Network error in send to Director: ERR=%s\n"),
         BnetStrerror(dir));
    return 0;
  }

  if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode) &&
      (BitIsSet(FO_MD5, ff_pkt->flags) || BitIsSet(FO_SHA1, ff_pkt->flags) ||
       BitIsSet(FO_SHA256, ff_pkt->flags) ||
       BitIsSet(FO_SHA512, ff_pkt->flags))) {
    int digest_stream = STREAM_NONE;
    DIGEST* digest = NULL;
    char* digest_buf = NULL;
    const char* digest_name = NULL;

    if (calculate_file_chksum(jcr, ff_pkt, &digest, &digest_stream, &digest_buf,
                              &digest_name)) {
      /*
       * Did digest initialization fail?
       */
      if (digest_stream != STREAM_NONE && digest == NULL) {
        Jmsg(jcr, M_WARNING, 0, _("%s digest initialization failed\n"),
             stream_to_ascii(digest_stream));
      } else if (digest && digest_buf) {
        Dmsg3(400, "send inx=%d %s=%s\n", jcr->JobFiles, digest_name,
              digest_buf);
        dir->fsend("%d %d %s *%s-%d*", jcr->JobFiles, digest_stream, digest_buf,
                   digest_name, jcr->JobFiles);
        Dmsg3(20, "filed>dir: %s len=%d: msg=%s\n", digest_name,
              dir->message_length, dir->msg);
      }
    }

    /*
     * Cleanup.
     */
    if (digest_buf) { free(digest_buf); }

    if (digest) { CryptoDigestFree(digest); }
  }

  return 1;
}

/**
 * Compute message digest for the file specified by ff_pkt.
 * In case of errors we need the job control record and file name.
 */
int DigestFile(JobControlRecord* jcr, FindFilesPacket* ff_pkt, DIGEST* digest)
{
  BareosWinFilePacket bfd;

  binit(&bfd);

  int noatime = BitIsSet(FO_NOATIME, ff_pkt->flags) ? O_NOATIME : 0;

  if ((bopen(&bfd, ff_pkt->fname, O_RDONLY | O_BINARY | noatime, 0,
             ff_pkt->statp.st_rdev)) < 0) {
    ff_pkt->ff_errno = errno;
    BErrNo be;
    be.SetErrno(bfd.BErrNo);
    Dmsg2(100, "Cannot open %s: ERR=%s\n", ff_pkt->fname, be.bstrerror());
    Jmsg(jcr, M_ERROR, 1, _("     Cannot open %s: ERR=%s.\n"), ff_pkt->fname,
         be.bstrerror());
    return 1;
  }
  ReadDigest(&bfd, digest, jcr);
  bclose(&bfd);

  if (have_darwin_os) {
    /*
     * Open resource fork if necessary
     */
    if (BitIsSet(FO_HFSPLUS, ff_pkt->flags) && ff_pkt->hfsinfo.rsrclength > 0) {
      if (BopenRsrc(&bfd, ff_pkt->fname, O_RDONLY | O_BINARY, 0) < 0) {
        ff_pkt->ff_errno = errno;
        BErrNo be;
        Jmsg(jcr, M_ERROR, -1,
             _("     Cannot open resource fork for %s: ERR=%s.\n"),
             ff_pkt->fname, be.bstrerror());
        if (IsBopen(&ff_pkt->bfd)) { bclose(&ff_pkt->bfd); }
        return 1;
      }
      ReadDigest(&bfd, digest, jcr);
      bclose(&bfd);
    }

    if (digest && BitIsSet(FO_HFSPLUS, ff_pkt->flags)) {
      CryptoDigestUpdate(digest, (uint8_t*)ff_pkt->hfsinfo.fndrinfo, 32);
    }
  }
  return 0;
}

/**
 * Read message digest of bfd, updating digest
 * In case of errors we need the job control record and file name.
 */
static int ReadDigest(BareosWinFilePacket* bfd,
                      DIGEST* digest,
                      JobControlRecord* jcr)
{
  char buf[DEFAULT_NETWORK_BUFFER_SIZE];
  int64_t n;
  int64_t bufsiz = (int64_t)sizeof(buf);
  FindFilesPacket* ff_pkt = (FindFilesPacket*)jcr->impl->ff;
  uint64_t fileAddr = 0; /* file address */


  Dmsg0(50, "=== ReadDigest\n");
  while ((n = bread(bfd, buf, bufsiz)) > 0) {
    /* Check for sparse blocks */
    if (BitIsSet(FO_SPARSE, ff_pkt->flags)) {
      bool allZeros = false;
      if ((n == bufsiz && fileAddr + n < (uint64_t)ff_pkt->statp.st_size) ||
          ((ff_pkt->type == FT_RAW || ff_pkt->type == FT_FIFO) &&
           (uint64_t)ff_pkt->statp.st_size == 0)) {
        allZeros = IsBufZero(buf, bufsiz);
      }
      fileAddr += n; /* update file address */
      /* Skip any block of all zeros */
      if (allZeros) { continue; /* skip block of zeros */ }
    }

    CryptoDigestUpdate(digest, (uint8_t*)buf, n);

    /* Can be used by BaseJobs or with accurate, update only for Verify
     * jobs
     */
    if (jcr->is_JobType(JT_VERIFY)) { jcr->JobBytes += n; }
    jcr->ReadBytes += n;
  }
  if (n < 0) {
    BErrNo be;
    be.SetErrno(bfd->BErrNo);
    Dmsg2(100, "Error reading file %s: ERR=%s\n", jcr->impl->last_fname,
          be.bstrerror());
    Jmsg(jcr, M_ERROR, 1, _("Error reading file %s: ERR=%s\n"),
         jcr->impl->last_fname, be.bstrerror());
    jcr->JobErrors++;
    return -1;
  }
  return 0;
}

/**
 * Calculate the chksum of a whole file and updates:
 * - digest
 * - digest_stream
 * - digest_buffer
 * - digest_name
 *
 * Returns: true   if digest calculation succeeded.
 *          false  if digest calculation failed.
 */
static bool calculate_file_chksum(JobControlRecord* jcr,
                                  FindFilesPacket* ff_pkt,
                                  DIGEST** digest,
                                  int* digest_stream,
                                  char** digest_buf,
                                  const char** digest_name)
{
  /*
   * Create our digest context.
   * If this fails, the digest will be set to NULL and not used.
   */
  if (BitIsSet(FO_MD5, ff_pkt->flags)) {
    *digest = crypto_digest_new(jcr, CRYPTO_DIGEST_MD5);
    *digest_stream = STREAM_MD5_DIGEST;
  } else if (BitIsSet(FO_SHA1, ff_pkt->flags)) {
    *digest = crypto_digest_new(jcr, CRYPTO_DIGEST_SHA1);
    *digest_stream = STREAM_SHA1_DIGEST;
  } else if (BitIsSet(FO_SHA256, ff_pkt->flags)) {
    *digest = crypto_digest_new(jcr, CRYPTO_DIGEST_SHA256);
    *digest_stream = STREAM_SHA256_DIGEST;
  } else if (BitIsSet(FO_SHA512, ff_pkt->flags)) {
    *digest = crypto_digest_new(jcr, CRYPTO_DIGEST_SHA512);
    *digest_stream = STREAM_SHA512_DIGEST;
  }

  /*
   * compute MD5 or SHA1 hash
   */
  if (*digest) {
    uint32_t size;
    char md[CRYPTO_DIGEST_MAX_SIZE];

    size = sizeof(md);
    if (DigestFile(jcr, ff_pkt, *digest) != 0) {
      jcr->JobErrors++;
      return false;
    }

    if (CryptoDigestFinalize(*digest, (uint8_t*)md, &size)) {
      *digest_buf = (char*)malloc(BASE64_SIZE(size));
      *digest_name = crypto_digest_name(*digest);

      BinToBase64(*digest_buf, BASE64_SIZE(size), md, size, true);
    }
  }

  return true;
}

/**
 * Compare a files chksum against a stored chksum.
 *
 * Returns: true   if chksum matches.
 *          false  if chksum is different.
 */
bool CalculateAndCompareFileChksum(JobControlRecord* jcr,
                                   FindFilesPacket* ff_pkt,
                                   const char* fname,
                                   const char* chksum)
{
  DIGEST* digest = NULL;
  int digest_stream = STREAM_NONE;
  char* digest_buf = NULL;
  const char* digest_name;
  bool retval = false;

  if (calculate_file_chksum(jcr, ff_pkt, &digest, &digest_stream, &digest_buf,
                            &digest_name)) {
    if (digest_stream != STREAM_NONE && digest == NULL) {
      Jmsg(jcr, M_WARNING, 0, _("%s digest initialization failed\n"),
           stream_to_ascii(digest_stream));
    } else if (digest && digest_buf) {
      if (!bstrcmp(digest_buf, chksum)) {
        Dmsg4(100, "%s      %s chksum  diff. Cat: %s File: %s\n", fname,
              digest_name, chksum, digest_buf);
      } else {
        retval = true;
      }
    }

    if (digest_buf) { free(digest_buf); }

    if (digest) { CryptoDigestFree(digest); }
  }

  return retval;
}
} /* namespace filedaemon */
