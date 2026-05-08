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

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import {
  buildDefaultSetupBootstrapUrl,
  DEFAULT_CATALOG_BACKUP_JOB_NAME,
  DEFAULT_CATALOG_NAME,
  DEFAULT_DIRECTOR_STORAGE_NAME,
  DEFAULT_JOBDEFS_NAME,
  DEFAULT_MONITOR_CONSOLE_NAME,
  DEFAULT_PRIMARY_BACKUP_JOB_NAME,
  DEFAULT_RESTORE_JOB_NAME,
  DEFAULT_STORAGE_DEVICE_NAME,
  DEFAULT_WEBUI_READONLY_PROFILE,
  DEFAULT_WEBUI_ADMIN_PROFILE,
  DEFAULT_OPERATOR_PROFILE,
  buildStorageBootstrapCommand,
  buildDefaultDaemonAddress,
  deriveSetupDaemonBaseName,
  deriveSetupDaemonNames,
  generateSetupPassword,
  buildInitialSetupRequests,
  useSetupStore,
} from '../../src/stores/setup.js'

function jsonResponse(payload, status = 200) {
  return {
    ok: status >= 200 && status < 300,
    status,
    text: vi.fn().mockResolvedValue(JSON.stringify(payload)),
  }
}

describe('setup store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    vi.unstubAllGlobals()
  })

  it('marks the webui as needing setup when no deployments exist', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(
      jsonResponse({ deployments: [] })
    ))

    const setup = useSetupStore()

    await expect(setup.refresh()).resolves.toEqual([])
    expect(fetch).toHaveBeenCalledWith('/api/bconfig/v1/deployments', expect.objectContaining({
      method: 'GET',
      cache: 'no-store',
    }))
    expect(setup.mode).toBe('needs-setup')
    expect(setup.needsSetup).toBe(true)
  })

  it('generates setup passwords with the requested length', () => {
    const password = generateSetupPassword(32)

    expect(password).toHaveLength(32)
    expect(password).toMatch(/^[A-HJ-NP-Za-km-z2-9_-]+$/)
  })

  it('uses the configured daemon address default when provided', () => {
    vi.stubGlobal('__BAREOS_SETUP_DEFAULTS__', {
      daemonAddress: 'bareos.example.com',
    })

    expect(buildDefaultDaemonAddress()).toBe('bareos.example.com')
  })

  it('falls back to the current hostname for the daemon address default', () => {
    vi.stubGlobal('location', {
      hostname: 'bareos.example.com',
      origin: 'http://bareos.example.com:8080',
    })

    expect(buildDefaultDaemonAddress()).toBe('bareos.example.com')
  })

  it('derives the default bootstrap url from the current origin', () => {
    vi.stubGlobal('location', {
      origin: 'http://bareos.example.com:8080',
    })

    expect(buildDefaultSetupBootstrapUrl()).toBe(
      'http://bareos.example.com:8080/api/bconfig/v1'
    )
  })

  it('renders the storage bootstrap command with shell-quoted values', () => {
    expect(buildStorageBootstrapCommand({
      bootstrapUrl: 'http://bareos.example.com:8080/api/bconfig/v1',
      sessionId: 'storage-bootstrap-1',
      bootstrapToken: 'secret-token',
    })).toBe(
      "sudo bareos-sd --discovery --bootstrap-url 'http://bareos.example.com:8080/api/bconfig/v1' --bootstrap-session 'storage-bootstrap-1' --bootstrap-token 'secret-token'"
    )
  })

  it('derives the daemon basename from the FQDN hostname', () => {
    expect(deriveSetupDaemonBaseName('backup.example.com')).toBe('backup')
    expect(deriveSetupDaemonBaseName('Backup-01.example.com')).toBe('backup-01')
  })

  it('derives default daemon names from the server address', () => {
    expect(deriveSetupDaemonNames('backup.example.com')).toEqual({
      directorName: 'backup-dir',
      storageName: 'backup-sd',
      clientName: 'backup-fd',
    })
  })

  it('creates the initial deployment resources and validates the result', async () => {
    const setup = useSetupStore()
    const spec = {
      deploymentId: 'prod',
      deploymentName: 'Production',
      repositoryPath: '/var/lib/bconfig/deployments/prod',
      runtimeRoot: '/var/lib/bareos',
      workflowMode: 'review',
      daemonAddress: '127.0.0.1',
      storageDaemonAddress: 'storage.example.com',
      storageArchivePath: '/srv/storage/bareos/storage',
      storageBootstrapSessionId: 'storage-bootstrap-1',
      storageBootstrapSelection: {
        filesystem_mountpoint: '/srv/storage',
        archive_path: '/srv/storage/bareos/storage',
        device_name: 'FileStorage',
      },
      localDaemonComponents: ['client', 'director'],
      directorPort: 41001,
      clientPort: 41002,
      storagePort: 41003,
      directorName: 'bareos-dir',
      storageName: 'bareos-sd',
      clientName: 'bareos-fd',
      consoleName: 'admin',
      directorDaemonPassword: 'director-daemon-secret',
      clientDirectorPassword: 'client-director-secret',
      storageDirectorPassword: 'storage-director-secret',
      directorMonitorPassword: 'director-monitor-secret',
      consolePassword: 'console-secret',
    }
    const initialRequests = buildInitialSetupRequests(spec)
    const fetchMock = vi.fn()
      .mockResolvedValueOnce(jsonResponse({ deployment: { id: 'prod' } }, 201))

    for (let index = 0; index < initialRequests.length; index += 1) {
      fetchMock.mockResolvedValueOnce(jsonResponse({}))
    }

    fetchMock
      .mockResolvedValueOnce(jsonResponse({
        storage_bootstrap_session: {
          id: 'storage-bootstrap-1',
          status: 'selected',
          selection: {
            filesystem_mountpoint: '/srv/storage',
            archive_path: '/srv/storage/bareos/storage',
            device_name: 'FileStorage',
          },
        },
      }))
      .mockResolvedValueOnce(jsonResponse({
        storage_bootstrap_session: {
          id: 'storage-bootstrap-1',
          status: 'applied',
          selection: {
            filesystem_mountpoint: '/srv/storage',
            archive_path: '/srv/storage/bareos/storage',
            device_name: 'FileStorage',
          },
        },
      }))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-1' } }, 201))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-1', status: 'succeeded', logs: [
        'job execution started',
        'validated 4 imported config root(s)',
      ] } }))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-2' } }, 201))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-2', status: 'succeeded', logs: [
        'job execution started',
        'created database user \'bareos\' and granted catalog privileges',
      ] } }))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-3' } }, 201))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-3', status: 'succeeded', logs: [
        'job execution started',
        'started director \'bareos-dir\' using /tmp/bareos-dir',
        'stopped director \'bareos-dir\'',
      ] } }))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-4' } }, 201))
      .mockResolvedValueOnce(jsonResponse({ job: { id: 'job-4', status: 'succeeded', logs: [
        'job execution started',
        'started director \'bareos-dir\' using /tmp/bareos-dir',
        'started 2 deployment daemon(s) via systemctl',
      ] } }))
      .mockResolvedValueOnce(jsonResponse({ deployments: [{ id: 'prod' }] }))
    vi.stubGlobal('fetch', fetchMock)

    await expect(setup.createInitialDeployment(spec)).resolves.toEqual({ id: 'prod' })

    const expectedPaths = [
      '/api/bconfig/v1/deployments',
      ...initialRequests.map(request => `/api/bconfig/v1${request.path}`),
      '/api/bconfig/v1/bootstrap/storage-sessions/storage-bootstrap-1/selection',
      '/api/bconfig/v1/bootstrap/storage-sessions/storage-bootstrap-1',
      '/api/bconfig/v1/jobs',
      '/api/bconfig/v1/jobs/job-1',
      '/api/bconfig/v1/jobs',
      '/api/bconfig/v1/jobs/job-2',
      '/api/bconfig/v1/jobs',
      '/api/bconfig/v1/jobs/job-3',
      '/api/bconfig/v1/jobs',
      '/api/bconfig/v1/jobs/job-4',
      '/api/bconfig/v1/deployments',
    ]

    expect(fetch.mock.calls.map(([path]) => path)).toEqual(expectedPaths)
    expect(JSON.parse(fetch.mock.calls[0][1].body)).toEqual({
      id: 'prod',
      name: 'Production',
      repository_path: '/var/lib/bconfig/deployments/prod',
      workflow_mode: 'review',
    })

    const requestIndexByPath = Object.fromEntries(
      initialRequests.map((request, index) => [request.path, index + 1])
    )

    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/directors/bareos-dir']][1].body)).toEqual({
      port: 41001,
      password: 'director-daemon-secret',
      working_directory: '/var/lib/bareos',
      messages: 'Daemon',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/directors/bareos-dir/profiles/webui-admin']][1].body)).toEqual({
      command_acl: [
        '!.bvfs_clear_cache',
        '!.exit',
        '!.sql',
        '!configure',
        '!create',
        '!delete',
        '!purge',
        '!prune',
        '!sqlquery',
        '!umount',
        '!unmount',
        '*all*',
      ],
      job_acl: ['*all*'],
      schedule_acl: ['*all*'],
      catalog_acl: ['*all*'],
      pool_acl: ['*all*'],
      storage_acl: ['*all*'],
      client_acl: ['*all*'],
      fileset_acl: ['*all*'],
      where_acl: ['*all*'],
      plugin_options_acl: ['*all*'],
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/directors/bareos-dir/profiles/${DEFAULT_OPERATOR_PROFILE}`]][1].body)).toEqual({
      description: 'Profile allowing normal Bareos operations.',
      command_acl: [
        '!.bvfs_clear_cache',
        '!.exit',
        '!.sql',
        '!configure',
        '!create',
        '!delete',
        '!purge',
        '!prune',
        '!sqlquery',
        '!umount',
        '!unmount',
        '*all*',
      ],
      catalog_acl: ['*all*'],
      client_acl: ['*all*'],
      fileset_acl: ['*all*'],
      job_acl: ['*all*'],
      plugin_options_acl: ['*all*'],
      pool_acl: ['*all*'],
      schedule_acl: ['*all*'],
      storage_acl: ['*all*'],
      where_acl: ['*all*'],
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/directors/bareos-dir/profiles/${DEFAULT_WEBUI_READONLY_PROFILE}`]][1].body)).toEqual({
      command_acl: [
        '.api',
        '.help',
        'use',
        'version',
        'status',
        'list',
        'llist',
        '.clients',
        '.jobs',
        '.filesets',
        '.pools',
        '.storages',
        '.defaults',
        '.schedule',
        '.bvfs_lsdirs',
        '.bvfs_lsfiles',
        '.bvfs_update',
        '.bvfs_get_jobids',
        '.bvfs_versions',
        '.bvfs_restore',
      ],
      job_acl: ['*all*'],
      schedule_acl: ['*all*'],
      catalog_acl: ['*all*'],
      pool_acl: ['*all*'],
      storage_acl: ['*all*'],
      client_acl: ['*all*'],
      fileset_acl: ['*all*'],
      where_acl: ['*all*'],
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/directors/bareos-dir/consoles/${DEFAULT_MONITOR_CONSOLE_NAME}`]][1].body)).toEqual({
      description: 'Restricted console used by tray-monitor to get the status of the director.',
      password: 'director-monitor-secret',
      command_acl: ['status', '.status'],
      job_acl: ['*all*'],
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/directors/bareos-dir/consoles/admin']][1].body)).toEqual({
      password: 'console-secret',
      profiles: [DEFAULT_WEBUI_ADMIN_PROFILE],
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/directors/bareos-dir/catalogs/MyCatalog']][1].body)).toEqual({
      db_name: 'bareos',
      db_user: 'bareos',
      db_password: '',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/clients/bareos-fd/directors/bareos-dir']][1].body)).toEqual({
      password: 'client-director-secret',
      description: 'Allow the configured Director to access this file daemon.',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/directors/bareos-dir/clients/bareos-fd']][1].body)).toEqual({
      address: '127.0.0.1',
      port: 41002,
      password: 'client-director-secret',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/storages/bareos-sd/directors/bareos-dir']][1].body)).toEqual({
      password: 'storage-director-secret',
      description: 'Director, who is permitted to contact this storage daemon.',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/storages/bareos-sd/devices/${DEFAULT_STORAGE_DEVICE_NAME}`]][1].body)).toEqual({
      media_type: 'File',
      archive_device: '/srv/storage/bareos/storage',
      device_type: 'File',
      label_media: true,
      random_access: true,
      automatic_mount: true,
      removable_media: false,
      always_open: false,
      description: 'File device. A connecting Director must have the same Name and MediaType.',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/directors/bareos-dir/storages/${DEFAULT_DIRECTOR_STORAGE_NAME}`]][1].body)).toEqual({
      address: 'storage.example.com',
      port: 41003,
      password: 'storage-director-secret',
      device: DEFAULT_STORAGE_DEVICE_NAME,
      media_type: 'File',
      maximum_concurrent_jobs: 10,
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/directors/bareos-dir/jobdefs/DefaultJob']][1].body)).toEqual({
      type: 'Backup',
      level: 'Incremental',
      client: 'bareos-fd',
      fileset: 'SelfTest',
      schedule: 'WeeklyCycle',
      storages: [DEFAULT_DIRECTOR_STORAGE_NAME],
      messages: 'Standard',
      pool: 'Incremental',
      priority: 10,
      write_bootstrap: '/var/lib/bareos/%c.bsr',
      full_backup_pool: 'Full',
      differential_backup_pool: 'Differential',
      incremental_backup_pool: 'Incremental',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/directors/bareos-dir/jobs/${DEFAULT_PRIMARY_BACKUP_JOB_NAME}`]][1].body)).toEqual({
      jobdefs: DEFAULT_JOBDEFS_NAME,
      client: 'bareos-fd',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/directors/bareos-dir/jobs/${DEFAULT_CATALOG_BACKUP_JOB_NAME}`]][1].body)).toEqual({
      description: 'Backup the catalog database (after the nightly save)',
      jobdefs: DEFAULT_JOBDEFS_NAME,
      level: 'Full',
      fileset: 'Catalog',
      schedule: 'WeeklyCycleAfterBackup',
      run_before_job_entries: ['/usr/lib/bareos/scripts/make_catalog_backup MyCatalog'],
      run_after_job_entries: ['/usr/lib/bareos/scripts/delete_catalog_backup MyCatalog'],
      write_bootstrap: '|/usr/sbin/bsmtp -h localhost -f "\\(Bareos\\) " -s "Bootstrap for Job %j" root',
      priority: 11,
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath[`/deployments/prod/directors/bareos-dir/jobs/${DEFAULT_RESTORE_JOB_NAME}`]][1].body)).toEqual({
      description: 'Standard Restore template. Only one such job is needed for all standard Jobs/Clients/Storage ...',
      type: 'Restore',
      client: 'bareos-fd',
      fileset: 'LinuxAll',
      storages: [DEFAULT_DIRECTOR_STORAGE_NAME],
      pool: 'Incremental',
      messages: 'Standard',
      where: '/tmp/bareos-restores',
      maximum_concurrent_jobs: 10,
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/storages/bareos-sd']][1].body)).toEqual({
      address: 'storage.example.com',
      port: 41003,
      working_directory: '/var/lib/bareos',
      messages: 'Standard',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/clients/bareos-fd']][1].body)).toEqual({
      address: '127.0.0.1',
      port: 41002,
      working_directory: '/var/lib/bareos',
      messages: 'Standard',
    })
    expect(JSON.parse(fetch.mock.calls[requestIndexByPath['/deployments/prod/consoles/admin/directors/bareos-dir']][1].body)).toEqual({
      address: '127.0.0.1',
      port: 41001,
      password: 'console-secret',
    })
    expect(JSON.parse(fetch.mock.calls[initialRequests.length + 1][1].body)).toEqual({
      selection: {
        filesystem_mountpoint: '/srv/storage',
        archive_path: '/srv/storage/bareos/storage',
        device_name: 'FileStorage',
      },
    })
    expect(JSON.parse(fetch.mock.calls[initialRequests.length + 3][1].body)).toEqual({
      type: 'validate_deployment_repo',
      deployment_id: 'prod',
    })
    expect(JSON.parse(fetch.mock.calls[initialRequests.length + 5][1].body)).toEqual({
      type: 'initialize_catalog_database',
      deployment_id: 'prod',
    })
    expect(JSON.parse(fetch.mock.calls[initialRequests.length + 7][1].body)).toEqual({
      type: 'smoke_test_deployment',
      deployment_id: 'prod',
      components: ['client', 'director'],
    })
    expect(JSON.parse(fetch.mock.calls[initialRequests.length + 9][1].body)).toEqual({
      type: 'start_deployment_daemons',
      deployment_id: 'prod',
      components: ['client', 'director'],
    })
    expect(setup.completed).toBe(true)
    expect(setup.progressTitle).toBe('Setup completed')
    expect(setup.progressLogs).toContain("Creating deployment 'Production' (prod) at /var/lib/bconfig/deployments/prod")
    expect(setup.progressLogs).toContain('Creating initial Bareos resources')
    expect(setup.progressLogs).toContain('Applying /deployments/prod/directors/bareos-dir')
    expect(setup.progressLogs).toContain('Bootstrapping storage daemon')
    expect(setup.progressLogs).toContain('Storage bootstrap status: applied')
    expect(setup.progressLogs).toContain('validated 4 imported config root(s)')
    expect(setup.progressLogs).toContain("created database user 'bareos' and granted catalog privileges")
    expect(setup.progressLogs).toContain("started director 'bareos-dir' using /tmp/bareos-dir")
    expect(setup.progressLogs).toContain('Starting generated Bareos daemons')
    expect(setup.progressLogs).toContain('started 2 deployment daemon(s) via systemctl')
    expect(setup.progressLogs).toContain('Setup finished successfully. Review the log and continue to the login page when you are ready.')
    expect(setup.mode).toBe('ready')
    expect(setup.needsSetup).toBe(false)
  })

  it('creates and refreshes a storage bootstrap session', async () => {
    const setup = useSetupStore()
    const fetchMock = vi.fn()
      .mockResolvedValueOnce(jsonResponse({
        storage_bootstrap_session: {
          id: 'storage-bootstrap-1',
          deployment_id: 'prod',
          storage_name: 'bareos-sd',
          bootstrap_token: 'secret-token',
          status: 'pending',
        },
      }, 201))
      .mockResolvedValueOnce(jsonResponse({
        storage_bootstrap_session: {
          id: 'storage-bootstrap-1',
          deployment_id: 'prod',
          storage_name: 'bareos-sd',
          bootstrap_token: 'secret-token',
          status: 'discovered',
          discovery_report: {
            hostname: 'sdhost',
            filesystems: [
              {
                mountpoint: '/srv/storage',
                filesystem_type: 'xfs',
                free_bytes: 1024,
                suitable_for_archive: true,
                recommended_archive_path: '/srv/storage/bareos/storage',
              },
            ],
          },
        },
      }))
    vi.stubGlobal('fetch', fetchMock)

    await expect(setup.createStorageBootstrapSession({
      deploymentId: 'prod',
      storageName: 'bareos-sd',
    })).resolves.toMatchObject({
      id: 'storage-bootstrap-1',
      status: 'pending',
    })

    await expect(setup.refreshStorageBootstrapSession('storage-bootstrap-1')).resolves.toMatchObject({
      id: 'storage-bootstrap-1',
      status: 'discovered',
    })

    expect(fetch.mock.calls.map(([path]) => path)).toEqual([
      '/api/bconfig/v1/bootstrap/storage-sessions',
      '/api/bconfig/v1/bootstrap/storage-sessions/storage-bootstrap-1',
    ])
    expect(JSON.parse(fetch.mock.calls[0][1].body)).toEqual({
      deployment_id: 'prod',
      storage_name: 'bareos-sd',
      ttl_seconds: 900,
    })
    expect(setup.storageBootstrapSession.discovery_report.filesystems[0]).toMatchObject({
      mountpoint: '/srv/storage',
      recommended_archive_path: '/srv/storage/bareos/storage',
    })
  })

  it('submits the selected storage archive path', async () => {
    const setup = useSetupStore()
    const fetchMock = vi.fn()
      .mockResolvedValueOnce(jsonResponse({
        storage_bootstrap_session: {
          id: 'storage-bootstrap-1',
          status: 'selected',
          selection: {
            filesystem_mountpoint: '/srv/storage',
            archive_path: '/srv/storage/bareos/storage',
            device_name: 'FileStorage',
          },
        },
      }))
    vi.stubGlobal('fetch', fetchMock)

    await expect(setup.submitStorageBootstrapSelection('storage-bootstrap-1', {
      filesystem_mountpoint: '/srv/storage',
      archive_path: '/srv/storage/bareos/storage',
      device_name: 'FileStorage',
    })).resolves.toMatchObject({
      status: 'selected',
    })

    expect(fetch).toHaveBeenCalledWith(
      '/api/bconfig/v1/bootstrap/storage-sessions/storage-bootstrap-1/selection',
      expect.objectContaining({
        method: 'POST',
      })
    )
    expect(JSON.parse(fetch.mock.calls[0][1].body)).toEqual({
      selection: {
        filesystem_mountpoint: '/srv/storage',
        archive_path: '/srv/storage/bareos/storage',
        device_name: 'FileStorage',
      },
    })
    expect(setup.storageBootstrapSession.selection.archive_path).toBe(
      '/srv/storage/bareos/storage'
    )
  })

  it('launches local storage bootstrap discovery', async () => {
    const setup = useSetupStore()
    const fetchMock = vi.fn()
      .mockResolvedValueOnce(jsonResponse({
        storage_bootstrap_session: {
          id: 'storage-bootstrap-1',
          status: 'pending',
        },
      }))
    vi.stubGlobal('fetch', fetchMock)

    await expect(setup.launchLocalStorageBootstrap(
      'storage-bootstrap-1',
      'http://bareos.example.com:8080/api/bconfig/v1'
    )).resolves.toMatchObject({
      id: 'storage-bootstrap-1',
      status: 'pending',
    })

    expect(fetch).toHaveBeenCalledWith(
      '/api/bconfig/v1/bootstrap/storage-sessions/storage-bootstrap-1/launch-local',
      expect.objectContaining({
        method: 'POST',
      })
    )
    expect(JSON.parse(fetch.mock.calls[0][1].body)).toEqual({
      bootstrap_url: 'http://bareos.example.com:8080/api/bconfig/v1',
    })
  })
})
