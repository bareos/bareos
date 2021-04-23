/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
 * low level operations on device (storage device)
 *
 * NOTE!!!! None of these routines are reentrant. You must
 * use dev->rLock() and dev->Unlock() at a higher level,
 * or use the xxx_device() equivalents. By moving the
 * thread synchronization to a higher level, we permit
 * the higher level routines to "seize" the device and
 * to carry out operations without worrying about who
 * set what lock (i.e. race conditions).
 *
 * Note, this is the device dependent code, and may have
 * to be modified for each system, but is meant to
 * be as "generic" as possible.
 *
 * The purpose of this code is to develop a SIMPLE Storage
 * daemon. More complicated coding (double buffering, writer
 * thread, ...) is left for a later version.
 */

/*
 * Handling I/O errors and end of tape conditions are a bit tricky.
 * This is how it is currently done when writing.
 * On either an I/O error or end of tape,
 * we will stop writing on the physical device (no I/O recovery is
 * attempted at least in this daemon). The state flag will be sent
 * to include ST_EOT, which is ephemeral, and ST_WEOT, which is
 * persistent. Lots of routines clear ST_EOT, but ST_WEOT is
 * cleared only when the problem goes away.  Now when ST_WEOT
 * is set all calls to WriteBlockToDevice() call the fix_up
 * routine. In addition, all threads are blocked
 * from writing on the tape by calling lock_dev(), and thread other
 * than the first thread to hit the EOT will block on a condition
 * variable. The first thread to hit the EOT will continue to
 * be able to read and write the tape (he sort of tunnels through
 * the locking mechanism -- see lock_dev() for details).
 *
 * Now presumably somewhere higher in the chain of command
 * (device.c), someone will notice the EOT condition and
 * get a new tape up, get the tape label read, and mark
 * the label for rewriting. Then this higher level routine
 * will write the unwritten buffer to the new volume.
 * Finally, he will release
 * any blocked threads by doing a broadcast on the condition
 * variable.  At that point, we should be totally back in
 * business with no lost data.
 */

#include "include/bareos.h"
#include "stored/block.h"
#include "stored/stored.h"
#include "stored/autochanger.h"
#include "stored/bsr.h"
#include "stored/device_control_record.h"
#include "stored/jcr_private.h"
#include "stored/sd_backends.h"
#include "lib/btimers.h"
#include "include/jcr.h"
#include "lib/berrno.h"

#ifndef HAVE_DYNAMIC_SD_BACKENDS
#  ifdef HAVE_GFAPI
#    include "backends/gfapi_device.h"
#  endif
#  ifdef HAVE_BAREOSSD_DROPLET_DEVICE
#    include "backends/chunked_device.h"
#    include "backends/droplet_device.h"
#  endif
#  ifdef HAVE_RADOS
#    include "backends/rados_device.h"
#  endif
#  ifdef HAVE_CEPHFS
#    include "backends/cephfs_device.h"
#  endif
#  include "backends/generic_tape_device.h"
#  ifdef HAVE_WIN32
#    include "backends/win32_tape_device.h"
#    include "backends/win32_fifo_device.h"
#  else
#    include "backends/unix_tape_device.h"
#    include "backends/unix_fifo_device.h"
#  endif
#endif /* HAVE_DYNAMIC_SD_BACKENDS */

#ifdef HAVE_WIN32
#  include "backends/win32_file_device.h"
#else
#  include "backends/unix_file_device.h"
#endif

#ifndef O_NONBLOCK
#  define O_NONBLOCK 0
#endif

