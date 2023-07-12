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
CurLink* lookup_hardlink(LinkHash* table,
                         ino_t ino,
                         dev_t dev)
{
  if (!table) { return nullptr; }

  // Search link list of hard linked files

  if (auto found = table->find(Hardlink{dev, ino});
      found != table->end()) {
    return &found->second;
  }

  return nullptr;
}

CurLink* new_hardlink(LinkHash*& table,
                      char* fname,
                      ino_t ino,
                      dev_t dev)
{
  if (!table) { table = new LinkHash(10000); }

  auto [iter, inserted] = table->try_emplace(Hardlink{dev, ino}, fname);
  if (!inserted) { return nullptr; }
  return &iter->second;
}

/**
 * When the current file is a hardlink, the backup code can compute
 * the checksum and store it into the CurLink structure.
 */
void FfPktSetLinkDigest(CurLink* link,
                        int32_t digest_stream,
                        const char* digest,
                        uint32_t len)
{
  if (link) { /* is a hardlink */
    link->set_digest(digest_stream, digest, len);
  }
}

void CurLink::set_digest(int32_t new_digest_stream,
                         const char* new_digest,
                         uint32_t len)
{
  if (digest.empty()) {
    digest.assign(new_digest, new_digest + len);
    digest_stream = new_digest_stream;
  }
}
