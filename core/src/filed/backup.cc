/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March MM
 */
/**
 * @file
 * Bareos File Daemon  backup.c  send file attributes and data to the Storage daemon.
 */

#include "bareos.h"
#include "filed.h"
#include "filed/accurate.h"
#include "filed/compression.h"
#include "filed/crypto.h"
#include "filed/heartbeat.h"
#include "filed/backup.h"
#include "ch.h"
#include "findlib/attribs.h"
#include "findlib/hardlink.h"
#include "findlib/find_one.h"

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
#define compressBound(sourceLen) (sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13)
#endif

/* Forward referenced functions */
int save_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level);
static int send_data(JobControlRecord *jcr, int stream, FindFilesPacket *ff_pkt,
                     DIGEST *digest, DIGEST *signature_digest);
bool encode_and_send_attributes(JobControlRecord *jcr, FindFilesPacket *ff_pkt, int &data_stream);
static void close_vss_backup_session(JobControlRecord *jcr);

/**
 * Find all the requested files and send them
 * to the Storage daemon.
 *
 * Note, we normally carry on a one-way
 * conversation from this point on with the SD, simply blasting
 * data to him.  To properly know what is going on, we
 * also run a "heartbeat" monitor which reads the socket and
 * reacts accordingly (at the moment it has nothing to do
 * except echo the heartbeat to the Director).
 */
bool blast_data_to_storage_daemon(JobControlRecord *jcr, char *addr, crypto_cipher_t cipher)
{
   BareosSocket *sd;
   bool ok = true;

   sd = jcr->store_bsock;

   jcr->setJobStatus(JS_Running);

   Dmsg1(300, "filed: opened data connection %d to stored\n", sd->fd_);

   LockRes();
   ClientResource *client = (ClientResource *)GetNextRes(R_CLIENT, NULL);
   UnlockRes();
   uint32_t buf_size;
   if (client) {
      buf_size = client->max_network_buffer_size;
   } else {
      buf_size = 0;                   /* use default */
   }
   if (!sd->set_buffer_size(buf_size, BNET_SETBUF_WRITE)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      Jmsg(jcr, M_FATAL, 0, _("Cannot set buffer size FD->SD.\n"));
      return false;
   }

   jcr->buf_size = sd->msglen;

   if (!adjust_compression_buffers(jcr)) {
      return false;
   }

   if (!crypto_session_start(jcr, cipher)) {
      return false;
   }

   set_find_options((FindFilesPacket *)jcr->ff, jcr->incremental, jcr->mtime);

   /**
    * In accurate mode, we overload the find_one check function
    */
   if (jcr->accurate) {
      set_find_changed_function((FindFilesPacket *)jcr->ff, accurate_check_file);
   }

   start_heartbeat_monitor(jcr);

   if (have_acl) {
      jcr->acl_data = (acl_data_t *)malloc(sizeof(acl_data_t));
      memset(jcr->acl_data, 0, sizeof(acl_data_t));
      jcr->acl_data->u.build = (acl_build_data_t *)malloc(sizeof(acl_build_data_t));
      memset(jcr->acl_data->u.build, 0, sizeof(acl_build_data_t));
      jcr->acl_data->u.build->content = get_pool_memory(PM_MESSAGE);
   }

   if (have_xattr) {
      jcr->xattr_data = (xattr_data_t *)malloc(sizeof(xattr_data_t));
      memset(jcr->xattr_data, 0, sizeof(xattr_data_t));
      jcr->xattr_data->u.build = (xattr_build_data_t *)malloc(sizeof(xattr_build_data_t));
      memset(jcr->xattr_data->u.build, 0, sizeof(xattr_build_data_t));
      jcr->xattr_data->u.build->content = get_pool_memory(PM_MESSAGE);
   }

   /**
    * Subroutine save_file() is called for each file
    */
   if (!find_files(jcr, (FindFilesPacket *)jcr->ff, save_file, plugin_save)) {
      ok = false;                     /* error */
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   if (have_acl && jcr->acl_data->u.build->nr_errors > 0) {
      Jmsg(jcr, M_WARNING, 0, _("Encountered %ld acl errors while doing backup\n"),
           jcr->acl_data->u.build->nr_errors);
   }
   if (have_xattr && jcr->xattr_data->u.build->nr_errors > 0) {
      Jmsg(jcr, M_WARNING, 0, _("Encountered %ld xattr errors while doing backup\n"),
           jcr->xattr_data->u.build->nr_errors);
   }

   close_vss_backup_session(jcr);

   accurate_finish(jcr);              /* send deleted or base file list to SD */

   stop_heartbeat_monitor(jcr);

   sd->signal(BNET_EOD);            /* end of sending data */

   if (have_acl && jcr->acl_data) {
      free_pool_memory(jcr->acl_data->u.build->content);
      free(jcr->acl_data->u.build);
      free(jcr->acl_data);
      jcr->acl_data = NULL;
   }

   if (have_xattr && jcr->xattr_data) {
      free_pool_memory(jcr->xattr_data->u.build->content);
      free(jcr->xattr_data->u.build);
      free(jcr->xattr_data);
      jcr->xattr_data = NULL;
   }

   if (jcr->big_buf) {
      free(jcr->big_buf);
      jcr->big_buf = NULL;
   }

   cleanup_compression(jcr);
   crypto_session_end(jcr);

   Dmsg1(100, "end blast_data ok=%d\n", ok);
   return ok;
}

/**
 * Save OSX specific resource forks and finder info.
 */
static inline bool save_rsrc_and_finder(b_save_ctx &bsctx)
{
   char flags[FOPTS_BYTES];
   int rsrc_stream;
   BareosSocket *sd = bsctx.jcr->store_bsock;
   bool retval = false;

   if (bsctx.ff_pkt->hfsinfo.rsrclength > 0) {
      if (bopen_rsrc(&bsctx.ff_pkt->bfd, bsctx.ff_pkt->fname, O_RDONLY | O_BINARY, 0) < 0) {
         bsctx.ff_pkt->ff_errno = errno;
         berrno be;
         Jmsg(bsctx.jcr, M_NOTSAVED, -1,
              _("     Cannot open resource fork for \"%s\": ERR=%s.\n"),
              bsctx.ff_pkt->fname, be.bstrerror());
         bsctx.jcr->JobErrors++;
         if (is_bopen(&bsctx.ff_pkt->bfd)) {
            bclose(&bsctx.ff_pkt->bfd);
         }
      } else {
         int status;

         memcpy(flags, bsctx.ff_pkt->flags, sizeof(flags));
         clear_bit(FO_COMPRESS, bsctx.ff_pkt->flags);
         clear_bit(FO_SPARSE, bsctx.ff_pkt->flags);
         clear_bit(FO_OFFSETS, bsctx.ff_pkt->flags);
         rsrc_stream = bit_is_set(FO_ENCRYPT, flags) ? STREAM_ENCRYPTED_MACOS_FORK_DATA :
                                                       STREAM_MACOS_FORK_DATA;

         status = send_data(bsctx.jcr, rsrc_stream, bsctx.ff_pkt,
                            bsctx.digest, bsctx.signing_digest);

         memcpy(bsctx.ff_pkt->flags, flags, sizeof(flags));
         bclose(&bsctx.ff_pkt->bfd);
         if (!status) {
            goto bail_out;
         }
      }
   }

   Dmsg1(300, "Saving Finder Info for \"%s\"\n", bsctx.ff_pkt->fname);
   sd->fsend("%ld %d 0", bsctx.jcr->JobFiles, STREAM_HFSPLUS_ATTRIBUTES);
   Dmsg1(300, "filed>stored:header %s", sd->msg);
   pm_memcpy(sd->msg, bsctx.ff_pkt->hfsinfo.fndrinfo, 32);
   sd->msglen = 32;
   if (bsctx.digest) {
      crypto_digest_update(bsctx.digest, (uint8_t *)sd->msg, sd->msglen);
   }
   if (bsctx.signing_digest) {
      crypto_digest_update(bsctx.signing_digest, (uint8_t *)sd->msg, sd->msglen);
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
static inline bool setup_encryption_digests(b_save_ctx &bsctx)
{
   bool retval = false;
   // TODO landonf: Allow the user to specify the digest algorithm
#ifdef HAVE_SHA2
   crypto_digest_t signing_algorithm = CRYPTO_DIGEST_SHA256;
#else
   crypto_digest_t signing_algorithm = CRYPTO_DIGEST_SHA1;
#endif

   if (bit_is_set(FO_MD5, bsctx.ff_pkt->flags)) {
      bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_MD5);
      bsctx.digest_stream = STREAM_MD5_DIGEST;
   } else if (bit_is_set(FO_SHA1, bsctx.ff_pkt->flags)) {
      bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_SHA1);
      bsctx.digest_stream = STREAM_SHA1_DIGEST;
   } else if (bit_is_set(FO_SHA256, bsctx.ff_pkt->flags)) {
      bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_SHA256);
      bsctx.digest_stream = STREAM_SHA256_DIGEST;
   } else if (bit_is_set(FO_SHA512, bsctx.ff_pkt->flags)) {
      bsctx.digest = crypto_digest_new(bsctx.jcr, CRYPTO_DIGEST_SHA512);
      bsctx.digest_stream = STREAM_SHA512_DIGEST;
   }

   /*
    * Did digest initialization fail?
    */
   if (bsctx.digest_stream != STREAM_NONE && bsctx.digest == NULL) {
      Jmsg(bsctx.jcr, M_WARNING, 0, _("%s digest initialization failed\n"),
           stream_to_ascii(bsctx.digest_stream));
   }

   /**
    * Set up signature digest handling. If this fails, the signature digest
    * will be set to NULL and not used.
    */
   /* TODO landonf: We should really only calculate the digest once, for
    * both verification and signing.
    */
   if (bsctx.jcr->crypto.pki_sign) {
      bsctx.signing_digest = crypto_digest_new(bsctx.jcr, signing_algorithm);

      /*
       * Full-stop if a failure occurred initializing the signature digest
       */
      if (bsctx.signing_digest == NULL) {
         Jmsg(bsctx.jcr, M_NOTSAVED, 0, _("%s signature digest initialization failed\n"),
              stream_to_ascii(signing_algorithm));
         bsctx.jcr->JobErrors++;
         goto bail_out;
      }
   }

   /*
    * Enable encryption
    */
   if (bsctx.jcr->crypto.pki_encrypt) {
      set_bit(FO_ENCRYPT, bsctx.ff_pkt->flags);
   }
   retval = true;

bail_out:
   return retval;
}

