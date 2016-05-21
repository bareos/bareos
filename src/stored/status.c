/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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
 * This file handles the status command
 *
 * Kern Sibbald, May MMIII
 */

#include "bareos.h"
#include "stored.h"
#include "lib/status.h"

/* Imported functions */
extern bool GetWindowsVersionString(char *buf, int maxsiz);

/* Imported variables */
extern void *start_heap;

/* Static variables */
static char statuscmd[] =
   "status %s\n";
static char dotstatuscmd[] =
   ".status %127s\n";

static char OKdotstatus[] =
   "3000 OK .status\n";
static char DotStatusJob[] =
   "JobId=%d JobStatus=%c JobErrors=%d\n";

/* Forward referenced functions */
static void sendit(const char *msg, int len, STATUS_PKT *sp);
static void sendit(POOL_MEM &msg, int len, STATUS_PKT *sp);
static void sendit(const char *msg, int len, void *arg);

static void trigger_device_status_hook(JCR *jcr,
                                       DEVRES *device,
                                       STATUS_PKT *sp,
                                       bsdEventType eventType);
static void send_blocked_status(DEVICE *dev, STATUS_PKT *sp);
static void send_device_status(DEVICE *dev, STATUS_PKT *sp);
static void list_terminated_jobs(STATUS_PKT *sp);
static void list_running_jobs(STATUS_PKT *sp);
static void list_jobs_waiting_on_reservation(STATUS_PKT *sp);
static void list_status_header(STATUS_PKT *sp);
static void list_devices(JCR *jcr, STATUS_PKT *sp, const char *devicenames);

static const char *level_to_str(int level);

/*
 * Status command from Director
 */
static void output_status(JCR *jcr, STATUS_PKT *sp, const char *devicenames)
{
   int len;
   POOL_MEM msg(PM_MESSAGE);

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
   list_devices(jcr, sp, devicenames);

   if (!sp->api) {
      len = Mmsg(msg, _("Used Volume status:\n"));
      sendit(msg, len, sp);
   }

   list_volumes(sendit, (void *)sp);
   if (!sp->api) {
      len = pm_strcpy(msg, "====\n\n");
      sendit(msg, len, sp);
   }

   list_spool_stats(sendit, (void *)sp);
   if (!sp->api) {
      len = pm_strcpy(msg, "====\n\n");
      sendit(msg, len, sp);
   }
}

static void list_resources(STATUS_PKT *sp)
{
#ifdef when_working
   int len;
   POOL_MEM msg(PM_MESSAGE);

   if (!sp->api) {
      len = Mmsg(msg, _("\nSD Resources:\n"));
      sendit(msg, len, sp);
   }

   dump_resource(R_DEVICE, resources[R_DEVICE - my_config->m_r_first], sp, false);

   if (!sp->api) {
      len = pm_strcpy(msg, "====\n\n");
      sendit(msg, len, sp);
   }
#endif
}

#ifdef xxxx
static find_device(char *devname)
{
   bool found;
   DEVRES *device;
   AUTOCHANGERRES *changer;

   foreach_res(device, R_DEVICE) {
      if (strcasecmp(device->name(), devname) == 0) {
         found = true;
         break;
      }
   }

   if (!found) {
      foreach_res(changer, R_AUTOCHANGER) {
         if (strcasecmp(changer->name(), devname) == 0) {
            break;
         }
      }
   }
}
#endif

static bool need_to_list_device(const char *devicenames, const char *devicename)
{
   char *cur, *bp;
   POOL_MEM namelist;

   /*
    * Make a local copy that we can split on ','
    */
   pm_strcpy(namelist, devicenames);

   /*
    * See if devicename is in the list.
    */
   cur = namelist.c_str();
   while (cur) {
      bp = strchr(cur, ',');
      if (bp) {
         *bp++ = '\0';
      }

      if (bstrcasecmp(cur, devicename)) {
         return true;
      }

      cur = bp;
   }

   return false;
}

