/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.

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
 *
 *   Bacula Director -- Bootstrap Record routines.
 *
 *      BSR (bootstrap record) handling routines split from
 *        ua_restore.c July MMIII
 *
 *     Kern Sibbald, July MMII
 *
 */

#include "bacula.h"
#include "dird.h"

/* Forward referenced functions */
static uint32_t write_bsr(UAContext *ua, RESTORE_CTX &rx, FILE *fd);

/*
 * Create new FileIndex entry for BSR
 */
RBSR_FINDEX *new_findex()
{
   RBSR_FINDEX *fi = (RBSR_FINDEX *)bmalloc(sizeof(RBSR_FINDEX));
   memset(fi, 0, sizeof(RBSR_FINDEX));
   return fi;
}

/* Free all BSR FileIndex entries */
static void free_findex(RBSR_FINDEX *fi)
{
   RBSR_FINDEX *next;
   for ( ; fi; fi=next) {
      next = fi->next;
      free(fi);
   }
}

/* 
 * Get storage device name from Storage resource
 */
static bool get_storage_device(char *device, char *storage)
{
   STORE *store;
   if (storage[0] == 0) {
      return false;
   }
   store = (STORE *)GetResWithName(R_STORAGE, storage);    
   if (!store) {
      return false;
   }
   DEVICE *dev = (DEVICE *)(store->device->first());
   if (!dev) {
      return false;
   }
   bstrncpy(device, dev->hdr.name, MAX_NAME_LENGTH);
   return true;
}

/*
 * Our data structures were not designed completely
 *  correctly, so the file indexes cover the full
 *  range regardless of volume. The FirstIndex and LastIndex
 *  passed in here are for the current volume, so when
 *  writing out the fi, constrain them to those values.
 *
 * We are called here once for each JobMedia record
 *  for each Volume.
 */
static uint32_t write_findex(RBSR_FINDEX *fi,
              int32_t FirstIndex, int32_t LastIndex, FILE *fd)
{
   uint32_t count = 0;
   for ( ; fi; fi=fi->next) {
      int32_t findex, findex2;
      if ((fi->findex >= FirstIndex && fi->findex <= LastIndex) ||
          (fi->findex2 >= FirstIndex && fi->findex2 <= LastIndex) ||
          (fi->findex < FirstIndex && fi->findex2 > LastIndex)) {
         findex = fi->findex < FirstIndex ? FirstIndex : fi->findex;
         findex2 = fi->findex2 > LastIndex ? LastIndex : fi->findex2;
         if (findex == findex2) {
            fprintf(fd, "FileIndex=%d\n", findex);
            count++;
         } else {
            fprintf(fd, "FileIndex=%d-%d\n", findex, findex2);
            count += findex2 - findex + 1;
         }
      }
   }
   return count;
}

/*
 * Find out if Volume defined with FirstIndex and LastIndex
 *   falls within the range of selected files in the bsr.
 */
static bool is_volume_selected(RBSR_FINDEX *fi,
              int32_t FirstIndex, int32_t LastIndex)
{
   for ( ; fi; fi=fi->next) {
      if ((fi->findex >= FirstIndex && fi->findex <= LastIndex) ||
          (fi->findex2 >= FirstIndex && fi->findex2 <= LastIndex) ||
          (fi->findex < FirstIndex && fi->findex2 > LastIndex)) {
         return true;
      }
   }
   return false;
}


/* Create a new bootstrap record */
RBSR *new_bsr()
{
   RBSR *bsr = (RBSR *)bmalloc(sizeof(RBSR));
   memset(bsr, 0, sizeof(RBSR));
   return bsr;
}

/* Free the entire BSR */
void free_bsr(RBSR *bsr)
{
   RBSR *next;
   for ( ; bsr; bsr=next) {
      free_findex(bsr->fi);
      if (bsr->VolParams) {
         free(bsr->VolParams);
      }
      if (bsr->fileregex) {
         free(bsr->fileregex);
      }
      next = bsr->next;
      free(bsr);
   }
}

/*
 * Complete the BSR by filling in the VolumeName and
 *  VolSessionId and VolSessionTime using the JobId
 */
