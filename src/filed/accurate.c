/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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

#include "bareos.h"
#include "filed.h"
#include "accurate.h"

static int dbglvl = 100;

bool accurate_mark_file_as_seen(JCR *jcr, char *fname)
{
   accurate_payload *temp;

   if (!jcr->accurate || !jcr->file_list) {
      return false;
   }

   temp = jcr->file_list->lookup_payload(jcr, fname);
   if (temp) {
      jcr->file_list->mark_file_as_seen(jcr, temp);
      Dmsg1(dbglvl, "marked <%s> as seen\n", fname);
   } else {
      Dmsg1(dbglvl, "<%s> not found to be marked as seen\n", fname);
   }

   return true;
}

static inline bool accurate_lookup(JCR *jcr, char *fname, accurate_payload **payload)
{
   bool found = false;

   *payload = jcr->file_list->lookup_payload(jcr, fname);
   if (*payload) {
      found = true;
      Dmsg1(dbglvl, "lookup <%s> ok\n", fname);
   }

   return found;
}

void accurate_free(JCR *jcr)
{
   if (jcr->file_list) {
      jcr->file_list->destroy(jcr);
      delete jcr->file_list;
      jcr->file_list = NULL;
   }
}

/*
 * Send the deleted or the base file list and cleanup.
 */
bool accurate_finish(JCR *jcr)
{
   bool retval = true;

   if (jcr->is_canceled() || jcr->is_incomplete()) {
      accurate_free(jcr);
      return retval;
   }

   if (jcr->accurate && jcr->file_list) {
      if (jcr->is_JobLevel(L_FULL)) {
         if (!jcr->rerunning) {
            retval = jcr->file_list->send_base_file_list(jcr);
         }
      } else {
         retval = jcr->file_list->send_deleted_list(jcr);
      }

      accurate_free(jcr);
      if (jcr->is_JobLevel(L_FULL)) {
         Jmsg(jcr, M_INFO, 0, _("Space saved with Base jobs: %lld MB\n"), jcr->base_size / (1024 * 1024));
      }
   }

   return retval;
}

/*
 * This function is called for each file seen in fileset.
 * We check in file_list hash if fname have been backuped
 * the last time. After we can compare Lstat field.
 * Full Lstat usage have been removed on 6612
 *
 * Returns: true   if file has changed (must be backed up)
 *          false  file not changed
 */
