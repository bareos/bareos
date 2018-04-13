/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March 2004
 */
/**
 * @file
 * Spooling code
 */

#include "bareos.h"
#include "stored.h"

/* Forward referenced subroutines */
static void make_unique_data_spool_filename(DeviceControlRecord *dcr, POOLMEM *&name);
static bool open_data_spool_file(DeviceControlRecord *dcr);
static bool close_data_spool_file(DeviceControlRecord *dcr, bool end_of_spool);
static bool despool_data(DeviceControlRecord *dcr, bool commit);
static int  read_block_from_spool_file(DeviceControlRecord *dcr);
static bool open_attr_spool_file(JobControlRecord *jcr, BareosSocket *bs);
static bool close_attr_spool_file(JobControlRecord *jcr, BareosSocket *bs);
static bool write_spool_header(DeviceControlRecord *dcr);
static bool write_spool_data(DeviceControlRecord *dcr);

struct spool_stats_t {
   uint32_t data_jobs;                /* current jobs spooling data */
   uint32_t attr_jobs;
   uint32_t total_data_jobs;          /* total jobs to have spooled data */
   uint32_t total_attr_jobs;
   int64_t max_data_size;             /* max data size */
   int64_t max_attr_size;
   int64_t data_size;                 /* current data size (all jobs running) */
   int64_t attr_size;
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static spool_stats_t spool_stats;

/**
 * Header for data spool record */
struct spool_hdr {
   int32_t  FirstIndex;               /* FirstIndex for buffer */
   int32_t  LastIndex;                /* LastIndex for buffer */
   uint32_t len;                      /* length of next buffer */
};

enum {
   RB_EOT = 1,
   RB_ERROR,
   RB_OK
};

void list_spool_stats(void sendit(const char *msg, int len, void *sarg), void *arg)
{
   char ed1[30], ed2[30];
   PoolMem msg(PM_MESSAGE);
   int len;

   len = Mmsg(msg, _("Spooling statistics:\n"));

   if (spool_stats.data_jobs || spool_stats.max_data_size) {
      len = Mmsg(msg, _("Data spooling: %u active jobs, %s bytes; %u total jobs, %s max bytes/job.\n"),
         spool_stats.data_jobs, edit_uint64_with_commas(spool_stats.data_size, ed1),
         spool_stats.total_data_jobs,
         edit_uint64_with_commas(spool_stats.max_data_size, ed2));

      sendit(msg.c_str(), len, arg);
   }
   if (spool_stats.attr_jobs || spool_stats.max_attr_size) {
      len = Mmsg(msg, _("Attr spooling: %u active jobs, %s bytes; %u total jobs, %s max bytes.\n"),
         spool_stats.attr_jobs, edit_uint64_with_commas(spool_stats.attr_size, ed1),
         spool_stats.total_attr_jobs,
         edit_uint64_with_commas(spool_stats.max_attr_size, ed2));

      sendit(msg.c_str(), len, arg);
   }
}

bool begin_data_spool(DeviceControlRecord *dcr)
{
   bool status = true;

   if (dcr->jcr->spool_data) {
      Dmsg0(100, "Turning on data spooling\n");
      dcr->spool_data = true;
      status = open_data_spool_file(dcr);
      if (status) {
         dcr->spooling = true;
         Jmsg(dcr->jcr, M_INFO, 0, _("Spooling data ...\n"));
         P(mutex);
         spool_stats.data_jobs++;
         V(mutex);
      }
   }

   return status;
}

bool discard_data_spool(DeviceControlRecord *dcr)
{
   if (dcr->spooling) {
      Dmsg0(100, "Data spooling discarded\n");
      return close_data_spool_file(dcr, true);
   }

   return true;
}

bool commit_data_spool(DeviceControlRecord *dcr)
{
   bool status;

   if (dcr->spooling) {
      Dmsg0(100, "Committing spooled data\n");
      status = despool_data(dcr, true /*commit*/);
      if (!status) {
         Dmsg1(100, _("Bad return from despool WroteVol=%d\n"), dcr->WroteVol);
         close_data_spool_file(dcr, true);
         return false;
      }
      return close_data_spool_file(dcr, true);
   }

   return true;
}

static void make_unique_data_spool_filename(DeviceControlRecord *dcr, POOLMEM *&name)
{
   const char *dir;

   if (dcr->dev->device->spool_directory) {
      dir = dcr->dev->device->spool_directory;
   } else {
      dir = working_directory;
   }

   Mmsg(name, "%s/%s.data.%u.%s.%s.spool", dir, my_name,
        dcr->jcr->JobId, dcr->jcr->Job, dcr->device->name());
}

static bool open_data_spool_file(DeviceControlRecord *dcr)
{
   int spool_fd;
   POOLMEM *name = get_pool_memory(PM_MESSAGE);

   make_unique_data_spool_filename(dcr, name);
   if ((spool_fd = open(name, O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0640)) >= 0) {
      dcr->spool_fd = spool_fd;
      dcr->jcr->spool_attributes = true;
   } else {
      berrno be;

      Jmsg(dcr->jcr, M_FATAL, 0, _("Open data spool file %s failed: ERR=%s\n"), name, be.bstrerror());
      free_pool_memory(name);
      return false;
   }
   Dmsg1(100, "Created spool file: %s\n", name);
   free_pool_memory(name);

   return true;
}

static bool close_data_spool_file(DeviceControlRecord *dcr, bool end_of_spool)
{
   POOLMEM *name = get_pool_memory(PM_MESSAGE);

   close(dcr->spool_fd);
   dcr->spool_fd = -1;
   dcr->spooling = false;

   make_unique_data_spool_filename(dcr, name);
   secure_erase(dcr->jcr, name);
   Dmsg1(100, "Deleted spool file: %s\n", name);
   free_pool_memory(name);

   P(mutex);
   spool_stats.data_jobs--;
   if (end_of_spool) {
      spool_stats.total_data_jobs++;
   }
   if (spool_stats.data_size < dcr->job_spool_size) {
      spool_stats.data_size = 0;
   } else {
      spool_stats.data_size -= dcr->job_spool_size;
   }
   V(mutex);

   P(dcr->dev->spool_mutex);
   dcr->dev->spool_size -= dcr->job_spool_size;
   dcr->job_spool_size = 0;
   V(dcr->dev->spool_mutex);

   return true;
}

static const char *spool_name = "*spool*";

/**
 * NB! This routine locks the device, but if committing will
 *     not unlock it. If not committing, it will be unlocked.
 */
static bool despool_data(DeviceControlRecord *dcr, bool commit)
{
   Device *rdev;
   DeviceControlRecord *rdcr;
   bool ok = true;
   DeviceBlock *block;
   JobControlRecord *jcr = dcr->jcr;
   int status;
   char ec1[50];
   BareosSocket *dir = jcr->dir_bsock;

   Dmsg0(100, "Despooling data\n");
   if (jcr->dcr->job_spool_size == 0) {
      Jmsg(jcr, M_WARNING, 0, _("Despooling zero bytes. Your disk is probably FULL!\n"));
   }

   /*
    * Commit means that the job is done, so we commit, otherwise, we
    * are despooling because of user spool size max or some error
    * (e.g. filesystem full).
    */
   if (commit) {
      Jmsg(jcr, M_INFO, 0, _("Committing spooled data to Volume \"%s\". Despooling %s bytes ...\n"),
         jcr->dcr->VolumeName,
         edit_uint64_with_commas(jcr->dcr->job_spool_size, ec1));
      jcr->setJobStatus(JS_DataCommitting);
   } else {
      Jmsg(jcr, M_INFO, 0, _("Writing spooled data to Volume. Despooling %s bytes ...\n"),
         edit_uint64_with_commas(jcr->dcr->job_spool_size, ec1));
      jcr->setJobStatus(JS_DataDespooling);
   }
   jcr->sendJobStatus(JS_DataDespooling);
   dcr->despool_wait = true;
   dcr->spooling = false;
   /*
    * We work with device blocked, but not locked so that other threads
    * e.g. reservations can lock the device structure.
    */
   dcr->dblock(BST_DESPOOLING);
   dcr->despool_wait = false;
   dcr->despooling = true;

   /*
    * This is really quite kludgy and should be fixed some time.
    * We create a dev structure to read from the spool file
    * in rdev and rdcr.
    */
   rdev = (Device *)malloc(sizeof(Device));
   memset(rdev, 0, sizeof(Device));
   rdev->dev_name = get_memory(strlen(spool_name)+1);
   bstrncpy(rdev->dev_name, spool_name, sizeof_pool_memory(rdev->dev_name));
   rdev->errmsg = get_pool_memory(PM_EMSG);
   *rdev->errmsg = 0;
   rdev->max_block_size = dcr->dev->max_block_size;
   rdev->min_block_size = dcr->dev->min_block_size;
   rdev->device = dcr->dev->device;
   rdcr = dcr->get_new_spooling_dcr();
   setup_new_dcr_device(jcr, rdcr, rdev, NULL);
   rdcr->spool_fd = dcr->spool_fd;
   block = dcr->block;                /* save block */
   dcr->block = rdcr->block;          /* make read and write block the same */

   Dmsg1(800, "read/write block size = %d\n", block->buf_len);
   lseek(rdcr->spool_fd, 0, SEEK_SET); /* rewind */

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   posix_fadvise(rdcr->spool_fd, 0, 0, POSIX_FADV_WILLNEED);
#endif

   /* Add run time, to get current wait time */
   int32_t despool_start = time(NULL) - jcr->run_time;

   set_new_file_parameters(dcr);

   while (ok) {
      if (job_canceled(jcr)) {
         ok = false;
         break;
      }
      status = read_block_from_spool_file(rdcr);
      if (status == RB_EOT) {
         break;
      } else if (status == RB_ERROR) {
         ok = false;
         break;
      }
      ok = dcr->write_block_to_device();
      if (!ok) {
         Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
               dcr->dev->print_name(), dcr->dev->bstrerror());
         Dmsg2(000, "Fatal append error on device %s: ERR=%s\n",
               dcr->dev->print_name(), dcr->dev->bstrerror());
         /* Force in case Incomplete set */
         jcr->forceJobStatus(JS_FatalError);
      }
      Dmsg3(800, "Write block ok=%d FI=%d LI=%d\n", ok, block->FirstIndex, block->LastIndex);
   }

