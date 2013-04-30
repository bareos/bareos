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
/**
 *  Bacula File Daemon  backup.c  send file attributes and data
 *   to the Storage daemon.
 *
 *    Kern Sibbald, March MM
 *
 */

#include "bacula.h"
#include "filed.h"
#include "ch.h"

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

/* Forward referenced functions */
int save_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level);
static int send_data(JCR *jcr, int stream, FF_PKT *ff_pkt, DIGEST *digest, DIGEST *signature_digest);
bool encode_and_send_attributes(JCR *jcr, FF_PKT *ff_pkt, int &data_stream);
static bool crypto_session_start(JCR *jcr);
static void crypto_session_end(JCR *jcr);
static bool crypto_session_send(JCR *jcr, BSOCK *sd);
static void close_vss_backup_session(JCR *jcr);

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
 *
 */
bool blast_data_to_storage_daemon(JCR *jcr, char *addr)
{
   BSOCK *sd;
   bool ok = true;
   // TODO landonf: Allow user to specify encryption algorithm

   sd = jcr->store_bsock;

   jcr->setJobStatus(JS_Running);

   Dmsg1(300, "bfiled: opened data connection %d to stored\n", sd->m_fd);

   LockRes();
   CLIENT *client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
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
   /**
    * Adjust for compression so that output buffer is
    *  12 bytes + 0.1% larger than input buffer plus 18 bytes.
    *  This gives a bit extra plus room for the sparse addr if any.
    *  Note, we adjust the read size to be smaller so that the
    *  same output buffer can be used without growing it.
    *
    *  For LZO1X compression the recommended value is :
    *                  output_block_size = input_block_size + (input_block_size / 16) + 64 + 3 + sizeof(comp_stream_header)
    *
    * The zlib compression workset is initialized here to minimize
    *  the "per file" load. The jcr member is only set, if the init 
    *  was successful.
    *
    *  For the same reason, lzo compression is initialized here.
    */
#ifdef HAVE_LZO
   jcr->compress_buf_size = MAX(jcr->buf_size + (jcr->buf_size / 16) + 67 + (int)sizeof(comp_stream_header), jcr->buf_size + ((jcr->buf_size+999) / 1000) + 30);
   jcr->compress_buf = get_memory(jcr->compress_buf_size);
#else
   jcr->compress_buf_size = jcr->buf_size + ((jcr->buf_size+999) / 1000) + 30;
   jcr->compress_buf = get_memory(jcr->compress_buf_size);
#endif
   
#ifdef HAVE_LIBZ
   z_stream *pZlibStream = (z_stream*)malloc(sizeof(z_stream));  
   if (pZlibStream) {
      pZlibStream->zalloc = Z_NULL;      
      pZlibStream->zfree = Z_NULL;
      pZlibStream->opaque = Z_NULL;
      pZlibStream->state = Z_NULL;

      if (deflateInit(pZlibStream, Z_DEFAULT_COMPRESSION) == Z_OK) {
         jcr->pZLIB_compress_workset = pZlibStream;
      } else {
         free (pZlibStream);
      }
   }
#endif

#ifdef HAVE_LZO
   lzo_voidp pLzoMem = (lzo_voidp) malloc(LZO1X_1_MEM_COMPRESS);
   if (pLzoMem) {
      if (lzo_init() == LZO_E_OK) {
         jcr->LZO_compress_workset = pLzoMem;
      } else {
         free (pLzoMem);
      }
   }
#endif

   if (!crypto_session_start(jcr)) {
      return false;
   }

   set_find_options((FF_PKT *)jcr->ff, jcr->incremental, jcr->mtime);

   /** in accurate mode, we overload the find_one check function */
   if (jcr->accurate) {
      set_find_changed_function((FF_PKT *)jcr->ff, accurate_check_file);
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

   /** Subroutine save_file() is called for each file */
   if (!find_files(jcr, (FF_PKT *)jcr->ff, save_file, plugin_save)) {
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
   if (jcr->compress_buf) {
      free_pool_memory(jcr->compress_buf);
      jcr->compress_buf = NULL;
   }
   if (jcr->pZLIB_compress_workset) {
      /* Free the zlib stream */
#ifdef HAVE_LIBZ
      deflateEnd((z_stream *)jcr->pZLIB_compress_workset);
#endif
      free (jcr->pZLIB_compress_workset);
      jcr->pZLIB_compress_workset = NULL;
   }
   if (jcr->LZO_compress_workset) {
      free (jcr->LZO_compress_workset);
      jcr->LZO_compress_workset = NULL;
   }

   crypto_session_end(jcr);


   Dmsg1(100, "end blast_data ok=%d\n", ok);
   return ok;
}

static bool crypto_session_start(JCR *jcr)
{
   crypto_cipher_t cipher = CRYPTO_CIPHER_AES_128_CBC;

   /**
    * Create encryption session data and a cached, DER-encoded session data
    * structure. We use a single session key for each backup, so we'll encode
    * the session data only once.
    */
   if (jcr->crypto.pki_encrypt) {
      uint32_t size = 0;

      /** Create per-job session encryption context */
      jcr->crypto.pki_session = crypto_session_new(cipher, jcr->crypto.pki_recipients);

      /** Get the session data size */
      if (!crypto_session_encode(jcr->crypto.pki_session, (uint8_t *)0, &size)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred while encrypting the stream.\n"));
         return false;
      }

      /** Allocate buffer */
      jcr->crypto.pki_session_encoded = get_memory(size);

      /** Encode session data */
      if (!crypto_session_encode(jcr->crypto.pki_session, (uint8_t *)jcr->crypto.pki_session_encoded, &size)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred while encrypting the stream.\n"));
         return false;
      }

      /** ... and store the encoded size */
      jcr->crypto.pki_session_encoded_size = size;

      /** Allocate the encryption/decryption buffer */
      jcr->crypto.crypto_buf = get_memory(CRYPTO_CIPHER_MAX_BLOCK_SIZE);
   }
   return true;
}

static void crypto_session_end(JCR *jcr)
{
   if (jcr->crypto.crypto_buf) {
      free_pool_memory(jcr->crypto.crypto_buf);
      jcr->crypto.crypto_buf = NULL;
   }
   if (jcr->crypto.pki_session) {
      crypto_session_free(jcr->crypto.pki_session);
   }
   if (jcr->crypto.pki_session_encoded) {
      free_pool_memory(jcr->crypto.pki_session_encoded);
      jcr->crypto.pki_session_encoded = NULL;
   }
}

static bool crypto_session_send(JCR *jcr, BSOCK *sd)
{
   POOLMEM *msgsave;

   /** Send our header */
   Dmsg2(100, "Send hdr fi=%ld stream=%d\n", jcr->JobFiles, STREAM_ENCRYPTED_SESSION_DATA);
   sd->fsend("%ld %d 0", jcr->JobFiles, STREAM_ENCRYPTED_SESSION_DATA);

   msgsave = sd->msg;
   sd->msg = jcr->crypto.pki_session_encoded;
   sd->msglen = jcr->crypto.pki_session_encoded_size;
   jcr->JobBytes += sd->msglen;

   Dmsg1(100, "Send data len=%d\n", sd->msglen);
   sd->send();
   sd->msg = msgsave;
   sd->signal(BNET_EOD);
   return true;
}


/**
 * Called here by find() for each file included.
 *   This is a callback. The original is find_files() above.
 *
 *  Send the file and its data to the Storage daemon.
 *
 *  Returns: 1 if OK
 *           0 if error
 *          -1 to ignore file/directory (not used here)
 */
int save_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level)
{
   bool do_read = false;
   bool plugin_started = false;
   bool do_plugin_set = false;
   int stat, data_stream; 
   int rtnstat = 0;
   DIGEST *digest = NULL;
   DIGEST *signing_digest = NULL;
   int digest_stream = STREAM_NONE;
   SIGNATURE *sig = NULL;
   bool has_file_data = false;
   struct save_pkt sp;          /* use by option plugin */
   // TODO landonf: Allow the user to specify the digest algorithm
#ifdef HAVE_SHA2
   crypto_digest_t signing_algorithm = CRYPTO_DIGEST_SHA256;
#else
   crypto_digest_t signing_algorithm = CRYPTO_DIGEST_SHA1;
#endif
   BSOCK *sd = jcr->store_bsock;

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
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not access \"%s\": ERR=%s\n"), ff_pkt->fname,
         be.bstrerror(ff_pkt->ff_errno));
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
      Jmsg(jcr, M_NOTSAVED, 0, _("     Could not stat \"%s\": ERR=%s\n"), ff_pkt->fname,
         be.bstrerror(ff_pkt->ff_errno));
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

   Dmsg1(130, "bfiled: sending %s to stored\n", ff_pkt->fname);

   /** Digests and encryption are only useful if there's file data */
   if (has_file_data) {
      /**
       * Setup for digest handling. If this fails, the digest will be set to NULL
       * and not used. Note, the digest (file hash) can be any one of the four
       * algorithms below.
       *
       * The signing digest is a single algorithm depending on
       * whether or not we have SHA2.              
       *   ****FIXME****  the signing algoritm should really be
       *   determined a different way!!!!!!  What happens if
       *   sha2 was available during backup but not restore?
       */
      if (ff_pkt->flags & FO_MD5) {
         digest = crypto_digest_new(jcr, CRYPTO_DIGEST_MD5);
         digest_stream = STREAM_MD5_DIGEST;

      } else if (ff_pkt->flags & FO_SHA1) {
         digest = crypto_digest_new(jcr, CRYPTO_DIGEST_SHA1);
         digest_stream = STREAM_SHA1_DIGEST;

      } else if (ff_pkt->flags & FO_SHA256) {
         digest = crypto_digest_new(jcr, CRYPTO_DIGEST_SHA256);
         digest_stream = STREAM_SHA256_DIGEST;

      } else if (ff_pkt->flags & FO_SHA512) {
         digest = crypto_digest_new(jcr, CRYPTO_DIGEST_SHA512);
         digest_stream = STREAM_SHA512_DIGEST;
      }

      /** Did digest initialization fail? */
      if (digest_stream != STREAM_NONE && digest == NULL) {
         Jmsg(jcr, M_WARNING, 0, _("%s digest initialization failed\n"),
            stream_to_ascii(digest_stream));
      }

      /**
       * Set up signature digest handling. If this fails, the signature digest
       * will be set to NULL and not used.
       */
      /* TODO landonf: We should really only calculate the digest once, for
       * both verification and signing.
       */
      if (jcr->crypto.pki_sign) {
         signing_digest = crypto_digest_new(jcr, signing_algorithm);

         /** Full-stop if a failure occurred initializing the signature digest */
         if (signing_digest == NULL) {
            Jmsg(jcr, M_NOTSAVED, 0, _("%s signature digest initialization failed\n"),
               stream_to_ascii(signing_algorithm));
            jcr->JobErrors++;
            goto good_rtn;
         }
      }

      /** Enable encryption */
      if (jcr->crypto.pki_encrypt) {
         ff_pkt->flags |= FO_ENCRYPT;
      }
   }

   /** Initialize the file descriptor we use for data and other streams. */
   binit(&ff_pkt->bfd);
   if (ff_pkt->flags & FO_PORTABLE) {
      set_portable_backup(&ff_pkt->bfd); /* disable Win32 BackupRead() */
   }

   if (ff_pkt->cmd_plugin) {
      do_plugin_set = true;

   /* option and cmd plugin are not compatible together */
   } else if (ff_pkt->opt_plugin) {
      
      /* ask the option plugin what to do with this file */
      switch (plugin_option_handle_file(jcr, ff_pkt, &sp)) {
      case bRC_OK:
         Dmsg2(10, "Option plugin %s will be used to backup %s\n",
               ff_pkt->plugin, ff_pkt->fname);
         do_plugin_set = true;
         break;
      case bRC_Skip:
         Dmsg2(10, "Option plugin %s decided to skip %s\n",
               ff_pkt->plugin, ff_pkt->fname);
         goto good_rtn;
      default:
         Dmsg2(10, "Option plugin %s decided to let bacula handle %s\n",
               ff_pkt->plugin, ff_pkt->fname);
         break;
      }
   }

   if (do_plugin_set) {
      /* Tell bfile that it needs to call plugin */
      if (!set_cmd_plugin(&ff_pkt->bfd, jcr)) {
         goto bail_out;
      }
      send_plugin_name(jcr, sd, true);      /* signal start of plugin data */
      plugin_started = true;
   }

   /** Send attributes -- must be done after binit() */
   if (!encode_and_send_attributes(jcr, ff_pkt, data_stream)) {
      goto bail_out;
   }
   /** Meta data only for restore object */
   if (IS_FT_OBJECT(ff_pkt->type)) {
      goto good_rtn;
   }
   /** Meta data only for deleted files */
   if (ff_pkt->type == FT_DELETED) {
      goto good_rtn;
   }
   /** Set up the encryption context and send the session data to the SD */
   if (has_file_data && jcr->crypto.pki_encrypt) {
      if (!crypto_session_send(jcr, sd)) {
         goto bail_out;
      }
   }

   /**
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
   } else if (ff_pkt->type == FT_RAW || ff_pkt->type == FT_FIFO ||
              ff_pkt->type == FT_REPARSE || ff_pkt->type == FT_JUNCTION ||
         (!is_portable_backup(&ff_pkt->bfd) && ff_pkt->type == FT_DIREND)) {
      do_read = true;
   }

   if (ff_pkt->cmd_plugin && !ff_pkt->no_read) {
      do_read = true;
   }

   Dmsg2(150, "type=%d do_read=%d\n", ff_pkt->type, do_read);
   if (do_read) {
      btimer_t *tid;

      if (ff_pkt->type == FT_FIFO) {
         tid = start_thread_timer(jcr, pthread_self(), 60);
      } else {
         tid = NULL;
      }
      int noatime = ff_pkt->flags & FO_NOATIME ? O_NOATIME : 0;
      ff_pkt->bfd.reparse_point = (ff_pkt->type == FT_REPARSE || 
                                   ff_pkt->type == FT_JUNCTION);
      if (bopen(&ff_pkt->bfd, ff_pkt->fname, O_RDONLY | O_BINARY | noatime, 0) < 0) {
         ff_pkt->ff_errno = errno;
         berrno be;
         Jmsg(jcr, M_NOTSAVED, 0, _("     Cannot open \"%s\": ERR=%s.\n"), ff_pkt->fname,
              be.bstrerror());
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

      stat = send_data(jcr, data_stream, ff_pkt, digest, signing_digest);

      if (ff_pkt->flags & FO_CHKCHANGES) {
         has_file_changed(jcr, ff_pkt);
      }

      bclose(&ff_pkt->bfd);
      
      if (!stat) {
         goto bail_out;
      }
   }

   if (have_darwin_os) {
      /** Regular files can have resource forks and Finder Info */
      if (ff_pkt->type != FT_LNKSAVED && (S_ISREG(ff_pkt->statp.st_mode) &&
          ff_pkt->flags & FO_HFSPLUS)) {
         if (ff_pkt->hfsinfo.rsrclength > 0) {
            int flags;
            int rsrc_stream;
            if (!bopen_rsrc(&ff_pkt->bfd, ff_pkt->fname, O_RDONLY | O_BINARY, 0) < 0) {
               ff_pkt->ff_errno = errno;
               berrno be;
               Jmsg(jcr, M_NOTSAVED, -1, _("     Cannot open resource fork for \"%s\": ERR=%s.\n"),
                    ff_pkt->fname, be.bstrerror());
               jcr->JobErrors++;
               if (is_bopen(&ff_pkt->bfd)) {
                  bclose(&ff_pkt->bfd);
               }
               goto good_rtn;
            }
            flags = ff_pkt->flags;
            ff_pkt->flags &= ~(FO_COMPRESS|FO_SPARSE|FO_OFFSETS);
            if (flags & FO_ENCRYPT) {
               rsrc_stream = STREAM_ENCRYPTED_MACOS_FORK_DATA;
            } else {
               rsrc_stream = STREAM_MACOS_FORK_DATA;
            }
            stat = send_data(jcr, rsrc_stream, ff_pkt, digest, signing_digest);
            ff_pkt->flags = flags;
            bclose(&ff_pkt->bfd);
            if (!stat) {
               goto bail_out;
            }
         }

         Dmsg1(300, "Saving Finder Info for \"%s\"\n", ff_pkt->fname);
         sd->fsend("%ld %d 0", jcr->JobFiles, STREAM_HFSPLUS_ATTRIBUTES);
         Dmsg1(300, "bfiled>stored:header %s\n", sd->msg);
         pm_memcpy(sd->msg, ff_pkt->hfsinfo.fndrinfo, 32);
         sd->msglen = 32;
         if (digest) {
            crypto_digest_update(digest, (uint8_t *)sd->msg, sd->msglen);
         }
         if (signing_digest) {
            crypto_digest_update(signing_digest, (uint8_t *)sd->msg, sd->msglen);
         }
         sd->send();
         sd->signal(BNET_EOD);
      }
   }

   /**
    * Save ACLs when requested and available for anything not being a symlink
    * and not being a plugin.
    */
   if (have_acl) {
      if (ff_pkt->flags & FO_ACL && ff_pkt->type != FT_LNK && !ff_pkt->cmd_plugin) {
         switch (build_acl_streams(jcr, ff_pkt)) {
         case bacl_exit_fatal:
            goto bail_out;
         case bacl_exit_error:
            /**
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
      }
   }

   /**
    * Save Extended Attributes when requested and available for all files not
    * being a plugin.
    */
   if (have_xattr) {
      if (ff_pkt->flags & FO_XATTR && !ff_pkt->cmd_plugin) {
         switch (build_xattr_streams(jcr, ff_pkt)) {
         case bxattr_exit_fatal:
            goto bail_out;
         case bxattr_exit_error:
            /**
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
      }
   }

   /** Terminate the signing digest and send it to the Storage daemon */
   if (signing_digest) {
      uint32_t size = 0;

      if ((sig = crypto_sign_new(jcr)) == NULL) {
         Jmsg(jcr, M_FATAL, 0, _("Failed to allocate memory for crypto signature.\n"));
         goto bail_out;
      }

      if (!crypto_sign_add_signer(sig, signing_digest, jcr->crypto.pki_keypair)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred while signing the stream.\n"));
         goto bail_out;
      }

      /** Get signature size */
      if (!crypto_sign_encode(sig, NULL, &size)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred while signing the stream.\n"));
         goto bail_out;
      }

      /** Grow the bsock buffer to fit our message if necessary */
      if (sizeof_pool_memory(sd->msg) < (int32_t)size) {
         sd->msg = realloc_pool_memory(sd->msg, size);
      }

      /** Send our header */
      sd->fsend("%ld %ld 0", jcr->JobFiles, STREAM_SIGNED_DIGEST);
      Dmsg1(300, "bfiled>stored:header %s\n", sd->msg);

      /** Encode signature data */
      if (!crypto_sign_encode(sig, (uint8_t *)sd->msg, &size)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred while signing the stream.\n"));
         goto bail_out;
      }

      sd->msglen = size;
      sd->send();
      sd->signal(BNET_EOD);              /* end of checksum */
   }

   /** Terminate any digest and send it to Storage daemon */
   if (digest) {
      uint32_t size;

      sd->fsend("%ld %d 0", jcr->JobFiles, digest_stream);
      Dmsg1(300, "bfiled>stored:header %s\n", sd->msg);

      size = CRYPTO_DIGEST_MAX_SIZE;

      /** Grow the bsock buffer to fit our message if necessary */
      if (sizeof_pool_memory(sd->msg) < (int32_t)size) {
         sd->msg = realloc_pool_memory(sd->msg, size);
      }

      if (!crypto_digest_finalize(digest, (uint8_t *)sd->msg, &size)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred finalizing signing the stream.\n"));
         goto bail_out;
      }

      /* Keep the checksum if this file is a hardlink */
      if (ff_pkt->linked) {
         ff_pkt_set_link_digest(ff_pkt, digest_stream, sd->msg, size);
      }

      sd->msglen = size;
      sd->send();
      sd->signal(BNET_EOD);              /* end of checksum */
   }

   /* Check if original file has a digest, and send it */
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
      jcr->plugin_sp = NULL;    /* sp is local to this function */
      jcr->plugin_ctx = NULL;
      jcr->plugin = NULL;
      jcr->opt_plugin = false;
   }
   if (digest) {
      crypto_digest_free(digest);
   }
   if (signing_digest) {
      crypto_digest_free(signing_digest);
   }
   if (sig) {
      crypto_sign_free(sig);        
   }
   return rtnstat;
}

/**
 * Send data read from an already open file descriptor.
 *
 * We return 1 on sucess and 0 on errors.
 *
 * ***FIXME***
 * We use ff_pkt->statp.st_size when FO_SPARSE to know when to stop
 *  reading.
 * Currently this is not a problem as the only other stream, resource forks,
 * are not handled as sparse files.
 */
static int send_data(JCR *jcr, int stream, FF_PKT *ff_pkt, DIGEST *digest, 
                     DIGEST *signing_digest)
{
   BSOCK *sd = jcr->store_bsock;
   uint64_t fileAddr = 0;             /* file address */
   char *rbuf, *wbuf;
   int32_t rsize = jcr->buf_size;      /* read buffer size */
   POOLMEM *msgsave;
   CIPHER_CONTEXT *cipher_ctx = NULL; /* Quell bogus uninitialized warnings */
   const uint8_t *cipher_input;
   uint32_t cipher_input_len;
   uint32_t cipher_block_size;
   uint32_t encrypted_len;
#ifdef FD_NO_SEND_TEST
   return 1;
#endif

   msgsave = sd->msg;
   rbuf = sd->msg;                    /* read buffer */
   wbuf = sd->msg;                    /* write buffer */
   cipher_input = (uint8_t *)rbuf;    /* encrypt uncompressed data */

   Dmsg1(300, "Saving data, type=%d\n", ff_pkt->type);

#if defined(HAVE_LIBZ) || defined(HAVE_LZO)
   uLong compress_len = 0;
   uLong max_compress_len = 0;
   const Bytef *cbuf = NULL;
 #ifdef HAVE_LIBZ
   int zstat;

   if ((ff_pkt->flags & FO_COMPRESS) && ff_pkt->Compress_algo == COMPRESS_GZIP) {
      if ((ff_pkt->flags & FO_SPARSE) || (ff_pkt->flags & FO_OFFSETS)) {
         cbuf = (Bytef *)jcr->compress_buf + OFFSET_FADDR_SIZE;
         max_compress_len = jcr->compress_buf_size - OFFSET_FADDR_SIZE;
      } else {
         cbuf = (Bytef *)jcr->compress_buf;
         max_compress_len = jcr->compress_buf_size; /* set max length */
      }
      wbuf = jcr->compress_buf;    /* compressed output here */
      cipher_input = (uint8_t *)jcr->compress_buf; /* encrypt compressed data */

      /**
       * Only change zlib parameters if there is no pending operation.
       * This should never happen as deflatereset is called after each
       * deflate.
       */

      if (((z_stream*)jcr->pZLIB_compress_workset)->total_in == 0) {
         /** set gzip compression level - must be done per file */
         if ((zstat=deflateParams((z_stream*)jcr->pZLIB_compress_workset, 
              ff_pkt->Compress_level, Z_DEFAULT_STRATEGY)) != Z_OK) {
            Jmsg(jcr, M_FATAL, 0, _("Compression deflateParams error: %d\n"), zstat);
            jcr->setJobStatus(JS_ErrorTerminated);
            goto err;
         }
      }
   }
 #endif
 #ifdef HAVE_LZO
   Bytef *cbuf2;
   int lzores;
   comp_stream_header ch;

   memset(&ch, 0, sizeof(comp_stream_header));
   cbuf2 = NULL;

   if ((ff_pkt->flags & FO_COMPRESS) && ff_pkt->Compress_algo == COMPRESS_LZO1X) {
      if ((ff_pkt->flags & FO_SPARSE) || (ff_pkt->flags & FO_OFFSETS)) {
         cbuf = (Bytef *)jcr->compress_buf + OFFSET_FADDR_SIZE;
         cbuf2 = (Bytef *)jcr->compress_buf + OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
         max_compress_len = jcr->compress_buf_size - OFFSET_FADDR_SIZE;
      } else {
         cbuf = (Bytef *)jcr->compress_buf;
         cbuf2 = (Bytef *)jcr->compress_buf + sizeof(comp_stream_header);
         max_compress_len = jcr->compress_buf_size; /* set max length */
      }
      ch.magic = COMPRESS_LZO1X;
      ch.version = COMP_HEAD_VERSION;
      wbuf = jcr->compress_buf;    /* compressed output here */
      cipher_input = (uint8_t *)jcr->compress_buf; /* encrypt compressed data */
   }
 #endif
#else
   const uint32_t max_compress_len = 0;
#endif

   if (ff_pkt->flags & FO_ENCRYPT) {
      if ((ff_pkt->flags & FO_SPARSE) || (ff_pkt->flags & FO_OFFSETS)) {
         Jmsg0(jcr, M_FATAL, 0, _("Encrypting sparse or offset data not supported.\n"));
         goto err;
      }
      /** Allocate the cipher context */
      if ((cipher_ctx = crypto_cipher_new(jcr->crypto.pki_session, true, 
           &cipher_block_size)) == NULL) {
         /* Shouldn't happen! */
         Jmsg0(jcr, M_FATAL, 0, _("Failed to initialize encryption context.\n"));
         goto err;
      }

      /**
       * Grow the crypto buffer, if necessary.
       * crypto_cipher_update() will buffer up to (cipher_block_size - 1).
       * We grow crypto_buf to the maximum number of blocks that
       * could be returned for the given read buffer size.
       * (Using the larger of either rsize or max_compress_len)
       */
      jcr->crypto.crypto_buf = check_pool_memory_size(jcr->crypto.crypto_buf, 
           (MAX(rsize + (int)sizeof(uint32_t), (int32_t)max_compress_len) + 
            cipher_block_size - 1) / cipher_block_size * cipher_block_size);

      wbuf = jcr->crypto.crypto_buf; /* Encrypted, possibly compressed output here. */
   }

   /**
    * Send Data header to Storage daemon
    *    <file-index> <stream> <info>
    */
   if (!sd->fsend("%ld %d 0", jcr->JobFiles, stream)) {
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      goto err;
   }
   Dmsg1(300, ">stored: datahdr %s\n", sd->msg);

   /**
    * Make space at beginning of buffer for fileAddr because this
    *   same buffer will be used for writing if compression is off.
    */
   if ((ff_pkt->flags & FO_SPARSE) || (ff_pkt->flags & FO_OFFSETS)) {
      rbuf += OFFSET_FADDR_SIZE;
      rsize -= OFFSET_FADDR_SIZE;
#ifdef HAVE_FREEBSD_OS
      /**
       * To read FreeBSD partitions, the read size must be
       *  a multiple of 512.
       */
      rsize = (rsize/512) * 512;
#endif
   }

   /** a RAW device read on win32 only works if the buffer is a multiple of 512 */
#ifdef HAVE_WIN32
   if (S_ISBLK(ff_pkt->statp.st_mode))
      rsize = (rsize/512) * 512;
#endif
   
   /**
    * Read the file data
    */
   while ((sd->msglen=(uint32_t)bread(&ff_pkt->bfd, rbuf, rsize)) > 0) {

      /** Check for sparse blocks */
      if (ff_pkt->flags & FO_SPARSE) {
         ser_declare;
         bool allZeros = false;
         if ((sd->msglen == rsize &&
              fileAddr+sd->msglen < (uint64_t)ff_pkt->statp.st_size) ||
             ((ff_pkt->type == FT_RAW || ff_pkt->type == FT_FIFO) &&
               (uint64_t)ff_pkt->statp.st_size == 0)) {
            allZeros = is_buf_zero(rbuf, rsize);
         }
         if (!allZeros) {
            /** Put file address as first data in buffer */
            ser_begin(wbuf, OFFSET_FADDR_SIZE);
            ser_uint64(fileAddr);     /* store fileAddr in begin of buffer */
         }
         fileAddr += sd->msglen;      /* update file address */
         /** Skip block of all zeros */
         if (allZeros) {
            continue;                 /* skip block of zeros */
         }
      } else if (ff_pkt->flags & FO_OFFSETS) {
         ser_declare;
         ser_begin(wbuf, OFFSET_FADDR_SIZE);
         ser_uint64(ff_pkt->bfd.offset);     /* store offset in begin of buffer */
      }

      jcr->ReadBytes += sd->msglen;         /* count bytes read */

      /** Uncompressed cipher input length */
      cipher_input_len = sd->msglen;

      /** Update checksum if requested */
      if (digest) {
         crypto_digest_update(digest, (uint8_t *)rbuf, sd->msglen);
      }

      /** Update signing digest if requested */
      if (signing_digest) {
         crypto_digest_update(signing_digest, (uint8_t *)rbuf, sd->msglen);
      }

#ifdef HAVE_LIBZ
      /** Do compression if turned on */
      if (ff_pkt->flags & FO_COMPRESS && ff_pkt->Compress_algo == COMPRESS_GZIP && jcr->pZLIB_compress_workset) {
         Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", cbuf, rbuf, sd->msglen);
         
         ((z_stream*)jcr->pZLIB_compress_workset)->next_in   = (Bytef *)rbuf;
                ((z_stream*)jcr->pZLIB_compress_workset)->avail_in  = sd->msglen;
         ((z_stream*)jcr->pZLIB_compress_workset)->next_out  = (Bytef *)cbuf;
                ((z_stream*)jcr->pZLIB_compress_workset)->avail_out = max_compress_len;

         if ((zstat=deflate((z_stream*)jcr->pZLIB_compress_workset, Z_FINISH)) != Z_STREAM_END) {
            Jmsg(jcr, M_FATAL, 0, _("Compression deflate error: %d\n"), zstat);
            jcr->setJobStatus(JS_ErrorTerminated);
            goto err;
         }
         compress_len = ((z_stream*)jcr->pZLIB_compress_workset)->total_out;
         /** reset zlib stream to be able to begin from scratch again */
         if ((zstat=deflateReset((z_stream*)jcr->pZLIB_compress_workset)) != Z_OK) {
            Jmsg(jcr, M_FATAL, 0, _("Compression deflateReset error: %d\n"), zstat);
            jcr->setJobStatus(JS_ErrorTerminated);
            goto err;
         }

         Dmsg2(400, "GZIP compressed len=%d uncompressed len=%d\n", compress_len, 
               sd->msglen);

         sd->msglen = compress_len;      /* set compressed length */
         cipher_input_len = compress_len;
      }
#endif
#ifdef HAVE_LZO
      /** Do compression if turned on */
      if (ff_pkt->flags & FO_COMPRESS && ff_pkt->Compress_algo == COMPRESS_LZO1X && jcr->LZO_compress_workset) {
         lzo_uint len;          /* TODO: See with the latest patch how to handle lzo_uint with 64bit */

         ser_declare;
         ser_begin(cbuf, sizeof(comp_stream_header));

         Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", cbuf, rbuf, sd->msglen);

         lzores = lzo1x_1_compress((const unsigned char*)rbuf, sd->msglen, cbuf2, 
                                   &len, jcr->LZO_compress_workset);
         compress_len = len;
         if (lzores == LZO_E_OK && compress_len <= max_compress_len)
         {
            /* complete header */
            ser_uint32(COMPRESS_LZO1X);
            ser_uint32(compress_len);
            ser_uint16(ch.level);
            ser_uint16(ch.version);
         } else {
            /** this should NEVER happen */
            Jmsg(jcr, M_FATAL, 0, _("Compression LZO error: %d\n"), lzores);
            jcr->setJobStatus(JS_ErrorTerminated);
            goto err;
         }

         Dmsg2(400, "LZO compressed len=%d uncompressed len=%d\n", compress_len, 
               sd->msglen);

         compress_len += sizeof(comp_stream_header); /* add size of header */
         sd->msglen = compress_len;      /* set compressed length */
         cipher_input_len = compress_len;
      }
#endif

      /**
       * Note, here we prepend the current record length to the beginning
       *  of the encrypted data. This is because both sparse and compression
       *  restore handling want records returned to them with exactly the
       *  same number of bytes that were processed in the backup handling.
       *  That is, both are block filters rather than a stream.  When doing
       *  compression, the compression routines may buffer data, so that for
       *  any one record compressed, when it is decompressed the same size
       *  will not be obtained. Of course, the buffered data eventually comes
       *  out in subsequent crypto_cipher_update() calls or at least
       *  when crypto_cipher_finalize() is called.  Unfortunately, this
       *  "feature" of encryption enormously complicates the restore code.
       */
      if (ff_pkt->flags & FO_ENCRYPT) {
         uint32_t initial_len = 0;
         ser_declare;

         if ((ff_pkt->flags & FO_SPARSE) || (ff_pkt->flags & FO_OFFSETS)) {
            cipher_input_len += OFFSET_FADDR_SIZE;
         }

         /** Encrypt the length of the input block */
         uint8_t packet_len[sizeof(uint32_t)];

         ser_begin(packet_len, sizeof(uint32_t));
         ser_uint32(cipher_input_len);    /* store data len in begin of buffer */
         Dmsg1(20, "Encrypt len=%d\n", cipher_input_len);

         if (!crypto_cipher_update(cipher_ctx, packet_len, sizeof(packet_len),
             (uint8_t *)jcr->crypto.crypto_buf, &initial_len)) {
            /** Encryption failed. Shouldn't happen. */
            Jmsg(jcr, M_FATAL, 0, _("Encryption error\n"));
            goto err;
         }

         /** Encrypt the input block */
         if (crypto_cipher_update(cipher_ctx, cipher_input, cipher_input_len, 
             (uint8_t *)&jcr->crypto.crypto_buf[initial_len], &encrypted_len)) {
            if ((initial_len + encrypted_len) == 0) {
               /** No full block of data available, read more data */
               continue;
            }
            Dmsg2(400, "encrypted len=%d unencrypted len=%d\n", encrypted_len, 
                  sd->msglen);
            sd->msglen = initial_len + encrypted_len; /* set encrypted length */
         } else {
            /** Encryption failed. Shouldn't happen. */
            Jmsg(jcr, M_FATAL, 0, _("Encryption error\n"));
            goto err;
         }
      }

      /* Send the buffer to the Storage daemon */
      if ((ff_pkt->flags & FO_SPARSE) || (ff_pkt->flags & FO_OFFSETS)) {
         sd->msglen += OFFSET_FADDR_SIZE; /* include fileAddr in size */
      }
      sd->msg = wbuf;              /* set correct write buffer */
      if (!sd->send()) {
         if (!jcr->is_job_canceled()) {
            Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                  sd->bstrerror());
         }
         goto err;
      }
      Dmsg1(130, "Send data to SD len=%d\n", sd->msglen);
      /*          #endif */
      jcr->JobBytes += sd->msglen;      /* count bytes saved possibly compressed/encrypted */
      sd->msg = msgsave;                /* restore read buffer */

   } /* end while read file data */

   if (sd->msglen < 0) {                 /* error */
      berrno be;
      Jmsg(jcr, M_ERROR, 0, _("Read error on file %s. ERR=%s\n"),
         ff_pkt->fname, be.bstrerror(ff_pkt->bfd.berrno));
      if (jcr->JobErrors++ > 1000) {       /* insanity check */
         Jmsg(jcr, M_FATAL, 0, _("Too many errors. JobErrors=%d.\n"), jcr->JobErrors);
      }
   } else if (ff_pkt->flags & FO_ENCRYPT) {
      /** 
       * For encryption, we must call finalize to push out any
       *  buffered data.
       */
      if (!crypto_cipher_finalize(cipher_ctx, (uint8_t *)jcr->crypto.crypto_buf, 
           &encrypted_len)) {
         /* Padding failed. Shouldn't happen. */
         Jmsg(jcr, M_FATAL, 0, _("Encryption padding error\n"));
         goto err;
      }

      /** Note, on SSL pre-0.9.7, there is always some output */
      if (encrypted_len > 0) {
         sd->msglen = encrypted_len;      /* set encrypted length */
         sd->msg = jcr->crypto.crypto_buf;       /* set correct write buffer */
         if (!sd->send()) {
            if (!jcr->is_job_canceled()) {
               Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                     sd->bstrerror());
            }
            goto err;
         }
         Dmsg1(130, "Send data to SD len=%d\n", sd->msglen);
         jcr->JobBytes += sd->msglen;     /* count bytes saved possibly compressed/encrypted */
         sd->msg = msgsave;               /* restore bnet buffer */
      }
   }

   if (!sd->signal(BNET_EOD)) {        /* indicate end of file data */
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      goto err;
   }

   /** Free the cipher context */
   if (cipher_ctx) {
      crypto_cipher_free(cipher_ctx);
   }
   return 1;

err:
   /** Free the cipher context */
   if (cipher_ctx) {
      crypto_cipher_free(cipher_ctx);
   }

   sd->msg = msgsave; /* restore bnet buffer */
   sd->msglen = 0;
   return 0;
}

bool encode_and_send_attributes(JCR *jcr, FF_PKT *ff_pkt, int &data_stream) 
{
   BSOCK *sd = jcr->store_bsock;
   char attribs[MAXSTRING];
   char attribsExBuf[MAXSTRING];
   char *attribsEx = NULL;
   int attr_stream;
   int comp_len;
   bool stat;
   int hangup = get_hangup();
#ifdef FD_NO_SEND_TEST
   return true;
#endif

   Dmsg1(300, "encode_and_send_attrs fname=%s\n", ff_pkt->fname);
   /** Find what data stream we will use, then encode the attributes */
   if ((data_stream = select_data_stream(ff_pkt)) == STREAM_NONE) {
      /* This should not happen */
      Jmsg0(jcr, M_FATAL, 0, _("Invalid file flags, no supported data stream type.\n"));
      return false;
   }
   encode_stat(attribs, &ff_pkt->statp, sizeof(ff_pkt->statp), ff_pkt->LinkFI, data_stream);

   /** Now possibly extend the attributes */
   if (IS_FT_OBJECT(ff_pkt->type)) {
      attr_stream = STREAM_RESTORE_OBJECT;
   } else {
      attribsEx = attribsExBuf;
      attr_stream = encode_attribsEx(jcr, attribsEx, ff_pkt);
   }

   Dmsg3(300, "File %s\nattribs=%s\nattribsEx=%s\n", ff_pkt->fname, attribs, attribsEx);

   jcr->lock();
   jcr->JobFiles++;                    /* increment number of files sent */
   ff_pkt->FileIndex = jcr->JobFiles;  /* return FileIndex */
   pm_strcpy(jcr->last_fname, ff_pkt->fname);
   jcr->unlock();

   /* Debug code: check if we must hangup */
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
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      return false;
   }
   Dmsg1(300, ">stored: attrhdr %s\n", sd->msg);

   /**
    * Send file attributes to Storage daemon
    *   File_index
    *   File type
    *   Filename (full path)
    *   Encoded attributes
    *   Link name (if type==FT_LNK or FT_LNKSAVED)
    *   Encoded extended-attributes (for Win32)
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
   case FT_LNK:
   case FT_LNKSAVED:
      Dmsg3(300, "Link %d %s to %s\n", jcr->JobFiles, ff_pkt->fname, ff_pkt->link);
      stat = sd->fsend("%ld %d %s%c%s%c%s%c%s%c%u%c", jcr->JobFiles,
                       ff_pkt->type, ff_pkt->fname, 0, attribs, 0, 
                       ff_pkt->link, 0, attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   case FT_DIREND:
   case FT_REPARSE:
   case FT_JUNCTION:
      /* Here link is the canonical filename (i.e. with trailing slash) */
      stat = sd->fsend("%ld %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                       ff_pkt->type, ff_pkt->link, 0, attribs, 0, 0, 
                       attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   case FT_PLUGIN_CONFIG:
   case FT_RESTORE_FIRST:
      comp_len = ff_pkt->object_len;
      ff_pkt->object_compression = 0;
      if (ff_pkt->object_len > 1000) {
         /* Big object, compress it */
         comp_len = ff_pkt->object_len + 1000;
         POOLMEM *comp_obj = get_memory(comp_len);
         /* *** FIXME *** check Zdeflate error */
         Zdeflate(ff_pkt->object, ff_pkt->object_len, comp_obj, comp_len);
         if (comp_len < ff_pkt->object_len) {
            ff_pkt->object = comp_obj;
            ff_pkt->object_compression = 1;    /* zlib level 9 compression */
         } else {
            /* Uncompressed object smaller, use it */
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
      /* Note we send one extra byte so Dir can store zero after object */
      sd->msglen += comp_len + 1;
      stat = sd->send();
      if (ff_pkt->object_compression) {
         free_and_null_pool_memory(ff_pkt->object);
      }
      break;
   case FT_REG:
      stat = sd->fsend("%ld %d %s%c%s%c%c%s%c%d%c", jcr->JobFiles,
               ff_pkt->type, ff_pkt->fname, 0, attribs, 0, 0, attribsEx, 0,
               ff_pkt->delta_seq, 0);
      break;
   default:
      stat = sd->fsend("%ld %d %s%c%s%c%c%s%c%u%c", jcr->JobFiles,
                       ff_pkt->type, ff_pkt->fname, 0, attribs, 0, 0,
                       attribsEx, 0, ff_pkt->delta_seq, 0);
      break;
   }

   if (!IS_FT_OBJECT(ff_pkt->type) && ff_pkt->type != FT_DELETED) {
      unstrip_path(ff_pkt);
   }

   Dmsg2(300, ">stored: attr len=%d: %s\n", sd->msglen, sd->msg);
   if (!stat && !jcr->is_job_canceled()) {
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
   }
   sd->signal(BNET_EOD);            /* indicate end of attributes data */
   return stat;
}

/**
 * Do in place strip of path
 */
static bool do_strip(int count, char *in)
{
   char *out = in;
   int stripped;
   int numsep = 0;

   /** Copy to first path separator -- Win32 might have c: ... */
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
   /* Copy to end */
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
 *   save file as if it came from a subdirectory.  This is most useful
 *   for dealing with snapshots, by removing the snapshot directory, or
 *   in handling vendor migrations where files have been restored with
 *   a vendor product into a subdirectory.
 */
void strip_path(FF_PKT *ff_pkt)
{
   if (!(ff_pkt->flags & FO_STRIPPATH) || ff_pkt->strip_path <= 0) {
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
    * Strip path.  If it doesn't succeed put it back.  If
    *  it does, and there is a different link string,
    *  attempt to strip the link. If it fails, back them
    *  both back.
    * Do not strip symlinks.
    * I.e. if either stripping fails don't strip anything.
    */
   if (!do_strip(ff_pkt->strip_path, ff_pkt->fname)) {
      unstrip_path(ff_pkt);
      goto rtn;
   } 
   /** Strip links but not symlinks */
   if (ff_pkt->type != FT_LNK && ff_pkt->fname != ff_pkt->link) {
      if (!do_strip(ff_pkt->strip_path, ff_pkt->link)) {
         unstrip_path(ff_pkt);
      }
   }

rtn:
   Dmsg3(100, "fname=%s stripped=%s link=%s\n", ff_pkt->fname_save, ff_pkt->fname, 
       ff_pkt->link);
}

void unstrip_path(FF_PKT *ff_pkt)
{
   if (!(ff_pkt->flags & FO_STRIPPATH) || ff_pkt->strip_path <= 0) {
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

static void close_vss_backup_session(JCR *jcr)
{
#if defined(WIN32_VSS)
   /* STOP VSS ON WIN32 */
   /* tell vss to close the backup session */
   if (jcr->VSS) {
      if (g_pVSSClient->CloseBackup()) {             
         /* inform user about writer states */
         for (int i=0; i<(int)g_pVSSClient->GetWriterCount(); i++) {
            int msg_type = M_INFO;
            if (g_pVSSClient->GetWriterState(i) < 1) {
               msg_type = M_WARNING;
               jcr->JobErrors++;
            }
            Jmsg(jcr, msg_type, 0, _("VSS Writer (BackupComplete): %s\n"), g_pVSSClient->GetWriterInfo(i));
         }
      }
      /* Generate Job global writer metadata */
      WCHAR *metadata = g_pVSSClient->GetMetadata();
      if (metadata) {
         FF_PKT *ff_pkt = jcr->ff;
         ff_pkt->fname = (char *)"*all*"; /* for all plugins */
         ff_pkt->type = FT_RESTORE_FIRST;
         ff_pkt->LinkFI = 0;
         ff_pkt->object_name = (char *)"job_metadata.xml";
         ff_pkt->object = (char *)metadata;
         ff_pkt->object_len = (wcslen(metadata) + 1) * sizeof(WCHAR);
         ff_pkt->object_index = (int)time(NULL);
         save_file(jcr, ff_pkt, true);
     }
   }
#endif
}