namespace storagedaemon {

const char* Device::mode_to_str(DeviceMode mode)
{
  static const char* modes[] = {"CREATE_READ_WRITE", "OPEN_READ_WRITE",
                                "OPEN_READ_ONLY", "OPEN_WRITE_ONLY"};


  int idx = static_cast<int>(mode);

  if (idx < 1 || idx > 4) {
    static char buf[100];
    Bsnprintf(buf, sizeof(buf), "BAD mode=%d", idx);
    return buf;
  }

  return modes[idx - 1];
}


/**
 * Fabric to allocate and initialize the Device structure
 *
 * Note, for a tape, the device->archive_device_string is the device name
 * (e.g. /dev/nst0), and for a file, the device name
 * is the directory in which the file will be placed.
 */

Device* FactoryCreateDevice(JobControlRecord* jcr,
                            DeviceResource* device_resource)
{
  Dmsg1(400, "max_block_size in device_resource res is %u\n",
        device_resource->max_block_size);

  // If no device type specified, try to guess
  if (device_resource->dev_type == DeviceType::B_UNKNOWN_DEV) {
    struct stat statp;
    // Check that device is available
    if (stat(device_resource->archive_device_string, &statp) < 0) {
      BErrNo be;
      Jmsg2(jcr, M_ERROR, 0, _("Unable to stat device %s: ERR=%s\n"),
            device_resource->archive_device_string, be.bstrerror());
      return nullptr;
    }
    if (S_ISDIR(statp.st_mode)) {
      device_resource->dev_type = DeviceType::B_FILE_DEV;
    } else if (S_ISCHR(statp.st_mode)) {
      device_resource->dev_type = DeviceType::B_TAPE_DEV;
    } else if (S_ISFIFO(statp.st_mode)) {
      device_resource->dev_type = DeviceType::B_FIFO_DEV;
    } else if (!BitIsSet(CAP_REQMOUNT, device_resource->cap_bits)) {
      Jmsg2(jcr, M_ERROR, 0,
            _("%s is an unknown device type. Must be tape or directory, "
              "st_mode=%04o\n"),
            device_resource->archive_device_string, (statp.st_mode & ~S_IFMT));
      return nullptr;
    }
  }

  Device* dev = nullptr;

  switch (device_resource->dev_type) {
    /*
     * When using dynamic loading use the InitBackendDevice() function
     * for any type of device not being of the type file.
     */
#ifndef HAVE_DYNAMIC_SD_BACKENDS
#  ifdef HAVE_GFAPI
    case DeviceType::B_GFAPI_DEV:
      dev = new gfapi_device;
      break;
#  endif
#  ifdef HAVE_BAREOSSD_DROPLET_DEVICE
    case DeviceType::B_DROPLET_DEV:
      dev = new DropletDevice;
      break;
#  endif
#  ifdef HAVE_RADOS
    case DeviceType::B_RADOS_DEV:
      dev = new rados_device;
      break;
#  endif
#  ifdef HAVE_CEPHFS
    case DeviceType::B_CEPHFS_DEV:
      dev = new cephfs_device;
      break;
#  endif
#  ifdef HAVE_WIN32
    case DeviceType::B_TAPE_DEV:
      dev = new win32_tape_device;
      break;
    case DeviceType::B_FIFO_DEV:
      dev = new win32_fifo_device;
      break;
#  else
    case DeviceType::B_TAPE_DEV:
      dev = new unix_tape_device;
      break;
    case DeviceType::B_FIFO_DEV:
      dev = new unix_fifo_device;
      break;
#  endif
#endif /* HAVE_DYNAMIC_SD_BACKENDS */
#ifdef HAVE_WIN32
    case DeviceType::B_FILE_DEV:
      dev = new win32_file_device;
      break;
#else
    case DeviceType::B_FILE_DEV:
      dev = new unix_file_device;
      break;
#endif
    default:
#ifdef HAVE_DYNAMIC_SD_BACKENDS
      dev = InitBackendDevice(jcr, device_resource->dev_type);
      if (!dev) {
        try {
          Jmsg2(jcr, M_ERROR, 0,
                _("Initialization of dynamic %s device \"%s\" with archive "
                  "device \"%s\" failed. Backend "
                  "library might be missing or backend directory incorrect.\n"),
                device_type_to_name_mapping.at(device_resource->dev_type),
                device_resource->resource_name_,
                device_resource->archive_device_string);
        } catch (const std::out_of_range&) {
          // device_resource->dev_type could exceed limits of map
        }
        return nullptr;
      }
#endif
      break;
  }

  if (!dev) {
    Jmsg2(jcr, M_ERROR, 0, _("%s has an unknown device type %d\n"),
          device_resource->archive_device_string, device_resource->dev_type);
    return nullptr;
  }
  dev->InvalidateSlotNumber(); /* unknown */

  // Copy user supplied device parameters from Resource
  dev->archive_device_string
      = GetMemory(strlen(device_resource->archive_device_string) + 1);
  PmStrcpy(dev->archive_device_string, device_resource->archive_device_string);
  if (device_resource->device_options) {
    dev->dev_options = GetMemory(strlen(device_resource->device_options) + 1);
    PmStrcpy(dev->dev_options, device_resource->device_options);
  }
  dev->prt_name = GetMemory(strlen(device_resource->archive_device_string)
                            + strlen(device_resource->resource_name_) + 20);


  Mmsg(dev->prt_name, "\"%s\" (%s)", device_resource->resource_name_,
       device_resource->archive_device_string);
  Dmsg1(400, "Allocate dev=%s\n", dev->print_name());
  CopySetBits(CAP_MAX, device_resource->cap_bits, dev->capabilities);


  dev->min_block_size = device_resource->min_block_size;
  dev->max_block_size = device_resource->max_block_size;
  dev->max_volume_size = device_resource->max_volume_size;
  dev->max_file_size = device_resource->max_file_size;
  dev->max_concurrent_jobs = device_resource->max_concurrent_jobs;
  dev->volume_capacity = device_resource->volume_capacity;
  dev->max_rewind_wait = device_resource->max_rewind_wait;
  dev->max_open_wait = device_resource->max_open_wait;
  dev->max_open_vols = device_resource->max_open_vols;
  dev->vol_poll_interval = device_resource->vol_poll_interval;
  dev->max_spool_size = device_resource->max_spool_size;
  dev->drive = device_resource->drive;
  dev->drive_index = device_resource->drive_index;
  dev->autoselect = device_resource->autoselect;
  dev->norewindonclose = device_resource->norewindonclose;
  dev->dev_type = device_resource->dev_type;
  dev->device_resource = device_resource;

  device_resource->dev = dev;

  if (dev->vol_poll_interval && dev->vol_poll_interval < 60) {
    dev->vol_poll_interval = 60;
  }

  if (dev->IsFifo()) { dev->SetCap(CAP_STREAM); }

  /*
   * If the device requires mount :
   * - Check that the mount point is available
   * - Check that (un)mount commands are defined
   */
  if (dev->IsFile() && dev->RequiresMount()) {
    struct stat statp;
    if (!device_resource->mount_point
        || stat(device_resource->mount_point, &statp) < 0) {
      BErrNo be;
      dev->dev_errno = errno;
      Jmsg2(jcr, M_ERROR_TERM, 0, _("Unable to stat mount point %s: ERR=%s\n"),
            device_resource->mount_point, be.bstrerror());
    }

    if (!device_resource->mount_command || !device_resource->unmount_command) {
      Jmsg0(jcr, M_ERROR_TERM, 0,
            _("Mount and unmount commands must defined for a device which "
              "requires mount.\n"));
    }
  }

  if (dev->min_block_size
      > (dev->max_block_size == 0 ? DEFAULT_BLOCK_SIZE : dev->max_block_size)) {
    Jmsg(jcr, M_ERROR_TERM, 0, _("Min block size > max on device %s\n"),
         dev->print_name());
  }
  if (dev->max_block_size > MAX_BLOCK_LENGTH) {
    Jmsg3(jcr, M_ERROR, 0,
          _("Block size %u on device %s is too large, using default %u\n"),
          dev->max_block_size, dev->print_name(), DEFAULT_BLOCK_SIZE);
    dev->max_block_size = 0;
  }
  if (dev->max_block_size % TAPE_BSIZE != 0) {
    Jmsg3(jcr, M_WARNING, 0,
          _("Max block size %u not multiple of device %s block size=%d.\n"),
          dev->max_block_size, dev->print_name(), TAPE_BSIZE);
  }
  if (dev->max_volume_size != 0
      && dev->max_volume_size < (dev->max_block_size << 4)) {
    Jmsg(jcr, M_ERROR_TERM, 0,
         _("Max Vol Size < 8 * Max Block Size for device %s\n"),
         dev->print_name());
  }

  dev->errmsg = GetPoolMemory(PM_EMSG);
  *dev->errmsg = 0;

  int errstat;

  if ((errstat = dev->InitMutex()) != 0) {
    BErrNo be;
    dev->dev_errno = errstat;
    Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"),
          be.bstrerror(errstat));
    Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
  }

