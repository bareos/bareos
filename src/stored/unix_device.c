/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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

#include "bareos.h"
#include "stored.h"
#include "unix_device.h"

int unix_device::d_open(const char *pathname, int flags)
{
   return ::open(pathname, flags);
}

ssize_t unix_device::d_read(int fd, void *buffer, size_t count)
{
   return ::read(fd, buffer, count);
}

ssize_t unix_device::d_write(int fd, const void *buffer, size_t count)
{
   return ::write(fd, buffer, count);
}

int unix_device::d_close(int fd)
{
   return ::close(fd);
}

int unix_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return ::ioctl(fd, request, op);
}

boffset_t unix_device::lseek(DCR *dcr, boffset_t offset, int whence)
{
   switch (dev_type) {
   case B_FILE_DEV:
      return ::lseek(m_fd, offset, whence);
   }
   return -1;
}

unix_device::~unix_device()
{
}

unix_device::unix_device()
{
   m_fd = -1;
}
