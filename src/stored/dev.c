/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 *
 *   dev.c  -- low level operations on device (storage device)
 *
 *              Kern Sibbald, MM
 *
 *     NOTE!!!! None of these routines are reentrant. You must
 *        use dev->rLock() and dev->Unlock() at a higher level,
 *        or use the xxx_device() equivalents.  By moving the
 *        thread synchronization to a higher level, we permit
 *        the higher level routines to "seize" the device and
 *        to carry out operations without worrying about who
 *        set what lock (i.e. race conditions).
 *
 *     Note, this is the device dependent code, and may have
 *           to be modified for each system, but is meant to
 *           be as "generic" as possible.
 *
 *     The purpose of this code is to develop a SIMPLE Storage
 *     daemon. More complicated coding (double buffering, writer
 *     thread, ...) is left for a later version.
 *
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
 * is set all calls to write_block_to_device() call the fix_up
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


#include "bacula.h"
#include "stored.h"

#ifndef O_NONBLOCK 
#define O_NONBLOCK 0
#endif

/* Forward referenced functions */
void set_os_device_parameters(DCR *dcr);   
static bool dev_get_os_pos(DEVICE *dev, struct mtget *mt_stat);
static const char *mode_to_str(int mode);
static DEVICE *m_init_dev(JCR *jcr, DEVRES *device, bool new_init);

/*
 * Allocate and initialize the DEVICE structure
 * Note, if dev is non-NULL, it is already allocated,
 * thus we neither allocate it nor free it. This allows
 * the caller to put the packet in shared memory.
 *
 *  Note, for a tape, the device->device_name is the device name
 *     (e.g. /dev/nst0), and for a file, the device name
 *     is the directory in which the file will be placed.
 *
 */
DEVICE *
init_dev(JCR *jcr, DEVRES *device)
{
   DEVICE *dev = m_init_dev(jcr, device, false);
   return dev;
}

static DEVICE *
m_init_dev(JCR *jcr, DEVRES *device, bool new_init)
{
   struct stat statp;
   int errstat;
   DCR *dcr = NULL;
   DEVICE *dev;
   uint32_t max_bs;


   /* If no device type specified, try to guess */
   if (!device->dev_type) {
      /* Check that device is available */
      if (stat(device->device_name, &statp) < 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to stat device %s: ERR=%s\n"), 
            device->device_name, be.bstrerror());
         return NULL;
      }
      if (S_ISDIR(statp.st_mode)) {
         device->dev_type = B_FILE_DEV;
      } else if (S_ISCHR(statp.st_mode)) {
         device->dev_type = B_TAPE_DEV;
      } else if (S_ISFIFO(statp.st_mode)) {
         device->dev_type = B_FIFO_DEV;
#ifdef USE_VTAPE
      /* must set DeviceType = Vtape 
       * in normal mode, autodetection is disabled
       */
      } else if (S_ISREG(statp.st_mode)) { 
         device->dev_type = B_VTAPE_DEV;
#endif
      } else if (!(device->cap_bits & CAP_REQMOUNT)) {
         Jmsg2(jcr, M_ERROR, 0, _("%s is an unknown device type. Must be tape or directory\n"
               " or have RequiresMount=yes for DVD. st_mode=%x\n"),
            device->device_name, statp.st_mode);
         return NULL;
      } else {
         device->dev_type = B_DVD_DEV;
      }
   }
   switch (device->dev_type) {
   case B_DVD_DEV:
      Jmsg0(jcr, M_FATAL, 0, _("DVD support is now deprecated\n"));
      return NULL;
   case B_VTAPE_DEV:
      dev = New(vtape);
      break;
#ifdef USE_FTP
   case B_FTP_DEV:
      dev = New(ftp_device);
      break;
#endif
#ifdef HAVE_WIN32
/* TODO: defined in src/win32/stored/mtops.cpp */
   case B_TAPE_DEV:
      dev = New(win32_tape_device);
      break;
   case B_FILE_DEV:
      dev = New(win32_file_device);
      break;
#else
   case B_TAPE_DEV:
   case B_FILE_DEV:
   case B_FIFO_DEV:
      dev = New(DEVICE);
      break;
#endif
   default:
         return NULL;
   }
   dev->clear_slot();         /* unknown */

   /* Copy user supplied device parameters from Resource */
   dev->dev_name = get_memory(strlen(device->device_name)+1);
   pm_strcpy(dev->dev_name, device->device_name);
   dev->prt_name = get_memory(strlen(device->device_name) + strlen(device->hdr.name) + 20);
   /* We edit "Resource-name" (physical-name) */
   Mmsg(dev->prt_name, "\"%s\" (%s)", device->hdr.name, device->device_name);
   Dmsg1(400, "Allocate dev=%s\n", dev->print_name());
   dev->capabilities = device->cap_bits;
   dev->min_block_size = device->min_block_size;
   dev->max_block_size = device->max_block_size;
   dev->max_volume_size = device->max_volume_size;
   dev->max_file_size = device->max_file_size;
   dev->max_concurrent_jobs = device->max_concurrent_jobs;
   dev->volume_capacity = device->volume_capacity;
   dev->max_rewind_wait = device->max_rewind_wait;
   dev->max_open_wait = device->max_open_wait;
   dev->max_open_vols = device->max_open_vols;
   dev->vol_poll_interval = device->vol_poll_interval;
   dev->max_spool_size = device->max_spool_size;
   dev->drive_index = device->drive_index;
   dev->autoselect = device->autoselect;
   dev->dev_type = device->dev_type;
   dev->device = device;
   if (dev->is_tape()) { /* No parts on tapes */
      dev->max_part_size = 0;
   } else {
      dev->max_part_size = device->max_part_size;
   }
   /* Sanity check */
   if (dev->vol_poll_interval && dev->vol_poll_interval < 60) {
      dev->vol_poll_interval = 60;
   }
   device->dev = dev;

   if (dev->is_fifo()) {
      dev->capabilities |= CAP_STREAM; /* set stream device */
   }

   /* If the device requires mount :
    * - Check that the mount point is available 
    * - Check that (un)mount commands are defined
    */
   if (dev->is_file() && dev->requires_mount()) {
      if (!device->mount_point || stat(device->mount_point, &statp) < 0) {
         berrno be;
         dev->dev_errno = errno;
         Jmsg2(jcr, M_ERROR_TERM, 0, _("Unable to stat mount point %s: ERR=%s\n"), 
            device->mount_point, be.bstrerror());
      }
   
      if (!device->mount_command || !device->unmount_command) {
         Jmsg0(jcr, M_ERROR_TERM, 0, _("Mount and unmount commands must defined for a device which requires mount.\n"));
      }
   }

   /* Sanity check */
   if (dev->max_block_size == 0) {
      max_bs = DEFAULT_BLOCK_SIZE;
   } else {
      max_bs = dev->max_block_size;
   }
   if (dev->min_block_size > max_bs) {
      Jmsg(jcr, M_ERROR_TERM, 0, _("Min block size > max on device %s\n"), 
           dev->print_name());
   }
   if (dev->max_block_size > 4096000) {
      Jmsg3(jcr, M_ERROR, 0, _("Block size %u on device %s is too large, using default %u\n"),
         dev->max_block_size, dev->print_name(), DEFAULT_BLOCK_SIZE);
      dev->max_block_size = 0;
   }
   if (dev->max_block_size % TAPE_BSIZE != 0) {
      Jmsg3(jcr, M_WARNING, 0, _("Max block size %u not multiple of device %s block size=%d.\n"),
         dev->max_block_size, dev->print_name(), TAPE_BSIZE);
   }
   if (dev->max_volume_size != 0 && dev->max_volume_size < (dev->max_block_size << 4)) {
      Jmsg(jcr, M_ERROR_TERM, 0, _("Max Vol Size < 8 * Max Block Size for device %s\n"), 
           dev->print_name());
   }

   dev->errmsg = get_pool_memory(PM_EMSG);
   *dev->errmsg = 0;

   if ((errstat = dev->init_mutex()) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.bstrerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), be.bstrerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait_next_vol, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), be.bstrerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_mutex_init(&dev->spool_mutex, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init spool mutex: ERR=%s\n"), be.bstrerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = dev->init_acquire_mutex()) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init acquire mutex: ERR=%s\n"), be.bstrerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = dev->init_read_acquire_mutex()) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init read acquire mutex: ERR=%s\n"), be.bstrerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }

   dev->set_mutex_priorities();


