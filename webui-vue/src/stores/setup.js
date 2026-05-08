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

import { computed, ref } from 'vue'
import { defineStore } from 'pinia'

export const BCONFIG_API_PREFIX = '/api/bconfig/v1'
export const DEFAULT_DEPLOYMENT_ID = 'prod'
export const JOB_POLL_ATTEMPTS = 80
export const JOB_POLL_INTERVAL_MS = 250
export const SMOKE_TEST_JOB_POLL_ATTEMPTS = 80
export const DEFAULT_WEBUI_ADMIN_PROFILE = 'webui-admin'
export const DEFAULT_WEBUI_READONLY_PROFILE = 'webui-readonly'
export const DEFAULT_OPERATOR_PROFILE = 'operator'
export const DEFAULT_MONITOR_CONSOLE_NAME = 'bareos-mon'
export const DEFAULT_DIRECTOR_STORAGE_NAME = 'File'
export const DEFAULT_STORAGE_DEVICE_NAME = 'FileStorage'
export const DEFAULT_CATALOG_NAME = 'MyCatalog'
export const DEFAULT_JOBDEFS_NAME = 'DefaultJob'
export const DEFAULT_PRIMARY_BACKUP_JOB_NAME = 'backup-bareos-fd'
export const DEFAULT_CATALOG_BACKUP_JOB_NAME = 'BackupCatalog'
export const DEFAULT_RESTORE_JOB_NAME = 'RestoreFiles'
export const DEFAULT_WEEKLY_CYCLE_NAME = 'WeeklyCycle'
export const DEFAULT_WEEKLY_CYCLE_AFTER_BACKUP_NAME = 'WeeklyCycleAfterBackup'
export const DEFAULT_SELFTEST_FILESET_NAME = 'SelfTest'
export const DEFAULT_CATALOG_FILESET_NAME = 'Catalog'
export const DEFAULT_LINUX_ALL_FILESET_NAME = 'LinuxAll'
export const DEFAULT_FULL_POOL_NAME = 'Full'
export const DEFAULT_DIFFERENTIAL_POOL_NAME = 'Differential'
export const DEFAULT_INCREMENTAL_POOL_NAME = 'Incremental'
export const DEFAULT_SCRATCH_POOL_NAME = 'Scratch'
export const DEFAULT_SETUP_DIRECTOR_PORT = 39101
export const DEFAULT_SETUP_CLIENT_PORT = 39102
export const DEFAULT_SETUP_STORAGE_PORT = 39103

const DEFAULT_BSMTP_PATH = '/usr/sbin/bsmtp'
const DEFAULT_HELPER_SCRIPT_DIRECTORY = '/usr/lib/bareos/scripts'
const DEFAULT_LOG_PATH = '/var/log/bareos/bareos.log'
const DEFAULT_SELFTEST_PATH = '/usr/sbin'
const DEFAULT_RESTORE_PATH = '/tmp/bareos-restores'
const DEFAULT_CATALOG_DB_NAME = 'bareos'
const DEFAULT_CATALOG_DB_USER = 'bareos'
const STORAGE_BOOTSTRAP_POLL_ATTEMPTS = 120

function readSetupRuntimeConfig() {
  const config = globalThis.__BAREOS_SETUP_DEFAULTS__
  return config && typeof config === 'object' ? config : {}
}

export function getDefaultSetupDirectorPort() {
  const port = readSetupRuntimeConfig().directorPort
  return Number.isInteger(port) ? port : DEFAULT_SETUP_DIRECTOR_PORT
}

export function getDefaultSetupClientPort() {
  const port = readSetupRuntimeConfig().clientPort
  return Number.isInteger(port) ? port : DEFAULT_SETUP_CLIENT_PORT
}

export function getDefaultSetupStoragePort() {
  const port = readSetupRuntimeConfig().storagePort
  return Number.isInteger(port) ? port : DEFAULT_SETUP_STORAGE_PORT
}

export function buildDefaultRepositoryPath(deploymentId = DEFAULT_DEPLOYMENT_ID) {
  const configuredPath = readSetupRuntimeConfig().repositoryPath
  if (typeof configuredPath === 'string' && configuredPath.trim()) {
    return configuredPath.trim()
  }
  const trimmed = String(deploymentId ?? '').trim() || DEFAULT_DEPLOYMENT_ID
  return `/var/lib/bconfig/deployments/${trimmed}`
}

export function buildDefaultRuntimeRoot(deploymentId = DEFAULT_DEPLOYMENT_ID) {
  const configuredRoot = readSetupRuntimeConfig().runtimeRoot
  if (typeof configuredRoot === 'string' && configuredRoot.trim()) {
    return configuredRoot.trim()
  }
  return '/var/lib/bareos'
}

