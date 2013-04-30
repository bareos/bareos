/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2012 Free Software Foundation Europe e.V.

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
 * SD -- mac.c --  responsible for doing
 *     migration, archive, copy, and virtual backup jobs.
 *
 *     Kern Sibbald, January MMVI
 *
 */

#include "bacula.h"
#include "stored.h"

/* Import functions */
extern char Job_end[];   

/* Forward referenced subroutines */
static bool record_cb(DCR *dcr, DEV_RECORD *rec);


/*
 *  Read Data and send to File Daemon
 *   Returns: false on failure
 *            true  on success
 */
bool do_mac(JCR *jcr)
{
   bool ok = true;
   BSOCK *dir = jcr->dir_bsock;
   const char *Type;
   char ec1[50];
   DEVICE *dev;

   switch(jcr->getJobType()) {
   case JT_MIGRATE:
      Type = "Migration";
      break;
   case JT_ARCHIVE:
      Type = "Archive";
      break;
   case JT_COPY:
      Type = "Copy";
      break;
   case JT_BACKUP:
      Type = "Virtual Backup";
      break;
   default:
      Type = "Unknown";
      break;
   }


   Dmsg0(20, "Start read data.\n");

   if (!jcr->read_dcr || !jcr->dcr) {
      Jmsg(jcr, M_FATAL, 0, _("Read and write devices not properly initialized.\n"));
      goto bail_out;
   }
   Dmsg2(100, "read_dcr=%p write_dcr=%p\n", jcr->read_dcr, jcr->dcr);

   if (jcr->NumReadVolumes == 0) {
      Jmsg(jcr, M_FATAL, 0, _("No Volume names found for %s.\n"), Type);
      goto bail_out;
   }

   Dmsg3(200, "Found %d volumes names for %s. First=%s\n", jcr->NumReadVolumes,
      jcr->VolList->VolumeName, Type);

   /* Ready devices for reading and writing */
   if (!acquire_device_for_read(jcr->read_dcr) ||
       !acquire_device_for_append(jcr->dcr)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      goto bail_out;
   }

   Dmsg2(200, "===== After acquire pos %u:%u\n", jcr->dcr->dev->file, jcr->dcr->dev->block_num);
     
   jcr->sendJobStatus(JS_Running);

   begin_data_spool(jcr->dcr);
   begin_attribute_spool(jcr);

   jcr->dcr->VolFirstIndex = jcr->dcr->VolLastIndex = 0;
   jcr->run_time = time(NULL);
   set_start_vol_position(jcr->dcr);

   jcr->JobFiles = 0;
   ok = read_records(jcr->read_dcr, record_cb, mount_next_read_volume);
   goto ok_out;

bail_out:
   ok = false;

ok_out:
   if (jcr->dcr) {
      dev = jcr->dcr->dev;
      Dmsg1(100, "ok=%d\n", ok);
      if (ok || dev->can_write()) {
         /* Flush out final partial block of this session */
         if (!jcr->dcr->write_block_to_device()) {
            Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
                  dev->print_name(), dev->bstrerror());
            Dmsg0(100, _("Set ok=FALSE after write_block_to_device.\n"));
            ok = false;
         }
         Dmsg2(200, "Flush block to device pos %u:%u\n", dev->file, dev->block_num);
      }  

      if (!ok) {
         discard_data_spool(jcr->dcr);
      } else {
         /* Note: if commit is OK, the device will remain blocked */
         commit_data_spool(jcr->dcr);
      }

      /*
       * Don't use time_t for job_elapsed as time_t can be 32 or 64 bits,
       *   and the subsequent Jmsg() editing will break
       */
      int32_t job_elapsed = time(NULL) - jcr->run_time;

      if (job_elapsed <= 0) {
         job_elapsed = 1;
      }

      Jmsg(jcr, M_INFO, 0, _("Elapsed time=%02d:%02d:%02d, Transfer rate=%s Bytes/second\n"),
            job_elapsed / 3600, job_elapsed % 3600 / 60, job_elapsed % 60,
            edit_uint64_with_suffix(jcr->JobBytes / job_elapsed, ec1));

      if (ok && dev->is_dvd()) {
         ok = dvd_close_job(jcr->dcr);   /* do DVD cleanup if any */
      }
      /* Release the device -- and send final Vol info to DIR */
      release_device(jcr->dcr);

      if (!ok || job_canceled(jcr)) {
         discard_attribute_spool(jcr);
      } else {
         commit_attribute_spool(jcr);
      }
   }

   if (jcr->read_dcr) {
      if (!release_device(jcr->read_dcr)) {
         ok = false;
      }
   }

   jcr->sendJobStatus();              /* update director */

   Dmsg0(30, "Done reading.\n");
   jcr->end_time = time(NULL);
   dequeue_messages(jcr);             /* send any queued messages */
   if (ok) {
      jcr->setJobStatus(JS_Terminated);
   }
   generate_daemon_event(jcr, "JobEnd");
   generate_plugin_event(jcr, bsdEventJobEnd);
   dir->fsend(Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
      edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);
   Dmsg4(100, Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles, ec1); 
       
   dir->signal(BNET_EOD);             /* send EOD to Director daemon */
   free_plugins(jcr);                 /* release instantiated plugins */

   return ok;
}

