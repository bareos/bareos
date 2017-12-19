/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * Kern Sibbald, January MMVI
 */
/**
 * @file
 * responsible for doing
 * migration, archive, copy, and virtual backup jobs.
 */

#include "bareos.h"
#include "stored.h"

/* Import functions */

/* Forward referenced subroutines */

/**
 * Responses sent to the Director
 */
static char Job_end[] =
   "3099 Job %s end JobStatus=%d JobFiles=%d JobBytes=%s JobErrors=%u\n";

/**
 * Responses received from Storage Daemon
 */
static char OK_start_replicate[] =
   "3000 OK start replicate ticket = %d\n";
static char OK_data[] =
   "3000 OK data\n";
static char OK_replicate[] =
   "3000 OK replicate data\n";
static char OK_end_replicate[] =
   "3000 OK end replicate\n";

/**
 * Commands sent to Storage Daemon
 */
static char start_replicate[] =
   "start replicate\n";
static char replicate_data[] =
   "replicate data %d\n";
static char end_replicate[] =
   "end replicate\n";


/* get information from first original SOS label for our job */
static bool found_first_sos_label = false;



/**
 * Get response from Storage daemon to a command we sent.
 * Check that the response is OK.
 *
 * Returns: false on failure
 *          true on success
 */
static bool response(JCR *jcr, BSOCK *sd, char *resp, const char *cmd)
{
   if (sd->errors) {
      return false;
                  }
   if (bget_msg(sd) > 0) {
      Dmsg1(110, "<stored: %s", sd->msg);
      if (bstrcmp(sd->msg, resp)) {
         return true;
      }
   }
   if (job_canceled(jcr)) {
      return false;                   /* if canceled avoid useless error messages */
   }
   if (is_bnet_error(sd)) {
      Jmsg2(jcr, M_FATAL, 0, _("Comm error with SD. bad response to %s. ERR=%s\n"),
            cmd, bnet_strerror(sd));
   } else {
      Jmsg3(jcr, M_FATAL, 0, _("Bad response to %s command. Wanted %s, got %s\n"),
            cmd, resp, sd->msg);
   }
   return false;
}

/**
 * Called here for each record from read_records()
 * This function is used when we do a internal clone of a Job e.g.
 * this SD is both the reading and writing SD.
 *
 * Returns: true if OK
 *           false if error
 */
