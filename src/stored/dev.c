/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * dev.c  -- low level operations on device (storage device)
 *
 * Kern Sibbald, MM
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

#include "bareos.h"
#include "stored.h"

#ifndef HAVE_DYNAMIC_SD_BACKENDS
#ifdef HAVE_GFAPI
#include "backends/gfapi_device.h"
#endif
#ifdef HAVE_OBJECTSTORE
#include "backends/chunked_device.h"
#include "backends/object_store_device.h"
#endif
#ifdef HAVE_RADOS
#include "backends/rados_device.h"
#endif
#ifdef HAVE_CEPHFS
#include "backends/cephfs_device.h"
#endif
#ifdef HAVE_ELASTO
#include "backends/elasto_device.h"
#endif
#include "backends/generic_tape_device.h"
#ifdef HAVE_WIN32
#include "backends/win32_tape_device.h"
#include "backends/win32_fifo_device.h"
#else
#include "backends/unix_tape_device.h"
#include "backends/unix_fifo_device.h"
#endif
#endif /* HAVE_DYNAMIC_SD_BACKENDS */

#ifdef HAVE_WIN32
#include "backends/win32_file_device.h"
#else
#include "backends/unix_file_device.h"
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif

/* Forward referenced functions */

static const char *modes[] = {
   "CREATE_READ_WRITE",
   "OPEN_READ_WRITE",
   "OPEN_READ_ONLY",
   "OPEN_WRITE_ONLY"
};

const char *DEVICE::mode_to_str(int mode)
{
   static char buf[100];

   if (mode < 1 || mode > 4) {
      bsnprintf(buf, sizeof(buf), "BAD mode=%d", mode);
      return buf;
   }

   return modes[mode - 1];
}