#ifdef xxx
   if ((errstat = rwl_init(&dev->lock)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.bstrerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
#endif

   dev->clear_opened();
   dev->attached_dcrs = New(dlist(dcr, &dcr->dev_link));
   Dmsg2(100, "init_dev: tape=%d dev_name=%s\n", dev->is_tape(), dev->dev_name);
   dev->initiated = true;
   
   return dev;
}

/* default primitives are designed for file */
int DEVICE::d_open(const char *pathname, int flags)
{
   return ::open(pathname, flags);
}

int DEVICE::d_close(int fd)
{
   return ::close(fd);
}

int DEVICE::d_ioctl(int fd, ioctl_req_t request, char *mt_com)
{
   return ::ioctl(fd, request, mt_com);
}

ssize_t DEVICE::d_read(int fd, void *buffer, size_t count)
{
   return ::read(fd, buffer, count);
}

ssize_t DEVICE::d_write(int fd, const void *buffer, size_t count)
{
   return ::write(fd, buffer, count);
}

/*
 * Open the device with the operating system and
 * initialize buffer pointers.
 *
 * Returns:  true on success
 *           false on error
 *
 * Note, for a tape, the VolName is the name we give to the
 *    volume (not really used here), but for a file, the
 *    VolName represents the name of the file to be created/opened.
 *    In the case of a file, the full name is the device name
 *    (archive_name) with the VolName concatenated.
 */
bool
DEVICE::open(DCR *dcr, int omode)
{
   int preserve = 0;
   if (is_open()) {
      if (openmode == omode) {
         return true;
      } else {
         d_close(m_fd);
         clear_opened();
         Dmsg0(100, "Close fd for mode change.\n");
         preserve = state & (ST_LABEL|ST_APPEND|ST_READ);
      }
   }
   if (dcr) {
      dcr->setVolCatName(dcr->VolumeName);
      VolCatInfo = dcr->VolCatInfo;    /* structure assign */
   }

   Dmsg4(100, "open dev: type=%d dev_name=%s vol=%s mode=%s\n", dev_type,
         print_name(), getVolCatName(), mode_to_str(omode));
   state &= ~(ST_LABEL|ST_APPEND|ST_READ|ST_EOT|ST_WEOT|ST_EOF);
   label_type = B_BACULA_LABEL;

   if (is_tape() || is_fifo()) {
      open_tape_device(dcr, omode);
   } else if (is_ftp()) {
      open_device(dcr, omode);
   } else {
      Dmsg1(100, "call open_file_device mode=%s\n", mode_to_str(omode));
      open_file_device(dcr, omode);
   }
   state |= preserve;                 /* reset any important state info */
   Dmsg2(100, "preserve=0x%x fd=%d\n", preserve, m_fd);
   return m_fd >= 0;
}

void DEVICE::set_mode(int new_mode) 
{
   switch (new_mode) {
   case CREATE_READ_WRITE:
      mode = O_CREAT | O_RDWR | O_BINARY;
      break;
   case OPEN_READ_WRITE:
      mode = O_RDWR | O_BINARY;
      break;
   case OPEN_READ_ONLY:
      mode = O_RDONLY | O_BINARY;
      break;
   case OPEN_WRITE_ONLY:
      mode = O_WRONLY | O_BINARY;
      break;
   default:
      Emsg0(M_ABORT, 0, _("Illegal mode given to open dev.\n"));
   }
}

/*
 */
void DEVICE::open_tape_device(DCR *dcr, int omode) 
{
   file_size = 0;
   int timeout = max_open_wait;
#if !defined(HAVE_WIN32)
   struct mtop mt_com;
   utime_t start_time = time(NULL);
#endif

   mount(1);                          /* do mount if required */

   Dmsg0(100, "Open dev: device is tape\n");

   get_autochanger_loaded_slot(dcr);

   openmode = omode;
   set_mode(omode);

   if (timeout < 1) {
      timeout = 1;
   }
   errno = 0;
   if (is_fifo() && timeout) {
      /* Set open timer */
      tid = start_thread_timer(dcr->jcr, pthread_self(), timeout);
   }
   Dmsg2(100, "Try open %s mode=%s\n", print_name(), mode_to_str(omode));
#if defined(HAVE_WIN32)

   /*   Windows Code */
   if ((m_fd = d_open(dev_name, mode)) < 0) {
      dev_errno = errno;
   }

#else

   /*  UNIX  Code */
   /* If busy retry each second for max_open_wait seconds */
   for ( ;; ) {
      /* Try non-blocking open */
      m_fd = d_open(dev_name, mode+O_NONBLOCK);
      if (m_fd < 0) {
         berrno be;
         dev_errno = errno;
         Dmsg5(100, "Open error on %s omode=%d mode=%x errno=%d: ERR=%s\n", 
              print_name(), omode, mode, errno, be.bstrerror());
      } else {
         /* Tape open, now rewind it */
         Dmsg0(100, "Rewind after open\n");
         mt_com.mt_op = MTREW;
         mt_com.mt_count = 1;
         /* rewind only if dev is a tape */
         if (is_tape() && (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0)) {
            berrno be;
            dev_errno = errno;           /* set error status from rewind */
            d_close(m_fd);
            clear_opened();
            Dmsg2(100, "Rewind error on %s close: ERR=%s\n", print_name(),
                  be.bstrerror(dev_errno));
            /* If we get busy, device is probably rewinding, try again */
            if (dev_errno != EBUSY) {
               break;                    /* error -- no medium */
            }
         } else {
            /* Got fd and rewind worked, so we must have medium in drive */
            d_close(m_fd);
            m_fd = d_open(dev_name, mode);  /* open normally */
            if (m_fd < 0) {
               berrno be;
               dev_errno = errno;
               Dmsg5(100, "Open error on %s omode=%d mode=%x errno=%d: ERR=%s\n", 
                     print_name(), omode, mode, errno, be.bstrerror());
               break;
            }
            dev_errno = 0;
            lock_door();
            set_os_device_parameters(dcr);       /* do system dependent stuff */
            break;                               /* Successfully opened and rewound */
         }
      }
      bmicrosleep(5, 0);
      /* Exceed wait time ? */
      if (time(NULL) - start_time >= max_open_wait) {
         break;                       /* yes, get out */
      }
   }
#endif

   if (!is_open()) {
      berrno be;
      Mmsg2(errmsg, _("Unable to open device %s: ERR=%s\n"),
            print_name(), be.bstrerror(dev_errno));
      Dmsg1(100, "%s", errmsg);
   }

   /* Stop any open() timer we started */
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }
   Dmsg1(100, "open dev: tape %d opened\n", m_fd);
}

void DEVICE::open_device(DCR *dcr, int omode)
{
   /* do nothing waiting to split open_file/tape_device */
}

/*
 * Open a file device.
 */
void DEVICE::open_file_device(DCR *dcr, int omode) 
{
   POOL_MEM archive_name(PM_FNAME);

   get_autochanger_loaded_slot(dcr);

   /*
    * Handle opening of File Archive (not a tape)
    */     

   pm_strcpy(archive_name, dev_name);
   /*  
    * If this is a virtual autochanger (i.e. changer_res != NULL)
    *  we simply use the device name, assuming it has been
    *  appropriately setup by the "autochanger".
    */
   if (!device->changer_res || device->changer_command[0] == 0) {
      if (VolCatInfo.VolCatName[0] == 0) {
         Mmsg(errmsg, _("Could not open file device %s. No Volume name given.\n"),
            print_name());
         clear_opened();
         return;
      }

      if (!IsPathSeparator(archive_name.c_str()[strlen(archive_name.c_str())-1])) {
         pm_strcat(archive_name, "/");
      }
      pm_strcat(archive_name, getVolCatName());
   }

   mount(1);                          /* do mount if required */
         
   openmode = omode;
   set_mode(omode);
   /* If creating file, give 0640 permissions */
   Dmsg3(100, "open disk: mode=%s open(%s, 0x%x, 0640)\n", mode_to_str(omode), 
         archive_name.c_str(), mode);
   /* Use system open() */
   if ((m_fd = ::open(archive_name.c_str(), mode, 0640)) < 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("Could not open: %s, ERR=%s\n"), archive_name.c_str(), 
            be.bstrerror());
      Dmsg1(100, "open failed: %s", errmsg);
   }
   if (m_fd >= 0) {
      dev_errno = 0;
      file = 0;
      file_addr = 0;
   }
   Dmsg4(100, "open dev: disk fd=%d opened, part=%d/%d, part_size=%u\n", 
      m_fd, part, num_dvd_parts, part_size);
}