  if ((errstat = pthread_cond_init(&dev->wait, NULL)) != 0) {
    BErrNo be;
    dev->dev_errno = errstat;
    Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"),
          be.bstrerror(errstat));
    Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
  }

  if ((errstat = pthread_cond_init(&dev->wait_next_vol, NULL)) != 0) {
    BErrNo be;
    dev->dev_errno = errstat;
    Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"),
          be.bstrerror(errstat));
    Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
  }

  if ((errstat = pthread_mutex_init(&dev->spool_mutex, NULL)) != 0) {
    BErrNo be;
    dev->dev_errno = errstat;
    Mmsg1(dev->errmsg, _("Unable to init spool mutex: ERR=%s\n"),
          be.bstrerror(errstat));
    Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
  }

  if ((errstat = dev->InitAcquireMutex()) != 0) {
    BErrNo be;
    dev->dev_errno = errstat;
    Mmsg1(dev->errmsg, _("Unable to init acquire mutex: ERR=%s\n"),
          be.bstrerror(errstat));
    Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
  }

  if ((errstat = dev->InitReadAcquireMutex()) != 0) {
    BErrNo be;
    dev->dev_errno = errstat;
    Mmsg1(dev->errmsg, _("Unable to init read acquire mutex: ERR=%s\n"),
          be.bstrerror(errstat));
    Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
  }

  dev->ClearOpened();
  dev->attached_dcrs.clear();
  Dmsg2(100, "FactoryCreateDevice: tape=%d archive_device_string=%s\n",
        dev->IsTape(), dev->archive_device_string);
  dev->initiated = true;
  Dmsg3(100, "dev=%s dev_max_bs=%u max_bs=%u\n", dev->archive_device_string,
        dev->device_resource->max_block_size, dev->max_block_size);

  return dev;
}