bool complete_bsr(UAContext *ua, RBSR *bsr)
{
   for ( ; bsr; bsr=bsr->next) {
      JOB_DBR jr;
      memset(&jr, 0, sizeof(jr));
      jr.JobId = bsr->JobId;
      if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
         ua->error_msg(_("Unable to get Job record. ERR=%s\n"), db_strerror(ua->db));
         return false;
      }
      bsr->VolSessionId = jr.VolSessionId;
      bsr->VolSessionTime = jr.VolSessionTime;
      if (jr.JobFiles == 0) {      /* zero files is OK, not an error, but */
         bsr->VolCount = 0;        /*   there are no volumes */
         continue;
      }
      if ((bsr->VolCount=db_get_job_volume_parameters(ua->jcr, ua->db, bsr->JobId,
           &(bsr->VolParams))) == 0) {
         ua->error_msg(_("Unable to get Job Volume Parameters. ERR=%s\n"), db_strerror(ua->db));
         if (bsr->VolParams) {
            free(bsr->VolParams);
            bsr->VolParams = NULL;
         }
         return false;
      }
   }
   return true;
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t uniq = 0;

static void make_unique_restore_filename(UAContext *ua, POOL_MEM &fname)
{
   JCR *jcr = ua->jcr;
   int i = find_arg_with_value(ua, "bootstrap");
   if (i >= 0) {
      Mmsg(fname, "%s", ua->argv[i]);              
      jcr->unlink_bsr = false;
   } else {
      P(mutex);
      uniq++;
      V(mutex);
      Mmsg(fname, "%s/%s.restore.%u.bsr", working_directory, my_name, uniq);
      jcr->unlink_bsr = true;
   }
   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
   }
   jcr->RestoreBootstrap = bstrdup(fname.c_str());
}

/*
 * Write the bootstrap records to file
 */
uint32_t write_bsr_file(UAContext *ua, RESTORE_CTX &rx)
{
   FILE *fd;
   POOL_MEM fname(PM_MESSAGE);
   uint32_t count = 0;;
   bool err;

   make_unique_restore_filename(ua, fname);
   fd = fopen(fname.c_str(), "w+b");
   if (!fd) {
      berrno be;
      ua->error_msg(_("Unable to create bootstrap file %s. ERR=%s\n"),
         fname.c_str(), be.bstrerror());
      goto bail_out;
   }
   /* Write them to file */
   count = write_bsr(ua, rx, fd);
   err = ferror(fd);
   fclose(fd);
   if (count == 0) {
      ua->info_msg(_("No files found to read. No bootstrap file written.\n"));
      goto bail_out;
   }
   if (err) {
      ua->error_msg(_("Error writing bsr file.\n"));
      count = 0;
      goto bail_out;
   }

   ua->send_msg(_("Bootstrap records written to %s\n"), fname.c_str());

   if (debug_level >= 10) {
      print_bsr(ua, rx);
   }

bail_out:
   return count;
}

static void display_vol_info(UAContext *ua, RESTORE_CTX &rx, JobId_t JobId)
{
   POOL_MEM volmsg(PM_MESSAGE);
   char Device[MAX_NAME_LENGTH];
   char online;
   RBSR *bsr;

   for (bsr=rx.bsr; bsr; bsr=bsr->next) {
      if (JobId && JobId != bsr->JobId) {
         continue;
      }

      for (int i=0; i < bsr->VolCount; i++) {
         if (bsr->VolParams[i].VolumeName[0]) {
            if (!get_storage_device(Device, bsr->VolParams[i].Storage)) {
               Device[0] = 0;
            }
            if (bsr->VolParams[i].InChanger && bsr->VolParams[i].Slot) {
               online = '*';
            } else {
               online = ' ';
            }
            Mmsg(volmsg, "%c%-25s %-25s %-25s", 
                 online, bsr->VolParams[i].VolumeName,
                 bsr->VolParams[i].Storage, Device);
            add_prompt(ua, volmsg.c_str());
         }
      }
   }
}

void display_bsr_info(UAContext *ua, RESTORE_CTX &rx)
{
   char *p;
   JobId_t JobId;

   /* Tell the user what he will need to mount */
   ua->send_msg("\n");
   ua->send_msg(_("The job will require the following\n"
                  "   Volume(s)                 Storage(s)                SD Device(s)\n"
                  "===========================================================================\n"));
   /* Create Unique list of Volumes using prompt list */
   start_prompt(ua, "");
   if (*rx.JobIds == 0) {
      /* Print Volumes in any order */
      display_vol_info(ua, rx, 0);
   } else {
      /* Ensure that the volumes are printed in JobId order */
      for (p=rx.JobIds; get_next_jobid_from_list(&p, &JobId) > 0; ) {
         display_vol_info(ua, rx, JobId);
      }
   }
   for (int i=0; i < ua->num_prompts; i++) {
      ua->send_msg("   %s\n", ua->prompt[i]);
      free(ua->prompt[i]);
   }
   if (ua->num_prompts == 0) {
      ua->send_msg(_("No Volumes found to restore.\n"));
   } else {
      ua->send_msg(_("\nVolumes marked with \"*\" are online.\n"));
   }
   ua->num_prompts = 0;
   ua->send_msg("\n");

   return;
}

