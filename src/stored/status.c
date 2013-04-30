/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.

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
 *  This file handles the status command
 *
 *     Kern Sibbald, May MMIII
 *
 *
 */

#include "bacula.h"
#include "stored.h"
#include "lib/status.h"

/* Imported functions */

/* Imported variables */
extern BSOCK *filed_chan;
extern void *start_heap;

/* Static variables */
static char qstatus[] = ".status %127s\n";

static char OKqstatus[]   = "3000 OK .status\n";
static char DotStatusJob[] = "JobId=%d JobStatus=%c JobErrors=%d\n";


/* Forward referenced functions */
static void sendit(const char *msg, int len, STATUS_PKT *sp);
static void sendit(POOL_MEM &msg, int len, STATUS_PKT *sp);
static void sendit(const char *msg, int len, void *arg);

static void send_blocked_status(DEVICE *dev, STATUS_PKT *sp);
static void send_device_status(DEVICE *dev, STATUS_PKT *sp);
static void list_terminated_jobs(STATUS_PKT *sp);
static void list_running_jobs(STATUS_PKT *sp);
static void list_jobs_waiting_on_reservation(STATUS_PKT *sp);
static void list_status_header(STATUS_PKT *sp);
static void list_devices(STATUS_PKT *sp);

static const char *level_to_str(int level);

/*
 * Status command from Director
 */
void output_status(STATUS_PKT *sp)
{
   POOL_MEM msg(PM_MESSAGE);
   int len;

   list_status_header(sp);

   /*
    * List running jobs
    */
   list_running_jobs(sp);

   /*
    * List jobs stuck in reservation system
    */
   list_jobs_waiting_on_reservation(sp);

   /*
    * List terminated jobs
    */
   list_terminated_jobs(sp);

   /*
    * List devices
    */
   list_devices(sp);


   len = Mmsg(msg, _("Used Volume status:\n"));
   if (!sp->api) sendit(msg, len, sp);

   list_volumes(sendit, (void *)sp);
   if (!sp->api) sendit("====\n\n", 6, sp);


   list_spool_stats(sendit, (void *)sp);
   if (!sp->api) sendit("====\n\n", 6, sp);

}

static void list_resources(STATUS_PKT *sp)
{
#ifdef when_working
   POOL_MEM msg(PM_MESSAGE);
   int len;

   len = Mmsg(msg, _("\nSD Resources:\n"));
   if (!sp->api) sendit(msg, len, sp);
   dump_resource(R_DEVICE, resources[R_DEVICE-r_first], sp);
   if (!sp->api) sendit("====\n\n", 6, sp);
#endif
}

