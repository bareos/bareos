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

export function buildLabelBarcodesCommand({
  storage,
  pool,
  drive,
  slots,
  encrypted = false,
}) {
  let cmd = `label barcodes storage="${storage}" pool="${pool}"`
  cmd += ` drive=${drive ?? 0}`

  if (encrypted) {
    cmd += ' encrypt'
  }

  const selectedSlots = String(slots ?? '').trim()
  if (selectedSlots.length > 0) {
    cmd += ` slots=${selectedSlots}`
  }

  return `${cmd} yes`
}

export function formatInDriveLabel(translate, drive) {
  const key = 'in drive {drive}'
  const label = translate(key, { drive })
  return label === key ? `in drive ${drive}` : label
}

export function shouldRefreshAutochangerTables({
  selectedStorage,
  slotsLoading,
  commandRunning,
}) {
  // Command output streams over a separate raw socket, so it must not pause
  // the regular slot/drives/import-export polling on the main connection.
  void commandRunning
  return Boolean(selectedStorage) && !slotsLoading
}
