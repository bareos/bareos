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

import { describe, expect, it } from 'vitest'
import {
  normaliseClient,
  normaliseJob,
  normaliseVolume,
} from '../../src/composables/useDirectorFetch.js'

describe('director data normalisers', () => {
  it('normalises job records from catalog-style keys', () => {
    expect(normaliseJob({
      jobid: '42',
      name: 'BackupClient1',
      clientname: 'bareos-fd',
      jobtype: 'B',
      joblevel: 'F',
      jobstatus: 'T',
      starttime: '2026-03-23 08:00:01',
      realendtime: '2026-03-23 08:12:44',
      duration: '0:12:43',
      jobfiles: '48231',
      jobbytes: '2147483648',
      joberrors: '0',
    })).toEqual({
      id: 42,
      name: 'BackupClient1',
      client: 'bareos-fd',
      type: 'B',
      level: 'F',
      status: 'T',
      starttime: '2026-03-23 08:00:01',
      endtime: '2026-03-23 08:12:44',
      duration: '0:12:43',
      files: 48231,
      bytes: 2147483648,
      errors: 0,
    })
  })

  it('extracts client OS and version details from uname', () => {
    expect(normaliseClient({
      clientid: 7,
      name: 'bareos-fd',
      uname: '23.0.0 (01Jan24) Debian GNU/Linux 12 (bookworm),x86_64-pc-linux-gnu',
      enabled: '1',
      fdport: '9102',
    })).toMatchObject({
      clientid: 7,
      name: 'bareos-fd',
      version: '23.0.0',
      buildDate: '01Jan24',
      enabled: true,
      os: 'linux',
      osInfo: 'Debian GNU/Linux 12 (bookworm)',
      arch: 'x86_64',
      port: 9102,
    })
  })

  it('normalises volume records from either naming convention', () => {
    expect(normaliseVolume({
      volume: 'Full-0001',
      Pool: 'Full',
      storagename: 'File',
      MediaType: 'File',
      VolStatus: 'Full',
      VolBytes: '42',
      VolFiles: '3',
      MaxVolBytes: '84',
      LastWritten: '2026-03-23 09:00:00',
      inchanger: '1',
      VolRetention: '365 days',
      Slot: '8',
      enabled: true,
    })).toEqual({
      volumename: 'Full-0001',
      pool: 'Full',
      storage: 'File',
      mediatype: 'File',
      volstatus: 'Full',
      volbytes: 42,
      volfiles: 3,
      maxvolbytes: 84,
      lastwritten: '2026-03-23 09:00:00',
      inchanger: true,
      retention: '365 days',
      slot: 8,
      enabled: true,
    })
  })
})