#ifdef xxxx
static find_device(char *devname)
{
   foreach_res(device, R_DEVICE) {
      if (strcasecmp(device->hdr.name, devname) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      foreach_res(changer, R_AUTOCHANGER) {
         if (strcasecmp(changer->hdr.name, devname) == 0) {
            break;
         }
      }
   }
}
#endif

static void list_devices(STATUS_PKT *sp)
{
   DEVRES *device;
   AUTOCHANGER *changer;
   DEVICE *dev;
   char b1[35], b2[35], b3[35];
   POOL_MEM msg(PM_MESSAGE);
   int len;
   int bpb;

   len = Mmsg(msg, _("\nDevice status:\n"));
   if (!sp->api) sendit(msg, len, sp);

   foreach_res(changer, R_AUTOCHANGER) {
      len = Mmsg(msg, _("Autochanger \"%s\" with devices:\n"),
         changer->hdr.name);
      sendit(msg, len, sp);

      foreach_alist(device, changer->device) {
         if (device->dev) {
            len = Mmsg(msg, "   %s\n", device->dev->print_name());
            sendit(msg, len, sp);
         } else {
            len = Mmsg(msg, "   %s\n", device->hdr.name);
            sendit(msg, len, sp);
         }
      }
   }


   foreach_res(device, R_DEVICE) {
      dev = device->dev;
      if (dev && dev->is_open()) {
         if (dev->is_labeled()) {
            len = Mmsg(msg, _("\nDevice %s is %s:\n"
                              "    Volume:      %s\n"
                              "    Pool:        %s\n"
                              "    Media type:  %s\n"),
               dev->print_name(), 
               dev->blocked()?_("waiting for"):_("mounted with"),
               dev->VolHdr.VolumeName, 
               dev->pool_name[0]?dev->pool_name:_("*unknown*"),
               dev->device->media_type);
            sendit(msg, len, sp);
         } else {
            len = Mmsg(msg, _("\nDevice %s open but no Bacula volume is currently mounted.\n"), 
               dev->print_name());
            sendit(msg, len, sp);
         }
         send_blocked_status(dev, sp);
         if (dev->can_append()) {
            bpb = dev->VolCatInfo.VolCatBlocks;
            if (bpb <= 0) {
               bpb = 1;
            }
            bpb = dev->VolCatInfo.VolCatBytes / bpb;
            len = Mmsg(msg, _("    Total Bytes=%s Blocks=%s Bytes/block=%s\n"),
               edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, b1),
               edit_uint64_with_commas(dev->VolCatInfo.VolCatBlocks, b2),
               edit_uint64_with_commas(bpb, b3));
            sendit(msg, len, sp);
         } else {  /* reading */
            bpb = dev->VolCatInfo.VolCatReads;
            if (bpb <= 0) {
               bpb = 1;
            }
            if (dev->VolCatInfo.VolCatRBytes > 0) {
               bpb = dev->VolCatInfo.VolCatRBytes / bpb;
            } else {
               bpb = 0;
            }
            len = Mmsg(msg, _("    Total Bytes Read=%s Blocks Read=%s Bytes/block=%s\n"),
               edit_uint64_with_commas(dev->VolCatInfo.VolCatRBytes, b1),
               edit_uint64_with_commas(dev->VolCatInfo.VolCatReads, b2),
               edit_uint64_with_commas(bpb, b3));
            sendit(msg, len, sp);
         }
         len = Mmsg(msg, _("    Positioned at File=%s Block=%s\n"),
            edit_uint64_with_commas(dev->file, b1),
            edit_uint64_with_commas(dev->block_num, b2));
         sendit(msg, len, sp);

      } else {
         if (dev) {
            len = Mmsg(msg, _("\nDevice %s is not open.\n"), dev->print_name());
            sendit(msg, len, sp);
            send_blocked_status(dev, sp);
        } else {
            len = Mmsg(msg, _("\nDevice \"%s\" is not open or does not exist.\n"), device->hdr.name);
            sendit(msg, len, sp);
         }
      }

      if (!sp->api) sendit("==\n", 4, sp);
   }
   if (!sp->api) sendit("====\n\n", 6, sp);
}

static void list_status_header(STATUS_PKT *sp)
{
   char dt[MAX_TIME_LENGTH];
   char b1[35], b2[35], b3[35], b4[35], b5[35];
   POOL_MEM msg(PM_MESSAGE);
   int len;

   len = Mmsg(msg, _("%s Version: %s (%s) %s %s %s\n"), 
              my_name, VERSION, BDATE, HOST_OS, DISTNAME, DISTVER);
   sendit(msg, len, sp);

   bstrftime_nc(dt, sizeof(dt), daemon_start_time);


   len = Mmsg(msg, _("Daemon started %s. Jobs: run=%d, running=%d.\n"),
        dt, num_jobs_run, job_count());
   sendit(msg, len, sp);
   len = Mmsg(msg, _(" Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n"),
         edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
         edit_uint64_with_commas(sm_bytes, b2),
         edit_uint64_with_commas(sm_max_bytes, b3),
         edit_uint64_with_commas(sm_buffers, b4),
         edit_uint64_with_commas(sm_max_buffers, b5));
   sendit(msg, len, sp);
   len = Mmsg(msg, " Sizes: boffset_t=%d size_t=%d int32_t=%d int64_t=%d "
              "mode=%d,%d\n", 
              (int)sizeof(boffset_t), (int)sizeof(size_t), (int)sizeof(int32_t),
              (int)sizeof(int64_t), (int)DEVELOPER_MODE, (int)BEEF);
   sendit(msg, len, sp);
   if (bplugin_list->size() > 0) {
      Plugin *plugin;
      int len;
      pm_strcpy(msg, " Plugin: ");
      foreach_alist(plugin, bplugin_list) {
         len = pm_strcat(msg, plugin->file);
         if (len > 80) {
            pm_strcat(msg, "\n   ");
         } else {
            pm_strcat(msg, " ");
         }
      }
      len = pm_strcat(msg, "\n");
      sendit(msg.c_str(), len, sp);
   }
}