// This routine initializes the device wait timers
void InitDeviceWaitTimers(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  JobControlRecord* jcr = dcr->jcr;

  /* ******FIXME******* put these on config variables */
  dev->min_wait = 60 * 60;
  dev->max_wait = 24 * 60 * 60;
  dev->max_num_wait = 9; /* 5 waits =~ 1 day, then 1 day at a time */
  dev->wait_sec = dev->min_wait;
  dev->rem_wait_sec = dev->wait_sec;
  dev->num_wait = 0;
  dev->poll = false;

  jcr->impl->device_wait_times.min_wait = 60 * 60;
  jcr->impl->device_wait_times.max_wait = 24 * 60 * 60;
  jcr->impl->device_wait_times.max_num_wait
      = 9; /* 5 waits =~ 1 day, then 1 day at a time */
  jcr->impl->device_wait_times.wait_sec = jcr->impl->device_wait_times.min_wait;
  jcr->impl->device_wait_times.rem_wait_sec
      = jcr->impl->device_wait_times.wait_sec;
  jcr->impl->device_wait_times.num_wait = 0;
}

void InitJcrDeviceWaitTimers(JobControlRecord* jcr)
{
  /* ******FIXME******* put these on config variables */
  jcr->impl->device_wait_times.min_wait = 60 * 60;
  jcr->impl->device_wait_times.max_wait = 24 * 60 * 60;
  jcr->impl->device_wait_times.max_num_wait
      = 9; /* 5 waits =~ 1 day, then 1 day at a time */
  jcr->impl->device_wait_times.wait_sec = jcr->impl->device_wait_times.min_wait;
  jcr->impl->device_wait_times.rem_wait_sec
      = jcr->impl->device_wait_times.wait_sec;
  jcr->impl->device_wait_times.num_wait = 0;
}

/**
 * The dev timers are used for waiting on a particular device
 *
 * Returns: true if time doubled
 *          false if max time expired
 */
bool DoubleDevWaitTime(Device* dev)
{
  dev->wait_sec *= 2;                  /* Double wait time */
  if (dev->wait_sec > dev->max_wait) { /* But not longer than maxtime */
    dev->wait_sec = dev->max_wait;
  }
  dev->num_wait++;
  dev->rem_wait_sec = dev->wait_sec;
  if (dev->num_wait >= dev->max_num_wait) { return false; }
  return true;
}

/**
 * Set the block size of the device.
 * If the volume block size is zero, we set the max block size to what is
 * configured in the device resource i.e. dev->device->max_block_size.
 *
 * If dev->device->max_block_size is zero, do nothing and leave
 * dev->max_block_size as it is.
 */
void Device::SetBlocksizes(DeviceControlRecord* dcr)
{
  Device* dev = this;
  JobControlRecord* jcr = dcr->jcr;
  uint32_t max_bs;

  Dmsg4(100,
        "Device %s has dev->device->max_block_size of %u and "
        "dev->max_block_size of %u, dcr->VolMaxBlocksize is %u\n",
        dev->print_name(), dev->device_resource->max_block_size,
        dev->max_block_size, dcr->VolMaxBlocksize);

  if (dcr->VolMaxBlocksize == 0 && dev->device_resource->max_block_size != 0) {
    Dmsg2(100,
          "setting dev->max_block_size to "
          "dev->device_resource->max_block_size=%u "
          "on device %s because dcr->VolMaxBlocksize is 0\n",
          dev->device_resource->max_block_size, dev->print_name());
    dev->min_block_size = dev->device_resource->min_block_size;
    dev->max_block_size = dev->device_resource->max_block_size;
  } else if (dcr->VolMaxBlocksize != 0) {
    dev->min_block_size = dcr->VolMinBlocksize;
    dev->max_block_size = dcr->VolMaxBlocksize;
  }

  // Sanity check
  if (dev->max_block_size == 0) {
    max_bs = DEFAULT_BLOCK_SIZE;
  } else {
    max_bs = dev->max_block_size;
  }

  if (dev->min_block_size > max_bs) {
    Jmsg(jcr, M_ERROR_TERM, 0, _("Min block size > max on device %s\n"),
         dev->print_name());
  }

  if (dev->max_block_size > MAX_BLOCK_LENGTH) {
    Jmsg3(jcr, M_ERROR, 0,
          _("Block size %u on device %s is too large, using default %u\n"),
          dev->max_block_size, dev->print_name(), DEFAULT_BLOCK_SIZE);
    dev->max_block_size = 0;
  }

  if (dev->max_block_size % TAPE_BSIZE != 0) {
    Jmsg3(jcr, M_WARNING, 0,
          _("Max block size %u not multiple of device %s block size=%d.\n"),
          dev->max_block_size, dev->print_name(), TAPE_BSIZE);
  }

  if (dev->max_volume_size != 0
      && dev->max_volume_size < (dev->max_block_size << 4)) {
    Jmsg(jcr, M_ERROR_TERM, 0,
         _("Max Vol Size < 8 * Max Block Size for device %s\n"),
         dev->print_name());
  }

  Dmsg3(100, "set minblocksize to %d, maxblocksize to %d on device %s\n",
        dev->min_block_size, dev->max_block_size, dev->print_name());

  /*
   * If blocklen is not dev->max_block_size create a new block with the right
   * size. (as header is always dev->label_block_size which is preset with
   * DEFAULT_BLOCK_SIZE)
   */
  if (dcr->block) {
    if (dcr->block->buf_len != dev->max_block_size) {
      Dmsg2(100, "created new block of buf_len: %u on device %s\n",
            dev->max_block_size, dev->print_name());
      FreeBlock(dcr->block);
      dcr->block = new_block(dev);
      Dmsg2(100,
            "created new block of buf_len: %u on device %s, freeing block\n",
            dcr->block->buf_len, dev->print_name());
    }
  }
}