static bool clone_record_internally(DCR *dcr, DEV_RECORD *rec)
{
   bool retval = false;
   bool translated_record = false;
   JCR *jcr = dcr->jcr;
   DEVICE *dev = jcr->dcr->dev;
   char buf1[100], buf2[100];

#ifdef xxx
   Dmsg5(000, "on entry     JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
         jcr->JobId, FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
#endif

   /*
    * If label, discard it as we create our SOS and EOS Labels
    * However, we still want the first Start Of Session label as that contains
    * the timestamps from the first original Job.
    *
    * We need to put this info in our own SOS and EOS Labels so that
    * bscan-ing of virtual and always incremental consolidated jobs
    * works.
    */

   if (rec->FileIndex < 0) {
      if (rec->FileIndex == SOS_LABEL) {
         if (!found_first_sos_label) {
            Dmsg0(200, "Found first SOS_LABEL and adopting job info\n");
            found_first_sos_label = true;

            static SESSION_LABEL the_label;
            static SESSION_LABEL* label= &the_label;

            struct date_time dt;
            struct tm tm;

            unser_session_label(label, rec);

            /*
             * set job info from first SOS label
             */
            jcr->JobId = label->JobId;
            jcr->setJobType(label->JobType);
            jcr->setJobLevel(label->JobLevel);
            Dmsg1(200, "joblevel from SOS_LABEL is now %c\n", label->JobLevel);
            bstrncpy(jcr->Job, label->Job, sizeof(jcr->Job));

            if (label->VerNum >= 11) {
               jcr->sched_time = btime_to_unix(label->write_btime);

            } else {
               dt.julian_day_number = label->write_date;
               dt.julian_day_fraction = label->write_time;
               tm_decode(&dt, &tm);
               jcr->sched_time = mktime(&tm);
            }
            jcr->start_time = jcr->sched_time;

            /* write the SOS Label with the existing timestamp infos */
            if (!write_session_label(jcr->dcr, SOS_LABEL)) {
               Jmsg1(jcr, M_FATAL, 0, _("Write session label failed. ERR=%s\n"),
                     dev->bstrerror());
               jcr->setJobStatus(JS_ErrorTerminated);
               retval = false;
               goto bail_out;
            }
         } else {
            Dmsg0(200, "Found additional SOS_LABEL, ignoring! \n");
            retval = true;
            goto bail_out;
         }

      } else  {
         /* Other label than SOS -> skip */
         retval = true;
         goto bail_out;
      }
   }

   /*
    * For normal migration jobs, FileIndex values are sequential because
    *  we are dealing with one job.  However, for Vbackup (consolidation),
    *  we will be getting records from multiple jobs and writing them back
    *  out, so we need to ensure that the output FileIndex is sequential.
    *  We do so by detecting a FileIndex change and incrementing the
    *  JobFiles, which we then use as the output FileIndex.
    */
   if (rec->FileIndex >= 0) {
      /*
       * If something changed, increment FileIndex
       */
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

   /*
    * Modify record SessionId and SessionTime to correspond to output.
    */
   rec->VolSessionId = jcr->VolSessionId;
   rec->VolSessionTime = jcr->VolSessionTime;

   Dmsg5(200, "before write JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
         jcr->JobId, FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

   /*
    * Perform record translations.
    */
   jcr->dcr->before_rec = rec;
   jcr->dcr->after_rec = NULL;
   if (generate_plugin_event(jcr, bsdEventWriteRecordTranslation, jcr->dcr) != bRC_OK) {
      goto bail_out;
   }

   /*
    * The record got translated when we got an after_rec pointer after calling the
    * bsdEventWriteRecordTranslation plugin event. If no translation has taken place
    * we just point the after_rec pointer to same DEV_RECORD as in the before_rec pointer.
    */
   if (!jcr->dcr->after_rec) {
      jcr->dcr->after_rec = jcr->dcr->before_rec;
   } else {
      translated_record = true;
   }

   while (!write_record_to_block(jcr->dcr, jcr->dcr->after_rec)) {
      Dmsg4(200, "!write_record_to_block blkpos=%u:%u len=%d rem=%d\n",
            dev->file, dev->block_num, jcr->dcr->after_rec->data_len, jcr->dcr->after_rec->remainder);
      if (!jcr->dcr->write_block_to_device()) {
         Dmsg2(90, "Got write_block_to_dev error on device %s. %s\n",
            dev->print_name(), dev->bstrerror());
         Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
               dev->print_name(), dev->bstrerror());
         goto bail_out;
      }
      Dmsg2(200, "===== Wrote block new pos %u:%u\n", dev->file, dev->block_num);
   }

   /*
    * Restore packet
    */
   jcr->dcr->after_rec->VolSessionId = jcr->dcr->after_rec->last_VolSessionId;
   jcr->dcr->after_rec->VolSessionTime = jcr->dcr->after_rec->last_VolSessionTime;

   if (jcr->dcr->after_rec->FileIndex < 0) {
      retval = true;                    /* don't send LABELs to Dir */
      goto bail_out;
   }

   jcr->JobBytes += jcr->dcr->after_rec->data_len;   /* increment bytes of this job */

   Dmsg5(500, "wrote_record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
         jcr->JobId, FI_to_ascii(buf1, jcr->dcr->after_rec->FileIndex), jcr->dcr->after_rec->VolSessionId,
         stream_to_ascii(buf2, jcr->dcr->after_rec->Stream, jcr->dcr->after_rec->FileIndex), jcr->dcr->after_rec->data_len);

   send_attrs_to_dir(jcr, jcr->dcr->after_rec);

   retval = true;

bail_out:
   if (translated_record) {
      free_record(jcr->dcr->after_rec);
      jcr->dcr->after_rec = NULL;
   }

   return retval;
}

/**
 * Called here for each record from read_records()
 * This function is used when we do a external clone of a Job e.g.
 * this SD is the reading SD. And a remote SD is the writing SD.
 *
 * Returns: true if OK
 *           false if error
 */
static bool clone_record_to_remote_sd(DCR *dcr, DEV_RECORD *rec)
{
   POOLMEM *msgsave;
   JCR *jcr = dcr->jcr;
   char buf1[100], buf2[100];
   BSOCK *sd = jcr->store_bsock;
   bool send_eod, send_header;

#ifdef xxx
   Dmsg5(000, "on entry     JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
         jcr->JobId, FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
#endif

   /*
    * If label discard it
    */
   if (rec->FileIndex < 0) {
      return true;
   }

   /*
    * See if this is the first record being processed.
    */
   if (rec->last_FileIndex == 0) {
      /*
       * Initialize the last counters so we can compare
       * things in the next run through here.
       */
      rec->last_VolSessionId = rec->VolSessionId;
      rec->last_VolSessionTime = rec->VolSessionTime;
      rec->last_FileIndex = rec->FileIndex;
      rec->last_Stream = rec->Stream;
      jcr->JobFiles = 1;

      /*
       * Need to send both a new header only.
       */
      send_eod = false;
      send_header = true;
   } else {
      /*
       * See if we are changing file or stream type.
       */
      if (rec->VolSessionId != rec->last_VolSessionId ||
          rec->VolSessionTime != rec->last_VolSessionTime ||
          rec->FileIndex != rec->last_FileIndex ||
          rec->Stream != rec->last_Stream) {
         /*
          * See if we are changing the FileIndex e.g.
          * start processing the next file in the backup stream.
          */
         if (rec->FileIndex != rec->last_FileIndex) {
            jcr->JobFiles++;
         }

         /*
          * Keep track of the new state.
          */
         rec->last_VolSessionId = rec->VolSessionId;
         rec->last_VolSessionTime = rec->VolSessionTime;
         rec->last_FileIndex = rec->FileIndex;
         rec->last_Stream = rec->Stream;

         /*
          * Need to send both a EOD and a new header.
          */
         send_eod = true;
         send_header = true;
      } else {
         send_eod = false;
         send_header = false;
      }
   }

   /*
    * Send a EOD when needed.
    */
   if (send_eod) {
      if (!sd->signal(BNET_EOD)) { /* indicate end of file data */
         if (!jcr->is_job_canceled()) {
            Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                  sd->bstrerror());
         }
         return false;
      }
   }

   /*
    * Send a header when needed.
    */
   if (send_header) {
      if (!sd->fsend("%ld %d 0", rec->FileIndex, rec->Stream)) {
         if (!jcr->is_job_canceled()) {
            Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                  sd->bstrerror());
         }
         return false;
      }
   }

   /*
    * Send the record data.
    * We don't want to copy the data from the record to the socket structure
    * so we save the original msg pointer and use the record data pointer for
    * sending and restore the original msg pointer when done.
    */
   msgsave = sd->msg;
   sd->msg = rec->data;
   sd->msglen = rec->data_len;

   if (!sd->send()) {
      sd->msg = msgsave;
      sd->msglen = 0;
      if (!jcr->is_job_canceled()) {
         Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
               sd->bstrerror());
      }
      return false;
   }

   jcr->JobBytes += sd->msglen;
   sd->msg = msgsave;

   Dmsg5(200, "wrote_record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
         jcr->JobId, FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

   return true;
}

/**
 * Check autoinflation/autodeflation settings.
 */
static inline void check_auto_xflation(JCR *jcr)
{
   /*
    * See if the autoxflateonreplication flag is set to true then we allow
    * autodeflation and autoinflation to take place.
    */
   if (me->autoxflateonreplication) {
      return;
   }

   /*
    * Check autodeflation.
    */
   switch (jcr->read_dcr->autodeflate) {
   case IO_DIRECTION_IN:
   case IO_DIRECTION_INOUT:
      Dmsg0(200, "Clearing autodeflate on read_dcr\n");
      jcr->read_dcr->autodeflate = IO_DIRECTION_NONE;
      break;
   default:
      break;
   }

   if (jcr->dcr) {
      switch (jcr->dcr->autodeflate) {
         case IO_DIRECTION_OUT:
         case IO_DIRECTION_INOUT:
            Dmsg0(200, "Clearing autodeflate on write dcr\n");
            jcr->dcr->autodeflate = IO_DIRECTION_NONE;
            break;
         default:
            break;
      }
   }

   /*
    * Check autoinflation.
    */
   switch (jcr->read_dcr->autoinflate) {
   case IO_DIRECTION_IN:
   case IO_DIRECTION_INOUT:
      Dmsg0(200, "Clearing autoinflate on read_dcr\n");
      jcr->read_dcr->autoinflate = IO_DIRECTION_NONE;
      break;
   default:
      break;
   }

   if (jcr->dcr) {
      switch (jcr->dcr->autoinflate) {
         case IO_DIRECTION_OUT:
         case IO_DIRECTION_INOUT:
            Dmsg0(200, "Clearing autoinflate on write dcr\n");
            jcr->dcr->autoinflate = IO_DIRECTION_NONE;
            break;
         default:
            break;
      }
   }
}

/**
 * Read Data and commit to new job.
 */
bool do_mac_run(JCR *jcr)
{
   utime_t now;
   char ec1[50];
   const char *Type;
   bool ok = true;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev = jcr->dcr->dev;

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

   if (jcr->NumReadVolumes == 0) {
      Jmsg(jcr, M_FATAL, 0, _("No Volume names found for %s.\n"), Type);
      goto bail_out;
   }

   /*
    * Check autoinflation/autodeflation settings.
    */
   check_auto_xflation(jcr);

   /*
    * See if we perform both read and write or read only.
    */
   if (jcr->remote_replicate) {
      BSOCK *sd;

      if (!jcr->read_dcr) {
         Jmsg(jcr, M_FATAL, 0, _("Read device not properly initialized.\n"));
         goto bail_out;
      }

      Dmsg1(100, "read_dcr=%p\n", jcr->read_dcr);
      Dmsg3(200, "Found %d volumes names for %s. First=%s\n",
            jcr->NumReadVolumes, Type, jcr->VolList->VolumeName);

      /*
       * Ready devices for reading.
       */
      if (!acquire_device_for_read(jcr->read_dcr)) {
         ok = false;
         goto bail_out;
      }

      Dmsg2(200, "===== After acquire pos %u:%u\n", jcr->read_dcr->dev->file, jcr->read_dcr->dev->block_num);

      jcr->sendJobStatus(JS_Running);

      /*
       * Set network buffering.
       */
      sd = jcr->store_bsock;
      if (!sd->set_buffer_size(me->max_network_buffer_size, BNET_SETBUF_WRITE)) {
         Jmsg(jcr, M_FATAL, 0, _("Cannot set buffer size SD->SD.\n"));
         ok = false;
         goto bail_out;
      }

      /*
       * Let the remote SD know we are about to start the replication.
       */
      sd->fsend(start_replicate);
      Dmsg1(110, ">stored: %s", sd->msg);

      /*
       * Expect to receive back the Ticket number.
       */
      if (bget_msg(sd) >= 0) {
         Dmsg1(110, "<stored: %s", sd->msg);
         if (sscanf(sd->msg, OK_start_replicate, &jcr->Ticket) != 1) {
            Jmsg(jcr, M_FATAL, 0, _("Bad response to start replicate: %s\n"), sd->msg);
            goto bail_out;
         }
         Dmsg1(110, "Got Ticket=%d\n", jcr->Ticket);
      } else {
         Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to start replicate command\n"));
         goto bail_out;
      }

      /*
       * Let the remote SD know we are now really going to send the data.
       */
      sd->fsend(replicate_data, jcr->Ticket);
      Dmsg1(110, ">stored: %s", sd->msg);

      /*
       * Expect to get response to the replicate data cmd from Storage daemon
       */
      if (!response(jcr, sd, OK_data, "replicate data")) {
         ok = false;
         goto bail_out;
      }

      /*
       * Update the initial Job Statistics.
       */
      now = (utime_t)time(NULL);
      update_job_statistics(jcr, now);

      /*
       * Read all data and send it to remote SD.
       */
      ok = read_records(jcr->read_dcr, clone_record_to_remote_sd, mount_next_read_volume);

      /*
       * Send the last EOD to close the last data transfer and a next EOD to
       * signal the remote we are done.
       */
      if (!sd->signal(BNET_EOD) || !sd->signal(BNET_EOD)) {
         if (!jcr->is_job_canceled()) {
            Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
                  sd->bstrerror());
         }
         goto bail_out;
      }

      /*
       * Expect to get response that the replicate data succeeded.
       */
      if (!response(jcr, sd, OK_replicate, "replicate data")) {
         ok = false;
         goto bail_out;
      }

      /*
       * End replicate session.
       */
      sd->fsend(end_replicate);
      Dmsg1(110, ">stored: %s", sd->msg);

      /*
       * Expect to get response to the end replicate cmd from Storage daemon
       */
      if (!response(jcr, sd, OK_end_replicate, "end replicate")) {
         ok = false;
         goto bail_out;
      }

      /* Inform Storage daemon that we are done */
      sd->signal(BNET_TERMINATE);
   } else {

      if (!jcr->read_dcr || !jcr->dcr) {
         Jmsg(jcr, M_FATAL, 0, _("Read and write devices not properly initialized.\n"));
         goto bail_out;
      }

      Dmsg2(100, "read_dcr=%p write_dcr=%p\n", jcr->read_dcr, jcr->dcr);
      Dmsg3(200, "Found %d volumes names for %s. First=%s\n",
            jcr->NumReadVolumes, Type, jcr->VolList->VolumeName);

      /*
       * Ready devices for reading and writing.
       */
      if (!acquire_device_for_read(jcr->read_dcr) ||
          !acquire_device_for_append(jcr->dcr)) {
         ok = false;
         goto bail_out;
      }

      Dmsg2(200, "===== After acquire pos %u:%u\n", jcr->dcr->dev->file, jcr->dcr->dev->block_num);

      jcr->sendJobStatus(JS_Running);

      /*
       * Update the initial Job Statistics.
       */
      now = (utime_t)time(NULL);
      update_job_statistics(jcr, now);

      if (!begin_data_spool(jcr->dcr) ) {
         ok = false;
         goto bail_out;
      }

      if (!begin_attribute_spool(jcr)) {
         ok = false;
         goto bail_out;
      }

      jcr->dcr->VolFirstIndex = jcr->dcr->VolLastIndex = 0;
      jcr->run_time = time(NULL);
      set_start_vol_position(jcr->dcr);
      jcr->JobFiles = 0;

      /*
       * Read all data and make a local clone of it.
       */
      ok = read_records(jcr->read_dcr, clone_record_internally, mount_next_read_volume);
   }

bail_out:
   if (!ok) {
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   if (!jcr->remote_replicate && jcr->dcr) {
      /*
       * Don't use time_t for job_elapsed as time_t can be 32 or 64 bits,
       *   and the subsequent Jmsg() editing will break
       */
      int32_t job_elapsed;

      Dmsg1(100, "ok=%d\n", ok);

      if (ok || dev->can_write()) {

         /*
            memorize current JobStatus and set to
            JS_Terminated to write into EOS_LABEL
          */
         char currentJobStatus = jcr->JobStatus;
         jcr->setJobStatus(JS_Terminated);

         /*
          * Write End Of Session Label
          */
         DCR *dcr = jcr->dcr;
         if (!write_session_label(dcr, EOS_LABEL)) {
            /*
             * Print only if ok and not cancelled to avoid spurious messages
             */

            if (ok && !jcr->is_job_canceled()) {
               Jmsg1(jcr, M_FATAL, 0, _("Error writing end session label. ERR=%s\n"),
                     dev->bstrerror());
            }
            jcr->setJobStatus(JS_ErrorTerminated);
            ok = false;
         } else {
            /* restore JobStatus */
            jcr->setJobStatus(currentJobStatus);
         }
         /*
          * Flush out final partial block of this session
          */
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
         /*
          * Note: if commit is OK, the device will remain blocked
          */
         commit_data_spool(jcr->dcr);
      }

      job_elapsed = time(NULL) - jcr->run_time;
      if (job_elapsed <= 0) {
         job_elapsed = 1;
      }

      Jmsg(jcr, M_INFO, 0, _("Elapsed time=%02d:%02d:%02d, Transfer rate=%s Bytes/second\n"),
           job_elapsed / 3600, job_elapsed % 3600 / 60, job_elapsed % 60,
           edit_uint64_with_suffix(jcr->JobBytes / job_elapsed, ec1));

      /*
       * Release the device -- and send final Vol info to DIR
       */
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

   generate_plugin_event(jcr, bsdEventJobEnd);
   dir->fsend(Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
              edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);
   Dmsg4(100, Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles, ec1);

   dir->signal(BNET_EOD);             /* send EOD to Director daemon */
   free_plugins(jcr);                 /* release instantiated plugins */

   return false;                      /* Continue DIR session ? */
}
