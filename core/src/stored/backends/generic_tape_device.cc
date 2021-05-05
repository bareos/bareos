/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2021 Bareos GmbH & Co. KG

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
*/
/*
 * Kern Sibbald, MM
 *
 * Extracted from other source files Marco van Wieringen, June 2014
 */
/**
 * @file
 * Generic Tape API device abstraction.
 */

#include "include/bareos.h"
#include "stored/device_control_record.h"
#include "stored/stored.h"
#include "generic_tape_device.h"
#include "stored/autochanger.h"
#include "lib/scsi_lli.h"
#include "lib/berrno.h"
#include "lib/util.h"

#include <string>

namespace storagedaemon {

// Open a tape device
void generic_tape_device::OpenDevice(DeviceControlRecord* dcr, DeviceMode omode)
{
  file_size = 0;
  int timeout = max_open_wait;
#if !defined(HAVE_WIN32)
  mtop mt_com{};
  utime_t start_time = time(NULL);
#endif

  mount(dcr, 1); /* do mount if required */

  Dmsg0(100, "Open dev: device is tape\n");

  GetAutochangerLoadedSlot(dcr);

  open_mode = omode;
  set_mode(omode);

  if (timeout < 1) { timeout = 1; }
  errno = 0;
  Dmsg2(100, "Try open %s mode=%s\n", prt_name, mode_to_str(omode));
#if defined(HAVE_WIN32)
  // Windows Code
  if ((fd = d_open(archive_device_string, oflags, 0)) < 0) {
    dev_errno = errno;
  }
#else
  /*
   * UNIX Code
   *
   * If busy retry each second for max_open_wait seconds
   */
  for (;;) {
    // Try non-blocking open
    fd = d_open(archive_device_string, oflags | O_NONBLOCK, 0);
    if (fd < 0) {
      BErrNo be;
      dev_errno = errno;
      Dmsg5(100, "Open error on %s omode=%d oflags=%x errno=%d: ERR=%s\n",
            prt_name, omode, oflags, errno, be.bstrerror());
    } else {
      // Tape open, now rewind it
      Dmsg0(100, "Rewind after open\n");
      mt_com.mt_op = MTREW;
      mt_com.mt_count = 1;

      // Rewind only if dev is a tape
      if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) {
        BErrNo be;
        dev_errno = errno; /* set error status from rewind */
        d_close(fd);
        ClearOpened();
        Dmsg2(100, "Rewind error on %s close: ERR=%s\n", prt_name,
              be.bstrerror(dev_errno));
        // If we get busy, device is probably rewinding, try again
        if (dev_errno != EBUSY) { break; /* error -- no medium */ }
      } else {
        // Got fd and rewind worked, so we must have medium in drive
        d_close(fd);
        fd = d_open(archive_device_string, oflags, 0); /* open normally */
        if (fd < 0) {
          BErrNo be;
          dev_errno = errno;
          Dmsg5(100, "Open error on %s omode=%d oflags=%x errno=%d: ERR=%s\n",
                prt_name, omode, oflags, errno, be.bstrerror());
          break;
        }
        dev_errno = 0;
        LockDoor();
        SetOsDeviceParameters(dcr); /* do system dependent stuff */
        break;                      /* Successfully opened and rewound */
      }
    }
    Bmicrosleep(5, 0);

    // Exceed wait time ?
    if (time(NULL) - start_time >= max_open_wait) { break; /* yes, get out */ }
  }
#endif

  if (!IsOpen()) {
    BErrNo be;
    Mmsg2(errmsg, _("Unable to open device %s: ERR=%s\n"), prt_name,
          be.bstrerror(dev_errno));
    Dmsg1(100, "%s", errmsg);
  }

  Dmsg1(100, "open dev: tape %d opened\n", fd);
}

/**
 * Position device to end of medium (end of data)
 *
 * Returns: true  on succes
 *          false on error
 */