   /*
    * If this Job is incomplete, we need to backup the FileIndex
    *  to the last correctly saved file so that the JobMedia
    *  LastIndex is correct.
    */
   if (jcr->is_JobStatus(JS_Incomplete)) {
      dcr->VolLastIndex = dir->get_FileIndex();
      Dmsg1(100, "======= Set FI=%ld\n", dir->get_FileIndex());
   }

   if (!dcr->dir_create_jobmedia_record(false)) {
      Jmsg2(jcr, M_FATAL, 0, _("Could not create JobMedia record for Volume=\"%s\" Job=%s\n"),
         dcr->getVolCatName(), jcr->Job);
      jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
   }
   /* Set new file/block parameters for current dcr */
   set_new_file_parameters(dcr);

   /*
    * Subtracting run_time give us elapsed time - wait_time since
    * we started despooling. Note, don't use time_t as it is 32 or 64
    * bits depending on the OS and doesn't edit with %d
    */
   int32_t despool_elapsed = time(NULL) - despool_start - jcr->run_time;

   if (despool_elapsed <= 0) {
      despool_elapsed = 1;
   }

   Jmsg(jcr, M_INFO, 0, _("Despooling elapsed time = %02d:%02d:%02d, Transfer rate = %s Bytes/second\n"),
        despool_elapsed / 3600, despool_elapsed % 3600 / 60, despool_elapsed % 60,
        edit_uint64_with_suffix(jcr->dcr->job_spool_size / despool_elapsed, ec1));