static inline DEVICE *m_init_dev(JCR *jcr, DEVRES *device, bool new_init)
{
   struct stat statp;
   int errstat;
   DCR *dcr = NULL;
   DEVICE *dev = NULL;
   uint32_t max_bs;

   Dmsg1(400, "max_block_size in device res is %u\n", device->max_block_size);

   /*
    * If no device type specified, try to guess
    */
   if (!device->dev_type) {
      /*
       * Check that device is available
       */
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
      } else if (!bit_is_set(CAP_REQMOUNT, device->cap_bits)) {
         Jmsg2(jcr, M_ERROR, 0,
               _("%s is an unknown device type. Must be tape or directory, st_mode=%04o\n"),
               device->device_name, (statp.st_mode & ~S_IFMT));
         return NULL;
      }
   }

   /*
    * See what type of device is wanted.
    */
   switch (device->dev_type) {
   /*
    * When using dynamic loading use the init_backend_dev() function
    * for any type of device not being of the type file.
    */
#ifndef HAVE_DYNAMIC_SD_BACKENDS
#ifdef HAVE_GFAPI
   case B_GFAPI_DEV:
      dev = New(gfapi_device);
      break;
#endif
#ifdef HAVE_OBJECTSTORE
   case B_OBJECT_STORE_DEV:
      dev = New(object_store_device);
      break;
#endif
#ifdef HAVE_RADOS
   case B_RADOS_DEV:
      dev = New(rados_device);
      break;
#endif
#ifdef HAVE_CEPHFS
   case B_CEPHFS_DEV:
      dev = New(cephfs_device);
      break;
#endif
#ifdef HAVE_ELASTO
   case B_ELASTO_DEV:
      dev = New(elasto_device);
      break;
#endif
#ifdef HAVE_WIN32
   case B_TAPE_DEV:
      dev = New(win32_tape_device);
      break;
   case B_FIFO_DEV:
      dev = New(win32_fifo_device);
      break;
#else
   case B_TAPE_DEV:
      dev = New(unix_tape_device);
      break;
   case B_FIFO_DEV:
      dev = New(unix_fifo_device);
      break;
#endif
#endif /* HAVE_DYNAMIC_SD_BACKENDS */
#ifdef HAVE_WIN32
   case B_FILE_DEV:
      dev = New(win32_file_device);
      break;
#else
   case B_FILE_DEV:
      dev = New(unix_file_device);
      break;
#endif
   default:
#ifdef HAVE_DYNAMIC_SD_BACKENDS
      dev = init_backend_dev(jcr, device->dev_type);
#endif
      break;
   }

   if (!dev) {
      Jmsg2(jcr, M_ERROR, 0, _("%s has an unknown device type %d\n"),
            device->device_name, device->dev_type);
      return NULL;
   }
   dev->clear_slot();         /* unknown */

   /*
    * Copy user supplied device parameters from Resource
    */
   dev->dev_name = get_memory(strlen(device->device_name) + 1);
   pm_strcpy(dev->dev_name, device->device_name);
   if (device->device_options) {
      dev->dev_options = get_memory(strlen(device->device_options) + 1);
      pm_strcpy(dev->dev_options, device->device_options);
   }
   dev->prt_name = get_memory(strlen(device->device_name) + strlen(device->name()) + 20);

   /*
    * We edit "Resource-name" (physical-name)
    */
   Mmsg(dev->prt_name, "\"%s\" (%s)", device->name(), device->device_name);
   Dmsg1(400, "Allocate dev=%s\n", dev->print_name());
   copy_bits(CAP_MAX, device->cap_bits, dev->capabilities);

   /*
    * current block sizes
    */
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
   dev->drive = device->drive;
   dev->drive_index = device->drive_index;
   dev->autoselect = device->autoselect;
   dev->norewindonclose = device->norewindonclose;
   dev->dev_type = device->dev_type;
   dev->device = device;

   /*
    * Sanity check
    */
   if (dev->vol_poll_interval && dev->vol_poll_interval < 60) {
      dev->vol_poll_interval = 60;
   }
   device->dev = dev;

   if (dev->is_fifo()) {
      dev->set_cap(CAP_STREAM);       /* set stream device */
   }

   /*
    * If the device requires mount :
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

   /*
    * Sanity check
    */
   if (dev->max_block_size == 0) {
      max_bs = DEFAULT_BLOCK_SIZE;
   } else {
      max_bs = dev->max_block_size;
   }
   if (dev->min_block_size > max_bs) {
      Jmsg(jcr, M_ERROR_TERM, 0, _("Min block size > max on device %s\n"), dev->print_name());
   }
   if (dev->max_block_size > MAX_BLOCK_LENGTH) {
      Jmsg3(jcr, M_ERROR, 0, _("Block size %u on device %s is too large, using default %u\n"),
            dev->max_block_size, dev->print_name(), DEFAULT_BLOCK_SIZE);
      dev->max_block_size = 0;
   }
   if (dev->max_block_size % TAPE_BSIZE != 0) {
      Jmsg3(jcr, M_WARNING, 0, _("Max block size %u not multiple of device %s block size=%d.\n"),
            dev->max_block_size, dev->print_name(), TAPE_BSIZE);
   }
   if (dev->max_volume_size != 0 && dev->max_volume_size < (dev->max_block_size << 4)) {
      Jmsg(jcr, M_ERROR_TERM, 0, _("Max Vol Size < 8 * Max Block Size for device %s\n"), dev->print_name());
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
   Dmsg3(100, "dev=%s dev_max_bs=%u max_bs=%u\n", dev->dev_name, dev->device->max_block_size, dev->max_block_size);

   return dev;
}

/*
 * Allocate and initialize the DEVICE structure
 * Note, if dev is non-NULL, it is already allocated,
 * thus we neither allocate it nor free it. This allows
 * the caller to put the packet in shared memory.
 *
 * Note, for a tape, the device->device_name is the device name
 * (e.g. /dev/nst0), and for a file, the device name
 * is the directory in which the file will be placed.
 */
DEVICE *init_dev(JCR *jcr, DEVRES *device)
{
   DEVICE *dev;

   dev = m_init_dev(jcr, device, false);
   return dev;
}

/*
 * Set the block size of the device.
 * If the volume block size is zero, we set the max block size to what is
 * configured in the device resource i.e. dev->device->max_block_size.
 *
 * If dev->device->max_block_size is zero, do nothing and leave dev->max_block_size as it is.
 */
