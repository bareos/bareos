/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, November MM
 */
/**
 * @file
 * Read code for Storage daemon
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/acquire.h"
#include "stored/bsr.h"
#include "stored/jcr_private.h"
#include "stored/mount.h"
#include "stored/read_record.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "include/jcr.h"

namespace storagedaemon {

/* Forward referenced subroutines */
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec);

/* Responses sent to the File daemon */
static char OK_data[] = "3000 OK data\n";
static char FD_error[] = "3000 error\n";
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";

/**
 * Read Data and send to File Daemon
 *
 * Returns: false on failure
 *          true  on success
 */
bool DoReadData(JobControlRecord* jcr)
{
  BareosSocket* fd = jcr->file_bsock;
  DeviceControlRecord* dcr = jcr->impl_->read_dcr;
  bool ok = true;

  Dmsg0(20, "Start read data.\n");

  if (!BnetSetBufferSize(fd, dcr->device->max_network_buffer_size,
                         BNET_SETBUF_WRITE)) {
    return false;
  }

  if (jcr->impl_->NumReadVolumes == 0) {
    Jmsg(jcr, M_FATAL, 0, _("No Volume names found for restore.\n"));
    fd->fsend(FD_error);
    return false;
  }

  Dmsg2(200, "Found %d volumes names to restore. First=%s\n",
        jcr->impl_->NumReadVolumes, jcr->impl_->VolList->VolumeName);

  /*
   * Ready device for reading
   */
  if (!AcquireDeviceForRead(dcr)) {
    fd->fsend(FD_error);
    return false;
  }

  /*
   * Let any SD plugin know now its time to setup the record translation infra.
   */
  if (GeneratePluginEvent(jcr, bsdEventSetupRecordTranslation, dcr) != bRC_OK) {
    jcr->setJobStatus(JS_ErrorTerminated);
    return false;
  }

  /*
   * Tell File daemon we will send data
   */
  fd->fsend(OK_data);
  jcr->sendJobStatus(JS_Running);
  ok = ReadRecords(dcr, RecordCb, MountNextReadVolume);

  /*
   * Send end of data to FD
   */
  fd->signal(BNET_EOD);

  if (!ReleaseDevice(jcr->impl_->read_dcr)) { ok = false; }

  Dmsg0(30, "Done reading.\n");
  return ok;
}

/**
 * Called here for each record from ReadRecords()
 *
 * Returns: true if OK
 *          false if error
 */
static bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec)
{
  JobControlRecord* jcr = dcr->jcr;
  BareosSocket* fd = jcr->file_bsock;
  bool ok = true;
  POOLMEM* save_msg;
  char ec1[50], ec2[50];

  if (rec->FileIndex < 0) { return true; }

  Dmsg5(400, "Send to FD: SessId=%u SessTim=%u FI=%s Strm=%s, len=%d\n",
        rec->VolSessionId, rec->VolSessionTime,
        FI_to_ascii(ec1, rec->FileIndex),
        stream_to_ascii(ec2, rec->Stream, rec->FileIndex), rec->data_len);

  /*
   * Send record header to File daemon
   */
  if (!fd->fsend(rec_header, rec->VolSessionId, rec->VolSessionTime,
                 rec->FileIndex, rec->Stream, rec->data_len)) {
    Pmsg1(000, _(">filed: Error Hdr=%s"), fd->msg);
    Jmsg1(jcr, M_FATAL, 0, _("Error sending to File daemon. ERR=%s\n"),
          fd->bstrerror());
    return false;
  } else {
    Dmsg1(400, ">filed: Hdr=%s\n", fd->msg);
  }

  /*
   * Send data record to File daemon
   */
  save_msg = fd->msg;  /* save fd message pointer */
  fd->msg = rec->data; /* pass data directly to the FD */
  fd->message_length = rec->data_len;

  Dmsg1(400, ">filed: send %d bytes data.\n", fd->message_length);
  if (!fd->send()) {
    Pmsg1(000, _("Error sending to FD. ERR=%s\n"), fd->bstrerror());
    Jmsg1(jcr, M_FATAL, 0, _("Error sending to File daemon. ERR=%s\n"),
          fd->bstrerror());

    ok = false;
  }
  fd->msg = save_msg; /* restore fd message pointer */

  return ok;
}

} /* namespace storagedaemon */