static void list_devices(JCR *jcr, STATUS_PKT *sp, const char *devicenames)
{
   int len;
   int bpb;
   DEVICE *dev;
   DEVRES *device;
   AUTOCHANGERRES *changer;
   POOL_MEM msg(PM_MESSAGE);
   char b1[35], b2[35], b3[35];

   if (!sp->api) {
      len = Mmsg(msg, _("\nDevice status:\n"));
      sendit(msg, len, sp);
   }

   foreach_res(changer, R_AUTOCHANGER) {
      /*
       * See if we need to list this autochanger.
       */
      if (devicenames && !need_to_list_device(devicenames, changer->name())) {
         continue;
      }

      len = Mmsg(msg, _("Autochanger \"%s\" with devices:\n"), changer->name());
      sendit(msg, len, sp);

      foreach_alist(device, changer->device) {
         if (device->dev) {
            len = Mmsg(msg, "   %s\n", device->dev->print_name());
            sendit(msg, len, sp);
         } else {
            len = Mmsg(msg, "   %s\n", device->name());
            sendit(msg, len, sp);
         }
      }
   }

   foreach_res(device, R_DEVICE) {
      /*
       * See if we need to check for devicenames at all.
       */
      if (devicenames) {
         /*
          * See if this device is part of an autochanger.
          */
         if (device->changer_res) {
            /*
             * See if we need to list this particular device part of the given autochanger.
             */
            if (!need_to_list_device(devicenames, device->changer_res->name())) {
               continue;
            }
         } else {
            /*
             * Try matching a non autochanger device.
             */
            if (!need_to_list_device(devicenames, device->name())) {
               continue;
            }
         }
      }

      dev = device->dev;
      if (dev && dev->is_open()) {
         if (dev->is_labeled()) {
            const char *state;

            switch (dev->blocked()) {
            case BST_NOT_BLOCKED:
            case BST_DESPOOLING:
            case BST_RELEASING:
               state = _("mounted with");
               break;
            case BST_MOUNT:
               state = _("waiting for");
               break;
            case BST_WRITING_LABEL:
               state = _("being labeled with");
               break;
            case BST_DOING_ACQUIRE:
               state = _("being acquired with");
               break;
            case BST_UNMOUNTED:
            case BST_WAITING_FOR_SYSOP:
            case BST_UNMOUNTED_WAITING_FOR_SYSOP:
               state = _("waiting for sysop intervention");
               break;
            default:
               state = _("unknown state");
               break;
            }

            len = Mmsg(msg, _("\nDevice %s is %s:\n"
                              "    Volume:      %s\n"
                              "    Pool:        %s\n"
                              "    Media type:  %s\n"),
                       dev->print_name(), state,
                       dev->VolHdr.VolumeName,
                       dev->pool_name[0] ? dev->pool_name : "*unknown*",
                       dev->device->media_type);
            sendit(msg, len, sp);
         } else {
            len = Mmsg(msg, _("\nDevice %s open but no Bareos volume is currently mounted.\n"), dev->print_name());
            sendit(msg, len, sp);
         }

         trigger_device_status_hook(jcr, device, sp, bsdEventDriveStatus);
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

         trigger_device_status_hook(jcr, device, sp, bsdEventVolumeStatus);
      } else {
         if (dev) {
            len = Mmsg(msg, _("\nDevice %s is not open.\n"), dev->print_name());
            sendit(msg, len, sp);
            send_blocked_status(dev, sp);
         } else {
            len = Mmsg(msg, _("\nDevice \"%s\" is not open or does not exist.\n"), device->name());
            sendit(msg, len, sp);
         }
      }

      if (!sp->api) {
         len = pm_strcpy(msg, "==\n");
         sendit(msg, len, sp);
      }
   }

   if (!sp->api) {
      len = pm_strcpy(msg, "====\n\n");
      sendit(msg, len, sp);
   }
}