/*
 * Rewind the device.
 *  Returns: true  on success
 *           false on failure
 */
bool DEVICE::rewind(DCR *dcr)
{
   struct mtop mt_com;
   unsigned int i;
   bool first = true;

   Dmsg3(400, "rewind res=%d fd=%d %s\n", num_reserved(), m_fd, print_name());
   state &= ~(ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   if (m_fd < 0) {
      return false;
   }
   if (is_tape()) {
      mt_com.mt_op = MTREW;
      mt_com.mt_count = 1;
      /* If we get an I/O error on rewind, it is probably because
       * the drive is actually busy. We loop for (about 5 minutes)
       * retrying every 5 seconds.
       */
      for (i=max_rewind_wait; ; i -= 5) {
         if (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0) {
            berrno be;
            clrerror(MTREW);
            if (i == max_rewind_wait) {
               Dmsg1(200, "Rewind error, %s. retrying ...\n", be.bstrerror());
            }
            /*
             * This is a gross hack, because if the user has the
             *   device mounted (i.e. open), then uses mtx to load
             *   a tape, the current open file descriptor is invalid.
             *   So, we close the drive and re-open it.
             */
            if (first && dcr) {
               int open_mode = openmode;
               d_close(m_fd);
               clear_opened();
               open(dcr, open_mode);
               if (m_fd < 0) {
                  return false;
               }
               first = false;
               continue;
            }
#ifdef HAVE_SUN_OS
            if (dev_errno == EIO) {         
               Mmsg1(errmsg, _("No tape loaded or drive offline on %s.\n"), print_name());
               return false;
            }
#else
            if (dev_errno == EIO && i > 0) {
               Dmsg0(200, "Sleeping 5 seconds.\n");
               bmicrosleep(5, 0);
               continue;
            }
#endif
            Mmsg2(errmsg, _("Rewind error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
            return false;
         }
         break;
      }
   } else if (is_file()) {
      if (lseek(dcr, (boffset_t)0, SEEK_SET) < 0) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
         return false;
      }
   }
   return true;
}


/*
 * Called to indicate that we have just read an
 *  EOF from the device.
 */
void DEVICE::set_ateof() 
{ 
   set_eof();
   if (is_tape()) {
      file++;
   }
   file_addr = 0;
   file_size = 0;
   block_num = 0;
}

/*
 * Called to indicate we are now at the end of the tape, and
 *   writing is not possible.
 */
void DEVICE::set_ateot() 
{
   /* Make tape effectively read-only */
   state |= (ST_EOF|ST_EOT|ST_WEOT);
   clear_append();
}

/*
 * Position device to end of medium (end of data)
 *  Returns: true  on succes
 *           false on error
 */
bool DEVICE::eod(DCR *dcr)
{
   struct mtop mt_com;
   bool ok = true;
   boffset_t pos;
   int32_t os_file;

   if (m_fd < 0) {
      dev_errno = EBADF;
      Mmsg1(errmsg, _("Bad call to eod. Device %s not open\n"), print_name());
      return false;
   }

#if defined (__digital__) && defined (__unix__)
   return fsf(VolCatInfo.VolCatFiles);
#endif

   Dmsg0(100, "Enter eod\n");
   if (at_eot()) {
      return true;
   }
   clear_eof();         /* remove EOF flag */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   if (is_fifo()) {
      return true;
   }
   if (!is_tape()) {
      pos = lseek(dcr, (boffset_t)0, SEEK_END);
      Dmsg1(200, "====== Seek to %lld\n", pos);
      if (pos >= 0) {
         update_pos(dcr);
         set_eot();
         return true;
      }
      dev_errno = errno;
      berrno be;
      Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
             print_name(), be.bstrerror());
      Dmsg0(100, errmsg);
      return false;
   }
#ifdef MTEOM
   if (has_cap(CAP_FASTFSF) && !has_cap(CAP_EOM)) {
      Dmsg0(100,"Using FAST FSF for EOM\n");
      /* If unknown position, rewind */
      if (get_os_tape_file() < 0) {
        if (!rewind(NULL)) {
          Dmsg0(100, "Rewind error\n");
          return false;
        }
      }
      mt_com.mt_op = MTFSF;
      /*
       * ***FIXME*** fix code to handle case that INT16_MAX is
       *   not large enough.
       */
      mt_com.mt_count = INT16_MAX;    /* use big positive number */
      if (mt_com.mt_count < 0) {
         mt_com.mt_count = INT16_MAX; /* brain damaged system */
      }
   }

   if (has_cap(CAP_MTIOCGET) && (has_cap(CAP_FASTFSF) || has_cap(CAP_EOM))) {
      if (has_cap(CAP_EOM)) {
         Dmsg0(100,"Using EOM for EOM\n");
         mt_com.mt_op = MTEOM;
         mt_com.mt_count = 1;
      }

      if (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0) {
         berrno be;
         clrerror(mt_com.mt_op);
         Dmsg1(50, "ioctl error: %s\n", be.bstrerror());
         update_pos(dcr);
         Mmsg2(errmsg, _("ioctl MTEOM error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
         Dmsg0(100, errmsg);
         return false;
      }

      os_file = get_os_tape_file();
      if (os_file < 0) {
         berrno be;
         clrerror(-1);
         Mmsg2(errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
         Dmsg0(100, errmsg);
         return false;
      }
      Dmsg1(100, "EOD file=%d\n", os_file);
      set_ateof();
      file = os_file;
   } else {
#else
   {
#endif
      /*
       * Rewind then use FSF until EOT reached
       */
      if (!rewind(NULL)) {
         Dmsg0(100, "Rewind error.\n");
         return false;
      }
      /*
       * Move file by file to the end of the tape
       */
      int file_num;
      for (file_num=file; !at_eot(); file_num++) {
         Dmsg0(200, "eod: doing fsf 1\n");
         if (!fsf(1)) {
            Dmsg0(100, "fsf error.\n");
            return false;
         }
         /*
          * Avoid infinite loop by ensuring we advance.
          */
         if (!at_eot() && file_num == (int)file) {
            Dmsg1(100, "fsf did not advance from file %d\n", file_num);
            set_ateof();
            os_file = get_os_tape_file();
            if (os_file >= 0) {
               Dmsg2(100, "Adjust file from %d to %d\n", file_num, os_file);
               file = os_file;
            }       
            break;
         }
      }
   }
   /*
    * Some drivers leave us after second EOF when doing
    * MTEOM, so we must backup so that appending overwrites
    * the second EOF.
    */
   if (has_cap(CAP_BSFATEOM)) {
      /* Backup over EOF */
      ok = bsf(1);
      /* If BSF worked and fileno is known (not -1), set file */
      os_file = get_os_tape_file();
      if (os_file >= 0) {
         Dmsg2(100, "BSFATEOF adjust file from %d to %d\n", file , os_file);
         file = os_file;
      } else {
         file++;                       /* wing it -- not correct on all OSes */
      }
   } else {
      update_pos(dcr);                 /* update position */
   }
   Dmsg1(200, "EOD dev->file=%d\n", file);
   return ok;
}

/*
 * Set the position of the device -- only for files and DVD
 *   For other devices, there is no generic way to do it.
 *  Returns: true  on succes
 *           false on error
 */
bool DEVICE::update_pos(DCR *dcr)
{
   boffset_t pos;
   bool ok = true;

   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad device call. Device not open\n"));
      Emsg1(M_FATAL, 0, "%s", errmsg);
      return false;
   }

   if (is_file()) {
      file = 0;
      file_addr = 0;
      pos = lseek(dcr, (boffset_t)0, SEEK_CUR);
      if (pos < 0) {
         berrno be;
         dev_errno = errno;
         Pmsg1(000, _("Seek error: ERR=%s\n"), be.bstrerror());
         Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
         ok = false;
      } else {
         file_addr = pos;
         block_num = (uint32_t)pos;
         file = (uint32_t)(pos >> 32);
      }
   }
   return ok;
}

/*
 * Return the status of the device.  This was meant
 * to be a generic routine. Unfortunately, it doesn't
 * seem possible (at least I do not know how to do it
 * currently), which means that for the moment, this
 * routine has very little value.
 *
 *   Returns: status
 */
uint32_t status_dev(DEVICE *dev)
{
   struct mtget mt_stat;
   uint32_t stat = 0;

   if (dev->state & (ST_EOT | ST_WEOT)) {
      stat |= BMT_EOD;
      Pmsg0(-20, " EOD");
   }
   if (dev->state & ST_EOF) {
      stat |= BMT_EOF;
      Pmsg0(-20, " EOF");
   }
   if (dev->is_tape()) {
      stat |= BMT_TAPE;
      Pmsg0(-20,_(" Bacula status:"));
      Pmsg2(-20,_(" file=%d block=%d\n"), dev->file, dev->block_num);
      if (dev->d_ioctl(dev->fd(), MTIOCGET, (char *)&mt_stat) < 0) {
         berrno be;
         dev->dev_errno = errno;
         Mmsg2(dev->errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
            dev->print_name(), be.bstrerror());
         return 0;
      }
      Pmsg0(-20, _(" Device status:"));

#if defined(HAVE_LINUX_OS)
      if (GMT_EOF(mt_stat.mt_gstat)) {
         stat |= BMT_EOF;
         Pmsg0(-20, " EOF");
      }
      if (GMT_BOT(mt_stat.mt_gstat)) {
         stat |= BMT_BOT;
         Pmsg0(-20, " BOT");
      }
      if (GMT_EOT(mt_stat.mt_gstat)) {
         stat |= BMT_EOT;
         Pmsg0(-20, " EOT");
      }
      if (GMT_SM(mt_stat.mt_gstat)) {
         stat |= BMT_SM;
         Pmsg0(-20, " SM");
      }
      if (GMT_EOD(mt_stat.mt_gstat)) {
         stat |= BMT_EOD;
         Pmsg0(-20, " EOD");
      }
      if (GMT_WR_PROT(mt_stat.mt_gstat)) {
         stat |= BMT_WR_PROT;
         Pmsg0(-20, " WR_PROT");
      }
      if (GMT_ONLINE(mt_stat.mt_gstat)) {
         stat |= BMT_ONLINE;
         Pmsg0(-20, " ONLINE");
      }
      if (GMT_DR_OPEN(mt_stat.mt_gstat)) {
         stat |= BMT_DR_OPEN;
         Pmsg0(-20, " DR_OPEN");
      }
      if (GMT_IM_REP_EN(mt_stat.mt_gstat)) {
         stat |= BMT_IM_REP_EN;
         Pmsg0(-20, " IM_REP_EN");
      }
#elif defined(HAVE_WIN32)
      if (GMT_EOF(mt_stat.mt_gstat)) {
         stat |= BMT_EOF;
         Pmsg0(-20, " EOF");
      }
      if (GMT_BOT(mt_stat.mt_gstat)) {
         stat |= BMT_BOT;
         Pmsg0(-20, " BOT");
      }
      if (GMT_EOT(mt_stat.mt_gstat)) {
         stat |= BMT_EOT;
         Pmsg0(-20, " EOT");
      }
      if (GMT_EOD(mt_stat.mt_gstat)) {
         stat |= BMT_EOD;
         Pmsg0(-20, " EOD");
      }
      if (GMT_WR_PROT(mt_stat.mt_gstat)) {
         stat |= BMT_WR_PROT;
         Pmsg0(-20, " WR_PROT");
      }
      if (GMT_ONLINE(mt_stat.mt_gstat)) {
         stat |= BMT_ONLINE;
         Pmsg0(-20, " ONLINE");
      }
      if (GMT_DR_OPEN(mt_stat.mt_gstat)) {
         stat |= BMT_DR_OPEN;
         Pmsg0(-20, " DR_OPEN");
      }
      if (GMT_IM_REP_EN(mt_stat.mt_gstat)) {
         stat |= BMT_IM_REP_EN;
         Pmsg0(-20, " IM_REP_EN");
      }

#endif /* !SunOS && !OSF */
      if (dev->has_cap(CAP_MTIOCGET)) {
         Pmsg2(-20, _(" file=%d block=%d\n"), mt_stat.mt_fileno, mt_stat.mt_blkno);
      } else {
         Pmsg2(-20, _(" file=%d block=%d\n"), -1, -1);
      }
   } else {
      stat |= BMT_ONLINE | BMT_BOT;
   }
   return stat;
}


/*
 * Load medium in device
 *  Returns: true  on success
 *           false on failure
 */
bool load_dev(DEVICE *dev)
{
#ifdef MTLOAD
   struct mtop mt_com;
#endif

   if (dev->fd() < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad call to load_dev. Device not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }
   if (!(dev->is_tape())) {
      return true;
   }
#ifndef MTLOAD
   Dmsg0(200, "stored: MTLOAD command not available\n");
   berrno be;
   dev->dev_errno = ENOTTY;           /* function not available */
   Mmsg2(dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
         dev->print_name(), be.bstrerror());
   return false;
#else

   dev->block_num = dev->file = 0;
   dev->file_size = 0;
   dev->file_addr = 0;
   mt_com.mt_op = MTLOAD;
   mt_com.mt_count = 1;
   if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
      berrno be;
      dev->dev_errno = errno;
      Mmsg2(dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
         dev->print_name(), be.bstrerror());
      return false;
   }
   return true;
#endif
}

/*
 * Rewind device and put it offline
 *  Returns: true  on success
 *           false on failure
 */
bool DEVICE::offline()
{
   struct mtop mt_com;

   if (!is_tape()) {
      return true;                    /* device not open */
   }

   state &= ~(ST_APPEND|ST_READ|ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   unlock_door();
   mt_com.mt_op = MTOFFL;
   mt_com.mt_count = 1;
   if (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("ioctl MTOFFL error on %s. ERR=%s.\n"),
         print_name(), be.bstrerror());
      return false;
   }
   Dmsg1(100, "Offlined device %s\n", print_name());
   return true;
}

bool DEVICE::offline_or_rewind()
{
   if (m_fd < 0) {
      return false;
   }
   if (has_cap(CAP_OFFLINEUNMOUNT)) {
      return offline();
   } else {
   /*
    * Note, this rewind probably should not be here (it wasn't
    *  in prior versions of Bacula), but on FreeBSD, this is
    *  needed in the case the tape was "frozen" due to an error
    *  such as backspacing after writing and EOF. If it is not
    *  done, all future references to the drive get and I/O error.
    */
      clrerror(MTREW);
      return rewind(NULL);
   }
}

/*
 * Foward space a file
 *   Returns: true  on success
 *            false on failure
 */
bool DEVICE::fsf(int num)
{
   int32_t os_file = 0;
   struct mtop mt_com;
   int stat = 0;

   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to fsf. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      return true;
   }

   if (at_eot()) {
      dev_errno = 0;
      Mmsg1(errmsg, _("Device %s at End of Tape.\n"), print_name());
      return false;
   }
   if (at_eof()) {
      Dmsg0(200, "ST_EOF set on entry to FSF\n");
   }

   Dmsg0(100, "fsf\n");
   block_num = 0;
   /*
    * If Fast forward space file is set, then we
    *  use MTFSF to forward space and MTIOCGET
    *  to get the file position. We assume that
    *  the SCSI driver will ensure that we do not
    *  forward space past the end of the medium.
    */
   if (has_cap(CAP_FSF) && has_cap(CAP_MTIOCGET) && has_cap(CAP_FASTFSF)) {
      int my_errno = 0;
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = num;
      stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
      if (stat < 0) {
         my_errno = errno;            /* save errno */
      } else if ((os_file=get_os_tape_file()) < 0) {
         my_errno = errno;            /* save errno */
      }
      if (my_errno != 0) {
         berrno be;
         set_eot();
         Dmsg0(200, "Set ST_EOT\n");
         clrerror(MTFSF);
         Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror(my_errno));
         Dmsg1(200, "%s", errmsg);
         return false;
      }

      Dmsg1(200, "fsf file=%d\n", os_file);
      set_ateof();
      file = os_file;
      return true;

   /*
    * Here if CAP_FSF is set, and virtually all drives
    *  these days support it, we read a record, then forward
    *  space one file. Using this procedure, which is slow,
    *  is the only way we can be sure that we don't read
    *  two consecutive EOF marks, which means End of Data.
    */
   } else if (has_cap(CAP_FSF)) {
      POOLMEM *rbuf;
      int rbuf_len;
      Dmsg0(200, "FSF has cap_fsf\n");
      if (max_block_size == 0) {
         rbuf_len = DEFAULT_BLOCK_SIZE;
      } else {
         rbuf_len = max_block_size;
      }
      rbuf = get_memory(rbuf_len);
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = 1;
      while (num-- && !at_eot()) {
         Dmsg0(100, "Doing read before fsf\n");
         if ((stat = this->read((char *)rbuf, rbuf_len)) < 0) {
            if (errno == ENOMEM) {     /* tape record exceeds buf len */
               stat = rbuf_len;        /* This is OK */
            /*
             * On IBM drives, they return ENOSPC at EOM
             *  instead of EOF status
             */
            } else if (at_eof() && errno == ENOSPC) {
               stat = 0;
            } else {
               berrno be;
               set_eot();
               clrerror(-1);
               Dmsg2(100, "Set ST_EOT read errno=%d. ERR=%s\n", dev_errno,
                  be.bstrerror());
               Mmsg2(errmsg, _("read error on %s. ERR=%s.\n"),
                  print_name(), be.bstrerror());
               Dmsg1(100, "%s", errmsg);
               break;
            }
         }
         if (stat == 0) {                /* EOF */
            Dmsg1(100, "End of File mark from read. File=%d\n", file+1);
            /* Two reads of zero means end of tape */
            if (at_eof()) {
               set_eot();
               Dmsg0(100, "Set ST_EOT\n");
               break;
            } else {
               set_ateof();
               continue;
            }
         } else {                        /* Got data */
            clear_eot();
            clear_eof();
         }

         Dmsg0(100, "Doing MTFSF\n");
         stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
         if (stat < 0) {                 /* error => EOT */
            berrno be;
            set_eot();
            Dmsg0(100, "Set ST_EOT\n");
            clrerror(MTFSF);
            Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
            Dmsg0(100, "Got < 0 for MTFSF\n");
            Dmsg1(100, "%s", errmsg);
         } else {
            set_ateof();
         }
      }
      free_memory(rbuf);

   /*
    * No FSF, so use FSR to simulate it
    */
   } else {
      Dmsg0(200, "Doing FSR for FSF\n");
      while (num-- && !at_eot()) {
         fsr(INT32_MAX);    /* returns -1 on EOF or EOT */
      }
      if (at_eot()) {
         dev_errno = 0;
         Mmsg1(errmsg, _("Device %s at End of Tape.\n"), print_name());
         stat = -1;
      } else {
         stat = 0;
      }
   }
   Dmsg1(200, "Return %d from FSF\n", stat);
   if (at_eof()) {
      Dmsg0(200, "ST_EOF set on exit FSF\n");
   }
   if (at_eot()) {
      Dmsg0(200, "ST_EOT set on exit FSF\n");
   }
   Dmsg1(200, "Return from FSF file=%d\n", file);
   return stat == 0;
}

/*
 * Backward space a file
 *  Returns: false on failure
 *           true  on success
 */
bool DEVICE::bsf(int num)
{
   struct mtop mt_com;
   int stat;

   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to bsf. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      Mmsg1(errmsg, _("Device %s cannot BSF because it is not a tape.\n"),
         print_name());
      return false;
   }

   Dmsg0(100, "bsf\n");
   clear_eot();
   clear_eof();
   file -= num;
   file_addr = 0;
   file_size = 0;
   mt_com.mt_op = MTBSF;
   mt_com.mt_count = num;
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror(MTBSF);
      Mmsg2(errmsg, _("ioctl MTBSF error on %s. ERR=%s.\n"),
         print_name(), be.bstrerror());
   }
   return stat == 0;
}


