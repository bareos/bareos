/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2013 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, December 2013
 *
 * UNIX Tape API device abstraction.
 *
 * Stacking is the following:
 *
 *   unix_tape_device::
 *         |
 *         v
 * generic_tape_device::
 *         |
 *         v
 *      DEVICE::
 *
 */
/**
 * @file
 * UNIX Tape API device abstraction.
 */

#include "bareos.h"
#include "stored.h"
#include "generic_tape_device.h"
#include "unix_tape_device.h"

int unix_tape_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return ::ioctl(fd, request, op);
}

unix_tape_device::~unix_tape_device()
{
}

unix_tape_device::unix_tape_device()
{
   set_cap(CAP_ADJWRITESIZE); /* Adjust write size to min/max */
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" DEVICE SD_IMP_EXP *backend_instantiate(JCR *jcr, int device_type)
{
   DEVICE *dev = NULL;

   switch (device_type) {
   case B_TAPE_DEV:
      dev = New(unix_tape_device);
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
