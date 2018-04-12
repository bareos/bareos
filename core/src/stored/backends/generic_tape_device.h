/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * GENERIC Tape API device abstraction.
 *
 * Marco van Wieringen, June 2014
 */

#ifndef GENERIC_TAPE_DEVICE_H
#define GENERIC_TAPE_DEVICE_H

class generic_tape_device: public DEVICE {
public:
   generic_tape_device() {};
   virtual ~generic_tape_device() {};

   /*
    * Interface from DEVICE
    */
   virtual void open_device(DCR *dcr, int omode);
   virtual char *status_dev();
   virtual bool eod(DCR *dcr);
   virtual void set_ateof();
   virtual void set_ateot();
   virtual bool offline();
   virtual bool weof(int num);
   virtual bool fsf(int num);
   virtual bool bsf(int num);
   virtual bool fsr(int num);
   virtual bool bsr(int num);
   virtual bool load_dev();
   virtual void lock_door();
   virtual void unlock_door();
   virtual void clrerror(int func);
   virtual void set_os_device_parameters(DCR *dcr);
   virtual int32_t get_os_tape_file();
   virtual bool rewind(DCR *dcr);
   virtual bool update_pos(DCR *dcr);
   virtual bool reposition(DCR *dcr, uint32_t rfile, uint32_t rblock);
   virtual bool mount_backend(DCR *dcr, int timeout);
   virtual bool unmount_backend(DCR *dcr, int timeout);
   virtual int d_close(int);
   virtual int d_open(const char *pathname, int flags, int mode);
   virtual int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   virtual boffset_t d_lseek(DCR *dcr, boffset_t offset, int whence);
   virtual ssize_t d_read(int fd, void *buffer, size_t count);
   virtual ssize_t d_write(int fd, const void *buffer, size_t count);
   virtual bool d_truncate(DCR *dcr);
};
#endif /* GENERIC_TAPE_DEVICE_H */