/**
 * Set the block size of the device to the label_block_size
 * to read labels as we want to always use that blocksize when
 * writing volume labels
 */
void Device::SetLabelBlocksize(DeviceControlRecord* dcr)
{
  Device* dev = this;
  Dmsg3(100,
        "setting minblocksize to %u, "
        "maxblocksize to label_block_size=%u, on device %s\n",
        dev->device_resource->label_block_size,
        dev->device_resource->label_block_size, dev->print_name());

  dev->min_block_size = dev->device_resource->label_block_size;
  dev->max_block_size = dev->device_resource->label_block_size;
  /*
   * If blocklen is not dev->max_block_size create a new block with the right
   * size (as header is always label_block_size)
   */
  if (dcr->block) {
    if (dcr->block->buf_len != dev->max_block_size) {
      FreeBlock(dcr->block);
      dcr->block = new_block(dev);
      Dmsg2(100, "created new block of buf_len: %u on device %s\n",
            dcr->block->buf_len, dev->print_name());
    }
  }
}

/**
 * Open the device with the operating system and
 * initialize buffer pointers.
 *
 * Returns: true on success
 *          false on error
 *
 * Note, for a tape, the VolName is the name we give to the
 * volume (not really used here), but for a file, the
 * VolName represents the name of the file to be created/opened.
 * In the case of a file, the full name is the device name
 * (archive_name) with the VolName concatenated.
 */
bool Device::open(DeviceControlRecord* dcr, DeviceMode omode)
{
  char preserve[ST_BYTES];

  ClearAllBits(ST_MAX, preserve);
  if (IsOpen()) {
    if (open_mode == omode) {
      return true;
    } else {
      d_close(fd);
      ClearOpened();
      Dmsg0(100, "Close fd for mode change.\n");

      if (BitIsSet(ST_LABEL, state)) SetBit(ST_LABEL, preserve);
      if (BitIsSet(ST_APPENDREADY, state)) SetBit(ST_APPENDREADY, preserve);
      if (BitIsSet(ST_READREADY, state)) SetBit(ST_READREADY, preserve);
    }
  }

  if (dcr) {
    dcr->setVolCatName(dcr->VolumeName);
    VolCatInfo = dcr->VolCatInfo; /* structure assign */
  }

  Dmsg4(100, "open dev: type=%d archive_device_string=%s vol=%s mode=%s\n",
        dev_type, print_name(), getVolCatName(), mode_to_str(omode));

  ClearBit(ST_LABEL, state);
  ClearBit(ST_APPENDREADY, state);
  ClearBit(ST_READREADY, state);
  ClearBit(ST_EOT, state);
  ClearBit(ST_WEOT, state);
  ClearBit(ST_EOF, state);

  label_type = B_BAREOS_LABEL;

  // We are about to open the device so let any plugin know we are.
  if (dcr && GeneratePluginEvent(dcr->jcr, bSdEventDeviceOpen, dcr) != bRC_OK) {
    Dmsg0(100, "open_dev: bSdEventDeviceOpen failed\n");
    return false;
  }

  Dmsg1(100, "call OpenDevice mode=%s\n", mode_to_str(omode));
  OpenDevice(dcr, omode);

  // Reset any important state info
  CopySetBits(ST_MAX, preserve, state);

  Dmsg2(100, "preserve=%08o fd=%d\n", preserve, fd);

  return fd >= 0;
}

void Device::set_mode(DeviceMode mode)
{
  switch (mode) {
    case DeviceMode::CREATE_READ_WRITE:
      oflags = O_CREAT | O_RDWR | O_BINARY;
      break;
    case DeviceMode::OPEN_READ_WRITE:
      oflags = O_RDWR | O_BINARY;
      break;
    case DeviceMode::OPEN_READ_ONLY:
      oflags = O_RDONLY | O_BINARY;
      break;
    case DeviceMode::OPEN_WRITE_ONLY:
      oflags = O_WRONLY | O_BINARY;
      break;
    default:
      Emsg0(M_ABORT, 0, _("Illegal mode given to open dev.\n"));
  }
}

