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
  buildRestoreBvfsJobidsCommand,
  buildRestoreBackupOption,
  buildRestorePluginFilesetDetails,
  buildRestorePluginFilesetMap,
  canNavigateRestoreBrowser,
  buildRestoreSourceQuery,
  decorateRestoreBackupsWithPluginJobs,
  dedupeRestoreVersions,
  getRestoreVersionsLookupJobId,
  filterRestoreVersionsByJobids,
  filterRestoreSourceClients,
  getAllRestorePluginHints,
  getRestorePluginInfo,
  getRestorePluginHints,
  getRestoreBrowserPlaceholder,
  normaliseRestoreToggle,
  parseRestorePluginDefinition,
  pushRestoreBreadcrumb,
  resolveRestoreSourceClient,
  resolveRestoreBackupOption,
  resolveRestorePluginHintId,
  resolveRestoreSourceDirector,
  shouldShowRestorePluginOptions,
  truncateRestoreBreadcrumbs,
} from '../../src/utils/restore.js'

describe('restore browser placeholder', () => {
  it('prefers the error state', () => {
    expect(getRestoreBrowserPlaceholder({
      browserError: 'Failed to load file tree',
      loadingBrowser: true,
      hasSelectedJob: true,
    })).toBe('error')
  })

  it('shows loading while a selected job is still fetching', () => {
    expect(getRestoreBrowserPlaceholder({
      browserError: '',
      loadingBrowser: true,
      hasSelectedJob: true,
    })).toBe('loading')
  })

  it('falls back to the initial empty prompt otherwise', () => {
    expect(getRestoreBrowserPlaceholder({
      browserError: '',
      loadingBrowser: false,
      hasSelectedJob: false,
    })).toBe('empty')
  })

  it('allows navigation only when the current directory is interactive', () => {
    expect(canNavigateRestoreBrowser({
      browserReady: true,
      loadingBrowser: false,
    })).toBe(true)
    expect(canNavigateRestoreBrowser({
      browserReady: false,
      loadingBrowser: false,
    })).toBe(false)
    expect(canNavigateRestoreBrowser({
      browserReady: true,
      loadingBrowser: true,
    })).toBe(false)
  })

  it('builds the next breadcrumb level without mutating the current stack', () => {
    const stack = [{ label: '/', pathId: null }]

    expect(pushRestoreBreadcrumb(stack, {
      label: 'etc/',
      pathId: 17,
    })).toEqual([
      { label: '/', pathId: null },
      { label: 'etc/', pathId: 17 },
    ])
    expect(stack).toEqual([{ label: '/', pathId: null }])
  })

  it('truncates breadcrumbs back to the requested level', () => {
    expect(truncateRestoreBreadcrumbs([
      { label: '/', pathId: null },
      { label: 'etc/', pathId: 17 },
      { label: 'bareos/', pathId: 23 },
    ], 1)).toEqual([
      { label: '/', pathId: null },
      { label: 'etc/', pathId: 17 },
    ])
  })

  it('prefers an explicit director when resolving a queried source client', () => {
    expect(resolveRestoreSourceClient([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'bareos-fd', director: 'prod-b' },
    ], {
      clientName: 'bareos-fd',
      directorName: 'prod-b',
      currentDirector: 'prod-a',
    })).toEqual({
      name: 'bareos-fd',
      director: 'prod-b',
    })
  })

  it('falls back to the current director when no explicit director is given', () => {
    expect(resolveRestoreSourceClient([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'bareos-fd', director: 'prod-b' },
    ], {
      clientName: 'bareos-fd',
      currentDirector: 'prod-a',
    })).toEqual({
      name: 'bareos-fd',
      director: 'prod-a',
    })
  })

  it('rejects a queried source client when it does not belong to the explicit director', () => {
    expect(resolveRestoreSourceClient([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'db-fd', director: 'prod-b' },
    ], {
      clientName: 'bareos-fd',
      directorName: 'prod-b',
      currentDirector: 'prod-a',
    })).toBeNull()
  })

  it('keeps only source directors that are still active', () => {
    expect(resolveRestoreSourceDirector(['prod-a', 'prod-b'], 'prod-b')).toBe('prod-b')
    expect(resolveRestoreSourceDirector(['prod-a', 'prod-b'], 'prod-c')).toBe('')
    expect(resolveRestoreSourceDirector(['prod-a'], '')).toBe('')
  })

  it('filters source clients to the selected common-mode director', () => {
    expect(filterRestoreSourceClients([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'db-fd', director: 'prod-b' },
      { name: 'web-fd', director: 'prod-a' },
    ], 'prod-a')).toEqual([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'web-fd', director: 'prod-a' },
    ])
  })

  it('keeps all source clients when no common-mode director is selected', () => {
    const clients = [
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'db-fd', director: 'prod-b' },
    ]
    expect(filterRestoreSourceClients(clients)).toEqual(clients)
  })

  it('writes the selected restore source back into the query', () => {
    expect(buildRestoreSourceQuery({
      foo: 'bar',
      jobid: 'old',
    }, {
      clientName: 'bareos-fd',
      directorName: 'prod-a',
      jobid: 42,
      mergeJobs: true,
      mergeFilesets: false,
    })).toEqual({
      foo: 'bar',
      client: 'bareos-fd',
      director: 'prod-a',
      jobid: '42',
      mergejobs: '1',
      mergefilesets: '0',
    })
  })

  it('builds compact restore backup selector options with level metadata', () => {
    expect(buildRestoreBackupOption({
      jobid: 42,
      name: 'backup-bareos-fd',
      level: 'I',
      starttime: '2026-06-09 10:00:00',
      jobbytes: 2048,
      jobfiles: 12,
    }, {
      formatBytes: value => `${value} B`,
    })).toEqual({
      value: 42,
      label: 'backup-bareos-fd',
      name: 'backup-bareos-fd',
      jobid: 42,
      level: 'I',
      levelCode: 'I',
      starttime: '2026-06-09 10:00:00',
      secondary: '#42 · 2026-06-09 10:00:00 · 2048 B · 12 files',
    })
  })

  it('omits zero-sized secondary restore backup metrics', () => {
    expect(buildRestoreBackupOption({
      jobid: 7,
      name: 'FullBackup',
      level: 'F',
      starttime: '2026-06-09 09:00:00',
      jobbytes: 0,
      jobfiles: 0,
    })).toEqual({
      value: 7,
      label: 'FullBackup',
      name: 'FullBackup',
      jobid: 7,
      level: 'F',
      levelCode: 'F',
      starttime: '2026-06-09 09:00:00',
      secondary: '#7 · 2026-06-09 09:00:00',
    })
  })

  it('resolves the selected restore backup option across string and number job ids', () => {
    const options = [
      buildRestoreBackupOption({ jobid: 7, name: 'FullBackup', level: 'F' }),
      buildRestoreBackupOption({ jobid: 42, name: 'IncBackup', level: 'I' }),
    ]

    expect(resolveRestoreBackupOption(options, '42')).toEqual(options[1])
    expect(resolveRestoreBackupOption(options, 7)).toEqual(options[0])
    expect(resolveRestoreBackupOption(options, '999')).toBeNull()
  })

  it('normalizes restore merge toggle query values', () => {
    expect(normaliseRestoreToggle('1')).toBe(true)
    expect(normaliseRestoreToggle('false', true)).toBe(false)
    expect(normaliseRestoreToggle(undefined, true)).toBe(true)
  })

  it('builds the BVFS jobids command only when related jobs are enabled', () => {
    expect(buildRestoreBvfsJobidsCommand(42, {
      mergeJobs: true,
      mergeFilesets: true,
    })).toBe('.bvfs_get_jobids jobid=42 all')
    expect(buildRestoreBvfsJobidsCommand(42, {
      mergeJobs: true,
      mergeFilesets: false,
    })).toBe('.bvfs_get_jobids jobid=42')
    expect(buildRestoreBvfsJobidsCommand(42, {
      mergeJobs: false,
      mergeFilesets: true,
    })).toBe('')
  })

  it('clears restore source query fields when no source is selected', () => {
    expect(buildRestoreSourceQuery({
      foo: 'bar',
      client: 'bareos-fd',
      director: 'prod-a',
      jobid: '42',
    }, {})).toEqual({
      foo: 'bar',
    })
  })

  it('detects filesets that require restore plugin options', () => {
    expect(buildRestorePluginFilesetMap({
      PluginFS: {
        include: [{ plugin: 'python:module_path=/tmp/plugin' }],
      },
      PlainFS: {
        include: [{ file: '/etc' }],
      },
    })).toEqual(new Map([
      ['PluginFS', true],
      ['PlainFS', false],
    ]))
  })

  it('extracts plugin names and option keys from backup definitions', () => {
    expect(parseRestorePluginDefinition(
      'python:module_path=/tmp/plugin:module_name=test:restore_file=/tmp/out'
    )).toEqual({
      raw: 'python:module_path=/tmp/plugin:module_name=test:restore_file=/tmp/out',
      pluginName: 'python',
      optionKeys: ['module_path', 'module_name', 'restore_file'],
    })
  })

  it('keeps detailed plugin fileset metadata from show fileset', () => {
    expect(buildRestorePluginFilesetDetails({
      PluginFS: {
        description: 'Plugin backup',
        include: [{ plugin: ['bpipe:file=/data:reader=cat /tmp/in:writer=cat >/tmp/out'] }],
      },
      PlainFS: {
        include: [{ file: '/etc' }],
      },
    })).toEqual(new Map([
      ['PluginFS', {
        filesetName: 'PluginFS',
        description: 'Plugin backup',
        hasPlugin: true,
        definitions: [{
          raw: 'bpipe:file=/data:reader=cat /tmp/in:writer=cat >/tmp/out',
          pluginName: 'bpipe',
          optionKeys: ['file', 'reader', 'writer'],
        }],
        pluginNames: ['bpipe'],
        optionKeys: ['file', 'reader', 'writer'],
      }],
      ['PlainFS', {
        filesetName: 'PlainFS',
        description: '',
        hasPlugin: false,
        definitions: [],
        pluginNames: [],
        optionKeys: [],
      }],
    ]))
  })

  it('extracts plugin fileset metadata from list filesets output', () => {
    expect(buildRestorePluginFilesetDetails([
      {
        fileset: 'PluginOptionsTest',
        filesettext: `FileSet {\n  Name = "PluginOptionsTest"\n  Description = "Plugin backup"\n  Include {\n    Plugin = "bpipe"\n             ":file=/plugin-options-demo.txt"\n             ":reader=bash /tmp/readprogram"\n             ":writer=bash /tmp/writeprogram"\n  }\n}\n`,
      },
      {
        fileset: 'PlainFS',
        filesettext: 'FileSet {\n  Name = "PlainFS"\n}\n',
      },
    ])).toEqual(new Map([
      ['PluginOptionsTest', {
        filesetName: 'PluginOptionsTest',
        description: 'Plugin backup',
        hasPlugin: true,
        definitions: [{
          raw: 'bpipe:file=/plugin-options-demo.txt:reader=bash /tmp/readprogram:writer=bash /tmp/writeprogram',
          pluginName: 'bpipe',
          optionKeys: ['file', 'reader', 'writer'],
        }],
        pluginNames: ['bpipe'],
        optionKeys: ['file', 'reader', 'writer'],
      }],
      ['PlainFS', {
        filesetName: 'PlainFS',
        description: '',
        hasPlugin: false,
        definitions: [],
        pluginNames: [],
        optionKeys: [],
      }],
    ]))
  })

  it('marks backups as plugin jobs from their fileset metadata', () => {
    expect(decorateRestoreBackupsWithPluginJobs([
      { jobid: 10, fileset: 'PluginFS' },
      { jobid: 11, fileset: 'PlainFS' },
    ], new Map([
      ['PluginFS', true],
      ['PlainFS', false],
    ]))).toEqual([
      { jobid: 10, fileset: 'PluginFS', pluginjob: true },
      { jobid: 11, fileset: 'PlainFS', pluginjob: false },
    ])
  })

  it('shows plugin options for the selected plugin backup', () => {
    expect(shouldShowRestorePluginOptions({
      backups: [
        { jobid: 10, pluginjob: true },
        { jobid: 11, pluginjob: false },
      ],
      selectedJobId: 10,
      mergedJobids: '',
      mergeJobs: false,
    })).toBe(true)
  })

  it('shows plugin options when merged restore jobs include a plugin backup', () => {
    expect(shouldShowRestorePluginOptions({
      backups: [
        { jobid: 10, pluginjob: false },
        { jobid: 11, pluginjob: true },
      ],
      selectedJobId: 10,
      mergedJobids: '10,11',
      mergeJobs: true,
    })).toBe(true)
  })

  it('describes the plugin restore context for the selected backup', () => {
    expect(getRestorePluginInfo({
      backups: [
        { jobid: 10, fileset: 'PluginFS', pluginjob: true, name: 'backup-plugin' },
        { jobid: 11, fileset: 'PlainFS', pluginjob: false, name: 'backup-plain' },
      ],
      pluginFilesets: new Map([
        ['PluginFS', {
          filesetName: 'PluginFS',
          description: 'Plugin backup',
          hasPlugin: true,
          definitions: [{
            raw: 'python:module_path=/tmp/plugin:module_name=test',
            pluginName: 'python',
            optionKeys: ['module_path', 'module_name'],
          }],
          pluginNames: ['python'],
          optionKeys: ['module_path', 'module_name'],
        }],
      ]),
      selectedJobId: 10,
      mergedJobids: '',
      mergeJobs: false,
    })).toEqual({
      backups: [
        { jobid: 10, fileset: 'PluginFS', pluginjob: true, name: 'backup-plugin' },
      ],
      filesetNames: ['PluginFS'],
      pluginNames: ['python'],
      optionKeys: ['module_path', 'module_name'],
      definitions: [{
        raw: 'python:module_path=/tmp/plugin:module_name=test',
        pluginName: 'python',
        optionKeys: ['module_path', 'module_name'],
        filesetName: 'PluginFS',
        filesetDescription: 'Plugin backup',
      }],
      usesMergedJobs: false,
    })
  })

  it('resolves direct plugin names to static hint entries', () => {
    expect(resolveRestorePluginHintId({
      raw: 'bpipe:file=/data:reader=cat /tmp/in:writer=cat >/tmp/out',
      pluginName: 'bpipe',
      optionKeys: ['file', 'reader', 'writer'],
    })).toBe('bpipe')
  })

  it('resolves python module names to specific plugin hint entries', () => {
    expect(resolveRestorePluginHintId({
      raw: 'python:module_path=/tmp/plugins:module_name=bareos-fd-vmware:config_file=/tmp/vmware.ini',
      pluginName: 'python',
      optionKeys: ['module_path', 'module_name', 'config_file'],
    })).toBe('vmware')
  })

  it('builds static plugin hints for the selected restore plugin', () => {
    expect(getRestorePluginHints({
      definitions: [{
        raw: 'python:module_path=/tmp/plugins:module_name=bareos-fd-vmware:config_file=/tmp/vmware.ini',
        pluginName: 'python',
        optionKeys: ['module_path', 'module_name', 'config_file'],
      }],
    })).toEqual([
      expect.objectContaining({
        id: 'vmware',
        displayName: 'VMware',
        example: 'vcserver=...:vcuser=...',
        manualUrl: 'https://docs.bareos.org/master/TasksAndConcepts/Plugins.html#vmwareplugin',
      }),
    ])
  })

  it('keeps documentation descriptions on static plugin hints', () => {
    expect(getRestorePluginHints({
      definitions: [{
        raw: 'python:module_name=bareos-fd-libcloud:config_file=/etc/bareos/libcloud.ini',
        pluginName: 'python',
        optionKeys: ['module_name', 'config_file'],
      }],
    })[0]?.options).toContainEqual(expect.objectContaining({
      name: 'buckets_exclude',
      description: 'Comma-separated list of buckets to exclude.',
      source: 'plugin-doc',
    }))
  })

  it('marks contrib plugin hints as unsupported contrib entries', () => {
    expect(getRestorePluginHints({
      definitions: [{
        raw: 'python:module_name=bareos_tasks.pgsql:databases=db001,db002',
        pluginName: 'python',
        optionKeys: ['module_name', 'databases'],
      }],
    })).toEqual([
      expect.objectContaining({
        id: 'tasksPgsql',
        displayName: 'Tasks PostgreSQL',
        supportLevel: 'contrib',
      }),
    ])
  })

  it('resolves bareos_tasks module names to documented task plugin hints', () => {
    expect(resolveRestorePluginHintId({
      raw: 'python:module_name=bareos_tasks.pgsql:databases=db001,db002',
      pluginName: 'python',
      optionKeys: ['module_name', 'databases'],
    })).toBe('tasksPgsql')
  })

  it('resolves qumulo plugin names to the documented third-party hint entry', () => {
    expect(getRestorePluginHints({
      definitions: [{
        raw: 'qumulo:cluster=qumulo.example.test:path=/data',
        pluginName: 'qumulo',
        optionKeys: ['cluster', 'path'],
      }],
    })).toContainEqual(expect.objectContaining({
      id: 'qumulo',
      displayName: 'Qumulo by Yuzuy',
      supportLevel: 'third-party',
      manualUrl: 'https://docs.bareos.org/master/TasksAndConcepts/Plugins.html#section-yuzuy-qumulo-plugin',
    }))
  })

  it('lists all known plugin hints in display-name order', () => {
    const hints = getAllRestorePluginHints()

    expect(hints[0]).toEqual(expect.objectContaining({
      displayName: 'BPipe',
      example: 'file=...:reader=...',
      manualUrl: 'https://docs.bareos.org/master/TasksAndConcepts/Plugins.html#bpipe',
    }))
    expect(hints.at(-1)).toEqual(expect.objectContaining({
      displayName: 'VMware',
      example: 'vcserver=...:vcuser=...',
      manualUrl: 'https://docs.bareos.org/master/TasksAndConcepts/Plugins.html#vmwareplugin',
    }))
  })

  it('includes related plugin jobs when merged restore jobsets are enabled', () => {
    expect(getRestorePluginInfo({
      backups: [
        { jobid: 10, fileset: 'PlainFS', pluginjob: false, name: 'backup-plain' },
        { jobid: 11, fileset: 'PluginFS', pluginjob: true, name: 'backup-plugin' },
      ],
      pluginFilesets: new Map([
        ['PluginFS', {
          filesetName: 'PluginFS',
          description: '',
          hasPlugin: true,
          definitions: [{
            raw: 'bpipe:file=/data:reader=cat /tmp/in:writer=cat >/tmp/out',
            pluginName: 'bpipe',
            optionKeys: ['file', 'reader', 'writer'],
          }],
          pluginNames: ['bpipe'],
          optionKeys: ['file', 'reader', 'writer'],
        }],
      ]),
      selectedJobId: 10,
      mergedJobids: '10,11',
      mergeJobs: true,
    })?.usesMergedJobs).toBe(true)
  })

  it('deduplicates restore versions by file id while preserving order', () => {
    expect(dedupeRestoreVersions([
      { fileid: 10, jobid: 1 },
      { fileid: 10, jobid: 1 },
      { fileid: 12, jobid: 2 },
    ])).toEqual([
      { fileid: 10, jobid: 1 },
      { fileid: 12, jobid: 2 },
    ])
  })

  it('falls back to version metadata when file ids are missing', () => {
    expect(dedupeRestoreVersions([
      { jobid: 1, stat: { mtime: 100, size: 20 }, volumename: 'Vol001', md5: 'abc' },
      { jobid: 1, stat: { mtime: 100, size: 20 }, volumename: 'Vol001', md5: 'abc' },
      { jobid: 1, stat: { mtime: 101, size: 20 }, volumename: 'Vol001', md5: 'abc' },
    ])).toEqual([
      { jobid: 1, stat: { mtime: 100, size: 20 }, volumename: 'Vol001', md5: 'abc' },
      { jobid: 1, stat: { mtime: 101, size: 20 }, volumename: 'Vol001', md5: 'abc' },
    ])
  })

  it('filters restore versions to the selected restore job set', () => {
    expect(filterRestoreVersionsByJobids([
      { fileid: 10, jobid: 1 },
      { fileid: 11, jobid: 2 },
      { fileid: 12, jobid: 3 },
    ], '1,3')).toEqual([
      { fileid: 10, jobid: 1 },
      { fileid: 12, jobid: 3 },
    ])
  })

  it('keeps deduplicated restore versions unchanged when no job filter is available', () => {
    expect(filterRestoreVersionsByJobids([
      { fileid: 10, jobid: 1 },
      { fileid: 10, jobid: 1 },
      { fileid: 12, jobid: 2 },
    ], '')).toEqual([
      { fileid: 10, jobid: 1 },
      { fileid: 12, jobid: 2 },
    ])
  })

  it('uses the selected job id for version lookups when available', () => {
    expect(getRestoreVersionsLookupJobId(42, '1,2,3')).toBe('42')
  })

  it('falls back to the first merged job id for version lookups', () => {
    expect(getRestoreVersionsLookupJobId(null, ' 10,11,12')).toBe('10')
  })

  it('uses zero for version lookups when no job ids are available', () => {
    expect(getRestoreVersionsLookupJobId('', '')).toBe('0')
  })
})
