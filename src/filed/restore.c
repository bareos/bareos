/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
/*
 *  Bacula File Daemon  restore.c Restorefiles.
 *
 *    Kern Sibbald, November MM
 *
 */

#include "bacula.h"
#include "filed.h"
#include "ch.h"
#include "restore.h"

#ifdef HAVE_DARWIN_OS
#include <sys/attr.h>
const bool have_darwin_os = true;
#else
const bool have_darwin_os = false;
#endif

#if defined(HAVE_CRYPTO)
const bool have_crypto = true;
#else
const bool have_crypto = false;
#endif

#if defined(HAVE_ACL)
const bool have_acl = true;
#else
const bool have_acl = false;
#endif

#ifdef HAVE_SHA2
const bool have_sha2 = true;
#else
const bool have_sha2 = false;
#endif

#if defined(HAVE_XATTR)
const bool have_xattr = true;
#else
const bool have_xattr = false;
#endif

/*
 * Data received from Storage Daemon
 */
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";

/*
 * Forward referenced functions
 */
#if   defined(HAVE_LIBZ)
static const char *zlib_strerror(int stat);
const bool have_libz = true;
#else
const bool have_libz = false;
#endif
#ifdef HAVE_LZO
const bool have_lzo = true;
#else
const bool have_lzo = false;
#endif

static void deallocate_cipher(r_ctx &rctx);
static void deallocate_fork_cipher(r_ctx &rctx);
static void free_signature(r_ctx &rctx);
static void free_session(r_ctx &rctx);
static bool close_previous_stream(JCR *jcr, r_ctx &rctx);
static bool verify_signature(JCR *jcr, r_ctx &rctx);
int32_t extract_data(JCR *jcr, BFILE *bfd, POOLMEM *buf, int32_t buflen,
                     uint64_t *addr, int flags, int32_t stream, RESTORE_CIPHER_CTX *cipher_ctx);
bool flush_cipher(JCR *jcr, BFILE *bfd, uint64_t *addr, int flags, int32_t stream,
                  RESTORE_CIPHER_CTX *cipher_ctx);

/*
 * Close a bfd check that we are at the expected file offset.
 * Makes use of some code from set_attributes().
 */
static int bclose_chksize(JCR *jcr, BFILE *bfd, boffset_t osize)
{
   char ec1[50], ec2[50];
   boffset_t fsize;

   fsize = blseek(bfd, 0, SEEK_CUR);
   bclose(bfd);
   if (fsize > 0 && fsize != osize) {
      Qmsg3(jcr, M_WARNING, 0, _("Size of data or stream of %s not correct. Original %s, restored %s.\n"),
            jcr->last_fname, edit_uint64(osize, ec1),
            edit_uint64(fsize, ec2));
      return -1;
   }
   return 0;
}

#ifdef HAVE_DARWIN_OS
static bool restore_finderinfo(JCR *jcr, POOLMEM *buf, int32_t buflen)
{
   struct attrlist attrList;

   memset(&attrList, 0, sizeof(attrList));
   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr = ATTR_CMN_FNDRINFO;

   Dmsg0(130, "Restoring Finder Info\n");
   jcr->ff->flags |= FO_HFSPLUS;
   if (buflen != 32) {
      Jmsg(jcr, M_WARNING, 0, _("Invalid length of Finder Info (got %d, not 32)\n"), buflen);
      return false;
   }

   if (setattrlist(jcr->last_fname, &attrList, buf, buflen, 0) != 0) {
      Jmsg(jcr, M_WARNING, 0, _("Could not set Finder Info on %s\n"), jcr->last_fname);
      return false;
   }

   return true;
}
#else
static bool restore_finderinfo(JCR *jcr, POOLMEM *buf, int32_t buflen)
{
   return true;
}
#endif

/*
 * Cleanup of delayed restore stack with streams for later
 * processing.
 */
static inline void drop_delayed_restore_streams(r_ctx &rctx, bool reuse)
{
   RESTORE_DATA_STREAM *rds;

   if (!rctx.delayed_streams ||
       rctx.delayed_streams->empty()) {
      return;
   }

   foreach_alist(rds, rctx.delayed_streams) {
      free(rds->content);
   }

   rctx.delayed_streams->destroy();
   if (reuse) {
      rctx.delayed_streams->init(10, owned_by_alist);
   }
}

/*
 * Push a data stream onto the delayed restore stack for
 * later processing.
 */
static inline void push_delayed_restore_stream(r_ctx &rctx, BSOCK *sd)
{
   RESTORE_DATA_STREAM *rds;

   if (!rctx.delayed_streams) {
      rctx.delayed_streams = New(alist(10, owned_by_alist));
   }

   rds = (RESTORE_DATA_STREAM *)malloc(sizeof(RESTORE_DATA_STREAM));
   rds->stream = rctx.stream;
   rds->content = (char *)malloc(sd->msglen);
   memcpy(rds->content, sd->msg, sd->msglen);
   rds->content_length = sd->msglen;

   rctx.delayed_streams->append(rds);
}

/*
 * Perform a restore of an ACL using the stream received.
 * This can either be a delayed restore or direct restore.
 */
static inline bool do_restore_acl(JCR *jcr,
                                  int stream,
                                  char *content,
                                  uint32_t content_length)

{
   switch (parse_acl_streams(jcr, stream, content, content_length)) {
   case bacl_exit_fatal:
      return false;
   case bacl_exit_error:
      /*
       * Non-fatal errors, count them and when the number is under ACL_REPORT_ERR_MAX_PER_JOB
       * print the error message set by the lower level routine in jcr->errmsg.
       */
      if (jcr->acl_data->u.parse->nr_errors < ACL_REPORT_ERR_MAX_PER_JOB) {
         Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);
      }
      jcr->acl_data->u.parse->nr_errors++;
      break;
   case bacl_exit_ok:
      break;
   }
   return true;
}

/*
 * Perform a restore of an XATTR using the stream received.
 * This can either be a delayed restore or direct restore.
 */
static inline bool do_restore_xattr(JCR *jcr,
                                    int stream,
                                    char *content,
                                    uint32_t content_length)
{
   switch (parse_xattr_streams(jcr, stream, content, content_length)) {
   case bxattr_exit_fatal:
      return false;
   case bxattr_exit_error:
      /*
       * Non-fatal errors, count them and when the number is under XATTR_REPORT_ERR_MAX_PER_JOB
       * print the error message set by the lower level routine in jcr->errmsg.
       */
      if (jcr->xattr_data->u.parse->nr_errors < XATTR_REPORT_ERR_MAX_PER_JOB) {
         Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);
      }
      jcr->xattr_data->u.parse->nr_errors++;
      break;
   case bxattr_exit_ok:
      break;
   }
   return true;
}

/*
 * Restore any data streams that are restored after the file
 * is fully restored and has its attributes restored. Things
 * like acls and xattr are restored after we set the file
 * attributes otherwise we might clear some security flags
 * by setting the attributes.
 */