   dcr->block = block;                /* reset block */

   /*
    * See if we are using secure erase.
    */
   if (me->secure_erase_cmdline) {
      close_data_spool_file(dcr, false);
      begin_data_spool(dcr);
   } else {
      lseek(rdcr->spool_fd, 0, SEEK_SET); /* rewind */
      if (ftruncate(rdcr->spool_fd, 0) != 0) {
         berrno be;

         Jmsg(jcr, M_ERROR, 0, _("Ftruncate spool file failed: ERR=%s\n"), be.bstrerror());
         /*
          * Note, try continuing despite ftruncate problem
          */
      }

      P(mutex);
      if (spool_stats.data_size < dcr->job_spool_size) {
         spool_stats.data_size = 0;
      } else {
         spool_stats.data_size -= dcr->job_spool_size;
      }
      V(mutex);
      P(dcr->dev->spool_mutex);
      dcr->dev->spool_size -= dcr->job_spool_size;
      dcr->job_spool_size = 0;            /* zap size in input dcr */
      V(dcr->dev->spool_mutex);
   }

   free_memory(rdev->dev_name);
   free_pool_memory(rdev->errmsg);

   /*
    * Be careful to NULL the jcr and free rdev after free_dcr()
    */
   rdcr->jcr = NULL;
   rdcr->set_dev(NULL);
   free_dcr(rdcr);
   free(rdev);
   dcr->spooling = true;           /* turn on spooling again */
   dcr->despooling = false;