/**
 * Terminate the signing digest and send it to the Storage daemon
 */
static inline bool terminate_signing_digest(b_save_ctx &bsctx)
{
   uint32_t size = 0;
   bool retval = false;
   SIGNATURE *signature = NULL;
   BareosSocket *sd = bsctx.jcr->store_bsock;

   if ((signature = crypto_sign_new(bsctx.jcr)) == NULL) {
      Jmsg(bsctx.jcr, M_FATAL, 0, _("Failed to allocate memory for crypto signature.\n"));
      goto bail_out;
   }

   if (!crypto_sign_add_signer(signature, bsctx.signing_digest, bsctx.jcr->crypto.pki_keypair)) {
      Jmsg(bsctx.jcr, M_FATAL, 0, _("An error occurred while signing the stream.\n"));
      goto bail_out;
   }

   /*
    * Get signature size
    */
   if (!crypto_sign_encode(signature, NULL, &size)) {
      Jmsg(bsctx.jcr, M_FATAL, 0, _("An error occurred while signing the stream.\n"));
      goto bail_out;
   }

   /*
    * Grow the bsock buffer to fit our message if necessary
    */
   if (sizeof_pool_memory(sd->msg) < (int32_t)size) {
      sd->msg = realloc_pool_memory(sd->msg, size);
   }

   /*
    * Send our header
    */
   sd->fsend("%ld %ld 0", bsctx.jcr->JobFiles, STREAM_SIGNED_DIGEST);
   Dmsg1(300, "filed>stored:header %s", sd->msg);

   /*
    * Encode signature data
    */
   if (!crypto_sign_encode(signature, (uint8_t *)sd->msg, &size)) {
      Jmsg(bsctx.jcr, M_FATAL, 0, _("An error occurred while signing the stream.\n"));
      goto bail_out;
   }

   sd->msglen = size;
   sd->send();
   sd->signal(BNET_EOD);              /* end of checksum */
   retval = true;

bail_out:
   if (signature) {
      crypto_sign_free(signature);
   }
   return retval;
}

/**
 * Terminate any digest and send it to Storage daemon
 */