static inline bool pop_delayed_data_streams(JCR *jcr, r_ctx &rctx)
{
   RESTORE_DATA_STREAM *rds;

   /*
    * See if there is anything todo.
    */
   if (!rctx.delayed_streams ||
        rctx.delayed_streams->empty()) {
      return true;
   }

   /*
    * Only process known delayed data streams here.
    * If you start using more delayed data streams
    * be sure to add them in this loop and add the
    * proper calls here.
    *
    * Currently we support delayed data stream
    * processing for the following type of streams:
    * - *_ACL_*
    * - *_XATTR_*
    */
   foreach_alist(rds, rctx.delayed_streams) {
      switch (rds->stream) {
      case STREAM_UNIX_ACCESS_ACL:
      case STREAM_UNIX_DEFAULT_ACL:
      case STREAM_ACL_AIX_TEXT:
      case STREAM_ACL_DARWIN_ACCESS_ACL:
      case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      case STREAM_ACL_FREEBSD_ACCESS_ACL:
      case STREAM_ACL_HPUX_ACL_ENTRY:
      case STREAM_ACL_IRIX_DEFAULT_ACL:
      case STREAM_ACL_IRIX_ACCESS_ACL:
      case STREAM_ACL_LINUX_DEFAULT_ACL:
      case STREAM_ACL_LINUX_ACCESS_ACL:
      case STREAM_ACL_TRU64_DEFAULT_ACL:
      case STREAM_ACL_TRU64_DEFAULT_DIR_ACL:
      case STREAM_ACL_TRU64_ACCESS_ACL:
      case STREAM_ACL_SOLARIS_ACLENT:
      case STREAM_ACL_SOLARIS_ACE:
      case STREAM_ACL_AFS_TEXT:
      case STREAM_ACL_AIX_AIXC:
      case STREAM_ACL_AIX_NFS4:
      case STREAM_ACL_FREEBSD_NFS4_ACL:
      case STREAM_ACL_HURD_DEFAULT_ACL:
      case STREAM_ACL_HURD_ACCESS_ACL:
         if (!do_restore_acl(jcr, rds->stream, rds->content, rds->content_length)) {
            goto bail_out;
         }
         free(rds->content);
         break;
      case STREAM_XATTR_HURD:
      case STREAM_XATTR_IRIX:
      case STREAM_XATTR_TRU64:
      case STREAM_XATTR_AIX:
      case STREAM_XATTR_OPENBSD:
      case STREAM_XATTR_SOLARIS_SYS:
      case STREAM_XATTR_DARWIN:
      case STREAM_XATTR_FREEBSD:
      case STREAM_XATTR_LINUX:
      case STREAM_XATTR_NETBSD:
         if (!do_restore_xattr(jcr, rds->stream, rds->content, rds->content_length)) {
            goto bail_out;
         }
         free(rds->content);
         break;
      default:
         Jmsg(jcr, M_WARNING, 0, _("Unknown stream=%d ignored. This shouldn't happen!\n"),
              rds->stream);
         break;
      }
   }

   /*
    * We processed the stack so we can destroy it.
    */
   rctx.delayed_streams->destroy();

   /*
    * (Re)Initialize the stack for a new use.
    */
   rctx.delayed_streams->init(10, owned_by_alist);

   return true;

bail_out:

   /*
    * Destroy the content of the stack and (re)initialize it for a new use.
    */
   drop_delayed_restore_streams(rctx, true);

   return false;
}

/*
 * Restore the requested files.
 */
