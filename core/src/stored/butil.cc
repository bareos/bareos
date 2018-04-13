/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Utility routines for "tool" programs such as bscan, bls,
 * bextract, ...  Some routines also used by Bareos.
 *
 * Normally nothing in this file is called by the Storage
 * daemon because we interact more directly with the user
 * i.e. printf, ...
 */

#include "bareos.h"
#include "stored.h"

/* Forward referenced functions */
static bool setup_to_access_device(DeviceControlRecord *dcr, JobControlRecord *jcr, char *dev_name,
                                   const char *VolumeName, bool readonly);
static DeviceResource *find_device_res(char *device_name, bool readonly);
static void my_free_jcr(JobControlRecord *jcr);

/* Global variables */
char SD_IMP_EXP *configfile;
STORES SD_IMP_EXP *me = NULL;         /* Our Global resource */
ConfigurationParser SD_IMP_EXP *my_config = NULL;  /* Our Global config */

/**
 * Setup a "daemon" JobControlRecord for the various standalone tools (e.g. bls, bextract, bscan, ...)
 */
JobControlRecord *setup_jcr(const char *name, char *dev_name,
               BootStrapRecord *bsr, DirectorResource *director, DeviceControlRecord *dcr,
               const char *VolumeName, bool readonly)
{
   JobControlRecord *jcr = new_jcr(sizeof(JobControlRecord), my_free_jcr);

   jcr->bsr = bsr;
   jcr->director = director;
   jcr->VolSessionId = 1;
   jcr->VolSessionTime = (uint32_t)time(NULL);
   jcr->NumReadVolumes = 0;
   jcr->NumWriteVolumes = 0;
   jcr->JobId = 0;
   jcr->setJobType(JT_CONSOLE);
   jcr->setJobLevel(L_FULL);
   jcr->JobStatus = JS_Terminated;
   jcr->where = bstrdup("");
   jcr->job_name = get_pool_memory(PM_FNAME);
   pm_strcpy(jcr->job_name, "Dummy.Job.Name");
   jcr->client_name = get_pool_memory(PM_FNAME);
   pm_strcpy(jcr->client_name, "Dummy.Client.Name");
   bstrncpy(jcr->Job, name, sizeof(jcr->Job));
   jcr->fileset_name = get_pool_memory(PM_FNAME);
   pm_strcpy(jcr->fileset_name, "Dummy.fileset.name");
   jcr->fileset_md5 = get_pool_memory(PM_FNAME);
   pm_strcpy(jcr->fileset_md5, "Dummy.fileset.md5");

   new_plugins(jcr);  /* instantiate plugins */

   init_autochangers();
   create_volume_lists();

   if (!setup_to_access_device(dcr, jcr, dev_name, VolumeName, readonly)) {
      return NULL;
   }

   if (!bsr && VolumeName) {
      bstrncpy(dcr->VolumeName, VolumeName, sizeof(dcr->VolumeName));
   }

   bstrncpy(dcr->pool_name, "Default", sizeof(dcr->pool_name));
   bstrncpy(dcr->pool_type, "Backup", sizeof(dcr->pool_type));

   return jcr;
}

/**
 * Setup device, jcr, and prepare to access device.
 *   If the caller wants read access, acquire the device, otherwise,
 *     the caller will do it.
 */
static bool setup_to_access_device(DeviceControlRecord *dcr, JobControlRecord *jcr, char *dev_name,
                                   const char *VolumeName, bool readonly)
{
   Device *dev;
   char *p;
   DeviceResource *device;
   char VolName[MAX_NAME_LENGTH];

   init_reservations_lock();

   /*
    * If no volume name already given and no bsr, and it is a file,
    * try getting name from Filename
    */
   if (VolumeName) {
      bstrncpy(VolName, VolumeName, sizeof(VolName));
      if (strlen(VolumeName) >= MAX_NAME_LENGTH) {
         Jmsg0(jcr, M_ERROR, 0, _("Volume name or names is too long. Please use a .bsr file.\n"));
      }
   } else {
      VolName[0] = 0;
   }
   if (!jcr->bsr && VolName[0] == 0) {
      if (!bstrncmp(dev_name, "/dev/", 5)) {
         /* Try stripping file part */
         p = dev_name + strlen(dev_name);

         while (p >= dev_name && !IsPathSeparator(*p))
            p--;
         if (IsPathSeparator(*p)) {
            bstrncpy(VolName, p+1, sizeof(VolName));
            *p = 0;
         }
      }
   }

   if ((device = find_device_res(dev_name, readonly)) == NULL) {
      Jmsg2(jcr, M_FATAL, 0, _("Cannot find device \"%s\" in config file %s.\n"),
            dev_name, configfile);
      return false;
   }

   dev = init_dev(jcr, device);
   if (!dev) {
      Jmsg1(jcr, M_FATAL, 0, _("Cannot init device %s\n"), dev_name);
      return false;
   }
   device->dev = dev;
   jcr->dcr = dcr;
   setup_new_dcr_device(jcr, dcr, dev, NULL);
   if (!readonly) {
      dcr->set_will_write();
   }

   if (VolName[0]) {
      bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
   }
   bstrncpy(dcr->dev_name, device->device_name, sizeof(dcr->dev_name));

   create_restore_volume_list(jcr);

   if (readonly) {                    /* read only access? */
      Dmsg0(100, "Acquire device for read\n");
      if (!acquire_device_for_read(dcr)) {
         return false;
      }
      jcr->read_dcr = dcr;
   } else {
      if (!first_open_device(dcr)) {
         Jmsg1(jcr, M_FATAL, 0, _("Cannot open %s\n"), dev->print_name());
         return false;
      }
   }

   return true;
}

