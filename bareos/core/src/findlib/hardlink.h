/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef BAREOS_FINDLIB_HARDLINK_H_
#define BAREOS_FINDLIB_HARDLINK_H_

CurLink* lookup_hardlink(JobControlRecord* jcr,
                         FindFilesPacket* ff_pkt,
                         ino_t ino,
                         dev_t dev);
CurLink* new_hardlink(JobControlRecord* jcr,
                      FindFilesPacket* ff_pkt,
                      char* fname,
                      ino_t ino,
                      dev_t dev);
void FfPktSetLinkDigest(FindFilesPacket* ff_pkt,
                        int32_t digest_stream,
                        const char* digest,
                        uint32_t len);

#endif  // BAREOS_FINDLIB_HARDLINK_H_