// Open a device.
void Device::OpenDevice(DeviceControlRecord* dcr, DeviceMode omode)
{
  PoolMem archive_name(PM_FNAME);

  GetAutochangerLoadedSlot(dcr);

  // Handle opening of File Archive (not a tape)
  PmStrcpy(archive_name, archive_device_string);

  /*
   * If this is a virtual autochanger (i.e. changer_res != NULL) we simply use
   * the device name, assuming it has been appropriately setup by the
   * "autochanger".
   */
  if (!device_resource->changer_res
      || device_resource->changer_command[0] == 0) {
    if (VolCatInfo.VolCatName[0] == 0) {
      Mmsg(errmsg, _("Could not open file device %s. No Volume name given.\n"),
           print_name());
      ClearOpened();
      return;
    }

    if (!IsPathSeparator(
            archive_name.c_str()[strlen(archive_name.c_str()) - 1])) {
      PmStrcat(archive_name, "/");
    }
    PmStrcat(archive_name, getVolCatName());
  }

  mount(dcr, 1); /* do mount if required */

  open_mode = omode;
  set_mode(omode);

  Dmsg3(100, "open archive: mode=%s open(%s, %08o, 0640)\n", mode_to_str(omode),
        archive_name.c_str(), oflags);

  if ((fd = d_open(archive_name.c_str(), oflags, 0640)) < 0) {
    BErrNo be;
    dev_errno = errno;
    if (dev_errno == 0) {
      Mmsg2(errmsg, _("Could not open: %s\n"), archive_name.c_str());
    } else {
      Mmsg2(errmsg, _("Could not open: %s, ERR=%s\n"), archive_name.c_str(),
            be.bstrerror());
    }
    Dmsg1(100, "open failed: %s", errmsg);
  }

  if (fd >= 0) {
    dev_errno = 0;
    file = 0;
    file_addr = 0;
  }

  Dmsg1(100, "open dev: disk fd=%d opened\n", fd);
}

/**
 * Rewind the device.
 *
 * Returns: true  on success
 *          false on failure
 */
bool Device::rewind(DeviceControlRecord* dcr)
{
  Dmsg3(400, "rewind res=%d fd=%d %s\n", NumReserved(), fd, print_name());

  // Remove EOF/EOT flags
  ClearBit(ST_EOT, state);
  ClearBit(ST_EOF, state);
  ClearBit(ST_WEOT, state);

  block_num = file = 0;
  file_size = 0;
  file_addr = 0;

  if (fd < 0) { return false; }

  if (IsFifo() || IsVtl()) { return true; }

  if (d_lseek(dcr, (boffset_t)0, SEEK_SET) < 0) {
    BErrNo be;
    dev_errno = errno;
    Mmsg2(errmsg, _("lseek error on %s. ERR=%s"), print_name(), be.bstrerror());
    return false;
  }

  return true;
}

// Called to indicate that we have just read an EOF from the device.
void Device::SetAteof()
{
  SetEof();
  file_addr = 0;
  file_size = 0;
  block_num = 0;
}

/**
 * Called to indicate we are now at the end of the volume, and writing is not
 * possible.
 */
void Device::SetAteot()
{
  // Make volume effectively read-only
  SetBit(ST_EOF, state);
  SetBit(ST_EOT, state);
  SetBit(ST_WEOT, state);
  ClearAppend();
}

/**
 * Position device to end of medium (end of data)
 *
 * Returns: true  on succes
 *          false on error
 */
bool Device::eod(DeviceControlRecord* dcr)
{
  boffset_t pos;

  if (fd < 0) {
    dev_errno = EBADF;
    Mmsg1(errmsg, _("Bad call to eod. Device %s not open\n"), print_name());
    return false;
  }

  if (IsVtl()) { return true; }

  Dmsg0(100, "Enter eod\n");
  if (AtEot()) { return true; }

  ClearEof(); /* remove EOF flag */

  block_num = file = 0;
  file_size = 0;
  file_addr = 0;

  pos = d_lseek(dcr, (boffset_t)0, SEEK_END);
  Dmsg1(200, "====== Seek to %lld\n", pos);

  if (pos >= 0) {
    UpdatePos(dcr);
    SetEot();
    return true;
  }

  dev_errno = errno;
  BErrNo be;
  Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"), print_name(),
        be.bstrerror());
  Dmsg0(100, errmsg);

  return false;
}

/**
 * Set the position of the device.
 *
 * Returns: true  on succes
 *          false on error
 */
bool Device::UpdatePos(DeviceControlRecord* dcr)
{
  boffset_t pos;
  bool ok = true;

  if (!IsOpen()) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad device call. Device not open\n"));
    Emsg1(M_FATAL, 0, "%s", errmsg);
    return false;
  }

  if (IsFifo() || IsVtl()) { return true; }

  file = 0;
  file_addr = 0;
  pos = d_lseek(dcr, (boffset_t)0, SEEK_CUR);
  if (pos < 0) {
    BErrNo be;
    dev_errno = errno;
    Pmsg1(000, _("Seek error: ERR=%s\n"), be.bstrerror());
    Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"), print_name(),
          be.bstrerror());
    ok = false;
  } else {
    file_addr = pos;
    block_num = (uint32_t)pos;
    file = (uint32_t)(pos >> 32);
  }

  return ok;
}

