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
#include "win32_file_device.h"

int win32_file_device::d_open(const char *pathname, int flags)
{
   return ::open(pathname, flags);
}

ssize_t win32_file_device::d_read(int fd, void *buffer, size_t count)
{
   return ::read(fd, buffer, count);
}

ssize_t win32_file_device::d_write(int fd, const void *buffer, size_t count)
{
   return ::write(fd, buffer, count);
}

int win32_file_device::d_close(int fd)
{
   return ::close(fd);
}

int win32_file_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t win32_file_device::lseek(DCR *dcr, boffset_t offset, int whence)
{
   return ::_lseeki64(m_fd, (__int64)offset, whence);
}

win32_file_device::~win32_file_device()
{
}

win32_file_device::win32_file_device()
{
   m_fd = -1;
}