/*
 * Write bsr data for a single bsr record
 */
static uint32_t write_bsr_item(RBSR *bsr, UAContext *ua, 
                   RESTORE_CTX &rx, FILE *fd, bool &first, uint32_t &LastIndex)
{
   char ed1[50], ed2[50];
   uint32_t count = 0;
   uint32_t total_count = 0;
   char device[MAX_NAME_LENGTH];

   /*
    * For a given volume, loop over all the JobMedia records.
    *   VolCount is the number of JobMedia records.
    */
   for (int i=0; i < bsr->VolCount; i++) {
      if (!is_volume_selected(bsr->fi, bsr->VolParams[i].FirstIndex,
           bsr->VolParams[i].LastIndex)) {
         bsr->VolParams[i].VolumeName[0] = 0;  /* zap VolumeName */
         continue;
      }
      if (!rx.store) {
         find_storage_resource(ua, rx, bsr->VolParams[i].Storage,
                                       bsr->VolParams[i].MediaType);
      }
      fprintf(fd, "Storage=\"%s\"\n", bsr->VolParams[i].Storage);
      fprintf(fd, "Volume=\"%s\"\n", bsr->VolParams[i].VolumeName);
      fprintf(fd, "MediaType=\"%s\"\n", bsr->VolParams[i].MediaType);
      if (bsr->fileregex) {
         fprintf(fd, "FileRegex=%s\n", bsr->fileregex);
      }
      if (get_storage_device(device, bsr->VolParams[i].Storage)) {
         fprintf(fd, "Device=\"%s\"\n", device);
      }
      if (bsr->VolParams[i].Slot > 0) {
         fprintf(fd, "Slot=%d\n", bsr->VolParams[i].Slot);
      }
      fprintf(fd, "VolSessionId=%u\n", bsr->VolSessionId);
      fprintf(fd, "VolSessionTime=%u\n", bsr->VolSessionTime);
      fprintf(fd, "VolAddr=%s-%s\n", edit_uint64(bsr->VolParams[i].StartAddr, ed1),
              edit_uint64(bsr->VolParams[i].EndAddr, ed2));
//    Dmsg2(100, "bsr VolParam FI=%u LI=%u\n",
//      bsr->VolParams[i].FirstIndex, bsr->VolParams[i].LastIndex);

      count = write_findex(bsr->fi, bsr->VolParams[i].FirstIndex,
                           bsr->VolParams[i].LastIndex, fd);
      if (count) {
         fprintf(fd, "Count=%u\n", count);
      }
      total_count += count;
      /* If the same file is present on two tapes or in two files
       *   on a tape, it is a continuation, and should not be treated
       *   twice in the totals.
       */
      if (!first && LastIndex == bsr->VolParams[i].FirstIndex) {
         total_count--;
      }
      first = false;
      LastIndex = bsr->VolParams[i].LastIndex;
   }
   return total_count;
}


/*
 * Here we actually write out the details of the bsr file.
 *  Note, there is one bsr for each JobId, but the bsr may
 *  have multiple volumes, which have been entered in the
 *  order they were written.  
 * The bsrs must be written out in the order the JobIds
 *  are found in the jobid list.
 */
static uint32_t write_bsr(UAContext *ua, RESTORE_CTX &rx, FILE *fd)
{
   bool first = true;
   uint32_t LastIndex = 0;
   uint32_t total_count = 0;
   char *p;
   JobId_t JobId;
   RBSR *bsr;
   if (*rx.JobIds == 0) {
      for (bsr=rx.bsr; bsr; bsr=bsr->next) {
         total_count += write_bsr_item(bsr, ua, rx, fd, first, LastIndex);
      }
      return total_count;
   }
   for (p=rx.JobIds; get_next_jobid_from_list(&p, &JobId) > 0; ) {
      for (bsr=rx.bsr; bsr; bsr=bsr->next) {
         if (JobId == bsr->JobId) {
            total_count += write_bsr_item(bsr, ua, rx, fd, first, LastIndex);
         }
      }
   }
   return total_count;
}

void print_bsr(UAContext *ua, RESTORE_CTX &rx)
{
   write_bsr(ua, rx, stdout);
}




/*
 * Add a FileIndex to the list of BootStrap records.
 *  Here we are only dealing with JobId's and the FileIndexes
 *  associated with those JobIds.
 * We expect that JobId, FileIndex are sorted ascending.
 */