/**
 * Called here when freeing JobControlRecord so that we can get rid
 *  of "daemon" specific memory allocated.
 */
static void my_free_jcr(JobControlRecord *jcr)
{
   if (jcr->job_name) {
      free_pool_memory(jcr->job_name);
      jcr->job_name = NULL;
   }

   if (jcr->client_name) {
      free_pool_memory(jcr->client_name);
      jcr->client_name = NULL;
   }

   if (jcr->fileset_name) {
      free_pool_memory(jcr->fileset_name);
      jcr->fileset_name = NULL;
   }

   if (jcr->fileset_md5) {
      free_pool_memory(jcr->fileset_md5);
      jcr->fileset_md5 = NULL;
   }

   if (jcr->comment) {
      free_pool_memory(jcr->comment);
      jcr->comment = NULL;
   }

   if (jcr->VolList) {
      free_restore_volume_list(jcr);
   }

   if (jcr->dcr) {
      free_dcr(jcr->dcr);
      jcr->dcr = NULL;
   }

   return;
}

/**
 * Search for device resource that corresponds to
 * device name on command line (or default).
 *
 * Returns: NULL on failure
 *          Device resource pointer on success
 */
static DeviceResource *find_device_res(char *device_name, bool readonly)
{
   bool found = false;
   DeviceResource *device;

   Dmsg0(900, "Enter find_device_res\n");
   LockRes();
   foreach_res(device, R_DEVICE) {
      Dmsg2(900, "Compare %s and %s\n", device->device_name, device_name);
      if (bstrcmp(device->device_name, device_name)) {
         found = true;
         break;
      }
   }

   if (!found) {
      /* Search for name of Device resource rather than archive name */
      if (device_name[0] == '"') {
         int len = strlen(device_name);
         bstrncpy(device_name, device_name+1, len+1);
         len--;
         if (len > 0) {
            device_name[len-1] = 0;   /* zap trailing " */
         }
      }
      foreach_res(device, R_DEVICE) {
         Dmsg2(900, "Compare %s and %s\n", device->name(), device_name);
         if (bstrcmp(device->name(), device_name)) {
            found = true;
            break;
         }
      }
   }
   UnlockRes();

   if (!found) {
      Pmsg2(0, _("Could not find device \"%s\" in config file %s.\n"), device_name,
            configfile);
      return NULL;
   }

   if (readonly) {
      Pmsg1(0, _("Using device: \"%s\" for reading.\n"), device_name);
   }
   else {
      Pmsg1(0, _("Using device: \"%s\" for writing.\n"), device_name);
   }

   return device;
}

/**
 * Device got an error, attempt to analyse it
 */
void display_tape_error_status(JobControlRecord *jcr, Device *dev)
{
   char *status;

   status = dev->status_dev();

   if (bit_is_set(BMT_EOD, status))
      Jmsg(jcr, M_ERROR, 0, _("Unexpected End of Data\n"));
   else if (bit_is_set(BMT_EOT, status))
      Jmsg(jcr, M_ERROR, 0, _("Unexpected End of Tape\n"));
   else if (bit_is_set(BMT_EOF, status))
      Jmsg(jcr, M_ERROR, 0, _("Unexpected End of File\n"));
   else if (bit_is_set(BMT_DR_OPEN, status))
      Jmsg(jcr, M_ERROR, 0, _("Tape Door is Open\n"));
   else if (!bit_is_set(BMT_ONLINE, status))
      Jmsg(jcr, M_ERROR, 0, _("Unexpected Tape is Off-line\n"));

   free(status);
}