void do_restore(JCR *jcr)
{
   BSOCK *sd;
   uint32_t VolSessionId, VolSessionTime;
   int32_t file_index;
   char ec1[50];                      /* Buffer printing huge values */
   uint32_t buf_size;                 /* client buffer size */
   int stat;
   int64_t rsrc_len = 0;              /* Original length of resource fork */
   r_ctx rctx;
   ATTR *attr;
   /* ***FIXME*** make configurable */
   crypto_digest_t signing_algorithm = have_sha2 ? 
                                       CRYPTO_DIGEST_SHA256 : CRYPTO_DIGEST_SHA1;
   memset(&rctx, 0, sizeof(rctx));
   rctx.jcr = jcr;

   /*
    * The following variables keep track of "known unknowns"
    */
   int non_support_data = 0;
   int non_support_attr = 0;
   int non_support_rsrc = 0;
   int non_support_finfo = 0;
   int non_support_acl = 0;
   int non_support_progname = 0;
   int non_support_crypto = 0;
   int non_support_xattr = 0;

   sd = jcr->store_bsock;
   jcr->setJobStatus(JS_Running);

   LockRes();
   CLIENT *client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   UnlockRes();
   if (client) {
      buf_size = client->max_network_buffer_size;
   } else {
      buf_size = 0;                   /* use default */
   }
   if (!bnet_set_buffer_size(sd, buf_size, BNET_SETBUF_WRITE)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      return;
   }
   jcr->buf_size = sd->msglen;

   /*
    * St Bernard code goes here if implemented -- see end of file
    */

   /* use the same buffer size to decompress both gzip and lzo */
   if (have_libz || have_lzo) {
      uint32_t compress_buf_size = jcr->buf_size + 12 + ((jcr->buf_size+999) / 1000) + 100;
      jcr->compress_buf = get_memory(compress_buf_size);
      jcr->compress_buf_size = compress_buf_size;
   }

#ifdef HAVE_LZO
   if (lzo_init() != LZO_E_OK) {
      Jmsg(jcr, M_FATAL, 0, _("LZO init failed\n"));
      goto bail_out;
   }
#endif

   if (have_crypto) {
      rctx.cipher_ctx.buf = get_memory(CRYPTO_CIPHER_MAX_BLOCK_SIZE);
      if (have_darwin_os) {
         rctx.fork_cipher_ctx.buf = get_memory(CRYPTO_CIPHER_MAX_BLOCK_SIZE);
      }
   }
   
   /*
    * Get a record from the Storage daemon. We are guaranteed to
    *   receive records in the following order:
    *   1. Stream record header
    *   2. Stream data (one or more of the following in the order given)
    *        a. Attributes (Unix or Win32)
    *        b. Possibly stream encryption session data (e.g., symmetric session key)
    *        c. File data for the file
    *        d. Alternate data stream (e.g. Resource Fork)
    *        e. Finder info
    *        f. ACLs
    *        g. XATTRs
    *        h. Possibly a cryptographic signature
    *        i. Possibly MD5 or SHA1 record
    *   3. Repeat step 1
    *
    * NOTE: We keep track of two bacula file descriptors:
    *   1. bfd for file data.
    *      This fd is opened for non empty files when an attribute stream is
    *      encountered and closed when we find the next attribute stream.
    *   2. fork_bfd for alternate data streams
    *      This fd is opened every time we encounter a new alternate data
    *      stream for the current file. When we find any other stream, we
    *      close it again.
    *      The expected size of the stream, fork_len, should be set when
    *      opening the fd.
    *   3. Not all the stream data records are required -- e.g. if there
    *      is no fork, there is no alternate data stream, no ACL, ...
    */
   binit(&rctx.bfd);
   binit(&rctx.forkbfd);
   attr = rctx.attr = new_attr(jcr);
   if (have_acl) {
      jcr->acl_data = (acl_data_t *)malloc(sizeof(acl_data_t));
      memset(jcr->acl_data, 0, sizeof(acl_data_t));
      jcr->acl_data->u.parse = (acl_parse_data_t *)malloc(sizeof(acl_parse_data_t));
      memset(jcr->acl_data->u.parse, 0, sizeof(acl_parse_data_t));
   }
   if (have_xattr) {
      jcr->xattr_data = (xattr_data_t *)malloc(sizeof(xattr_data_t));
      memset(jcr->xattr_data, 0, sizeof(xattr_data_t));
      jcr->xattr_data->u.parse = (xattr_parse_data_t *)malloc(sizeof(xattr_parse_data_t));
      memset(jcr->xattr_data->u.parse, 0, sizeof(xattr_parse_data_t));
   }

   while (bget_msg(sd) >= 0 && !job_canceled(jcr)) {
      /*
       * Remember previous stream type
       */
      rctx.prev_stream = rctx.stream;

      /*
       * First we expect a Stream Record Header
       */
      if (sscanf(sd->msg, rec_header, &VolSessionId, &VolSessionTime, &file_index,
          &rctx.full_stream, &rctx.size) != 5) {
         Jmsg1(jcr, M_FATAL, 0, _("Record header scan error: %s\n"), sd->msg);
         goto bail_out;
      }
      /* Strip off new stream high bits */
      rctx.stream = rctx.full_stream & STREAMMASK_TYPE;
      Dmsg5(150, "Got hdr: Files=%d FilInx=%d size=%d Stream=%d, %s.\n", 
            jcr->JobFiles, file_index, rctx.size, rctx.stream, stream_to_ascii(rctx.stream));

      /*
       * Now we expect the Stream Data
       */
      if (bget_msg(sd) < 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Data record error. ERR=%s\n"), sd->bstrerror());
         goto bail_out;
      }
      if (rctx.size != (uint32_t)sd->msglen) {
         Jmsg2(jcr, M_FATAL, 0, _("Actual data size %d not same as header %d\n"), 
               sd->msglen, rctx.size);
         Dmsg2(50, "Actual data size %d not same as header %d\n",
               sd->msglen, rctx.size);
         goto bail_out;
      }
      Dmsg3(130, "Got stream: %s len=%d extract=%d\n", stream_to_ascii(rctx.stream), 
            sd->msglen, rctx.extract);

      /*
       * If we change streams, close and reset alternate data streams
       */
      if (rctx.prev_stream != rctx.stream) {
         if (is_bopen(&rctx.forkbfd)) {
            deallocate_fork_cipher(rctx);
            bclose_chksize(jcr, &rctx.forkbfd, rctx.fork_size);
         }
         /*
          * Use an impossible value and set a proper one below
          */
         rctx.fork_size = -1;
         rctx.fork_addr = 0;
      }

      /*
       * File Attributes stream
       */
      switch (rctx.stream) {
      case STREAM_UNIX_ATTRIBUTES:
      case STREAM_UNIX_ATTRIBUTES_EX:
         /*
          * if any previous stream open, close it
          */
         if (!close_previous_stream(jcr, rctx)) {
            goto bail_out;
         }

         /*
          * TODO: manage deleted files
          */
         if (rctx.type == FT_DELETED) { /* deleted file */
            continue;
         }
         /*
          * Restore objects should be ignored here -- they are
          * returned at the beginning of the restore. 
          */
         if (IS_FT_OBJECT(rctx.type)) {
            continue;
         }

         /*
          * Unpack attributes and do sanity check them
          */
         if (!unpack_attributes_record(jcr, rctx.stream, sd->msg, sd->msglen, attr)) {
            goto bail_out;
         }

         Dmsg3(100, "File %s\nattrib=%s\nattribsEx=%s\n", attr->fname,
               attr->attr, attr->attrEx);
         Dmsg3(100, "=== msglen=%d attrExlen=%d msg=%s\n", sd->msglen,
               strlen(attr->attrEx), sd->msg);

         attr->data_stream = decode_stat(attr->attr, &attr->statp, sizeof(attr->statp), &attr->LinkFI);

         if (!is_restore_stream_supported(attr->data_stream)) {
            if (!non_support_data++) {
               Jmsg(jcr, M_WARNING, 0, _("%s stream not supported on this Client.\n"),
                    stream_to_ascii(attr->data_stream));
            }
            continue;
         }

         build_attr_output_fnames(jcr, attr);

         /*
          * Try to actually create the file, which returns a status telling
          * us if we need to extract or not.
          */
         jcr->num_files_examined++;
         rctx.extract = false;
         stat = CF_CORE;        /* By default, let Bacula's core handle it */

         if (jcr->plugin) {
            stat = plugin_create_file(jcr, attr, &rctx.bfd, jcr->replace);
         } 
         
         if (stat == CF_CORE) {
            stat = create_file(jcr, attr, &rctx.bfd, jcr->replace);
         }
         jcr->lock();  
         pm_strcpy(jcr->last_fname, attr->ofname);
         jcr->last_type = attr->type;
         jcr->unlock();
         Dmsg2(130, "Outfile=%s create_file stat=%d\n", attr->ofname, stat);
         switch (stat) {
         case CF_ERROR:
         case CF_SKIP:
            jcr->JobFiles++;
            break;
         case CF_EXTRACT:
            /*
             * File created and we expect file data
             */
            rctx.extract = true;
            /*
             * FALLTHROUGH
             */
         case CF_CREATED:
            /*
             * File created, but there is no content
             */
            rctx.fileAddr = 0;
            print_ls_output(jcr, attr);

            if (have_darwin_os) {
               /*
                * Only restore the resource fork for regular files
                */
               from_base64(&rsrc_len, attr->attrEx);
               if (attr->type == FT_REG && rsrc_len > 0) {
                  rctx.extract = true;
               }

               /*
                * Do not count the resource forks as regular files being restored.
                */
               if (rsrc_len == 0) {
                  jcr->JobFiles++;
               }
            } else {
               jcr->JobFiles++;
            }

            if (!rctx.extract) {
               /*
                * set attributes now because file will not be extracted
                */
               if (jcr->plugin) {
                  plugin_set_attributes(jcr, attr, &rctx.bfd);
               } else {
                  set_attributes(jcr, attr, &rctx.bfd);
               }
            }
            break;
         }
         break;

      /*
       * Data stream
       */
      case STREAM_ENCRYPTED_SESSION_DATA:
         crypto_error_t cryptoerr;

         /*
          * Is this an unexpected session data entry?
          */
         if (rctx.cs) {
            Jmsg0(jcr, M_ERROR, 0, _("Unexpected cryptographic session data stream.\n"));
            rctx.extract = false;
            bclose(&rctx.bfd);
            continue;
         }

         /*
          * Do we have any keys at all?
          */
         if (!jcr->crypto.pki_recipients) {
            Jmsg(jcr, M_ERROR, 0, _("No private decryption keys have been defined to decrypt encrypted backup data.\n"));
            rctx.extract = false;
            bclose(&rctx.bfd);
            break;
         }

         if (jcr->crypto.digest) {
            crypto_digest_free(jcr->crypto.digest);
         }  
         jcr->crypto.digest = crypto_digest_new(jcr, signing_algorithm);
         if (!jcr->crypto.digest) {
            Jmsg0(jcr, M_FATAL, 0, _("Could not create digest.\n"));
            rctx.extract = false;
            bclose(&rctx.bfd);
            break;
         }

         /*
          * Decode and save session keys.
          */
         cryptoerr = crypto_session_decode((uint8_t *)sd->msg, (uint32_t)sd->msglen, 
                        jcr->crypto.pki_recipients, &rctx.cs);
         switch(cryptoerr) {
         case CRYPTO_ERROR_NONE:
            /*
             * Success
             */
            break;
         case CRYPTO_ERROR_NORECIPIENT:
            Jmsg(jcr, M_ERROR, 0, _("Missing private key required to decrypt encrypted backup data.\n"));
            break;
         case CRYPTO_ERROR_DECRYPTION:
            Jmsg(jcr, M_ERROR, 0, _("Decrypt of the session key failed.\n"));
            break;
         default:
            /*
             * Shouldn't happen
             */
            Jmsg1(jcr, M_ERROR, 0, _("An error occurred while decoding encrypted session data stream: %s\n"), crypto_strerror(cryptoerr));
            break;
         }

         if (cryptoerr != CRYPTO_ERROR_NONE) {
            rctx.extract = false;
            bclose(&rctx.bfd);
            continue;
         }

         break;

      case STREAM_FILE_DATA:
      case STREAM_SPARSE_DATA:
      case STREAM_WIN32_DATA:
      case STREAM_GZIP_DATA:
      case STREAM_SPARSE_GZIP_DATA:
      case STREAM_WIN32_GZIP_DATA:
      case STREAM_COMPRESSED_DATA:
      case STREAM_SPARSE_COMPRESSED_DATA:
      case STREAM_WIN32_COMPRESSED_DATA:
      case STREAM_ENCRYPTED_FILE_DATA:
      case STREAM_ENCRYPTED_WIN32_DATA:
      case STREAM_ENCRYPTED_FILE_GZIP_DATA:
      case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
      case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
      case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
         /*
          * Force an expected, consistent stream type here
          */
         if (rctx.extract && (rctx.prev_stream == rctx.stream 
                         || rctx.prev_stream == STREAM_UNIX_ATTRIBUTES
                         || rctx.prev_stream == STREAM_UNIX_ATTRIBUTES_EX
                         || rctx.prev_stream == STREAM_ENCRYPTED_SESSION_DATA)) {
            rctx.flags = 0;

            if (rctx.stream == STREAM_SPARSE_DATA
                  || rctx.stream == STREAM_SPARSE_COMPRESSED_DATA
                  || rctx.stream == STREAM_SPARSE_GZIP_DATA) {
               rctx.flags |= FO_SPARSE;
            }

            if (rctx.stream == STREAM_GZIP_DATA 
                  || rctx.stream == STREAM_SPARSE_GZIP_DATA
                  || rctx.stream == STREAM_WIN32_GZIP_DATA
                  || rctx.stream == STREAM_ENCRYPTED_FILE_GZIP_DATA
                  || rctx.stream == STREAM_COMPRESSED_DATA
                  || rctx.stream == STREAM_SPARSE_COMPRESSED_DATA
                  || rctx.stream == STREAM_WIN32_COMPRESSED_DATA
                  || rctx.stream == STREAM_ENCRYPTED_FILE_COMPRESSED_DATA
                  || rctx.stream == STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA
                  || rctx.stream == STREAM_ENCRYPTED_WIN32_GZIP_DATA) {
               rctx.flags |= FO_COMPRESS;
               rctx.comp_stream = rctx.stream;
            }

            if (rctx.stream == STREAM_ENCRYPTED_FILE_DATA
                  || rctx.stream == STREAM_ENCRYPTED_FILE_GZIP_DATA
                  || rctx.stream == STREAM_ENCRYPTED_WIN32_DATA
                  || rctx.stream == STREAM_ENCRYPTED_FILE_COMPRESSED_DATA
                  || rctx.stream == STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA
                  || rctx.stream == STREAM_ENCRYPTED_WIN32_GZIP_DATA) {               
               /*
                * Set up a decryption context
                */
               if (!rctx.cipher_ctx.cipher) {
                  if (!rctx.cs) {
                     Jmsg1(jcr, M_ERROR, 0, _("Missing encryption session data stream for %s\n"), jcr->last_fname);
                     rctx.extract = false;
                     bclose(&rctx.bfd);
                     continue;
                  }

                  if ((rctx.cipher_ctx.cipher = crypto_cipher_new(rctx.cs, false, 
                           &rctx.cipher_ctx.block_size)) == NULL) {
                     Jmsg1(jcr, M_ERROR, 0, _("Failed to initialize decryption context for %s\n"), jcr->last_fname);
                     free_session(rctx);
                     rctx.extract = false;
                     bclose(&rctx.bfd);
                     continue;
                  }
               }
               rctx.flags |= FO_ENCRYPT;
            }

            if (is_win32_stream(rctx.stream) && !have_win32_api()) {
               set_portable_backup(&rctx.bfd);
               /*
                * "decompose" BackupWrite data
                */
               rctx.flags |= FO_WIN32DECOMP;
            }

            if (extract_data(jcr, &rctx.bfd, sd->msg, sd->msglen, &rctx.fileAddr,
                             rctx.flags, rctx.stream, &rctx.cipher_ctx) < 0) {
               rctx.extract = false;
               bclose(&rctx.bfd);
               continue;
            }
         }
         break;

      /*
       * Resource fork stream - only recorded after a file to be restored
       * Silently ignore if we cannot write - we already reported that
       */
      case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      case STREAM_MACOS_FORK_DATA:
         if (have_darwin_os) {
            rctx.fork_flags = 0;
            jcr->ff->flags |= FO_HFSPLUS;

            if (rctx.stream == STREAM_ENCRYPTED_MACOS_FORK_DATA) {
               rctx.fork_flags |= FO_ENCRYPT;

               /*
                * Set up a decryption context
                */
               if (rctx.extract && !rctx.fork_cipher_ctx.cipher) {
                  if (!rctx.cs) {
                     Jmsg1(jcr, M_ERROR, 0, _("Missing encryption session data stream for %s\n"), jcr->last_fname);
                     rctx.extract = false;
                     bclose(&rctx.bfd);
                     continue;
                  }

                  if ((rctx.fork_cipher_ctx.cipher = crypto_cipher_new(rctx.cs, false, &rctx.fork_cipher_ctx.block_size)) == NULL) {
                     Jmsg1(jcr, M_ERROR, 0, _("Failed to initialize decryption context for %s\n"), jcr->last_fname);
                     free_session(rctx);
                     rctx.extract = false;
                     bclose(&rctx.bfd);
                     continue;
                  }
               }
            }

            if (rctx.extract) {
               if (rctx.prev_stream != rctx.stream) {
                  if (bopen_rsrc(&rctx.forkbfd, jcr->last_fname, O_WRONLY | O_TRUNC | O_BINARY, 0) < 0) {
                     Jmsg(jcr, M_WARNING, 0, _("Cannot open resource fork for %s.\n"), jcr->last_fname);
                     rctx.extract = false;
                     continue;
                  }

                  rctx.fork_size = rsrc_len;
                  Dmsg0(130, "Restoring resource fork\n");
               }

               if (extract_data(jcr, &rctx.forkbfd, sd->msg, sd->msglen, &rctx.fork_addr, rctx.fork_flags,
                                rctx.stream, &rctx.fork_cipher_ctx) < 0) {
                  rctx.extract = false;
                  bclose(&rctx.forkbfd);
                  continue;
               }
            }
         } else {
            non_support_rsrc++;
         }
         break;

      case STREAM_HFSPLUS_ATTRIBUTES:
         if (have_darwin_os) {
            if (!restore_finderinfo(jcr, sd->msg, sd->msglen)) {
               continue;
            }
         } else {
            non_support_finfo++;
         }
         break;

      case STREAM_UNIX_ACCESS_ACL:
      case STREAM_UNIX_DEFAULT_ACL:
      case STREAM_ACL_AIX_TEXT:
      case STREAM_ACL_DARWIN_ACCESS_ACL:
      case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      case STREAM_ACL_FREEBSD_ACCESS_ACL:
      case STREAM_ACL_HPUX_ACL_ENTRY:
      case STREAM_ACL_IRIX_DEFAULT_ACL:
      case STREAM_ACL_IRIX_ACCESS_ACL:
      case STREAM_ACL_LINUX_DEFAULT_ACL:
      case STREAM_ACL_LINUX_ACCESS_ACL:
      case STREAM_ACL_TRU64_DEFAULT_ACL:
      case STREAM_ACL_TRU64_DEFAULT_DIR_ACL:
      case STREAM_ACL_TRU64_ACCESS_ACL:
      case STREAM_ACL_SOLARIS_ACLENT:
      case STREAM_ACL_SOLARIS_ACE:
      case STREAM_ACL_AFS_TEXT:
      case STREAM_ACL_AIX_AIXC:
      case STREAM_ACL_AIX_NFS4:
      case STREAM_ACL_FREEBSD_NFS4_ACL:
      case STREAM_ACL_HURD_DEFAULT_ACL:
      case STREAM_ACL_HURD_ACCESS_ACL:
         /*
          * Do not restore ACLs when
          * a) The current file is not extracted
          * b)     and it is not a directory (they are never "extracted")
          * c) or the file name is empty
          */
         if ((!rctx.extract &&
               jcr->last_type != FT_DIREND) ||
             (*jcr->last_fname == 0)) {
            break;
         }
         if (have_acl) {
            /*
             * For anything that is not a directory we delay
             * the restore of acls till a later stage.
             */
            if (jcr->last_type != FT_DIREND) {
               push_delayed_restore_stream(rctx, sd);
            } else {
               if (!do_restore_acl(jcr, rctx.stream, sd->msg, sd->msglen)) {
                  goto bail_out;
               }
            }
         } else {
            non_support_acl++;
         }
         break;

      case STREAM_XATTR_HURD:
      case STREAM_XATTR_IRIX:
      case STREAM_XATTR_TRU64:
      case STREAM_XATTR_AIX:
      case STREAM_XATTR_OPENBSD:
      case STREAM_XATTR_SOLARIS_SYS:
      case STREAM_XATTR_DARWIN:
      case STREAM_XATTR_FREEBSD:
      case STREAM_XATTR_LINUX:
      case STREAM_XATTR_NETBSD:
         /*
          * Do not restore Extended Attributes when
          * a) The current file is not extracted
          * b)     and it is not a directory (they are never "extracted")
          * c) or the file name is empty
          */
         if ((!rctx.extract &&
               jcr->last_type != FT_DIREND) ||
             (*jcr->last_fname == 0)) {
            break;
         }
         if (have_xattr) {
            /*
             * For anything that is not a directory we delay
             * the restore of xattr till a later stage.
             */
            if (jcr->last_type != FT_DIREND) {
               push_delayed_restore_stream(rctx, sd);
            } else {
               if (!do_restore_xattr(jcr, rctx.stream, sd->msg, sd->msglen)) {
                  goto bail_out;
               }
            }
         } else {
            non_support_xattr++;
         }
         break;

      case STREAM_XATTR_SOLARIS:
         /*
          * Do not restore Extended Attributes when
          * a) The current file is not extracted
          * b)     and it is not a directory (they are never "extracted")
          * c) or the file name is empty
          */
         if ((!rctx.extract &&
               jcr->last_type != FT_DIREND) ||
             (*jcr->last_fname == 0)) {
            break;
         }
         if (have_xattr) {
            if (!do_restore_xattr(jcr, rctx.stream, sd->msg, sd->msglen)) {
               goto bail_out;
            }
         } else {
            non_support_xattr++;
         }
         break;

      case STREAM_SIGNED_DIGEST:
         /*
          * Is this an unexpected signature?
          */
         if (rctx.sig) {
            Jmsg0(jcr, M_ERROR, 0, _("Unexpected cryptographic signature data stream.\n"));
            free_signature(rctx);
            continue;
         }
         /*
          * Save signature.
          */
         if (rctx.extract && (rctx.sig = crypto_sign_decode(jcr, (uint8_t *)sd->msg, (uint32_t)sd->msglen)) == NULL) {
            Jmsg1(jcr, M_ERROR, 0, _("Failed to decode message signature for %s\n"), jcr->last_fname);
         }
         break;

      case STREAM_MD5_DIGEST:
      case STREAM_SHA1_DIGEST:
      case STREAM_SHA256_DIGEST:
      case STREAM_SHA512_DIGEST:
         break;

      case STREAM_PROGRAM_NAMES:
      case STREAM_PROGRAM_DATA:
         if (!non_support_progname) {
            Pmsg0(000, "Got Program Name or Data Stream. Ignored.\n");
            non_support_progname++;
         }
         break;

      case STREAM_PLUGIN_NAME:
         if (!close_previous_stream(jcr, rctx)) {
            goto bail_out;
         }
         Dmsg1(50, "restore stream_plugin_name=%s\n", sd->msg);
         plugin_name_stream(jcr, sd->msg);
         break;

      case STREAM_RESTORE_OBJECT:
         break;                    /* these are sent by Director */

      default:
         if (!close_previous_stream(jcr, rctx)) {
            goto bail_out;
         }
         Jmsg(jcr, M_WARNING, 0, _("Unknown stream=%d ignored. This shouldn't happen!\n"),
              rctx.stream);
         Dmsg2(0, "Unknown stream=%d data=%s\n", rctx.stream, sd->msg);
         break;
      } /* end switch(stream) */
   } /* end while get_msg() */

   /*
    * If output file is still open, it was the last one in the
    * archive since we just hit an end of file, so close the file.
    */
   if (is_bopen(&rctx.forkbfd)) {
      bclose_chksize(jcr, &rctx.forkbfd, rctx.fork_size);
   }

   if (!close_previous_stream(jcr, rctx)) {
      goto bail_out;
   }
   jcr->setJobStatus(JS_Terminated);
   goto ok_out;

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);