bool generic_tape_device::eod(DeviceControlRecord* dcr)
{
  mtop mt_com{};
  bool ok = true;
  int32_t os_file;

  if (fd < 0) {
    dev_errno = EBADF;
    Mmsg1(errmsg, _("Bad call to eod. Device %s not open\n"), prt_name);
    return false;
  }

#if defined(__digital__) && defined(__unix__)
  return fsf(VolCatInfo.VolCatFiles);
#endif

  Dmsg0(100, "Enter eod\n");
  if (AtEot()) { return true; }

  ClearEof(); /* remove EOF flag */
  block_num = file = 0;
  file_size = 0;
  file_addr = 0;

#ifdef MTEOM
  if (HasCap(CAP_FASTFSF) && !HasCap(CAP_EOM)) {
    Dmsg0(100, "Using FAST FSF for EOM\n");
    // If unknown position, rewind
    if (GetOsTapeFile() < 0) {
      if (!rewind(NULL)) {
        Dmsg0(100, "Rewind error\n");
        return false;
      }
    }
    mt_com.mt_op = MTFSF;
    // fix code to handle case that INT16_MAX is not large enough.
    mt_com.mt_count = INT16_MAX; /* use big positive number */
    if (mt_com.mt_count < 0) {
      mt_com.mt_count = INT16_MAX; /* brain damaged system */
    }
  }

  if (HasCap(CAP_MTIOCGET) && (HasCap(CAP_FASTFSF) || HasCap(CAP_EOM))) {
    if (HasCap(CAP_EOM)) {
      Dmsg0(100, "Using EOM for EOM\n");
      mt_com.mt_op = MTEOM;
      mt_com.mt_count = 1;
    }

    if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) {
      BErrNo be;
      clrerror(mt_com.mt_op);
      Dmsg1(50, "ioctl error: %s\n", be.bstrerror());
      UpdatePos(dcr);
      Mmsg2(errmsg, _("ioctl MTEOM error on %s. ERR=%s.\n"), prt_name,
            be.bstrerror());
      Dmsg0(100, errmsg);
      return false;
    }

    os_file = GetOsTapeFile();
    if (os_file < 0) {
      BErrNo be;
      clrerror(-1);
      Mmsg2(errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"), prt_name,
            be.bstrerror());
      Dmsg0(100, errmsg);
      return false;
    }
    Dmsg1(100, "EOD file=%d\n", os_file);
    SetAteof();
    file = os_file;
  } else {
#endif
    // Rewind then use FSF until EOT reached
    if (!rewind(NULL)) {
      Dmsg0(100, "Rewind error.\n");
      return false;
    }

    // Move file by file to the end of the tape
    int file_num;
    for (file_num = file; !AtEot(); file_num++) {
      Dmsg0(200, "eod: doing fsf 1\n");
      if (!fsf(1)) {
        Dmsg0(100, "fsf error.\n");
        return false;
      }
      // Avoid infinite loop by ensuring we advance.
      if (!AtEot() && file_num == (int)file) {
        Dmsg1(100, "fsf did not advance from file %d\n", file_num);
        SetAteof();
        os_file = GetOsTapeFile();
        if (os_file >= 0) {
          Dmsg2(100, "Adjust file from %d to %d\n", file_num, os_file);
          file = os_file;
        }
        break;
      }
    }
#ifdef MTEOM
  }
#endif

  /*
   * Some drivers leave us after second EOF when doing MTEOM, so we must backup
   * so that appending overwrites the second EOF.
   */
  if (HasCap(CAP_BSFATEOM)) {
    // Backup over EOF
    ok = bsf(1);

    // If BSF worked and fileno is known (not -1), set file
    os_file = GetOsTapeFile();
    if (os_file >= 0) {
      Dmsg2(100, "BSFATEOF adjust file from %d to %d\n", file, os_file);
      file = os_file;
    } else {
      file++; /* wing it -- not correct on all OSes */
    }
  } else {
    UpdatePos(dcr); /* update position */
  }
  Dmsg1(200, "EOD dev->file=%d\n", file);

  return ok;
}

// Called to indicate that we have just read an EOF from the device.
void generic_tape_device::SetAteof()
{
  SetEof();
  file++;
  file_addr = 0;
  file_size = 0;
  block_num = 0;
}

/**
 * Called to indicate we are now at the end of the volume, and writing is not
 * possible.
 */
void generic_tape_device::SetAteot()
{
  // Make volume effectively read-only
  SetBit(ST_EOF, state);
  SetBit(ST_EOT, state);
  SetBit(ST_WEOT, state);
  ClearAppend();
}

/**
 * Rewind device and put it offline
 *
 * Returns: true  on success
 *          false on failure
 */
bool generic_tape_device::offline()
{
  mtop mt_com{};

  // Remove EOF/EOT flags.
  ClearBit(ST_APPENDREADY, state);
  ClearBit(ST_READREADY, state);
  ClearBit(ST_EOT, state);
  ClearBit(ST_EOF, state);
  ClearBit(ST_WEOT, state);

  block_num = file = 0;
  file_size = 0;
  file_addr = 0;
  UnlockDoor();
  mt_com.mt_op = MTOFFL;
  mt_com.mt_count = 1;

  if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) {
    BErrNo be;
    dev_errno = errno;
    Mmsg2(errmsg, _("ioctl MTOFFL error on %s. ERR=%s.\n"), prt_name,
          be.bstrerror());
    return false;
  }
  Dmsg1(100, "Offlined device %s\n", prt_name);

  return true;
}

/**
 * Write an end of file on the device
 *
 * Returns: true on success
 *          false on failure
 */