export function buildDefaultSetupBootstrapUrl() {
  const configuredUrl = readSetupRuntimeConfig().bootstrapUrl
  if (typeof configuredUrl === 'string' && configuredUrl.trim()) {
    return configuredUrl.trim()
  }

  const origin = globalThis.location?.origin
  if (typeof origin === 'string' && origin.trim()) {
    return `${origin.trim()}${BCONFIG_API_PREFIX}`
  }

  return BCONFIG_API_PREFIX
}

export function buildDefaultDaemonAddress() {
  const configuredAddress = readSetupRuntimeConfig().daemonAddress
  if (typeof configuredAddress === 'string' && configuredAddress.trim()) {
    return configuredAddress.trim()
  }

  const hostname = globalThis.location?.hostname
  if (typeof hostname === 'string' && hostname.trim()) {
    return hostname.trim()
  }

  return '127.0.0.1'
}

export function deriveSetupDaemonBaseName(address) {
  const normalizedAddress = String(address ?? '').trim().toLowerCase()
  const host = normalizedAddress.includes('.')
    ? normalizedAddress.split('.')[0]
    : normalizedAddress
  const sanitized = host
    .replace(/[^a-z0-9-]/g, '-')
    .replace(/-+/g, '-')
    .replace(/^-|-$/g, '')

  return sanitized || 'bareos'
}

export function deriveSetupDaemonNames(address) {
  const baseName = deriveSetupDaemonBaseName(address)
  return {
    directorName: `${baseName}-dir`,
    storageName: `${baseName}-sd`,
    clientName: `${baseName}-fd`,
  }
}

export function generateSetupPassword(length = 24) {
  const size = Number.isInteger(length) && length > 0 ? length : 24
  const characters = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789-_'
  const array = new Uint32Array(size)

  if (globalThis.crypto?.getRandomValues) {
    globalThis.crypto.getRandomValues(array)
  } else {
    for (let index = 0; index < size; index += 1) {
      array[index] = Math.floor(Math.random() * 0x100000000)
    }
  }

  return Array.from(array, value => characters[value % characters.length]).join('')
}

function shellQuote(value) {
  return `'${String(value ?? '').replaceAll("'", "'\"'\"'")}'`
}

export function buildStorageBootstrapCommand({
  bootstrapUrl = buildDefaultSetupBootstrapUrl(),
  sessionId,
  bootstrapToken,
} = {}) {
  if (!String(sessionId ?? '').trim() || !String(bootstrapToken ?? '').trim()) {
    return ''
  }

  return [
    'sudo',
    'bareos-sd',
    '--discovery',
    '--bootstrap-url',
    shellQuote(bootstrapUrl),
    '--bootstrap-session',
    shellQuote(sessionId),
    '--bootstrap-token',
    shellQuote(bootstrapToken),
  ].join(' ')
}

function sleep(delayMs) {
  return new Promise(resolve => {
    setTimeout(resolve, delayMs)
  })
}

function normalizeJobLogs(job) {
  return Array.isArray(job?.logs)
    ? job.logs.filter(entry => typeof entry === 'string' && entry.trim())
    : []
}

async function parseResponse(response) {
  const text = await response.text()
  if (!text) {
    return {}
  }

  try {
    return JSON.parse(text)
  } catch {
    if (!response.ok) {
      throw new Error(text)
    }
    return {}
  }
}

function getResponseError(payload, response) {
  if (typeof payload?.error === 'string' && payload.error.trim()) {
    return payload.error
  }
  if (typeof payload?.message === 'string' && payload.message.trim()) {
    return payload.message
  }
  if (typeof payload?.last_error === 'string' && payload.last_error.trim()) {
    return payload.last_error
  }

  return `Request failed (${response.status})`
}

async function requestBconfig(path, { method = 'GET', body } = {}) {
  const response = await fetch(`${BCONFIG_API_PREFIX}${path}`, {
    method,
    cache: 'no-store',
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  })
  const payload = await parseResponse(response)

  if (!response.ok) {
    throw new Error(getResponseError(payload, response))
  }

  return payload
}

