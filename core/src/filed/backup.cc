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
// Kern Sibbald, March MM
/**
 * @file
 * Bareos File Daemon  backup.c  send file attributes and data to the Storage
 * daemon.
 */
#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/accurate.h"
#include "filed/compression.h"
#include "filed/crypto.h"
#include "filed/heartbeat.h"
#include "filed/backup.h"
#include "filed/filed_jcr_impl.h"
#include "include/ch.h"
#include "findlib/attribs.h"
#include "findlib/hardlink.h"
#include "findlib/find_one.h"
#include "lib/attribs.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "lib/btimers.h"
#include "lib/message.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/serial.h"
#include "lib/compression.h"

#include "lib/channel.h"
#include "lib/network_order.h"

#include <cstring>

namespace filedaemon {

// allow to set the allowed maximum to something lower for testing purposes
#ifndef MAXIMUM_ALLOWED_FILES_PER_JOB
#  define MAXIMUM_ALLOWED_FILES_PER_JOB \
    std::numeric_limits<decltype(JobControlRecord::JobFiles)>::max()
#endif

#ifdef HAVE_DARWIN_OS
const bool have_darwin_os = true;
#else
const bool have_darwin_os = false;
#endif

#if defined(HAVE_ACL)
const bool have_acl = true;
#else
const bool have_acl = false;
#endif

#if defined(HAVE_XATTR)
const bool have_xattr = true;
#else
const bool have_xattr = false;
#endif

#ifndef compressBound
#  define compressBound(sourceLen) \
    (sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13)
#endif

/* Forward referenced functions */

static int send_data(JobControlRecord* jcr,
                     int stream,
                     FindFilesPacket* ff_pkt,
                     DIGEST* digest,
                     DIGEST* signature_digest);
bool EncodeAndSendAttributes(JobControlRecord* jcr,
                             FindFilesPacket* ff_pkt,
                             int& data_stream);
#if defined(WIN32_VSS)
static void CloseVssBackupSession(JobControlRecord* jcr);
#endif

/**
 * Find all the requested files and send them
 * to the Storage daemon.
 *
 * Note, we normally carry on a one-way
 * conversation from this point on with the SD, sfd_imply blasting
 * data to him.  To properly know what is going on, we
 * also run a "heartbeat" monitor which reads the socket and
 * reacts accordingly (at the moment it has nothing to do
 * except echo the heartbeat to the Director).
 */
bool BlastDataToStorageDaemon(JobControlRecord* jcr, crypto_cipher_t cipher)
{
  BareosSocket* sd;
  bool ok = true;

  sd = jcr->store_bsock;

  jcr->setJobStatusWithPriorityCheck(JS_Running);
  Jmsg(jcr, M_INFO, 0, T_("Version: %s (%s) %s\n"), kBareosVersionStrings.Full,
       kBareosVersionStrings.Date, kBareosVersionStrings.GetOsInfo());

  Dmsg1(300, "filed: opened data connection %d to stored\n", sd->fd_);
  ClientResource* client = nullptr;
  {
    ResLocker _{my_config};
    client = (ClientResource*)my_config->GetNextRes(R_CLIENT, NULL);
  }
  uint32_t buf_size;
  if (client) {
    buf_size = client->max_network_buffer_size;
  } else {
    buf_size = 0; /* use default */
  }
  if (!sd->SetBufferSize(buf_size, BNET_SETBUF_WRITE)) {
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    Jmsg(jcr, M_FATAL, 0, T_("Cannot set buffer size FD->SD.\n"));
    return false;
  }

  jcr->buf_size = sd->message_length;

  if (!AdjustCompressionBuffers(jcr)) { return false; }

  if (!CryptoSessionStart(jcr, cipher)) { return false; }

  SetFindOptions((FindFilesPacket*)jcr->fd_impl->ff, jcr->fd_impl->incremental,
                 jcr->fd_impl->since_time);

  // In accurate mode, we overload the find_one check function
  if (jcr->accurate) {
    SetFindChangedFunction((FindFilesPacket*)jcr->fd_impl->ff,
                           AccurateCheckFile);
  }

  auto hb_send = MakeHeartbeatMonitor(jcr);

  if (have_acl) { jcr->fd_impl->acl_data = std::make_unique<AclBuildData>(); }

  if (have_xattr) {
    jcr->fd_impl->xattr_data = std::make_unique<XattrBuildData>();
  }

  // Subroutine SaveFile() is called for each file
  if (!FindFiles(jcr, (FindFilesPacket*)jcr->fd_impl->ff, SaveFile,
                 PluginSave)) {
    ok = false; /* error */
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
  }

  if (have_acl && jcr->fd_impl->acl_data->nr_errors > 0) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Encountered %" PRIu32 " acl errors while doing backup\n"),
         jcr->fd_impl->acl_data->nr_errors);
  }
  if (have_xattr && jcr->fd_impl->xattr_data->nr_errors > 0) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Encountered %" PRIu32 " xattr errors while doing backup\n"),
         jcr->fd_impl->xattr_data->nr_errors);
  }

#if defined(WIN32_VSS)
  CloseVssBackupSession(jcr);
#endif

  AccurateFinish(jcr); /* send deleted or base file list to SD */

  hb_send.reset();

  sd->signal(BNET_EOD); /* end of sending data */

  if (jcr->fd_impl->big_buf) {
    free(jcr->fd_impl->big_buf);
    jcr->fd_impl->big_buf = NULL;
  }

  CleanupCompression(jcr);
  CryptoSessionEnd(jcr);

  Jmsg(jcr, M_INFO, 0, "finished sending data");

  Dmsg1(100, "end blast_data ok=%d\n", ok);
  return ok;
}

// Save OSX specific resource forks and finder info.
static inline bool SaveRsrcAndFinder(b_save_ctx& bsctx)
{
  char flags[FOPTS_BYTES];
  int rsrc_stream;
  BareosSocket* sd = bsctx.jcr->store_bsock;
  bool retval = false;

  if (bsctx.ff_pkt->hfsinfo.rsrclength > 0) {
    bool done = false;
    if constexpr (have_darwin_os) {
      if (BopenRsrc(&bsctx.ff_pkt->bfd, bsctx.ff_pkt->fname,
                    O_RDONLY | O_BINARY, 0)
          < 0) {
        done = true;
        bsctx.ff_pkt->ff_errno = errno;
        BErrNo be;
        Jmsg(bsctx.jcr, M_NOTSAVED, -1,
             T_("     Cannot open resource fork for \"%s\": ERR=%s.\n"),
             bsctx.ff_pkt->fname, be.bstrerror());
        bsctx.jcr->JobErrors++;
        if (IsBopen(&bsctx.ff_pkt->bfd)) { bclose(&bsctx.ff_pkt->bfd); }
      }
    }
    if (!done) {
      int status;

      memcpy(flags, bsctx.ff_pkt->flags, sizeof(flags));
      ClearBit(FO_COMPRESS, bsctx.ff_pkt->flags);
      ClearBit(FO_SPARSE, bsctx.ff_pkt->flags);
      ClearBit(FO_OFFSETS, bsctx.ff_pkt->flags);
      rsrc_stream = BitIsSet(FO_ENCRYPT, flags)
                        ? STREAM_ENCRYPTED_MACOS_FORK_DATA
                        : STREAM_MACOS_FORK_DATA;

      status = send_data(bsctx.jcr, rsrc_stream, bsctx.ff_pkt, bsctx.digest,
                         bsctx.signing_digest);

      memcpy(bsctx.ff_pkt->flags, flags, sizeof(flags));
      bclose(&bsctx.ff_pkt->bfd);
      if (!status) { goto bail_out; }
    }
  }

  Dmsg1(300, "Saving Finder Info for \"%s\"\n", bsctx.ff_pkt->fname);
  sd->fsend("%" PRIu32 " %" PRId32 " 0", bsctx.jcr->JobFiles,
            STREAM_HFSPLUS_ATTRIBUTES);
  Dmsg1(300, "filed>stored:header %s", sd->msg);
  PmMemcpy(sd->msg, bsctx.ff_pkt->hfsinfo.fndrinfo, 32);
  sd->message_length = 32;
  if (bsctx.digest) {
    CryptoDigestUpdate(bsctx.digest, (uint8_t*)sd->msg, sd->message_length);
  }
  if (bsctx.signing_digest) {
    CryptoDigestUpdate(bsctx.signing_digest, (uint8_t*)sd->msg,
                       sd->message_length);
  }
  sd->send();
  sd->signal(BNET_EOD);

  retval = true;

bail_out:
  return retval;
}