ok_out:
   /*
    * First output the statistics.
    */
   Dmsg2(10, "End Do Restore. Files=%d Bytes=%s\n", jcr->JobFiles,
      edit_uint64(jcr->JobBytes, ec1));
   if (have_acl && jcr->acl_data->u.parse->nr_errors > 0) {
      Jmsg(jcr, M_WARNING, 0, _("Encountered %ld acl errors while doing restore\n"),
           jcr->acl_data->u.parse->nr_errors);
   }
   if (have_xattr && jcr->xattr_data->u.parse->nr_errors > 0) {
      Jmsg(jcr, M_WARNING, 0, _("Encountered %ld xattr errors while doing restore\n"),
           jcr->xattr_data->u.parse->nr_errors);
   }
   if (non_support_data > 1 || non_support_attr > 1) {
      Jmsg(jcr, M_WARNING, 0, _("%d non-supported data streams and %d non-supported attrib streams ignored.\n"),
         non_support_data, non_support_attr);
   }
   if (non_support_rsrc) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported resource fork streams ignored.\n"), non_support_rsrc);
   }
   if (non_support_finfo) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported Finder Info streams ignored.\n"), non_support_finfo);
   }
   if (non_support_acl) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported acl streams ignored.\n"), non_support_acl);
   }
   if (non_support_crypto) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported crypto streams ignored.\n"), non_support_acl);
   }
   if (non_support_xattr) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported xattr streams ignored.\n"), non_support_xattr);
   }

   /*
    * Free Signature & Crypto Data
    */
   free_signature(rctx);
   free_session(rctx);
   if (jcr->crypto.digest) {
      crypto_digest_free(jcr->crypto.digest);
      jcr->crypto.digest = NULL;
   }

   /*
    * Free file cipher restore context
    */
   if (rctx.cipher_ctx.cipher) {
      crypto_cipher_free(rctx.cipher_ctx.cipher);
      rctx.cipher_ctx.cipher = NULL;
   }

   if (rctx.cipher_ctx.buf) {
      free_pool_memory(rctx.cipher_ctx.buf);
      rctx.cipher_ctx.buf = NULL;
   }

   /*
    * Free alternate stream cipher restore context
    */
   if (rctx.fork_cipher_ctx.cipher) {
      crypto_cipher_free(rctx.fork_cipher_ctx.cipher);
      rctx.fork_cipher_ctx.cipher = NULL;
   }
   if (rctx.fork_cipher_ctx.buf) {
      free_pool_memory(rctx.fork_cipher_ctx.buf);
      rctx.fork_cipher_ctx.buf = NULL;
   }

   if (jcr->compress_buf) {
      free_pool_memory(jcr->compress_buf);
      jcr->compress_buf = NULL;
      jcr->compress_buf_size = 0;
   }

   if (have_acl && jcr->acl_data) {
      free(jcr->acl_data->u.parse);
      free(jcr->acl_data);
      jcr->acl_data = NULL;
   }

   if (have_xattr && jcr->xattr_data) {
      free(jcr->xattr_data->u.parse);
      free(jcr->xattr_data);
      jcr->xattr_data = NULL;
   }

   /*
    * Free the delayed stream stack list.
    */
   if (rctx.delayed_streams) {
      drop_delayed_restore_streams(rctx, false);
      delete rctx.delayed_streams;
   }

   bclose(&rctx.forkbfd);
   bclose(&rctx.bfd);
   free_attr(rctx.attr);
}