void DEVICE::set_blocksizes(DCR *dcr) {

   DEVICE* dev = this;
   JCR* jcr = dcr->jcr;
   uint32_t max_bs;

   Dmsg4(100, "Device %s has dev->device->max_block_size of %u and dev->max_block_size of %u, dcr->VolMaxBlocksize is %u\n",
         dev->print_name(), dev->device->max_block_size, dev->max_block_size, dcr->VolMaxBlocksize);

   if (dcr->VolMaxBlocksize == 0 && dev->device->max_block_size != 0) {
      Dmsg2(100, "setting dev->max_block_size to dev->device->max_block_size=%u "
                 "on device %s because dcr->VolMaxBlocksize is 0\n",
            dev->device->max_block_size, dev->print_name());
      dev->min_block_size = dev->device->min_block_size;
      dev->max_block_size = dev->device->max_block_size;
   } else if (dcr->VolMaxBlocksize != 0) {
       dev->min_block_size = dcr->VolMinBlocksize;
       dev->max_block_size = dcr->VolMaxBlocksize;
   }

   /*
    * Sanity check
    */
   if (dev->max_block_size == 0) {
      max_bs = DEFAULT_BLOCK_SIZE;
   } else {
      max_bs = dev->max_block_size;
   }

   if (dev->min_block_size > max_bs) {
      Jmsg(jcr, M_ERROR_TERM, 0, _("Min block size > max on device %s\n"), dev->print_name());
   }

   if (dev->max_block_size > MAX_BLOCK_LENGTH) {
      Jmsg3(jcr, M_ERROR, 0, _("Block size %u on device %s is too large, using default %u\n"),
            dev->max_block_size, dev->print_name(), DEFAULT_BLOCK_SIZE);
      dev->max_block_size = 0;
   }

   if (dev->max_block_size % TAPE_BSIZE != 0) {
      Jmsg3(jcr, M_WARNING, 0, _("Max block size %u not multiple of device %s block size=%d.\n"),
            dev->max_block_size, dev->print_name(), TAPE_BSIZE);
   }

   if (dev->max_volume_size != 0 && dev->max_volume_size < (dev->max_block_size << 4)) {
      Jmsg(jcr, M_ERROR_TERM, 0, _("Max Vol Size < 8 * Max Block Size for device %s\n"), dev->print_name());
   }

   Dmsg3(100, "set minblocksize to %d, maxblocksize to %d on device %s\n",
         dev->min_block_size, dev->max_block_size, dev->print_name());

   /*
    * If blocklen is not dev->max_block_size create a new block with the right size.
    * (as header is always dev->label_block_size which is preset with DEFAULT_BLOCK_SIZE)
    */
   if (dcr->block) {
     if (dcr->block->buf_len != dev->max_block_size) {
         Dmsg2(100, "created new block of buf_len: %u on device %s\n",
               dev->max_block_size, dev->print_name());
         free_block(dcr->block);
         dcr->block = new_block(dev);
         Dmsg2(100, "created new block of buf_len: %u on device %s, freeing block\n",
               dcr->block->buf_len, dev->print_name());
      }
   }
}

/*
 * Set the block size of the device to the label_block_size
 * to read labels as we want to always use that blocksize when
 * writing volume labels
 */
void DEVICE::set_label_blocksize(DCR *dcr)
{
   DEVICE *dev = this;
   Dmsg3(100, "setting minblocksize to %u, "
              "maxblocksize to label_block_size=%u, on device %s\n",
         dev->device->label_block_size, dev->device->label_block_size, dev->print_name());

   dev->min_block_size = dev->device->label_block_size;
   dev->max_block_size = dev->device->label_block_size;
   /*
    * If blocklen is not dev->max_block_size create a new block with the right size
    * (as header is always label_block_size)
    */
   if (dcr->block) {
     if (dcr->block->buf_len != dev->max_block_size) {
         free_block(dcr->block);
         dcr->block = new_block(dev);
         Dmsg2(100, "created new block of buf_len: %u on device %s\n",
               dcr->block->buf_len, dev->print_name());
      }
   }
}

