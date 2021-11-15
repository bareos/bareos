/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2021 Bareos GmbH & Co. KG

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
// Kern Sibbald, March MMVII
/**
 * @file
 * Status packet definition that is used in both the SD and FD. It
 * permits Win32 to call OutputStatus() and get the output back
 * at the callback address line by line, and for Linux code,
 * the output can be sent directly to a BareosSocket.
 */

#ifndef BAREOS_LIB_STATUS_PACKET_H_
#define BAREOS_LIB_STATUS_PACKET_H_

#include <include/bareos.h>
#include <string>

class BareosSocket;

// Packet to send to OutputStatus()
class StatusPacket {
 public:
  StatusPacket() = default;

  virtual ~StatusPacket() = default;
  BareosSocket* bs = nullptr; /* used on Unix machines */
  void* context = nullptr;    /* Win32 */
  void (*callback)(const char* msg, int len, void* context)
      = nullptr;    /* Win32 */
  bool api = false; /* set if we want API output */
  void send(const char* msg, int len);
  void send(PoolMem& msg, int len) { send(msg.c_str(), len); }
  void send(const std::string& msg) { send(msg.c_str(), msg.length()); }
};
#endif  // BAREOS_LIB_STATUS_PACKET_H_