#ifdef HAVE_LIBZ
/*
 * Convert ZLIB error code into an ASCII message
 */
static const char *zlib_strerror(int stat)
{
   if (stat >= 0) {
      return _("None");
   }
   switch (stat) {
   case Z_ERRNO:
      return _("Zlib errno");
   case Z_STREAM_ERROR:
      return _("Zlib stream error");
   case Z_DATA_ERROR:
      return _("Zlib data error");
   case Z_MEM_ERROR:
      return _("Zlib memory error");
   case Z_BUF_ERROR:
      return _("Zlib buffer error");
   case Z_VERSION_ERROR:
      return _("Zlib version error");
   default:
      return _("*none*");
   }
}
#endif

static int do_file_digest(JCR *jcr, FF_PKT *ff_pkt, bool top_level) 
{
   Dmsg1(50, "do_file_digest jcr=%p\n", jcr);
   return (digest_file(jcr, ff_pkt, jcr->crypto.digest));
}

/*
 * Verify the signature for the last restored file
 * Return value is either true (signature correct)
 * or false (signature could not be verified).
 * TODO landonf: Implement without using find_one_file and
 * without re-reading the file.
 */
static bool verify_signature(JCR *jcr, r_ctx &rctx)
{
   X509_KEYPAIR *keypair;
   DIGEST *digest = NULL;
   crypto_error_t err;
   uint64_t saved_bytes;
   crypto_digest_t signing_algorithm = have_sha2 ? 
                                       CRYPTO_DIGEST_SHA256 : CRYPTO_DIGEST_SHA1;
   crypto_digest_t algorithm;
   SIGNATURE *sig = rctx.sig;


   if (!jcr->crypto.pki_sign) {
      /*
       * no signature OK
       */
      return true;
   }
   if (!sig) {
      if (rctx.type == FT_REGE || rctx.type == FT_REG || rctx.type == FT_RAW) { 
         Jmsg1(jcr, M_ERROR, 0, _("Missing cryptographic signature for %s\n"), 
               jcr->last_fname);
         goto bail_out;
      }
      return true;
   }

   /*
    * Iterate through the trusted signers
    */
   foreach_alist(keypair, jcr->crypto.pki_signers) {
      err = crypto_sign_get_digest(sig, jcr->crypto.pki_keypair, algorithm, &digest);
      switch (err) {
      case CRYPTO_ERROR_NONE:
         Dmsg0(50, "== Got digest\n");
         /*
          * We computed jcr->crypto.digest using signing_algorithm while writing
          * the file. If it is not the same as the algorithm used for 
          * this file, punt by releasing the computed algorithm and 
          * computing by re-reading the file.
          */
         if (algorithm != signing_algorithm) {
            if (jcr->crypto.digest) {
               crypto_digest_free(jcr->crypto.digest);
               jcr->crypto.digest = NULL;
            }  
         }
         if (jcr->crypto.digest) {
             /*
              * Use digest computed while writing the file to verify the signature
              */
            if ((err = crypto_sign_verify(sig, keypair, jcr->crypto.digest)) != CRYPTO_ERROR_NONE) {
               Dmsg1(50, "Bad signature on %s\n", jcr->last_fname);
               Jmsg2(jcr, M_ERROR, 0, _("Signature validation failed for file %s: ERR=%s\n"), 
                     jcr->last_fname, crypto_strerror(err));
               goto bail_out;
            }
         } else {   
            /*
             * Signature found, digest allocated.  Old method, 
             * re-read the file and compute the digest
             */
            jcr->crypto.digest = digest;

            /*
             * Checksum the entire file
             * Make sure we don't modify JobBytes by saving and restoring it
             */
            saved_bytes = jcr->JobBytes;                     
            if (find_one_file(jcr, jcr->ff, do_file_digest, jcr->last_fname, (dev_t)-1, 1) != 0) {
               Jmsg(jcr, M_ERROR, 0, _("Digest one file failed for file: %s\n"), 
                    jcr->last_fname);
               jcr->JobBytes = saved_bytes;
               goto bail_out;
            }
            jcr->JobBytes = saved_bytes;

            /*
             * Verify the signature
             */
            if ((err = crypto_sign_verify(sig, keypair, digest)) != CRYPTO_ERROR_NONE) {
               Dmsg1(50, "Bad signature on %s\n", jcr->last_fname);
               Jmsg2(jcr, M_ERROR, 0, _("Signature validation failed for file %s: ERR=%s\n"), 
                     jcr->last_fname, crypto_strerror(err));
               goto bail_out;
            }
            jcr->crypto.digest = NULL;
         }

         /*
          * Valid signature
          */
         Dmsg1(50, "Signature good on %s\n", jcr->last_fname);
         crypto_digest_free(digest);
         return true;

      case CRYPTO_ERROR_NOSIGNER:
         /*
          * Signature not found, try again
          */
         if (digest) {
            crypto_digest_free(digest);
            digest = NULL;
         }
         continue;
      default:
         /*
          * Something strange happened (that shouldn't happen!)...
          */
         Qmsg2(jcr, M_ERROR, 0, _("Signature validation failed for %s: %s\n"), jcr->last_fname, crypto_strerror(err));
         goto bail_out;
      }
   }

   /*
    * No signer
    */
   Dmsg1(50, "Could not find a valid public key for signature on %s\n", jcr->last_fname);

bail_out:
   if (digest) {
      crypto_digest_free(digest);
   }
   return false;
}