static void list_status_header(STATUS_PKT *sp)
{
   int len;
   POOL_MEM msg(PM_MESSAGE);
   char dt[MAX_TIME_LENGTH];
   char b1[35], b2[35], b3[35], b4[35], b5[35];
#if defined(HAVE_WIN32)
   char buf[300];
#endif

   len = Mmsg(msg, _("%s Version: %s (%s) %s %s %s\n"),
              my_name, VERSION, BDATE, HOST_OS, DISTNAME, DISTVER);
   sendit(msg, len, sp);

   bstrftime_nc(dt, sizeof(dt), daemon_start_time);

   len = Mmsg(msg, _("Daemon started %s. Jobs: run=%d, running=%d.\n"),
              dt, num_jobs_run, job_count());
   sendit(msg, len, sp);

#if defined(HAVE_WIN32)
   if (GetWindowsVersionString(buf, sizeof(buf))) {
      len = Mmsg(msg, "%s\n", buf);
      sendit(msg.c_str(), len, sp);
   }

   if (debug_level > 0) {
      len = Mmsg(msg, "APIs=%sOPT,%sATP,%sLPV,%sCFA,%sCFW,\n",
                 p_OpenProcessToken ? "" : "!",
                 p_AdjustTokenPrivileges ? "" : "!",
                 p_LookupPrivilegeValue ? "" : "!",
                 p_CreateFileA ? "" : "!",
                 p_CreateFileW ? "" : "!");
      sendit(msg.c_str(), len, sp);
      len = Mmsg(msg, " %sWUL,%sWMKD,%sGFAA,%sGFAW,%sGFAEA,%sGFAEW,%sSFAA,%sSFAW,%sBR,%sBW,%sSPSP,\n",
                 p_wunlink ? "" : "!",
                 p_wmkdir ? "" : "!",
                 p_GetFileAttributesA ? "" : "!",
                 p_GetFileAttributesW ? "" : "!",
                 p_GetFileAttributesExA ? "" : "!",
                 p_GetFileAttributesExW ? "" : "!",
                 p_SetFileAttributesA ? "" : "!",
                 p_SetFileAttributesW ? "" : "!",
                 p_BackupRead ? "" : "!",
                 p_BackupWrite ? "" : "!",
                 p_SetProcessShutdownParameters ? "" : "!");
      sendit(msg.c_str(), len, sp);
      len = Mmsg(msg, " %sWC2MB,%sMB2WC,%sFFFA,%sFFFW,%sFNFA,%sFNFW,%sSCDA,%sSCDW,\n",
                 p_WideCharToMultiByte ? "" : "!",
                 p_MultiByteToWideChar ? "" : "!",
                 p_FindFirstFileA ? "" : "!",
                 p_FindFirstFileW ? "" : "!",
                 p_FindNextFileA ? "" : "!",
                 p_FindNextFileW ? "" : "!",
                 p_SetCurrentDirectoryA ? "" : "!",
                 p_SetCurrentDirectoryW ? "" : "!");
      sendit(msg.c_str(), len, sp);
      len = Mmsg(msg, " %sGCDA,%sGCDW,%sGVPNW,%sGVNFVMPW\n",
                 p_GetCurrentDirectoryA ? "" : "!",
                 p_GetCurrentDirectoryW ? "" : "!",
                 p_GetVolumePathNameW ? "" : "!",
                 p_GetVolumeNameForVolumeMountPointW ? "" : "!");
      sendit(msg.c_str(), len, sp);
   }
#endif

   len = Mmsg(msg, _(" Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n"),
         edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
         edit_uint64_with_commas(sm_bytes, b2),
         edit_uint64_with_commas(sm_max_bytes, b3),
         edit_uint64_with_commas(sm_buffers, b4),
         edit_uint64_with_commas(sm_max_buffers, b5));
   sendit(msg, len, sp);
   len = Mmsg(msg, " Sizes: boffset_t=%d size_t=%d int32_t=%d int64_t=%d "
                   "mode=%d bwlimit=%skB/s\n",
              (int)sizeof(boffset_t), (int)sizeof(size_t), (int)sizeof(int32_t),
              (int)sizeof(int64_t), (int)DEVELOPER_MODE,
              edit_uint64_with_commas(me->max_bandwidth_per_job / 1024, b1));
   sendit(msg, len, sp);


   if (me->secure_erase_cmdline) {
      len = Mmsg(msg, _(" secure erase command='%s'\n"), me->secure_erase_cmdline);
      sendit(msg, len, sp);
   }

   len = list_sd_plugins(msg);
   if (len > 0) {
      sendit(msg.c_str(), len, sp);
   }
}

static void trigger_device_status_hook(JCR *jcr,
                                       DEVRES *device,
                                       STATUS_PKT *sp,
                                       bsdEventType eventType)
{
   bsdDevStatTrig dst;

   dst.device = device;
   dst.status = get_pool_memory(PM_MESSAGE);
   dst.status_length = 0;

   if (generate_plugin_event(jcr, eventType, &dst) == bRC_OK) {
      if (dst.status_length > 0) {
         sendit(dst.status, dst.status_length, sp);
      }
   }
   free_pool_memory(dst.status);
}

