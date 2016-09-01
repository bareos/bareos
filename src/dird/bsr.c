/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * BAREOS Director -- Bootstrap routines.
 *
 * BSR (bootstrap record) handling routines split from ua_restore.c July 2003
 * Bootstrap send handling routines split from restore.c July 2012
 *
 * Kern Sibbald, July 2002
 */

#include "bareos.h"
#include "dird.h"

#define UA_CMD_SIZE 1000

/* Forward referenced functions */

/*
 * Create new FileIndex entry for BSR
 */
RBSR_FINDEX *new_findex()
{
   RBSR_FINDEX *fi = (RBSR_FINDEX *)bmalloc(sizeof(RBSR_FINDEX));
   memset(fi, 0, sizeof(RBSR_FINDEX));
   return fi;
}

/*
 * Free all BSR FileIndex entries
 */
static inline void free_findex(RBSR_FINDEX *fi)
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
   STORERES *store;
   if (storage[0] == 0) {
      return false;
   }
   store = (STORERES *)GetResWithName(R_STORAGE, storage);
   if (!store) {
      return false;
   }
   DEVICERES *dev = (DEVICERES *)(store->device->first());
   if (!dev) {
      return false;
   }
   bstrncpy(device, dev->hdr.name, MAX_NAME_LENGTH);
   return true;
}

/*
 * Print a BSR entry into a memory buffer.
 */
static void print_bsr_item(POOL_MEM *pool_buf, const char *fmt, ...)
{
   va_list arg_ptr;
   int len, maxlen;
   POOL_MEM item(PM_MESSAGE);

   while (1) {
      maxlen = item.max_size() - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(item.c_str(), maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen - 5)) {
         item.realloc_pm(maxlen + maxlen / 2);
         continue;
      }
      break;
   }

   pool_buf->strcat(item.c_str());
}

/*
 * Our data structures were not designed completely
 * correctly, so the file indexes cover the full
 * range regardless of volume. The FirstIndex and LastIndex
 * passed in here are for the current volume, so when
 * writing out the fi, constrain them to those values.
 *
 * We are called here once for each JobMedia record
 * for each Volume.
 */
static inline uint32_t write_findex(RBSR_FINDEX *fi,
                                    int32_t FirstIndex,
                                    int32_t LastIndex,
                                    POOL_MEM *buffer)
{
   int32_t findex, findex2;
   uint32_t count = 0;

   while (fi) {
      if ((fi->findex >= FirstIndex && fi->findex <= LastIndex) ||
          (fi->findex2 >= FirstIndex && fi->findex2 <= LastIndex) ||
          (fi->findex < FirstIndex && fi->findex2 > LastIndex)) {
         findex = fi->findex < FirstIndex ? FirstIndex : fi->findex;
         findex2 = fi->findex2 > LastIndex ? LastIndex : fi->findex2;
         if (findex == findex2) {
            print_bsr_item(buffer, "FileIndex=%d\n", findex);
            count++;
         } else {
            print_bsr_item(buffer, "FileIndex=%d-%d\n", findex, findex2);
            count += findex2 - findex + 1;
         }
      }
      fi = fi->next;
   }

   return count;
}

/*
 * Find out if Volume defined with FirstIndex and LastIndex
 * falls within the range of selected files in the bsr.
 */
static inline bool is_volume_selected(RBSR_FINDEX *fi,
                                      int32_t FirstIndex,
                                      int32_t LastIndex)
{
   while (fi) {
      if ((fi->findex >= FirstIndex && fi->findex <= LastIndex) ||
          (fi->findex2 >= FirstIndex && fi->findex2 <= LastIndex) ||
          (fi->findex < FirstIndex && fi->findex2 > LastIndex)) {
         return true;
      }
      fi = fi->next;
   }
   return false;
}

/*
 * Create a new bootstrap record
 */
RBSR *new_bsr()
{
   RBSR *bsr = (RBSR *)bmalloc(sizeof(RBSR));

   memset(bsr, 0, sizeof(RBSR));
   return bsr;
}