static void send_blocked_status(DEVICE *dev, STATUS_PKT *sp)
{
   POOL_MEM msg(PM_MESSAGE);
   int len;

   if (!dev) {
      len = Mmsg(msg, _("No DEVICE structure.\n\n"));
      sendit(msg, len, sp);
      return;
   }
   switch (dev->blocked()) {
   case BST_UNMOUNTED:
      len = Mmsg(msg, _("    Device is BLOCKED. User unmounted.\n"));
      sendit(msg, len, sp);
      break;
   case BST_UNMOUNTED_WAITING_FOR_SYSOP:
      len = Mmsg(msg, _("    Device is BLOCKED. User unmounted during wait for media/mount.\n"));
      sendit(msg, len, sp);
      break;
   case BST_WAITING_FOR_SYSOP:
      {
         DCR *dcr;
         bool found_jcr = false;
         dev->Lock();
         foreach_dlist(dcr, dev->attached_dcrs) {
            if (dcr->jcr->JobStatus == JS_WaitMount) {
               len = Mmsg(msg, _("    Device is BLOCKED waiting for mount of volume \"%s\",\n"
                                 "       Pool:        %s\n"
                                 "       Media type:  %s\n"),
                          dcr->VolumeName,
                          dcr->pool_name,
                          dcr->media_type);
               sendit(msg, len, sp);
               found_jcr = true;
            } else if (dcr->jcr->JobStatus == JS_WaitMedia) {
               len = Mmsg(msg, _("    Device is BLOCKED waiting to create a volume for:\n"
                                 "       Pool:        %s\n"
                                 "       Media type:  %s\n"),
                          dcr->pool_name,
                          dcr->media_type);
               sendit(msg, len, sp);
               found_jcr = true;
            }
         }
         dev->Unlock();
         if (!found_jcr) {
            len = Mmsg(msg, _("    Device is BLOCKED waiting for media.\n"));
            sendit(msg, len, sp);
         }
      }
      break;
   case BST_DOING_ACQUIRE:
      len = Mmsg(msg, _("    Device is being initialized.\n"));
      sendit(msg, len, sp);
      break;
   case BST_WRITING_LABEL:
      len = Mmsg(msg, _("    Device is blocked labeling a Volume.\n"));
      sendit(msg, len, sp);
      break;
   default:
      break;
   }
   /* Send autochanger slot status */
   if (dev->is_autochanger()) {
      if (dev->get_slot() > 0) {
         len = Mmsg(msg, _("    Slot %d %s loaded in drive %d.\n"), 
            dev->get_slot(), dev->is_open()?"is": "was last", dev->drive_index);
         sendit(msg, len, sp);
      } else if (dev->get_slot() <= 0) {
         len = Mmsg(msg, _("    Drive %d is not loaded.\n"), dev->drive_index);
         sendit(msg, len, sp);
      }
   }
   if (debug_level > 1) {
      send_device_status(dev, sp);
   }
}