/*
 * Foward space num records
 *  Returns: false on failure
 *           true  on success
 */
bool DEVICE::fsr(int num)
{
   struct mtop mt_com;
   int stat;

   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to fsr. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      return false;
   }

   if (!has_cap(CAP_FSR)) {
      Mmsg1(errmsg, _("ioctl MTFSR not permitted on %s.\n"), print_name());
      return false;
   }

   Dmsg1(100, "fsr %d\n", num);
   mt_com.mt_op = MTFSR;
   mt_com.mt_count = num;
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      clear_eof();
      block_num += num;
   } else {
      berrno be;
      struct mtget mt_stat;
      clrerror(MTFSR);
      Dmsg1(100, "FSF fail: ERR=%s\n", be.bstrerror());
      if (dev_get_os_pos(this, &mt_stat)) {
         Dmsg4(100, "Adjust from %d:%d to %d:%d\n", file,
            block_num, mt_stat.mt_fileno, mt_stat.mt_blkno);
         file = mt_stat.mt_fileno;
         block_num = mt_stat.mt_blkno;
      } else {
         if (at_eof()) {
            set_eot();
         } else {
            set_ateof();
         }
      }
      Mmsg3(errmsg, _("ioctl MTFSR %d error on %s. ERR=%s.\n"),
         num, print_name(), be.bstrerror());
   }
   return stat == 0;
}

