/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, November 2014
 */
/**
 * @file
 * ELASTO API device abstraction.
 */

#include "include/bareos.h"

#ifdef HAVE_ELASTO
#include "stored/stored.h"
#include "elasto_device.h"

#define ELASTO_GRANULARITY 512

 /*
  * Options that can be specified for this device type.
  */
enum device_option_type {
   argument_none = 0,
   argument_publishfile,
   argument_basedir,
   argument_insecure_http
};

struct device_option {
   const char *name;
   enum device_option_type type;
   int compare_size;
};

static device_option device_options[] = {
   { "publish_file=", argument_publishfile, 13 },
   { "basedir=", argument_basedir, 9 },
   { "insecure_http", argument_insecure_http, 13 },
   { NULL, argument_none }
};

/**
 * Open a volume using libelasto.
 */
int elasto_device::d_open(const char *pathname, int flags, int mode)
{
   BErrNo be;
   int status;
   uint32_t elasto_flags = 0;
   struct elasto_fauth auth;
   struct elasto_fstat efstat;

   /*
    * Elasto only allows reads/writes in chunks of 512 bytes so
    * make sure the minimum blocksize and maximum blocksize are set correctly.
    */
   if (min_block_size == 0 || (min_block_size % ELASTO_GRANULARITY) != 0) {
      Emsg2(M_ABORT, 0, _("Device minimum block size (%d) not multiple of elasto blocksize size (%d)\n"),
            min_block_size, ELASTO_GRANULARITY);
      goto bail_out;
   }

   if (max_block_size == 0 || (max_block_size % ELASTO_GRANULARITY) != 0) {
      Emsg2(M_ABORT, 0, _("Device maximum block size (%d) not multiple of elasto blocksize size (%d)\n"),
            max_block_size, ELASTO_GRANULARITY);
      goto bail_out;
   }

   if (!elasto_configstring_) {
      char *bp, *next_option;
      bool done;

      if (!dev_options) {
         Mmsg0(errmsg, _("No device options configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      elasto_configstring_ = bstrdup(dev_options);

      bp = elasto_configstring_;
      while (bp) {
         next_option = strchr(bp, ',');
         if (next_option) {
            *next_option++ = '\0';
         }

         done = false;
         for (int i = 0; !done && device_options[i].name; i++) {
            /*
             * Try to find a matching device option.
             */
            if (bstrncasecmp(bp, device_options[i].name, device_options[i].compare_size)) {
               switch (device_options[i].type) {
               case argument_publishfile:
                  elasto_ps_file_ = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_basedir:
                  basedir_ = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_insecure_http:
                  insecure_http_ = true;
                  done = true;
                  break;
               default:
                  break;
               }
            }
         }

         if (!done) {
            Mmsg1(errmsg, _("Unable to parse device option: %s\n"), bp);
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
         }

         bp = next_option;
      }
   }

   if (!elasto_ps_file_) {
      Mmsg0(errmsg, _("No publish file configured\n"));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
   }

   /*
    * See if we don't have a file open already.
    */
   if (efh_) {
      elasto_fclose(efh_);
      efh_ = NULL;
   }

   auth.type = ELASTO_FILE_AZURE;
   auth.az.ps_path = elasto_ps_file_;
   auth.insecure_http = insecure_http_;

   if (basedir_) {
#if 0
      status = elasto_fmkdir(&auth, basedir_);
      if (status < 0) {
         goto bail_out;
      }
#endif
      Mmsg(virtual_filename_, "%s/%s", basedir_, getVolCatName());
   } else {
      Mmsg(virtual_filename_, "%s", getVolCatName());
   }

   if (flags & O_CREAT) {
      elasto_flags = ELASTO_FOPEN_CREATE;
   }

   status = elasto_fopen(&auth, virtual_filename_, elasto_flags, &efh_);
   if (status < 0) {
      goto bail_out;
   }

   status = elasto_fstat(efh_, &efstat);
   if (status < 0) {
      goto bail_out;
   }

   if (efstat.lease_status == ELASTO_FLEASE_LOCKED) {
      status = elasto_flease_break(efh_);
      if (status < 0) {
         goto bail_out;
      }
   }

   offset_ = 0;

   return 0;

bail_out:
   if (efh_) {
      elasto_fclose(efh_);
      efh_ = NULL;
   }

   return -1;
}

/**
 * Read data from a volume using libelasto.
 */
ssize_t elasto_device::d_read(int fd, void *buffer, size_t count)
{
   if (efh_) {
      int status;
      int nr_read;
      struct elasto_data *data;

      /*
       * When at start of volume make sure we are able to read the amount
       * of bytes requested as the error handling of elasto_fread on
       * a zero size object (when a volume is labeled its first read to
       * see if its not an existing labeled volume).
       */
      if (offset_ == 0) {
         struct elasto_fstat efstat;

         status = elasto_fstat(efh_, &efstat);
         if (status < 0) {
            errno = EIO;
            return -1;
         }

         if (count > efstat.size) {
            errno = EIO;
            return -1;
         }
      }

      status = elasto_data_iov_new((uint8_t *)buffer, count, 0, false, &data);
      if (status < 0) {
         Mmsg0(errmsg, _("Failed to setup iovec for reading data.\n"));
         Emsg0(M_FATAL, 0, errmsg);
         return -1;
      }

      nr_read = elasto_fread(efh_, offset_, count, data);
      if (nr_read >= 0) {
         offset_ += nr_read;
         data->iov.buf = NULL;
         elasto_data_free(data);

         return nr_read;
      } else {
         errno = -nr_read;
         return -1;
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

/**
 * Write data to a volume using libelasto.
 */
ssize_t elasto_device::d_write(int fd, const void *buffer, size_t count)
{
   if (efh_) {
      int status;
      int nr_written;
      struct elasto_data *data;

      status = elasto_data_iov_new((uint8_t *)buffer, count, 0, false, &data);
      if (status < 0) {
         Mmsg0(errmsg, _("Failed to setup iovec for writing data.\n"));
         Emsg0(M_FATAL, 0, errmsg);
         return -1;
      }

      nr_written = elasto_fwrite(efh_, offset_, count, data);
      if (nr_written >= 0) {
         offset_ += nr_written;
         data->iov.buf = NULL;
         elasto_data_free(data);

         return nr_written;
      } else {
         errno = -nr_written;
         return -1;
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

int elasto_device::d_close(int fd)
{
   if (efh_) {
      elasto_fclose(efh_);
      efh_ = NULL;
   } else {
      errno = EBADF;
      return -1;
   }

   return 0;
}

int elasto_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t elasto_device::d_lseek(DeviceControlRecord *dcr, boffset_t offset, int whence)
{
   switch (whence) {
   case SEEK_SET:
      offset_ = offset;
      break;
   case SEEK_CUR:
      offset_ += offset;
      break;
   case SEEK_END:
      if (efh_) {
         int status;
         struct elasto_fstat efstat;

         status = elasto_fstat(efh_, &efstat);
         if (status < 0) {
            return -1;
         }
         offset_ = efstat.size + offset;
      } else {
         return -1;
      }
      break;
   default:
      return -1;
   }

   return offset_;
}

bool elasto_device::d_truncate(DeviceControlRecord *dcr)
{
   if (efh_) {
      int status;
      struct elasto_fstat efstat;

      status = elasto_ftruncate(efh_, 0);
      if (status < 0) {
         return false;
      }

      status = elasto_fstat(efh_, &efstat);
      if (status < 0) {
         return false;
      }

      if (efstat.size == 0) {
         offset_ = 0;
      } else {
         return false;
      }
   }

   return true;
}

elasto_device::~elasto_device()
{
   if (efh_) {
      elasto_fclose(efh_);
      efh_ = NULL;
   }

   if (elasto_configstring_) {
      free(elasto_configstring_);
   }

   FreePoolMemory(virtual_filename_);
}

elasto_device::elasto_device()
{
   elasto_configstring_ = NULL;
   elasto_ps_file_ = NULL;
   basedir_ = NULL;
   insecure_http_ = false;
   efh_ = NULL;
   virtual_filename_ = GetPoolMemory(PM_FNAME);
   SetCap(CAP_ADJWRITESIZE); /* Adjust write size to min/max */
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" Device SD_IMP_EXP *backend_instantiate(JobControlRecord *jcr, int device_type)
{
   Device *dev = NULL;

   switch (device_type) {
   case B_ELASTO_DEV:
      dev = New(elasto_device);
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Request for unknown devicetype: %d\n"), device_type);
      break;
   }

   return dev;
}

extern "C" void SD_IMP_EXP flush_backend(void)
{
}
#endif
#endif /* HAVE_ELASTO */
