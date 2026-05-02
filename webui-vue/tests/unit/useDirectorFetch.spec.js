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
  directorCommandCategory,
  directorCommandAllowed,
  directorCollection,
  extractStorageRuntime,
  missingDirectorCommands,
  normaliseJobHistory,
  normaliseDirectorCommandPermissions,
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

  it('extracts storage runtime metrics from director status rows', () => {
    expect(extractStorageRuntime({
      sd_sample_time: '1746114000',
      sd_files: '17',
      sd_bytes: '8192',
      sd_average_bytes_per_second: '2048',
      sd_last_bytes_per_second: '1024',
      sd_spooling: '1',
      sd_despooling: '0',
      sd_despool_wait: '1',
      sd_current_file: '/srv/data/example.txt',
      sd_write_device: 'File',
      sd_write_volume: 'Vol001',
      sd_pool: 'Full',
    })).toEqual({
      sampleTime: 1746114000,
      jobStatus: null,
      files: 17,
      bytes: 8192,
      averageBytesPerSecond: 2048,
      lastBytesPerSecond: 1024,
      spooling: true,
      despooling: false,
      despoolWait: true,
      currentFile: '/srv/data/example.txt',
      readDevice: '',
      writeDevice: 'File',
      readVolume: '',
      writeVolume: 'Vol001',
      pool: 'Full',
    })
  })

  it('normalises director job history events', () => {
    expect(normaliseJobHistory({
      available: true,
      events: [
        {
          timestamp: '1746114000',
          source: 'storage-daemon',
          previous_status: 'S',
          previous_status_long: 'Waiting on Storage daemon',
          new_status: 'R',
          new_status_long: 'Running',
          director_status: 'R',
          director_status_long: 'Running',
          storage_daemon_status: 'R',
          storage_daemon_status_long: 'Running',
          file_daemon_status: 'f',
          file_daemon_status_long: 'Waiting on File daemon',
          job_files: '17',
          job_bytes: '8192',
          current_file: '/srv/data/example.txt',
        },
      ],
    })).toEqual({
      available: true,
      events: [
        {
          timestamp: 1746114000,
          source: 'storage-daemon',
          previousStatus: 'S',
          previousStatusLong: 'Waiting on Storage daemon',
          newStatus: 'R',
          newStatusLong: 'Running',
          directorStatus: 'R',
          directorStatusLong: 'Running',
          storageDaemonStatus: 'R',
          storageDaemonStatusLong: 'Running',
          fileDaemonStatus: 'f',
          fileDaemonStatusLong: 'Waiting on File daemon',
          jobFiles: 17,
          jobBytes: 8192,
          currentFile: '/srv/data/example.txt',
        },
      ],
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
      HasEncryptionKey: '1',
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
      encryptionkey: '',
      hasencryptionkey: '1',
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

  it('converts keyed director collections to arrays', () => {
    expect(directorCollection({
      one: { name: 'alpha' },
      two: { name: 'beta' },
    })).toEqual([
      { name: 'alpha' },
      { name: 'beta' },
    ])
  })

  it('flattens nested director collections', () => {
    expect(directorCollection({
      full: [{ name: 'Full-0001' }],
      incr: [{ name: 'Incr-0001' }, { name: 'Incr-0002' }],
    })).toEqual([
      { name: 'Full-0001' },
      { name: 'Incr-0001' },
      { name: 'Incr-0002' },
    ])
  })

  it('detects allowed director commands from help output', () => {
    expect(directorCommandAllowed({
      delete: { permission: false },
      prune: { permission: true },
    }, 'prune')).toBe(true)
    expect(directorCommandAllowed({
      delete: { permission: false },
    }, 'delete')).toBe(false)
  })

  it('lists missing required director commands', () => {
    expect(missingDirectorCommands({
      prune: { permission: true },
      delete: { permission: false },
    }, ['delete', 'prune', 'list'])).toEqual(['delete', 'list'])
  })

  it('groups director commands into display categories', () => {
    expect(directorCommandCategory('delete')).toBe('Commands')
    expect(directorCommandCategory('.help')).toBe('Dot commands')
    expect(directorCommandCategory('.bvfs_restore')).toBe('BVFS')
  })

  it('normalises command permission records for display', () => {
    expect(normaliseDirectorCommandPermissions({
      '.help': {
        description: 'Print parsable information about a command',
        arguments: '[ all | item=cmd ]',
        permission: true,
      },
      delete: {
        description: 'Delete catalog records',
        arguments: 'jobid=<jobid>',
        permission: false,
      },
    })).toEqual([
      {
        command: '.help',
        description: 'Print parsable information about a command',
        arguments: '[ all | item=cmd ]',
        permission: true,
        category: 'Dot commands',
      },
      {
        command: 'delete',
        description: 'Delete catalog records',
        arguments: 'jobid=<jobid>',
        permission: false,
        category: 'Commands',
      },
    ])
  })
})