char* Device::StatusDev()
{
  char* status;

  status = (char*)malloc(BMT_BYTES);
  ClearAllBits(BMT_MAX, status);

  if (BitIsSet(ST_EOT, state) || BitIsSet(ST_WEOT, state)) {
    SetBit(BMT_EOD, status);
    Pmsg0(-20, " EOD");
  }

  if (BitIsSet(ST_EOF, state)) {
    SetBit(BMT_EOF, status);
    Pmsg0(-20, " EOF");
  }

  SetBit(BMT_ONLINE, status);
  SetBit(BMT_BOT, status);

  return status;
}

bool Device::OfflineOrRewind()
{
  if (fd < 0) { return false; }
  if (HasCap(CAP_OFFLINEUNMOUNT)) {
    return offline();
  } else {
    /*
     * Note, this rewind probably should not be here (it wasn't
     * in prior versions of Bareos), but on FreeBSD, this is
     * needed in the case the tape was "frozen" due to an error
     * such as backspacing after writing and EOF. If it is not
     * done, all future references to the drive get and I/O error.
     */
    clrerror(MTREW);
    return rewind(NULL);
  }
}

void Device::SetSlotNumber(slot_number_t slot)
{
  slot_ = slot;
  if (vol) { vol->InvalidateSlotNumber(); }
}

void Device::InvalidateSlotNumber()
{
  slot_ = kInvalidSlotNumber;
  if (vol) { vol->SetSlotNumber(kInvalidSlotNumber); }
}

/**
 * Reposition the device to file, block
 *
 * Returns: false on failure
 *          true  on success
 */
bool Device::Reposition(DeviceControlRecord* dcr,
                        uint32_t rfile,
                        uint32_t rblock)
{
  if (!IsOpen()) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad call to Reposition. Device not open\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  if (IsFifo() || IsVtl()) { return true; }

  boffset_t pos = (((boffset_t)rfile) << 32) | rblock;
  Dmsg1(100, "===== lseek to %d\n", (int)pos);
  if (d_lseek(dcr, pos, SEEK_SET) == (boffset_t)-1) {
    BErrNo be;
    dev_errno = errno;
    Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"), print_name(),
          be.bstrerror());
    return false;
  }
  file = rfile;
  block_num = rblock;
  file_addr = pos;
  return true;
}

// Set to unload the current volume in the drive.
void Device::SetUnload()
{
  if (!unload_ && VolHdr.VolumeName[0] != 0) {
    unload_ = true;
    memcpy(UnloadVolName, VolHdr.VolumeName, sizeof(UnloadVolName));
  }
}

// Clear volume header.
void Device::ClearVolhdr()
{
  Dmsg1(100, "Clear volhdr vol=%s\n", VolHdr.VolumeName);
  VolHdr = Volume_Label{};
  setVolCatInfo(false);
}

// Close the device.
bool Device::close(DeviceControlRecord* dcr)
{
  // Called with dcr=nullptr on termination (from destructor) .

  bool retval = true;
  int status;
  Dmsg1(100, "close_dev %s\n", print_name());

  if (!IsOpen()) {
    Dmsg2(100, "device %s already closed vol=%s\n", print_name(),
          VolHdr.VolumeName);
    goto bail_out; /* already closed */
  }

  if (!norewindonclose) { OfflineOrRewind(); }

  switch (dev_type) {
    case DeviceType::B_VTL_DEV:
    case DeviceType::B_TAPE_DEV:
      UnlockDoor();
      // Fall through wanted
    default:
      status = d_close(fd);
      if (status < 0) {
        BErrNo be;

        Mmsg2(errmsg, _("Unable to close device %s. ERR=%s\n"), print_name(),
              be.bstrerror());
        dev_errno = errno;
        retval = false;
      }
      break;
  }

  unmount(dcr, 1); /* do unmount if required */

  // Clean up device packet so it can be reused.
  ClearOpened();

  ClearBit(ST_LABEL, state);
  ClearBit(ST_READREADY, state);
  ClearBit(ST_APPENDREADY, state);
  ClearBit(ST_EOT, state);
  ClearBit(ST_WEOT, state);
  ClearBit(ST_EOF, state);
  ClearBit(ST_MOUNTED, state);
  ClearBit(ST_MEDIA, state);
  ClearBit(ST_SHORT, state);

  label_type = B_BAREOS_LABEL;
  file = block_num = 0;
  file_size = 0;
  file_addr = 0;
  EndFile = EndBlock = 0;
  open_mode = DeviceMode::kUndefined;
  ClearVolhdr();
  VolCatInfo = VolumeCatalogInfo{};
  if (tid) {
    StopThreadTimer(tid);
    tid = 0;
  }

  // We closed the device so let any plugin know we did.
  if (dcr) { GeneratePluginEvent(dcr->jcr, bSdEventDeviceClose, dcr); }

bail_out:
  return retval;
}