static void send_device_status(DEVICE *dev, STATUS_PKT *sp)
{
   POOL_MEM msg(PM_MESSAGE);
   int len;

   if (debug_level > 5) {
      len = Mmsg(msg, _("Configured device capabilities:\n"));
      sendit(msg, len, sp);
      len = Mmsg(msg, "  %sEOF %sBSR %sBSF %sFSR %sFSF %sEOM %sREM %sRACCESS %sAUTOMOUNT %sLABEL %sANONVOLS %sALWAYSOPEN\n",
         dev->capabilities & CAP_EOF ? "" : "!", 
         dev->capabilities & CAP_BSR ? "" : "!", 
         dev->capabilities & CAP_BSF ? "" : "!", 
         dev->capabilities & CAP_FSR ? "" : "!", 
         dev->capabilities & CAP_FSF ? "" : "!", 
         dev->capabilities & CAP_EOM ? "" : "!", 
         dev->capabilities & CAP_REM ? "" : "!", 
         dev->capabilities & CAP_RACCESS ? "" : "!",
         dev->capabilities & CAP_AUTOMOUNT ? "" : "!", 
         dev->capabilities & CAP_LABEL ? "" : "!", 
         dev->capabilities & CAP_ANONVOLS ? "" : "!", 
         dev->capabilities & CAP_ALWAYSOPEN ? "" : "!");
      sendit(msg, len, sp);
   }

   len = Mmsg(msg, _("Device state:\n"));
   sendit(msg, len, sp);
   len = Mmsg(msg, "  %sOPENED %sTAPE %sLABEL %sMALLOC %sAPPEND %sREAD %sEOT %sWEOT %sEOF %sNEXTVOL %sSHORT %sMOUNTED\n", 
      dev->is_open() ? "" : "!", 
      dev->is_tape() ? "" : "!", 
      dev->is_labeled() ? "" : "!", 
      dev->state & ST_MALLOC ? "" : "!", 
      dev->can_append() ? "" : "!", 
      dev->can_read() ? "" : "!", 
      dev->at_eot() ? "" : "!", 
      dev->state & ST_WEOT ? "" : "!", 
      dev->at_eof() ? "" : "!", 
      dev->state & ST_NEXTVOL ? "" : "!", 
      dev->state & ST_SHORT ? "" : "!", 
      dev->state & ST_MOUNTED ? "" : "!");
   sendit(msg, len, sp);
   len = Mmsg(msg, _("  num_writers=%d reserves=%d block=%d\n"), dev->num_writers,
              dev->num_reserved(), dev->blocked());
   sendit(msg, len, sp);

   len = Mmsg(msg, _("Attached Jobs: "));
   sendit(msg, len, sp);
   DCR *dcr = NULL; 
   bool found = false;
   dev->Lock();
   foreach_dlist(dcr, dev->attached_dcrs) {
      if (dcr->jcr) {
         if (found) {
            sendit(",", 1, sp);
         }
         len = Mmsg(msg, "%d", (int)dcr->jcr->JobId);
         sendit(msg, len, sp);
         found = true;
      }
   }
   dev->Unlock();
   sendit("\n", 1, sp);

   len = Mmsg(msg, _("Device parameters:\n"));
   sendit(msg, len, sp);
   len = Mmsg(msg, _("  Archive name: %s Device name: %s\n"), dev->archive_name(),
      dev->name());
   sendit(msg, len, sp);
   len = Mmsg(msg, _("  File=%u block=%u\n"), dev->file, dev->block_num);
   sendit(msg, len, sp);
   len = Mmsg(msg, _("  Min block=%u Max block=%u\n"), dev->min_block_size, dev->max_block_size);
   sendit(msg, len, sp);
}

