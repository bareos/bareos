/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, July MMII
 */
/**
 * @file
 * Verify files on a Volume
 * versus attributes in Catalog
 */

#include "bareos.h"
#include "filed.h"

/* Data received from Storage Daemon */
static char rec_header[] =
   "rechdr %ld %ld %ld %ld %ld";

/* Forward referenced functions */


/**
 * Verify attributes of the requested files on the Volume
 *
 */
void do_verify_volume(JCR *jcr)
{
   BSOCK *sd, *dir;
   POOLMEM *fname;                    /* original file name */
   POOLMEM *lname;                    /* link name */
   int32_t stream;
   uint32_t size;
   uint32_t VolSessionId, VolSessionTime, file_index;
   uint32_t record_file_index;
   char digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];
   int type, status;

   sd = jcr->store_bsock;
   if (!sd) {
      Jmsg(jcr, M_FATAL, 0, _("Storage command not issued before Verify.\n"));
      jcr->setJobStatus(JS_FatalError);
      return;
   }
   dir = jcr->dir_bsock;
   jcr->setJobStatus(JS_Running);

   LockRes();
   CLIENTRES *client = (CLIENTRES *)GetNextRes(R_CLIENT, NULL);
   UnlockRes();
   uint32_t buf_size;
   if (client) {
      buf_size = client->max_network_buffer_size;
   } else {
      buf_size = 0;                   /* use default */
   }
   if (!bnet_set_buffer_size(sd, buf_size, BNET_SETBUF_WRITE)) {
      jcr->setJobStatus(JS_FatalError);
      return;
   }
   jcr->buf_size = sd->msglen;

   fname = get_pool_memory(PM_FNAME);
   lname = get_pool_memory(PM_FNAME);

   /*
    * Get a record from the Storage daemon
    */
   while (bget_msg(sd) >= 0 && !job_canceled(jcr)) {
      /*
       * First we expect a Stream Record Header
       */
      if (sscanf(sd->msg, rec_header, &VolSessionId, &VolSessionTime, &file_index,
          &stream, &size) != 5) {
         Jmsg1(jcr, M_FATAL, 0, _("Record header scan error: %s\n"), sd->msg);
         goto bail_out;
      }
      Dmsg2(30, "Got hdr: FilInx=%d Stream=%d.\n", file_index, stream);

      /*
       * Now we expect the Stream Data
       */
      if (bget_msg(sd) < 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Data record error. ERR=%s\n"), bnet_strerror(sd));
         goto bail_out;
      }
      if (size != ((uint32_t)sd->msglen)) {
         Jmsg2(jcr, M_FATAL, 0, _("Actual data size %d not same as header %d\n"), sd->msglen, size);
         goto bail_out;
      }
      Dmsg1(30, "Got stream data, len=%d\n", sd->msglen);

      /* File Attributes stream */
      switch (stream) {
      case STREAM_UNIX_ATTRIBUTES:
      case STREAM_UNIX_ATTRIBUTES_EX:
         char *ap, *lp, *fp;

         Dmsg0(400, "Stream=Unix Attributes.\n");

         if ((int)sizeof_pool_memory(fname) < sd->msglen) {
            fname = realloc_pool_memory(fname, sd->msglen + 1);
         }

         if ((int)sizeof_pool_memory(lname) < sd->msglen) {
            lname = realloc_pool_memory(lname, sd->msglen + 1);
         }
         *fname = 0;
         *lname = 0;

         /*
          * An Attributes record consists of:
          *    File_index
          *    Type   (FT_types)
          *    Filename
          *    Attributes
          *    Link name (if file linked i.e. FT_LNK)
          *    Extended Attributes (if Win32)
          */
         if (sscanf(sd->msg, "%d %d", &record_file_index, &type) != 2) {
            Jmsg(jcr, M_FATAL, 0, _("Error scanning record header: %s\n"), sd->msg);
            Dmsg0(0, "\nError scanning header\n");
            goto bail_out;
         }
         Dmsg2(30, "Got Attr: FilInx=%d type=%d\n", record_file_index, type);
         ap = sd->msg;
         while (*ap++ != ' ')         /* skip record file index */
            ;
         while (*ap++ != ' ')         /* skip type */
            ;
         /* Save filename and position to attributes */
         fp = fname;
         while (*ap != 0) {
            *fp++  = *ap++;           /* copy filename to fname */
         }
         *fp = *ap++;                 /* terminate filename & point to attribs */

         Dmsg1(200, "Attr=%s\n", ap);
         /* Skip to Link name */
         if (type == FT_LNK || type == FT_LNKSAVED) {
            lp = ap;
            while (*lp++ != 0) {
               ;
            }
            pm_strcat(lname, lp);        /* "save" link name */
         } else {
            *lname = 0;
         }
         jcr->lock();
         jcr->JobFiles++;
         jcr->num_files_examined++;
         pm_strcpy(jcr->last_fname, fname); /* last file examined */
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
         /* Send file attributes to Director */
         Dmsg2(200, "send ATTR inx=%d fname=%s\n", jcr->JobFiles, fname);
         if (type == FT_LNK || type == FT_LNKSAVED) {
            status = dir->fsend("%d %d %s %s%c%s%c%s%c", jcr->JobFiles,
                                STREAM_UNIX_ATTRIBUTES, "pinsug5", fname,
                                0, ap, 0, lname, 0);
         /* for a deleted record, we set fileindex=0 */
         } else if (type == FT_DELETED)  {
            status = dir->fsend("%d %d %s %s%c%s%c%c", 0,
                                STREAM_UNIX_ATTRIBUTES, "pinsug5", fname,
                                0, ap, 0, 0);
         } else {
            status = dir->fsend("%d %d %s %s%c%s%c%c", jcr->JobFiles,
                                STREAM_UNIX_ATTRIBUTES, "pinsug5", fname,
                                0, ap, 0, 0);
         }
         Dmsg2(200, "filed>dir: attribs len=%d: msg=%s\n", dir->msglen, dir->msg);
         if (!status) {
            Jmsg(jcr, M_FATAL, 0, _("Network error in send to Director: ERR=%s\n"), bnet_strerror(dir));
            goto bail_out;
         }
         break;

      case STREAM_MD5_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_MD5_SIZE, true);
         Dmsg2(400, "send inx=%d MD5=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *MD5-%d*", jcr->JobFiles, STREAM_MD5_DIGEST, digest,
                    jcr->JobFiles);
         Dmsg2(20, "filed>dir: MD5 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      case STREAM_SHA1_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_SHA1_SIZE, true);
         Dmsg2(400, "send inx=%d SHA1=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *SHA1-%d*", jcr->JobFiles, STREAM_SHA1_DIGEST,
                    digest, jcr->JobFiles);
         Dmsg2(20, "filed>dir: SHA1 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      case STREAM_SHA256_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_SHA256_SIZE, true);
         Dmsg2(400, "send inx=%d SHA256=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *SHA256-%d*", jcr->JobFiles, STREAM_SHA256_DIGEST,
                    digest, jcr->JobFiles);
         Dmsg2(20, "filed>dir: SHA256 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      case STREAM_SHA512_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_SHA512_SIZE, true);
         Dmsg2(400, "send inx=%d SHA512=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *SHA512-%d*", jcr->JobFiles, STREAM_SHA512_DIGEST,
                    digest, jcr->JobFiles);
         Dmsg2(20, "filed>dir: SHA512 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      case STREAM_RESTORE_OBJECT:
         jcr->lock();
         jcr->JobFiles++;
         jcr->num_files_examined++;
         jcr->unlock();

         Dmsg2(400, "send inx=%d STREAM_RESTORE_OBJECT-%d\n", jcr->JobFiles, STREAM_RESTORE_OBJECT);
         dir->fsend("%d %d %s %s%c%c%c", jcr->JobFiles,
                    STREAM_RESTORE_OBJECT, "ReStOrEObJeCt", fname, 0, 0, 0);
         break;

      /* Ignore everything else */
      default:
         break;

      } /* end switch */
   } /* end while bnet_get */
   jcr->setJobStatus(JS_Terminated);
   goto ok_out;

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);

ok_out:
   cleanup_compression(jcr);

   free_pool_memory(fname);
   free_pool_memory(lname);
   Dmsg2(050, "End Verify-Vol. Files=%d Bytes=%" lld "\n", jcr->JobFiles, jcr->JobBytes);
}