static inline bool terminate_digest(b_save_ctx &bsctx)
{
   uint32_t size;
   bool retval = false;
   BareosSocket *sd = bsctx.jcr->store_bsock;

   sd->fsend("%ld %d 0", bsctx.jcr->JobFiles, bsctx.digest_stream);
   Dmsg1(300, "filed>stored:header %s", sd->msg);

   size = CRYPTO_DIGEST_MAX_SIZE;

   /*
    * Grow the bsock buffer to fit our message if necessary
    */
   if (sizeof_pool_memory(sd->msg) < (int32_t)size) {
      sd->msg = realloc_pool_memory(sd->msg, size);
   }

   if (!crypto_digest_finalize(bsctx.digest, (uint8_t *)sd->msg, &size)) {
      Jmsg(bsctx.jcr, M_FATAL, 0, _("An error occurred finalizing signing the stream.\n"));
      goto bail_out;
   }

   /*
    * Keep the checksum if this file is a hardlink
    */
   if (bsctx.ff_pkt->linked) {
      ff_pkt_set_link_digest(bsctx.ff_pkt, bsctx.digest_stream, sd->msg, size);
   }

   sd->msglen = size;
   sd->send();
   sd->signal(BNET_EOD);              /* end of checksum */
   retval = true;

bail_out:
   return retval;
}

static inline bool do_backup_acl(JobControlRecord *jcr, FindFilesPacket *ff_pkt)
{
   bacl_exit_code retval;

   jcr->acl_data->filetype = ff_pkt->type;
   jcr->acl_data->last_fname = jcr->last_fname;

   if (jcr->is_plugin()) {
      retval = plugin_build_acl_streams(jcr, jcr->acl_data, ff_pkt);
   } else {
      retval = build_acl_streams(jcr, jcr->acl_data, ff_pkt);
   }

   switch (retval) {
   case bacl_exit_fatal:
      return false;
   case bacl_exit_error:
      /*
       * Non-fatal errors, count them and when the number is under
       * ACL_REPORT_ERR_MAX_PER_JOB print the error message set by the
       * lower level routine in jcr->errmsg.
       */
      if (jcr->acl_data->u.build->nr_errors < ACL_REPORT_ERR_MAX_PER_JOB) {
         Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);
      }
      jcr->acl_data->u.build->nr_errors++;
      break;
   case bacl_exit_ok:
      break;
   }

   return true;
}

static inline bool do_backup_xattr(JobControlRecord *jcr, FindFilesPacket *ff_pkt)
{
   bxattr_exit_code retval;

   jcr->xattr_data->last_fname = jcr->last_fname;

   if (jcr->is_plugin()) {
      retval = plugin_build_xattr_streams(jcr, jcr->xattr_data, ff_pkt);
   } else {
      retval = build_xattr_streams(jcr, jcr->xattr_data, ff_pkt);
   }

   switch (retval) {
   case bxattr_exit_fatal:
      return false;
   case bxattr_exit_error:
      /*
       * Non-fatal errors, count them and when the number is under
       * XATTR_REPORT_ERR_MAX_PER_JOB print the error message set by the
       * lower level routine in jcr->errmsg.
       */
      if (jcr->xattr_data->u.build->nr_errors < XATTR_REPORT_ERR_MAX_PER_JOB) {
         Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);
      }
      jcr->xattr_data->u.build->nr_errors++;
      break;
   case bxattr_exit_ok:
      break;
   }

   return true;
}

/**
 * Called here by find() for each file included.
 * This is a callback. The original is find_files() above.
 *
 * Send the file and its data to the Storage daemon.
 *
 * Returns: 1 if OK
 *          0 if error
 *         -1 to ignore file/directory (not used here)
 */
