/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * Written by Marco van Wieringen and Philipp Storz, April 2014
 */
/**
 * @file
 * Statistics collector thread.
 */

#include "bareos.h"
#include "dird.h"

/*
 * Commands received from storage daemon that need scanning
 */
static char DevStats[] =
   "Devicestats [%lld]: Device=%s Read=%llu, Write=%llu, SpoolSize=%llu, NumWaiting=%lu, NumWriters=%lu, "
   "ReadTime=%lld, WriteTime=%lld, MediaId=%ld, VolBytes=%llu, VolFiles=%llu, VolBlocks=%llu";
static char TapeAlerts[] =
   "Tapealerts [%lld]: Device=%s TapeAlert=%llu";
static char JobStats[] =
   "Jobstats [%lld]: JobId=%ld, JobFiles=%lu, JobBytes=%llu, DevName=%s";

/* Static globals */
static bool quit = false;
static bool statistics_initialized = false;
static bool need_flush = true;
static pthread_t statistics_tid;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_for_next_run_cond = PTHREAD_COND_INITIALIZER;

/*
 * Cache the last lookup of a DeviceId.
 */
static struct cached_device {
   char device_name[MAX_NAME_LENGTH];
   DBId_t StorageId;
   DBId_t DeviceId;
} cached_device;

static inline bool lookup_device(JCR *jcr, const char *device_name, DBId_t StorageId, DBId_t *DeviceId)
{
   DEVICE_DBR dr;

   memset(&dr, 0, sizeof(dr));

   /*
    * See if we can use the cached DeviceId.
    */
   if (cached_device.StorageId == StorageId &&
       bstrcmp(cached_device.device_name, device_name)) {
      *DeviceId = cached_device.DeviceId;
      return true;
   }

   /*
    * Find or create device record
    */
   dr.StorageId = StorageId;
   bstrncpy(dr.Name, device_name, sizeof(dr.Name));
   if (!jcr->db->create_device_record(jcr, &dr)){
      Dmsg0(100, "Failed to create new Device record\n");
      goto bail_out;
   }

   Dmsg3(200, "Deviceid of \"%s\" on StorageId %d is %d\n", dr.Name, dr.StorageId, dr.DeviceId);

   /*
    * Cache the result.
    */
   bstrncpy(cached_device.device_name, device_name, sizeof(cached_device.device_name));
   cached_device.StorageId = StorageId;
   cached_device.DeviceId = dr.DeviceId;

   *DeviceId = dr.DeviceId;
   return true;

bail_out:
   return false;
}

/**
 * Wait for the next run.
 */
static inline void wait_for_next_run()
{
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;

   /*
    * Wait for a next run. Normally this waits exactly me->stats_collect_interval seconds.
    * It can be interrupted when signaled by the stop_statistics_thread() function.
    */
   gettimeofday(&tv, &tz);
   timeout.tv_nsec = tv.tv_usec * 1000;
   timeout.tv_sec = tv.tv_sec + me->stats_collect_interval;

   P(mutex);
   pthread_cond_timedwait(&wait_for_next_run_cond, &mutex, &timeout);
   V(mutex);
}

/**
 * Entry point for a separate statistics thread.
 */
