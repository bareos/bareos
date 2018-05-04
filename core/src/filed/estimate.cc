/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, September MMI
 */
/**
 * @file
 * Bareos File Daemon estimate.c
 * Make and estimate of the number of files and size to be saved.
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/accurate.h"

static int tally_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool);

/**
 * Find all the requested files and count them.
 */
int MakeEstimate(JobControlRecord *jcr)
{
   int status;

   jcr->setJobStatus(JS_Running);

   SetFindOptions((FindFilesPacket *)jcr->ff, jcr->incremental, jcr->mtime);
   /* in accurate mode, we overwrite the find_one check function */
   if (jcr->accurate) {
      SetFindChangedFunction((FindFilesPacket *)jcr->ff, accurate_check_file);
   }

   status = FindFiles(jcr, (FindFilesPacket *)jcr->ff, tally_file, plugin_estimate);
   AccurateFree(jcr);
   return status;
}

/**
 * Called here by find() for each file included.
 *
 */
static int tally_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level)
{
   Attributes attr;

   if (JobCanceled(jcr)) {
      return 0;
   }
   switch (ff_pkt->type) {
   case FT_LNKSAVED:                  /* Hard linked, file already saved */
   case FT_REGE:
   case FT_REG:
   case FT_LNK:
   case FT_NORECURSE:
   case FT_NOFSCHG:
   case FT_INVALIDFS:
   case FT_INVALIDDT:
   case FT_REPARSE:
   case FT_JUNCTION:
   case FT_DIREND:
   case FT_SPEC:
   case FT_RAW:
   case FT_FIFO:
      break;
   case FT_DIRBEGIN:
   case FT_NOACCESS:
   case FT_NOFOLLOW:
   case FT_NOSTAT:
   case FT_DIRNOCHG:
   case FT_NOCHG:
   case FT_ISARCH:
   case FT_NOOPEN:
   default:
      return 1;
   }

   if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode)) {
      if (ff_pkt->statp.st_size > 0) {
         jcr->JobBytes += ff_pkt->statp.st_size;
      }
#ifdef HAVE_DARWIN_OS
      if (BitIsSet(FO_HFSPLUS, ff_pkt->flags)) {
         if (ff_pkt->hfsinfo.rsrclength > 0) {
            jcr->JobBytes += ff_pkt->hfsinfo.rsrclength;
         }
         jcr->JobBytes += 32;    /* Finder info */
      }
#endif
   }
   jcr->num_files_examined++;
   jcr->JobFiles++;                  /* increment number of files seen */
   if (jcr->listing) {
      memcpy(&attr.statp, &ff_pkt->statp, sizeof(struct stat));
      attr.type = ff_pkt->type;
      attr.ofname = (POOLMEM *)ff_pkt->fname;
      attr.olname = (POOLMEM *)ff_pkt->link;
      PrintLsOutput(jcr, &attr);
   }
   return 1;
}