static void send_blocked_status(DEVICE *dev, STATUS_PKT *sp)
{
   int len;
   POOL_MEM msg(PM_MESSAGE);

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

   /*
    * Send autochanger slot status
    */
   if (dev->is_autochanger()) {
      if (dev->get_slot() > 0) {
         len = Mmsg(msg, _("    Slot %hd %s loaded in drive %hd.\n"),
                    dev->get_slot(), dev->is_open() ? "is" : "was last", dev->drive);
         sendit(msg, len, sp);
      } else if (dev->get_slot() <= 0) {
         len = Mmsg(msg, _("    Drive %hd is not loaded.\n"), dev->drive);
         sendit(msg, len, sp);
      }
   }
   if (debug_level > 1) {
      send_device_status(dev, sp);
   }
}

static void send_device_status(DEVICE *dev, STATUS_PKT *sp)
{
   int len;
   DCR *dcr = NULL;
   bool found = false;
   POOL_MEM msg(PM_MESSAGE);

   if (debug_level > 5) {
      len = Mmsg(msg, _("Configured device capabilities:\n"));
      sendit(msg, len, sp);

      len = Mmsg(msg, "  %sEOF %sBSR %sBSF %sFSR %sFSF %sEOM %sREM %sRACCESS %sAUTOMOUNT %sLABEL %sANONVOLS %sALWAYSOPEN\n",
                 dev->has_cap(CAP_EOF) ? "" : "!",
                 dev->has_cap(CAP_BSR) ? "" : "!",
                 dev->has_cap(CAP_BSF) ? "" : "!",
                 dev->has_cap(CAP_FSR) ? "" : "!",
                 dev->has_cap(CAP_FSF) ? "" : "!",
                 dev->has_cap(CAP_EOM) ? "" : "!",
                 dev->has_cap(CAP_REM) ? "" : "!",
                 dev->has_cap(CAP_RACCESS) ? "" : "!",
                 dev->has_cap(CAP_AUTOMOUNT) ? "" : "!",
                 dev->has_cap(CAP_LABEL) ? "" : "!",
                 dev->has_cap(CAP_ANONVOLS) ? "" : "!",
                 dev->has_cap(CAP_ALWAYSOPEN) ? "" : "!");
      sendit(msg, len, sp);
   }

   len = Mmsg(msg, _("Device state:\n"));
   sendit(msg, len, sp);

   len = Mmsg(msg, "  %sOPENED %sTAPE %sLABEL %sMALLOC %sAPPEND %sREAD %sEOT %sWEOT %sEOF %sNEXTVOL %sSHORT %sMOUNTED\n",
      dev->is_open() ? "" : "!",
      dev->is_tape() ? "" : "!",
      dev->is_labeled() ? "" : "!",
      bit_is_set(ST_MALLOC, dev->state) ? "" : "!",
      dev->can_append() ? "" : "!",
      dev->can_read() ? "" : "!",
      dev->at_eot() ? "" : "!",
      bit_is_set(ST_WEOT, dev->state) ? "" : "!",
      dev->at_eof() ? "" : "!",
      bit_is_set(ST_NEXTVOL, dev->state) ? "" : "!",
      bit_is_set(ST_SHORT, dev->state) ? "" : "!",
      bit_is_set(ST_MOUNTED, dev->state) ? "" : "!");
   sendit(msg, len, sp);

   len = Mmsg(msg, _("  num_writers=%d reserves=%d block=%d\n"), dev->num_writers,
              dev->num_reserved(), dev->blocked());
   sendit(msg, len, sp);

   len = Mmsg(msg, _("Attached Jobs: "));
   sendit(msg, len, sp);
   dev->Lock();
   foreach_dlist(dcr, dev->attached_dcrs) {
      if (dcr->jcr) {
         if (found) {
            len = Mmsg(msg, ",%d", (int)dcr->jcr->JobId);
         } else {
            len = Mmsg(msg, "%d", (int)dcr->jcr->JobId);
         }
         sendit(msg, len, sp);
         found = true;
      }
   }
   dev->Unlock();
   sendit("\n", 1, sp);

   len = Mmsg(msg, _("Device parameters:\n"));
   sendit(msg, len, sp);
   len = Mmsg(msg, _("  Archive name: %s Device name: %s\n"),
              dev->archive_name(), dev->name());
   sendit(msg, len, sp);
   len = Mmsg(msg, _("  File=%u block=%u\n"),
              dev->file, dev->block_num);
   sendit(msg, len, sp);
   len = Mmsg(msg, _("  Min block=%u Max block=%u\n"),
              dev->min_block_size, dev->max_block_size);
   sendit(msg, len, sp);
}

