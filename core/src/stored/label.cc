/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, MM
/**
 * @file
 * Bareos routines to handle labels
 */
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/bareos.h" /* pull in global headers */
#include "stored/stored.h"  /* pull in Storage Daemon headers */
#include "stored/stored_globals.h"
#include "stored/dev.h"
#include "stored/device.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/label.h"
#include "lib/edit.h"
#include "include/jcr.h"
#include "lib/version.h"
#include "lib/serial.h"

namespace storagedaemon {

/* Forward referenced functions */
static void CreateVolumeLabelRecord(DeviceControlRecord* dcr,
                                    Device* dev,
                                    DeviceRecord* rec);

/**
 * Read the volume label
 *
 * If dcr->VolumeName == NULL, we accept any Bareos Volume
 * If dcr->VolumeName[0] == 0, we accept any Bareos Volume
 * otherwise dcr->VolumeName must match the Volume.
 *
 * If VolName given, ensure that it matches
 *
 * Returns VOL_  code as defined in record.h
 *    VOL_NOT_READ
 *    VOL_OK                          good label found
 *    VOL_NO_LABEL                    volume not labeled
 *    VOL_IO_ERROR                    I/O error reading tape
 *    VOL_NAME_ERROR                  label has wrong name
 *    VOL_CREATE_ERROR                Error creating label
 *    VOL_VERSION_ERROR               label has wrong version
 *    VOL_LABEL_ERROR                 bad label type
 *    VOL_NO_MEDIA                    no media in drive
 *
 * The dcr block is emptied on return, and the Volume is rewound.
 */
int ReadDevVolumeLabel(DeviceControlRecord* dcr)
{
  JobControlRecord* jcr = dcr->jcr;
  char* VolName = dcr->VolumeName;
  DeviceRecord* record;
  bool ok = false;
  int status;
  bool want_ansi_label;
  bool have_ansi_label = false;

  /* We always write the label in an 64512 byte / 63k block.
   * so we never have problems reading the volume label. */
  dcr->dev->SetLabelBlocksize(dcr);

  Dmsg5(100,
        "Enter ReadVolumeLabel res=%d device=%s vol=%s dev_Vol=%s "
        "max_blocksize=%u\n",
        dcr->dev->NumReserved(), dcr->dev->print_name(), VolName,
        dcr->dev->VolHdr.VolumeName[0] ? dcr->dev->VolHdr.VolumeName : "*NULL*",
        dcr->dev->max_block_size);

  if (!dcr->dev->IsOpen()) {
    if (!dcr->dev->open(dcr, DeviceMode::OPEN_READ_ONLY)) {
      return VOL_IO_ERROR;
    }
  }

  dcr->dev->ClearLabeled();
  dcr->dev->ClearAppend();
  dcr->dev->ClearRead();
  dcr->dev->label_type = B_BAREOS_LABEL;

  if (!dcr->dev->rewind(dcr)) {
    Mmsg(jcr->errmsg, T_("Couldn't rewind device %s: ERR=%s\n"),
         dcr->dev->print_name(), dcr->dev->print_errmsg());
    Dmsg1(130, "return VOL_NO_MEDIA: %s", jcr->errmsg);
    return VOL_NO_MEDIA;
  }
  bstrncpy(dcr->dev->VolHdr.Id, "**error**", sizeof(dcr->dev->VolHdr.Id));

  /* The stored plugin handling the bSdEventLabelRead event can abort
   * the reading of the label by returning a non bRC_OK. */
  if (GeneratePluginEvent(jcr, bSdEventLabelRead, dcr) != bRC_OK) {
    Dmsg0(200, "Error from bSdEventLabelRead plugin event.\n");
    return VOL_NO_MEDIA;
  }

  // Read ANSI/IBM label if so requested
  want_ansi_label = dcr->VolCatInfo.LabelType != B_BAREOS_LABEL
                    || dcr->device_resource->label_type != B_BAREOS_LABEL;
  if (want_ansi_label || dcr->dev->HasCap(CAP_CHECKLABELS)) {
    status = ReadAnsiIbmLabel(dcr);
    // If we want a label and didn't find it, return error
    if (want_ansi_label && status != VOL_OK) { goto bail_out; }
    if (status == VOL_NAME_ERROR || status == VOL_LABEL_ERROR) {
      Mmsg(jcr->errmsg,
           T_("Wrong Volume mounted on device %s: Wanted %s have %s\n"),
           dcr->dev->print_name(), VolName, dcr->dev->VolHdr.VolumeName);
      if (!dcr->dev->poll && jcr->sd_impl->label_errors++ > 100) {
        Jmsg(jcr, M_FATAL, 0, T_("Too many tries: %s"), jcr->errmsg);
      }
      goto bail_out;
    }
    if (status != VOL_OK) { /* Not an ANSI/IBM label, so re-read */
      dcr->dev->rewind(dcr);
    } else {
      have_ansi_label = true;
    }
  }

  // Read the Bareos Volume label block
  record = new_record();
  EmptyBlock(dcr->block);

  Dmsg0(130, "Big if statement in ReadVolumeLabel\n");
  if (DeviceControlRecord::ReadStatus::Ok
      != dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
    Mmsg(jcr->errmsg,
         T_("Requested Volume \"%s\" on %s is not a Bareos "
            "labeled Volume, because: ERR=%s"),
         NPRT(VolName), dcr->dev->print_name(), dcr->dev->print_errmsg());
    Dmsg1(130, "%s", jcr->errmsg);
  } else if (!ReadRecordFromBlock(dcr, record)) {
    Mmsg(jcr->errmsg, T_("Could not read Volume label from block.\n"));
    Dmsg1(130, "%s", jcr->errmsg);
  } else if (!UnserVolumeLabel(dcr->dev, record)) {
    Mmsg(jcr->errmsg, T_("Could not UnSerialize Volume label: ERR=%s\n"),
         dcr->dev->print_errmsg());
    Dmsg1(130, "%s", jcr->errmsg);
  } else if (!bstrcmp(dcr->dev->VolHdr.Id, BareosId)) {
    Mmsg(jcr->errmsg, T_("Volume Header Id bad: %s\n"), dcr->dev->VolHdr.Id);
    Dmsg1(130, "%s", jcr->errmsg);
  } else {
    ok = true;
  }
  FreeRecord(record); /* finished reading Volume record */

  if (!dcr->dev->IsVolumeToUnload()) { dcr->dev->ClearUnload(); }

  if (!ok) {
    if (forge_on || jcr->sd_impl->ignore_label_errors) {
      dcr->dev->SetLabeled(); /* set has Bareos label */
      Jmsg(jcr, M_ERROR, 0, "%s", jcr->errmsg);
      goto ok_out;
    }
    Dmsg0(100, "No volume label - bailing out\n");
    status = VOL_NO_LABEL;
    goto bail_out;
  }

  /* At this point, we have read the first Bareos block, and
   * then read the Bareos Volume label. Now we need to
   * make sure we have the right Volume. */
  if (dcr->dev->VolHdr.VerNum != BareosTapeVersion) {
    Mmsg(jcr->errmsg,
         T_("Volume on %s has wrong Bareos version. Wanted %d got %d\n"),
         dcr->dev->print_name(), BareosTapeVersion, dcr->dev->VolHdr.VerNum);
    Dmsg1(130, "VOL_VERSION_ERROR: %s", jcr->errmsg);
    status = VOL_VERSION_ERROR;
    goto bail_out;
  }

  /* We are looking for either an unused Bareos tape (PRE_LABEL) or
   * a Bareos volume label (VOL_LABEL) */
  if (dcr->dev->VolHdr.LabelType != PRE_LABEL
      && dcr->dev->VolHdr.LabelType != VOL_LABEL) {
    Mmsg(jcr->errmsg, T_("Volume on %s has bad Bareos label type: %x\n"),
         dcr->dev->print_name(), dcr->dev->VolHdr.LabelType);
    Dmsg1(130, "%s", jcr->errmsg);
    if (!dcr->dev->poll && jcr->sd_impl->label_errors++ > 100) {
      Jmsg(jcr, M_FATAL, 0, T_("Too many tries: %s"), jcr->errmsg);
    }
    Dmsg0(150, "return VOL_LABEL_ERROR\n");
    status = VOL_LABEL_ERROR;
    goto bail_out;
  }

  dcr->dev->SetLabeled(); /* set has Bareos label */

  /* Compare Volume Names */
  Dmsg2(130, "Compare Vol names: VolName=%s hdr=%s\n", VolName ? VolName : "*",
        dcr->dev->VolHdr.VolumeName);
  if (VolName && *VolName && *VolName != '*'
      && !bstrcmp(dcr->dev->VolHdr.VolumeName, VolName)) {
    Mmsg(jcr->errmsg,
         T_("Wrong Volume mounted on device %s: Wanted %s have %s\n"),
         dcr->dev->print_name(), VolName, dcr->dev->VolHdr.VolumeName);
    Dmsg1(130, "%s", jcr->errmsg);
    /* Cancel Job if too many label errors
     *  => we are in a loop */
    if (!dcr->dev->poll && jcr->sd_impl->label_errors++ > 100) {
      Jmsg(jcr, M_FATAL, 0, "Too many tries: %s", jcr->errmsg);
    }
    Dmsg0(150, "return VOL_NAME_ERROR\n");
    status = VOL_NAME_ERROR;
    goto bail_out;
  }

  if (debug_level >= 200) { DumpVolumeLabel(dcr->dev); }

  Dmsg0(130, "Leave ReadVolumeLabel() VOL_OK\n");
  // If we are a streaming device, we only get one chance to read
  if (!dcr->dev->HasCap(CAP_STREAM)) {
    dcr->dev->rewind(dcr);
    if (have_ansi_label) {
      status = ReadAnsiIbmLabel(dcr);
      // If we want a label and didn't find it, return error
      if (status != VOL_OK) { goto bail_out; }
    }
  }

  Dmsg1(100, "Call reserve_volume=%s\n", dcr->dev->VolHdr.VolumeName);
  if (reserve_volume(dcr, dcr->dev->VolHdr.VolumeName) == NULL) {
    Mmsg2(jcr->errmsg, T_("Could not reserve volume %s on %s\n"),
          dcr->dev->VolHdr.VolumeName, dcr->dev->print_name());
    Dmsg2(150, "Could not reserve volume %s on %s\n",
          dcr->dev->VolHdr.VolumeName, dcr->dev->print_name());
    status = VOL_NAME_ERROR;
    goto bail_out;
  }

ok_out:
  /* The stored plugin handling the bSdEventLabelVerified event can override
   * the return value e.g. although we think the volume label is ok the plugin
   * has reasons to override that. So when the plugin returns something else
   * then bRC_OK it want to tell us the volume is not OK to use and as
   * such we return VOL_NAME_ERROR as error although it might not be the
   * best error it should be sufficient. */
  if (GeneratePluginEvent(jcr, bSdEventLabelVerified, dcr) != bRC_OK) {
    Dmsg0(200, "Error from bSdEventLabelVerified plugin event.\n");
    status = VOL_NAME_ERROR;
    goto bail_out;
  }
  EmptyBlock(dcr->block);

  /* Reset blocksizes from volinfo to device as we set blocksize to
   * DEFAULT_BLOCK_SIZE to read the label */
  dcr->dev->SetBlocksizes(dcr);

  return VOL_OK;

bail_out:
  EmptyBlock(dcr->block);
  dcr->dev->rewind(dcr);
  Dmsg1(150, "return %d\n", status);
  return status;
}

/**
 * Put a volume label into the block
 *
 * Returns: false on failure
 *          true  on success
 */
static bool WriteVolumeLabelToBlock(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  DeviceRecord rec;
  JobControlRecord* jcr = dcr->jcr;

  Dmsg0(130, "write Label in WriteVolumeLabelToBlock()\n");

  rec.data = GetMemory(SER_LENGTH_Volume_Label);
  EmptyBlock(dcr->block); /* Volume label always at beginning */

  CreateVolumeLabelRecord(dcr, dev, &rec);

  dcr->block->BlockNumber = 0;
  if (!WriteRecordToBlock(dcr, &rec)) {
    FreePoolMemory(rec.data);
    Jmsg1(jcr, M_FATAL, 0,
          T_("Cannot write Volume label to block for device %s\n"),
          dev->print_name());
    return false;
  } else {
    Dmsg2(130, "Wrote label of %d bytes to block. Vol=%s\n", rec.data_len,
          dcr->VolumeName);
  }
  FreePoolMemory(rec.data);
  return true;
}

/**
 * Write a Volume Label
 *  !!! Note, this is ONLY used for writing
 *            a fresh volume label.  Any data
 *            after the label will be destroyed,
 *            in fact, we write the label 5 times !!!!
 *
 *  This routine should be used only when labeling a blank tape.
 */
bool WriteNewVolumeLabelToDev(DeviceControlRecord* dcr,
                              const char* VolName,
                              const char* PoolName,
                              bool relabel)
{
  DeviceRecord* rec;
  JobControlRecord* jcr = dcr->jcr;
  Device* dev = dcr->dev;

  // Set the default blocksize to read the label
  dev->SetLabelBlocksize(dcr);

  Dmsg0(150, "write_volume_label()\n");
  if (*VolName == 0) {
    Pmsg0(0, "=== ERROR: WriteNewVolumeLabelToDev called with NULL VolName\n");
    goto bail_out;
  }
  if (relabel) {
    VolumeUnused(dcr); /* mark current volume unused */
    /* Truncate device */
    if (!dev->d_truncate(dcr)) { goto bail_out; }
    if (!dev->IsTape()) {
      dev->close(dcr); /* make sure file closed for rename */
    }
  }

  /* Set the new filename for open, ... */
  dev->setVolCatName(VolName);
  dcr->setVolCatName(VolName);
  Dmsg1(150, "New VolName=%s\n", VolName);


  if (!dev->open(dcr, DeviceMode::OPEN_READ_WRITE)) {
    /* If device is not tape, attempt to create it */
    if (dev->IsTape() || !dev->open(dcr, DeviceMode::CREATE_READ_WRITE)) {
      Jmsg3(jcr, M_WARNING, 0,
            T_("Open device %s Volume \"%s\" failed: ERR=%s\n"),
            dev->print_name(), dcr->VolumeName, dev->bstrerror());
      goto bail_out;
    }
  }
  Dmsg1(150, "Label type=%d\n", dev->label_type);

  /* Let any stored plugin know that we are about to write a new label to the
   * volume. */
  if (GeneratePluginEvent(jcr, bSdEventLabelWrite, dcr) != bRC_OK) {
    Dmsg0(200, "Error from bSdEventLabelWrite plugin event.\n");
    goto bail_out;
  }

  EmptyBlock(dcr->block);
  if (!dev->rewind(dcr)) {
    Dmsg2(130, "Bad status on %s from rewind: ERR=%s\n", dev->print_name(),
          dev->print_errmsg());
    if (!forge_on) { goto bail_out; }
  }

  /* Temporarily mark in append state to enable writing */
  dev->SetAppend();

  /* Create PRE_LABEL */
  CreateVolumeLabel(dev, VolName, PoolName);

  /* If we have already detected an ANSI label, re-read it
   *   to skip past it. Otherwise, we write a new one if
   *   so requested. */
  if (dev->label_type != B_BAREOS_LABEL) {
    if (ReadAnsiIbmLabel(dcr) != VOL_OK) {
      dev->rewind(dcr);
      goto bail_out;
    }
  } else if (!WriteAnsiIbmLabels(dcr, ANSI_VOL_LABEL, VolName)) {
    goto bail_out;
  }

  rec = new_record();
  CreateVolumeLabelRecord(dcr, dev, rec);
  rec->Stream = 0;
  rec->maskedStream = 0;

  if (!WriteRecordToBlock(dcr, rec)) {
    Dmsg2(130, "Bad Label write on %s: ERR=%s\n", dev->print_name(),
          dev->print_errmsg());
    FreeRecord(rec);
    goto bail_out;
  } else {
    Dmsg2(130, "Wrote label of %d bytes to %s\n", rec->data_len,
          dev->print_name());
    FreeRecord(rec);
  }

  Dmsg0(130, "Call WriteBlockToDev()\n");
  if (!dcr->WriteBlockToDev()) {
    Dmsg2(130, "Bad Label write on %s: ERR=%s\n", dev->print_name(),
          dev->print_errmsg());
    goto bail_out;
  }
  dev = dcr->dev;

  Dmsg0(130, " Wrote block to device\n");

  if (dev->weof(1)) {
    dev->SetLabeled();
    WriteAnsiIbmLabels(dcr, ANSI_EOF_LABEL, dev->VolHdr.VolumeName);
  }

  if (debug_level >= 20) { DumpVolumeLabel(dev); }
  Dmsg0(100, "Call reserve_volume\n");
  if (reserve_volume(dcr, VolName) == NULL) {
    Mmsg2(jcr->errmsg, T_("Could not reserve volume %s on %s\n"),
          dev->VolHdr.VolumeName, dev->print_name());
    Dmsg1(100, "%s", jcr->errmsg);
    goto bail_out;
  }
  dev = dcr->dev; /* may have changed in reserve_volume */

  dev->ClearAppend(); /* remove append since this is PRE_LABEL */

  /* Reset blocksizes from volinfo to device as we set blocksize to
   * DEFAULT_BLOCK_SIZE to read the label. */
  dev->SetBlocksizes(dcr);

  return true;

bail_out:
  VolumeUnused(dcr);
  dev->ClearVolhdr();
  dev->ClearAppend(); /* remove append since this is PRE_LABEL */
  return false;
}

/**
 *  create_volume_label_record
 *   Serialize label (from dev->VolHdr structure) into device record.
 *   Assumes that the dev->VolHdr structure is properly
 *   initialized.
 */
static void CreateVolumeLabelRecord(DeviceControlRecord* dcr,
                                    Device* dev,
                                    DeviceRecord* rec)
{
  ser_declare;
  JobControlRecord* jcr = dcr->jcr;
  char buf[100];

  /* Serialize the label into the device record. */

  rec->data = CheckPoolMemorySize(rec->data, SER_LENGTH_Volume_Label);
  SerBegin(rec->data, SER_LENGTH_Volume_Label);
  SerString(dev->VolHdr.Id);

  ser_uint32(dev->VolHdr.VerNum);

  // make sure this volume wasn't written by bacula 1.26 or earlier
  ASSERT(dev->VolHdr.VerNum >= 11);
  SerBtime(dev->VolHdr.label_btime);
  dev->VolHdr.write_btime = GetCurrentBtime();
  SerBtime(dev->VolHdr.write_btime);
  dev->VolHdr.write_date = 0;
  dev->VolHdr.write_time = 0;
  ser_float64(dev->VolHdr.write_date); /* 0 if VerNum >= 11 */
  ser_float64(dev->VolHdr.write_time); /* 0  if VerNum >= 11 */

  SerString(dev->VolHdr.VolumeName);
  SerString(dev->VolHdr.PrevVolumeName);
  SerString(dev->VolHdr.PoolName);
  SerString(dev->VolHdr.PoolType);
  SerString(dev->VolHdr.MediaType);

  SerString(dev->VolHdr.HostName);
  SerString(dev->VolHdr.LabelProg);
  SerString(dev->VolHdr.ProgVersion);
  SerString(dev->VolHdr.ProgDate);

  SerEnd(rec->data, SER_LENGTH_Volume_Label);
  bstrncpy(dcr->VolumeName, dev->VolHdr.VolumeName, sizeof(dcr->VolumeName));
  rec->data_len = SerLength(rec->data);
  rec->FileIndex = dev->VolHdr.LabelType;
  rec->VolSessionId = jcr->VolSessionId;
  rec->VolSessionTime = jcr->VolSessionTime;
  rec->Stream = jcr->sd_impl->NumWriteVolumes;
  rec->maskedStream = jcr->sd_impl->NumWriteVolumes;
  Dmsg2(150, "Created Vol label rec: FI=%s len=%d\n",
        FI_to_ascii(buf, rec->FileIndex), rec->data_len);
}

// Create a volume label in memory
void CreateVolumeLabel(Device* dev, const char* VolName, const char* PoolName)
{
  DeviceResource* device_resource = dev->device_resource;

  Dmsg0(130, "Start CreateVolumeLabel()\n");

  ASSERT(dev != NULL);

  dev->ClearVolhdr(); /* clear any old volume info */

  bstrncpy(dev->VolHdr.Id, BareosId, sizeof(dev->VolHdr.Id));
  dev->VolHdr.VerNum = BareosTapeVersion;

  dev->VolHdr.LabelType = PRE_LABEL; /* Mark tape as unused */
  bstrncpy(dev->VolHdr.VolumeName, VolName, sizeof(dev->VolHdr.VolumeName));
  bstrncpy(dev->VolHdr.PoolName, PoolName, sizeof(dev->VolHdr.PoolName));
  bstrncpy(dev->VolHdr.MediaType, device_resource->media_type,
           sizeof(dev->VolHdr.MediaType));

  bstrncpy(dev->VolHdr.PoolType, "Backup", sizeof(dev->VolHdr.PoolType));

  dev->VolHdr.label_btime = GetCurrentBtime();
  dev->VolHdr.label_date = 0;
  dev->VolHdr.label_time = 0;

  if (gethostname(dev->VolHdr.HostName, sizeof(dev->VolHdr.HostName)) != 0) {
    dev->VolHdr.HostName[0] = 0;
  }
  bstrncpy(dev->VolHdr.LabelProg, my_name, sizeof(dev->VolHdr.LabelProg));
  snprintf(dev->VolHdr.ProgVersion, sizeof(dev->VolHdr.ProgVersion),
           "Ver. %.26s %.17s", kBareosVersionStrings.Full,
           kBareosVersionStrings.Date);
  snprintf(dev->VolHdr.ProgDate, sizeof(dev->VolHdr.ProgDate), "Build %s",
           kBareosVersionStrings.ProgDateTime);
  dev->SetLabeled(); /* set has Bareos label */
  if (debug_level >= 90) { DumpVolumeLabel(dev); }
}

/**
 * Create session label
 *  The pool memory must be released by the calling program
 */
static void CreateSessionLabel(DeviceControlRecord* dcr,
                               DeviceRecord* rec,
                               int label)
{
  JobControlRecord* jcr = dcr->jcr;
  ser_declare;

  rec->VolSessionId = jcr->VolSessionId;
  rec->VolSessionTime = jcr->VolSessionTime;
  rec->Stream = jcr->JobId;
  rec->maskedStream = jcr->JobId;

  rec->data = CheckPoolMemorySize(rec->data, SER_LENGTH_Session_Label);
  SerBegin(rec->data, SER_LENGTH_Session_Label);
  SerString(BareosId);
  ser_uint32(BareosTapeVersion);


  ser_uint32(jcr->JobId);

  /* Changed in VerNum 11 */
  SerBtime(GetCurrentBtime());
  ser_float64(0);

  SerString(dcr->pool_name);
  SerString(dcr->pool_type);
  SerString(jcr->sd_impl->job_name); /* base Job name */
  SerString(jcr->client_name);

  /* Added in VerNum 10 */
  SerString(jcr->Job); /* Unique name of this Job */
  SerString(jcr->sd_impl->fileset_name);
  ser_uint32(jcr->getJobType());
  ser_uint32(jcr->getJobLevel());
  /* Added in VerNum 11 */
  SerString(jcr->sd_impl->fileset_md5);

  if (label == EOS_LABEL) {
    ser_uint32(jcr->JobFiles);
    ser_uint64(jcr->JobBytes);
    ser_uint32(dcr->StartBlock);
    ser_uint32(dcr->EndBlock);
    ser_uint32(dcr->StartFile);
    ser_uint32(dcr->EndFile);
    ser_uint32(jcr->JobErrors);

    /* Added in VerNum 11 */
    ser_uint32(jcr->getJobStatus());
  }
  SerEnd(rec->data, SER_LENGTH_Session_Label);
  rec->data_len = SerLength(rec->data);
  rec->remainder = rec->data_len;
}

/* Write session label
 *  Returns: false on failure
 *           true  on success
 */
bool WriteSessionLabel(DeviceControlRecord* dcr, int label)
{
  JobControlRecord* jcr = dcr->jcr;
  Device* dev = dcr->dev;
  DeviceRecord* rec;
  DeviceBlock* block = dcr->block;
  char buf1[100], buf2[100];

  rec = new_record();
  Dmsg1(130, "session_label record=%p\n", rec);
  if (label != SOS_LABEL && label != EOS_LABEL) {
    Jmsg1(jcr, M_ABORT, 0, T_("Bad Volume session label = %d\n"), label);
  }
  CreateSessionLabel(dcr, rec, label);
  rec->FileIndex = label;

  /* We guarantee that the session record can totally fit
   *  into a block. If not, write the block, and put it in
   *  the next block. Having the sesssion record totally in
   *  one block makes reading them much easier (no need to
   *  read the next block). */
  if (!CanWriteRecordToBlock(block, rec)) {
    Dmsg0(150, "Cannot write session label to block.\n");
    if (!dcr->WriteBlockToDevice()) {
      Dmsg0(130, "Got session label WriteBlockToDev error.\n");
      FreeRecord(rec);
      return false;
    }
  }
  if (!WriteRecordToBlock(dcr, rec)) {
    FreeRecord(rec);
    return false;
  }

  Dmsg6(150,
        "Write sesson_label record JobId=%d FI=%s SessId=%d Strm=%s len=%d "
        "remainder=%d\n",
        jcr->JobId, FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
        stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
        rec->remainder);

  FreeRecord(rec);
  Dmsg2(150, "Leave WriteSessionLabel Block=%ud File=%ud\n", dev->GetBlockNum(),
        dev->GetFile());
  return true;
}

/*  unser_volume_label
 *
 * UnSerialize the Bareos Volume label into the device Volume_Label
 * structure.
 *
 * Assumes that the record is already read.
 *
 * Returns: false on error
 *          true  on success
 */
bool UnserVolumeLabel(Device* dev, DeviceRecord* rec)
{
  ser_declare;
  char buf1[100], buf2[100];

  if (rec->FileIndex != VOL_LABEL && rec->FileIndex != PRE_LABEL) {
    Mmsg3(dev->errmsg,
          T_("Expecting Volume Label, got FI=%s Stream=%s len=%d\n"),
          FI_to_ascii(buf1, rec->FileIndex),
          stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
    if (!forge_on) { return false; }
  }

  dev->VolHdr.LabelType = rec->FileIndex;
  dev->VolHdr.LabelSize = rec->data_len;


  /* UnSerialize the record into the Volume Header */
  rec->data = CheckPoolMemorySize(rec->data, SER_LENGTH_Volume_Label);
  SerBegin(rec->data, SER_LENGTH_Volume_Label);
  UnserString(dev->VolHdr.Id);
  unser_uint32(dev->VolHdr.VerNum);

  if (dev->VolHdr.VerNum >= 11) {
    UnserBtime(dev->VolHdr.label_btime);
    UnserBtime(dev->VolHdr.write_btime);
  } else { /* old way */
    unser_float64(dev->VolHdr.label_date);
    unser_float64(dev->VolHdr.label_time);
  }
  unser_float64(dev->VolHdr.write_date); /* Unused with VerNum >= 11 */
  unser_float64(dev->VolHdr.write_time); /* Unused with VerNum >= 11 */

  UnserString(dev->VolHdr.VolumeName);
  UnserString(dev->VolHdr.PrevVolumeName);
  UnserString(dev->VolHdr.PoolName);
  UnserString(dev->VolHdr.PoolType);
  UnserString(dev->VolHdr.MediaType);

  UnserString(dev->VolHdr.HostName);
  UnserString(dev->VolHdr.LabelProg);
  UnserString(dev->VolHdr.ProgVersion);
  UnserString(dev->VolHdr.ProgDate);

  SerEnd(rec->data, SER_LENGTH_Volume_Label);
  Dmsg0(190, "unser_vol_label\n");
  if (debug_level >= 190) { DumpVolumeLabel(dev); }
  return true;
}

bool UnserSessionLabel(Session_Label* label, DeviceRecord* rec)
{
  ser_declare;

  rec->data = CheckPoolMemorySize(rec->data, SER_LENGTH_Session_Label);
  UnserBegin(rec->data, SER_LENGTH_Session_Label);
  UnserString(label->Id);
  unser_uint32(label->VerNum);
  unser_uint32(label->JobId);
  if (label->VerNum >= 11) {
    UnserBtime(label->write_btime);
  } else {
    unser_float64(label->write_date);
  }
  unser_float64(label->write_time);
  UnserString(label->PoolName);
  UnserString(label->PoolType);
  UnserString(label->JobName);
  UnserString(label->ClientName);
  if (label->VerNum >= 10) {
    UnserString(label->Job); /* Unique name of this Job */
    UnserString(label->FileSetName);
    unser_uint32(label->JobType);
    unser_uint32(label->JobLevel);
  }
  if (label->VerNum >= 11) {
    UnserString(label->FileSetMD5);
  } else {
    label->FileSetMD5[0] = 0;
  }
  if (rec->FileIndex == EOS_LABEL) {
    unser_uint32(label->JobFiles);
    unser_uint64(label->JobBytes);
    unser_uint32(label->StartBlock);
    unser_uint32(label->EndBlock);
    unser_uint32(label->StartFile);
    unser_uint32(label->EndFile);
    unser_uint32(label->JobErrors);
    if (label->VerNum >= 11) {
      unser_uint32(label->JobStatus);
    } else {
      label->JobStatus = JS_Terminated; /* kludge */
    }
  }
  return true;
}

void DumpVolumeLabel(Device* dev)
{
  int dbl = debug_level;
  uint32_t File;
  const char* LabelType;
  char buf[30];

  debug_level = 1;
  File = dev->file;
  switch (dev->VolHdr.LabelType) {
    case PRE_LABEL:
      LabelType = "PRE_LABEL";
      break;
    case VOL_LABEL:
      LabelType = "VOL_LABEL";
      break;
    case EOM_LABEL:
      LabelType = "EOM_LABEL";
      break;
    case SOS_LABEL:
      LabelType = "SOS_LABEL";
      break;
    case EOS_LABEL:
      LabelType = "EOS_LABEL";
      break;
    case EOT_LABEL:
      goto bail_out;
    default:
      LabelType = buf;
      sprintf(buf, T_("Unknown %d"), dev->VolHdr.LabelType);
      break;
  }

  Pmsg11(-1,
         T_("\nVolume Label:\n"
            "Id                : %s"
            "VerNo             : %d\n"
            "VolName           : %s\n"
            "PrevVolName       : %s\n"
            "VolFile           : %d\n"
            "LabelType         : %s\n"
            "LabelSize         : %d\n"
            "PoolName          : %s\n"
            "MediaType         : %s\n"
            "PoolType          : %s\n"
            "HostName          : %s\n"
            ""),
         dev->VolHdr.Id, dev->VolHdr.VerNum, dev->VolHdr.VolumeName,
         dev->VolHdr.PrevVolumeName, File, LabelType, dev->VolHdr.LabelSize,
         dev->VolHdr.PoolName, dev->VolHdr.MediaType, dev->VolHdr.PoolType,
         dev->VolHdr.HostName);

  // make sure this volume wasn't written by bacula 1.26 or earlier
  ASSERT(dev->VolHdr.VerNum >= 11);
  char dt[50];
  bstrftime(dt, sizeof(dt), BtimeToUtime(dev->VolHdr.label_btime));
  Pmsg1(-1, T_("Date label written: %s\n"), dt);

bail_out:
  debug_level = dbl;
}

static void DumpSessionLabel(DeviceRecord* rec, const char* type)
{
  int dbl;
  Session_Label label;
  char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], ec6[30], ec7[30];

  UnserSessionLabel(&label, rec);
  dbl = debug_level;
  debug_level = 1;
  Pmsg7(-1,
        T_("\n%s Record:\n"
           "JobId             : %d\n"
           "VerNum            : %d\n"
           "PoolName          : %s\n"
           "PoolType          : %s\n"
           "JobName           : %s\n"
           "ClientName        : %s\n"
           ""),
        type, label.JobId, label.VerNum, label.PoolName, label.PoolType,
        label.JobName, label.ClientName);

  if (label.VerNum >= 10) {
    Pmsg4(-1,
          T_("Job (unique name) : %s\n"
             "FileSet           : %s\n"
             "JobType           : %c\n"
             "JobLevel          : %c\n"
             ""),
          label.Job, label.FileSetName, label.JobType, label.JobLevel);
  }

  if (rec->FileIndex == EOS_LABEL) {
    Pmsg8(-1,
          T_("JobFiles          : %s\n"
             "JobBytes          : %s\n"
             "StartBlock        : %s\n"
             "EndBlock          : %s\n"
             "StartFile         : %s\n"
             "EndFile           : %s\n"
             "JobErrors         : %s\n"
             "JobStatus         : %c\n"
             ""),
          edit_uint64_with_commas(label.JobFiles, ec1),
          edit_uint64_with_commas(label.JobBytes, ec2),
          edit_uint64_with_commas(label.StartBlock, ec3),
          edit_uint64_with_commas(label.EndBlock, ec4),
          edit_uint64_with_commas(label.StartFile, ec5),
          edit_uint64_with_commas(label.EndFile, ec6),
          edit_uint64_with_commas(label.JobErrors, ec7), label.JobStatus);
  }
  // make sure this volume wasn't written by bacula 1.26 or earlier
  ASSERT(label.VerNum >= 11);
  char dt[50];
  bstrftime(dt, sizeof(dt), BtimeToUtime(label.write_btime));
  Pmsg1(-1, T_("Date written      : %s\n"), dt);

  debug_level = dbl;
}

void DumpLabelRecord(Device* dev, DeviceRecord* rec, bool verbose)
{
  const char* type;
  int dbl;

  if (rec->FileIndex == 0 && rec->VolSessionId == 0
      && rec->VolSessionTime == 0) {
    return;
  }
  dbl = debug_level;
  debug_level = 1;
  switch (rec->FileIndex) {
    case PRE_LABEL:
      type = T_("Fresh Volume");
      break;
    case VOL_LABEL:
      type = T_("Volume");
      break;
    case SOS_LABEL:
      type = T_("Begin Job Session");
      break;
    case EOS_LABEL:
      type = T_("End Job Session");
      break;
    case EOM_LABEL:
      type = T_("End of Media");
      break;
    case EOT_LABEL:
      type = T_("End of Tape");
      break;
    default:
      type = T_("Unknown");
      break;
  }
  if (verbose) {
    switch (rec->FileIndex) {
      case PRE_LABEL:
      case VOL_LABEL:
        UnserVolumeLabel(dev, rec);
        DumpVolumeLabel(dev);
        break;
      case SOS_LABEL:
        DumpSessionLabel(rec, type);
        break;
      case EOS_LABEL:
        DumpSessionLabel(rec, type);
        break;
      case EOM_LABEL:
        Pmsg7(-1,
              T_("%s Record: File:blk=%u:%u SessId=%d SessTime=%d JobId=%d "
                 "DataLen=%d\n"),
              type, dev->file, dev->block_num, rec->VolSessionId,
              rec->VolSessionTime, rec->Stream, rec->data_len);
        break;
      case EOT_LABEL:
        Pmsg0(-1, T_("End of physical tape.\n"));
        break;
      default:
        Pmsg7(-1,
              T_("%s Record: File:blk=%u:%u SessId=%d SessTime=%d JobId=%d "
                 "DataLen=%d\n"),
              type, dev->file, dev->block_num, rec->VolSessionId,
              rec->VolSessionTime, rec->Stream, rec->data_len);
        break;
    }
  } else {
    Session_Label label;
    char dt[50];
    switch (rec->FileIndex) {
      case SOS_LABEL:
        UnserSessionLabel(&label, rec);
        bstrftimes(dt, sizeof(dt), BtimeToUtime(label.write_btime));
        Pmsg6(-1,
              T_("%s Record: File:blk=%u:%u SessId=%d SessTime=%d JobId=%d\n"),
              type, dev->file, dev->block_num, rec->VolSessionId,
              rec->VolSessionTime, label.JobId);
        Pmsg4(-1, T_("   Job=%s Date=%s Level=%c Type=%c\n"), label.Job, dt,
              label.JobLevel, label.JobType);
        break;
      case EOS_LABEL:
        char ed1[30], ed2[30];
        UnserSessionLabel(&label, rec);
        bstrftimes(dt, sizeof(dt), BtimeToUtime(label.write_btime));
        Pmsg6(-1,
              T_("%s Record: File:blk=%u:%u SessId=%d SessTime=%d JobId=%d\n"),
              type, dev->file, dev->block_num, rec->VolSessionId,
              rec->VolSessionTime, label.JobId);
        Pmsg7(
            -1,
            T_("   Job=%s Date=%s Level=%c Type=%c Files=%s Bytes=%s Errors=%d "
               "Status=%c\n"),
            label.Job, dt, label.JobLevel, label.JobType,
            edit_uint64_with_commas(label.JobFiles, ed1),
            edit_uint64_with_commas(label.JobBytes, ed2), label.JobErrors,
            (char)label.JobStatus);
        break;
      case EOM_LABEL:
      case PRE_LABEL:
      case VOL_LABEL:
      default:
        Pmsg7(-1,
              T_("%s Record: File:blk=%u:%u SessId=%d SessTime=%d JobId=%d "
                 "DataLen=%d\n"),
              type, dev->file, dev->block_num, rec->VolSessionId,
              rec->VolSessionTime, rec->Stream, rec->data_len);
        break;
      case EOT_LABEL:
        break;
    }
  }
  debug_level = dbl;
}

/**
 * Write a volume label. This is ONLY called if we have a valid Bareos
 *   label of type PRE_LABEL or we are recyling an existing Volume.
 *
 *  Returns: true if OK
 *           false if unable to write it
 */
bool DeviceControlRecord::RewriteVolumeLabel(bool recycle)
{
  DeviceControlRecord* dcr = this;

  // Set the label blocksize to write the label
  dev->SetLabelBlocksize(dcr);

  if (!dev->open(dcr, DeviceMode::OPEN_READ_WRITE)) {
    Jmsg3(jcr, M_WARNING, 0,
          T_("Open device %s Volume \"%s\" failed: ERR=%s\n"),
          dev->print_name(), dcr->VolumeName, dev->bstrerror());
    return false;
  }

  Dmsg2(190, "set append found freshly labeled volume. fd=%d dev=%p\n", dev->fd,
        dev);

  // Let any stored plugin know that we are (re)writing the label.
  if (GeneratePluginEvent(jcr, bSdEventLabelWrite, dcr) != bRC_OK) {
    Dmsg0(200, "Error from bSdEventLabelWrite plugin event.\n");
    return false;
  }

  dev->VolHdr.LabelType = VOL_LABEL; /* set Volume label */
  dev->SetAppend();
  if (!WriteVolumeLabelToBlock(dcr)) {
    Dmsg0(200, "Error from write volume label.\n");
    return false;
  }

  Dmsg1(150, "wrote vol label to block. Vol=%s\n", dcr->VolumeName);

  dev->setVolCatInfo(false);
  dev->VolCatInfo.VolCatBytes = 0; /* reset byte count */

  /* If we are not dealing with a streaming device,
   *  write the block now to ensure we have write permission.
   *  It is better to find out now rather than later.
   * We do not write the block now if this is an ANSI label. This
   *  avoids re-writing the ANSI label, which we do not want to do. */
  if (!dev->HasCap(CAP_STREAM)) {
    if (!dev->rewind(dcr)) {
      Jmsg2(jcr, M_FATAL, 0, T_("Rewind error on device %s: ERR=%s\n"),
            dev->print_name(), dev->print_errmsg());
      return false;
    }
    if (recycle) {
      Dmsg1(150, "Doing recycle. Vol=%s\n", dcr->VolumeName);
      if (!dev->d_truncate(dcr)) {
        Jmsg2(jcr, M_FATAL, 0, T_("Truncate error on device %s: ERR=%s\n"),
              dev->print_name(), dev->print_errmsg());
        return false;
      }
      if (!dev->open(dcr, DeviceMode::OPEN_READ_WRITE)) {
        Jmsg2(jcr, M_FATAL, 0,
              T_("Failed to re-open after truncate on device %s: ERR=%s\n"),
              dev->print_name(), dev->print_errmsg());
        return false;
      }
    }

    /* If we have already detected an ANSI label, re-read it
     *   to skip past it. Otherwise, we write a new one if
     *   so requested. */
    if (dev->label_type != B_BAREOS_LABEL) {
      if (ReadAnsiIbmLabel(dcr) != VOL_OK) {
        dev->rewind(dcr);
        return false;
      }
    } else if (!WriteAnsiIbmLabels(dcr, ANSI_VOL_LABEL,
                                   dev->VolHdr.VolumeName)) {
      return false;
    }

    /* Attempt write to check write permission */
    Dmsg1(200, "Attempt to write to device fd=%d.\n", dev->fd);
    if (!dcr->WriteBlockToDev()) {
      Jmsg2(jcr, M_ERROR, 0, T_("Unable to write device %s: ERR=%s\n"),
            dev->print_name(), dev->print_errmsg());
      Dmsg0(200, "===ERROR write block to dev\n");
      return false;
    }
  }
  dev->SetLabeled();
  /* Set or reset Volume statistics */
  dev->VolCatInfo.VolCatJobs = 0;
  dev->VolCatInfo.VolCatFiles = 0;
  dev->VolCatInfo.VolCatErrors = 0;
  dev->VolCatInfo.VolCatBlocks = 0;
  dev->VolCatInfo.VolCatRBytes = 0;
  if (recycle) {
    dev->VolCatInfo.VolCatMounts++;
    dev->VolCatInfo.VolCatRecycles++;
    dcr->DirCreateJobmediaRecord(true);
  } else {
    dev->VolCatInfo.VolCatMounts = 1;
    dev->VolCatInfo.VolCatRecycles = 0;
    dev->VolCatInfo.VolCatWrites = 1;
    dev->VolCatInfo.VolCatReads = 1;
  }
  Dmsg1(150, "dir_update_vol_info. Set Append vol=%s\n", dcr->VolumeName);
  dev->VolCatInfo.VolFirstWritten = time(NULL);
  bstrncpy(dev->VolCatInfo.VolCatStatus, "Append",
           sizeof(dev->VolCatInfo.VolCatStatus));
  dev->setVolCatName(dcr->VolumeName);
  if (!dcr->DirUpdateVolumeInfo(
          is_labeloperation::True)) { /* indicate doing relabel */
    return false;
  }
  if (recycle) {
    Jmsg(jcr, M_INFO, 0,
         T_("Recycled volume \"%s\" on device %s, all previous data lost.\n"),
         dcr->VolumeName, dev->print_name());
  } else {
    Jmsg(jcr, M_INFO, 0,
         T_("Wrote label to prelabeled Volume \"%s\" on device %s\n"),
         dcr->VolumeName, dev->print_name());
  }
  /* End writing real Volume label (from pre-labeled tape), or recycling
   *  the volume. */
  Dmsg1(150, "OK from rewrite vol label. Vol=%s\n", dcr->VolumeName);

  /* reset blocksizes from volinfo to device as we set blocksize to
   * DEFAULT_BLOCK_SIZE to write the label */
  dev->SetBlocksizes(dcr);

  /* Let any stored plugin know the label was rewritten and as such is verified
   * . */
  if (GeneratePluginEvent(jcr, bSdEventLabelVerified, dcr) != bRC_OK) {
    Dmsg0(200, "Error from bSdEventLabelVerified plugin event.\n");
    return false;
  }

  return true;
}

} /* namespace storagedaemon */