/*
 * Backward space a record
 *   Returns:  false on failure
 *             true  on success
 */
bool DEVICE::bsr(int num)
{
   struct mtop mt_com;
   int stat;

   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to bsr_dev. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      return false;
   }

   if (!has_cap(CAP_BSR)) {
      Mmsg1(errmsg, _("ioctl MTBSR not permitted on %s.\n"), print_name());
      return false;
   }

   Dmsg0(100, "bsr_dev\n");
   block_num -= num;
   clear_eof();
   clear_eot();
   mt_com.mt_op = MTBSR;
   mt_com.mt_count = num;
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror(MTBSR);
      Mmsg2(errmsg, _("ioctl MTBSR error on %s. ERR=%s.\n"),
         print_name(), be.bstrerror());
   }
   return stat == 0;
}

void DEVICE::lock_door()
{
#ifdef MTLOCK
   struct mtop mt_com;
   if (!is_tape()) return;
   mt_com.mt_op = MTLOCK;
   mt_com.mt_count = 1;
   d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
#endif
}

void DEVICE::unlock_door()
{
#ifdef MTUNLOCK
   struct mtop mt_com;
   if (!is_tape()) return;
   mt_com.mt_op = MTUNLOCK;
   mt_com.mt_count = 1;
   d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
#endif
}

void DEVICE::set_slot(int32_t slot)
{ 
   m_slot = slot; 
   if (vol) vol->clear_slot();
}

void DEVICE::clear_slot()
{ 
   m_slot = -1; 
   if (vol) vol->set_slot(-1);
}

 

/*
 * Reposition the device to file, block
 * Returns: false on failure
 *          true  on success
 */
