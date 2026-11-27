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


const FILE_TYPE_CHARS = {
  0o040000: 'd',
  0o120000: 'l',
  0o060000: 'b',
  0o020000: 'c',
  0o010000: 'p',
  0o140000: 's',
}

export function formatMtime(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  const yyyy = d.getFullYear()
  const mm   = (d.getMonth() + 1).toString().padStart(2, '0')
  const dd   = d.getDate().toString().padStart(2, '0')
  const hh   = d.getHours().toString().padStart(2, '0')
  const min  = d.getMinutes().toString().padStart(2, '0')
  const ss   = d.getSeconds().toString().padStart(2, '0')
  return `${yyyy}-${mm}-${dd} ${hh}:${min}:${ss}`
}

// ls -l style mode string, e.g. "drwxr-xr-x".
export function formatMode(mode) {
  if (!mode) return ''
  let type = '-'
  for (const [mask, ch] of Object.entries(FILE_TYPE_CHARS)) {
    if ((mode & 0o170000) === Number(mask)) { type = ch; break }
  }
  const bits = 'rwxrwxrwx'
  let perms = ''
  for (let i = 8; i >= 0; i--) {
    perms += (mode >> i) & 1 ? bits[8 - i] : '-'
  }
  // setuid/setgid/sticky
  if (mode & 0o4000) perms = perms.slice(0, 2) + (perms[2] === 'x' ? 's' : 'S') + perms.slice(3)
  if (mode & 0o2000) perms = perms.slice(0, 5) + (perms[5] === 'x' ? 's' : 'S') + perms.slice(6)
  if (mode & 0o1000) perms = perms.slice(0, 8) + (perms[8] === 'x' ? 't' : 'T')
  return type + perms
}