static void list_running_jobs(STATUS_PKT *sp)
{
   JCR *jcr;
   DCR *dcr, *rdcr;
   bool found = false;
   time_t now = time(NULL);
   POOL_MEM msg(PM_MESSAGE);
   int len, avebps, bps, sec;
   char JobName[MAX_NAME_LENGTH];
   char b1[50], b2[50], b3[50], b4[50];

   if (!sp->api) {
      len = Mmsg(msg, _("\nRunning Jobs:\n"));
      sendit(msg, len, sp);
   }

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
                       rdcr->dev ? rdcr->dev->print_name():
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
                       dcr->dev ? dcr->dev->print_name():
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
      if (!sp->api) {
         len = Mmsg(msg, _("No Jobs running.\n"));
         sendit(msg, len, sp);
      }
   }

   if (!sp->api) {
      len = pm_strcpy(msg, "====\n");
      sendit(msg, len, sp);
   }
}

static void list_jobs_waiting_on_reservation(STATUS_PKT *sp)
{
   int len;
   JCR *jcr;
   POOL_MEM msg(PM_MESSAGE);

   if (!sp->api) {
      len = Mmsg(msg, _("\nJobs waiting to reserve a drive:\n"));
      sendit(msg, len, sp);
   }

   foreach_jcr(jcr) {
      if (!jcr->reserve_msgs) {
         continue;
      }
      send_drive_reserve_messages(jcr, sendit, sp);
   }
   endeach_jcr(jcr);

   if (!sp->api) {
      len = pm_strcpy(msg, "====\n");
      sendit(msg, len, sp);
   }
}

static void list_terminated_jobs(STATUS_PKT *sp)
{
   int len;
   char level[10];
   struct s_last_job *je;
   POOL_MEM msg(PM_MESSAGE);
   char dt[MAX_TIME_LENGTH], b1[30], b2[30];

   if (!sp->api) {
      len = pm_strcpy(msg, _("\nTerminated Jobs:\n"));
      sendit(msg, len, sp);
   }

   if (last_jobs->size() == 0) {
      if (!sp->api) {
         len = pm_strcpy(msg, "====\n");
         sendit(msg, len, sp);
      }
      return;
   }

   lock_last_jobs_list();

   if (!sp->api) {
      len = pm_strcpy(msg, _(" JobId  Level    Files      Bytes   Status   Finished        Name \n"));
      sendit(msg, len, sp);
      len = pm_strcpy(msg, _("===================================================================\n"));
      sendit(msg, len, sp);
   }

   foreach_dlist(je, last_jobs) {
      char JobName[MAX_NAME_LENGTH];
      const char *termstat;

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
         len = Mmsg(msg, _("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"),
                    je->JobId,
                    level,
                    edit_uint64_with_commas(je->JobFiles, b1),
                    edit_uint64_with_suffix(je->JobBytes, b2),
                    termstat,
                    dt,
                    JobName);
      } else {
         len = Mmsg(msg, _("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"),
                    je->JobId,
                    level,
                    edit_uint64_with_commas(je->JobFiles, b1),
                    edit_uint64_with_suffix(je->JobBytes, b2),
                    termstat,
                    dt,
                    JobName);
      }
      sendit(msg, len, sp);
   }

   unlock_last_jobs_list();

   if (!sp->api) {
      len = pm_strcpy(msg, "====\n");
      sendit(msg, len, sp);
   }
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
      break;
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
   POOL_MEM devicenames;
   STATUS_PKT sp;
   BSOCK *dir = jcr->dir_bsock;

   sp.bs = dir;
   devicenames.check_size(dir->msglen);
   if (sscanf(dir->msg, statuscmd, devicenames.c_str()) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3900 No arg in status command: %s\n"), jcr->errmsg);
      dir->signal(BNET_EOD);

      return false;
   }
   unbash_spaces(devicenames);

   dir->fsend("\n");
   output_status(jcr, &sp, devicenames.c_str());
   dir->signal(BNET_EOD);

   return true;
}