/*
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
bool DEVICE::open(DCR *dcr, int omode)
{
   char preserve[ST_BYTES];

   clear_all_bits(ST_MAX, preserve);
   if (is_open()) {
      if (open_mode == omode) {
         return true;
      } else {
         d_close(m_fd);
         clear_opened();
         Dmsg0(100, "Close fd for mode change.\n");

         if (bit_is_set(ST_LABEL, state))
            set_bit(ST_LABEL, preserve);
         if (bit_is_set(ST_APPENDREADY, state))
            set_bit(ST_APPENDREADY, preserve);
         if (bit_is_set(ST_READREADY, state))
            set_bit(ST_READREADY, preserve);
      }
   }

   if (dcr) {
      dcr->setVolCatName(dcr->VolumeName);
      VolCatInfo = dcr->VolCatInfo;    /* structure assign */
   }

   Dmsg4(100, "open dev: type=%d dev_name=%s vol=%s mode=%s\n", dev_type,
         print_name(), getVolCatName(), mode_to_str(omode));

   clear_bit(ST_LABEL, state);
   clear_bit(ST_APPENDREADY, state);
   clear_bit(ST_READREADY, state);
   clear_bit(ST_EOT, state);
   clear_bit(ST_WEOT, state);
   clear_bit(ST_EOF, state);

   label_type = B_BAREOS_LABEL;

   /*
    * We are about to open the device so let any plugin know we are.
    */
   if (dcr && generate_plugin_event(dcr->jcr, bsdEventDeviceOpen, dcr) != bRC_OK) {
      Dmsg0(100, "open_dev: bsdEventDeviceOpen failed\n");
      return false;
   }

   Dmsg1(100, "call open_device mode=%s\n", mode_to_str(omode));
   open_device(dcr, omode);

   /*
    * Reset any important state info
    */
   clone_bits(ST_MAX, preserve, state);

   Dmsg2(100, "preserve=%08o fd=%d\n", preserve, m_fd);

   return m_fd >= 0;
}

void DEVICE::set_mode(int mode)
{
   switch (mode) {
   case CREATE_READ_WRITE:
      oflags = O_CREAT | O_RDWR | O_BINARY;
      break;
   case OPEN_READ_WRITE:
      oflags = O_RDWR | O_BINARY;
      break;
   case OPEN_READ_ONLY:
      oflags = O_RDONLY | O_BINARY;
      break;
   case OPEN_WRITE_ONLY:
      oflags = O_WRONLY | O_BINARY;
      break;
   default:
      Emsg0(M_ABORT, 0, _("Illegal mode given to open dev.\n"));
   }
}

/*
 * Open a device.
 */
void DEVICE::open_device(DCR *dcr, int omode)
{
   POOL_MEM archive_name(PM_FNAME);

   get_autochanger_loaded_slot(dcr);

   /*
    * Handle opening of File Archive (not a tape)
    */
   pm_strcpy(archive_name, dev_name);

   /*
    * If this is a virtual autochanger (i.e. changer_res != NULL) we simply use
    * the device name, assuming it has been appropriately setup by the "autochanger".
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

   mount(dcr, 1);                     /* do mount if required */

   open_mode = omode;
   set_mode(omode);

   /*
    * If creating file, give 0640 permissions
    */
   Dmsg3(100, "open disk: mode=%s open(%s, %08o, 0640)\n", mode_to_str(omode),
         archive_name.c_str(), oflags);

   if ((m_fd = d_open(archive_name.c_str(), oflags, 0640)) < 0) {
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

   Dmsg1(100, "open dev: disk fd=%d opened\n", m_fd);
}

/*
 * Rewind the device.
 *
 * Returns: true  on success
 *          false on failure
 */
bool DEVICE::rewind(DCR *dcr)
{
   Dmsg3(400, "rewind res=%d fd=%d %s\n", num_reserved(), m_fd, print_name());

   /*
    * Remove EOF/EOT flags
    */
   clear_bit(ST_EOT, state);
   clear_bit(ST_EOF, state);
   clear_bit(ST_WEOT, state);

   block_num = file = 0;
   file_size = 0;
   file_addr = 0;

   if (m_fd < 0) {
      return false;
   }

   if (is_fifo() || is_vtl()) {
      return true;
   }

   if (lseek(dcr, (boffset_t)0, SEEK_SET) < 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"), print_name(), be.bstrerror());
      return false;
   }

   return true;
}

/*
 * Called to indicate that we have just read an EOF from the device.
 */
void DEVICE::set_ateof()
{
   set_eof();
   file_addr = 0;
   file_size = 0;
   block_num = 0;
}

/*
 * Called to indicate we are now at the end of the volume, and writing is not possible.
 */
void DEVICE::set_ateot()
{
   /*
    * Make volume effectively read-only
    */
   set_bit(ST_EOF, state);
   set_bit(ST_EOT, state);
   set_bit(ST_WEOT, state);
   clear_append();
}

/*
 * Position device to end of medium (end of data)
 *
 * Returns: true  on succes
 *          false on error
 */
bool DEVICE::eod(DCR *dcr)
{
   boffset_t pos;

   if (m_fd < 0) {
      dev_errno = EBADF;
      Mmsg1(errmsg, _("Bad call to eod. Device %s not open\n"), print_name());
      return false;
   }

   if (is_vtl()) {
      return true;
   }

   Dmsg0(100, "Enter eod\n");
   if (at_eot()) {
      return true;
   }

   clear_eof();         /* remove EOF flag */

   block_num = file = 0;
   file_size = 0;
   file_addr = 0;

   pos = lseek(dcr, (boffset_t)0, SEEK_END);
   Dmsg1(200, "====== Seek to %lld\n", pos);

   if (pos >= 0) {
      update_pos(dcr);
      set_eot();
      return true;
   }

   dev_errno = errno;
   berrno be;
   Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"), print_name(), be.bstrerror());
   Dmsg0(100, errmsg);

   return false;
}

/*
 * Set the position of the device.
 *
 * Returns: true  on succes
 *          false on error
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

   if (is_fifo() || is_vtl()) {
      return true;
   }

   file = 0;
   file_addr = 0;
   pos = lseek(dcr, (boffset_t)0, SEEK_CUR);
   if (pos < 0) {
      berrno be;
      dev_errno = errno;
      Pmsg1(000, _("Seek error: ERR=%s\n"), be.bstrerror());
      Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"), print_name(), be.bstrerror());
      ok = false;
   } else {
      file_addr = pos;
      block_num = (uint32_t)pos;
      file = (uint32_t)(pos >> 32);
   }

   return ok;
}

char *DEVICE::status_dev()
{
   char *status;

   status = (char *)malloc(BMT_BYTES);
   clear_all_bits(BMT_MAX, status);

   if (bit_is_set(ST_EOT, state) || bit_is_set(ST_WEOT, state)) {
      set_bit(BMT_EOD, status);
      Pmsg0(-20, " EOD");
   }

   if (bit_is_set(ST_EOF, state)) {
      set_bit(BMT_EOF, status);
      Pmsg0(-20, " EOF");
   }

   set_bit(BMT_ONLINE, status);
   set_bit(BMT_BOT, status);

   return status;
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
       * in prior versions of Bareos), but on FreeBSD, this is
       * needed in the case the tape was "frozen" due to an error
       * such as backspacing after writing and EOF. If it is not
       * done, all future references to the drive get and I/O error.
       */
      clrerror(MTREW);
      return rewind(NULL);
   }
}