bool accurate_check_file(JCR *jcr, FF_PKT *ff_pkt)
{
   char *opts;
   char *fname;
   int32_t LinkFIc;
   struct stat statc;
   int digest_stream;
   bool status = false;
   accurate_payload *payload;

   digest_stream = STREAM_NONE;
   ff_pkt->delta_seq = 0;
   ff_pkt->accurate_found = false;

   if (!jcr->accurate && !jcr->rerunning) {
      return true;
   }

   if (!jcr->file_list) {
      return true;              /* Not initialized properly */
   }

   strip_path(ff_pkt);

   if (S_ISDIR(ff_pkt->statp.st_mode)) {
      fname = ff_pkt->link;
   } else {
      fname = ff_pkt->fname;
   }

   if (!accurate_lookup(jcr, fname, &payload)) {
      Dmsg1(dbglvl, "accurate %s (not found)\n", fname);
      status = true;
      goto bail_out;
   }

   ff_pkt->accurate_found = true;
   ff_pkt->delta_seq = payload->delta_seq;

   decode_stat(payload->lstat, &statc, sizeof(statc), &LinkFIc); /* decode catalog stat */

   if (!jcr->rerunning && (jcr->getJobLevel() == L_FULL)) {
      opts = ff_pkt->BaseJobOpts;
   } else {
      opts = ff_pkt->AccurateOpts;
   }

   /*
    * Loop over options supplied by user and verify the
    * fields he requests.
    */
   for (char *p = opts; !status && *p; p++) {
      char ed1[30], ed2[30];
      switch (*p) {
      case 'i':                /* compare INODEs */
         if (statc.st_ino != ff_pkt->statp.st_ino) {
            Dmsg3(dbglvl-1, "%s      st_ino   differ. Cat: %s File: %s\n",
                  fname,
                  edit_uint64((uint64_t)statc.st_ino, ed1),
                  edit_uint64((uint64_t)ff_pkt->statp.st_ino, ed2));
            status = true;
         }
         break;
      case 'p':                /* permissions bits */
         /*
          * TODO: If something change only in perm, user, group
          * Backup only the attribute stream
          */
         if (statc.st_mode != ff_pkt->statp.st_mode) {
            Dmsg3(dbglvl-1, "%s     st_mode  differ. Cat: %x File: %x\n",
                  fname,
                  (uint32_t)statc.st_mode, (uint32_t)ff_pkt->statp.st_mode);
            status = true;
         }
         break;
      case 'n':                /* number of links */
         if (statc.st_nlink != ff_pkt->statp.st_nlink) {
            Dmsg3(dbglvl-1, "%s      st_nlink differ. Cat: %d File: %d\n",
                  fname,
                  (uint32_t)statc.st_nlink, (uint32_t)ff_pkt->statp.st_nlink);
            status = true;
         }
         break;
      case 'u':                /* user id */
         if (statc.st_uid != ff_pkt->statp.st_uid) {
            Dmsg3(dbglvl-1, "%s      st_uid   differ. Cat: %u File: %u\n",
                  fname,
                  (uint32_t)statc.st_uid, (uint32_t)ff_pkt->statp.st_uid);
            status = true;
         }
         break;
      case 'g':                /* group id */
         if (statc.st_gid != ff_pkt->statp.st_gid) {
            Dmsg3(dbglvl-1, "%s      st_gid   differ. Cat: %u File: %u\n",
                  fname,
                  (uint32_t)statc.st_gid, (uint32_t)ff_pkt->statp.st_gid);
            status = true;
         }
         break;
      case 's':                /* size */
         if (statc.st_size != ff_pkt->statp.st_size) {
            Dmsg3(dbglvl-1, "%s      st_size  differ. Cat: %s File: %s\n",
                  fname,
                  edit_uint64((uint64_t)statc.st_size, ed1),
                  edit_uint64((uint64_t)ff_pkt->statp.st_size, ed2));
            status = true;
         }
         break;
      case 'a':                /* access time */
         if (statc.st_atime != ff_pkt->statp.st_atime) {
            Dmsg1(dbglvl-1, "%s      st_atime differs\n", fname);
            status = true;
         }
         break;
      case 'm':                 /* modification time */
         if (statc.st_mtime != ff_pkt->statp.st_mtime) {
            Dmsg1(dbglvl-1, "%s      st_mtime differs\n", fname);
            status = true;
         }
         break;
      case 'c':                /* ctime */
         if (statc.st_ctime != ff_pkt->statp.st_ctime) {
            Dmsg1(dbglvl-1, "%s      st_ctime differs\n", fname);
            status = true;
         }
         break;
      case 'd':                /* file size decrease */
         if (statc.st_size > ff_pkt->statp.st_size) {
            Dmsg3(dbglvl-1, "%s      st_size  decrease. Cat: %s File: %s\n",
                  fname,
                  edit_uint64((uint64_t)statc.st_size, ed1),
                  edit_uint64((uint64_t)ff_pkt->statp.st_size, ed2));
            status = true;
         }
         break;
      case 'A':                 /* Always backup a file */
         status = true;
         break;
      /*
       * TODO: cleanup and factorise this function with verify.c
       */
      case '5':                /* compare MD5 */
      case '1':                /* compare SHA1 */
        /*
          * The remainder of the function is all about getting the checksum.
          * First we initialise, then we read files, other streams and Finder Info.
          */
         if (!status && ff_pkt->type != FT_LNKSAVED &&
             (S_ISREG(ff_pkt->statp.st_mode) &&
             ff_pkt->flags & (FO_MD5 | FO_SHA1 | FO_SHA256 | FO_SHA512))) {

            if (!*payload->chksum && !jcr->rerunning) {
               Jmsg(jcr, M_WARNING, 0, _("Cannot verify checksum for %s\n"), ff_pkt->fname);
               status = true;
               break;
            }

            /*
             * Create our digest context. If this fails, the digest will be set
             * to NULL and not used.
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

            /*
             * Did digest initialization fail?
             */
            if (digest_stream != STREAM_NONE && digest == NULL) {
               Jmsg(jcr, M_WARNING, 0, _("%s digest initialization failed\n"),
                    stream_to_ascii(digest_stream));
            }

            /*
             * Compute MD5 or SHA1 hash
             */
            if (digest) {
               char md[CRYPTO_DIGEST_MAX_SIZE];
               uint32_t size;

               size = sizeof(md);

               if (digest_file(jcr, ff_pkt, digest) != 0) {
                  jcr->JobErrors++;

               } else if (crypto_digest_finalize(digest, (uint8_t *)md, &size)) {
                  char *digest_buf;
                  const char *digest_name;

                  digest_buf = (char *)malloc(BASE64_SIZE(size));
                  digest_name = crypto_digest_name(digest);

                  bin_to_base64(digest_buf, BASE64_SIZE(size), md, size, true);

                  if (!bstrcmp(digest_buf, payload.chksum)) {
                     Dmsg4(dbglvl,"%s      %s chksum  diff. Cat: %s File: %s\n",
                           fname,
                           digest_name,
                           payload.chksum,
                           digest_buf);
                     status = true;
                  }

                  free(digest_buf);
               }
               crypto_digest_free(digest);
            }
         }

         break;
      case ':':
      case 'J':
      case 'C':
      default:
         break;
      }
   }

   /*
    * In Incr/Diff accurate mode, we mark all files as seen
    * When in Full+Base mode, we mark only if the file match exactly
    */
   if (jcr->getJobLevel() == L_FULL) {
      if (!status) {
         /*
          * Compute space saved with basefile.
          */
         jcr->base_size += ff_pkt->statp.st_size;
         jcr->file_list->mark_file_as_seen(jcr, payload);
      }
   } else {
      jcr->file_list->mark_file_as_seen(jcr, payload);
   }

bail_out:
   unstrip_path(ff_pkt);
   return status;
}