/**
 * Mount the device.
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool Device::mount(DeviceControlRecord* dcr, int timeout)
{
  bool retval = true;
  Dmsg0(190, "Enter mount\n");

  if (IsMounted()) { return true; }

  retval = MountBackend(dcr, timeout);

  /*
   * When the mount command succeeded sent a
   * bSdEventDeviceMount plugin event so any plugin
   * that want to do something can do things now.
   */
  if (retval
      && GeneratePluginEvent(dcr->jcr, bSdEventDeviceMount, dcr) != bRC_OK) {
    retval = false;
  }

  // Mark the device mounted if we succeed.
  if (retval) { SetMounted(); }

  return retval;
}

/**
 * Unmount the device
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool Device::unmount(DeviceControlRecord* dcr, int timeout)
{
  bool retval = true;
  Dmsg0(100, "Enter unmount\n");

  // See if the device is mounted.
  if (!IsMounted()) { return true; }

  /*
   * Before running the unmount program sent a
   * bSdEventDeviceUnmount plugin event so any plugin
   * that want to do something can do things now.
   */
  if (dcr
      && GeneratePluginEvent(dcr->jcr, bSdEventDeviceUnmount, dcr) != bRC_OK) {
    retval = false;
    goto bail_out;
  }

  retval = UnmountBackend(dcr, timeout);

  // Mark the device unmounted if we succeed.
  if (retval) { ClearMounted(); }

bail_out:
  return retval;
}

/**
 * Edit codes into (Un)MountCommand
 *  %% = %
 *  %a = archive device name
 *  %m = mount point
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *
 */
void Device::EditMountCodes(PoolMem& omsg, const char* imsg)
{
  const char* p;
  const char* str;
  char add[20];

  PoolMem archive_name(PM_FNAME);

  omsg.c_str()[0] = 0;
  Dmsg1(800, "EditMountCodes: %s\n", imsg);
  for (p = imsg; *p; p++) {
    if (*p == '%') {
      switch (*++p) {
        case '%':
          str = "%";
          break;
        case 'a':
          str = archive_device_string;
          break;
        case 'm':
          str = device_resource->mount_point;
          break;
        default:
          add[0] = '%';
          add[1] = *p;
          add[2] = 0;
          str = add;
          break;
      }
    } else {
      add[0] = *p;
      add[1] = 0;
      str = add;
    }
    Dmsg1(1900, "add_str %s\n", str);
    PmStrcat(omsg, (char*)str);
    Dmsg1(1800, "omsg=%s\n", omsg.c_str());
  }
}

// Return the last timer interval (ms) or 0 if something goes wrong
btime_t Device::GetTimerCount()
{
  btime_t temp = last_timer;
  last_timer = GetCurrentBtime();
  temp = last_timer - temp; /* get elapsed time */

  return (temp > 0) ? temp : 0; /* take care of skewed clock */
}

// Read from device.
ssize_t Device::read(void* buf, size_t len)
{
  ssize_t read_len;

  GetTimerCount();

  read_len = d_read(fd, buf, len);

  last_tick = GetTimerCount();

  DevReadTime += last_tick;
  VolCatInfo.VolReadTime += last_tick;

  if (read_len > 0) { /* skip error */
    DevReadBytes += read_len;
  }

  return read_len;
}

// Write to device.
ssize_t Device::write(const void* buf, size_t len)
{
  ssize_t write_len;

  GetTimerCount();

  write_len = d_write(fd, buf, len);

  last_tick = GetTimerCount();

  DevWriteTime += last_tick;
  VolCatInfo.VolWriteTime += last_tick;

  if (write_len > 0) { /* skip error */
    DevWriteBytes += write_len;
  }

  return write_len;
}

// Return the resource name for the device
const char* Device::name() const { return device_resource->resource_name_; }

Device::~Device()
{
  Dmsg1(900, "term dev: %s\n", print_name());

  if (archive_device_string) {
    FreeMemory(archive_device_string);
    archive_device_string = nullptr;
  }
  if (dev_options) {
    FreeMemory(dev_options);
    dev_options = nullptr;
  }
  if (prt_name) {
    FreeMemory(prt_name);
    prt_name = nullptr;
  }
  if (errmsg) {
    FreePoolMemory(errmsg);
    errmsg = nullptr;
  }
  pthread_mutex_destroy(&mutex_);
  pthread_cond_destroy(&wait);
  pthread_cond_destroy(&wait_next_vol);
  pthread_mutex_destroy(&spool_mutex);
  // RwlDestroy(&lock);
  attached_dcrs.clear();

  if (device_resource && device_resource->dev == this) {
    device_resource->dev = nullptr;
  }
}

bool Device::CanStealLock() const
{
  return blocked_
         && (blocked_ == BST_UNMOUNTED || blocked_ == BST_WAITING_FOR_SYSOP
             || blocked_ == BST_UNMOUNTED_WAITING_FOR_SYSOP);
}

bool Device::waiting_for_mount() const
{
  return (blocked_ == BST_UNMOUNTED || blocked_ == BST_WAITING_FOR_SYSOP
          || blocked_ == BST_UNMOUNTED_WAITING_FOR_SYSOP);
}

bool Device::IsBlocked() const { return blocked_ != BST_NOT_BLOCKED; }

} /* namespace storagedaemon */