extern "C"
void *statistics_thread_runner(void *arg)
{
   JCR *jcr;
   utime_t now;
   POOL_MEM current_store(PM_NAME);

   memset(&cached_device, 0, sizeof(struct cached_device));
   pm_strcpy(current_store, "");

   /*
    * Create a dummy JCR for the statistics thread.
    */
   jcr = new_control_jcr("*StatisticsCollector*", JT_SYSTEM);

   /*
    * Open a connection to the database for storing long term statistics.
    */
   jcr->res.catalog = (CATRES *)GetNextRes(R_CATALOG, NULL);
   jcr->db = db_sql_get_pooled_connection(jcr,
                                          jcr->res.catalog->db_driver,
                                          jcr->res.catalog->db_name,
                                          jcr->res.catalog->db_user,
                                          jcr->res.catalog->db_password.value,
                                          jcr->res.catalog->db_address,
                                          jcr->res.catalog->db_port,
                                          jcr->res.catalog->db_socket,
                                          jcr->res.catalog->mult_db_connections,
                                          jcr->res.catalog->disable_batch_insert,
                                          jcr->res.catalog->try_reconnect,
                                          jcr->res.catalog->exit_on_fatal);
   if (jcr->db == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Could not open database \"%s\".\n"), jcr->res.catalog->db_name);
      goto bail_out;
   }

   /*
    * Do our work as long as we are not signaled to quit.
    */
   while (!quit) {
      now = (utime_t)time(NULL);

      Dmsg1(200, "statistics_thread_runner: Doing work at %ld\n", now);

      /*
       * Do nothing if no job is running currently.
       */
      if (job_count() == 0) {
         if (!need_flush) {
            Dmsg0(200, "statistics_thread_runner: do nothing as no jobs are running\n");
            wait_for_next_run();
            continue;
         } else {
            /*
             * Flush any pending statistics data one more time and then sleep until new jobs start running.
             */
            Dmsg0(200, "statistics_thread_runner: flushing pending statistics\n");
            need_flush = false;
         }
      } else {
         /*
          * We have running jobs so on a next run we still need to flush any collected data.
          */
         need_flush = true;
      }

      while (1) {
         BSOCK *sd;
         STORERES *store;
         int64_t StorageId;

         LockRes();
         if ((current_store.c_str())[0]) {
            store = (STORERES *)GetResWithName(R_STORAGE, current_store.c_str());
         } else {
            store = NULL;
         }

         store = (STORERES *)GetNextRes(R_STORAGE, (RES *)store);
         if (!store) {
            pm_strcpy(current_store, "");
            UnlockRes();
            break;
         }

         pm_strcpy(current_store, store->name());
         if (!store->collectstats) {
            UnlockRes();
            continue;
         }

         switch (store->Protocol) {
         case APT_NATIVE:
            break;
         default:
            UnlockRes();
            continue;
         }

         /*
          * Try connecting 2 times with a max time to wait of 1 seconds.
          * We don't want to lock the resources to long. And as the stored
          * will cache the stats anyway we can always try collecting things
          * in the next run.
          */
         jcr->res.rstore = store;
         if (!connect_to_storage_daemon(jcr, 2, 1, false)) {
            UnlockRes();
            continue;
         }

         StorageId = store->StorageId;
         sd = jcr->store_bsock;

         UnlockRes();

         /*
          * Do our work retrieving the statistics from the remote SD.
          */
         if (sd->fsend("stats")) {
            while (bnet_recv(sd) >= 0) {
               Dmsg1(200, "<stored: %s", sd->msg);
               if (bstrncmp(sd->msg, "Devicestats", 10)) {
                  POOL_MEM DevName(PM_NAME);
                  DEVICE_STATS_DBR dsr;

                  memset(&dsr, 0, sizeof(dsr));
                  if (sscanf(sd->msg, DevStats, &dsr.SampleTime, DevName.c_str(), &dsr.ReadBytes,
                             &dsr.WriteBytes, &dsr.SpoolSize, &dsr.NumWaiting, &dsr.NumWriters,
                             &dsr.ReadTime, &dsr.WriteTime, &dsr.MediaId,
                             &dsr.VolCatBytes, &dsr.VolCatFiles, &dsr.VolCatBlocks) == 13) {

                     Dmsg5(200, "New Devstats [%lld]: Device=%s Read=%llu, Write=%llu, SpoolSize=%llu,\n",
                           dsr.SampleTime, DevName.c_str(), dsr.ReadBytes, dsr.WriteBytes, dsr.SpoolSize);
                     Dmsg4(200, "NumWaiting=%lu, NumWriters=%lu, ReadTime=%lld, WriteTime=%lld,\n",
                           dsr.NumWaiting, dsr.NumWriters, dsr.ReadTime, dsr.WriteTime);
                     Dmsg4(200, "MediaId=%ld, VolBytes=%llu, VolFiles=%llu, VolBlocks=%llu\n",
                           dsr.MediaId, dsr.VolCatBytes, dsr.VolCatFiles, dsr.VolCatBlocks);

                     if (!lookup_device(jcr, DevName.c_str(), StorageId, &dsr.DeviceId)) {
                        continue;
                     }

                     if (!jcr->db->create_device_statistics(jcr, &dsr)) {
                        continue;
                     }
                  } else {
                     Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), sd->msg);
                  }
               } else if (bstrncmp(sd->msg, "Tapealerts", 10)) {
                  POOL_MEM DevName(PM_NAME);
                  TAPEALERT_STATS_DBR tsr;

                  memset(&tsr, 0, sizeof(tsr));
                  if (sscanf(sd->msg, TapeAlerts, &tsr.SampleTime, DevName.c_str(), &tsr.AlertFlags) == 3) {
                     unbash_spaces(DevName);

                     Dmsg3(200, "New stats [%lld]: Device %s TapeAlert %llu\n",
                           tsr.SampleTime, DevName.c_str(), tsr.AlertFlags);

                     if (!lookup_device(jcr, DevName.c_str(), StorageId, &tsr.DeviceId)) {
                        continue;
                     }

                     if (!jcr->db->create_tapealert_statistics(jcr, &tsr)) {
                        continue;
                     }
                  } else {
                     Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), sd->msg);
                  }
               } else if (bstrncmp(sd->msg, "Jobstats", 8)) {
                  POOL_MEM DevName(PM_NAME);
                  JOB_STATS_DBR jsr;

                  memset(&jsr, 0, sizeof(jsr));
                  if (sscanf(sd->msg, JobStats, &jsr.SampleTime, &jsr.JobId, &jsr.JobFiles, &jsr.JobBytes, DevName.c_str()) == 5) {
                     unbash_spaces(DevName);

                     Dmsg5(200, "New Jobstats [%lld]: JobId %ld, JobFiles %lu, JobBytes %llu, DevName %s\n",
                           jsr.SampleTime, jsr.JobId, jsr.JobFiles, jsr.JobBytes, DevName.c_str());

                     if (!lookup_device(jcr, DevName.c_str(), StorageId, &jsr.DeviceId)) {
                        continue;
                     }

                     if (!jcr->db->create_job_statistics(jcr, &jsr)) {
                        continue;
                     }
                  } else {
                     Jmsg1(jcr, M_ERROR, 0, _("Malformed message: %s\n"), sd->msg);
                  }
               }
            }
         }

         /*
          * Disconnect.
          */
         jcr->store_bsock->close();
         delete jcr->store_bsock;
         jcr->store_bsock = NULL;
      }

      wait_for_next_run();
   }

   db_sql_close_pooled_connection(jcr, jcr->db);

bail_out:
   free_jcr(jcr);

   return NULL;
}

int start_statistics_thread(void)
{
   int status;

   if (!me->stats_collect_interval) {
      return 0;
   }

   if ((status = pthread_create(&statistics_tid, NULL, statistics_thread_runner, NULL)) != 0) {
      return status;
   }

   statistics_initialized = true;

   return 0;
}

void stop_statistics_thread()
{
   if (!statistics_initialized) {
      return;
   }

   quit = true;
   pthread_cond_broadcast(&wait_for_next_run_cond);
   if (!pthread_equal(statistics_tid, pthread_self())) {
      pthread_join(statistics_tid, NULL);
   }
}

void stats_job_started()
{
   /*
    * A new Job was started so we need to flush any pending statistics the next run.
    */
   if (statistics_initialized) {
      need_flush = true;
   }
}