bool sparse_data(JCR *jcr, BFILE *bfd, uint64_t *addr, char **data, uint32_t *length)
{
      unser_declare;
      uint64_t faddr;
      char ec1[50];
      unser_begin(*data, OFFSET_FADDR_SIZE);
      unser_uint64(faddr);
      if (*addr != faddr) {
         *addr = faddr;
         if (blseek(bfd, (boffset_t)*addr, SEEK_SET) < 0) {
            berrno be;
            Jmsg3(jcr, M_ERROR, 0, _("Seek to %s error on %s: ERR=%s\n"),
                  edit_uint64(*addr, ec1), jcr->last_fname, 
                  be.bstrerror(bfd->berrno));
            return false;
         }
      }
      *data += OFFSET_FADDR_SIZE;
      *length -= OFFSET_FADDR_SIZE;
      return true;
}

bool decompress_data(JCR *jcr, int32_t stream, char **data, uint32_t *length)
{
#if defined(HAVE_LZO) || defined(HAVE_LIBZ)
   char ec1[50]; /* Buffer printing huge values */
#endif

   Dmsg1(200, "Stream found in decompress_data(): %d\n", stream);
   if(stream == STREAM_COMPRESSED_DATA || stream == STREAM_SPARSE_COMPRESSED_DATA || stream == STREAM_WIN32_COMPRESSED_DATA
       || stream == STREAM_ENCRYPTED_FILE_COMPRESSED_DATA || stream == STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA)
   {
      uint32_t comp_magic, comp_len;
      uint16_t comp_level, comp_version;
#ifdef HAVE_LZO
      lzo_uint compress_len;
      const unsigned char *cbuf;
      int r, real_compress_len;
#endif

      /* read compress header */
      unser_declare;
      unser_begin(*data, sizeof(comp_stream_header));
      unser_uint32(comp_magic);
      unser_uint32(comp_len);
      unser_uint16(comp_level);
      unser_uint16(comp_version);
      Dmsg4(200, "Compressed data stream found: magic=0x%x, len=%d, level=%d, ver=0x%x\n", comp_magic, comp_len,
                              comp_level, comp_version);

      /* version check */
      if (comp_version != COMP_HEAD_VERSION) {
         Qmsg(jcr, M_ERROR, 0, _("Compressed header version error. version=0x%x\n"), comp_version);
         return false;
      }
      /* size check */
      if (comp_len + sizeof(comp_stream_header) != *length) {
         Qmsg(jcr, M_ERROR, 0, _("Compressed header size error. comp_len=%d, msglen=%d\n"),
              comp_len, *length);
         return false;
      }
      switch(comp_magic) {
#ifdef HAVE_LZO
         case COMPRESS_LZO1X:
            compress_len = jcr->compress_buf_size;
            cbuf = (const unsigned char*)*data + sizeof(comp_stream_header);
            real_compress_len = *length - sizeof(comp_stream_header);
            Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
            while ((r=lzo1x_decompress_safe(cbuf, real_compress_len,
                                            (unsigned char *)jcr->compress_buf, &compress_len, NULL)) == LZO_E_OUTPUT_OVERRUN)
            {
               /*
                * The buffer size is too small, try with a bigger one
                */
               compress_len = jcr->compress_buf_size = jcr->compress_buf_size + (jcr->compress_buf_size >> 1);
               Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
               jcr->compress_buf = check_pool_memory_size(jcr->compress_buf,
                                                    compress_len);
            }
            if (r != LZO_E_OK) {
               Qmsg(jcr, M_ERROR, 0, _("LZO uncompression error on file %s. ERR=%d\n"),
                    jcr->last_fname, r);
               return false;
            }
            *data = jcr->compress_buf;
            *length = compress_len;
            Dmsg2(200, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));
            return true;
#endif
         default:
            Qmsg(jcr, M_ERROR, 0, _("Compression algorithm 0x%x found, but not supported!\n"), comp_magic);
            return false;
      }
    } else {
#ifdef HAVE_LIBZ
      uLong compress_len;
      int stat;

      /* 
       * NOTE! We only use uLong and Byte because they are
       * needed by the zlib routines, they should not otherwise
       * be used in Bacula.
       */
      compress_len = jcr->compress_buf_size;
      Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
      while ((stat=uncompress((Byte *)jcr->compress_buf, &compress_len,
                              (const Byte *)*data, (uLong)*length)) == Z_BUF_ERROR)
      {
         /*
          * The buffer size is too small, try with a bigger one
          */
         compress_len = jcr->compress_buf_size = jcr->compress_buf_size + (jcr->compress_buf_size >> 1);
         Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
         jcr->compress_buf = check_pool_memory_size(jcr->compress_buf,
                                                    compress_len);
      }
      if (stat != Z_OK) {
         Qmsg(jcr, M_ERROR, 0, _("Uncompression error on file %s. ERR=%s\n"),
              jcr->last_fname, zlib_strerror(stat));
         return false;
      }
      *data = jcr->compress_buf;
      *length = compress_len;
      Dmsg2(200, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));
      return true;