bool generic_tape_device::weof(int num)
{
  mtop mt_com{};
  int status;
  Dmsg1(129, "=== weof_dev=%s\n", prt_name);

  if (!IsOpen()) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad call to weof_dev. Device not open\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }
  file_size = 0;

  if (!CanAppend()) {
    Mmsg0(errmsg, _("Attempt to WEOF on non-appendable Volume\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  ClearEof();
  ClearEot();
  mt_com.mt_op = MTWEOF;
  mt_com.mt_count = num;
  status = d_ioctl(fd, MTIOCTOP, (char*)&mt_com);
  if (status == 0) {
    block_num = 0;
    file += num;
    file_addr = 0;
  } else {
    BErrNo be;

    clrerror(mt_com.mt_op);
    if (status == -1) {
      Mmsg2(errmsg, _("ioctl MTWEOF error on %s. ERR=%s.\n"), prt_name,
            be.bstrerror());
    }
  }

  return status == 0;
}

/**
 * Foward space a file
 *
 * Returns: true  on success
 *          false on failure
 */
bool generic_tape_device::fsf(int num)
{
  int32_t os_file = 0;
  mtop mt_com{};
  int status = 0;

  if (!IsOpen()) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad call to fsf. Device not open\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  if (AtEot()) {
    dev_errno = 0;
    Mmsg1(errmsg, _("Device %s at End of Tape.\n"), prt_name);
    return false;
  }

  if (AtEof()) { Dmsg0(200, "ST_EOF set on entry to FSF\n"); }

  Dmsg0(100, "fsf\n");
  block_num = 0;

  /*
   * If Fast forward space file is set, then we
   * use MTFSF to forward space and MTIOCGET
   * to get the file position. We assume that
   * the SCSI driver will ensure that we do not
   * forward space past the end of the medium.
   */
  if (HasCap(CAP_FSF) && HasCap(CAP_MTIOCGET) && HasCap(CAP_FASTFSF)) {
    int my_errno = 0;
    mt_com.mt_op = MTFSF;
    mt_com.mt_count = num;
    status = d_ioctl(fd, MTIOCTOP, (char*)&mt_com);
    if (status < 0) {
      my_errno = errno; /* save errno */
    } else if ((os_file = GetOsTapeFile()) < 0) {
      my_errno = errno; /* save errno */
    }
    if (my_errno != 0) {
      BErrNo be;

      SetEot();
      Dmsg0(200, "Set ST_EOT\n");
      clrerror(mt_com.mt_op);
      Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"), prt_name,
            be.bstrerror(my_errno));
      Dmsg1(200, "%s", errmsg);
      return false;
    }

    Dmsg1(200, "fsf file=%d\n", os_file);
    SetAteof();
    file = os_file;
    return true;

    /*
     * Here if CAP_FSF is set, and virtually all drives
     * these days support it, we read a record, then forward
     * space one file. Using this procedure, which is slow,
     * is the only way we can be sure that we don't read
     * two consecutive EOF marks, which means End of Data.
     */
  } else if (HasCap(CAP_FSF)) {
    POOLMEM* rbuf;
    int rbuf_len;
    Dmsg0(200, "FSF has cap_fsf\n");
    if (max_block_size == 0) {
      rbuf_len = DEFAULT_BLOCK_SIZE;
    } else {
      rbuf_len = max_block_size;
    }
    rbuf = GetMemory(rbuf_len);
    mt_com.mt_op = MTFSF;
    mt_com.mt_count = 1;
    while (num-- && !AtEot()) {
      Dmsg0(100, "Doing read before fsf\n");
      if ((status = this->read((char*)rbuf, rbuf_len)) < 0) {
        if (errno == ENOMEM) { /* tape record exceeds buf len */
          status = rbuf_len;   /* This is OK */
          // On IBM drives, they return ENOSPC at EOM instead of EOF status
        } else if (AtEof() && errno == ENOSPC) {
          status = 0;
        } else if (HasCap(CAP_IOERRATEOM) && AtEof() && errno == EIO) {
          if (HasCap(CAP_IBMLINTAPE)) {
            Dmsg0(100, "Got EIO on read, checking lin_tape sense data\n");
            if (CheckScsiAtEod(fd)) {
              Dmsg0(100, "Sense data confirms it's EOD\n");
              status = 0;
            } else {
              Dmsg0(100,
                    "Not at EOD, might be a real error. Check sense trace from "
                    "lin_taped logs.\n");
              SetEot();
              clrerror(-1);
              Mmsg1(errmsg, _("read error on %s. ERR=Input/Output error.\n"),
                    prt_name);
              break;
            }
          } else {
            Dmsg0(100, "Got EIO on read, assuming that's due to EOD\n");
            status = 0;
          }
        } else {
          BErrNo be;

          SetEot();
          clrerror(-1);
          Dmsg2(100, "Set ST_EOT read errno=%d. ERR=%s\n", dev_errno,
                be.bstrerror());
          Mmsg2(errmsg, _("read error on %s. ERR=%s.\n"), prt_name,
                be.bstrerror());
          Dmsg1(100, "%s", errmsg);
          break;
        }
      }
      if (status == 0) { /* EOF */
        Dmsg1(100, "End of File mark from read. File=%d\n", file + 1);
        // Two reads of zero means end of tape
        if (AtEof()) {
          SetEot();
          Dmsg0(100, "Set ST_EOT\n");
          break;
        } else {
          SetAteof();
          continue;
        }
      } else { /* Got data */
        ClearEot();
        ClearEof();
      }

      Dmsg0(100, "Doing MTFSF\n");
      status = d_ioctl(fd, MTIOCTOP, (char*)&mt_com);
      if (status < 0) { /* error => EOT */
        BErrNo be;

        SetEot();
        Dmsg0(100, "Set ST_EOT\n");
        clrerror(mt_com.mt_op);
        Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"), prt_name,
              be.bstrerror());
        Dmsg0(100, "Got < 0 for MTFSF\n");
        Dmsg1(100, "%s", errmsg);
      } else {
        SetAteof();
      }
    }
    FreeMemory(rbuf);

    // No FSF, so use FSR to simulate it
  } else {
    Dmsg0(200, "Doing FSR for FSF\n");
    while (num-- && !AtEot()) { fsr(INT32_MAX); /* returns -1 on EOF or EOT */ }
    if (AtEot()) {
      dev_errno = 0;
      Mmsg1(errmsg, _("Device %s at End of Tape.\n"), prt_name);
      status = -1;
    } else {
      status = 0;
    }
  }
  Dmsg1(200, "Return %d from FSF\n", status);
  if (AtEof()) { Dmsg0(200, "ST_EOF set on exit FSF\n"); }
  if (AtEot()) { Dmsg0(200, "ST_EOT set on exit FSF\n"); }
  Dmsg1(200, "Return from FSF file=%d\n", file);

  return status == 0;
}

