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
/*
 *  Version $Id $
 *
 */

#include "bacula.h"
#include "filed.h"

static int dbglvl=100;

typedef struct PrivateCurFile {
   hlink link;
   char *fname;
   char *lstat;
   char *chksum;
   int32_t delta_seq;
   bool seen;
} CurFile;

bool accurate_mark_file_as_seen(JCR *jcr, char *fname)
{
   if (!jcr->accurate || !jcr->file_list) {
      return false;
   }
   /* TODO: just use elt->seen = 1 */
   CurFile *temp = (CurFile *)jcr->file_list->lookup(fname);
   if (temp) {
      temp->seen = 1;              /* records are in memory */
      Dmsg1(dbglvl, "marked <%s> as seen\n", fname);
   } else {
      Dmsg1(dbglvl, "<%s> not found to be marked as seen\n", fname);
   }
   return true;
}

static bool accurate_mark_file_as_seen(JCR *jcr, CurFile *elt)
{
   /* TODO: just use elt->seen = 1 */
   CurFile *temp = (CurFile *)jcr->file_list->lookup(elt->fname);
   if (temp) {
      temp->seen = 1;              /* records are in memory */
   }
   return true;
}

static bool accurate_lookup(JCR *jcr, char *fname, CurFile *ret)
{
   bool found=false;
   ret->seen = 0;

   CurFile *temp = (CurFile *)jcr->file_list->lookup(fname);
   if (temp) {
      memcpy(ret, temp, sizeof(CurFile));
      found=true;
      Dmsg1(dbglvl, "lookup <%s> ok\n", fname);
   }

   return found;
}

static bool accurate_init(JCR *jcr, int nbfile)
{
   CurFile *elt = NULL;
   jcr->file_list = (htable *)malloc(sizeof(htable));
   jcr->file_list->init(elt, &elt->link, nbfile);
   return true;
}

static bool accurate_send_base_file_list(JCR *jcr)
{
   CurFile *elt;
   struct stat statc;
   int32_t LinkFIc;
   FF_PKT *ff_pkt;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate || jcr->getJobLevel() != L_FULL) {
      return true;
   }

   if (jcr->file_list == NULL) {
      return true;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_BASE;

   foreach_htable(elt, jcr->file_list) {
      if (elt->seen) {
         Dmsg2(dbglvl, "base file fname=%s seen=%i\n", elt->fname, elt->seen);
         /* TODO: skip the decode and use directly the lstat field */
         decode_stat(elt->lstat, &statc, sizeof(statc), &LinkFIc); /* decode catalog stat */  
         ff_pkt->fname = elt->fname;
         ff_pkt->statp = statc;
         encode_and_send_attributes(jcr, ff_pkt, stream);
//       free(elt->fname);
      }
   }

   term_find_files(ff_pkt);
   return true;
}


/* This function is called at the end of backup
 * We walk over all hash disk element, and we check
 * for elt.seen.
 */
static bool accurate_send_deleted_list(JCR *jcr)
{
   CurFile *elt;
   struct stat statc;
   int32_t LinkFIc;
   FF_PKT *ff_pkt;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate) {
      return true;
   }

   if (jcr->file_list == NULL) {
      return true;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_DELETED;

   foreach_htable(elt, jcr->file_list) {
      if (elt->seen || plugin_check_file(jcr, elt->fname)) {
         continue;
      }
      Dmsg2(dbglvl, "deleted fname=%s seen=%i\n", elt->fname, elt->seen);
      /* TODO: skip the decode and use directly the lstat field */
      decode_stat(elt->lstat, &statc, sizeof(statc), &LinkFIc); /* decode catalog stat */
      ff_pkt->fname = elt->fname;
      ff_pkt->statp.st_mtime = statc.st_mtime;
      ff_pkt->statp.st_ctime = statc.st_ctime;
      encode_and_send_attributes(jcr, ff_pkt, stream);
//    free(elt->fname);
   }

   term_find_files(ff_pkt);
   return true;
}

void accurate_free(JCR *jcr)
{
   if (jcr->file_list) {
      jcr->file_list->destroy();
      free(jcr->file_list);
      jcr->file_list = NULL;
   }
}

/* Send the deleted or the base file list and cleanup  */
bool accurate_finish(JCR *jcr)
{
   bool ret = true;

   if (jcr->is_canceled() || jcr->is_incomplete()) {
      accurate_free(jcr);
      return ret;
   }
   if (jcr->accurate) {
      if (jcr->is_JobLevel(L_FULL)) {
         if (!jcr->rerunning) {
            ret = accurate_send_base_file_list(jcr);
         }
      } else {
         ret = accurate_send_deleted_list(jcr);
      }
      accurate_free(jcr);
      if (jcr->is_JobLevel(L_FULL)) {
         Jmsg(jcr, M_INFO, 0, _("Space saved with Base jobs: %lld MB\n"), 
              jcr->base_size/(1024*1024));
      }
   }
   return ret;
}