static void list_running_jobs(STATUS_PKT *sp)
{
   bool found = false;
   int avebps, bps, sec;
   JCR *jcr;
   DCR *dcr, *rdcr;
   char JobName[MAX_NAME_LENGTH];
   char b1[50], b2[50], b3[50], b4[50];
   int len;
   POOL_MEM msg(PM_MESSAGE);
   time_t now = time(NULL);

   len = Mmsg(msg, _("\nRunning Jobs:\n"));
   if (!sp->api) sendit(msg, len, sp);

   foreach_jcr(jcr) {
      if (jcr->JobStatus == JS_WaitFD) {
         len = Mmsg(msg, _("%s Job %s waiting for Client connection.\n"),
            job_type_to_str(jcr->getJobType()), jcr->Job);
         sendit(msg, len, sp);
      }
      dcr = jcr->dcr;
      rdcr = jcr->read_dcr;
      if ((dcr && dcr->device) || (rdcr && rdcr->device)) {
         bstrncpy(JobName, jcr->Job, sizeof(JobName));
         /* There are three periods after the Job name */
         char *p;
         for (int i=0; i<3; i++) {
            if ((p=strrchr(JobName, '.')) != NULL) {
               *p = 0;
            }
         }
         if (rdcr && rdcr->device) {
            len = Mmsg(msg, _("Reading: %s %s job %s JobId=%d Volume=\"%s\"\n"
                            "    pool=\"%s\" device=%s\n"),
                   job_level_to_str(jcr->getJobLevel()),
                   job_type_to_str(jcr->getJobType()),
                   JobName,
                   jcr->JobId,
                   rdcr->VolumeName,
                   rdcr->pool_name,
                   rdcr->dev?rdcr->dev->print_name(): 
                            rdcr->device->device_name);
            sendit(msg, len, sp);
         }
         if (dcr && dcr->device) {
            len = Mmsg(msg, _("Writing: %s %s job %s JobId=%d Volume=\"%s\"\n"
                            "    pool=\"%s\" device=%s\n"),
                   job_level_to_str(jcr->getJobLevel()),
                   job_type_to_str(jcr->getJobType()),
                   JobName,
                   jcr->JobId,
                   dcr->VolumeName,
                   dcr->pool_name,
                   dcr->dev?dcr->dev->print_name(): 
                            dcr->device->device_name);
            sendit(msg, len, sp);
            len= Mmsg(msg, _("    spooling=%d despooling=%d despool_wait=%d\n"),
                   dcr->spooling, dcr->despooling, dcr->despool_wait);
            sendit(msg, len, sp);
         }
         if (jcr->last_time == 0) {
            jcr->last_time = jcr->run_time;
         }
         sec = now - jcr->last_time;
         if (sec <= 0) {
            sec = 1;
         }
         bps = (jcr->JobBytes - jcr->LastJobBytes) / sec;
         if (jcr->LastRate == 0) {
            jcr->LastRate = bps;
         }
         avebps = (jcr->LastRate + bps) / 2;
         len = Mmsg(msg, _("    Files=%s Bytes=%s AveBytes/sec=%s LastBytes/sec=%s\n"),
            edit_uint64_with_commas(jcr->JobFiles, b1),
            edit_uint64_with_commas(jcr->JobBytes, b2),
            edit_uint64_with_commas(avebps, b3),
            edit_uint64_with_commas(bps, b4));
         sendit(msg, len, sp);
         jcr->LastRate = avebps;
         jcr->LastJobBytes = jcr->JobBytes;
         jcr->last_time = now;
         found = true;
#ifdef DEBUG
         if (jcr->file_bsock) {
            len = Mmsg(msg, _("    FDReadSeqNo=%s in_msg=%u out_msg=%d fd=%d\n"),
               edit_uint64_with_commas(jcr->file_bsock->read_seqno, b1),
               jcr->file_bsock->in_msg_no, jcr->file_bsock->out_msg_no,
               jcr->file_bsock->m_fd);
            sendit(msg, len, sp);
         } else {
            len = Mmsg(msg, _("    FDSocket closed\n"));
            sendit(msg, len, sp);
         }
#endif
      }
   }
   endeach_jcr(jcr);

   if (!found) {
      len = Mmsg(msg, _("No Jobs running.\n"));
      if (!sp->api) sendit(msg, len, sp);
   }
   if (!sp->api) sendit("====\n", 5, sp);
}

static void list_jobs_waiting_on_reservation(STATUS_PKT *sp)
{ 
   JCR *jcr;
   POOL_MEM msg(PM_MESSAGE);
   int len;

   len = Mmsg(msg, _("\nJobs waiting to reserve a drive:\n"));
   if (!sp->api) sendit(msg, len, sp);

   foreach_jcr(jcr) {
      if (!jcr->reserve_msgs) {
         continue;
      }
      send_drive_reserve_messages(jcr, sendit, sp);
   }
   endeach_jcr(jcr);

   if (!sp->api) sendit("====\n", 5, sp);
}