/**
 * Backward space a file
 *
 * Returns: false on failure
 *          true  on success
 */
bool generic_tape_device::bsf(int num)
{
  mtop mt_com{};
  int status;

  if (!IsOpen()) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad call to bsf. Device not open\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  Dmsg0(100, "bsf\n");
  ClearEot();
  ClearEof();
  file -= num;
  file_addr = 0;
  file_size = 0;
  mt_com.mt_op = MTBSF;
  mt_com.mt_count = num;

  status = d_ioctl(fd, MTIOCTOP, (char*)&mt_com);
  if (status < 0) {
    BErrNo be;

    clrerror(mt_com.mt_op);
    Mmsg2(errmsg, _("ioctl MTBSF error on %s. ERR=%s.\n"), prt_name,
          be.bstrerror());
  }

  return status == 0;
}

static inline bool DevGetOsPos(Device* dev, struct mtget* mt_stat)
{
  Dmsg0(100, "DevGetOsPos\n");
  return dev->HasCap(CAP_MTIOCGET)
         && dev->d_ioctl(dev->fd, MTIOCGET, (char*)mt_stat) == 0
         && mt_stat->mt_fileno >= 0;
}

/**
 * Foward space num records
 *
 * Returns: false on failure
 *          true  on success
 */
bool generic_tape_device::fsr(int num)
{
  mtop mt_com{};
  int status;

  if (!IsOpen()) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad call to fsr. Device not open\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  if (!HasCap(CAP_FSR)) {
    Mmsg1(errmsg, _("ioctl MTFSR not permitted on %s.\n"), prt_name);
    return false;
  }

  Dmsg1(100, "fsr %d\n", num);
  mt_com.mt_op = MTFSR;
  mt_com.mt_count = num;

  status = d_ioctl(fd, MTIOCTOP, (char*)&mt_com);
  if (status == 0) {
    ClearEof();
    block_num += num;
  } else {
    BErrNo be;
    struct mtget mt_stat;

    clrerror(mt_com.mt_op);
    Dmsg1(100, "FSF fail: ERR=%s\n", be.bstrerror());
    if (DevGetOsPos(this, &mt_stat)) {
      Dmsg4(100, "Adjust from %d:%d to %d:%d\n", file, block_num,
            mt_stat.mt_fileno, mt_stat.mt_blkno);
      file = mt_stat.mt_fileno;
      block_num = mt_stat.mt_blkno;
    } else {
      if (AtEof()) {
        SetEot();
      } else {
        SetAteof();
      }
    }
    Mmsg3(errmsg, _("ioctl MTFSR %d error on %s. ERR=%s.\n"), num, prt_name,
          be.bstrerror());
  }

  return status == 0;
}

/**
 * Backward space a record
 *
 * Returns:  false on failure
 *           true  on success
 */
bool generic_tape_device::bsr(int num)
{
  mtop mt_com{};
  int status;

  if (!IsOpen()) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad call to bsr_dev. Device not open\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  if (!HasCap(CAP_BSR)) {
    Mmsg1(errmsg, _("ioctl MTBSR not permitted on %s.\n"), prt_name);
    return false;
  }

  Dmsg0(100, "bsr_dev\n");
  block_num -= num;
  ClearEof();
  ClearEot();
  mt_com.mt_op = MTBSR;
  mt_com.mt_count = num;

  status = d_ioctl(fd, MTIOCTOP, (char*)&mt_com);
  if (status < 0) {
    BErrNo be;

    clrerror(mt_com.mt_op);
    Mmsg2(errmsg, _("ioctl MTBSR error on %s. ERR=%s.\n"), prt_name,
          be.bstrerror());
  }

  return status == 0;
}

/**
 * Load medium in device
 *
 * Returns: true  on success
 *          false on failure
 */
bool generic_tape_device::LoadDev()
{
#ifdef MTLOAD
  mtop mt_com{};
#endif

  if (fd < 0) {
    dev_errno = EBADF;
    Mmsg0(errmsg, _("Bad call to LoadDev. Device not open\n"));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

#ifndef MTLOAD
  Dmsg0(200, "stored: MTLOAD command not available\n");
  BErrNo be;
  dev_errno = ENOTTY; /* function not available */
  Mmsg2(errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"), prt_name,
        be.bstrerror());
  return false;
#else
  block_num = file = 0;
  file_size = 0;
  file_addr = 0;
  mt_com.mt_op = MTLOAD;
  mt_com.mt_count = 1;
  if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) {
    BErrNo be;
    dev_errno = errno;
    Mmsg2(errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"), prt_name,
          be.bstrerror());
    return false;
  }

  return true;
#endif
}

