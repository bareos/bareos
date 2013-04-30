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
 *  Bacula File Daemon  verify.c  Verify files.
 *
 *    Kern Sibbald, October MM
 *
 */

#include "bacula.h"
#include "filed.h"

#ifdef HAVE_DARWIN_OS
const bool have_darwin_os = true;
#else
const bool have_darwin_os = false;
#endif

static int verify_file(JCR *jcr, FF_PKT *ff_pkt, bool);
static int read_digest(BFILE *bfd, DIGEST *digest, JCR *jcr);

/*
 * Find all the requested files and send attributes
 * to the Director.
 *
 */
void do_verify(JCR *jcr)
{
   jcr->setJobStatus(JS_Running);
   jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   if ((jcr->big_buf = (char *) malloc(jcr->buf_size)) == NULL) {
      Jmsg1(jcr, M_ABORT, 0, _("Cannot malloc %d network read buffer\n"),
         DEFAULT_NETWORK_BUFFER_SIZE);
   }
   set_find_options((FF_PKT *)jcr->ff, jcr->incremental, jcr->mtime);
   Dmsg0(10, "Start find files\n");
   /* Subroutine verify_file() is called for each file */
   find_files(jcr, (FF_PKT *)jcr->ff, verify_file, NULL);
   Dmsg0(10, "End find files\n");

   if (jcr->big_buf) {
      free(jcr->big_buf);
      jcr->big_buf = NULL;
   }
   jcr->setJobStatus(JS_Terminated);
}

/*
 * Called here by find() for each file.
 *
 *  Find the file, compute the MD5 or SHA1 and send it back to the Director
 */