bool DEVICE::reposition(DCR *dcr, uint32_t rfile, uint32_t rblock)
{
   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to reposition. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      boffset_t pos = (((boffset_t)rfile)<<32) | rblock;
      Dmsg1(100, "===== lseek to %d\n", (int)pos);
      if (lseek(dcr, pos, SEEK_SET) == (boffset_t)-1) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
         return false;
      }
      file = rfile;
      block_num = rblock;
      file_addr = pos;
      return true;
   }

   /* After this point, we are tape only */
   Dmsg4(100, "reposition from %u:%u to %u:%u\n", file, block_num, rfile, rblock);
   if (rfile < file) {
      Dmsg0(100, "Rewind\n");
      if (!rewind(NULL)) {
         return false;
      }
   }
   if (rfile > file) {
      Dmsg1(100, "fsf %d\n", rfile-file);
      if (!fsf(rfile-file)) {
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
   if (has_cap(CAP_POSITIONBLOCKS) && rblock > block_num) {
      /* Ignore errors as Bacula can read to the correct block */
      Dmsg1(100, "fsr %d\n", rblock-block_num);
      return fsr(rblock-block_num);
   } else {
      while (rblock > block_num) {
         if (!dcr->read_block_from_dev(NO_BLOCK_NUMBER_CHECK)) {
            berrno be;
            dev_errno = errno;
            Dmsg2(30, "Failed to find requested block on %s: ERR=%s",
               print_name(), be.bstrerror());
            return false;
         }
         Dmsg2(300, "moving forward wanted_blk=%d at_blk=%d\n", rblock, block_num);
      }
   }
   return true;
}



/*
 * Write an end of file on the device
 *   Returns: true on success
 *            false on failure
 */
bool DEVICE::weof(int num)
{
   struct mtop mt_com;
   int stat;
   Dmsg1(129, "=== weof_dev=%s\n", print_name());
   
   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to weof_dev. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }
   file_size = 0;

   if (!is_tape()) {
      return true;
   }
   if (!can_append()) {
      Mmsg0(errmsg, _("Attempt to WEOF on non-appendable Volume\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }
      
   clear_eof();
   clear_eot();
   mt_com.mt_op = MTWEOF;
   mt_com.mt_count = num;
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      block_num = 0;
      file += num;
      file_addr = 0;
   } else {
      berrno be;
      clrerror(MTWEOF);
      if (stat == -1) {
         Mmsg2(errmsg, _("ioctl MTWEOF error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
       }
   }
   return stat == 0;
}


/*
 * If implemented in system, clear the tape
 * error status.
 */
void DEVICE::clrerror(int func)
{
   const char *msg = NULL;
   char buf[100];

   dev_errno = errno;         /* save errno */
   if (errno == EIO) {
      VolCatInfo.VolCatErrors++;
   }

   if (!is_tape()) {
      return;
   }

   if (errno == ENOTTY || errno == ENOSYS) { /* Function not implemented */
      switch (func) {
      case -1:
         break;                  /* ignore message printed later */
      case MTWEOF:
         msg = "WTWEOF";
         clear_cap(CAP_EOF);     /* turn off feature */
         break;
#ifdef MTEOM
      case MTEOM:
         msg = "WTEOM";
         clear_cap(CAP_EOM);     /* turn off feature */
         break;
#endif
      case MTFSF:
         msg = "MTFSF";
         clear_cap(CAP_FSF);     /* turn off feature */
         break;
      case MTBSF:
         msg = "MTBSF";
         clear_cap(CAP_BSF);     /* turn off feature */
         break;
      case MTFSR:
         msg = "MTFSR";
         clear_cap(CAP_FSR);     /* turn off feature */
         break;
      case MTBSR:
         msg = "MTBSR";
         clear_cap(CAP_BSR);     /* turn off feature */
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
#ifdef MTUNLOCK
      case MTUNLOCK:
         msg = "MTUNLOCK";
         break;
#endif
      case MTOFFL:
         msg = "MTOFFL";
         break;
      default:
         bsnprintf(buf, sizeof(buf), _("unknown func code %d"), func);
         msg = buf;
         break;
      }
      if (msg != NULL) {
         dev_errno = ENOSYS;
         Mmsg1(errmsg, _("I/O function \"%s\" not supported on this device.\n"), msg);
         Emsg0(M_ERROR, 0, errmsg);
      }
   }

   /*
    * Now we try different methods of clearing the error
    *  status on the drive so that it is not locked for
    *  further operations.
    */

   /* On some systems such as NetBSD, this clears all errors */
   get_os_tape_file();

/* Found on Solaris */
#ifdef MTIOCLRERR
{
   d_ioctl(m_fd, MTIOCLRERR);
   Dmsg0(200, "Did MTIOCLRERR\n");
}
#endif

/* Typically on FreeBSD */
#ifdef MTIOCERRSTAT
{
  berrno be;
   /* Read and clear SCSI error status */
   union mterrstat mt_errstat;
   Dmsg2(200, "Doing MTIOCERRSTAT errno=%d ERR=%s\n", dev_errno,
      be.bstrerror(dev_errno));
   d_ioctl(m_fd, MTIOCERRSTAT, (char *)&mt_errstat);
}
#endif

/* Clear Subsystem Exception TRU64 */
#ifdef MTCSE
{
   struct mtop mt_com;
   mt_com.mt_op = MTCSE;
   mt_com.mt_count = 1;
   /* Clear any error condition on the tape */
   d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   Dmsg0(200, "Did MTCSE\n");
}
#endif
}


/*
 * Set to unload the current volume in the drive
 */
void DEVICE::set_unload()
{
   if (!m_unload && VolHdr.VolumeName[0] != 0) {
       m_unload = true;
       memcpy(UnloadVolName, VolHdr.VolumeName, sizeof(UnloadVolName));
   }
}


/*
 * Clear volume header
 */
void DEVICE::clear_volhdr()
{
   Dmsg1(100, "Clear volhdr vol=%s\n", VolHdr.VolumeName);
   memset(&VolHdr, 0, sizeof(VolHdr));
   setVolCatInfo(false);
}


/*
 * Close the device
 */
void DEVICE::close()
{
   Dmsg1(100, "close_dev %s\n", print_name());
   offline_or_rewind();

   if (!is_open()) {
      Dmsg2(100, "device %s already closed vol=%s\n", print_name(),
         VolHdr.VolumeName);
      return;                         /* already closed */
   }

   switch (dev_type) {
   case B_VTL_DEV:
   case B_VTAPE_DEV:
   case B_TAPE_DEV:
      unlock_door();
      /* Fall through wanted */
   default:
      d_close(m_fd);
      break;
   }

   unmount(1);                        /* do unmount if required */

   /* Clean up device packet so it can be reused */
   clear_opened();
   /*
    * Be careful not to clear items needed by the DVD driver
    *    when it is closing a single part.
    */
   state &= ~(ST_LABEL|ST_READ|ST_APPEND|ST_EOT|ST_WEOT|ST_EOF|
              ST_MOUNTED|ST_MEDIA|ST_SHORT);
   label_type = B_BACULA_LABEL;
   file = block_num = 0;
   file_size = 0;
   file_addr = 0;
   EndFile = EndBlock = 0;
   openmode = 0;
   clear_volhdr();
   memset(&VolCatInfo, 0, sizeof(VolCatInfo));
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }
}

/*
 * This call closes the device, but it is used in DVD handling
 *  where we close one part and then open the next part. The
 *  difference between close_part() and close() is that close_part()
 *  saves the state information of the device (e.g. the Volume lable,
 *  the Volume Catalog record, ...  This permits opening and closing
 *  the Volume parts multiple times without losing track of what the    
 *  main Volume parameters are.
 */
void DEVICE::close_part(DCR * /*dcr*/)
{
   VOLUME_LABEL saveVolHdr;
   VOLUME_CAT_INFO saveVolCatInfo;     /* Volume Catalog Information */


   saveVolHdr = VolHdr;               /* structure assignment */
   saveVolCatInfo = VolCatInfo;       /* structure assignment */
   close();                           /* close current part */
   VolHdr = saveVolHdr;               /* structure assignment */
   VolCatInfo = saveVolCatInfo;       /* structure assignment */
}

boffset_t DEVICE::lseek(DCR *dcr, boffset_t offset, int whence)
{
   switch (dev_type) {
   case B_FILE_DEV:
#if defined(HAVE_WIN32)
      return ::_lseeki64(m_fd, (__int64)offset, whence);
#else
      return ::lseek(m_fd, offset, whence);
#endif
   }
   return -1;
}

/*
 * Truncate a volume.
 */
bool DEVICE::truncate(DCR *dcr) /* We need the DCR for DVD-writing */
{
   struct stat st;
   DEVICE *dev = this;

   Dmsg1(100, "truncate %s\n", print_name());
   switch (dev_type) {
   case B_VTL_DEV:
   case B_VTAPE_DEV:
   case B_TAPE_DEV:
      /* maybe we should rewind and write and eof ???? */
      return true;                    /* we don't really truncate tapes */
   case B_FILE_DEV:
      for ( ;; ) {
         if (ftruncate(dev->m_fd, 0) != 0) {
            berrno be;
            Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), 
                  print_name(), be.bstrerror());
            return false;
         }

         /*
          * Check for a successful ftruncate() and issue a work-around for devices 
          * (mostly cheap NAS) that don't support truncation. 
          * Workaround supplied by Martin Schmid as a solution to bug #1011.
          * 1. close file
          * 2. delete file
          * 3. open new file with same mode
          * 4. change ownership to original
          */

         if (fstat(dev->m_fd, &st) != 0) {
            berrno be;
            Mmsg2(errmsg, _("Unable to stat device %s. ERR=%s\n"), 
                  print_name(), be.bstrerror());
            return false;
         }
             
         if (st.st_size != 0) {             /* ftruncate() didn't work */
            POOL_MEM archive_name(PM_FNAME);
                   
            pm_strcpy(archive_name, dev_name);
            if (!IsPathSeparator(archive_name.c_str()[strlen(archive_name.c_str())-1])) {
               pm_strcat(archive_name, "/");
            }
            pm_strcat(archive_name, dcr->VolumeName);
                      
            Mmsg2(errmsg, _("Device %s doesn't support ftruncate(). Recreating file %s.\n"), 
                  print_name(), archive_name.c_str());

            /* Close file and blow it away */
            ::close(dev->m_fd);
            ::unlink(archive_name.c_str());
                      
            /* Recreate the file -- of course, empty */
            dev->set_mode(CREATE_READ_WRITE);
            if ((dev->m_fd = ::open(archive_name.c_str(), mode, st.st_mode)) < 0) {
               berrno be;
               dev_errno = errno;
               Mmsg2(errmsg, _("Could not reopen: %s, ERR=%s\n"), archive_name.c_str(), 
                     be.bstrerror());
               Dmsg1(100, "reopen failed: %s", errmsg);
               Emsg0(M_FATAL, 0, errmsg);
               return false;
            }
                      
            /* Reset proper owner */
            chown(archive_name.c_str(), st.st_uid, st.st_gid);  
         }
         break;
      }
      return true;
   }
   return false;
}

/*
 * Mount the device.
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool DEVICE::mount(int timeout) 
{
   Dmsg0(190, "Enter mount\n");

   if (is_mounted()) {
      return true;
   }

   switch (dev_type) {
   case B_VTL_DEV:
   case B_VTAPE_DEV:
   case B_TAPE_DEV:
      if (device->mount_command) {
         return do_tape_mount(1, timeout);
      }
      break;
   case B_FILE_DEV:
      if (requires_mount() && device->mount_command) {
         return do_file_mount(1, timeout);
      }
      break;
   default:
      break;
   }

   return true;
}

/*
 * Unmount the device
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool DEVICE::unmount(int timeout) 
{
   Dmsg0(100, "Enter unmount\n");

   if (!is_mounted()) {
      return true;
   }

   switch (dev_type) {
   case B_VTL_DEV:
   case B_VTAPE_DEV:
   case B_TAPE_DEV:
      if (device->unmount_command) {
         return do_tape_mount(0, timeout);
      }
      break;
   case B_FILE_DEV:
   case B_DVD_DEV:
      if (requires_mount() && device->unmount_command) {
         return do_file_mount(0, timeout);
      }
      break;
   default:
      break;
   }

   return true;
}

/*
 * (Un)mount the device (for tape devices)
 */
bool DEVICE::do_tape_mount(int mount, int dotimeout)
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   char *icmd;
   int status, tries;
   berrno be;

   Dsm_check(200);
   if (mount) {
      icmd = device->mount_command;
   } else {
      icmd = device->unmount_command;
   }

   edit_mount_codes(ocmd, icmd);

   Dmsg2(100, "do_tape_mount: cmd=%s mounted=%d\n", ocmd.c_str(), !!is_mounted());

   if (dotimeout) {
      /* Try at most 10 times to (un)mount the device. This should perhaps be configurable. */
      tries = 10;
   } else {
      tries = 1;
   }
   results = get_memory(4000);

   /* If busy retry each second */
   Dmsg1(100, "do_tape_mount run_prog=%s\n", ocmd.c_str());
   while ((status = run_program_full_output(ocmd.c_str(), max_open_wait/2, results)) != 0) {
      if (tries-- > 0) {
         continue;
      }

      Dmsg5(100, "Device %s cannot be %smounted. stat=%d result=%s ERR=%s\n", print_name(),
           (mount ? "" : "un"), status, results, be.bstrerror(status));
      Mmsg(errmsg, _("Device %s cannot be %smounted. ERR=%s\n"),
           print_name(), (mount ? "" : "un"), be.bstrerror(status));

      set_mounted(false);
      free_pool_memory(results);
      Dmsg0(200, "============ mount=0\n");
      Dsm_check(200);
      return false;
   }

   set_mounted(mount);              /* set/clear mounted flag */
   free_pool_memory(results);
   Dmsg1(200, "============ mount=%d\n", mount);
   return true;
}

