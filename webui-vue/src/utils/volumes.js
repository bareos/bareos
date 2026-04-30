/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

export function volumeEncryptionKey(volume) {
  if (!volume || typeof volume !== 'object') {
    return ''
  }

  return String(
    volume.encryptionkey
      ?? volume.EncryptionKey
      ?? volume.encrkey
      ?? volume.EncrKey
      ?? ''
  ).trim()
}

function volumeHasEncryptionKeyFlag(volume) {
  const flag = volume?.hasencryptionkey ?? volume?.HasEncryptionKey

  if (typeof flag === 'boolean') {
    return flag
  }

  if (typeof flag === 'number') {
    return flag !== 0
  }

  if (typeof flag === 'string') {
    const normalized = flag.trim().toLowerCase()
    return normalized === '1' || normalized === 'true' || normalized === 'yes'
  }

  return false
}

export function volumeHasEncryptionKey(volume) {
  return volumeHasEncryptionKeyFlag(volume)
    || volumeEncryptionKey(volume).length > 0
}