export function buildInitialSetupRequests(spec) {
  const deploymentId = encodeURIComponent(spec.deploymentId)
  const directorName = encodeURIComponent(spec.directorName)
  const storageName = encodeURIComponent(spec.storageName)
  const clientName = encodeURIComponent(spec.clientName)
  const consoleName = encodeURIComponent(spec.consoleName)
  const defaultDirectorStorageName = encodeURIComponent(DEFAULT_DIRECTOR_STORAGE_NAME)
  const defaultStorageDeviceName = encodeURIComponent(DEFAULT_STORAGE_DEVICE_NAME)
  const defaultCatalogName = encodeURIComponent(DEFAULT_CATALOG_NAME)
  const defaultMonitorConsoleName = encodeURIComponent(DEFAULT_MONITOR_CONSOLE_NAME)
  const defaultOperatorProfile = encodeURIComponent(DEFAULT_OPERATOR_PROFILE)
  const defaultReadonlyProfile = encodeURIComponent(DEFAULT_WEBUI_READONLY_PROFILE)
  const defaultWebuiAdminProfile = encodeURIComponent(DEFAULT_WEBUI_ADMIN_PROFILE)
  const defaultWeeklyCycleName = encodeURIComponent(DEFAULT_WEEKLY_CYCLE_NAME)
  const defaultWeeklyCycleAfterBackupName = encodeURIComponent(DEFAULT_WEEKLY_CYCLE_AFTER_BACKUP_NAME)
  const defaultSelfTestFilesetName = encodeURIComponent(DEFAULT_SELFTEST_FILESET_NAME)
  const defaultCatalogFilesetName = encodeURIComponent(DEFAULT_CATALOG_FILESET_NAME)
  const defaultLinuxAllFilesetName = encodeURIComponent(DEFAULT_LINUX_ALL_FILESET_NAME)
  const defaultJobDefsName = encodeURIComponent(DEFAULT_JOBDEFS_NAME)
  const defaultPrimaryBackupJobName = encodeURIComponent(DEFAULT_PRIMARY_BACKUP_JOB_NAME)
  const defaultCatalogBackupJobName = encodeURIComponent(DEFAULT_CATALOG_BACKUP_JOB_NAME)
  const defaultRestoreJobName = encodeURIComponent(DEFAULT_RESTORE_JOB_NAME)
  const defaultFullPoolName = encodeURIComponent(DEFAULT_FULL_POOL_NAME)
  const defaultDifferentialPoolName = encodeURIComponent(DEFAULT_DIFFERENTIAL_POOL_NAME)
  const defaultIncrementalPoolName = encodeURIComponent(DEFAULT_INCREMENTAL_POOL_NAME)
  const defaultScratchPoolName = encodeURIComponent(DEFAULT_SCRATCH_POOL_NAME)
  const directorDaemonPassword = spec.directorDaemonPassword || generateSetupPassword()
  const clientDirectorPassword = spec.clientDirectorPassword || generateSetupPassword()
  const storageDirectorPassword = spec.storageDirectorPassword || generateSetupPassword()
  const clientMonitorPassword = spec.clientMonitorPassword || generateSetupPassword()
  const storageMonitorPassword = spec.storageMonitorPassword || generateSetupPassword()
  const directorMonitorPassword = spec.directorMonitorPassword || generateSetupPassword()
  const runtimeRoot = spec.runtimeRoot || buildDefaultRuntimeRoot(spec.deploymentId)
  const directorPort = Number.isInteger(spec.directorPort)
    ? spec.directorPort
    : getDefaultSetupDirectorPort()
  const storagePort = Number.isInteger(spec.storagePort)
    ? spec.storagePort
    : getDefaultSetupStoragePort()
  const clientPort = Number.isInteger(spec.clientPort)
    ? spec.clientPort
    : getDefaultSetupClientPort()
  const directorWorkingDirectory = runtimeRoot
  const storageWorkingDirectory = runtimeRoot
  const clientWorkingDirectory = runtimeRoot
  const storageArchiveDirectory = String(
    spec.storageArchivePath || `${runtimeRoot}/storage`
  ).trim()
  const storageDaemonAddress = String(
    spec.storageDaemonAddress || spec.daemonAddress
  ).trim()
  const bootstrapFile = `${runtimeRoot}/%c.bsr`
  const catalogDumpFile = `${runtimeRoot}/${DEFAULT_CATALOG_DB_NAME}.sql`
  const catalogConfigPath = spec.repositoryPath || '/usr/local/etc/bareos'
  const webuiAdminCommandAcl = [
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
  ]

  return [
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/messages/Daemon`,
      body: {
        description: 'Managed daemon messages',
        console_entries: ['all'],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/messages/Standard`,
      body: {
        description: 'Reasonable message delivery -- send most everything to email address and to the console.',
        operator_command: `${DEFAULT_BSMTP_PATH} -h localhost -f "\\(Bareos\\) \\<%r\\>" -s "Bareos: Intervention needed for %j" %r`,
        mail_command: `${DEFAULT_BSMTP_PATH} -h localhost -f "\\(Bareos\\) \\<%r\\>" -s "Bareos: %t %e of %c %l" %r`,
        operator_entries: ['root = mount'],
        mail_entries: ['root = all, !skipped, !saved, !audit'],
        console_entries: ['all, !skipped, !saved, !audit'],
        append_entries: [`"${DEFAULT_LOG_PATH}" = all, !skipped, !saved, !audit`],
        catalog_entries: ['all, !skipped, !saved, !audit'],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}`,
      body: {
        port: directorPort,
        password: directorDaemonPassword,
        working_directory: directorWorkingDirectory,
        messages: 'Daemon',
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/profiles/${defaultOperatorProfile}`,
      body: {
        description: 'Profile allowing normal Bareos operations.',
        command_acl: webuiAdminCommandAcl,
        catalog_acl: ['*all*'],
        client_acl: ['*all*'],
        fileset_acl: ['*all*'],
        job_acl: ['*all*'],
        plugin_options_acl: ['*all*'],
        pool_acl: ['*all*'],
        schedule_acl: ['*all*'],
        storage_acl: ['*all*'],
        where_acl: ['*all*'],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/profiles/${defaultReadonlyProfile}`,
      body: {
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
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/profiles/${defaultWebuiAdminProfile}`,
      body: {
        command_acl: webuiAdminCommandAcl,
        job_acl: ['*all*'],
        schedule_acl: ['*all*'],
        catalog_acl: ['*all*'],
        pool_acl: ['*all*'],
        storage_acl: ['*all*'],
        client_acl: ['*all*'],
        fileset_acl: ['*all*'],
        where_acl: ['*all*'],
        plugin_options_acl: ['*all*'],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/consoles/${defaultMonitorConsoleName}`,
      body: {
        description: 'Restricted console used by tray-monitor to get the status of the director.',
        password: directorMonitorPassword,
        command_acl: ['status', '.status'],
        job_acl: ['*all*'],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/consoles/${consoleName}`,
      body: {
        password: spec.consolePassword,
        profiles: [DEFAULT_WEBUI_ADMIN_PROFILE],
      },
    },
    {
      path: `/deployments/${deploymentId}/storages/${storageName}/messages/Standard`,
      body: {
        description: 'Managed storage messages',
        director_entries: [`${spec.directorName} = all`],
      },
    },
    {
      path: `/deployments/${deploymentId}/storages/${storageName}`,
      body: {
        address: storageDaemonAddress,
        port: storagePort,
        working_directory: storageWorkingDirectory,
        messages: 'Standard',
      },
    },
    {
      path: `/deployments/${deploymentId}/storages/${storageName}/devices/${defaultStorageDeviceName}`,
      body: {
        media_type: 'File',
        archive_device: storageArchiveDirectory,
        device_type: 'File',
        label_media: true,
        random_access: true,
        automatic_mount: true,
        removable_media: false,
        always_open: false,
        description: 'File device. A connecting Director must have the same Name and MediaType.',
      },
    },
    {
      path: `/deployments/${deploymentId}/clients/${clientName}/messages/Standard`,
      body: {
        description: 'Managed client messages',
        director_entries: [`${spec.directorName} = all, !skipped, !restored`],
      },
    },
    {
      path: `/deployments/${deploymentId}/clients/${clientName}`,
      body: {
        address: spec.daemonAddress,
        port: clientPort,
        working_directory: clientWorkingDirectory,
        messages: 'Standard',
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/clients/${clientName}`,
      body: {
        address: spec.daemonAddress,
        port: clientPort,
        password: clientDirectorPassword,
      },
    },
    {
      path: `/deployments/${deploymentId}/clients/${clientName}/directors/${directorName}`,
      body: {
        password: clientDirectorPassword,
        description: 'Allow the configured Director to access this file daemon.',
      },
    },
    {
      path: `/deployments/${deploymentId}/storages/${storageName}/directors/${directorName}`,
      body: {
        password: storageDirectorPassword,
        description: 'Director, who is permitted to contact this storage daemon.',
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/storages/${defaultDirectorStorageName}`,
      body: {
        address: storageDaemonAddress,
        port: storagePort,
        password: storageDirectorPassword,
        device: DEFAULT_STORAGE_DEVICE_NAME,
        media_type: 'File',
        maximum_concurrent_jobs: 10,
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/catalogs/${defaultCatalogName}`,
      body: {
        db_name: DEFAULT_CATALOG_DB_NAME,
        db_user: DEFAULT_CATALOG_DB_USER,
        db_password: '',
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/pools/${defaultFullPoolName}`,
      body: {
        pool_type: 'Backup',
        recycle: true,
        auto_prune: true,
        volume_retention: 365 * 24 * 60 * 60,
        maximum_volume_bytes: 50 * 1024 * 1024 * 1024,
        maximum_volumes: 100,
        label_format: 'Full-',
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/pools/${defaultDifferentialPoolName}`,
      body: {
        pool_type: 'Backup',
        recycle: true,
        auto_prune: true,
        volume_retention: 90 * 24 * 60 * 60,
        maximum_volume_bytes: 10 * 1024 * 1024 * 1024,
        maximum_volumes: 100,
        label_format: 'Differential-',
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/pools/${defaultIncrementalPoolName}`,
      body: {
        pool_type: 'Backup',
        recycle: true,
        auto_prune: true,
        volume_retention: 30 * 24 * 60 * 60,
        maximum_volume_bytes: 1024 * 1024 * 1024,
        maximum_volumes: 100,
        label_format: 'Incremental-',
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/pools/${defaultScratchPoolName}`,
      body: {
        pool_type: 'Scratch',
        recycle_pool: DEFAULT_SCRATCH_POOL_NAME,
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/schedules/${defaultWeeklyCycleName}`,
      body: {
        run_entries: [
          'Full 1st sat at 21:00',
          'Differential 2nd-5th sat at 21:00',
          'Incremental mon-fri at 21:00',
        ],
        enabled: true,
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/schedules/${defaultWeeklyCycleAfterBackupName}`,
      body: {
        description: 'This schedule does the catalog. It starts after the WeeklyCycle.',
        run_entries: ['Full mon-fri at 21:10'],
        enabled: true,
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/filesets/${defaultSelfTestFilesetName}`,
      body: {
        description: 'fileset just to backup some files for selftest',
        include_blocks: [
          `  Include {\n    Options {\n      Signature = XXH128\n    }\n    File = "${DEFAULT_SELFTEST_PATH}"\n  }\n`,
        ],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/filesets/${defaultCatalogFilesetName}`,
      body: {
        description: 'Backup the catalog dump and Bareos configuration files.',
        include_blocks: [
          `  Include {\n    Options {\n      Signature = XXH128\n    }\n    File = "${catalogDumpFile}"\n    File = "${catalogConfigPath}"\n  }\n`,
        ],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/filesets/${defaultLinuxAllFilesetName}`,
      body: {
        description: 'Backup all regular filesystems, determined by filesystem type.',
        include_blocks: [
          '  Include {\n    Options {\n      Signature = XXH128\n      One FS = No\n      FS Type = btrfs\n      FS Type = ext2\n      FS Type = ext3\n      FS Type = ext4\n      FS Type = reiserfs\n      FS Type = jfs\n      FS Type = vfat\n      FS Type = xfs\n      FS Type = zfs\n    }\n    File = /\n  }\n',
        ],
        exclude_blocks: [
          `  Exclude {\n    File = "${directorWorkingDirectory}"\n    File = "${storageArchiveDirectory}"\n    File = /proc\n    File = /dev\n    File = /sys\n    File = /tmp\n    File = /var/tmp\n    File = /.journal\n    File = /.fsck\n  }\n`,
        ],
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/jobdefs/${defaultJobDefsName}`,
      body: {
        type: 'Backup',
        level: 'Incremental',
        client: spec.clientName,
        fileset: DEFAULT_SELFTEST_FILESET_NAME,
        schedule: DEFAULT_WEEKLY_CYCLE_NAME,
        storages: [DEFAULT_DIRECTOR_STORAGE_NAME],
        messages: 'Standard',
        pool: DEFAULT_INCREMENTAL_POOL_NAME,
        priority: 10,
        write_bootstrap: bootstrapFile,
        full_backup_pool: DEFAULT_FULL_POOL_NAME,
        differential_backup_pool: DEFAULT_DIFFERENTIAL_POOL_NAME,
        incremental_backup_pool: DEFAULT_INCREMENTAL_POOL_NAME,
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/jobs/${defaultPrimaryBackupJobName}`,
      body: {
        jobdefs: DEFAULT_JOBDEFS_NAME,
        client: spec.clientName,
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/jobs/${defaultCatalogBackupJobName}`,
      body: {
        description: 'Backup the catalog database (after the nightly save)',
        jobdefs: DEFAULT_JOBDEFS_NAME,
        level: 'Full',
        fileset: DEFAULT_CATALOG_FILESET_NAME,
        schedule: DEFAULT_WEEKLY_CYCLE_AFTER_BACKUP_NAME,
        run_before_job_entries: [`${DEFAULT_HELPER_SCRIPT_DIRECTORY}/make_catalog_backup ${DEFAULT_CATALOG_NAME}`],
        run_after_job_entries: [`${DEFAULT_HELPER_SCRIPT_DIRECTORY}/delete_catalog_backup ${DEFAULT_CATALOG_NAME}`],
        write_bootstrap: `|${DEFAULT_BSMTP_PATH} -h localhost -f "\\(Bareos\\) " -s "Bootstrap for Job %j" root`,
        priority: 11,
      },
    },
    {
      path: `/deployments/${deploymentId}/directors/${directorName}/jobs/${defaultRestoreJobName}`,
      body: {
        description: 'Standard Restore template. Only one such job is needed for all standard Jobs/Clients/Storage ...',
        type: 'Restore',
        client: spec.clientName,
        fileset: DEFAULT_LINUX_ALL_FILESET_NAME,
        storages: [DEFAULT_DIRECTOR_STORAGE_NAME],
        pool: DEFAULT_INCREMENTAL_POOL_NAME,
        messages: 'Standard',
        where: DEFAULT_RESTORE_PATH,
        maximum_concurrent_jobs: 10,
      },
    },
    {
      path: `/deployments/${deploymentId}/consoles/${consoleName}/directors/${directorName}`,
      body: {
        address: spec.daemonAddress,
        port: directorPort,
        password: spec.consolePassword,
      },
    },
    {
      path: `/deployments/${deploymentId}/consoles/${consoleName}/consoles/${consoleName}`,
      body: {
        director: spec.directorName,
        password: spec.consolePassword,
      },
    },
  ]
}

async function waitForJob(
  jobId,
  failureMessage,
  attempts = JOB_POLL_ATTEMPTS,
  onProgress = null
) {
  for (let attempt = 0; attempt < attempts; attempt += 1) {
    const payload = await requestBconfig(`/jobs/${encodeURIComponent(jobId)}`)
    const job = payload?.job

    if (!job) {
      throw new Error('Validation job did not return a job record')
    }

    onProgress?.(job)

    if (job.status === 'succeeded') {
      return job
    }

    if (job.status === 'failed') {
      throw new Error(job.last_error || job.logs?.at(-1) || failureMessage)
    }

    await sleep(JOB_POLL_INTERVAL_MS)
  }

  throw new Error(`${failureMessage} timed out`)
}

async function waitForStorageBootstrap(
  sessionId,
  failureMessage,
  attempts = STORAGE_BOOTSTRAP_POLL_ATTEMPTS,
  onProgress = null
) {
  for (let attempt = 0; attempt < attempts; attempt += 1) {
    const payload = await requestBconfig(
      `/bootstrap/storage-sessions/${encodeURIComponent(sessionId)}`
    )
    const session = payload?.storage_bootstrap_session

    if (!session) {
      throw new Error('Storage bootstrap request did not return a session record')
    }

    onProgress?.(session)

    if (session.status === 'applied') {
      return session
    }

    if (['failed', 'expired'].includes(session.status)) {
      throw new Error(session.last_error || failureMessage)
    }

    await sleep(JOB_POLL_INTERVAL_MS)
  }

  throw new Error(`${failureMessage} timed out`)
}

export const useSetupStore = defineStore('setup', () => {
  const mode = ref('unknown')
  const deployments = ref([])
  const loading = ref(false)
  const creating = ref(false)
  const storageBootstrapLoading = ref(false)
  const submittingStorageSelection = ref(false)
  const completed = ref(false)
  const error = ref(null)
  const storageBootstrapError = ref(null)
  const progressTitle = ref(null)
  const progressLogs = ref([])
  const storageBootstrapSession = ref(null)
  let pendingRefresh = null

  const needsSetup = computed(() => mode.value === 'needs-setup')
  const isReady = computed(() => mode.value === 'ready')

  function updateProgress(title) {
    progressTitle.value = title
  }

  function appendProgress(...entries) {
    for (const entry of entries) {
      if (typeof entry !== 'string') {
        continue
      }

      const trimmed = entry.trim()
      if (!trimmed) {
        continue
      }

      progressLogs.value = [...progressLogs.value, trimmed]
    }
  }

  function clearProgress() {
    completed.value = false
    progressTitle.value = null
    progressLogs.value = []
  }

  function clearStorageBootstrapState() {
    storageBootstrapError.value = null
    storageBootstrapSession.value = null
  }

  async function refresh(force = false) {
    if (!force && (mode.value === 'ready' || mode.value === 'needs-setup')) {
      return deployments.value
    }
    if (pendingRefresh) {
      return pendingRefresh
    }

    pendingRefresh = (async () => {
      loading.value = true
      error.value = null

      try {
        const payload = await requestBconfig('/deployments')
        deployments.value = Array.isArray(payload?.deployments)
          ? payload.deployments
          : []
        mode.value = deployments.value.length > 0 ? 'ready' : 'needs-setup'
        return deployments.value
      } catch (e) {
        deployments.value = []
        mode.value = 'error'
        error.value = e?.message ?? String(e)
        throw e
      } finally {
        loading.value = false
        pendingRefresh = null
      }
    })()

    return pendingRefresh
  }

  async function createInitialDeployment(spec) {
    creating.value = true
    completed.value = false
    error.value = null
    clearProgress()
    if (!String(spec.storageBootstrapSessionId ?? '').trim()) {
      clearStorageBootstrapState()
    }

    try {
      updateProgress('Creating deployment repository')
      appendProgress('Creating deployment repository')
      appendProgress(
        `Creating deployment '${spec.deploymentName}' (${spec.deploymentId}) at ${spec.repositoryPath}`
      )
      const payload = await requestBconfig('/deployments', {
        method: 'POST',
        body: {
          id: spec.deploymentId,
          name: spec.deploymentName,
          repository_path: spec.repositoryPath,
          workflow_mode: spec.workflowMode,
        },
      })
      appendProgress(`Created deployment repository for '${spec.deploymentId}'`)

      const initialRequests = buildInitialSetupRequests(spec)
      updateProgress('Creating initial Bareos resources')
      appendProgress('Creating initial Bareos resources')
      appendProgress(`Creating ${initialRequests.length} managed Bareos resources`)

      for (const request of initialRequests) {
        appendProgress(`Applying ${request.path}`)
        await requestBconfig(request.path, {
          method: 'PUT',
          body: request.body,
        })
        appendProgress(`Applied ${request.path}`)
      }

      const localDaemonComponents = Array.isArray(spec.localDaemonComponents)
        ? spec.localDaemonComponents
        : null
      const pendingStorageSelection = (
        String(spec.storageBootstrapSessionId ?? '').trim()
        && String(spec.storageBootstrapSelection?.archive_path ?? '').trim()
      )
        ? spec.storageBootstrapSelection
        : null

      if (pendingStorageSelection) {
        updateProgress('Bootstrapping storage daemon')
        appendProgress('Bootstrapping storage daemon')

        if (storageBootstrapSession.value?.selection?.archive_path !== pendingStorageSelection.archive_path) {
          appendProgress(`Submitting detected archive path ${pendingStorageSelection.archive_path}`)
          await submitStorageBootstrapSelection(
            spec.storageBootstrapSessionId,
            pendingStorageSelection
          )
        }

        let storageBootstrapLogStatus = null
        await waitForStorageBootstrap(
          spec.storageBootstrapSessionId,
          'Storage daemon bootstrap failed',
          STORAGE_BOOTSTRAP_POLL_ATTEMPTS,
          session => {
            storageBootstrapSession.value = session
            if (session.status && session.status !== storageBootstrapLogStatus) {
              storageBootstrapLogStatus = session.status
              appendProgress(`Storage bootstrap status: ${session.status}`)
            }
          }
        )
      }

      updateProgress('Validating generated configuration')
      appendProgress('Validating generated configuration')
      appendProgress(`Queueing configuration validation for deployment '${spec.deploymentId}'`)
      const validationJobPayload = await requestBconfig('/jobs', {
        method: 'POST',
        body: {
          type: 'validate_deployment_repo',
          deployment_id: spec.deploymentId,
        },
      })

      const validationJobId = validationJobPayload?.job?.id
      if (!validationJobId) {
        throw new Error('Validation job did not return an id')
      }
      appendProgress(`Started validation job ${validationJobId}`)

      let validationLogCount = 0
      await waitForJob(
        validationJobId,
        'Initial configuration validation failed',
        JOB_POLL_ATTEMPTS,
        job => {
          updateProgress('Validating generated configuration')
          const logs = normalizeJobLogs(job)
          appendProgress(...logs.slice(validationLogCount))
          validationLogCount = logs.length
        }
      )

      updateProgress('Creating catalog database')
      appendProgress('Creating catalog database')
      appendProgress(`Queueing catalog initialization for deployment '${spec.deploymentId}'`)
      const catalogSetupJobPayload = await requestBconfig('/jobs', {
        method: 'POST',
        body: {
          type: 'initialize_catalog_database',
          deployment_id: spec.deploymentId,
        },
      })
      const catalogSetupJobId = catalogSetupJobPayload?.job?.id
      if (!catalogSetupJobId) {
        throw new Error('Catalog setup job did not return an id')
      }
      appendProgress(`Started catalog setup job ${catalogSetupJobId}`)

      let catalogLogCount = 0
      await waitForJob(
        catalogSetupJobId,
        'Catalog database setup failed',
        JOB_POLL_ATTEMPTS,
        job => {
          updateProgress('Creating catalog database')
          const logs = normalizeJobLogs(job)
          appendProgress(...logs.slice(catalogLogCount))
          catalogLogCount = logs.length
        }
      )

      updateProgress('Testing generated configuration')
      appendProgress('Testing generated configuration')
      appendProgress(`Queueing smoke test for deployment '${spec.deploymentId}'`)
      const smokeTestJobPayload = await requestBconfig('/jobs', {
        method: 'POST',
        body: {
          type: 'smoke_test_deployment',
          deployment_id: spec.deploymentId,
          components: localDaemonComponents,
        },
      })
      const smokeTestJobId = smokeTestJobPayload?.job?.id
      if (!smokeTestJobId) {
        throw new Error('Smoke test job did not return an id')
      }
      appendProgress(`Started smoke test job ${smokeTestJobId}`)

      let smokeTestLogCount = 0
      await waitForJob(
        smokeTestJobId,
        'Bareos daemon smoke test failed',
        SMOKE_TEST_JOB_POLL_ATTEMPTS,
        job => {
          updateProgress('Testing generated configuration')
          const logs = normalizeJobLogs(job)
          appendProgress(...logs.slice(smokeTestLogCount))
          smokeTestLogCount = logs.length
        }
      )

      updateProgress('Starting generated Bareos daemons')
      appendProgress('Starting generated Bareos daemons')
      appendProgress(`Queueing daemon startup for deployment '${spec.deploymentId}'`)
      const startDaemonsJobPayload = await requestBconfig('/jobs', {
        method: 'POST',
        body: {
          type: 'start_deployment_daemons',
          deployment_id: spec.deploymentId,
          components: localDaemonComponents,
        },
      })
      const startDaemonsJobId = startDaemonsJobPayload?.job?.id
      if (!startDaemonsJobId) {
        throw new Error('Start daemons job did not return an id')
      }
      appendProgress(`Started daemon startup job ${startDaemonsJobId}`)

      let startDaemonsLogCount = 0
      await waitForJob(
        startDaemonsJobId,
        'Starting generated Bareos daemons failed',
        SMOKE_TEST_JOB_POLL_ATTEMPTS,
        job => {
          updateProgress('Starting generated Bareos daemons')
          const logs = normalizeJobLogs(job)
          appendProgress(...logs.slice(startDaemonsLogCount))
          startDaemonsLogCount = logs.length
        }
      )
      await refresh(true)
      completed.value = true
      updateProgress('Setup completed')
      appendProgress('Setup completed')
      appendProgress('Setup finished successfully. Review the log and continue to the login page when you are ready.')
      return payload?.deployment ?? null
    } catch (e) {
      completed.value = false
      mode.value = 'error'
      error.value = e?.message ?? String(e)
      updateProgress('Setup failed')
      appendProgress('Setup failed')
      appendProgress(`Setup failed: ${error.value}`)
      throw e
    } finally {
      creating.value = false
    }
  }

  async function createStorageBootstrapSession({
    deploymentId,
    storageName,
    ttlSeconds = 900,
  }) {
    storageBootstrapLoading.value = true
    storageBootstrapError.value = null

    try {
      const payload = await requestBconfig('/bootstrap/storage-sessions', {
        method: 'POST',
        body: {
          deployment_id: deploymentId,
          storage_name: storageName,
          ttl_seconds: ttlSeconds,
        },
      })
      storageBootstrapSession.value = payload?.storage_bootstrap_session ?? null
      return storageBootstrapSession.value
    } catch (e) {
      storageBootstrapError.value = e?.message ?? String(e)
      throw e
    } finally {
      storageBootstrapLoading.value = false
    }
  }

  async function refreshStorageBootstrapSession(sessionId) {
    if (!String(sessionId ?? '').trim()) {
      storageBootstrapSession.value = null
      return null
    }

    storageBootstrapLoading.value = true
    storageBootstrapError.value = null

    try {
      const payload = await requestBconfig(
        `/bootstrap/storage-sessions/${encodeURIComponent(sessionId)}`
      )
      storageBootstrapSession.value = payload?.storage_bootstrap_session ?? null
      return storageBootstrapSession.value
    } catch (e) {
      storageBootstrapError.value = e?.message ?? String(e)
      throw e
    } finally {
      storageBootstrapLoading.value = false
    }
  }

  async function submitStorageBootstrapSelection(sessionId, selection) {
    if (!String(sessionId ?? '').trim()) {
      throw new Error('Storage bootstrap session id is required')
    }

    submittingStorageSelection.value = true
    storageBootstrapError.value = null

    try {
      const payload = await requestBconfig(
        `/bootstrap/storage-sessions/${encodeURIComponent(sessionId)}/selection`,
        {
          method: 'POST',
          body: { selection },
        }
      )
      storageBootstrapSession.value = payload?.storage_bootstrap_session ?? null
      return storageBootstrapSession.value
    } catch (e) {
      storageBootstrapError.value = e?.message ?? String(e)
      throw e
    } finally {
      submittingStorageSelection.value = false
    }
  }

  async function launchLocalStorageBootstrap(sessionId, bootstrapUrl) {
    if (!String(sessionId ?? '').trim()) {
      throw new Error('Storage bootstrap session id is required')
    }
    if (!String(bootstrapUrl ?? '').trim()) {
      throw new Error('Storage bootstrap url is required')
    }

    storageBootstrapLoading.value = true
    storageBootstrapError.value = null

    try {
      const payload = await requestBconfig(
        `/bootstrap/storage-sessions/${encodeURIComponent(sessionId)}/launch-local`,
        {
          method: 'POST',
          body: {
            bootstrap_url: bootstrapUrl,
          },
        }
      )
      storageBootstrapSession.value = payload?.storage_bootstrap_session ?? null
      return storageBootstrapSession.value
    } catch (e) {
      storageBootstrapError.value = e?.message ?? String(e)
      throw e
    } finally {
      storageBootstrapLoading.value = false
    }
  }

  return {
    mode,
    deployments,
    loading,
    creating,
    storageBootstrapLoading,
    submittingStorageSelection,
    completed,
    error,
    storageBootstrapError,
    progressTitle,
    progressLogs,
    storageBootstrapSession,
    needsSetup,
    isReady,
    refresh,
    createInitialDeployment,
    clearStorageBootstrapState,
    createStorageBootstrapSession,
    refreshStorageBootstrapSession,
    submitStorageBootstrapSelection,
    launchLocalStorageBootstrap,
  }
})
