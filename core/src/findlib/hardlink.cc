/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2023 Bareos GmbH & Co. KG

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
// Written by Marco van Wieringen, May 2014
/**
 * @file
 * Hardlinked files handling. The hardlinks are tracked using a htable.
 */

#include "include/bareos.h"
#include "find.h"
#include "findlib/hardlink.h"

// Lookup a inode/dev in the list of hardlinked files.
CurLink* lookup_hardlink(JobControlRecord* jcr,
                         FindFilesPacket* ff_pkt,
                         ino_t ino,
                         dev_t dev)
{
  if (!ff_pkt->linkhash) { return nullptr; }

  // Search link list of hard linked files

  if (auto found = ff_pkt->linkhash->find(Hardlink{dev, ino});
      found != ff_pkt->linkhash->end()) {
    return &found->second;
  }

  return nullptr;
}

CurLink* new_hardlink(JobControlRecord* jcr,
                      FindFilesPacket* ff_pkt,
                      char* fname,
                      ino_t ino,
                      dev_t dev)
{
  if (!ff_pkt->linkhash) { ff_pkt->linkhash = new LinkHash(10000); }

  auto [iter, inserted] = ff_pkt->linkhash->try_emplace(Hardlink{dev, ino});
  if (!inserted) { return nullptr; }
  CurLink& hl = iter->second;
  hl.name.assign(fname);
  hl.ino = ino;
  hl.dev = dev;

  hl.digest_stream = 0; /* Set later */
  hl.FileIndex = 0;     /* Set later */

  return &hl;
}

/**
 * When the current file is a hardlink, the backup code can compute
 * the checksum and store it into the CurLink structure.
 */
void FfPktSetLinkDigest(FindFilesPacket* ff_pkt,
                        int32_t digest_stream,
                        const char* digest,
                        uint32_t len)
{
  if (ff_pkt->linked && ff_pkt->linked->digest.empty()) { /* is a hardlink */
    ff_pkt->linked->digest = std::vector(digest, digest + len);
    ff_pkt->linked->digest_stream = digest_stream;
  }
}