static int verify_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level)
{
   char attribs[MAXSTRING];
   char attribsEx[MAXSTRING];
   int digest_stream = STREAM_NONE;
   int stat;
   DIGEST *digest = NULL;
   BSOCK *dir;

   if (job_canceled(jcr)) {
      return 0;
   }

   dir = jcr->dir_bsock;
   jcr->num_files_examined++;         /* bump total file count */

   switch (ff_pkt->type) {
   case FT_LNKSAVED:                  /* Hard linked, file already saved */
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
      jcr->num_files_examined--;      /* correct file count */
      return 1;                       /* ignored */
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
      berrno be;
      be.set_errno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not access %s: ERR=%s\n"), ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
   }
   case FT_NOFOLLOW: {
      berrno be;
      be.set_errno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not follow link %s: ERR=%s\n"), ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
   }
   case FT_NOSTAT: {
      berrno be;
      be.set_errno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not stat %s: ERR=%s\n"), ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
   }
   case FT_DIRNOCHG:
   case FT_NOCHG:
      Jmsg(jcr, M_SKIPPED, 1, _("     Unchanged file skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_ISARCH:
      Jmsg(jcr, M_SKIPPED, 1, _("     Archive file skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_NORECURSE:
      Jmsg(jcr, M_SKIPPED, 1, _("     Recursion turned off. Directory skipped: %s\n"), ff_pkt->fname);
      ff_pkt->type = FT_DIREND;     /* directory entry was backed up */
      break;
   case FT_NOFSCHG:
      Jmsg(jcr, M_SKIPPED, 1, _("     File system change prohibited. Directory skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_PLUGIN_CONFIG:
   case FT_RESTORE_FIRST:
      return 1;                       /* silently skip */
   case FT_NOOPEN: {
      berrno be;
      be.set_errno(ff_pkt->ff_errno);
      Jmsg(jcr, M_NOTSAVED, 1, _("     Could not open directory %s: ERR=%s\n"), ff_pkt->fname, be.bstrerror());
      jcr->JobErrors++;
      return 1;
   }
   default:
      Jmsg(jcr, M_NOTSAVED, 0, _("     Unknown file type %d: %s\n"), ff_pkt->type, ff_pkt->fname);
      jcr->JobErrors++;
      return 1;
   }

   /* Encode attributes and possibly extend them */
   encode_stat(attribs, &ff_pkt->statp, sizeof(ff_pkt->statp), ff_pkt->LinkFI, 0);
   encode_attribsEx(jcr, attribsEx, ff_pkt);

   jcr->lock();
   jcr->JobFiles++;                  /* increment number of files sent */
   pm_strcpy(jcr->last_fname, ff_pkt->fname);
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
   /* Send file attributes to Director (note different format than for Storage) */
   Dmsg2(400, "send ATTR inx=%d fname=%s\n", jcr->JobFiles, ff_pkt->fname);
   if (ff_pkt->type == FT_LNK || ff_pkt->type == FT_LNKSAVED) {
      stat = dir->fsend("%d %d %s %s%c%s%c%s%c", jcr->JobFiles,
                        STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts, ff_pkt->fname,
                        0, attribs, 0, ff_pkt->link, 0);
   } else if (ff_pkt->type == FT_DIREND || ff_pkt->type == FT_REPARSE ||
              ff_pkt->type == FT_JUNCTION) {
      /* Here link is the canonical filename (i.e. with trailing slash) */
      stat = dir->fsend("%d %d %s %s%c%s%c%c", jcr->JobFiles,
                        STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts, ff_pkt->link,
                        0, attribs, 0, 0);
   } else {
      stat = dir->fsend("%d %d %s %s%c%s%c%c", jcr->JobFiles,
                        STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts, ff_pkt->fname,
                        0, attribs, 0, 0);
   }
   Dmsg2(20, "bfiled>bdird: attribs len=%d: msg=%s\n", dir->msglen, dir->msg);
   if (!stat) {
      Jmsg(jcr, M_FATAL, 0, _("Network error in send to Director: ERR=%s\n"), bnet_strerror(dir));
      return 0;
   }

   /*
    * The remainder of the function is all about getting the checksum.
    * First we initialise, then we read files, other streams and Finder Info.
    */
   if (ff_pkt->type != FT_LNKSAVED && (S_ISREG(ff_pkt->statp.st_mode) &&
            ff_pkt->flags & (FO_MD5|FO_SHA1|FO_SHA256|FO_SHA512))) {
      /*
       * Create our digest context. If this fails, the digest will be set to NULL
       * and not used.
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
            goto good_rtn;
         }

         if (crypto_digest_finalize(digest, (uint8_t *)md, &size)) {
            char *digest_buf;
            const char *digest_name;
            
            digest_buf = (char *)malloc(BASE64_SIZE(size));
            digest_name = crypto_digest_name(digest);

            bin_to_base64(digest_buf, BASE64_SIZE(size), md, size, true);
            Dmsg3(400, "send inx=%d %s=%s\n", jcr->JobFiles, digest_name, digest_buf);
            dir->fsend("%d %d %s *%s-%d*", jcr->JobFiles, digest_stream, digest_buf,
                       digest_name, jcr->JobFiles);
            Dmsg3(20, "bfiled>bdird: %s len=%d: msg=%s\n", digest_name,
                  dir->msglen, dir->msg);

            free(digest_buf);
         }
      }
   }

good_rtn:
   if (digest) {
      crypto_digest_free(digest);
   }
   return 1;
}

/*
 * Compute message digest for the file specified by ff_pkt.
 * In case of errors we need the job control record and file name.
 */
int digest_file(JCR *jcr, FF_PKT *ff_pkt, DIGEST *digest)
{
   BFILE bfd;

   Dmsg0(50, "=== digest_file\n");
   binit(&bfd);

   if (ff_pkt->statp.st_size > 0 ||
       ff_pkt->type == FT_RAW ||
       ff_pkt->type == FT_FIFO) {
      int noatime = ff_pkt->flags & FO_NOATIME ? O_NOATIME : 0;
      if ((bopen(&bfd, ff_pkt->fname, O_RDONLY | O_BINARY | noatime, 0)) < 0) {
         ff_pkt->ff_errno = errno;
         berrno be;
         be.set_errno(bfd.berrno);
         Dmsg2(100, "Cannot open %s: ERR=%s\n", ff_pkt->fname, be.bstrerror());
         Jmsg(jcr, M_ERROR, 1, _("     Cannot open %s: ERR=%s.\n"),
               ff_pkt->fname, be.bstrerror());
         return 1;
      }
      read_digest(&bfd, digest, jcr);
      bclose(&bfd);
   }

   if (have_darwin_os) {
      /* Open resource fork if necessary */
      if (ff_pkt->flags & FO_HFSPLUS && ff_pkt->hfsinfo.rsrclength > 0) {
         if (bopen_rsrc(&bfd, ff_pkt->fname, O_RDONLY | O_BINARY, 0) < 0) {
            ff_pkt->ff_errno = errno;
            berrno be;
            Jmsg(jcr, M_ERROR, -1, _("     Cannot open resource fork for %s: ERR=%s.\n"),
                  ff_pkt->fname, be.bstrerror());
            if (is_bopen(&ff_pkt->bfd)) {
               bclose(&ff_pkt->bfd);
            }
            return 1;
         }
         read_digest(&bfd, digest, jcr);
         bclose(&bfd);
      }

      if (digest && ff_pkt->flags & FO_HFSPLUS) {
         crypto_digest_update(digest, (uint8_t *)ff_pkt->hfsinfo.fndrinfo, 32);
      }
   }
   return 0;
}

/*
 * Read message digest of bfd, updating digest
 * In case of errors we need the job control record and file name.
 */
static int read_digest(BFILE *bfd, DIGEST *digest, JCR *jcr)
{
   char buf[DEFAULT_NETWORK_BUFFER_SIZE];
   int64_t n;
   int64_t bufsiz = (int64_t)sizeof(buf);
   FF_PKT *ff_pkt = (FF_PKT *)jcr->ff;
   uint64_t fileAddr = 0;             /* file address */


   Dmsg0(50, "=== read_digest\n");
   while ((n=bread(bfd, buf, bufsiz)) > 0) {
      /* Check for sparse blocks */
      if (ff_pkt->flags & FO_SPARSE) {
         bool allZeros = false;
         if ((n == bufsiz &&
              fileAddr+n < (uint64_t)ff_pkt->statp.st_size) ||
             ((ff_pkt->type == FT_RAW || ff_pkt->type == FT_FIFO) &&
               (uint64_t)ff_pkt->statp.st_size == 0)) {
            allZeros = is_buf_zero(buf, bufsiz);
         }
         fileAddr += n;               /* update file address */
         /* Skip any block of all zeros */
         if (allZeros) {
            continue;                 /* skip block of zeros */
         }
      }
      
      crypto_digest_update(digest, (uint8_t *)buf, n);

      /* Can be used by BaseJobs or with accurate, update only for Verify
       * jobs 
       */
      if (jcr->getJobType() == JT_VERIFY) {
         jcr->JobBytes += n;
      }
      jcr->ReadBytes += n;
   }
   if (n < 0) {
      berrno be;
      be.set_errno(bfd->berrno);
      Dmsg2(100, "Error reading file %s: ERR=%s\n", jcr->last_fname, be.bstrerror());
      Jmsg(jcr, M_ERROR, 1, _("Error reading file %s: ERR=%s\n"),
            jcr->last_fname, be.bstrerror());
      jcr->JobErrors++;
      return -1;
   }
   return 0;
}