void add_findex(RBSR *bsr, uint32_t JobId, int32_t findex)
{
   RBSR *nbsr;
   RBSR_FINDEX *fi, *lfi;

   if (findex == 0) {
      return;                         /* probably a dummy directory */
   }

   if (bsr->fi == NULL) {             /* if no FI add one */
      /* This is the first FileIndex item in the chain */
      bsr->fi = new_findex();
      bsr->JobId = JobId;
      bsr->fi->findex = findex;
      bsr->fi->findex2 = findex;
      return;
   }
   /* Walk down list of bsrs until we find the JobId */
   if (bsr->JobId != JobId) {
      for (nbsr=bsr->next; nbsr; nbsr=nbsr->next) {
         if (nbsr->JobId == JobId) {
            bsr = nbsr;
            break;
         }
      }

      if (!nbsr) {                    /* Must add new JobId */
         /* Add new JobId at end of chain */
         for (nbsr=bsr; nbsr->next; nbsr=nbsr->next)
            {  }
         nbsr->next = new_bsr();
         nbsr->next->JobId = JobId;
         nbsr->next->fi = new_findex();
         nbsr->next->fi->findex = findex;
         nbsr->next->fi->findex2 = findex;
         return;
      }
   }

   /*
    * At this point, bsr points to bsr containing this JobId,
    *  and we are sure that there is at least one fi record.
    */
   lfi = fi = bsr->fi;
   /* Check if this findex is smaller than first item */
   if (findex < fi->findex) {
      if ((findex+1) == fi->findex) {
         fi->findex = findex;         /* extend down */
         return;
      }
      fi = new_findex();              /* yes, insert before first item */
      fi->findex = findex;
      fi->findex2 = findex;
      fi->next = lfi;
      bsr->fi = fi;
      return;
   }
   /* Walk down fi chain and find where to insert insert new FileIndex */
   for ( ; fi; fi=fi->next) {
      if (findex == (fi->findex2 + 1)) {  /* extend up */
         RBSR_FINDEX *nfi;
         fi->findex2 = findex;
         /*
          * If the following record contains one higher, merge its
          *   file index by extending it up.
          */
         if (fi->next && ((findex+1) == fi->next->findex)) {
            nfi = fi->next;
            fi->findex2 = nfi->findex2;
            fi->next = nfi->next;
            free(nfi);
         }
         return;
      }
      if (findex < fi->findex) {      /* add before */
         if ((findex+1) == fi->findex) {
            fi->findex = findex;
            return;
         }
         break;
      }
      lfi = fi;
   }
   /* Add to last place found */
   fi = new_findex();
   fi->findex = findex;
   fi->findex2 = findex;
   fi->next = lfi->next;
   lfi->next = fi;
   return;
}

/*
 * Add all possible  FileIndexes to the list of BootStrap records.
 *  Here we are only dealing with JobId's and the FileIndexes
 *  associated with those JobIds.
 */
void add_findex_all(RBSR *bsr, uint32_t JobId)
{
   RBSR *nbsr;
   RBSR_FINDEX *fi;

   if (bsr->fi == NULL) {             /* if no FI add one */
      /* This is the first FileIndex item in the chain */
      bsr->fi = new_findex();
      bsr->JobId = JobId;
      bsr->fi->findex = 1;
      bsr->fi->findex2 = INT32_MAX;
      return;
   }
   /* Walk down list of bsrs until we find the JobId */
   if (bsr->JobId != JobId) {
      for (nbsr=bsr->next; nbsr; nbsr=nbsr->next) {
         if (nbsr->JobId == JobId) {
            bsr = nbsr;
            break;
         }
      }

      if (!nbsr) {                    /* Must add new JobId */
         /* Add new JobId at end of chain */
         for (nbsr=bsr; nbsr->next; nbsr=nbsr->next)
            {  }

         nbsr->next = new_bsr();
         nbsr->next->JobId = JobId;

         /* If we use regexp to restore, set it for each jobid */
         if (bsr->fileregex) { 
            nbsr->next->fileregex = bstrdup(bsr->fileregex);
         }

         nbsr->next->fi = new_findex();
         nbsr->next->fi->findex = 1;
         nbsr->next->fi->findex2 = INT32_MAX;
         return;
      }
   }

   /*
    * At this point, bsr points to bsr containing this JobId,
    *  and we are sure that there is at least one fi record.
    */
   fi = bsr->fi;
   fi->findex = 1;
   fi->findex2 = INT32_MAX;
   return;
}