void generic_tape_device::LockDoor()
{
#ifdef MTLOCK
  mtop mt_com{};

  mt_com.mt_op = MTLOCK;
  mt_com.mt_count = 1;
  if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) { clrerror(mt_com.mt_op); }
#endif
}

void generic_tape_device::UnlockDoor()
{
#ifdef MTUNLOCK
  mtop mt_com{};

  mt_com.mt_op = MTUNLOCK;
  mt_com.mt_count = 1;
  if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) { clrerror(mt_com.mt_op); }
#endif
}

void generic_tape_device::OsClrError()
{
#if defined(MTIOCLRERR)
  // Found on Solaris
  if (d_ioctl(fd, MTIOCLRERR) < 0) { HandleError(MTIOCLRERR); }
  Dmsg0(200, "Did MTIOCLRERR\n");
#elif defined(MTIOCERRSTAT)
  // Typically on FreeBSD
  BErrNo be;
  union mterrstat mt_errstat;

  // Read and clear SCSI error status
  Dmsg2(200, "Doing MTIOCERRSTAT errno=%d ERR=%s\n", dev_errno,
        be.bstrerror(dev_errno));
  if (d_ioctl(fd, MTIOCERRSTAT, (char*)&mt_errstat) < 0) {
    HandleError(MTIOCERRSTAT);
  }
#elif defined(MTCSE)
  // Clear Subsystem Exception TRU64
  mtop mt_com{};

  // Clear any error condition on the tape
  mt_com.mt_op = MTCSE;
  mt_com.mt_count = 1;
  if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) { HandleError(mt_com.mt_op); }
  Dmsg0(200, "Did MTCSE\n");
#endif
}

void generic_tape_device::HandleError(int func)
{
  dev_errno = errno;
  if (errno == EIO) {
    VolCatInfo.VolCatErrors++;
  } else if (errno == ENOTTY
             || errno == ENOSYS) { /* Function not implemented */
    std::string msg;
    switch (func) {
      case -1:
        break; /* ignore message printed later */
      case MTWEOF:
        msg = "WTWEOF";
        ClearCap(CAP_EOF); /* turn off feature */
        break;
#ifdef MTEOM
      case MTEOM:
        msg = "WTEOM";
        ClearCap(CAP_EOM); /* turn off feature */
        break;
#endif
      case MTFSF:
        msg = "MTFSF";
        ClearCap(CAP_FSF); /* turn off feature */
        break;
      case MTBSF:
        msg = "MTBSF";
        ClearCap(CAP_BSF); /* turn off feature */
        break;
      case MTFSR:
        msg = "MTFSR";
        ClearCap(CAP_FSR); /* turn off feature */
        break;
      case MTBSR:
        msg = "MTBSR";
        ClearCap(CAP_BSR); /* turn off feature */
        break;
      case MTREW:
        msg = "MTREW";
        break;
#ifdef MTSETBLK
      case MTSETBLK:
        msg = "MTSETBLK";
        break;
#endif
#ifdef MTSETDRVBUFFER
      case MTSETDRVBUFFER:
        msg = "MTSETDRVBUFFER";
        break;
#endif
#ifdef MTRESET
      case MTRESET:
        msg = "MTRESET";
        break;
#endif
#ifdef MTSETBSIZ
      case MTSETBSIZ:
        msg = "MTSETBSIZ";
        break;
#endif
#ifdef MTSRSZ
      case MTSRSZ:
        msg = "MTSRSZ";
        break;
#endif
#ifdef MTLOAD
      case MTLOAD:
        msg = "MTLOAD";
        break;
#endif
#ifdef MTLOCK
      case MTLOCK:
        msg = "MTLOCK";
        break;
#endif
#ifdef MTUNLOCK
      case MTUNLOCK:
        msg = "MTUNLOCK";
        break;
#endif
      case MTOFFL:
        msg = "MTOFFL";
        break;
#ifdef MTIOCLRERR
      case MTIOCLRERR:
        msg = "MTIOCLRERR";
        break;
#endif
#ifdef MTIOCERRSTAT
      case MTIOCERRSTAT:
        msg = "MTIOCERRSTAT";
        break;
#endif
#ifdef MTCSE
      case MTCSE:
        msg = "MTCSE";
        break;
#endif
      default:
        char buf[100];
        Bsnprintf(buf, sizeof(buf), _("unknown func code %d"), func);
        msg = buf;
        break;
    }
    if (!msg.empty()) {
      dev_errno = ENOSYS;
      Mmsg1(errmsg, _("I/O function \"%s\" not supported on this device.\n"),
            msg.c_str());
      Emsg0(M_ERROR, 0, errmsg);
    }
  }
}

void generic_tape_device::clrerror(int func)
{
  HandleError(func);

  /*
   * Now we try different methods of clearing the error status on the drive
   * so that it is not locked for further operations.
   */

  // On some systems such as NetBSD, this clears all errors
  GetOsTapeFile();

  // OS specific clear function.
  OsClrError();
}