/*
 * .status command from Director
 */
bool dotstatus_cmd(JCR *jcr)
{
   JCR *njcr;
   POOL_MEM cmd;
   STATUS_PKT sp;
   s_last_job* job;
   BSOCK *dir = jcr->dir_bsock;

   sp.bs = dir;
   cmd.check_size(dir->msglen);
   if (sscanf(dir->msg, dotstatuscmd, cmd.c_str()) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("3900 No arg in .status command: %s\n"), jcr->errmsg);
      dir->signal(BNET_EOD);

      return false;
   }
   unbash_spaces(cmd);

   Dmsg1(200, "cmd=%s\n", cmd.c_str());

   if (bstrcasecmp(cmd.c_str(), "current")) {
      dir->fsend(OKdotstatus, cmd.c_str());
      foreach_jcr(njcr) {
         if (njcr->JobId != 0) {
            dir->fsend(DotStatusJob, njcr->JobId, njcr->JobStatus, njcr->JobErrors);
         }
      }
      endeach_jcr(njcr);
   } else if (bstrcasecmp(cmd.c_str(), "last")) {
      dir->fsend(OKdotstatus, cmd.c_str());
      if ((last_jobs) && (last_jobs->size() > 0)) {
         job = (s_last_job*)last_jobs->last();
         dir->fsend(DotStatusJob, job->JobId, job->JobStatus, job->Errors);
      }
   } else if (bstrcasecmp(cmd.c_str(), "header")) {
       sp.api = true;
       list_status_header(&sp);
   } else if (bstrcasecmp(cmd.c_str(), "running")) {
       sp.api = true;
       list_running_jobs(&sp);
   } else if (bstrcasecmp(cmd.c_str(), "waitreservation")) {
       sp.api = true;
       list_jobs_waiting_on_reservation(&sp);
   } else if (bstrcasecmp(cmd.c_str(), "devices")) {
       sp.api = true;
       list_devices(jcr, &sp, NULL);
   } else if (bstrcasecmp(cmd.c_str(), "volumes")) {
       sp.api = true;
       list_volumes(sendit, &sp);
   } else if (bstrcasecmp(cmd.c_str(), "spooling")) {
       sp.api = true;
       list_spool_stats(sendit, &sp);
   } else if (bstrcasecmp(cmd.c_str(), "terminated")) {
       sp.api = true;
       list_terminated_jobs(&sp);
   } else if (bstrcasecmp(cmd.c_str(), "resources")) {
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
int bareosstat = 0;

/*
 * Return a one line status for the tray monitor
 */
char *bareos_status(char *buf, int buf_len)
{
   JCR *njcr;
   const char *termstat = _("Bareos Storage: Idle");
   struct s_last_job *job;
   int status = 0;                    /* Idle */

   if (!last_jobs) {
      goto done;
   }
   Dmsg0(1000, "Begin bareos_status jcr loop.\n");
   foreach_jcr(njcr) {
      if (njcr->JobId != 0) {
         status = JS_Running;
         termstat = _("Bareos Storage: Running");
         break;
      }
   }
   endeach_jcr(njcr);

   if (status != 0) {
      goto done;
   }
   if (last_jobs->size() > 0) {
      job = (struct s_last_job *)last_jobs->last();
      status = job->JobStatus;
      switch (job->JobStatus) {
      case JS_Canceled:
         termstat = _("Bareos Storage: Last Job Canceled");
         break;
      case JS_ErrorTerminated:
      case JS_FatalError:
         termstat = _("Bareos Storage: Last Job Failed");
         break;
      default:
         if (job->Errors) {
            termstat = _("Bareos Storage: Last Job had Warnings");
         }
         break;
      }
   }
   Dmsg0(1000, "End bareos_status jcr loop.\n");
done:
   bareosstat = status;
   if (buf) {
      bstrncpy(buf, termstat, buf_len);
   }
   return buf;
}
#endif /* HAVE_WIN32 */
