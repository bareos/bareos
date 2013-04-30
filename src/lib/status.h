/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.

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
 *  Status packet definition that is used in both the SD and FD. It 
 *    permits Win32 to call output_status() and get the output back
 *    at the callback address line by line, and for Linux code,
 *    the output can be sent directly to a BSOCK.
 *
 *     Kern Sibbald, March MMVII
 *
 *   Version $Id: $
 *
 */

#ifndef __STATUS_H_
#define __STATUS_H_

/*
 * Packet to send to output_status()
 */
class STATUS_PKT {
public:
  BSOCK *bs;                       /* used on Unix machines */
  void *context;                   /* Win32 */
  void (*callback)(const char *msg, int len, void *context);  /* Win32 */
  bool api;                        /* set if we want API output */

  /* Methods */
  STATUS_PKT() { memset(this, 0, sizeof(STATUS_PKT)); };
  ~STATUS_PKT() { };
};

extern void output_status(STATUS_PKT *sp);


#endif