void generic_tape_device::SetOsDeviceParameters(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;

  if (bstrcmp(dev->archive_device_string, "/dev/null")) {
    return; /* no use trying to set /dev/null */
  }

#if defined(HAVE_LINUX_OS) || defined(HAVE_WIN32)
  mtop mt_com{};

  Dmsg0(100, "In SetOsDeviceParameters\n");
#  if defined(MTSETBLK)
  if (dev->min_block_size == dev->max_block_size
      && dev->min_block_size == 0) { /* variable block mode */
    mt_com.mt_op = MTSETBLK;
    mt_com.mt_count = 0;
    Dmsg0(100, "Set block size to zero\n");
    if (dev->d_ioctl(dev->fd, MTIOCTOP, (char*)&mt_com) < 0) {
      dev->clrerror(mt_com.mt_op);
    }
  }
#  endif
#  if defined(MTSETDRVBUFFER)
  if (getuid() == 0) { /* Only root can do this */
    mt_com.mt_op = MTSETDRVBUFFER;
    mt_com.mt_count = MT_ST_CLEARBOOLEANS;
    if (!dev->HasCap(CAP_TWOEOF)) { mt_com.mt_count |= MT_ST_TWO_FM; }
    if (dev->HasCap(CAP_EOM)) { mt_com.mt_count |= MT_ST_FAST_MTEOM; }
    Dmsg0(100, "MTSETDRVBUFFER\n");
    if (dev->d_ioctl(dev->fd, MTIOCTOP, (char*)&mt_com) < 0) {
      dev->clrerror(mt_com.mt_op);
    }
  }
#  endif
  return;
#endif

#ifdef HAVE_NETBSD_OS
  mtop mt_com{};
  if (dev->min_block_size == dev->max_block_size
      && dev->min_block_size == 0) { /* variable block mode */
    mt_com.mt_op = MTSETBSIZ;
    mt_com.mt_count = 0;
    if (dev->d_ioctl(dev->fd, MTIOCTOP, (char*)&mt_com) < 0) {
      dev->clrerror(mt_com.mt_op);
    }
    /* Get notified at logical end of tape */
    mt_com.mt_op = MTEWARN;
    mt_com.mt_count = 1;
    if (dev->d_ioctl(dev->fd, MTIOCTOP, (char*)&mt_com) < 0) {
      dev->clrerror(mt_com.mt_op);
    }
  }
  return;
#endif

#if HAVE_FREEBSD_OS || HAVE_OPENBSD_OS
  mtop mt_com{};
  if (dev->min_block_size == dev->max_block_size
      && dev->min_block_size == 0) { /* variable block mode */
    mt_com.mt_op = MTSETBSIZ;
    mt_com.mt_count = 0;
    if (dev->d_ioctl(dev->fd, MTIOCTOP, (char*)&mt_com) < 0) {
      dev->clrerror(mt_com.mt_op);
    }
  }
#  if defined(MTIOCSETEOTMODEL)
  uint32_t neof;
  if (dev->HasCap(CAP_TWOEOF)) {
    neof = 2;
  } else {
    neof = 1;
  }
  if (dev->d_ioctl(dev->fd, MTIOCSETEOTMODEL, (caddr_t)&neof) < 0) {
    BErrNo be;
    dev->dev_errno = errno; /* save errno */
    Mmsg2(dev->errmsg, _("Unable to set eotmodel on device %s: ERR=%s\n"),
          dev->print_name(), be.bstrerror(dev->dev_errno));
    Jmsg(dcr->jcr, M_FATAL, 0, dev->errmsg);
  }
#  endif
  return;
#endif

#ifdef HAVE_SUN_OS
  mtop mt_com{};
  if (dev->min_block_size == dev->max_block_size
      && dev->min_block_size == 0) { /* variable block mode */
    mt_com.mt_op = MTSRSZ;
    mt_com.mt_count = 0;
    if (dev->d_ioctl(dev->fd, MTIOCTOP, (char*)&mt_com) < 0) {
      dev->clrerror(mt_com.mt_op);
    }
  }
  return;
#endif
}

// Returns file position on tape or -1
int32_t generic_tape_device::GetOsTapeFile()
{
  struct mtget mt_stat;

  if (HasCap(CAP_MTIOCGET) && d_ioctl(fd, MTIOCGET, (char*)&mt_stat) == 0) {
    return mt_stat.mt_fileno;
  }

  return -1;
}

/**
 * Rewind the device.
 *
 * Returns: true  on success
 *          false on failure
 */
bool generic_tape_device::rewind(DeviceControlRecord* dcr)
{
  mtop mt_com{};
  unsigned int i;
  bool first = true;

  Dmsg3(400, "rewind res=%d fd=%d %s\n", NumReserved(), fd, prt_name);

  // Remove EOF/EOT flags.
  ClearBit(ST_EOT, state);
  ClearBit(ST_EOF, state);
  ClearBit(ST_WEOT, state);

  block_num = file = 0;
  file_size = 0;
  file_addr = 0;
  if (fd < 0) { return false; }

  mt_com.mt_op = MTREW;
  mt_com.mt_count = 1;

  /*
   * If we get an I/O error on rewind, it is probably because
   * the drive is actually busy. We loop for (about 5 minutes)
   * retrying every 5 seconds.
   */
  for (i = max_rewind_wait;; i -= 5) {
    if (d_ioctl(fd, MTIOCTOP, (char*)&mt_com) < 0) {
      BErrNo be;

      clrerror(mt_com.mt_op);
      if (i == max_rewind_wait) {
        Dmsg1(200, "Rewind error, %s. retrying ...\n", be.bstrerror());
      }
      /*
       * This is a gross hack, because if the user has the
       * device mounted (i.e. open), then uses mtx to load
       * a tape, the current open file descriptor is invalid.
       * So, we close the drive and re-open it.
       */
      if (first && dcr) {
        DeviceMode oo_mode = open_mode;
        d_close(fd);
        ClearOpened();
        open(dcr, oo_mode);
        if (fd < 0) { return false; }
        first = false;
        continue;
      }
#ifdef HAVE_SUN_OS
      if (dev_errno == EIO) {
        Mmsg1(errmsg, _("No tape loaded or drive offline on %s.\n"), prt_name);
        return false;
      }
#else
      if (dev_errno == EIO && i > 0) {
        Dmsg0(200, "Sleeping 5 seconds.\n");
        Bmicrosleep(5, 0);
        continue;
      }
#endif
      Mmsg2(errmsg, _("Rewind error on %s. ERR=%s.\n"), prt_name,
            be.bstrerror());
      return false;
    }
    break;
  }

  return true;
}