bool accurate_cmd(JCR *jcr)
{
   uint32_t nb;
   int fname_length,
       lstat_length,
       chksum_length;
   char *fname, *lstat, *chksum;
   uint16_t delta_seq;
   BSOCK *dir = jcr->dir_bsock;

   if (job_canceled(jcr)) {
      return true;
   }

   if (sscanf(dir->msg, "accurate files=%u", &nb) != 1) {
      dir->fsend(_("2991 Bad accurate command\n"));
      return false;
   }

#ifdef HAVE_LMDB
   if (me->always_use_lmdb) {
      jcr->file_list = New(B_ACCURATE_LMDB);
   } else {
      if (me->lmdb_threshold > 0 && nb >= me->lmdb_threshold) {
         jcr->file_list = New(B_ACCURATE_LMDB);
      } else {
         jcr->file_list = New(B_ACCURATE_HTABLE);
      }
   }
#else
   jcr->file_list = New(B_ACCURATE_HTABLE);
#endif

   jcr->file_list->init(jcr, nb);
   jcr->accurate = true;

   /*
    * dirmsg = fname + \0 + lstat + \0 + checksum + \0 + delta_seq + \0
    */
   while (dir->recv() >= 0) {
      fname = dir->msg;
      fname_length = strlen(fname);
      lstat = dir->msg + fname_length + 1;
      lstat_length = strlen(lstat);

      /*
       * No checksum.
       */
      if ((fname_length + lstat_length + 2) >= dir->msglen) {
         chksum = NULL;
         chksum_length = 0;
         delta_seq = 0;
      } else {
         chksum = lstat + lstat_length + 1;
         chksum_length = strlen(chksum);
         delta_seq = str_to_int32(chksum + chksum_length + 1);

         /*
          * Sanity check total length of the received msg must be at least
          * total of the 3 lengths calculcated + 3 (\0)
          */
         if ((fname_length + lstat_length + chksum_length + 3) > dir->msglen) {
            continue;
         }
      }

      jcr->file_list->add_file(jcr, fname, fname_length, lstat, lstat_length,
                               chksum, chksum_length, delta_seq);
   }

   if (!jcr->file_list->end_load(jcr)) {
      return false;
   }

   return true;
}