int save_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level)
{
   bool do_read = false;
   bool plugin_started = false;
   bool do_plugin_set = false;
   int status, data_stream;
   int rtnstat = 0;
   b_save_ctx bsctx;
   bool has_file_data = false;
   struct save_pkt sp;          /* use by option plugin */
   BareosSocket *sd = jcr->store_bsock;

   if (jcr->is_canceled() || jcr->is_incomplete()) {
      return 0;
   }

   jcr->num_files_examined++;         /* bump total file count */

   switch (ff_pkt->type) {
   case FT_LNKSAVED:                  /* Hard linked, file already saved */
      Dmsg2(130, "FT_LNKSAVED hard link: %s => %s\n", ff_pkt->fname, ff_pkt->link);
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
      Dmsg2(130, "FT_LNK saving: %s -> %s\n", ff_pkt->fname, ff_pkt->link);
      break;
   case FT_RESTORE_FIRST:
      Dmsg1(100, "FT_RESTORE_FIRST saving: %s\n", ff_pkt->fname);
      break;
   case FT_PLUGIN_CONFIG:
      Dmsg1(100, "FT_PLUGIN_CONFIG saving: %s\n", ff_pkt->fname);
      break;
   case FT_DIRBEGIN:
      jcr->num_files_examined--;      /* correct file count */
      return 1;                       /* not used */
   case FT_NORECURSE:
      Jmsg(jcr, M_INFO, 1, _("     Recursion turned off. Will not descend from %s into %s\n"),
           ff_pkt->top_fname, ff_pkt->fname);
      ff_pkt->type = FT_DIREND;       /* Backup only the directory entry */
      break;
   case FT_NOFSCHG:
      /* Suppress message for /dev filesystems */
      if (!is_in_fileset(ff_pkt)) {
         Jmsg(jcr, M_INFO, 1, _("     %s is a different filesystem. Will not descend from %s into it.\n"),
              ff_pkt->fname, ff_pkt->top_fname);
      }
      ff_pkt->type = FT_DIREND;       /* Backup only the directory entry */
      break;
   case FT_INVALIDFS:
      Jmsg(jcr, M_INFO, 1, _("     Disallowed filesystem. Will not descend from %s into %s\n"),
           ff_pkt->top_fname, ff_pkt->fname);
      ff_pkt->type = FT_DIREND;       /* Backup only the directory entry */
      break;
   case FT_INVALIDDT:
      Jmsg(jcr, M_INFO, 1, _("     Disallowed drive type. Will not descend into %s\n"),
           ff_pkt->fname);
      break;
   case FT_REPARSE:
   case FT_JUNCTION:
   case FT_DIREND:
      Dmsg1(130, "FT_DIREND: %s\n", ff_pkt->link);
      break;
   case FT_SPEC:
      Dmsg1(130, "FT_SPEC saving: %s\n", ff_pkt->fname);
      if (S_ISSOCK(ff_pkt->statp.st_mode)) {
        Jmsg(jcr, M_SKIPPED, 1, _("     Socket file skipped: %s\n"), ff_pkt->fname);
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
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not access \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_NOFOLLOW: {
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not follow link \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_NOSTAT: {
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not stat \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_DIRNOCHG:
   case FT_NOCHG:
      Jmsg(jcr, M_SKIPPED, 1, _("     Unchanged file skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_ISARCH:
      Jmsg(jcr, M_NOTSAVED, 0, _("     Archive file not saved: %s\n"), ff_pkt->fname);
      return 1;
   case FT_NOOPEN: {
      berrno be;
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not open directory \"%s\": ERR=%s\n"),
           ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
      jcr->JobErrors++;
      return 1;
   }
   case FT_DELETED:
      Dmsg1(130, "FT_DELETED: %s\n", ff_pkt->fname);
      break;
   default:
      Jmsg(jcr, M_NOTSAVED, 0,  _("     Unknown file type %d; not saved: %s\n"),
           ff_pkt->type, ff_pkt->fname);
      jcr->JobErrors++;
      return 1;
   }

   Dmsg1(130, "filed: sending %s to stored\n", ff_pkt->fname);

   /*
    * Setup backup signing context.
    */
   memset(&bsctx, 0, sizeof(b_save_ctx));
   bsctx.digest_stream = STREAM_NONE;
   bsctx.jcr = jcr;
   bsctx.ff_pkt = ff_pkt;

   /*
    * Digests and encryption are only useful if there's file data
    */
   if (has_file_data) {
      if (!setup_encryption_digests(bsctx)) {
         goto good_rtn;
      }
   }

   /*
    * Initialize the file descriptor we use for data and other streams.
    */
   binit(&ff_pkt->bfd);
   if (bit_is_set(FO_PORTABLE, ff_pkt->flags)) {
      set_portable_backup(&ff_pkt->bfd); /* disable Win32 BackupRead() */
   }

   /*
    * Option and cmd plugin are not compatible together
    */
   if (ff_pkt->cmd_plugin) {
      do_plugin_set = true;
   } else if (ff_pkt->opt_plugin) {
      /*
       * Ask the option plugin what to do with this file
       */
      switch (plugin_option_handle_file(jcr, ff_pkt, &sp)) {
      case bRC_OK:
         Dmsg2(10, "Option plugin %s will be used to backup %s\n", ff_pkt->plugin, ff_pkt->fname);
         jcr->opt_plugin = true;
         jcr->plugin_sp = &sp;
         plugin_update_ff_pkt(ff_pkt, &sp);
         do_plugin_set = true;
         break;
      case bRC_Skip:
         Dmsg2(10, "Option plugin %s decided to skip %s\n", ff_pkt->plugin, ff_pkt->fname);
         goto good_rtn;
      case bRC_Core:
         Dmsg2(10, "Option plugin %s decided to let bareos handle %s\n", ff_pkt->plugin, ff_pkt->fname);
         break;
      default:
         goto bail_out;
      }
   }

   if (do_plugin_set) {
      /*
       * Tell bfile that it needs to call plugin
       */
      if (!set_cmd_plugin(&ff_pkt->bfd, jcr)) {
         goto bail_out;
      }
      send_plugin_name(jcr, sd, true);      /* signal start of plugin data */
      plugin_started = true;
   }

   /*
    * Send attributes -- must be done after binit()
    */
   if (!encode_and_send_attributes(jcr, ff_pkt, data_stream)) {
      goto bail_out;
   }

   /*
    * Meta data only for restore object
    */
   if (IS_FT_OBJECT(ff_pkt->type)) {
      goto good_rtn;
   }

   /*
    * Meta data only for deleted files
    */
   if (ff_pkt->type == FT_DELETED) {
      goto good_rtn;
   }

   /*
    * Set up the encryption context and send the session data to the SD
    */
   if (has_file_data && jcr->crypto.pki_encrypt) {
      if (!crypto_session_send(jcr, sd)) {
         goto bail_out;
      }
   }

   /*
    * For a command plugin use the setting from the plugins savepkt no_read field
    * which is saved in the ff_pkt->no_read variable. do_read is the inverted
    * value of this variable as no_read == TRUE means do_read == FALSE
    */
   if (ff_pkt->cmd_plugin) {
      do_read = !ff_pkt->no_read;
   } else {
      /*
       * Open any file with data that we intend to save, then save it.
       *
       * Note, if is_win32_backup, we must open the Directory so that
       * the BackupRead will save its permissions and ownership streams.
       */
      if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode)) {
#ifdef HAVE_WIN32
         do_read = !is_portable_backup(&ff_pkt->bfd) || ff_pkt->statp.st_size > 0;
#else
         do_read = ff_pkt->statp.st_size > 0;
#endif
      } else if (ff_pkt->type == FT_RAW ||
                 ff_pkt->type == FT_FIFO ||
                 ff_pkt->type == FT_REPARSE ||
                 ff_pkt->type == FT_JUNCTION ||
                (!is_portable_backup(&ff_pkt->bfd) &&
                 ff_pkt->type == FT_DIREND)) {
         do_read = true;
      }
   }

   Dmsg2(150, "type=%d do_read=%d\n", ff_pkt->type, do_read);
   if (do_read) {
      btimer_t *tid;
      int noatime;

      if (ff_pkt->type == FT_FIFO) {
         tid = start_thread_timer(jcr, pthread_self(), 60);
      } else {
         tid = NULL;
      }

      noatime = bit_is_set(FO_NOATIME, ff_pkt->flags) ? O_NOATIME : 0;
      ff_pkt->bfd.reparse_point = (ff_pkt->type == FT_REPARSE ||
                                   ff_pkt->type == FT_JUNCTION);

      if (bopen(&ff_pkt->bfd, ff_pkt->fname, O_RDONLY | O_BINARY | noatime, 0, ff_pkt->statp.st_rdev) < 0) {
         ff_pkt->ff_errno = errno;
         berrno be;
         Jmsg(jcr, M_NOTSAVED, 0,
              _("     Cannot open \"%s\": ERR=%s.\n"),
              ff_pkt->fname, be.bstrerror());
         jcr->JobErrors++;
         if (tid) {
            stop_thread_timer(tid);
            tid = NULL;
         }
         goto good_rtn;
      }

      if (tid) {
         stop_thread_timer(tid);
         tid = NULL;
      }

      status = send_data(jcr, data_stream, ff_pkt, bsctx.digest, bsctx.signing_digest);

      if (bit_is_set(FO_CHKCHANGES, ff_pkt->flags)) {
         has_file_changed(jcr, ff_pkt);
      }

      bclose(&ff_pkt->bfd);

      if (!status) {
         goto bail_out;
      }
   }

   if (have_darwin_os) {
      /*
       * Regular files can have resource forks and Finder Info
       */
      if (ff_pkt->type != FT_LNKSAVED &&
         (S_ISREG(ff_pkt->statp.st_mode) &&
          bit_is_set(FO_HFSPLUS, ff_pkt->flags))) {
         if (!save_rsrc_and_finder(bsctx)) {
            goto bail_out;
         }
      }
   }

   /*
    * Save ACLs when requested and available for anything not being a symlink.
    */
   if (have_acl) {
      if (bit_is_set(FO_ACL, ff_pkt->flags) && ff_pkt->type != FT_LNK) {
         if (!do_backup_acl(jcr, ff_pkt)) {
            goto bail_out;
         }
      }
   }

   /*
    * Save Extended Attributes when requested and available for all files.
    */
   if (have_xattr) {
      if (bit_is_set(FO_XATTR, ff_pkt->flags)) {
         if (!do_backup_xattr(jcr, ff_pkt)) {
            goto bail_out;
         }
      }
   }

   /*
    * Terminate the signing digest and send it to the Storage daemon
    */
   if (bsctx.signing_digest) {
      if (!terminate_signing_digest(bsctx)) {
         goto bail_out;
      }
   }

   /*
    * Terminate any digest and send it to Storage daemon
    */
   if (bsctx.digest) {
      if (!terminate_digest(bsctx)) {
         goto bail_out;
      }
   }

   /*
    * Check if original file has a digest, and send it
    */
   if (ff_pkt->type == FT_LNKSAVED && ff_pkt->digest) {
      Dmsg2(300, "Link %s digest %d\n", ff_pkt->fname, ff_pkt->digest_len);
      sd->fsend("%ld %d 0", jcr->JobFiles, ff_pkt->digest_stream);

      sd->msg = check_pool_memory_size(sd->msg, ff_pkt->digest_len);
      memcpy(sd->msg, ff_pkt->digest, ff_pkt->digest_len);
      sd->msglen = ff_pkt->digest_len;
      sd->send();

      sd->signal(BNET_EOD);              /* end of hardlink record */
   }

good_rtn:
   rtnstat = jcr->is_canceled() ? 0 : 1; /* good return if not canceled */

bail_out:
   if (jcr->is_incomplete() || jcr->is_canceled()) {
      rtnstat = 0;
   }
   if (plugin_started) {
      send_plugin_name(jcr, sd, false); /* signal end of plugin data */
   }
   if (ff_pkt->opt_plugin) {
      jcr->plugin_sp = NULL;            /* sp is local to this function */
      jcr->opt_plugin = false;
   }
   if (bsctx.digest) {
      crypto_digest_free(bsctx.digest);
   }
   if (bsctx.signing_digest) {
      crypto_digest_free(bsctx.signing_digest);
   }

   return rtnstat;
}

/**
 * Handle the data just read and send it to the SD after doing any postprocessing needed.
 */
static inline bool send_data_to_sd(b_ctx *bctx)
{
   BareosSocket *sd = bctx->jcr->store_bsock;
   bool need_more_data;

   /*
    * Check for sparse blocks
    */
   if (bit_is_set(FO_SPARSE, bctx->ff_pkt->flags)) {
      bool allZeros;
      ser_declare;

      allZeros = false;
      if ((sd->msglen == bctx->rsize &&
          (bctx->fileAddr + sd->msglen < (uint64_t)bctx->ff_pkt->statp.st_size)) ||
          ((bctx->ff_pkt->type == FT_RAW ||
            bctx->ff_pkt->type == FT_FIFO) &&
          ((uint64_t)bctx->ff_pkt->statp.st_size == 0))) {
         allZeros = is_buf_zero(bctx->rbuf, bctx->rsize);
      }

      if (!allZeros) {
         /*
          * Put file address as first data in buffer
          */
         ser_begin(bctx->wbuf, OFFSET_FADDR_SIZE);
         ser_uint64(bctx->fileAddr); /* store fileAddr in begin of buffer */
      }

      bctx->fileAddr += sd->msglen; /* update file address */

      /*
       * Skip block of all zeros
       */
      if (allZeros) {
         return true;
      }
   } else if (bit_is_set(FO_OFFSETS, bctx->ff_pkt->flags)) {
      ser_declare;
      ser_begin(bctx->wbuf, OFFSET_FADDR_SIZE);
      ser_uint64(bctx->ff_pkt->bfd.offset); /* store offset in begin of buffer */
   }

   bctx->jcr->ReadBytes += sd->msglen; /* count bytes read */

   /*
    * Uncompressed cipher input length
    */
   bctx->cipher_input_len = sd->msglen;

   /*
    * Update checksum if requested
    */
   if (bctx->digest) {
      crypto_digest_update(bctx->digest, (uint8_t *)bctx->rbuf, sd->msglen);
   }

   /*
    * Update signing digest if requested
    */
   if (bctx->signing_digest) {
      crypto_digest_update(bctx->signing_digest, (uint8_t *)bctx->rbuf, sd->msglen);
   }

   /*
    * Compress the data.
    */
   if (bit_is_set(FO_COMPRESS, bctx->ff_pkt->flags)) {
      if (!compress_data(bctx->jcr, bctx->ff_pkt->Compress_algo, bctx->rbuf,
                         bctx->jcr->store_bsock->msglen, bctx->cbuf,
                         bctx->max_compress_len, &bctx->compress_len)) {
         return false;
      }

      /*
       * See if we need to generate a compression header.
       */
      if (bctx->chead) {
         ser_declare;

         /*
          * Complete header
          */
         ser_begin(bctx->chead, sizeof(comp_stream_header));
         ser_uint32(bctx->ch.magic);
         ser_uint32(bctx->compress_len);
         ser_uint16(bctx->ch.level);
         ser_uint16(bctx->ch.version);
         ser_end(bctx->chead, sizeof(comp_stream_header));

         bctx->compress_len += sizeof(comp_stream_header); /* add size of header */
      }

      bctx->jcr->store_bsock->msglen = bctx->compress_len; /* set compressed length */
      bctx->cipher_input_len = bctx->compress_len;
   }

   /*
    * Encrypt the data.
    */
   need_more_data = false;
   if (bit_is_set(FO_ENCRYPT, bctx->ff_pkt->flags) && !encrypt_data(bctx, &need_more_data)) {
      if (need_more_data) {
         return true;
      }
      return false;
   }

   /*
    * Send the buffer to the Storage daemon
    */
   if (bit_is_set(FO_SPARSE, bctx->ff_pkt->flags) || bit_is_set(FO_OFFSETS, bctx->ff_pkt->flags)) {
      sd->msglen += OFFSET_FADDR_SIZE; /* include fileAddr in size */
   }
   sd->msg = bctx->wbuf; /* set correct write buffer */

   if (!sd->send()) {
      if (!bctx->jcr->is_job_canceled()) {
         Jmsg1(bctx->jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
      }
      return false;
   }

   Dmsg1(130, "Send data to SD len=%d\n", sd->msglen);
   bctx->jcr->JobBytes += sd->msglen; /* count bytes saved possibly compressed/encrypted */
   sd->msg = bctx->msgsave; /* restore read buffer */

   return true;
}

#ifdef HAVE_WIN32
/**
 * Callback method for ReadEncryptedFileRaw()
 */
static DWORD WINAPI send_efs_data(PBYTE pbData, PVOID pvCallbackContext, ULONG ulLength)
{
   b_ctx *bctx = (b_ctx *)pvCallbackContext;
   BareosSocket *sd = bctx->jcr->store_bsock;

   if (ulLength == 0) {
      return ERROR_SUCCESS;
   }

   /*
    * See if we can fit the data into the current bctx->rbuf which can hold bctx->rsize bytes.
    */
   if (ulLength <= (ULONG)bctx->rsize) {
      sd->msglen = ulLength;
      memcpy(bctx->rbuf, pbData, ulLength);
      if (!send_data_to_sd(bctx)) {
         return ERROR_NET_WRITE_FAULT;
      }
   } else {
      /*
       * Need to chunk the data into pieces.
       */
      ULONG offset = 0;

      while (ulLength > 0) {
         sd->msglen = MIN((ULONG)bctx->rsize, ulLength);
         memcpy(bctx->rbuf, pbData + offset, sd->msglen);
         if (!send_data_to_sd(bctx)) {
            return ERROR_NET_WRITE_FAULT;
         }

         offset += sd->msglen;
         ulLength -= sd->msglen;
      }
   }

   return ERROR_SUCCESS;
}

/**
 * Send the content of an Encrypted file on an EFS filesystem.
 */
static inline bool send_encrypted_data(b_ctx &bctx)
{
   bool retval = false;

   if (!p_ReadEncryptedFileRaw) {
      Jmsg0(bctx.jcr, M_FATAL, 0, _("Encrypted file but no EFS support functions\n"));
   }

   /*
    * The EFS read function, ReadEncryptedFileRaw(), works in a specific way.
    * You have to give it a function that it calls repeatedly every time the
    * read buffer is filled.
    *
    * So ReadEncryptedFileRaw() will not return until it has read the whole file.
    */
   if (p_ReadEncryptedFileRaw((PFE_EXPORT_FUNC)send_efs_data, &bctx, bctx.ff_pkt->bfd.pvContext)) {
      goto bail_out;
   }
   retval = true;

bail_out:
   return retval;
}
#endif

/**
 * Send the content of a file on anything but an EFS filesystem.
 */
static inline bool send_plain_data(b_ctx &bctx)
{
   bool retval = false;
   BareosSocket *sd = bctx.jcr->store_bsock;

   /*
    * Read the file data
    */
   while ((sd->msglen = (uint32_t)bread(&bctx.ff_pkt->bfd, bctx.rbuf, bctx.rsize)) > 0) {
      if (!send_data_to_sd(&bctx)) {
         goto bail_out;
      }
   }
   retval = true;

bail_out:
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
static int send_data(JobControlRecord *jcr, int stream, FindFilesPacket *ff_pkt,
                     DIGEST *digest, DIGEST *signing_digest)
{
   b_ctx bctx;
   BareosSocket *sd = jcr->store_bsock;
#ifdef FD_NO_SEND_TEST
   return 1;
#endif

   /*
    * Setup backup context.
    */
   memset(&bctx, 0, sizeof(b_ctx));
   bctx.jcr = jcr;
   bctx.ff_pkt = ff_pkt;
   bctx.msgsave = sd->msg; /* save the original sd buffer */
   bctx.rbuf = sd->msg; /* read buffer */
   bctx.wbuf = sd->msg; /* write buffer */
   bctx.rsize = jcr->buf_size; /* read buffer size */
   bctx.cipher_input = (uint8_t *)bctx.rbuf; /* encrypt uncompressed data */
   bctx.digest = digest; /* encryption digest */
   bctx.signing_digest = signing_digest; /* signing digest */

   Dmsg1(300, "Saving data, type=%d\n", ff_pkt->type);

   if (!setup_compression_context(bctx)) {
      goto bail_out;
   }

   if (!setup_encryption_context(bctx)) {
      goto bail_out;
   }

   /*
    * Send Data header to Storage daemon
    *    <file-index> <stream> <info>
    */
   if (!sd->fsend("%ld %d 0", jcr->JobFiles, stream)) {
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
      }
      goto bail_out;
   }
   Dmsg1(300, ">stored: datahdr %s", sd->msg);

   /*
    * Make space at beginning of buffer for fileAddr because this
    *   same buffer will be used for writing if compression is off.
    */
   if (bit_is_set(FO_SPARSE, ff_pkt->flags) || bit_is_set(FO_OFFSETS, ff_pkt->flags)) {
      bctx.rbuf += OFFSET_FADDR_SIZE;
      bctx.rsize -= OFFSET_FADDR_SIZE;
#ifdef HAVE_FREEBSD_OS
      /*
       * To read FreeBSD partitions, the read size must be a multiple of 512.
       */
      bctx.rsize = (bctx.rsize / 512) * 512;
#endif
   }

   /*
    * A RAW device read on win32 only works if the buffer is a multiple of 512
    */
#ifdef HAVE_WIN32
   if (S_ISBLK(ff_pkt->statp.st_mode)) {
      bctx.rsize = (bctx.rsize / 512) * 512;
   }

   if (ff_pkt->statp.st_rdev & FILE_ATTRIBUTE_ENCRYPTED) {
      if (!send_encrypted_data(bctx)) {
         goto bail_out;
      }
   } else {
      if (!send_plain_data(bctx)) {
         goto bail_out;
      }
   }
#else
   if (!send_plain_data(bctx)) {
      goto bail_out;
   }
#endif

   if (sd->msglen < 0) { /* error */
      berrno be;
      Jmsg(jcr, M_ERROR, 0, _("Read error on file %s. ERR=%s\n"), ff_pkt->fname, be.bstrerror(ff_pkt->bfd.berrno));
      if (jcr->JobErrors++ > 1000) { /* insanity check */
         Jmsg(jcr, M_FATAL, 0, _("Too many errors. JobErrors=%d.\n"), jcr->JobErrors);
      }
   } else if (bit_is_set(FO_ENCRYPT, ff_pkt->flags)) {
      /*
       * For encryption, we must call finalize to push out any buffered data.
       */
      if (!crypto_cipher_finalize(bctx.cipher_ctx,
                                  (uint8_t *)jcr->crypto.crypto_buf,
                                  &bctx.encrypted_len)) {
         /*
          * Padding failed. Shouldn't happen.
          */
         Jmsg(jcr, M_FATAL, 0, _("Encryption padding error\n"));
         goto bail_out;
      }

      /*
       * Note, on SSL pre-0.9.7, there is always some output
       */
      if (bctx.encrypted_len > 0) {
         sd->msglen = bctx.encrypted_len; /* set encrypted length */
         sd->msg = jcr->crypto.crypto_buf; /* set correct write buffer */
         if (!sd->send()) {
            if (!jcr->is_job_canceled()) {
               Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
            }
            goto bail_out;
         }
         Dmsg1(130, "Send data to SD len=%d\n", sd->msglen);
         jcr->JobBytes += sd->msglen; /* count bytes saved possibly compressed/encrypted */
         sd->msg = bctx.msgsave; /* restore bnet buffer */
      }
   }

   if (!sd->signal(BNET_EOD)) { /* indicate end of file data */
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
      }
      goto bail_out;
   }

   /*
    * Free the cipher context
    */
   if (bctx.cipher_ctx) {
      crypto_cipher_free(bctx.cipher_ctx);
   }

   return 1;

bail_out:
   /*
    * Free the cipher context
    */
   if (bctx.cipher_ctx) {
      crypto_cipher_free(bctx.cipher_ctx);
   }

   sd->msg = bctx.msgsave; /* restore bnet buffer */
   sd->msglen = 0;

   return 0;
}

bool encode_and_send_attributes(JobControlRecord *jcr, FindFilesPacket *ff_pkt, int &data_stream)
{
   BareosSocket *sd = jcr->store_bsock;
   PoolMem attribs(PM_NAME),
            attribsExBuf(PM_NAME);
   char *attribsEx = NULL;
   int attr_stream;
   int comp_len;
   bool status;
   int hangup = get_hangup();
#ifdef FD_NO_SEND_TEST
   return true;
#endif

   Dmsg1(300, "encode_and_send_attrs fname=%s\n", ff_pkt->fname);
   /** Find what data stream we will use, then encode the attributes */
   if ((data_stream = select_data_stream(ff_pkt, me->compatible)) == STREAM_NONE) {
      /* This should not happen */
      Jmsg0(jcr, M_FATAL, 0, _("Invalid file flags, no supported data stream type.\n"));
      return false;
   }
   encode_stat(attribs.c_str(), &ff_pkt->statp, sizeof(ff_pkt->statp), ff_pkt->LinkFI, data_stream);

   /** Now possibly extend the attributes */
   if (IS_FT_OBJECT(ff_pkt->type)) {
      attr_stream = STREAM_RESTORE_OBJECT;
   } else {
      attribsEx = attribsExBuf.c_str();
      attr_stream = encode_attribsEx(jcr, attribsEx, ff_pkt);
   }

   Dmsg3(300, "File %s\nattribs=%s\nattribsEx=%s\n", ff_pkt->fname, attribs.c_str(), attribsEx);

   jcr->lock();
   jcr->JobFiles++;                    /* increment number of files sent */
   ff_pkt->FileIndex = jcr->JobFiles;  /* return FileIndex */
   pm_strcpy(jcr->last_fname, ff_pkt->fname);
   jcr->unlock();

   /*
    * Debug code: check if we must hangup
    */
   if (hangup && (jcr->JobFiles > (uint32_t)hangup)) {
      jcr->setJobStatus(JS_Incomplete);
      Jmsg1(jcr, M_FATAL, 0, "Debug hangup requested after %d files.\n", hangup);
      set_hangup(0);
      return false;
   }

   /**
    * Send Attributes header to Storage daemon
    *    <file-index> <stream> <info>
    */
   if (!sd->fsend("%ld %d 0", jcr->JobFiles, attr_stream)) {
      if (!jcr->is_canceled() && !jcr->is_incomplete()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
      }
      return false;
   }
   Dmsg1(300, ">stored: attrhdr %s", sd->msg);

   /**
    * Send file attributes to Storage daemon
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
    * slash. For a linked file, link is the link.
    */
   if (!IS_FT_OBJECT(ff_pkt->type) && ff_pkt->type != FT_DELETED) { /* already stripped */
      strip_path(ff_pkt);
   }
   switch (ff_pkt->type) {
   case FT_JUNCTION:
   case FT_LNK:
   case FT_LNKSAVED:
      Dmsg3(300, "Link %d %s to %s\n", jcr->JobFiles, ff_pkt->fname, ff_pkt->link);
      status = sd->fsend("%ld %d %s%c%s%c%s%c%s%c%u%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->fname, 0, attribs.c_str(), 0,
                         ff_pkt->link, 0, attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   case FT_DIREND:
   case FT_REPARSE:
      /* Here link is the canonical filename (i.e. with trailing slash) */
      status = sd->fsend("%ld %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->link, 0, attribs.c_str(), 0, 0,
                         attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   case FT_PLUGIN_CONFIG:
   case FT_RESTORE_FIRST:
      comp_len = ff_pkt->object_len;
      ff_pkt->object_compression = 0;

      if (ff_pkt->object_len > 1000) {
         /*
          * Big object, compress it
          */
         comp_len = compressBound(ff_pkt->object_len);
         POOLMEM *comp_obj = get_memory(comp_len);
         /*
          * FIXME: check Zdeflate error
          */
         Zdeflate(ff_pkt->object, ff_pkt->object_len, comp_obj, comp_len);
         if (comp_len < ff_pkt->object_len) {
            ff_pkt->object = comp_obj;
            ff_pkt->object_compression = 1;    /* zlib level 9 compression */
         } else {
            /*
             * Uncompressed object smaller, use it
             */
            comp_len = ff_pkt->object_len;
         }
         Dmsg2(100, "Object compressed from %d to %d bytes\n", ff_pkt->object_len, comp_len);
      }

      sd->msglen = Mmsg(sd->msg, "%d %d %d %d %d %d %s%c%s%c",
                        jcr->JobFiles, ff_pkt->type, ff_pkt->object_index,
                        comp_len, ff_pkt->object_len, ff_pkt->object_compression,
                        ff_pkt->fname, 0, ff_pkt->object_name, 0);
      sd->msg = check_pool_memory_size(sd->msg, sd->msglen + comp_len + 2);
      memcpy(sd->msg + sd->msglen, ff_pkt->object, comp_len);

      /*
       * Note we send one extra byte so Dir can store zero after object
       */
      sd->msglen += comp_len + 1;
      status = sd->send();
      if (ff_pkt->object_compression) {
         free_and_null_pool_memory(ff_pkt->object);
      }
      break;
   case FT_REG:
      status = sd->fsend("%ld %d %s%c%s%c%c%s%c%d%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->fname, 0, attribs.c_str(), 0, 0,
                         attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   default:
      status = sd->fsend("%ld %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                         ff_pkt->type, ff_pkt->fname, 0, attribs.c_str(), 0, 0,
                         attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   }

   if (!IS_FT_OBJECT(ff_pkt->type) && ff_pkt->type != FT_DELETED) {
      unstrip_path(ff_pkt);
   }

   Dmsg2(300, ">stored: attr len=%d: %s\n", sd->msglen, sd->msg);
   if (!status && !jcr->is_job_canceled()) {
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
   }

   sd->signal(BNET_EOD);            /* indicate end of attributes data */

   return status;
}

/**
 * Do in place strip of path
 */
static bool do_strip(int count, char *in)
{
   char *out = in;
   int stripped;
   int numsep = 0;

   /**
    * Copy to first path separator -- Win32 might have c: ...
    */
   while (*in && !IsPathSeparator(*in)) {
      out++; in++;
   }
   if (*in) {                    /* Not at the end of the string */
      out++; in++;
      numsep++;                  /* one separator seen */
   }
   for (stripped=0; stripped<count && *in; stripped++) {
      while (*in && !IsPathSeparator(*in)) {
         in++;                   /* skip chars */
      }
      if (*in) {
         numsep++;               /* count separators seen */
         in++;                   /* skip separator */
      }
   }

   /*
    * Copy to end
    */
   while (*in) {                /* copy to end */
      if (IsPathSeparator(*in)) {
         numsep++;
      }
      *out++ = *in++;
   }
   *out = 0;
   Dmsg4(500, "stripped=%d count=%d numsep=%d sep>count=%d\n",
         stripped, count, numsep, numsep>count);
   return stripped==count && numsep>count;
}

/**
 * If requested strip leading components of the path so that we can
 * save file as if it came from a subdirectory.  This is most useful
 * for dealing with snapshots, by removing the snapshot directory, or
 * in handling vendor migrations where files have been restored with
 * a vendor product into a subdirectory.
 */
void strip_path(FindFilesPacket *ff_pkt)
{
   if (!bit_is_set(FO_STRIPPATH, ff_pkt->flags) || ff_pkt->strip_path <= 0) {
      Dmsg1(200, "No strip for %s\n", ff_pkt->fname);
      return;
   }

   if (!ff_pkt->fname_save) {
     ff_pkt->fname_save = get_pool_memory(PM_FNAME);
     ff_pkt->link_save = get_pool_memory(PM_FNAME);
   }

   pm_strcpy(ff_pkt->fname_save, ff_pkt->fname);
   if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link) {
      pm_strcpy(ff_pkt->link_save, ff_pkt->link);
      Dmsg2(500, "strcpy link_save=%d link=%d\n", strlen(ff_pkt->link_save),
         strlen(ff_pkt->link));
      Dsm_check(200);
   }

   /**
    * Strip path. If it doesn't succeed put it back. If it does, and there
    * is a different link string, attempt to strip the link. If it fails,
    * back them both back. Do not strip symlinks. I.e. if either stripping
    * fails don't strip anything.
    */
   if (!do_strip(ff_pkt->strip_path, ff_pkt->fname)) {
      unstrip_path(ff_pkt);
      goto rtn;
   }

   /**
    * Strip links but not symlinks
    */
   if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link) {
      if (!do_strip(ff_pkt->strip_path, ff_pkt->link)) {
         unstrip_path(ff_pkt);
      }
   }

rtn:
   Dmsg3(100, "fname=%s stripped=%s link=%s\n", ff_pkt->fname_save, ff_pkt->fname, ff_pkt->link);
}

void unstrip_path(FindFilesPacket *ff_pkt)
{
   if (!bit_is_set(FO_STRIPPATH, ff_pkt->flags) || ff_pkt->strip_path <= 0) {
      return;
   }

   strcpy(ff_pkt->fname, ff_pkt->fname_save);
   if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link) {
      Dmsg2(500, "strcpy link=%s link_save=%s\n", ff_pkt->link,
          ff_pkt->link_save);
      strcpy(ff_pkt->link, ff_pkt->link_save);
      Dmsg2(500, "strcpy link=%d link_save=%d\n", strlen(ff_pkt->link),
          strlen(ff_pkt->link_save));
      Dsm_check(200);
   }
}

static void close_vss_backup_session(JobControlRecord *jcr)
{
#if defined(WIN32_VSS)
   /*
    * STOP VSS ON WIN32
    * Tell vss to close the backup session
    */
   if (jcr->pVSSClient) {
      /*
       * We are about to call the BackupComplete VSS method so let all plugins know
       * that by raising the bEventVssBackupComplete event.
       */
      generate_plugin_event(jcr, bEventVssBackupComplete);
      if (jcr->pVSSClient->CloseBackup()) {
         /*
          * Inform user about writer states
          */
         for (int i = 0; i < (int)jcr->pVSSClient->GetWriterCount(); i++) {
            int msg_type = M_INFO;
            if (jcr->pVSSClient->GetWriterState(i) < 1) {
               msg_type = M_WARNING;
               jcr->JobErrors++;
            }
            Jmsg(jcr, msg_type, 0, _("VSS Writer (BackupComplete): %s\n"), jcr->pVSSClient->GetWriterInfo(i));
         }
      }

      /*
       * Generate Job global writer metadata
       */
      wchar_t *metadata = jcr->pVSSClient->GetMetadata();
      if (metadata) {
         FindFilesPacket *ff_pkt = jcr->ff;
         ff_pkt->fname = (char *)"*all*"; /* for all plugins */
         ff_pkt->type = FT_RESTORE_FIRST;
         ff_pkt->LinkFI = 0;
         ff_pkt->object_name = (char *)"job_metadata.xml";
         ff_pkt->object = BSTR_2_str(metadata);
         ff_pkt->object_len = (wcslen(metadata) + 1) * sizeof(wchar_t);
         ff_pkt->object_index = (int)time(NULL);
         save_file(jcr, ff_pkt, true);
     }
   }
#endif
}