// (Un)mount the device (for tape devices)
static bool do_mount(DeviceControlRecord* dcr, int mount, int dotimeout)
{
  DeviceResource* device_resource = dcr->dev->device_resource;
  PoolMem ocmd(PM_FNAME);
  POOLMEM* results;
  char* icmd;
  int status, tries;
  BErrNo be;

  if (mount) {
    icmd = device_resource->mount_command;
  } else {
    icmd = device_resource->unmount_command;
  }

  dcr->dev->EditMountCodes(ocmd, icmd);
  Dmsg2(100, "do_mount: cmd=%s mounted=%d\n", ocmd.c_str(),
        dcr->dev->IsMounted());

  if (dotimeout) {
    /* Try at most 10 times to (un)mount the device. This should perhaps be
     * configurable. */
    tries = 10;
  } else {
    tries = 1;
  }
  results = GetMemory(4000);

  /* If busy retry each second */
  Dmsg1(100, "do_mount run_prog=%s\n", ocmd.c_str());
  while ((status = RunProgramFullOutput(ocmd.c_str(),
                                        dcr->dev->max_open_wait / 2, results))
         != 0) {
    if (tries-- > 0) { continue; }

    Dmsg5(100, "Device %s cannot be %smounted. stat=%d result=%s ERR=%s\n",
          dcr->dev->print_name(), (mount ? "" : "un"), status, results,
          be.bstrerror(status));
    Mmsg(dcr->dev->errmsg, _("Device %s cannot be %smounted. ERR=%s\n"),
         dcr->dev->print_name(), (mount ? "" : "un"), be.bstrerror(status));

    FreePoolMemory(results);
    Dmsg0(200, "============ mount=0\n");
    return false;
  }

  FreePoolMemory(results);
  Dmsg1(200, "============ mount=%d\n", mount);
  return true;
}

char* generic_tape_device::StatusDev()
{
  struct mtget mt_stat;
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

  SetBit(BMT_TAPE, status);
  Pmsg0(-20, _(" Bareos status:"));
  Pmsg2(-20, _(" file=%d block=%d\n"), file, block_num);
  if (d_ioctl(fd, MTIOCGET, (char*)&mt_stat) < 0) {
    BErrNo be;

    dev_errno = errno;
    Mmsg2(errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"), print_name(),
          be.bstrerror());
    free(status);
    return 0;
  }
  Pmsg0(-20, _(" Device status:"));

#if defined(HAVE_LINUX_OS)
  if (GMT_EOF(mt_stat.mt_gstat)) {
    SetBit(BMT_EOF, status);
    Pmsg0(-20, " EOF");
  }
  if (GMT_BOT(mt_stat.mt_gstat)) {
    SetBit(BMT_BOT, status);
    Pmsg0(-20, " BOT");
  }
  if (GMT_EOT(mt_stat.mt_gstat)) {
    SetBit(BMT_EOT, status);
    Pmsg0(-20, " EOT");
  }
  if (GMT_SM(mt_stat.mt_gstat)) {
    SetBit(BMT_SM, status);
    Pmsg0(-20, " SM");
  }
  if (GMT_EOD(mt_stat.mt_gstat)) {
    SetBit(BMT_EOD, status);
    Pmsg0(-20, " EOD");
  }
  if (GMT_WR_PROT(mt_stat.mt_gstat)) {
    SetBit(BMT_WR_PROT, status);
    Pmsg0(-20, " WR_PROT");
  }
  if (GMT_ONLINE(mt_stat.mt_gstat)) {
    SetBit(BMT_ONLINE, status);
    Pmsg0(-20, " ONLINE");
  }
  if (GMT_DR_OPEN(mt_stat.mt_gstat)) {
    SetBit(BMT_DR_OPEN, status);
    Pmsg0(-20, " DR_OPEN");
  }
  if (GMT_IM_REP_EN(mt_stat.mt_gstat)) {
    SetBit(BMT_IM_REP_EN, status);
    Pmsg0(-20, " IM_REP_EN");
  }
#elif defined(HAVE_WIN32)
  if (GMT_EOF(mt_stat.mt_gstat)) {
    SetBit(BMT_EOF, status);
    Pmsg0(-20, " EOF");
  }
  if (GMT_BOT(mt_stat.mt_gstat)) {
    SetBit(BMT_BOT, status);
    Pmsg0(-20, " BOT");
  }
  if (GMT_EOT(mt_stat.mt_gstat)) {
    SetBit(BMT_EOT, status);
    Pmsg0(-20, " EOT");
  }
  if (GMT_EOD(mt_stat.mt_gstat)) {
    SetBit(BMT_EOD, status);
    Pmsg0(-20, " EOD");
  }
  if (GMT_WR_PROT(mt_stat.mt_gstat)) {
    SetBit(BMT_WR_PROT, status);
    Pmsg0(-20, " WR_PROT");
  }
  if (GMT_ONLINE(mt_stat.mt_gstat)) {
    SetBit(BMT_ONLINE, status);
    Pmsg0(-20, " ONLINE");
  }
  if (GMT_DR_OPEN(mt_stat.mt_gstat)) {
    SetBit(BMT_DR_OPEN, status);
    Pmsg0(-20, " DR_OPEN");
  }
  if (GMT_IM_REP_EN(mt_stat.mt_gstat)) {
    SetBit(BMT_IM_REP_EN, status);
    Pmsg0(-20, " IM_REP_EN");
  }
#endif /* HAVE_LINUX_OS || HAVE_WIN32 */

  if (HasCap(CAP_MTIOCGET)) {
    Pmsg2(-20, _(" file=%d block=%d\n"), mt_stat.mt_fileno, mt_stat.mt_blkno);
  } else {
    Pmsg2(-20, _(" file=%d block=%d\n"), -1, -1);
  }

  return status;
}

