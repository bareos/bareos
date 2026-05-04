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

const JOB_LEVEL_INFO = {
  F: { badge: 'F', color: 'primary', labelKey: 'Full' },
  I: { badge: 'I', color: 'teal', labelKey: 'Incremental' },
  D: { badge: 'D', color: 'orange', labelKey: 'Differential' },
  V: { badge: 'V', color: 'purple', labelKey: 'Virtual Full' },
  B: { badge: 'B', color: 'grey', labelKey: 'Base' },
}

const JOB_LEVEL_CODE_BY_NAME = {
  full: 'F',
  incremental: 'I',
  increme: 'I',
  incr: 'I',
  differential: 'D',
  differe: 'D',
  diff: 'D',
  'virtualfull': 'V',
  virtual: 'V',
  vful: 'V',
  base: 'B',
}

export function resolveJobLevelCode(level) {
  if (typeof level !== 'string' || !level) {
    return null
  }

  if (JOB_LEVEL_INFO[level]) {
    return level
  }

  const normalized = level.toLowerCase().replace(/\s+/g, '')
  return JOB_LEVEL_CODE_BY_NAME[normalized] ?? null
}

export function resolveJobLevelInfo(level) {
  const code = resolveJobLevelCode(level)
  if (code) {
    return JOB_LEVEL_INFO[code]
  }

  return {
    badge: typeof level === 'string' ? level : '',
    color: 'grey',
    labelKey: typeof level === 'string' ? level : '',
  }
}