/**
 * Setup for digest handling. If this fails, the digest will be set to NULL
 * and not used. Note, the digest (file hash) can be any one of the four
 * algorithms below.
 *
 * The signing digest is a single algorithm depending on
 * whether or not we have SHA2.
 *   ****FIXME****  the signing algorithm should really be
 *   determined a different way!!!!!!  What happens if
 *   sha2 was available during backup but not restore?
 */
static inline bool SetupEncryptionDigests(b_save_ctx& bsctx)
{
  bool retval = false;
  // TODO landonf: Allow the user to specify the digest algorithm
#ifdef HAVE_SHA2
  crypto_digest_t signing_algorithm = CRYPTO_DIGEST_SHA256;
#else
  crypto_digest_t signing_algorithm = CRYPTO_DIGEST_SHA1;
#endif

  if (BitIsSet(FO_MD5, bsctx.ff_pkt->flags)) {
    bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_MD5);
    bsctx.digest_stream = STREAM_MD5_DIGEST;
  } else if (BitIsSet(FO_SHA1, bsctx.ff_pkt->flags)) {
    bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_SHA1);
    bsctx.digest_stream = STREAM_SHA1_DIGEST;
  } else if (BitIsSet(FO_SHA256, bsctx.ff_pkt->flags)) {
    bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_SHA256);
    bsctx.digest_stream = STREAM_SHA256_DIGEST;
  } else if (BitIsSet(FO_SHA512, bsctx.ff_pkt->flags)) {
    bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_SHA512);
    bsctx.digest_stream = STREAM_SHA512_DIGEST;
  } else if (BitIsSet(FO_XXH128, bsctx.ff_pkt->flags)) {
    bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_XXH128);
    bsctx.digest_stream = STREAM_XXH128_DIGEST;
  }

  // Did digest initialization fail?
  if (bsctx.digest_stream != STREAM_NONE && bsctx.digest == NULL) {
    Jmsg(bsctx.jcr, M_WARNING, 0, T_("%s digest initialization failed\n"),
         stream_to_ascii(bsctx.digest_stream));
  }

  /* Set up signature digest handling. If this fails, the signature digest
   * will be set to NULL and not used. */
  /* TODO landonf: We should really only calculate the digest once, for
   * both verification and signing.
   */
  if (bsctx.jcr->fd_impl->crypto.pki_sign) {
    bsctx.signing_digest = crypto_digest_new(bsctx.jcr, signing_algorithm);

    // Full-stop if a failure occurred initializing the signature digest
    if (bsctx.signing_digest == NULL) {
      Jmsg(bsctx.jcr, M_NOTSAVED, 0,
           T_("%s signature digest initialization failed\n"),
           stream_to_ascii(signing_algorithm));
      bsctx.jcr->JobErrors++;
      goto bail_out;
    }
  }

  // Enable encryption
  if (bsctx.jcr->fd_impl->crypto.pki_encrypt) {
    SetBit(FO_ENCRYPT, bsctx.ff_pkt->flags);
  }
  retval = true;

bail_out:
  return retval;
}

// Terminate the signing digest and send it to the Storage daemon
static inline bool TerminateSigningDigest(b_save_ctx& bsctx)
{
  uint32_t size = 0;
  bool retval = false;
  SIGNATURE* signature = NULL;
  BareosSocket* sd = bsctx.jcr->store_bsock;

  if ((signature = crypto_sign_new(bsctx.jcr)) == NULL) {
    Jmsg(bsctx.jcr, M_FATAL, 0,
         T_("Failed to allocate memory for crypto signature.\n"));
    goto bail_out;
  }

  if (!CryptoSignAddSigner(signature, bsctx.signing_digest,
                           bsctx.jcr->fd_impl->crypto.pki_keypair)) {
    Jmsg(bsctx.jcr, M_FATAL, 0,
         T_("An error occurred while signing the stream.\n"));
    goto bail_out;
  }

  // Get signature size
  if (!CryptoSignEncode(signature, NULL, &size)) {
    Jmsg(bsctx.jcr, M_FATAL, 0,
         T_("An error occurred while signing the stream.\n"));
    goto bail_out;
  }

  // Grow the bsock buffer to fit our message if necessary
  if (SizeofPoolMemory(sd->msg) < (int32_t)size) {
    sd->msg = ReallocPoolMemory(sd->msg, size);
  }

  // Send our header
  sd->fsend("%" PRIu32 " %" PRId32 " 0", bsctx.jcr->JobFiles,
            STREAM_SIGNED_DIGEST);
  Dmsg1(300, "filed>stored:header %s", sd->msg);

  // Encode signature data
  if (!CryptoSignEncode(signature, (uint8_t*)sd->msg, &size)) {
    Jmsg(bsctx.jcr, M_FATAL, 0,
         T_("An error occurred while signing the stream.\n"));
    goto bail_out;
  }

  sd->message_length = size;
  sd->send();
  sd->signal(BNET_EOD); /* end of checksum */
  retval = true;

bail_out:
  if (signature) { CryptoSignFree(signature); }
  return retval;
}

// Terminate any digest and send it to Storage daemon
static inline bool TerminateDigest(b_save_ctx& bsctx)
{
  uint32_t size;
  bool retval = false;
  BareosSocket* sd = bsctx.jcr->store_bsock;

  sd->fsend("%" PRIu32 " %" PRId32 " 0", bsctx.jcr->JobFiles,
            bsctx.digest_stream);
  Dmsg1(300, "filed>stored:header %s", sd->msg);

  size = CRYPTO_DIGEST_MAX_SIZE;

  // Grow the bsock buffer to fit our message if necessary
  if (SizeofPoolMemory(sd->msg) < (int32_t)size) {
    sd->msg = ReallocPoolMemory(sd->msg, size);
  }

  if (!CryptoDigestFinalize(bsctx.digest, (uint8_t*)sd->msg, &size)) {
    Jmsg(bsctx.jcr, M_FATAL, 0,
         T_("An error occurred finalizing signing the stream.\n"));
    goto bail_out;
  }

  // Keep the checksum if this file is a hardlink
  if (bsctx.ff_pkt->linked) {
    bsctx.ff_pkt->linked->set_digest(bsctx.digest_stream, sd->msg, size);
  }

  sd->message_length = size;
  sd->send();
  sd->signal(BNET_EOD); /* end of checksum */
  retval = true;

bail_out:
  return retval;
}

static inline bool DoBackupAcl(JobControlRecord* jcr, FindFilesPacket* ff_pkt)
{
  bacl_exit_code retval;

  jcr->fd_impl->acl_data->filetype = ff_pkt->type;
  jcr->fd_impl->acl_data->last_fname = jcr->fd_impl->last_fname;

  auto* acl_build_data
      = dynamic_cast<AclBuildData*>(jcr->fd_impl->acl_data.get());
  if (jcr->IsPlugin()) {
    retval = PluginBuildAclStreams(jcr, acl_build_data, ff_pkt);
  } else {
    retval = BuildAclStreams(jcr, acl_build_data, ff_pkt);
  }

  switch (retval) {
    case bacl_exit_fatal:
      return false;
    case bacl_exit_error:
      Jmsg(jcr, M_ERROR, 0, "%s", jcr->errmsg);
      jcr->fd_impl->acl_data->nr_errors++;
      break;
    case bacl_exit_ok:
      break;
  }

  return true;
}

static inline bool DoBackupXattr(JobControlRecord* jcr, FindFilesPacket* ff_pkt)
{
  BxattrExitCode retval;

  jcr->fd_impl->xattr_data->last_fname = jcr->fd_impl->last_fname;

  if (jcr->IsPlugin()) {
    retval = PluginBuildXattrStreams(
        jcr, dynamic_cast<XattrBuildData*>(jcr->fd_impl->xattr_data.get()),
        ff_pkt);
  } else {
    retval = BuildXattrStreams(
        jcr, dynamic_cast<XattrBuildData*>(jcr->fd_impl->xattr_data.get()),
        ff_pkt);
  }

  switch (retval) {
    case BxattrExitCode::kErrorFatal:
      return false;
    case BxattrExitCode::kWarning:
      Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);
      break;
    case BxattrExitCode::kError:
      Jmsg(jcr, M_ERROR, 0, "%s", jcr->errmsg);
      jcr->fd_impl->xattr_data->nr_errors++;
      break;
    case BxattrExitCode::kSuccess:
      break;
  }

  return true;
}