/*
 * (Un)mount the device (either a FILE or DVD device)
 */
bool DEVICE::do_file_mount(int mount, int dotimeout) 
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   DIR* dp;
   char *icmd;
   struct dirent *entry, *result;
   int status, tries, name_max, count;
   berrno be;

   Dsm_check(200);
   if (mount) {
      icmd = device->mount_command;
   } else {
      icmd = device->unmount_command;
   }
   
   clear_freespace_ok();
   edit_mount_codes(ocmd, icmd);
   
   Dmsg2(100, "do_file_mount: cmd=%s mounted=%d\n", ocmd.c_str(), !!is_mounted());

   if (dotimeout) {
      /* Try at most 10 times to (un)mount the device. This should perhaps be configurable. */
      tries = 10;
   } else {
      tries = 1;
   }
   results = get_memory(4000);

   /* If busy retry each second */
   Dmsg1(100, "do_file_mount run_prog=%s\n", ocmd.c_str());
   while ((status = run_program_full_output(ocmd.c_str(), max_open_wait/2, results)) != 0) {
      /* Doesn't work with internationalization (This is not a problem) */
      if (mount && fnmatch("*is already mounted on*", results, 0) == 0) {
         break;
      }
      if (!mount && fnmatch("* not mounted*", results, 0) == 0) {
         break;
      }
      if (tries-- > 0) {
         /* Sometimes the device cannot be mounted because it is already mounted.
          * Try to unmount it, then remount it */
         if (mount) {
            Dmsg1(400, "Trying to unmount the device %s...\n", print_name());
            do_file_mount(0, 0);
         }
         bmicrosleep(1, 0);
         continue;
      }
      Dmsg5(100, "Device %s cannot be %smounted. stat=%d result=%s ERR=%s\n", print_name(),
           (mount ? "" : "un"), status, results, be.bstrerror(status));
      Mmsg(errmsg, _("Device %s cannot be %smounted. ERR=%s\n"),
           print_name(), (mount ? "" : "un"), be.bstrerror(status));

      /*
       * Now, just to be sure it is not mounted, try to read the filesystem.
       */
      name_max = pathconf(".", _PC_NAME_MAX);
      if (name_max < 1024) {
         name_max = 1024;
      }
         
      if (!(dp = opendir(device->mount_point))) {
         berrno be;
         dev_errno = errno;
         Dmsg3(100, "do_file_mount: failed to open dir %s (dev=%s), ERR=%s\n", 
               device->mount_point, print_name(), be.bstrerror());
         goto get_out;
      }
      
      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
      count = 0;
      while (1) {
         if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
            dev_errno = EIO;
            Dmsg2(129, "do_file_mount: failed to find suitable file in dir %s (dev=%s)\n", 
                  device->mount_point, print_name());
            break;
         }
         if ((strcmp(result->d_name, ".")) && (strcmp(result->d_name, "..")) && (strcmp(result->d_name, ".keep"))) {
            count++; /* result->d_name != ., .. or .keep (Gentoo-specific) */
            break;
         } else {
            Dmsg2(129, "do_file_mount: ignoring %s in %s\n", result->d_name, device->mount_point);
         }
      }
      free(entry);
      closedir(dp);
      
      Dmsg1(100, "do_file_mount: got %d files in the mount point (not counting ., .. and .keep)\n", count);
      
      if (count > 0) {
         /* If we got more than ., .. and .keep */
         /*   there must be something mounted */
         if (mount) {
            Dmsg1(100, "Did Mount by count=%d\n", count);
            break;
         } else {
            /* An unmount request. We failed to unmount - report an error */
            set_mounted(true);
            free_pool_memory(results);
            Dmsg0(200, "== error mount=1 wanted unmount\n");
            return false;
         }
      }
get_out:
      set_mounted(false);
      free_pool_memory(results);
      Dmsg0(200, "============ mount=0\n");
      Dsm_check(200);
      return false;
   }
   
   set_mounted(mount);              /* set/clear mounted flag */
   free_pool_memory(results);
   /* Do not check free space when unmounting */
   if (mount && !update_freespace()) {
      return false;
   }
   Dmsg1(200, "============ mount=%d\n", mount);
   return true;
}

/*
 * Edit codes into (Un)MountCommand, Write(First)PartCommand
 *  %% = %
 *  %a = archive device name
 *  %e = erase (set if cannot mount and first part)
 *  %n = part number
 *  %m = mount point
 *  %v = last part name
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *
 */
void DEVICE::edit_mount_codes(POOL_MEM &omsg, const char *imsg)
{
   const char *p;
   const char *str;
   char add[20];
   
   POOL_MEM archive_name(PM_FNAME);

   omsg.c_str()[0] = 0;
   Dmsg1(800, "edit_mount_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'a':
            str = dev_name;
            break;
         case 'e':
            if (num_dvd_parts == 0) {
               if (truncating || blank_dvd) {
                  str = "2";
               } else {
                  str = "1";
               }
            } else {
               str = "0";
            }
            break;
         case 'n':
            bsnprintf(add, sizeof(add), "%d", part);
            str = add;
            break;
         case 'm':
            str = device->mount_point;
            break;
         case 'v':
            make_spooled_dvd_filename(this, archive_name);
            str = archive_name.c_str();
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
      pm_strcat(omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg.c_str());
   }
}