#else
      Qmsg(jcr, M_ERROR, 0, _("GZIP data stream found, but GZIP not configured!\n"));
      return false;
#endif
   }
}

static void unser_crypto_packet_len(RESTORE_CIPHER_CTX *ctx)
{
   unser_declare;
   if (ctx->packet_len == 0 && ctx->buf_len >= CRYPTO_LEN_SIZE) {
      unser_begin(&ctx->buf[0], CRYPTO_LEN_SIZE);
      unser_uint32(ctx->packet_len);
      ctx->packet_len += CRYPTO_LEN_SIZE;
   }
}

bool store_data(JCR *jcr, BFILE *bfd, char *data, const int32_t length, bool win32_decomp)
{
   if (jcr->crypto.digest) {
      crypto_digest_update(jcr->crypto.digest, (uint8_t *)data, length);
   }
   if (win32_decomp) {
      if (!processWin32BackupAPIBlock(bfd, data, length)) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Write error in Win32 Block Decomposition on %s: %s\n"), 
               jcr->last_fname, be.bstrerror(bfd->berrno));
         return false;
      }
   } else if (bwrite(bfd, data, length) != (ssize_t)length) {
      berrno be;
      Jmsg2(jcr, M_ERROR, 0, _("Write error on %s: %s\n"), 
            jcr->last_fname, be.bstrerror(bfd->berrno));
      return false;
   }

   return true;
}

/*
 * In the context of jcr, write data to bfd.
 * We write buflen bytes in buf at addr. addr is updated in place.
 * The flags specify whether to use sparse files or compression.
 * Return value is the number of bytes written, or -1 on errors.
 */
int32_t extract_data(JCR *jcr, BFILE *bfd, POOLMEM *buf, int32_t buflen,
                     uint64_t *addr, int flags, int32_t stream, RESTORE_CIPHER_CTX *cipher_ctx)
{
   char *wbuf;                 /* write buffer */
   uint32_t wsize;             /* write size */
   uint32_t rsize;             /* read size */
   uint32_t decrypted_len = 0; /* Decryption output length */
   char ec1[50];               /* Buffer printing huge values */

   rsize = buflen;
   jcr->ReadBytes += rsize;
   wsize = rsize;
   wbuf = buf;

   if (flags & FO_ENCRYPT) {
      ASSERT(cipher_ctx->cipher);

      /*
       * NOTE: We must implement block preserving semantics for the
       * non-streaming compression and sparse code.
       *
       * Grow the crypto buffer, if necessary.
       * crypto_cipher_update() will process only whole blocks,
       * buffering the remaining input.
       */
      cipher_ctx->buf = check_pool_memory_size(cipher_ctx->buf, 
                        cipher_ctx->buf_len + wsize + cipher_ctx->block_size);

      /*
       * Decrypt the input block
       */
      if (!crypto_cipher_update(cipher_ctx->cipher, 
                                (const u_int8_t *)wbuf, 
                                wsize, 
                                (u_int8_t *)&cipher_ctx->buf[cipher_ctx->buf_len], 
                                &decrypted_len)) {
         /*
          * Decryption failed. Shouldn't happen.
          */
         Jmsg(jcr, M_FATAL, 0, _("Decryption error\n"));
         goto bail_out;
      }

      if (decrypted_len == 0) {
         /*
          * No full block of encrypted data available, write more data
          */
         return 0;
      }

      Dmsg2(200, "decrypted len=%d encrypted len=%d\n", decrypted_len, wsize);

      cipher_ctx->buf_len += decrypted_len;
      wbuf = cipher_ctx->buf;

      /*
       * If one full preserved block is available, write it to disk,
       * and then buffer any remaining data. This should be effecient
       * as long as Bacula's block size is not significantly smaller than the
       * encryption block size (extremely unlikely!)
       */
      unser_crypto_packet_len(cipher_ctx);
      Dmsg1(500, "Crypto unser block size=%d\n", cipher_ctx->packet_len - CRYPTO_LEN_SIZE);

      if (cipher_ctx->packet_len == 0 || cipher_ctx->buf_len < cipher_ctx->packet_len) {
         /*
          * No full preserved block is available.
          */
         return 0;
      }

      /*
       * We have one full block, set up the filter input buffers
       */
      wsize = cipher_ctx->packet_len - CRYPTO_LEN_SIZE;
      wbuf = &wbuf[CRYPTO_LEN_SIZE]; /* Skip the block length header */
      cipher_ctx->buf_len -= cipher_ctx->packet_len;
      Dmsg2(130, "Encryption writing full block, %u bytes, remaining %u bytes in buffer\n", wsize, cipher_ctx->buf_len);
   }

   if ((flags & FO_SPARSE) || (flags & FO_OFFSETS)) {
      if (!sparse_data(jcr, bfd, addr, &wbuf, &wsize)) {
         goto bail_out;
      }
   }

   if (flags & FO_COMPRESS) {
      if (!decompress_data(jcr, stream, &wbuf, &wsize)) {
         goto bail_out;
      }
   }

   if (!store_data(jcr, bfd, wbuf, wsize, (flags & FO_WIN32DECOMP) != 0)) {
      goto bail_out;
   }
   jcr->JobBytes += wsize;
   *addr += wsize;
   Dmsg2(130, "Write %u bytes, JobBytes=%s\n", wsize, edit_uint64(jcr->JobBytes, ec1));

   /*
    * Clean up crypto buffers
    */
   if (flags & FO_ENCRYPT) {
      /* Move any remaining data to start of buffer */
      if (cipher_ctx->buf_len > 0) {
         Dmsg1(130, "Moving %u buffered bytes to start of buffer\n", cipher_ctx->buf_len);
         memmove(cipher_ctx->buf, &cipher_ctx->buf[cipher_ctx->packet_len], 
            cipher_ctx->buf_len);
      }
      /*
       * The packet was successfully written, reset the length so that the next
       * packet length may be re-read by unser_crypto_packet_len()
       */
      cipher_ctx->packet_len = 0;
   }
   return wsize;

bail_out:
   return -1;
}

/*
 * If extracting, close any previous stream
 */