/**
 * Called here by find() for each file included.
 * This is a callback. The original is FindFiles() above.
 *
 * Send the file and its data to the Storage daemon.
 *
 * Returns: 1 if OK
 *          0 if error
 *         -1 to ignore file/directory (not used here)
 */
int SaveFile(JobControlRecord* jcr, FindFilesPacket* ff_pkt, bool)
{
  bool do_read = false;
  bool plugin_started = false;
  bool do_plugin_set = false;
  int status, data_stream;
  int rtnstat = 0;
  b_save_ctx bsctx;
  bool has_file_data = false;
  save_pkt sp; /* use by option plugin */
  BareosSocket* sd = jcr->store_bsock;

  if (jcr->IsJobCanceled() || jcr->IsIncomplete()) { return 0; }

  jcr->fd_impl->num_files_examined++; /* bump total file count */

  switch (ff_pkt->type) {
    case FT_LNKSAVED: /* Hard linked, file already saved */
      Dmsg2(130, "FT_LNKSAVED hard link: %s => %s\n", ff_pkt->fname,
            ff_pkt->link_or_dir);
      break;
    case FT_REGE:
      Dmsg1(130, "FT_REGE saving: %s\n", ff_pkt->fname);
      has_file_data = true;
      break;
    case FT_REG:
      Dmsg1(130, "FT_REG saving: %s\n", ff_pkt->fname);
      has_file_data = true;
      break;
    case FT_LNK:
      Dmsg2(130, "FT_LNK saving: %s -> %s\n", ff_pkt->fname,
            ff_pkt->link_or_dir);
      break;
    case FT_RESTORE_FIRST:
      Dmsg1(100, "FT_RESTORE_FIRST saving: %s\n", ff_pkt->fname);
      break;
    case FT_PLUGIN_CONFIG:
      Dmsg1(100, "FT_PLUGIN_CONFIG saving: %s\n", ff_pkt->fname);
      break;
    case FT_DIRBEGIN:
      jcr->fd_impl->num_files_examined--; /* correct file count */
      return 1;                           /* not used */
    case FT_NORECURSE:
      Jmsg(jcr, M_INFO, 1,
           T_("     Recursion turned off. Will not descend from %s into %s\n"),
           ff_pkt->top_fname, ff_pkt->fname);
      ff_pkt->type = FT_DIREND; /* Backup only the directory entry */
      break;
    case FT_NOFSCHG:
      /* Suppress message for /dev filesystems */
      if (!IsInFileset(ff_pkt)) {
        Jmsg(jcr, M_INFO, 1,
             T_("     %s is a different filesystem. Will not descend from %s "
                "into it.\n"),
             ff_pkt->fname, ff_pkt->top_fname);
      }
      ff_pkt->type = FT_DIREND; /* Backup only the directory entry */
      break;
    case FT_INVALIDFS:
      Jmsg(jcr, M_INFO, 1,
           T_("     Disallowed filesystem. Will not descend from %s into %s\n"),
           ff_pkt->top_fname, ff_pkt->fname);
      ff_pkt->type = FT_DIREND; /* Backup only the directory entry */
      break;
    case FT_INVALIDDT:
      Jmsg(jcr, M_INFO, 1,
           T_("     Disallowed drive type. Will not descend into %s\n"),
           ff_pkt->fname);
      break;
    case FT_REPARSE:
    case FT_JUNCTION:
    case FT_DIREND:
      Dmsg1(130, "FT_DIREND: %s\n", ff_pkt->link_or_dir);
      break;
    case FT_SPEC:
      Dmsg1(130, "FT_SPEC saving: %s\n", ff_pkt->fname);
      if (S_ISSOCK(ff_pkt->statp.st_mode)) {
        Jmsg(jcr, M_SKIPPED, 1, T_("     Socket file skipped: %s\n"),
             ff_pkt->fname);
        return 1;
      }
      break;
    case FT_RAW:
      Dmsg1(130, "FT_RAW saving: %s\n", ff_pkt->fname);
      has_file_data = true;
      break;
    case FT_FIFO:
      Dmsg1(130, "FT_FIFO saving: %s\n", ff_pkt->fname);
      break;
    case FT_NOACCESS: {
      BErrNo be;
      Jmsg(jcr, M_NOTSAVED, 0, T_("     Could not access \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
    }
    case FT_NOFOLLOW: {
      BErrNo be;
      Jmsg(jcr, M_NOTSAVED, 0,
           T_("     Could not follow link \"%s\": ERR=%s\n"), ff_pkt->fname,
           be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
    }
    case FT_NOSTAT: {
      BErrNo be;
      Jmsg(jcr, M_NOTSAVED, 0, T_("     Could not stat \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
    }
    case FT_DIRNOCHG:
    case FT_NOCHG:
      Jmsg(jcr, M_SKIPPED, 1, T_("     Unchanged file skipped: %s\n"),
           ff_pkt->fname);
      return 1;
    case FT_ISARCH:
      Jmsg(jcr, M_NOTSAVED, 0, T_("     Archive file not saved: %s\n"),
           ff_pkt->fname);
      return 1;
    case FT_NOOPEN: {
      BErrNo be;
      Jmsg(jcr, M_NOTSAVED, 0,
           T_("     Could not open directory \"%s\": ERR=%s\n"), ff_pkt->fname,
           be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
    }
    case FT_DELETED:
      Dmsg1(130, "FT_DELETED: %s\n", ff_pkt->fname);
      break;
    default:
      Jmsg(jcr, M_NOTSAVED, 0, T_("     Unknown file type %d; not saved: %s\n"),
           ff_pkt->type, ff_pkt->fname);
      jcr->JobErrors++;
      return 1;
  }

  Dmsg1(130, "filed: sending %s to stored\n", ff_pkt->fname);

  // Setup backup signing context.
  memset(&bsctx, 0, sizeof(b_save_ctx));
  bsctx.digest_stream = STREAM_NONE;
  bsctx.jcr = jcr;
  bsctx.ff_pkt = ff_pkt;

  // Digests and encryption are only useful if there's file data
  if (has_file_data) {
    if (!SetupEncryptionDigests(bsctx)) { goto good_rtn; }
  }

  // Initialize the file descriptor we use for data and other streams.
  binit(&ff_pkt->bfd);
  if (BitIsSet(FO_PORTABLE, ff_pkt->flags)) {
    SetPortableBackup(&ff_pkt->bfd); /* disable Win32 BackupRead() */
  }

  // Option and cmd plugin are not compatible together
  if (ff_pkt->cmd_plugin) {
    do_plugin_set = true;
  } else if (ff_pkt->opt_plugin) {
    // Ask the option plugin what to do with this file
    switch (PluginOptionHandleFile(jcr, ff_pkt, &sp)) {
      case bRC_OK:
        Dmsg2(10, "Option plugin %s will be used to backup %s\n",
              ff_pkt->plugin, ff_pkt->fname);
        jcr->opt_plugin = true;
        jcr->fd_impl->plugin_sp = &sp;
        PluginUpdateFfPkt(ff_pkt, &sp);
        do_plugin_set = true;
        break;
      case bRC_Skip:
        Dmsg2(10, "Option plugin %s decided to skip %s\n", ff_pkt->plugin,
              ff_pkt->fname);
        goto good_rtn;
      case bRC_Core:
        Dmsg2(10, "Option plugin %s decided to let bareos handle %s\n",
              ff_pkt->plugin, ff_pkt->fname);
        break;
      default:
        goto bail_out;
    }
  }

  if (do_plugin_set) {
    // Tell bfile that it needs to call plugin
    if (!SetCmdPlugin(&ff_pkt->bfd, jcr)) { goto bail_out; }
    SendPluginName(jcr, sd, true); /* signal start of plugin data */
    plugin_started = true;
  }

  // Send attributes -- must be done after binit()
  if (!EncodeAndSendAttributes(jcr, ff_pkt, data_stream)) { goto bail_out; }

  // Meta data only for restore object
  if (IS_FT_OBJECT(ff_pkt->type)) { goto good_rtn; }

  // Meta data only for deleted files
  if (ff_pkt->type == FT_DELETED) { goto good_rtn; }

  // Set up the encryption context and send the session data to the SD
  if (has_file_data && jcr->fd_impl->crypto.pki_encrypt) {
    if (!CryptoSessionSend(jcr, sd)) { goto bail_out; }
  }

  /* For a command plugin use the setting from the plugins savepkt no_read field
   * which is saved in the ff_pkt->no_read variable. do_read is the inverted
   * value of this variable as no_read == TRUE means do_read == FALSE */
  if (ff_pkt->cmd_plugin) {
    do_read = !ff_pkt->no_read;
  } else {
    /* Open any file with data that we intend to save, then save it.
     *
     * Note, if is_win32_backup, we must open the Directory so that
     * the BackupRead will save its permissions and ownership streams. */
    if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode)) {
#ifdef HAVE_WIN32
      do_read = !IsPortableBackup(&ff_pkt->bfd) || ff_pkt->statp.st_size > 0;
#else
      do_read = ff_pkt->statp.st_size > 0;
#endif
    } else if (ff_pkt->type == FT_RAW || ff_pkt->type == FT_FIFO
               || ff_pkt->type == FT_REPARSE || ff_pkt->type == FT_JUNCTION
               || (!IsPortableBackup(&ff_pkt->bfd)
                   && ff_pkt->type == FT_DIREND)) {
      do_read = true;
    }
  }

  Dmsg2(150, "type=%d do_read=%d\n", ff_pkt->type, do_read);
  if (do_read) {
    btimer_t* tid;
    int noatime;

    if (ff_pkt->type == FT_FIFO) {
      tid = StartThreadTimer(jcr, pthread_self(), 60);
    } else {
      tid = NULL;
    }

    noatime = BitIsSet(FO_NOATIME, ff_pkt->flags) ? O_NOATIME : 0;
    ff_pkt->bfd.reparse_point
        = (ff_pkt->type == FT_REPARSE || ff_pkt->type == FT_JUNCTION);

    if (bopen(&ff_pkt->bfd, ff_pkt->fname, O_RDONLY | O_BINARY | noatime, 0,
              ff_pkt->statp.st_rdev)
        < 0) {
      ff_pkt->ff_errno = errno;
      BErrNo be;
      Jmsg(jcr, M_NOTSAVED, 0, T_("     Cannot open \"%s\": ERR=%s.\n"),
           ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      if (tid) {
        StopThreadTimer(tid);
        tid = NULL;
      }
      goto good_rtn;
    }

    if (tid) {
      StopThreadTimer(tid);
      tid = NULL;
    }

    status = send_data(jcr, data_stream, ff_pkt, bsctx.digest,
                       bsctx.signing_digest);

    if (BitIsSet(FO_CHKCHANGES, ff_pkt->flags)) { HasFileChanged(jcr, ff_pkt); }

    bclose(&ff_pkt->bfd);

    if (!status) { goto bail_out; }
  }

  if (have_darwin_os) {
    // Regular files can have resource forks and Finder Info
    if (ff_pkt->type != FT_LNKSAVED
        && (S_ISREG(ff_pkt->statp.st_mode)
            && BitIsSet(FO_HFSPLUS, ff_pkt->flags))) {
      if (!SaveRsrcAndFinder(bsctx)) { goto bail_out; }
    }
  }

  // Save ACLs when requested and available for anything not being a symlink.
  if (have_acl) {
    if (BitIsSet(FO_ACL, ff_pkt->flags) && ff_pkt->type != FT_LNK) {
      if (!DoBackupAcl(jcr, ff_pkt)) { goto bail_out; }
    }
  }

  // Save Extended Attributes when requested and available for all files.
  if (have_xattr) {
    if (BitIsSet(FO_XATTR, ff_pkt->flags)) {
      if (!DoBackupXattr(jcr, ff_pkt)) { goto bail_out; }
    }
  }

  // Terminate the signing digest and send it to the Storage daemon
  if (bsctx.signing_digest) {
    if (!TerminateSigningDigest(bsctx)) { goto bail_out; }
  }

  // Terminate any digest and send it to Storage daemon
  if (bsctx.digest) {
    if (!TerminateDigest(bsctx)) { goto bail_out; }
  }

  // Check if original file has a digest, and send it
  if (ff_pkt->type == FT_LNKSAVED && ff_pkt->digest) {
    Dmsg2(300, "Link %s digest %d\n", ff_pkt->fname, ff_pkt->digest_len);
    sd->fsend("%" PRIu32 " %" PRId32 " 0", jcr->JobFiles,
              ff_pkt->digest_stream);

    sd->msg = CheckPoolMemorySize(sd->msg, ff_pkt->digest_len);
    memcpy(sd->msg, ff_pkt->digest, ff_pkt->digest_len);
    sd->message_length = ff_pkt->digest_len;
    sd->send();

    sd->signal(BNET_EOD); /* end of hardlink record */
  }

good_rtn:
  rtnstat = jcr->IsJobCanceled() ? 0 : 1; /* good return if not canceled */

bail_out:
  if (jcr->IsIncomplete() || jcr->IsJobCanceled()) { rtnstat = 0; }
  if (plugin_started) {
    SendPluginName(jcr, sd, false); /* signal end of plugin data */
  }
  if (ff_pkt->opt_plugin) {
    jcr->fd_impl->plugin_sp = NULL; /* sp is local to this function */
    jcr->opt_plugin = false;
  }
  if (bsctx.digest) { CryptoDigestFree(bsctx.digest); }
  if (bsctx.signing_digest) { CryptoDigestFree(bsctx.signing_digest); }

  DequeueMessages(jcr);

  return rtnstat;
}

/**
 * Handle the data just read and send it to the SD after doing any
 * postprocessing needed.
 */
static inline bool SendDataToSd(b_ctx* bctx)
{
  BareosSocket* sd = bctx->jcr->store_bsock;
  bool need_more_data;

  // Check for sparse blocks
  if (BitIsSet(FO_SPARSE, bctx->ff_pkt->flags)) {
    bool allZeros;
    ser_declare;

    allZeros = false;
    if ((sd->message_length == bctx->rsize
         && (bctx->fileAddr + sd->message_length
             < (uint64_t)bctx->ff_pkt->statp.st_size))
        || ((bctx->ff_pkt->type == FT_RAW || bctx->ff_pkt->type == FT_FIFO)
            && ((uint64_t)bctx->ff_pkt->statp.st_size == 0))) {
      allZeros = IsBufZero(bctx->rbuf, bctx->rsize);
    }

    if (!allZeros) {
      // Put file address as first data in buffer
      SerBegin(bctx->wbuf, OFFSET_FADDR_SIZE);
      ser_uint64(bctx->fileAddr); /* store fileAddr in begin of buffer */
    }

    bctx->fileAddr += sd->message_length; /* update file address */

    // Skip block of all zeros
    if (allZeros) { return true; }
  } else if (BitIsSet(FO_OFFSETS, bctx->ff_pkt->flags)) {
    ser_declare;
    SerBegin(bctx->wbuf, OFFSET_FADDR_SIZE);
    ser_uint64(bctx->ff_pkt->bfd.offset); /* store offset in begin of buffer */
  }

  bctx->jcr->ReadBytes += sd->message_length; /* count bytes read */

  // Uncompressed cipher input length
  bctx->cipher_input_len = sd->message_length;

  // Update checksum if requested
  if (bctx->digest) {
    CryptoDigestUpdate(bctx->digest, (uint8_t*)bctx->rbuf, sd->message_length);
  }

  // Update signing digest if requested
  if (bctx->signing_digest) {
    CryptoDigestUpdate(bctx->signing_digest, (uint8_t*)bctx->rbuf,
                       sd->message_length);
  }

  // Compress the data.
  if (BitIsSet(FO_COMPRESS, bctx->ff_pkt->flags)) {
    if (!CompressData(bctx->jcr, bctx->ff_pkt->Compress_algo, bctx->rbuf,
                      bctx->jcr->store_bsock->message_length, bctx->cbuf,
                      bctx->max_compress_len, &bctx->compress_len)) {
      return false;
    }

    // See if we need to generate a compression header.
    if (bctx->chead) {
      ser_declare;

      // Complete header
      SerBegin(bctx->chead, sizeof(comp_stream_header));
      ser_uint32(bctx->ch.magic);
      ser_uint32(bctx->compress_len);
      ser_uint16(bctx->ch.level);
      ser_uint16(bctx->ch.version);
      SerEnd(bctx->chead, sizeof(comp_stream_header));

      bctx->compress_len += sizeof(comp_stream_header); /* add size of header */
    }

    bctx->jcr->store_bsock->message_length
        = bctx->compress_len; /* set compressed length */
    bctx->cipher_input_len = bctx->compress_len;
  }

  // Encrypt the data.
  need_more_data = false;
  if (BitIsSet(FO_ENCRYPT, bctx->ff_pkt->flags)
      && !EncryptData(bctx, &need_more_data)) {
    if (need_more_data) { return true; }
    return false;
  }

  // Send the buffer to the Storage daemon
  if (BitIsSet(FO_SPARSE, bctx->ff_pkt->flags)
      || BitIsSet(FO_OFFSETS, bctx->ff_pkt->flags)) {
    sd->message_length += OFFSET_FADDR_SIZE; /* include fileAddr in size */
  }
  sd->msg = bctx->wbuf; /* set correct write buffer */

  if (!sd->send()) {
    if (!bctx->jcr->IsJobCanceled()) {
      Jmsg1(bctx->jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
    }
    return false;
  }

  Dmsg1(130, "Send data to SD len=%d\n", sd->message_length);
  bctx->jcr->JobBytes += sd->message_length; /* count bytes saved possibly
                                                compressed/encrypted */
  sd->msg = bctx->msgsave;                   /* restore read buffer */

  return true;
}

#ifdef HAVE_WIN32
// Callback method for ReadEncryptedFileRaw()
static DWORD WINAPI send_efs_data(PBYTE pbData,
                                  PVOID pvCallbackContext,
                                  ULONG ulLength)
{
  b_ctx* bctx = (b_ctx*)pvCallbackContext;
  BareosSocket* sd = bctx->jcr->store_bsock;

  if (ulLength == 0) { return ERROR_SUCCESS; }

  /* See if we can fit the data into the current bctx->rbuf which can hold
   * bctx->rsize bytes. */
  if (ulLength <= (ULONG)bctx->rsize) {
    sd->message_length = ulLength;
    memcpy(bctx->rbuf, pbData, ulLength);
    if (!SendDataToSd(bctx)) { return ERROR_NET_WRITE_FAULT; }
  } else {
    // Need to chunk the data into pieces.
    ULONG offset = 0;

    while (ulLength > 0) {
      sd->message_length = MIN((ULONG)bctx->rsize, ulLength);
      memcpy(bctx->rbuf, pbData + offset, sd->message_length);
      if (!SendDataToSd(bctx)) { return ERROR_NET_WRITE_FAULT; }

      offset += sd->message_length;
      ulLength -= sd->message_length;
    }
  }

  return ERROR_SUCCESS;
}

// Send the content of an Encrypted file on an EFS filesystem.
static inline bool SendEncryptedData(b_ctx& bctx)
{
  bool retval = false;

  if (!p_ReadEncryptedFileRaw) {
    Jmsg0(bctx.jcr, M_FATAL, 0,
          T_("Encrypted file but no EFS support functions\n"));
  }

  /* The EFS read function, ReadEncryptedFileRaw(), works in a specific way.
   * You have to give it a function that it calls repeatedly every time the
   * read buffer is filled.
   *
   * So ReadEncryptedFileRaw() will not return until it has read the whole file.
   */
  if (p_ReadEncryptedFileRaw((PFE_EXPORT_FUNC)send_efs_data, &bctx,
                             bctx.ff_pkt->bfd.pvContext)) {
    goto bail_out;
  }
  retval = true;

bail_out:
  return retval;
}
#endif

static inline bool SendPlainDataSerially(b_ctx& bctx)
{
  bool retval = false;
  BareosSocket* sd = bctx.jcr->store_bsock;

  // Read the file data
  while ((sd->message_length
          = (uint32_t)bread(&bctx.ff_pkt->bfd, bctx.rbuf, bctx.rsize))
         > 0) {
    if (!SendDataToSd(&bctx)) { goto bail_out; }
  }
  retval = true;

bail_out:
  return retval;
}

static result<std::size_t> SendData(BareosSocket* sd,
                                    POOLMEM* data,
                                    size_t size)
{
  sd->message_length = size;
  sd->msg = data; /* set correct write buffer */

  if (!sd->send()) {
    PoolMem error;
    Mmsg(error, "Network send error to SD. ERR=%s", sd->bstrerror());
    return error;
  }

  Dmsg1(130, "Send data to SD len=%d\n", sd->message_length);
  return size;
}

class data_message {
  /* some data is prefixed by a OFFSET_FADDR_SIZE-byte number -- called header
   * here, which basically contains the file position to which to write the
   * following block of data.
   * The difference between FADDR and OFFSET is that offset may be any value
   * (given to the core by a plugin), whereas FADDR is computed by the core
   * itself and is equal to the number of bytes already read from the file
   * descriptor. */
  static inline constexpr std::size_t header_size = OFFSET_FADDR_SIZE;
  /* our bsocket functions assume that they are allowed to overwrite
   * the four bytes directly preceding the given buffer
   * To keep the message alignment to 8, we "allocate" full 8 bytes instead
   * of the required 4. */
  static inline constexpr std::size_t bnet_size = 8;
  static inline constexpr std::size_t data_offset = header_size + bnet_size;

  std::vector<char> buffer{};
  bool has_header{false};

 public:
  data_message(std::size_t data_size)
  {
    buffer.resize(data_size + data_offset);
  }
  data_message() : data_message(0) {}
  data_message(const data_message&) = delete;
  data_message& operator=(const data_message&) = delete;
  data_message(data_message&&) = default;
  data_message& operator=(data_message&&) = default;

  // creates a message with the same header -- if any
  data_message derived() const
  {
    data_message derived;

    if (has_header) {
      derived.has_header = true;
      std::memcpy(derived.header_ptr(), header_ptr(), header_size);
    }

    return derived;
  }

  void set_header(std::uint64_t h)
  {
    has_header = true;
    auto* ptr = header_ptr();
    network_order::network net{h};  // save in network order
    std::memcpy(ptr, &net, header_size);
  }

  void resize(std::size_t new_size) { buffer.resize(data_offset + new_size); }

  char* header_ptr() { return &buffer[bnet_size]; }
  char* data_ptr() { return &buffer[data_offset]; }
  const char* header_ptr() const { return &buffer[bnet_size]; }
  const char* data_ptr() const { return &buffer[data_offset]; }

  std::size_t data_size() const
  {
    ASSERT(buffer.size() >= data_offset);
    return buffer.size() - data_offset;
  }

  /* important: this is not actually a POOLMEM*; do not pass it to POOLMEM*
   *            functions, except to pass it to BareosSocket::SendData(). */
  POOLMEM* as_socket_message()
  {
    if (has_header) {
      return header_ptr();
    } else {
      return data_ptr();
    }
  }

  std::size_t message_size() const
  {
    auto size_with_header = buffer.size() - bnet_size;
    if (has_header) {
      return size_with_header;
    } else {
      return size_with_header - header_size;
    }
  }
};

using shared_message = std::shared_ptr<data_message>;

static std::future<result<std::size_t>> MakeSendThread(
    thread_pool& pool,
    BareosSocket* sd,
    channel::output<std::future<result<shared_message>>> out)
{
  std::promise<result<std::size_t>> promise;
  std::future fut = promise.get_future();

  pool.borrow_thread(
      [prom = std::move(promise), out = std::move(out), sd]() mutable {
        std::size_t accumulated = 0;
        for (;;) {
          std::optional out_fut = out.get();
          if (!out_fut) { break; }
          result p = out_fut->get();
          if (p.holds_error()) {
            prom.set_value(std::move(p.error_unchecked()));
            return;
          }

          auto& val = p.value_unchecked();
          auto size = val->message_size();
          POOLMEM* msg = val->as_socket_message();

          // technically we are overwriting part of message here
          // but its only the "size" field of the message, which is not
          // read/written to otherwise after making it a shared_message.
          result ret = SendData(sd, msg, size);
          if (ret.holds_error()) {
            prom.set_value(std::move(ret.error_unchecked()));
            return;
          }

          accumulated += ret.value_unchecked();
        }
        prom.set_value(accumulated);
      });
  return fut;
}

struct compression_context {
  comp_stream_header ch;
  uint32_t algorithm;
  int level;
};

static result<shared_message> DoCompressMessage(compression_context& compctx,
                                                const data_message& input)
{
  auto data_size = RequiredCompressionOutputBufferSize(compctx.algorithm,
                                                       input.data_size());

  auto msg = input.derived();
  msg.resize(data_size);
  result comp_size = ThreadlocalCompress(
      compctx.algorithm, compctx.level, input.data_ptr(), input.data_size(),
      msg.data_ptr() + sizeof(comp_stream_header),
      msg.data_size() - sizeof(comp_stream_header));

  if (comp_size.holds_error()) {
    return std::move(comp_size.error_unchecked());
  }

  auto csize = comp_size.value_unchecked();

  if (csize > std::numeric_limits<std::uint32_t>::max()) {
    PoolMem error;
    Mmsg(error, "Compressed size to big (%" PRIuz " > %" PRIu32 ")", csize,
         std::numeric_limits<std::uint32_t>::max());
    return error;
  }

  {
    // Write compression header
    ser_declare;
    SerBegin(msg.data_ptr(), sizeof(comp_stream_header));
    ser_uint32(compctx.ch.magic);
    ser_uint32(csize);
    ser_uint16(compctx.ch.level);
    ser_uint16(compctx.ch.version);
    SerEnd(msg.data_ptr(), sizeof(comp_stream_header));
  }

  auto total_size = csize + sizeof(comp_stream_header);
  ASSERT(total_size <= msg.data_size());
  msg.resize(total_size);

  return shared_message{new data_message{std::move(msg)}};
}

// Send the content of a file on anything but an EFS filesystem.
static inline bool SendPlainData(b_ctx& bctx)
{
  std::size_t max_buf_size = bctx.rsize;

  auto file_type = bctx.ff_pkt->type;
  auto file_size = bctx.ff_pkt->statp.st_size;
  auto* flags = bctx.ff_pkt->flags;

  const std::size_t num_workers = me->MaxWorkersPerJob;
  // Currently we do not support encryption while doing
  // parallel sending/checksumming/compression/etc.
  // This is mostly because EncryptData() is weird!
  if (BitIsSet(FO_ENCRYPT, flags)) { return SendPlainDataSerially(bctx); }

  // Setting up the parallel pipeline is not worth it for small files.
  if (static_cast<std::size_t>(file_size) < 2 * max_buf_size) {
    return SendPlainDataSerially(bctx);
  }

  // setting maximum worker threads to 0 means that you do not want
  // multithreading, so just use the serial code path for now.
  if (num_workers == 0) { return SendPlainDataSerially(bctx); }

  bool retval = false;
  BareosSocket* sd = bctx.jcr->store_bsock;

  auto& bfd = bctx.ff_pkt->bfd;

  bool support_sparse = BitIsSet(FO_SPARSE, flags);
  bool support_offsets = BitIsSet(FO_OFFSETS, flags);

  std::optional<compression_context> compctx;
  if (BitIsSet(FO_COMPRESS, flags)) {
    compctx = compression_context{
        .ch = bctx.ch,
        .algorithm = bctx.ff_pkt->Compress_algo,
        .level = bctx.ff_pkt->Compress_level,
    };
  }

  auto& threadpool = bctx.jcr->fd_impl->threads;

  work_group compute_group(num_workers * 3);

  // FIXME(ssura): this should become a std::latch once C++20 arrives
  std::condition_variable compute_fin;
  synchronized<std::size_t> latch{num_workers};
  threadpool.borrow_threads(num_workers,
                            [&latch, &compute_group, &compute_fin] {
                              compute_group.work_until_completion();

                              auto lock = latch.lock();
                              *lock -= 1;
                              compute_fin.notify_one();
                            });

  auto [in, out]
      = channel::CreateBufferedChannel<std::future<result<shared_message>>>(
          num_workers);

  std::future bytes_send_fut = MakeSendThread(threadpool, sd, std::move(out));

  DIGEST* checksum = bctx.digest;
  DIGEST* signing = bctx.signing_digest;

  std::optional<std::future<void>> update_digest;

  std::uint64_t bytes_read{0};
  std::uint64_t offset{0};

  std::uint64_t& header = *(support_sparse ? &bytes_read : &offset);

  static_assert(sizeof(header) == OFFSET_FADDR_SIZE);
  bool include_header = support_sparse || support_offsets;

  bool read_error = false;

  // Read the file data
  for (;;) {
    data_message msg(max_buf_size);
    for (bool skip_block = true; skip_block;) {
      skip_block = false;
      ssize_t read_bytes = bread(&bfd, msg.data_ptr(), msg.data_size());
      // update offset _before_ sending the header
      offset = bfd.offset;

      if (read_bytes <= 0) {
        if (read_bytes < 0) { read_error = true; }
        goto end_read_loop;
      }

      msg.resize(read_bytes);

      bool unsized_file
          = (file_type == FT_RAW || file_type == FT_FIFO) && (file_size == 0);

      /* This is looks weird but this is the idea:
       * If we sparse is enabled and the block is just zero, we want to skip it;
       * but we need to recover the file size on restore, so we need to
       * at least always send the last block to the sd regardless of its
       * contents (This is the `< file_size` check) For unsized raw/fifo files,
       * we do not[1] recover the correct file size with sparse enabled on a
       * restore, so we skip the test.
       *
       * [1] Not sure why this was decided on.  Maybe there was specific use
       *     case in mind when this was decided;  I would have expected us to
       *     disable SPARSE in that case and continue on as normal.
       */
      if (support_sparse
          && ((msg.data_size() == max_buf_size
               && (msg.data_size() + max_buf_size < (uint64_t)file_size))
              || unsized_file)
          // IsBufZero actually requires 8 bytes of alignment
          && IsBufZero(msg.data_ptr(), msg.data_size())) {
        skip_block = true;
      } else if (include_header) {
        msg.set_header(header);
      }

      // update bytes_read _after_ sending the header
      bytes_read += read_bytes;
    }
    ASSERT(msg.data_size() > 0);

    shared_message shared_msg{new data_message{std::move(msg)}};

    if (update_digest) {
      // updating the digest has to be done serially
      // so we have to wait until the last task is finished
      // before issuing a new one.
      update_digest->get();
    }

    if (checksum || signing) {
      update_digest.emplace(compute_group.submit([checksum, signing,
                                                  shared_msg]() mutable {
        auto* data = reinterpret_cast<const uint8_t*>(shared_msg->data_ptr());
        auto size = shared_msg->data_size();
        // Update checksum if requested
        if (checksum) { CryptoDigestUpdate(checksum, data, size); }

        // Update signing digest if requested
        if (signing) { CryptoDigestUpdate(signing, data, size); }
      }));
    }

    std::future<result<shared_message>> copy_fut;
    if (compctx) {
      copy_fut = compute_group.submit(
          [cctx = compctx.value(), shared_msg]() mutable {
            return DoCompressMessage(cctx, *shared_msg.get());
          });
    } else {
      std::promise<result<shared_message>> prom;
      prom.set_value(std::move(shared_msg));
      copy_fut = prom.get_future();
    }

    // Send the buffer to the Storage daemon
    if (!in.emplace(std::move(copy_fut))) { goto bail_out; }
  }
end_read_loop:
  retval = true;

bail_out:
  compute_group.shutdown();
  latch.lock().wait(compute_fin, [](int num) { return num == 0; });
  in.close();
  if (update_digest) { update_digest->get(); }
  result sendres = bytes_send_fut.get();
  if (auto* error = sendres.error()) {
    if (!bctx.jcr->IsJobCanceled()) {
      Jmsg1(bctx.jcr, M_FATAL, 0, "%s\n", error->c_str());
    }
    retval = false;
  } else {
    bctx.jcr->JobBytes
        += sendres.value_unchecked();  /* count bytes saved possibly
                                          compressed/encrypted */
    bctx.jcr->ReadBytes += bytes_read; /* count bytes read */
  }
  sd->msg = bctx.msgsave; /* restore read buffer */

  if (read_error) {
    // the following code recognizes read errors by looking at
    // the value inside sd->message_length; if its negative then a
    // read error occurred. Otherwise everything went fine.
    sd->message_length = -1;
  }
  return retval;
}

/**
 * Send data read from an already open file descriptor.
 *
 * We return 1 on sucess and 0 on errors.
 *
 * ***FIXME***
 * We use ff_pkt->statp.st_size when FO_SPARSE to know when to stop reading.
 * Currently this is not a problem as the only other stream, resource forks,
 * are not handled as sparse files.
 */
static int send_data(JobControlRecord* jcr,
                     int stream,
                     FindFilesPacket* ff_pkt,
                     DIGEST* digest,
                     DIGEST* signing_digest)
{
  b_ctx bctx;
  BareosSocket* sd = jcr->store_bsock;
#ifdef FD_NO_SEND_TEST
  return 1;
#endif

  // Setup backup context.
  memset(&bctx, 0, sizeof(b_ctx));
  bctx.jcr = jcr;
  bctx.ff_pkt = ff_pkt;
  bctx.msgsave = sd->msg;                  /* save the original sd buffer */
  bctx.rbuf = sd->msg;                     /* read buffer */
  bctx.wbuf = sd->msg;                     /* write buffer */
  bctx.rsize = jcr->buf_size;              /* read buffer size */
  bctx.cipher_input = (uint8_t*)bctx.rbuf; /* encrypt uncompressed data */
  bctx.digest = digest;                    /* encryption digest */
  bctx.signing_digest = signing_digest;    /* signing digest */

  Dmsg1(300, "Saving data, type=%d\n", ff_pkt->type);

  if (!SetupCompressionContext(bctx)) { goto bail_out; }

  if (!SetupEncryptionContext(bctx)) { goto bail_out; }

  /* Send Data header to Storage daemon
   *    <file-index> <stream> <info> */
  if (!sd->fsend("%" PRIu32 " %" PRId32 " 0", jcr->JobFiles, stream)) {
    if (!jcr->IsJobCanceled()) {
      Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
    }
    goto bail_out;
  }
  Dmsg1(300, ">stored: datahdr %s", sd->msg);

  /* Make space at beginning of buffer for fileAddr because this
   *   same buffer will be used for writing if compression is off. */
  if (BitIsSet(FO_SPARSE, ff_pkt->flags)
      || BitIsSet(FO_OFFSETS, ff_pkt->flags)) {
    bctx.rbuf += OFFSET_FADDR_SIZE;
    bctx.rsize -= OFFSET_FADDR_SIZE;
#ifdef HAVE_FREEBSD_OS
    // To read FreeBSD partitions, the read size must be a multiple of 512.
    bctx.rsize = (bctx.rsize / 512) * 512;
#endif
  }

  // A RAW device read on win32 only works if the buffer is a multiple of 512
#ifdef HAVE_WIN32
  if (S_ISBLK(ff_pkt->statp.st_mode)) { bctx.rsize = (bctx.rsize / 512) * 512; }

  if (ff_pkt->statp.st_rdev & FILE_ATTRIBUTE_ENCRYPTED) {
    if (!SendEncryptedData(bctx)) { goto bail_out; }
  } else {
    if (!SendPlainData(bctx)) { goto bail_out; }
  }
#else
  if (!SendPlainData(bctx)) { goto bail_out; }
#endif

  if (sd->message_length < 0) { /* error */
    BErrNo be;
    Jmsg(jcr, M_ERROR, 0, T_("Read error on file %s. ERR=%s\n"), ff_pkt->fname,
         be.bstrerror(ff_pkt->bfd.BErrNo));
    if (jcr->JobErrors++ > 1000) { /* insanity check */
      Jmsg(jcr, M_FATAL, 0, T_("Too many errors. JobErrors=%d.\n"),
           jcr->JobErrors);
    }
  } else if (BitIsSet(FO_ENCRYPT, ff_pkt->flags)) {
    // For encryption, we must call finalize to push out any buffered data.
    if (!CryptoCipherFinalize(bctx.cipher_ctx,
                              (uint8_t*)jcr->fd_impl->crypto.crypto_buf,
                              &bctx.encrypted_len)) {
      // Padding failed. Shouldn't happen.
      Jmsg(jcr, M_FATAL, 0, T_("Encryption padding error\n"));
      goto bail_out;
    }

    // Note, on SSL pre-0.9.7, there is always some output
    if (bctx.encrypted_len > 0) {
      sd->message_length = bctx.encrypted_len;   /* set encrypted length */
      sd->msg = jcr->fd_impl->crypto.crypto_buf; /* set correct write buffer */
      if (!sd->send()) {
        if (!jcr->IsJobCanceled()) {
          Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
                sd->bstrerror());
        }
        goto bail_out;
      }
      Dmsg1(130, "Send data to SD len=%d\n", sd->message_length);
      jcr->JobBytes += sd->message_length; /* count bytes saved possibly
                                              compressed/encrypted */
      sd->msg = bctx.msgsave;              /* restore bnet buffer */
    }
  }

  if (!sd->signal(BNET_EOD)) { /* indicate end of file data */
    if (!jcr->IsJobCanceled()) {
      Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
    }
    goto bail_out;
  }

  // Free the cipher context
  if (bctx.cipher_ctx) { CryptoCipherFree(bctx.cipher_ctx); }

  return 1;

bail_out:
  // Free the cipher context
  if (bctx.cipher_ctx) { CryptoCipherFree(bctx.cipher_ctx); }

  sd->msg = bctx.msgsave; /* restore bnet buffer */
  sd->message_length = 0;

  return 0;
}

// jcr->mutex_guard() needs to be unlocked!
static std::optional<int32_t> get_next_findex(JobControlRecord* jcr)
{
  std::unique_lock l(jcr->mutex_guard());
  if (jcr->JobFiles == MAXIMUM_ALLOWED_FILES_PER_JOB) {
    // maximum reached, we cannot add a next one
    return std::nullopt;
  }
  return ++jcr->JobFiles; /* increment number of files sent */
}

bool EncodeAndSendAttributes(JobControlRecord* jcr,
                             FindFilesPacket* ff_pkt,
                             int& data_stream)
{
  BareosSocket* sd = jcr->store_bsock;
  PoolMem attribs(PM_NAME), attribsExBuf(PM_NAME);
  char* attribsEx = NULL;
  int attr_stream;
  int comp_len;
  bool status;
  int hangup = GetHangup();
#ifdef FD_NO_SEND_TEST
  return true;
#endif

  Dmsg1(300, "encode_and_send_attrs fname=%s\n", ff_pkt->fname);
  /** Find what data stream we will use, then encode the attributes */
  if ((data_stream = SelectDataStream(ff_pkt)) == STREAM_NONE) {
    /* This should not happen */
    Jmsg0(jcr, M_FATAL, 0,
          T_("Invalid file flags, no supported data stream type.\n"));
    return false;
  }
  EncodeStat(attribs.c_str(), &ff_pkt->statp, sizeof(ff_pkt->statp),
             ff_pkt->LinkFI, data_stream);

  /** Now possibly extend the attributes */
  if (IS_FT_OBJECT(ff_pkt->type)) {
    attr_stream = STREAM_RESTORE_OBJECT;
  } else {
    attribsEx = attribsExBuf.c_str();
    attr_stream = encode_attribsEx(jcr, attribsEx, ff_pkt);
  }

  Dmsg3(300, "File %s\nattribs=%s\nattribsEx=%s\n", ff_pkt->fname,
        attribs.c_str(), attribsEx);

  if (std::optional findex = get_next_findex(jcr)) {
    ff_pkt->FileIndex = *findex;
    PmStrcpy(jcr->fd_impl->last_fname, ff_pkt->fname);
  } else {
    Jmsg1(jcr, M_FATAL, 0,
          "FileIndex overflow detected. Please split the fileset to backup "
          "fewer files.\n");
    ff_pkt->FileIndex = -1;
    return false;
  }

  // Debug code: check if we must hangup
  if (hangup && (jcr->JobFiles > (uint32_t)hangup)) {
    jcr->setJobStatusWithPriorityCheck(JS_Incomplete);
    Jmsg1(jcr, M_FATAL, 0, "Debug hangup requested after %d files.\n", hangup);
    SetHangup(0);
    return false;
  }

  /* Send Attributes header to Storage daemon
   *    <file-index> <stream> <info> */
  if (!sd->fsend("%" PRIu32 " %" PRId32 " 0", jcr->JobFiles, attr_stream)) {
    if (!jcr->IsJobCanceled() && !jcr->IsIncomplete()) {
      Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
    }
    return false;
  }
  Dmsg1(300, ">stored: attrhdr %s", sd->msg);

  /* Send file attributes to Storage daemon
   *   File_index
   *   File type
   *   Filename (full path)
   *   Encoded attributes
   *   Link name (if type==FT_LNK or FT_LNKSAVED)
   *   Encoded extended-attributes (for Win32)
   *   Delta Sequence Number
   *
   * or send Restore Object to Storage daemon
   *   File_index
   *   File_type
   *   Object_index
   *   Object_len  (possibly compressed)
   *   Object_full_len (not compressed)
   *   Object_compression
   *   Plugin_name
   *   Object_name
   *   Binary Object data
   *
   * For a directory, link is the same as fname, but with trailing
   * slash. For a linked file, link is the link. */
  if (!IS_FT_OBJECT(ff_pkt->type)
      && ff_pkt->type != FT_DELETED) { /* already stripped */
    StripPath(ff_pkt);
  }
  switch (ff_pkt->type) {
    case FT_JUNCTION:
    case FT_LNK:
    case FT_LNKSAVED:
      Dmsg3(300, "Link %d %s to %s\n", jcr->JobFiles, ff_pkt->fname,
            ff_pkt->link_or_dir);
      status = sd->fsend("%" PRIu32 " %d %s%c%s%c%s%c%s%c%u%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->fname, 0, attribs.c_str(), 0,
                         ff_pkt->link_or_dir, 0, attribsEx, 0,
                         ff_pkt->delta_seq, 0);
      break;
    case FT_DIREND:
    case FT_REPARSE:
      /* Here link is the canonical filename (i.e. with trailing slash) */
      status = sd->fsend("%" PRIu32 " %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->link_or_dir, 0, attribs.c_str(),
                         0, 0, attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
    case FT_PLUGIN_CONFIG:
    case FT_RESTORE_FIRST:
      comp_len = ff_pkt->object_len;
      ff_pkt->object_compression = 0;

      if (ff_pkt->object_len > 1000) {
        // Big object, compress it
        comp_len = compressBound(ff_pkt->object_len);
        POOLMEM* comp_obj = GetMemory(comp_len);
        // FIXME: check Zdeflate error
        Zdeflate(ff_pkt->object, ff_pkt->object_len, comp_obj, comp_len);
        if (comp_len < ff_pkt->object_len) {
          ff_pkt->object = comp_obj;
          ff_pkt->object_compression = 1; /* zlib level 9 compression */
        } else {
          // Uncompressed object smaller, use it
          comp_len = ff_pkt->object_len;
        }
        Dmsg2(100, "Object compressed from %d to %d bytes\n",
              ff_pkt->object_len, comp_len);
      }

      sd->message_length = Mmsg(
          sd->msg, "%d %d %d %d %d %d %s%c%s%c", jcr->JobFiles, ff_pkt->type,
          ff_pkt->object_index, comp_len, ff_pkt->object_len,
          ff_pkt->object_compression, ff_pkt->fname, 0, ff_pkt->object_name, 0);
      sd->msg = CheckPoolMemorySize(sd->msg, sd->message_length + comp_len + 2);
      memcpy(sd->msg + sd->message_length, ff_pkt->object, comp_len);

      // Note we send one extra byte so Dir can store zero after object
      sd->message_length += comp_len + 1;
      // initalize the extra byte
      sd->msg[sd->message_length - 1] = '\0';
      status = sd->send();
      if (ff_pkt->object_compression) { FreeAndNullPoolMemory(ff_pkt->object); }
      break;
    case FT_REG:
      status = sd->fsend("%" PRIu32 " %d %s%c%s%c%c%s%c%d%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->fname, 0, attribs.c_str(), 0, 0,
                         attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
    default:
      status = sd->fsend("%" PRIu32 " %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->fname, 0, attribs.c_str(), 0, 0,
                         attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
  }

  if (!IS_FT_OBJECT(ff_pkt->type) && ff_pkt->type != FT_DELETED) {
    UnstripPath(ff_pkt);
  }

  Dmsg2(300, ">stored: attr len=%d: %s\n", sd->message_length, sd->msg);
  if (!status && !jcr->IsJobCanceled()) {
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
  }

  sd->signal(BNET_EOD); /* indicate end of attributes data */

  return status;
}

// Do in place strip of path
static bool do_strip(int count, char* in)
{
  char* out = in;
  int stripped;
  int numsep = 0;

  // Copy to first path separator -- Win32 might have c: ...
  while (*in && !IsPathSeparator(*in)) {
    out++;
    in++;
  }
  if (*in) { /* Not at the end of the string */
    out++;
    in++;
    numsep++; /* one separator seen */
  }
  for (stripped = 0; stripped < count && *in; stripped++) {
    while (*in && !IsPathSeparator(*in)) { in++; /* skip chars */ }
    if (*in) {
      numsep++; /* count separators seen */
      in++;     /* skip separator */
    }
  }

  // Copy to end
  while (*in) { /* copy to end */
    if (IsPathSeparator(*in)) { numsep++; }
    *out++ = *in++;
  }
  *out = 0;
  Dmsg4(500, "stripped=%d count=%d numsep=%d sep>count=%d\n", stripped, count,
        numsep, numsep > count);
  return stripped == count && numsep > count;
}

/**
 * If requested strip leading components of the path so that we can
 * save file as if it came from a subdirectory.  This is most useful
 * for dealing with snapshots, by removing the snapshot directory, or
 * in handling vendor migrations where files have been restored with
 * a vendor product into a subdirectory.
 */
void StripPath(FindFilesPacket* ff_pkt)
{
  if (!BitIsSet(FO_STRIPPATH, ff_pkt->flags) || ff_pkt->StripPath <= 0) {
    Dmsg1(200, "No strip for %s\n", ff_pkt->fname);
    return;
  }

  if (!ff_pkt->fname_save) {
    ff_pkt->fname_save = GetPoolMemory(PM_FNAME);
    ff_pkt->link_save = GetPoolMemory(PM_FNAME);
  }

  PmStrcpy(ff_pkt->fname_save, ff_pkt->fname);
  if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link_or_dir) {
    PmStrcpy(ff_pkt->link_save, ff_pkt->link_or_dir);
    Dmsg2(500, "strcpy link_save=%" PRIuz " link=%" PRIuz "\n",
          strlen(ff_pkt->link_save), strlen(ff_pkt->link_or_dir));
  }

  /* Strip path. If it doesn't succeed put it back. If it does, and there
   * is a different link string, attempt to strip the link. If it fails,
   * back them both back. Do not strip symlinks. I.e. if either stripping
   * fails don't strip anything. */
  if ((ff_pkt->type != FT_DIREND && ff_pkt->type != FT_REPARSE)
      || ff_pkt->fname == ff_pkt->link_or_dir) {
    if (!do_strip(ff_pkt->StripPath, ff_pkt->fname)) {
      UnstripPath(ff_pkt);
      goto rtn;
    }
  }

  // Strip links but not symlinks
  if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link_or_dir) {
    if (!do_strip(ff_pkt->StripPath, ff_pkt->link_or_dir)) {
      UnstripPath(ff_pkt);
    }
  }

rtn:
  Dmsg3(100, "fname=%s stripped=%s link=%s\n", ff_pkt->fname_save,
        ff_pkt->fname, ff_pkt->link_or_dir);
}

void UnstripPath(FindFilesPacket* ff_pkt)
{
  if (!BitIsSet(FO_STRIPPATH, ff_pkt->flags) || ff_pkt->StripPath <= 0) {
    return;
  }

  strcpy(ff_pkt->fname, ff_pkt->fname_save);
  if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link_or_dir) {
    Dmsg2(500, "strcpy link=%s link_save=%s\n", ff_pkt->link_or_dir,
          ff_pkt->link_save);
    strcpy(ff_pkt->link_or_dir, ff_pkt->link_save);
    Dmsg2(500, "strcpy link=%" PRIuz " link_save=%" PRIuz "\n",
          strlen(ff_pkt->link_or_dir), strlen(ff_pkt->link_save));
  }
}

#if defined(WIN32_VSS)
static void CloseVssBackupSession(JobControlRecord* jcr)
{
  /* STOP VSS ON WIN32
   * Tell vss to close the backup session */
  if (jcr->fd_impl->pVSSClient) {
    /* We are about to call the BackupComplete VSS method so let all plugins
     * know that by raising the bEventVssBackupComplete event. */
    GeneratePluginEvent(jcr, bEventVssBackupComplete);
    if (jcr->fd_impl->pVSSClient->CloseBackup()) {
      // Inform user about writer states
      for (size_t i = 0; i < jcr->fd_impl->pVSSClient->GetWriterCount(); i++) {
        int msg_type = M_INFO;
        if (jcr->fd_impl->pVSSClient->GetWriterState(i) < 1) {
          msg_type = M_WARNING;
          jcr->JobErrors++;
        }
        Jmsg(jcr, msg_type, 0, T_("VSS Writer (BackupComplete): %s\n"),
             jcr->fd_impl->pVSSClient->GetWriterInfo(i));
      }
    }

    // Generate Job global writer metadata
    wchar_t* metadata = jcr->fd_impl->pVSSClient->GetMetadata();
    if (metadata) {
      FindFilesPacket* ff_pkt = jcr->fd_impl->ff;
      ff_pkt->fname = (char*)"*all*"; /* for all plugins */
      ff_pkt->type = FT_RESTORE_FIRST;
      ff_pkt->LinkFI = 0;
      ff_pkt->object_name = (char*)"job_metadata.xml";
      ff_pkt->object = BSTR_2_str(metadata);
      ff_pkt->object_len = strlen(ff_pkt->object) + 1;
      ff_pkt->object_index = (int)time(NULL);
      SaveFile(jcr, ff_pkt, true);
    }
  }
}
#endif
} /* namespace filedaemon */