/* return the last timer interval (ms) 
 * or 0 if something goes wrong
 */
btime_t DEVICE::get_timer_count()
{
   btime_t temp = last_timer;
   last_timer = get_current_btime();
   temp = last_timer - temp;   /* get elapsed time */
   return (temp>0)?temp:0;     /* take care of skewed clock */
}

/* read from fd */
ssize_t DEVICE::read(void *buf, size_t len)
{
   ssize_t read_len ;

   get_timer_count();

   read_len = d_read(m_fd, buf, len);

   last_tick = get_timer_count();

   DevReadTime += last_tick;
   VolCatInfo.VolReadTime += last_tick;

   if (read_len > 0) {          /* skip error */
      DevReadBytes += read_len;
   }

   return read_len;   
}

/* write to fd */
ssize_t DEVICE::write(const void *buf, size_t len)
{
   ssize_t write_len ;

   get_timer_count();

   write_len = d_write(m_fd, buf, len);

   last_tick = get_timer_count();

   DevWriteTime += last_tick;
   VolCatInfo.VolWriteTime += last_tick;

   if (write_len > 0) {         /* skip error */
      DevWriteBytes += write_len;
   }

   return write_len;   
}

/* Return the resource name for the device */
const char *DEVICE::name() const
{
   return device->hdr.name;
}

/* Returns file position on tape or -1 */
int32_t DEVICE::get_os_tape_file()
{
   struct mtget mt_stat;

   if (has_cap(CAP_MTIOCGET) &&
       d_ioctl(m_fd, MTIOCGET, (char *)&mt_stat) == 0) {
      return mt_stat.mt_fileno;
   }
   return -1;
}

char *
dev_vol_name(DEVICE *dev)
{
   return dev->getVolCatName();
}


/*
 * Free memory allocated for the device
 */
void DEVICE::term(void)
{
   DEVICE *dev = NULL;
   Dmsg1(900, "term dev: %s\n", print_name());
   close();
   if (dev_name) {
      free_memory(dev_name);
      dev_name = NULL;
   }
   if (prt_name) {
      free_memory(prt_name);
      prt_name = NULL;
   }
   if (errmsg) {
      free_pool_memory(errmsg);
      errmsg = NULL;
   }
   pthread_mutex_destroy(&m_mutex);
   pthread_cond_destroy(&wait);
   pthread_cond_destroy(&wait_next_vol);
   pthread_mutex_destroy(&spool_mutex);
// rwl_destroy(&lock);
   if (attached_dcrs) {
      delete attached_dcrs;
      attached_dcrs = NULL;
   }
   if (device) {
      device->dev = NULL;
   }
   delete this;
   if (dev) {
      dev->term();
   }
}

/*
 * This routine initializes the device wait timers
 */
void init_device_wait_timers(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   /* ******FIXME******* put these on config variables */
   dev->min_wait = 60 * 60;
   dev->max_wait = 24 * 60 * 60;
   dev->max_num_wait = 9;              /* 5 waits =~ 1 day, then 1 day at a time */
   dev->wait_sec = dev->min_wait;
   dev->rem_wait_sec = dev->wait_sec;
   dev->num_wait = 0;
   dev->poll = false;

   jcr->min_wait = 60 * 60;
   jcr->max_wait = 24 * 60 * 60;
   jcr->max_num_wait = 9;              /* 5 waits =~ 1 day, then 1 day at a time */
   jcr->wait_sec = jcr->min_wait;
   jcr->rem_wait_sec = jcr->wait_sec;
   jcr->num_wait = 0;

}

void init_jcr_device_wait_timers(JCR *jcr)
{
   /* ******FIXME******* put these on config variables */
   jcr->min_wait = 60 * 60;
   jcr->max_wait = 24 * 60 * 60;
   jcr->max_num_wait = 9;              /* 5 waits =~ 1 day, then 1 day at a time */
   jcr->wait_sec = jcr->min_wait;
   jcr->rem_wait_sec = jcr->wait_sec;
   jcr->num_wait = 0;
}


/*
 * The dev timers are used for waiting on a particular device 
 *
 * Returns: true if time doubled
 *          false if max time expired
 */
bool double_dev_wait_time(DEVICE *dev)
{
   dev->wait_sec *= 2;               /* double wait time */
   if (dev->wait_sec > dev->max_wait) {   /* but not longer than maxtime */
      dev->wait_sec = dev->max_wait;
   }
   dev->num_wait++;
   dev->rem_wait_sec = dev->wait_sec;
   if (dev->num_wait >= dev->max_num_wait) {
      return false;
   }
   return true;
}


void set_os_device_parameters(DCR *dcr)
{
   DEVICE *dev = dcr->dev;

   if (strcmp(dev->dev_name, "/dev/null") == 0) {
      return;                            /* no use trying to set /dev/null */
   }

#if defined(HAVE_LINUX_OS) || defined(HAVE_WIN32)
   struct mtop mt_com;

   Dmsg0(100, "In set_os_device_parameters\n");
#if defined(MTSETBLK) 
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSETBLK;
      mt_com.mt_count = 0;
      Dmsg0(100, "Set block size to zero\n");
      if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTSETBLK);
      }
   }
#endif
#if defined(MTSETDRVBUFFER)
   if (getuid() == 0) {          /* Only root can do this */
      mt_com.mt_op = MTSETDRVBUFFER;
      mt_com.mt_count = MT_ST_CLEARBOOLEANS;
      if (!dev->has_cap(CAP_TWOEOF)) {
         mt_com.mt_count |= MT_ST_TWO_FM;
      }
      if (dev->has_cap(CAP_EOM)) {
         mt_com.mt_count |= MT_ST_FAST_MTEOM;
      }
      Dmsg0(100, "MTSETDRVBUFFER\n");
      if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTSETDRVBUFFER);
      }
   }
#endif
   return;
#endif

#ifdef HAVE_NETBSD_OS
   struct mtop mt_com;
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSETBSIZ;
      mt_com.mt_count = 0;
      if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTSETBSIZ);
      }
      /* Get notified at logical end of tape */
      mt_com.mt_op = MTEWARN;
      mt_com.mt_count = 1;
      if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTEWARN);
      }
   }
   return;
#endif

#if HAVE_FREEBSD_OS || HAVE_OPENBSD_OS
   struct mtop mt_com;
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSETBSIZ;
      mt_com.mt_count = 0;
      if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTSETBSIZ);
      }
   }
#if defined(MTIOCSETEOTMODEL) 
   uint32_t neof;
   if (dev->has_cap(CAP_TWOEOF)) {
      neof = 2;
   } else {
      neof = 1;
   }
   if (dev->d_ioctl(dev->fd(), MTIOCSETEOTMODEL, (caddr_t)&neof) < 0) {
      berrno be;
      dev->dev_errno = errno;         /* save errno */
      Mmsg2(dev->errmsg, _("Unable to set eotmodel on device %s: ERR=%s\n"),
            dev->print_name(), be.bstrerror(dev->dev_errno));
      Jmsg(dcr->jcr, M_FATAL, 0, dev->errmsg);
   }
#endif
   return;
#endif

#ifdef HAVE_SUN_OS
   struct mtop mt_com;
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSRSZ;
      mt_com.mt_count = 0;
      if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTSRSZ);
      }
   }
   return;
#endif
}

static bool dev_get_os_pos(DEVICE *dev, struct mtget *mt_stat)
{
   Dmsg0(100, "dev_get_os_pos\n");
   return dev->has_cap(CAP_MTIOCGET) && 
          dev->d_ioctl(dev->fd(), MTIOCGET, (char *)mt_stat) == 0 &&
          mt_stat->mt_fileno >= 0;
}

static const char *modes[] = {
   "CREATE_READ_WRITE",
   "OPEN_READ_WRITE",
   "OPEN_READ_ONLY",
   "OPEN_WRITE_ONLY"
};


static const char *mode_to_str(int mode)  
{
   static char buf[100];
   if (mode < 1 || mode > 4) {
      bsnprintf(buf, sizeof(buf), "BAD mode=%d", mode);
      return buf;
    }
   return modes[mode-1];
}