static void list_terminated_jobs(STATUS_PKT *sp)
{
   char dt[MAX_TIME_LENGTH], b1[30], b2[30];
   char level[10];
   struct s_last_job *je;
   const char *msg;

   msg =  _("\nTerminated Jobs:\n");
   if (!sp->api) sendit(msg, strlen(msg), sp);
   if (last_jobs->size() == 0) {
      if (!sp->api) sendit("====\n", 5, sp);
      return;
   }
   lock_last_jobs_list();
   msg =  _(" JobId  Level    Files      Bytes   Status   Finished        Name \n");
   if (!sp->api) sendit(msg, strlen(msg), sp);
   msg =  _("===================================================================\n");
   if (!sp->api) sendit(msg, strlen(msg), sp);
   foreach_dlist(je, last_jobs) {
      char JobName[MAX_NAME_LENGTH];
      const char *termstat;
      char buf[1000];

      bstrftime_nc(dt, sizeof(dt), je->end_time);
      switch (je->JobType) {
      case JT_ADMIN:
      case JT_RESTORE:
         bstrncpy(level, "    ", sizeof(level));
         break;
      default:
         bstrncpy(level, level_to_str(je->JobLevel), sizeof(level));
         level[4] = 0;
         break;
      }
      switch (je->JobStatus) {
      case JS_Created:
         termstat = _("Created");
         break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         termstat = _("Error");
         break;
      case JS_Differences:
         termstat = _("Diffs");
         break;
      case JS_Canceled:
         termstat = _("Cancel");
         break;
      case JS_Terminated:
         termstat = _("OK");
         break;
      case JS_Warnings:
         termstat = _("OK -- with warnings");
         break;
      default:
         termstat = _("Other");
         break;
      }
      bstrncpy(JobName, je->Job, sizeof(JobName));
      /* There are three periods after the Job name */
      char *p;
      for (int i=0; i<3; i++) {
         if ((p=strrchr(JobName, '.')) != NULL) {
            *p = 0;
         }
      }
      if (sp->api) {
         bsnprintf(buf, sizeof(buf), _("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"),
            je->JobId,
            level,
            edit_uint64_with_commas(je->JobFiles, b1),
            edit_uint64_with_suffix(je->JobBytes, b2),
            termstat,
            dt, JobName);
      } else {
         bsnprintf(buf, sizeof(buf), _("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"),
            je->JobId,
            level,
            edit_uint64_with_commas(je->JobFiles, b1),
            edit_uint64_with_suffix(je->JobBytes, b2),
            termstat,
            dt, JobName);
      }  
      sendit(buf, strlen(buf), sp);
   }
   unlock_last_jobs_list();
   if (!sp->api) sendit("====\n", 5, sp);
}

/*
 * Convert Job Level into a string
 */
static const char *level_to_str(int level)
{
   const char *str;

   switch (level) {
   case L_BASE:
      str = _("Base");
   case L_FULL:
      str = _("Full");
      break;
   case L_INCREMENTAL:
      str = _("Incremental");
      break;
   case L_DIFFERENTIAL:
      str = _("Differential");
      break;
   case L_SINCE:
      str = _("Since");
      break;
   case L_VERIFY_CATALOG:
      str = _("Verify Catalog");
      break;
   case L_VERIFY_INIT:
      str = _("Init Catalog");
      break;
   case L_VERIFY_VOLUME_TO_CATALOG:
      str = _("Volume to Catalog");
      break;
   case L_VERIFY_DISK_TO_CATALOG:
      str = _("Disk to Catalog");
      break;
   case L_VERIFY_DATA:
      str = _("Data");
      break;
   case L_NONE:
      str = " ";
      break;
   default:
      str = _("Unknown Job Level");
      break;
   }
   return str;
}

/*
 * Send to Director
 */
static void sendit(const char *msg, int len, STATUS_PKT *sp)
{
   BSOCK *bs = sp->bs;
   if (bs) {
      memcpy(bs->msg, msg, len+1);
      bs->msglen = len+1;
      bs->send();
   } else {
      sp->callback(msg, len, sp->context);
   }
}

static void sendit(const char *msg, int len, void *sp)
{
   sendit(msg, len, (STATUS_PKT *)sp);
}

static void sendit(POOL_MEM &msg, int len, STATUS_PKT *sp)
{
   BSOCK *bs = sp->bs;
   if (bs) {
      memcpy(bs->msg, msg.c_str(), len+1);
      bs->msglen = len+1;
      bs->send();
   } else {
      sp->callback(msg.c_str(), len, sp->context);
   }
}


/*
 * Status command from Director
 */