/*
 * Called here for each record from read_records()
 *  Returns: true if OK
 *           false if error
 */
static bool record_cb(DCR *dcr, DEV_RECORD *rec)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = jcr->dcr->dev;
   char buf1[100], buf2[100];
   
#ifdef xxx
   Dmsg5(000, "on entry     JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
#endif
   /* If label and not for us, discard it */
   if (rec->FileIndex < 0 && rec->match_stat <= 0) {
      return true;
   }
   /* We want to write SOS_LABEL and EOS_LABEL discard all others */
   switch (rec->FileIndex) {                        
   case PRE_LABEL:
   case VOL_LABEL:
   case EOT_LABEL:
   case EOM_LABEL:
      return true;                    /* don't write vol labels */
   }
//   if (jcr->getJobType() == JT_BACKUP) {
      /*
       * For normal migration jobs, FileIndex values are sequential because
       *  we are dealing with one job.  However, for Vbackup (consolidation),
       *  we will be getting records from multiple jobs and writing them back
       *  out, so we need to ensure that the output FileIndex is sequential.
       *  We do so by detecting a FileIndex change and incrementing the
       *  JobFiles, which we then use as the output FileIndex.
       */
      if (rec->FileIndex >= 0) { 
         /* If something changed, increment FileIndex */
         if (rec->VolSessionId != rec->last_VolSessionId || 
             rec->VolSessionTime != rec->last_VolSessionTime ||
             rec->FileIndex != rec->last_FileIndex) {
            jcr->JobFiles++;
            rec->last_VolSessionId = rec->VolSessionId;
            rec->last_VolSessionTime = rec->VolSessionTime;
            rec->last_FileIndex = rec->FileIndex;
         }
         rec->FileIndex = jcr->JobFiles;     /* set sequential output FileIndex */
      }
//   }
   /*
    * Modify record SessionId and SessionTime to correspond to
    * output.
    */
   rec->VolSessionId = jcr->VolSessionId;
   rec->VolSessionTime = jcr->VolSessionTime;
   Dmsg5(200, "before write JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
   while (!write_record_to_block(jcr->dcr, rec)) {
      Dmsg4(200, "!write_record_to_block blkpos=%u:%u len=%d rem=%d\n", 
            dev->file, dev->block_num, rec->data_len, rec->remainder);
      if (!jcr->dcr->write_block_to_device()) {
         Dmsg2(90, "Got write_block_to_dev error on device %s. %s\n",
            dev->print_name(), dev->bstrerror());
         Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
               dev->print_name(), dev->bstrerror());
         return false;
      }
      Dmsg2(200, "===== Wrote block new pos %u:%u\n", dev->file, dev->block_num);
   }
   /* Restore packet */
   rec->VolSessionId = rec->last_VolSessionId;
   rec->VolSessionTime = rec->last_VolSessionTime;
   if (rec->FileIndex < 0) {
      return true;                    /* don't send LABELs to Dir */
   }
   jcr->JobBytes += rec->data_len;   /* increment bytes this job */
   Dmsg5(500, "wrote_record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

   send_attrs_to_dir(jcr, rec);

   return true;
}