   /*
    * Note, if committing we leave the device blocked. It will be removed in release_device();
    */
   if (!commit) {
      dcr->dev->dunblock();
   }
   jcr->sendJobStatus(JS_Running);

   return ok;
}

/**
 * Read a block from the spool file
 *
 *  Returns RB_OK on success
 *          RB_EOT when file done
 *          RB_ERROR on error
 */
static int read_block_from_spool_file(DeviceControlRecord *dcr)
{
   uint32_t rlen;
   ssize_t status;
   spool_hdr hdr;
   DeviceBlock *block = dcr->block;
   JobControlRecord *jcr = dcr->jcr;

   rlen = sizeof(hdr);
   status = read(dcr->spool_fd, (char *)&hdr, (size_t)rlen);
   if (status == 0) {
      Dmsg0(100, "EOT on spool read.\n");
      return RB_EOT;
   } else if (status != (ssize_t)rlen) {
      if (status == -1) {
         berrno be;

         Jmsg(dcr->jcr, M_FATAL, 0, _("Spool header read error. ERR=%s\n"), be.bstrerror());
      } else {
         Pmsg2(000, _("Spool read error. Wanted %u bytes, got %d\n"), rlen, status);
         Jmsg2(jcr, M_FATAL, 0, _("Spool header read error. Wanted %u bytes, got %d\n"), rlen, status);
      }
      jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
      return RB_ERROR;
   }
   rlen = hdr.len;
   if (rlen > block->buf_len) {
      Pmsg2(000, _("Spool block too big. Max %u bytes, got %u\n"), block->buf_len, rlen);
      Jmsg2(jcr, M_FATAL, 0, _("Spool block too big. Max %u bytes, got %u\n"), block->buf_len, rlen);
      jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
      return RB_ERROR;
   }
   status = read(dcr->spool_fd, (char *)block->buf, (size_t)rlen);
   if (status != (ssize_t)rlen) {
      Pmsg2(000, _("Spool data read error. Wanted %u bytes, got %d\n"), rlen, status);
      Jmsg2(dcr->jcr, M_FATAL, 0, _("Spool data read error. Wanted %u bytes, got %d\n"), rlen, status);
      jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
      return RB_ERROR;
   }
   /* Setup write pointers */
   block->binbuf = rlen;
   block->bufp = block->buf + block->binbuf;
   block->FirstIndex = hdr.FirstIndex;
   block->LastIndex = hdr.LastIndex;
   block->VolSessionId = dcr->jcr->VolSessionId;
   block->VolSessionTime = dcr->jcr->VolSessionTime;
   Dmsg2(800, "Read block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);
   return RB_OK;
}

/**
 * Write a block to the spool file
 *
 *  Returns: true on success or EOT
 *           false on hard error
 */
bool write_block_to_spool_file(DeviceControlRecord *dcr)
{
   uint32_t wlen, hlen;               /* length to write */
   bool despool = false;
   DeviceBlock *block = dcr->block;

   if (job_canceled(dcr->jcr)) {
      return false;
   }
   ASSERT(block->binbuf == ((uint32_t) (block->bufp - block->buf)));
   if (block->binbuf <= WRITE_BLKHDR_LENGTH) {  /* Does block have data in it? */
      return true;
   }

   hlen = sizeof(spool_hdr);
   wlen = block->binbuf;
   P(dcr->dev->spool_mutex);
   dcr->job_spool_size += hlen + wlen;
   dcr->dev->spool_size += hlen + wlen;
   if ((dcr->max_job_spool_size > 0 && dcr->job_spool_size >= dcr->max_job_spool_size) ||
       (dcr->dev->max_spool_size > 0 && dcr->dev->spool_size >= dcr->dev->max_spool_size)) {
      despool = true;
   }
   V(dcr->dev->spool_mutex);
   P(mutex);
   spool_stats.data_size += hlen + wlen;
   if (spool_stats.data_size > spool_stats.max_data_size) {
      spool_stats.max_data_size = spool_stats.data_size;
   }
   V(mutex);
   if (despool) {
      char ec1[30], ec2[30];
      if (dcr->max_job_spool_size > 0) {
         Jmsg(dcr->jcr, M_INFO, 0, _("User specified Job spool size reached: "
            "JobSpoolSize=%s MaxJobSpoolSize=%s\n"),
            edit_uint64_with_commas(dcr->job_spool_size, ec1),
            edit_uint64_with_commas(dcr->max_job_spool_size, ec2));
      } else {
         Jmsg(dcr->jcr, M_INFO, 0, _("User specified Device spool size reached: "
            "DevSpoolSize=%s MaxDevSpoolSize=%s\n"),
            edit_uint64_with_commas(dcr->dev->spool_size, ec1),
            edit_uint64_with_commas(dcr->dev->max_spool_size, ec2));
      }

      if (!despool_data(dcr, false)) {
         Pmsg0(000, _("Bad return from despool in write_block.\n"));
         return false;
      }
      /* Despooling cleared these variables so reset them */
      P(dcr->dev->spool_mutex);
      dcr->job_spool_size += hlen + wlen;
      dcr->dev->spool_size += hlen + wlen;
      V(dcr->dev->spool_mutex);
      Jmsg(dcr->jcr, M_INFO, 0, _("Spooling data again ...\n"));
   }


   if (!write_spool_header(dcr)) {
      return false;
   }
   if (!write_spool_data(dcr)) {
     return false;
   }

   Dmsg2(800, "Wrote block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);
   empty_block(block);
   return true;
}

static bool write_spool_header(DeviceControlRecord *dcr)
{
   spool_hdr hdr;
   ssize_t status;
   DeviceBlock *block = dcr->block;
   JobControlRecord *jcr = dcr->jcr;

   hdr.FirstIndex = block->FirstIndex;
   hdr.LastIndex = block->LastIndex;
   hdr.len = block->binbuf;

   /* Write header */
   for (int retry = 0; retry <= 1; retry++) {
      status = write(dcr->spool_fd, (char*)&hdr, sizeof(hdr));
      if (status == -1) {
         berrno be;

         Jmsg(jcr, M_FATAL, 0, _("Error writing header to spool file. ERR=%s\n"), be.bstrerror());
         jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
      }
      if (status != (ssize_t)sizeof(hdr)) {
         Jmsg(jcr, M_ERROR, 0, _("Error writing header to spool file."
              " Disk probably full. Attempting recovery. Wanted to write=%d got=%d\n"),
              (int)status, (int)sizeof(hdr));
         /* If we wrote something, truncate it, then despool */
         if (status != -1) {
#if defined(HAVE_WIN32)
            boffset_t   pos = _lseeki64(dcr->spool_fd, (__int64)0, SEEK_CUR);
#else
            boffset_t   pos = lseek(dcr->spool_fd, 0, SEEK_CUR);
#endif
            if (ftruncate(dcr->spool_fd, pos - status) != 0) {
               berrno be;

               Jmsg(dcr->jcr, M_ERROR, 0, _("Ftruncate spool file failed: ERR=%s\n"), be.bstrerror());
              /* Note, try continuing despite ftruncate problem */
            }
         }
         if (!despool_data(dcr, false)) {
            Jmsg(jcr, M_FATAL, 0, _("Fatal despooling error."));
            jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
            return false;
         }
         continue;                    /* try again */
      }
      return true;
   }
   Jmsg(jcr, M_FATAL, 0, _("Retrying after header spooling error failed.\n"));
   jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
   return false;
}

static bool write_spool_data(DeviceControlRecord *dcr)
{
   ssize_t status;
   DeviceBlock *block = dcr->block;
   JobControlRecord *jcr = dcr->jcr;

   /*
    * Write data
    */
   for (int retry = 0; retry <= 1; retry++) {
      status = write(dcr->spool_fd, block->buf, (size_t)block->binbuf);
      if (status == -1) {
         berrno be;

         Jmsg(jcr, M_FATAL, 0, _("Error writing data to spool file. ERR=%s\n"), be.bstrerror());
         jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
      }
      if (status != (ssize_t)block->binbuf) {
         /*
          * If we wrote something, truncate it and the header, then despool
          */
         if (status != -1) {
#if defined(HAVE_WIN32)
            boffset_t   pos = _lseeki64(dcr->spool_fd, (__int64)0, SEEK_CUR);
#else
            boffset_t   pos = lseek(dcr->spool_fd, 0, SEEK_CUR);
#endif
            if (ftruncate(dcr->spool_fd, pos - status - sizeof(spool_hdr)) != 0) {
               berrno be;

               Jmsg(dcr->jcr, M_ERROR, 0, _("Ftruncate spool file failed: ERR=%s\n"), be.bstrerror());
               /* Note, try continuing despite ftruncate problem */
            }
         }

         if (!despool_data(dcr, false)) {
            Jmsg(jcr, M_FATAL, 0, _("Fatal despooling error."));
            jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
            return false;
         }

         if (!write_spool_header(dcr)) {
            return false;
         }

         continue;                    /* try again */
      }

      return true;
   }

   Jmsg(jcr, M_FATAL, 0, _("Retrying after data spooling error failed.\n"));
   jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */

   return false;
}

bool are_attributes_spooled(JobControlRecord *jcr)
{
   return jcr->spool_attributes && jcr->dir_bsock->spool_fd_ != -1;
}

/**
 * Create spool file for attributes.
 *  This is done by "attaching" to the bsock, and when
 *  it is called, the output is written to a file.
 *  The actual spooling is turned on and off in
 *  append.c only during writing of the attributes.
 */
bool begin_attribute_spool(JobControlRecord *jcr)
{
   if (!jcr->no_attributes && jcr->spool_attributes) {
      return open_attr_spool_file(jcr, jcr->dir_bsock);
   }
   return true;
}

bool discard_attribute_spool(JobControlRecord *jcr)
{
   if (are_attributes_spooled(jcr)) {
      return close_attr_spool_file(jcr, jcr->dir_bsock);
   }
   return true;
}

static void update_attr_spool_size(ssize_t size)
{
   P(mutex);
   if (size > 0) {
     if ((spool_stats.attr_size - size) > 0) {
        spool_stats.attr_size -= size;
     } else {
        spool_stats.attr_size = 0;
     }
   }
   V(mutex);
}

static void make_unique_spool_filename(JobControlRecord *jcr, POOLMEM *&name, int fd)
{
   Mmsg(name, "%s/%s.attr.%s.%d.spool", working_directory, my_name, jcr->Job, fd);
}

/**
 * Tell Director where to find the attributes spool file
 *  Note, if we are not on the same machine, the Director will
 *  return an error, and the higher level routine will transmit
 *  the data record by record -- using bsock->despool().
 */
static bool blast_attr_spool_file(JobControlRecord *jcr, boffset_t size)
{
   POOLMEM *name = get_pool_memory(PM_MESSAGE);

   /*
    * Send full spool file name
    */
   make_unique_spool_filename(jcr, name, jcr->dir_bsock->fd_);
   bash_spaces(name);
   jcr->dir_bsock->fsend("BlastAttr Job=%s File=%s\n", jcr->Job, name);
   free_pool_memory(name);

   if (jcr->dir_bsock->recv() <= 0) {
      Jmsg(jcr, M_FATAL, 0, _("Network error on BlastAttributes.\n"));
      jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
      return false;
   }

   if (!bstrcmp(jcr->dir_bsock->msg, "1000 OK BlastAttr\n")) {
      return false;
   }

   return true;
}

bool commit_attribute_spool(JobControlRecord *jcr)
{
   boffset_t size, data_end;
   char ec1[30];
   char tbuf[MAX_TIME_LENGTH];
   BareosSocket *dir;

   Dmsg1(100, "Commit attributes at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
   if (are_attributes_spooled(jcr)) {
      dir = jcr->dir_bsock;
      if ((size = lseek(dir->spool_fd_, 0, SEEK_END)) == -1) {
         berrno be;

         Jmsg(jcr, M_FATAL, 0, _("lseek on attributes file failed: ERR=%s\n"), be.bstrerror());
         jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
         goto bail_out;
      }

      if (jcr->is_JobStatus(JS_Incomplete)) {
         data_end = dir->get_data_end();

         /*
          * Check and truncate to last valid data_end if necssary
          */
         if (size > data_end) {
            if (ftruncate(dir->spool_fd_, data_end) != 0) {
               berrno be;

               Jmsg(jcr, M_FATAL, 0, _("Truncate on attributes file failed: ERR=%s\n"), be.bstrerror());
               jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
               goto bail_out;
            }
            Dmsg2(100, "=== Attrib spool truncated from %lld to %lld\n", size, data_end);
            size = data_end;
         }
      }

      if (size < 0) {
         berrno be;

         Jmsg(jcr, M_FATAL, 0, _("Fseek on attributes file failed: ERR=%s\n"),
              be.bstrerror());
         jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
         goto bail_out;
      }

      P(mutex);
      if (spool_stats.attr_size + size > spool_stats.max_attr_size) {
         spool_stats.max_attr_size = spool_stats.attr_size + size;
      }
      spool_stats.attr_size += size;
      V(mutex);

      jcr->sendJobStatus(JS_AttrDespooling);
      Jmsg(jcr, M_INFO, 0, _("Sending spooled attrs to the Director. Despooling %s bytes ...\n"),
           edit_uint64_with_commas(size, ec1));

      if (!blast_attr_spool_file(jcr, size)) {
         /* Can't read spool file from director side,
          * send content over network.
          */
         dir->despool(update_attr_spool_size, size);
      }
      return close_attr_spool_file(jcr, dir);
   }
   return true;

bail_out:
   close_attr_spool_file(jcr, dir);
   return false;
}

static bool open_attr_spool_file(JobControlRecord *jcr, BareosSocket *bs)
{
   POOLMEM *name = get_pool_memory(PM_MESSAGE);

   make_unique_spool_filename(jcr, name, bs->fd_);
   bs->spool_fd_ = open(name, O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0640);
   if (bs->spool_fd_ == -1) {
      berrno be;

      Jmsg(jcr, M_FATAL, 0, _("fopen attr spool file %s failed: ERR=%s\n"), name, be.bstrerror());
      jcr->forceJobStatus(JS_FatalError);  /* override any Incomplete */
      free_pool_memory(name);
      return false;
   }

   P(mutex);
   spool_stats.attr_jobs++;
   V(mutex);

   free_pool_memory(name);

   return true;
}

static bool close_attr_spool_file(JobControlRecord *jcr, BareosSocket *bs)
{
   POOLMEM *name;
   char tbuf[MAX_TIME_LENGTH];

   Dmsg1(100, "Close attr spool file at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
   if (bs->spool_fd_ == -1) {
      return true;
   }

   name = get_pool_memory(PM_MESSAGE);

   P(mutex);
   spool_stats.attr_jobs--;
   spool_stats.total_attr_jobs++;
   V(mutex);

   make_unique_spool_filename(jcr, name, bs->fd_);

   close(bs->spool_fd_);
   secure_erase(jcr, name);
   free_pool_memory(name);
   bs->spool_fd_ = -1;
   bs->clear_spooling();

   return true;
}