/**
 * Set the position of the device.
 *
 * Returns: true  on succes
 *          false on error
 */
bool generic_tape_device::UpdatePos(DeviceControlRecord* dcr) { return true; }

/**
 * Reposition the device to file, block
 *
 * Returns: false on failure
 *          true  on success
 */
bool generic_tape_device::Reposition(DeviceControlRecord* dcr,
                                     uint32_t rfile,
                                     uint32_t rblock)
{
  Dmsg4(100, "Reposition from %u:%u to %u:%u\n", file, block_num, rfile,
        rblock);
  if (rfile < file) {
    Dmsg0(100, "Rewind\n");
    if (!rewind(NULL)) { return false; }
  }

  if (rfile > file) {
    Dmsg1(100, "fsf %d\n", rfile - file);
    if (!fsf(rfile - file)) {
      Dmsg1(100, "fsf failed! ERR=%s\n", bstrerror());
      return false;
    }
    Dmsg2(100, "wanted_file=%d at_file=%d\n", rfile, file);
  }

  if (rblock < block_num) {
    Dmsg2(100, "wanted_blk=%d at_blk=%d\n", rblock, block_num);
    Dmsg0(100, "bsf 1\n");
    bsf(1);
    Dmsg0(100, "fsf 1\n");
    fsf(1);
    Dmsg2(100, "wanted_blk=%d at_blk=%d\n", rblock, block_num);
  }

  if (HasCap(CAP_POSITIONBLOCKS) && rblock > block_num) {
    // Ignore errors as Bareos can read to the correct block.
    Dmsg1(100, "fsr %d\n", rblock - block_num);
    return fsr(rblock - block_num);
  } else {
    while (rblock > block_num) {
      if (DeviceControlRecord::ReadStatus::Ok
          != dcr->ReadBlockFromDev(NO_BLOCK_NUMBER_CHECK)) {
        BErrNo be;
        dev_errno = errno;
        Dmsg2(30, "Failed to find requested block on %s: ERR=%s", prt_name,
              be.bstrerror());
        return false;
      }
      Dmsg2(300, "moving forward wanted_blk=%d at_blk=%d\n", rblock, block_num);
    }
  }

  return true;
}

/**
 * Mount the device.
 *
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool generic_tape_device::MountBackend(DeviceControlRecord* dcr, int timeout)
{
  bool retval = true;

  if (RequiresMount() && device_resource->mount_command) {
    retval = do_mount(dcr, true, timeout);
  }

  return retval;
}

/**
 * Unmount the device
 *
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool generic_tape_device::UnmountBackend(DeviceControlRecord* dcr, int timeout)
{
  bool retval = true;

  if (RequiresMount() && device_resource->unmount_command) {
    retval = do_mount(dcr, false, timeout);
  }

  return retval;
}

int generic_tape_device::d_open(const char* pathname, int flags, int mode)
{
  return ::open(pathname, flags, mode);
}

ssize_t generic_tape_device::d_read(int fd, void* buffer, size_t count)
{
  return ::read(fd, buffer, count);
}

ssize_t generic_tape_device::d_write(int fd, const void* buffer, size_t count)
{
  return ::write(fd, buffer, count);
}

int generic_tape_device::d_close(int fd) { return ::close(fd); }

int generic_tape_device::d_ioctl(int fd, ioctl_req_t request, char* op)
{
  return -1;
}

boffset_t generic_tape_device::d_lseek(DeviceControlRecord* dcr,
                                       boffset_t offset,
                                       int whence)
{
  return -1;
}

bool generic_tape_device::d_truncate(DeviceControlRecord* dcr)
{
  // Maybe we should rewind and write and eof ????
  return true; /* We don't really truncate tapes */
}

} /* namespace storagedaemon  */