static bool accurate_add_file(JCR *jcr, uint32_t len, 
                              char *fname, char *lstat, char *chksum,
                              int32_t delta)
{
   bool ret = true;
   CurFile *item;

   /* we store CurFile, fname and ctime/mtime in the same chunk 
    * we need one extra byte to handle an empty chksum
    */
   item = (CurFile *)jcr->file_list->hash_malloc(sizeof(CurFile)+len+3);
   item->seen = 0;

   /* TODO: see if we can optimize this part with memcpy instead of strcpy */
   item->fname  = (char *)item+sizeof(CurFile);
   strcpy(item->fname, fname);

   item->lstat  = item->fname+strlen(item->fname)+1;
   strcpy(item->lstat, lstat);

   item->chksum = item->lstat+strlen(item->lstat)+1;
   strcpy(item->chksum, chksum);

   item->delta_seq = delta;

   jcr->file_list->insert(item->fname, item); 

   Dmsg4(dbglvl, "add fname=<%s> lstat=%s  delta_seq=%i chksum=%s\n", 
         fname, lstat, delta, chksum);
   return ret;
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
   int digest_stream = STREAM_NONE;
   DIGEST *digest = NULL;

   struct stat statc;
   int32_t LinkFIc;
   bool stat = false;
   char *opts;
   char *fname;
   CurFile elt;

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

   if (!accurate_lookup(jcr, fname, &elt)) {
      Dmsg1(dbglvl, "accurate %s (not found)\n", fname);
      stat = true;
      goto bail_out;
   }

   ff_pkt->accurate_found = true;
   ff_pkt->delta_seq = elt.delta_seq;

   decode_stat(elt.lstat, &statc, sizeof(statc), &LinkFIc); /* decode catalog stat */

   if (!jcr->rerunning && (jcr->getJobLevel() == L_FULL)) {
      opts = ff_pkt->BaseJobOpts;
   } else {
      opts = ff_pkt->AccurateOpts;
   }

   /*
    * Loop over options supplied by user and verify the
    * fields he requests.
    */
   for (char *p=opts; !stat && *p; p++) {
      char ed1[30], ed2[30];
      switch (*p) {
      case 'i':                /* compare INODEs */
         if (statc.st_ino != ff_pkt->statp.st_ino) {
            Dmsg3(dbglvl-1, "%s      st_ino   differ. Cat: %s File: %s\n",
                  fname,
                  edit_uint64((uint64_t)statc.st_ino, ed1),
                  edit_uint64((uint64_t)ff_pkt->statp.st_ino, ed2));
            stat = true;
         }
         break;
      case 'p':                /* permissions bits */
         /* TODO: If something change only in perm, user, group
          * Backup only the attribute stream
          */
         if (statc.st_mode != ff_pkt->statp.st_mode) {
            Dmsg3(dbglvl-1, "%s     st_mode  differ. Cat: %x File: %x\n",
                  fname,
                  (uint32_t)statc.st_mode, (uint32_t)ff_pkt->statp.st_mode);
            stat = true;
         }
         break;
      case 'n':                /* number of links */
         if (statc.st_nlink != ff_pkt->statp.st_nlink) {
            Dmsg3(dbglvl-1, "%s      st_nlink differ. Cat: %d File: %d\n",
                  fname,
                  (uint32_t)statc.st_nlink, (uint32_t)ff_pkt->statp.st_nlink);
            stat = true;
         }
         break;
      case 'u':                /* user id */
         if (statc.st_uid != ff_pkt->statp.st_uid) {
            Dmsg3(dbglvl-1, "%s      st_uid   differ. Cat: %u File: %u\n",
                  fname,
                  (uint32_t)statc.st_uid, (uint32_t)ff_pkt->statp.st_uid);
            stat = true;
         }
         break;
      case 'g':                /* group id */
         if (statc.st_gid != ff_pkt->statp.st_gid) {
            Dmsg3(dbglvl-1, "%s      st_gid   differ. Cat: %u File: %u\n",
                  fname,
                  (uint32_t)statc.st_gid, (uint32_t)ff_pkt->statp.st_gid);
            stat = true;
         }
         break;
      case 's':                /* size */
         if (statc.st_size != ff_pkt->statp.st_size) {
            Dmsg3(dbglvl-1, "%s      st_size  differ. Cat: %s File: %s\n",
                  fname,
                  edit_uint64((uint64_t)statc.st_size, ed1),
                  edit_uint64((uint64_t)ff_pkt->statp.st_size, ed2));
            stat = true;
         }
         break;
      case 'a':                /* access time */
         if (statc.st_atime != ff_pkt->statp.st_atime) {
            Dmsg1(dbglvl-1, "%s      st_atime differs\n", fname);
            stat = true;
         }
         break;
      case 'm':                 /* modification time */
         if (statc.st_mtime != ff_pkt->statp.st_mtime) {
            Dmsg1(dbglvl-1, "%s      st_mtime differs\n", fname);
            stat = true;
         }
         break;
      case 'c':                /* ctime */
         if (statc.st_ctime != ff_pkt->statp.st_ctime) {
            Dmsg1(dbglvl-1, "%s      st_ctime differs\n", fname);
            stat = true;
         }
         break;
      case 'd':                /* file size decrease */
         if (statc.st_size > ff_pkt->statp.st_size) {
            Dmsg3(dbglvl-1, "%s      st_size  decrease. Cat: %s File: %s\n",
                  fname,
                  edit_uint64((uint64_t)statc.st_size, ed1),
                  edit_uint64((uint64_t)ff_pkt->statp.st_size, ed2));
            stat = true;
         }
         break;
      case 'A':                 /* Always backup a file */
         stat = true;
         break;
      /* TODO: cleanup and factorise this function with verify.c */
      case '5':                /* compare MD5 */
      case '1':                /* compare SHA1 */
        /*
          * The remainder of the function is all about getting the checksum.
          * First we initialise, then we read files, other streams and Finder Info.
          */
         if (!stat && ff_pkt->type != FT_LNKSAVED && 
             (S_ISREG(ff_pkt->statp.st_mode) && 
              ff_pkt->flags & (FO_MD5|FO_SHA1|FO_SHA256|FO_SHA512))) 
         {

            if (!*elt.chksum && !jcr->rerunning) {
               Jmsg(jcr, M_WARNING, 0, _("Cannot verify checksum for %s\n"),
                    ff_pkt->fname);
               stat = true;
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
            
            /* Did digest initialization fail? */
            if (digest_stream != STREAM_NONE && digest == NULL) {
               Jmsg(jcr, M_WARNING, 0, _("%s digest initialization failed\n"),
                    stream_to_ascii(digest_stream));
            }

            /* compute MD5 or SHA1 hash */
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

                  if (strcmp(digest_buf, elt.chksum)) {
                     Dmsg4(dbglvl,"%s      %s chksum  diff. Cat: %s File: %s\n",
                           fname,
                           digest_name,
                           elt.chksum,
                           digest_buf);
                     stat = true;
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

   /* In Incr/Diff accurate mode, we mark all files as seen
    * When in Full+Base mode, we mark only if the file match exactly
    */
   if (jcr->getJobLevel() == L_FULL) {
      if (!stat) {               
         /* compute space saved with basefile */
         jcr->base_size += ff_pkt->statp.st_size;
         accurate_mark_file_as_seen(jcr, &elt);
      }
   } else {
      accurate_mark_file_as_seen(jcr, &elt);
   }

bail_out:
   unstrip_path(ff_pkt);
   return stat;
}

/* 
 * TODO: use big buffer from htable
 */
int accurate_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int lstat_pos, chksum_pos;
   int32_t nb;
   uint16_t delta_seq;

   if (job_canceled(jcr)) {
      return true;
   }
   if (sscanf(dir->msg, "accurate files=%ld", &nb) != 1) {
      dir->fsend(_("2991 Bad accurate command\n"));
      return false;
   }

   jcr->accurate = true;

   accurate_init(jcr, nb);

   /*
    * buffer = sizeof(CurFile) + dirmsg
    * dirmsg = fname + \0 + lstat + \0 + checksum + \0 + delta_seq + \0
    */
   /* get current files */
   while (dir->recv() >= 0) {
      lstat_pos = strlen(dir->msg) + 1;
      if (lstat_pos < dir->msglen) {
         chksum_pos = lstat_pos + strlen(dir->msg + lstat_pos) + 1;

         if (chksum_pos >= dir->msglen) {
            chksum_pos = lstat_pos - 1;    /* tweak: no checksum, point to the last \0 */
            delta_seq = 0;
         } else {
            delta_seq = str_to_int32(dir->msg + 
                                     chksum_pos + 
                                     strlen(dir->msg + chksum_pos) + 1);
         }

         accurate_add_file(jcr, dir->msglen, 
                           dir->msg,               /* Path */
                           dir->msg + lstat_pos,   /* LStat */
                           dir->msg + chksum_pos,  /* CheckSum */
                           delta_seq);             /* Delta Sequence */
      }
   }

#ifdef DEBUG
   extern void *start_heap;

   char b1[50], b2[50], b3[50], b4[50], b5[50];
   Dmsg5(dbglvl," Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n",
         edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
         edit_uint64_with_commas(sm_bytes, b2),
         edit_uint64_with_commas(sm_max_bytes, b3),
         edit_uint64_with_commas(sm_buffers, b4),
         edit_uint64_with_commas(sm_max_buffers, b5));
#endif

   return true;
}