/*
 * Free the entire BSR
 */
void free_bsr(RBSR *bsr)
{
   RBSR *next;

   while (bsr) {
      free_findex(bsr->fi);

      if (bsr->VolParams) {
         free(bsr->VolParams);
      }

      if (bsr->fileregex) {
         free(bsr->fileregex);
      }

      next = bsr->next;
      free(bsr);
      bsr = next;
   }
}

/*
 * Complete the BSR by filling in the VolumeName and
 * VolSessionId and VolSessionTime using the JobId
 */
bool complete_bsr(UAContext *ua, RBSR *bsr)
{
   for ( ; bsr; bsr=bsr->next) {
      JOB_DBR jr;
      memset(&jr, 0, sizeof(jr));
      jr.JobId = bsr->JobId;
      if (!ua->db->get_job_record(ua->jcr, &jr)) {
         ua->error_msg(_("Unable to get Job record. ERR=%s\n"), ua->db->strerror());
         return false;
      }
      bsr->VolSessionId = jr.VolSessionId;
      bsr->VolSessionTime = jr.VolSessionTime;
      if (jr.JobFiles == 0) {      /* zero files is OK, not an error, but */
         bsr->VolCount = 0;        /*   there are no volumes */
         continue;
      }
      if ((bsr->VolCount = ua->db->get_job_volume_parameters(ua->jcr, bsr->JobId,
           &(bsr->VolParams))) == 0) {
         ua->error_msg(_("Unable to get Job Volume Parameters. ERR=%s\n"), ua->db->strerror());
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
   bool err;
   uint32_t count = 0;
   POOL_MEM fname(PM_MESSAGE);
   POOL_MEM buffer(PM_MESSAGE);

   make_unique_restore_filename(ua, fname);
   fd = fopen(fname.c_str(), "w+b");
   if (!fd) {
      berrno be;
      ua->error_msg(_("Unable to create bootstrap file %s. ERR=%s\n"),
         fname.c_str(), be.bstrerror());
      goto bail_out;
   }

   /*
    * Write them to a buffer.
    */
   count = write_bsr(ua, rx, &buffer);

   /*
    * Write the buffer to file
    */
   fprintf(fd, "%s", buffer.c_str());

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
   int i;
   RBSR *bsr;
   char online;
   POOL_MEM volmsg(PM_MESSAGE);
   char Device[MAX_NAME_LENGTH];

   for (bsr = rx.bsr; bsr; bsr = bsr->next) {
      if (JobId && JobId != bsr->JobId) {
         continue;
      }

      for (i = 0; i < bsr->VolCount; i++) {
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
   int i;
   char *p;
   JobId_t JobId;

   /*
    * Tell the user what he will need to mount
    */
   ua->send_msg("\n");
   ua->send_msg(_("The job will require the following\n"
                  "   Volume(s)                 Storage(s)                SD Device(s)\n"
                  "===========================================================================\n"));

   /*
    * Create Unique list of Volumes using prompt list
    */
   start_prompt(ua, "");
   if (*rx.JobIds == 0) {
      /*
       * Print Volumes in any order
       */
      display_vol_info(ua, rx, 0);
   } else {
      /*
       * Ensure that the volumes are printed in JobId order
       */
      for (p = rx.JobIds; get_next_jobid_from_list(&p, &JobId) > 0; ) {
         display_vol_info(ua, rx, JobId);
      }
   }

   for (i = 0; i < ua->num_prompts; i++) {
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
                               RESTORE_CTX &rx, POOL_MEM *buffer,
                               bool &first, uint32_t &LastIndex)
{
   int i;
   char ed1[50], ed2[50];
   uint32_t count = 0;
   uint32_t total_count = 0;
   char device[MAX_NAME_LENGTH];

   /*
    * For a given volume, loop over all the JobMedia records.
    * VolCount is the number of JobMedia records.
    */
   for (i = 0; i < bsr->VolCount; i++) {
      if (!is_volume_selected(bsr->fi, bsr->VolParams[i].FirstIndex,
           bsr->VolParams[i].LastIndex)) {
         bsr->VolParams[i].VolumeName[0] = 0;  /* zap VolumeName */
         continue;
      }

      if (!rx.store) {
         find_storage_resource(ua, rx, bsr->VolParams[i].Storage,
                               bsr->VolParams[i].MediaType);
      }

      print_bsr_item(buffer, "Storage=\"%s\"\n", bsr->VolParams[i].Storage);
      print_bsr_item(buffer, "Volume=\"%s\"\n", bsr->VolParams[i].VolumeName);
      print_bsr_item(buffer, "MediaType=\"%s\"\n", bsr->VolParams[i].MediaType);

      if (bsr->fileregex) {
         print_bsr_item(buffer, "FileRegex=%s\n", bsr->fileregex);
      }

      if (get_storage_device(device, bsr->VolParams[i].Storage)) {
         print_bsr_item(buffer, "Device=\"%s\"\n", device);
      }

      if (bsr->VolParams[i].Slot > 0) {
         print_bsr_item(buffer, "Slot=%d\n", bsr->VolParams[i].Slot);
      }

      print_bsr_item(buffer, "VolSessionId=%u\n", bsr->VolSessionId);
      print_bsr_item(buffer, "VolSessionTime=%u\n", bsr->VolSessionTime);
      print_bsr_item(buffer, "VolAddr=%s-%s\n", edit_uint64(bsr->VolParams[i].StartAddr, ed1),
                     edit_uint64(bsr->VolParams[i].EndAddr, ed2));
//    Dmsg2(100, "bsr VolParam FI=%u LI=%u\n",
//          bsr->VolParams[i].FirstIndex, bsr->VolParams[i].LastIndex);

      count = write_findex(bsr->fi, bsr->VolParams[i].FirstIndex,
                           bsr->VolParams[i].LastIndex, buffer);
      if (count) {
         print_bsr_item(buffer, "Count=%u\n", count);
      }

      total_count += count;

      /*
       * If the same file is present on two tapes or in two files
       * on a tape, it is a continuation, and should not be treated
       * twice in the totals.
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
 * Note, there is one bsr for each JobId, but the bsr may
 * have multiple volumes, which have been entered in the
 * order they were written.
 *
 * The bsrs must be written out in the order the JobIds
 * are found in the jobid list.
 */
uint32_t write_bsr(UAContext *ua, RESTORE_CTX &rx, POOL_MEM *buffer)
{
   bool first = true;
   uint32_t LastIndex = 0;
   uint32_t total_count = 0;
   char *p;
   JobId_t JobId;
   RBSR *bsr;

   if (*rx.JobIds == 0) {
      for (bsr = rx.bsr; bsr; bsr = bsr->next) {
         total_count += write_bsr_item(bsr, ua, rx, buffer, first, LastIndex);
      }
      return total_count;
   }

   for (p = rx.JobIds; get_next_jobid_from_list(&p, &JobId) > 0; ) {
      for (bsr = rx.bsr; bsr; bsr = bsr->next) {
         if (JobId == bsr->JobId) {
            total_count += write_bsr_item(bsr, ua, rx, buffer, first, LastIndex);
         }
      }
   }

   return total_count;
}

void print_bsr(UAContext *ua, RESTORE_CTX &rx)
{
   POOL_MEM buffer(PM_MESSAGE);

   write_bsr(ua, rx, &buffer);
   fprintf(stdout, "%s", buffer.c_str());
}

/*
 * Add a FileIndex to the list of BootStrap records.
 * Here we are only dealing with JobId's and the FileIndexes
 * associated with those JobIds.
 *
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
      /*
       * This is the first FileIndex item in the chain
       */
      bsr->fi = new_findex();
      bsr->JobId = JobId;
      bsr->fi->findex = findex;
      bsr->fi->findex2 = findex;
      return;
   }

   /*
    * Walk down list of bsrs until we find the JobId
    */
   if (bsr->JobId != JobId) {
      for (nbsr = bsr->next; nbsr; nbsr = nbsr->next) {
         if (nbsr->JobId == JobId) {
            bsr = nbsr;
            break;
         }
      }

      if (!nbsr) {                    /* Must add new JobId */
         /*
          * Add new JobId at end of chain
          */
         for (nbsr = bsr; nbsr->next; nbsr = nbsr->next)
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

   /*
    * Check if this findex is a duplicate.
    */
   if (findex >= fi->findex && findex <= fi->findex2) {
      return;
   }

   /*
    * Check if this findex is smaller than first item
    */
   if (findex < fi->findex) {
      if ((findex + 1) == fi->findex) {
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

   /*
    * Walk down fi chain and find where to insert insert new FileIndex
    */
   while (fi) {
      /*
       * Check if this findex is a duplicate.
       */
      if (findex >= fi->findex && findex <= fi->findex2) {
         return;
      }

      if (findex == (fi->findex2 + 1)) {  /* extend up */
         RBSR_FINDEX *nfi;
         fi->findex2 = findex;

         /*
          * If the following record contains one higher, merge its
          * file index by extending it up.
          */
         if (fi->next && ((findex + 1) == fi->next->findex)) {
            nfi = fi->next;
            fi->findex2 = nfi->findex2;
            fi->next = nfi->next;
            free(nfi);
         }
         return;
      }

      if (findex < fi->findex) {      /* add before */
         if ((findex + 1) == fi->findex) {
            fi->findex = findex;
            return;
         }
         break;
      }

      lfi = fi;
      fi = fi->next;
   }

   /*
    * Add to last place found
    */
   fi = new_findex();
   fi->findex = findex;
   fi->findex2 = findex;
   fi->next = lfi->next;
   lfi->next = fi;

   return;
}

/*
 * Add all possible  FileIndexes to the list of BootStrap records.
 * Here we are only dealing with JobId's and the FileIndexes
 * associated with those JobIds.
 */
void add_findex_all(RBSR *bsr, uint32_t JobId)
{
   RBSR *nbsr;
   RBSR_FINDEX *fi;

   if (bsr->fi == NULL) {             /* if no FI add one */
      /*
       * This is the first FileIndex item in the chain
       */
      bsr->fi = new_findex();
      bsr->JobId = JobId;
      bsr->fi->findex = 1;
      bsr->fi->findex2 = INT32_MAX;
      return;
   }

   /*
    * Walk down list of bsrs until we find the JobId
    */
   if (bsr->JobId != JobId) {
      for (nbsr = bsr->next; nbsr; nbsr = nbsr->next) {
         if (nbsr->JobId == JobId) {
            bsr = nbsr;
            break;
         }
      }

      if (!nbsr) {                    /* Must add new JobId */
         /*
          * Add new JobId at end of chain
          */
         for (nbsr = bsr; nbsr->next; nbsr = nbsr->next)
            {  }

         nbsr->next = new_bsr();
         nbsr->next->JobId = JobId;

         /*
          * If we use regexp to restore, set it for each jobid
          */
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
    * and we are sure that there is at least one fi record.
    */
   fi = bsr->fi;
   fi->findex = 1;
   fi->findex2 = INT32_MAX;
   return;
}

/*
 * Open the bootstrap file and find the first Storage=
 * Returns ok if able to open
 *
 * It fills the storage name (should be the first line)
 * and the file descriptor to the bootstrap file,
 * it should be used for next operations, and need to be
 * closed at the end.
 */
bool open_bootstrap_file(JCR *jcr, bootstrap_info &info)
{
   FILE *bs;
   UAContext *ua;
   info.bs = NULL;
   info.ua = NULL;

   if (!jcr->RestoreBootstrap) {
      return false;
   }
   bstrncpy(info.storage, jcr->res.rstore->name(), MAX_NAME_LENGTH);

   bs = fopen(jcr->RestoreBootstrap, "rb");
   if (!bs) {
      berrno be;
      Jmsg(jcr, M_FATAL, 0, _("Could not open bootstrap file %s: ERR=%s\n"),
         jcr->RestoreBootstrap, be.bstrerror());
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   ua = new_ua_context(jcr);
   ua->cmd = check_pool_memory_size(ua->cmd, UA_CMD_SIZE + 1);
   while (!fgets(ua->cmd, UA_CMD_SIZE, bs)) {
      parse_ua_args(ua);
      if (ua->argc != 1) {
         continue;
      }
      if (bstrcasecmp(ua->argk[0], "Storage")) {
         bstrncpy(info.storage, ua->argv[0], MAX_NAME_LENGTH);
         break;
      }
   }
   info.bs = bs;
   info.ua = ua;
   fseek(bs, 0, SEEK_SET);      /* return to the top of the file */
   return true;
}

/*
 * This function compare the given storage name with the
 * the current one. We compare the name and the address:port.
 * Returns true if we use the same storage.
 */
static inline bool is_on_same_storage(JCR *jcr, char *new_one)
{
   STORERES *new_store;

   /*
    * With old FD, we send the whole bootstrap to the storage
    */
   if (jcr->FDVersion < FD_VERSION_2) {
      return true;
   }

   /*
    * We are in init loop ? shoudn't fail here
    */
   if (!*new_one) {
      return true;
   }

   /*
    * Same name
    */
   if (bstrcmp(new_one, jcr->res.rstore->name())) {
      return true;
   }

   new_store = (STORERES *)GetResWithName(R_STORAGE, new_one);
   if (!new_store) {
      Jmsg(jcr, M_WARNING, 0,
           _("Could not get storage resource '%s'.\n"), new_one);
      return true;
   }

   /*
    * If Port and Hostname/IP are same, we are talking to the same
    * Storage Daemon
    */
   if (jcr->res.rstore->SDport != new_store->SDport ||
       !bstrcmp(jcr->res.rstore->address, new_store->address)) {
      return false;
   }

   return true;
}

/*
 * Check if the current line contains Storage="xxx", and compare the
 * result to the current storage. We use UAContext to analyse the bsr
 * string.
 *
 * Returns true if we need to change the storage, and it set the new
 * Storage resource name in "storage" arg.
 */
static inline bool check_for_new_storage(JCR *jcr, bootstrap_info &info)
{
   UAContext *ua = info.ua;

   parse_ua_args(ua);
   if (ua->argc != 1) {
      return false;
   }

   if (bstrcasecmp(ua->argk[0], "Storage")) {
      /*
       * Continue if this is a volume from the same storage.
       */
      if (is_on_same_storage(jcr, ua->argv[0])) {
         return false;
      }

      /*
       * Note the next storage name
       */
      bstrncpy(info.storage, ua->argv[0], MAX_NAME_LENGTH);
      Dmsg1(5, "Change storage to %s\n", info.storage);
      return true;
   }

   return false;
}

/*
 * Send bootstrap file to Storage daemon section by section.
 */
bool send_bootstrap_file(JCR *jcr, BSOCK *sock, bootstrap_info &info)
{
   boffset_t pos;
   const char *bootstrap = "bootstrap\n";
   UAContext *ua = info.ua;
   FILE *bs = info.bs;

   Dmsg1(400, "send_bootstrap_file: %s\n", jcr->RestoreBootstrap);
   if (!jcr->RestoreBootstrap) {
      return false;
   }

   sock->fsend(bootstrap);
   pos = ftello(bs);

   while (fgets(ua->cmd, UA_CMD_SIZE, bs)) {
      if (check_for_new_storage(jcr, info)) {
         /*
          * Otherwise, we need to contact another storage daemon.
          * Reset bs to the beginning of the current segment.
          */
         fseeko(bs, pos, SEEK_SET);
         break;
      }
      sock->fsend("%s", ua->cmd);
      pos = ftello(bs);
   }

   sock->signal(BNET_EOD);

   return true;
}

/*
 * Clean the bootstrap_info struct
 */
void close_bootstrap_file(bootstrap_info &info)
{
   if (info.bs) {
      fclose(info.bs);
   }
   if (info.ua) {
      free_ua_context(info.ua);
   }
}