void DEVICE::set_slot(slot_number_t slot)
{
   m_slot = slot;
   if (vol) {
      vol->clear_slot();
   }
}

void DEVICE::clear_slot()
{
   m_slot = -1;
   if (vol) {
      vol->set_slot(-1);
   }
}

/*
 * Reposition the device to file, block
 *
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

   if (is_fifo() || is_vtl()) {
      return true;
   }

   boffset_t pos = (((boffset_t)rfile) << 32) | rblock;
   Dmsg1(100, "===== lseek to %d\n", (int)pos);
   if (lseek(dcr, pos, SEEK_SET) == (boffset_t)-1) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"), print_name(), be.bstrerror());
      return false;
   }
   file = rfile;
   block_num = rblock;
   file_addr = pos;
   return true;
}

/*
 * Set to unload the current volume in the drive.
 */
void DEVICE::set_unload()
{
   if (!m_unload && VolHdr.VolumeName[0] != 0) {
       m_unload = true;
       memcpy(UnloadVolName, VolHdr.VolumeName, sizeof(UnloadVolName));
   }
}

/*
 * Clear volume header.
 */
void DEVICE::clear_volhdr()
{
   Dmsg1(100, "Clear volhdr vol=%s\n", VolHdr.VolumeName);
   memset(&VolHdr, 0, sizeof(VolHdr));
   setVolCatInfo(false);
}

/*
 * Close the device.
 */
