/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

const JOB_TYPE_INFO = {
  B: { badge: 'B', color: 'primary', labelKey: 'Backup' },
  A: { badge: 'A', color: 'blue-grey', labelKey: 'Archive' },
  V: { badge: 'V', color: 'purple', labelKey: 'Verify' },
  R: { badge: 'R', color: 'teal', labelKey: 'Restore' },
  D: { badge: 'D', color: 'orange', labelKey: 'Admin' },
  C: { badge: 'C', color: 'cyan-8', labelKey: 'Job Copy' },
  c: { badge: 'C', color: 'cyan', labelKey: 'Copy' },
  M: { badge: 'M', color: 'deep-purple', labelKey: 'Migrated Job' },
  g: { badge: 'M', color: 'deep-purple', labelKey: 'Migration' },
  O: { badge: 'O', color: 'indigo', labelKey: 'Consolidate' },
  S: { badge: 'S', color: 'brown', labelKey: 'Scan' },
  U: { badge: 'U', color: 'grey-7', labelKey: 'Console' },
  I: { badge: 'I', color: 'grey-8', labelKey: 'System' },
}

const JOB_TYPE_CODE_BY_NAME = {
  Backup: 'B',
  Restore: 'R',
  Verify: 'V',
  Admin: 'D',
  Archive: 'A',
  'Job Copy': 'C',
  Copy: 'c',
  Migrate: 'g',
  Migration: 'g',
  'Migrated Job': 'M',
  Consolidate: 'O',
  Console: 'U',
  System: 'I',
  Scan: 'S',
  Diagnostic: 'D',
}

export function resolveJobTypeInfo(type) {
  if (typeof type !== 'string' || !type) {
    return { badge: '', color: 'grey', labelKey: '' }
  }

  const code = JOB_TYPE_CODE_BY_NAME[type] ?? type
  const info = JOB_TYPE_INFO[code]
  if (info) {
    return info
  }

  return { badge: type, color: 'grey', labelKey: type }
}

export function resolveJobTypeCode(type) {
  if (typeof type !== 'string' || !type) {
    return null
  }

  return JOB_TYPE_CODE_BY_NAME[type] ?? (JOB_TYPE_INFO[type] ? type : null)
}