static bool close_previous_stream(JCR *jcr, r_ctx &rctx)
{
   /*
    * If extracting, it was from previous stream, so
    * close the output file and validate the signature.
    */
   if (rctx.extract) {
      if (rctx.size > 0 && !is_bopen(&rctx.bfd)) {
         Jmsg0(rctx.jcr, M_ERROR, 0, _("Logic error: output file should be open\n"));
         Dmsg2(000, "=== logic error size=%d bopen=%d\n", rctx.size, 
            is_bopen(&rctx.bfd));
      }

      if (rctx.prev_stream != STREAM_ENCRYPTED_SESSION_DATA) {
         deallocate_cipher(rctx);
         deallocate_fork_cipher(rctx);
      }

      if (rctx.jcr->plugin) {
         plugin_set_attributes(rctx.jcr, rctx.attr, &rctx.bfd);
      } else {
         set_attributes(rctx.jcr, rctx.attr, &rctx.bfd);
      }
      rctx.extract = false;

      /*
       * Now perform the delayed restore of some specific data streams.
       */
      if (!pop_delayed_data_streams(jcr, rctx)) {
         return false;
      }

      /*
       * Verify the cryptographic signature, if any
       */
      rctx.type = rctx.attr->type;
      verify_signature(rctx.jcr, rctx);

      /*
       * Free Signature
       */
      free_signature(rctx);
      free_session(rctx);
      rctx.jcr->ff->flags = 0;
      Dmsg0(130, "Stop extracting.\n");
   } else if (is_bopen(&rctx.bfd)) {
      Jmsg0(rctx.jcr, M_ERROR, 0, _("Logic error: output file should not be open\n"));
      Dmsg0(000, "=== logic error !open\n");
      bclose(&rctx.bfd);
   }

   return true;
}

/*
 * In the context of jcr, flush any remaining data from the cipher context,
 * writing it to bfd.
 * Return value is true on success, false on failure.
 */
bool flush_cipher(JCR *jcr, BFILE *bfd, uint64_t *addr, int flags, int32_t stream,
                  RESTORE_CIPHER_CTX *cipher_ctx)
{
   uint32_t decrypted_len = 0;
   char *wbuf;                        /* write buffer */
   uint32_t wsize;                    /* write size */
   char ec1[50];                      /* Buffer printing huge values */
   bool second_pass = false;

again:
   /*
    * Write out the remaining block and free the cipher context
    */
   cipher_ctx->buf = check_pool_memory_size(cipher_ctx->buf, cipher_ctx->buf_len + 
                                            cipher_ctx->block_size);

   if (!crypto_cipher_finalize(cipher_ctx->cipher, (uint8_t *)&cipher_ctx->buf[cipher_ctx->buf_len],
        &decrypted_len)) {
      /*
       * Writing out the final, buffered block failed. Shouldn't happen.
       */
      Jmsg3(jcr, M_ERROR, 0, _("Decryption error. buf_len=%d decrypt_len=%d on file %s\n"), 
            cipher_ctx->buf_len, decrypted_len, jcr->last_fname);
   }

   Dmsg2(130, "Flush decrypt len=%d buf_len=%d\n", decrypted_len, cipher_ctx->buf_len);
   /*
    * If nothing new was decrypted, and our output buffer is empty, return
    */
   if (decrypted_len == 0 && cipher_ctx->buf_len == 0) {
      return true;
   }

   cipher_ctx->buf_len += decrypted_len;

   unser_crypto_packet_len(cipher_ctx);
   Dmsg1(500, "Crypto unser block size=%d\n", cipher_ctx->packet_len - CRYPTO_LEN_SIZE);
   wsize = cipher_ctx->packet_len - CRYPTO_LEN_SIZE;
   /*
    * Decrypted, possibly decompressed output here.
    */
   wbuf = &cipher_ctx->buf[CRYPTO_LEN_SIZE];
   cipher_ctx->buf_len -= cipher_ctx->packet_len;
   Dmsg2(130, "Encryption writing full block, %u bytes, remaining %u bytes in buffer\n", wsize, cipher_ctx->buf_len);

   if ((flags & FO_SPARSE) || (flags & FO_OFFSETS)) {
      if (!sparse_data(jcr, bfd, addr, &wbuf, &wsize)) {
         return false;
      }
   }

   if (flags & FO_COMPRESS) {
      if (!decompress_data(jcr, stream, &wbuf, &wsize)) {
         return false;
      }
   }

   Dmsg0(130, "Call store_data\n");
   if (!store_data(jcr, bfd, wbuf, wsize, (flags & FO_WIN32DECOMP) != 0)) {
      return false;
   }
   jcr->JobBytes += wsize;
   Dmsg2(130, "Flush write %u bytes, JobBytes=%s\n", wsize, edit_uint64(jcr->JobBytes, ec1));

   /*
    * Move any remaining data to start of buffer
    */
   if (cipher_ctx->buf_len > 0) {
      Dmsg1(130, "Moving %u buffered bytes to start of buffer\n", cipher_ctx->buf_len);
      memmove(cipher_ctx->buf, &cipher_ctx->buf[cipher_ctx->packet_len], 
         cipher_ctx->buf_len);
   }
   /*
    * The packet was successfully written, reset the length so that the next
    * packet length may be re-read by unser_crypto_packet_len()
    */
   cipher_ctx->packet_len = 0;

   if (cipher_ctx->buf_len >0 && !second_pass) {
      second_pass = true;
      goto again;
   }

   /*
    * Stop decryption
    */
   cipher_ctx->buf_len = 0;
   cipher_ctx->packet_len = 0;

   return true;
}

static void deallocate_cipher(r_ctx &rctx)
{
   /*
    * Flush and deallocate previous stream's cipher context
    */
   if (rctx.cipher_ctx.cipher) {
      flush_cipher(rctx.jcr, &rctx.bfd, &rctx.fileAddr, rctx.flags, rctx.comp_stream, &rctx.cipher_ctx);
      crypto_cipher_free(rctx.cipher_ctx.cipher);
      rctx.cipher_ctx.cipher = NULL;
   }
}

static void deallocate_fork_cipher(r_ctx &rctx)
{

   /*
    * Flush and deallocate previous stream's fork cipher context
    */
   if (rctx.fork_cipher_ctx.cipher) {
      flush_cipher(rctx.jcr, &rctx.forkbfd, &rctx.fork_addr, rctx.fork_flags, rctx.comp_stream, &rctx.fork_cipher_ctx);
      crypto_cipher_free(rctx.fork_cipher_ctx.cipher);
      rctx.fork_cipher_ctx.cipher = NULL;
   }
}

static void free_signature(r_ctx &rctx)
{
   if (rctx.sig) {
      crypto_sign_free(rctx.sig);
      rctx.sig = NULL;
   }
}

static void free_session(r_ctx &rctx)
{
   if (rctx.cs) {
      crypto_session_free(rctx.cs);
      rctx.cs = NULL;
   }
}


/*
 * This code if implemented goes above
 */
#ifdef stbernard_implemented
/  #if defined(HAVE_WIN32)
   bool        bResumeOfmOnExit = FALSE;
   if (isOpenFileManagerRunning()) {
       if ( pauseOpenFileManager() ) {
          Jmsg(jcr, M_INFO, 0, _("Open File Manager paused\n") );
          bResumeOfmOnExit = TRUE;
       }
       else {
          Jmsg(jcr, M_ERROR, 0, _("FAILED to pause Open File Manager\n") );
       }
   }
   {
       char username[UNLEN+1];
       DWORD usize = sizeof(username);
       int privs = enable_backup_privileges(NULL, 1);
       if (GetUserName(username, &usize)) {
          Jmsg2(jcr, M_INFO, 0, _("Running as '%s'. Privmask=%#08x\n"), username,
       } else {
          Jmsg(jcr, M_WARNING, 0, _("Failed to retrieve current UserName\n"));
       }
   }
#endif