bool status_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   STATUS_PKT sp;

   dir->fsend("\n");
   sp.bs = dir;
   output_status(&sp);
   dir->signal(BNET_EOD);
   return true;
}

/*
 * .status command from Director
 */
bool qstatus_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM cmd;
   JCR *njcr;
   s_last_job* job;
   STATUS_PKT sp;

   sp.bs = dir;
   if (sscanf(dir->msg, qstatus, cmd.c_str()) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3900 No arg in .status command: %s\n"), jcr->errmsg);
      dir->signal(BNET_EOD);
      return false;
   }
   unbash_spaces(cmd);

   Dmsg1(200, "cmd=%s\n", cmd.c_str());

   if (strcasecmp(cmd.c_str(), "current") == 0) {
      dir->fsend(OKqstatus, cmd.c_str());
      foreach_jcr(njcr) {
         if (njcr->JobId != 0) {
            dir->fsend(DotStatusJob, njcr->JobId, njcr->JobStatus, njcr->JobErrors);
         }
      }
      endeach_jcr(njcr);
   } else if (strcasecmp(cmd.c_str(), "last") == 0) {
      dir->fsend(OKqstatus, cmd.c_str());
      if ((last_jobs) && (last_jobs->size() > 0)) {
         job = (s_last_job*)last_jobs->last();
         dir->fsend(DotStatusJob, job->JobId, job->JobStatus, job->Errors);
      }
   } else if (strcasecmp(cmd.c_str(), "header") == 0) {
       sp.api = true;
       list_status_header(&sp);
   } else if (strcasecmp(cmd.c_str(), "running") == 0) {
       sp.api = true;
       list_running_jobs(&sp);
   } else if (strcasecmp(cmd.c_str(), "waitreservation") == 0) {
       sp.api = true;
       list_jobs_waiting_on_reservation(&sp);
   } else if (strcasecmp(cmd.c_str(), "devices") == 0) {
       sp.api = true;
       list_devices(&sp);
   } else if (strcasecmp(cmd.c_str(), "volumes") == 0) {
       sp.api = true;
       list_volumes(sendit, &sp);
   } else if (strcasecmp(cmd.c_str(), "spooling") == 0) {
       sp.api = true;
       list_spool_stats(sendit, &sp);
   } else if (strcasecmp(cmd.c_str(), "terminated") == 0) {
       sp.api = true;
       list_terminated_jobs(&sp);
   } else if (strcasecmp(cmd.c_str(), "resources") == 0) {
       sp.api = true;
       list_resources(&sp);
   } else {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3900 Unknown arg in .status command: %s\n"), jcr->errmsg);
      dir->signal(BNET_EOD);
      return false;
   }
   dir->signal(BNET_EOD);
   return true;
}

#if defined(HAVE_WIN32)
int bacstat = 0;

/* Return a one line status for the tray monitor */
char *bac_status(char *buf, int buf_len)
{
   JCR *njcr;
   const char *termstat = _("Bacula Storage: Idle");
   struct s_last_job *job;
   int stat = 0;                      /* Idle */

   if (!last_jobs) {
      goto done;
   }
   Dmsg0(1000, "Begin bac_status jcr loop.\n");
   foreach_jcr(njcr) {
      if (njcr->JobId != 0) {
         stat = JS_Running;
         termstat = _("Bacula Storage: Running");
         break;
      }
   }
   endeach_jcr(njcr);

   if (stat != 0) {
      goto done;
   }
   if (last_jobs->size() > 0) {
      job = (struct s_last_job *)last_jobs->last();
      stat = job->JobStatus;
      switch (job->JobStatus) {
      case JS_Canceled:
         termstat = _("Bacula Storage: Last Job Canceled");
         break;
      case JS_ErrorTerminated:
      case JS_FatalError:
         termstat = _("Bacula Storage: Last Job Failed");
         break;
      default:
         if (job->Errors) {
            termstat = _("Bacula Storage: Last Job had Warnings");
         }
         break;
      }
   }
   Dmsg0(1000, "End bac_status jcr loop.\n");
done:
   bacstat = stat;
   if (buf) {
      bstrncpy(buf, termstat, buf_len);
   }
   return buf;
}

#endif /* HAVE_WIN32 */