bool DEVICE::close(DCR *dcr)
{
   bool retval = true;
   int status;
   Dmsg1(100, "close_dev %s\n", print_name());

   if (!is_open()) {
      Dmsg2(100, "device %s already closed vol=%s\n", print_name(), VolHdr.VolumeName);
      goto bail_out;                  /* already closed */
   }

   if (!norewindonclose) {
      offline_or_rewind();
   }

   switch (dev_type) {
   case B_VTL_DEV:
   case B_TAPE_DEV:
      unlock_door();
      /*
       * Fall through wanted
       */
   default:
      status = d_close(m_fd);
      if (status < 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to close device %s. ERR=%s\n"),
               print_name(), be.bstrerror());
         dev_errno = errno;
         retval = false;
      }
      break;
   }

   unmount(dcr, 1);                   /* do unmount if required */

   /*
    * Clean up device packet so it can be reused.
    */
   clear_opened();

   clear_bit(ST_LABEL, state);
   clear_bit(ST_READREADY, state);
   clear_bit(ST_APPENDREADY, state);
   clear_bit(ST_EOT, state);
   clear_bit(ST_WEOT, state);
   clear_bit(ST_EOF, state);
   clear_bit(ST_MOUNTED, state);
   clear_bit(ST_MEDIA, state);
   clear_bit(ST_SHORT, state);

   label_type = B_BAREOS_LABEL;
   file = block_num = 0;
   file_size = 0;
   file_addr = 0;
   EndFile = EndBlock = 0;
   open_mode = 0;
   clear_volhdr();
   memset(&VolCatInfo, 0, sizeof(VolCatInfo));
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }

   /*
    * We closed the device so let any plugin know we did.
    */
   if (dcr) {
      generate_plugin_event(dcr->jcr, bsdEventDeviceClose, dcr);
   }

bail_out:
   return retval;
}

/*
 * Mount the device.
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool DEVICE::mount(DCR *dcr, int timeout)
{
   bool retval = true;
   Dmsg0(190, "Enter mount\n");

   if (is_mounted()) {
      return true;
   }

   retval = mount_backend(dcr, timeout);

   /*
    * When the mount command succeeded sent a
    * bsdEventDeviceMount plugin event so any plugin
    * that want to do something can do things now.
    */
   if (retval && generate_plugin_event(dcr->jcr, bsdEventDeviceMount, dcr) != bRC_OK) {
      retval = false;
   }

   /*
    * Mark the device mounted if we succeed.
    */
   if (retval) {
      set_mounted();
   }

   return retval;
}

/*
 * Unmount the device
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool DEVICE::unmount(DCR *dcr, int timeout)
{
   bool retval = true;
   Dmsg0(100, "Enter unmount\n");

   /*
    * See if the device is mounted.
    */
   if (!is_mounted()) {
      return true;
   }

   /*
    * Before running the unmount program sent a
    * bsdEventDeviceUnmount plugin event so any plugin
    * that want to do something can do things now.
    */
   if (dcr && generate_plugin_event(dcr->jcr, bsdEventDeviceUnmount, dcr) != bRC_OK) {
      retval = false;
      goto bail_out;
   }

   retval = unmount_backend(dcr, timeout);

   /*
    * Mark the device unmounted if we succeed.
    */
   if (retval) {
      clear_mounted();
   }

bail_out:
   return retval;
}

/*
 * Edit codes into (Un)MountCommand
 *  %% = %
 *  %a = archive device name
 *  %m = mount point
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
         case 'm':
            str = device->mount_point;
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

/*
 * Return the last timer interval (ms) or 0 if something goes wrong
 */
btime_t DEVICE::get_timer_count()
{
   btime_t temp = last_timer;
   last_timer = get_current_btime();
   temp = last_timer - temp;   /* get elapsed time */

   return (temp > 0) ? temp : 0; /* take care of skewed clock */
}

/*
 * Read from device.
 */
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

/*
 * Write to device.
 */
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

/*
 * Return the resource name for the device
 */
const char *DEVICE::name() const
{
   return device->name();
}

/*
 * Free memory allocated for the device
 */
void DEVICE::term()
{
   Dmsg1(900, "term dev: %s\n", print_name());

   /*
    * On termination we don't have any DCRs left
    * so we call close with a NULL argument as
    * the dcr argument is only used in the unmount
    * method to generate a plugin_event we just check
    * there if the dcr is not NULL and otherwise skip
    * the plugin event generation.
    */
   close(NULL);

   if (dev_name) {
      free_memory(dev_name);
      dev_name = NULL;
   }
   if (dev_options) {
      free_memory(dev_options);
      dev_options = NULL;
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
   dev->wait_sec *= 2;               /* Double wait time */
   if (dev->wait_sec > dev->max_wait) { /* But not longer than maxtime */
      dev->wait_sec = dev->max_wait;
   }
   dev->num_wait++;
   dev->rem_wait_sec = dev->wait_sec;
   if (dev->num_wait >= dev->max_num_wait) {
      return false;
   }
   return true;
}

DEVICE::DEVICE()
{
   m_fd = -1;
}
