// Shared mock data for all pages

export const mockJobs = [
  { id: 1, name: 'BackupClient1', client: 'bareos-fd', type: 'B', level: 'F', starttime: '2026-03-23 08:00:01', endtime: '2026-03-23 08:12:44', duration: '0:12:43', files: 48231, bytes: 2147483648, errors: 0, status: 'T' },
  { id: 2, name: 'BackupClient2', client: 'fileserver-fd', type: 'B', level: 'I', starttime: '2026-03-23 09:00:00', endtime: '2026-03-23 09:04:12', duration: '0:04:12', files: 1203, bytes: 104857600, errors: 0, status: 'T' },
  { id: 3, name: 'BackupClient3', client: 'db-server-fd', type: 'B', level: 'F', starttime: '2026-03-23 10:00:00', endtime: '2026-03-23 10:45:30', duration: '0:45:30', files: 892341, bytes: 107374182400, errors: 2, status: 'W' },
  { id: 4, name: 'RestoreJob',    client: 'bareos-fd',    type: 'R', level: 'F', starttime: '2026-03-23 11:00:00', endtime: '2026-03-23 11:08:10', duration: '0:08:10', files: 12000, bytes: 524288000, errors: 0, status: 'T' },
  { id: 5, name: 'BackupClient4', client: 'mail-fd',      type: 'B', level: 'I', starttime: '2026-03-23 12:00:00', endtime: '2026-03-23 12:01:05', duration: '0:01:05', files: 45, bytes: 1048576, errors: 0, status: 'f' },
  { id: 6, name: 'BackupClient5', client: 'web-fd',       type: 'B', level: 'D', starttime: '2026-03-23 13:00:00', endtime: '2026-03-23 13:22:15', duration: '0:22:15', files: 23451, bytes: 5368709120, errors: 0, status: 'T' },
  { id: 7, name: 'BackupClient1', client: 'bareos-fd',    type: 'B', level: 'I', starttime: '2026-03-23 14:00:00', endtime: null, duration: null, files: 9812, bytes: 314572800, errors: 0, status: 'R' },
  { id: 8, name: 'BackupClient6', client: 'nas-fd',       type: 'B', level: 'F', starttime: '2026-03-22 22:00:00', endtime: '2026-03-23 02:14:33', duration: '4:14:33', files: 1234567, bytes: 1099511627776, errors: 0, status: 'T' },
]

export const mockClients = [
  { name: 'bareos-fd',     uname: 'x86_64-linux-gnu', version: '23.0.2',  enabled: true,  os: 'linux' },
  { name: 'fileserver-fd', uname: 'x86_64-linux-gnu', version: '23.0.2',  enabled: true,  os: 'linux' },
  { name: 'db-server-fd',  uname: 'x86_64-linux-gnu', version: '22.1.0',  enabled: true,  os: 'linux' },
  { name: 'mail-fd',       uname: 'x86_64-linux-gnu', version: '23.0.2',  enabled: false, os: 'linux' },
  { name: 'web-fd',        uname: 'x86_64-linux-gnu', version: '23.0.2',  enabled: true,  os: 'linux' },
  { name: 'nas-fd',        uname: 'x86_64-linux-gnu', version: '21.0.0',  enabled: true,  os: 'linux' },
  { name: 'win-client-fd', uname: 'x86_64-pc-win32',  version: '23.0.2',  enabled: true,  os: 'windows' },
]

export const mockStorages = [
  { name: 'File',      autochanger: false, address: 'bareos-sd', port: 9103, mediatype: 'File', enabled: true },
  { name: 'Tape',      autochanger: true,  address: 'tape-sd',   port: 9103, mediatype: 'LTO8', enabled: true },
  { name: 'Cloud',     autochanger: false, address: 's3-sd',     port: 9103, mediatype: 'Cloud', enabled: true },
]

export const mockPools = [
  { name: 'Full',        pooltype: 'Backup', numvols: 24, maxvols: 100, volretention: '365 days', maxvoljobs: 1,  maxvolbytes: '50G' },
  { name: 'Incremental', pooltype: 'Backup', numvols: 120, maxvols: 500, volretention: '30 days',  maxvoljobs: 10, maxvolbytes: '20G' },
  { name: 'Differential',pooltype: 'Backup', numvols: 48, maxvols: 200, volretention: '90 days',  maxvoljobs: 5,  maxvolbytes: '30G' },
  { name: 'Scratch',     pooltype: 'Scratch',numvols: 0,  maxvols: 10,  volretention: '0 days',   maxvoljobs: 0,  maxvolbytes: '0'  },
]

export const mockVolumes = [
  { volumename: 'Full-0001',  pool: 'Full',        storage: 'File', mediatype: 'File', lastwritten: '2026-03-22 22:14:33', volstatus: 'Full',     inchanger: false, retention: '365 days', maxvolbytes: '53G', volbytes: '52.4G' },
  { volumename: 'Full-0002',  pool: 'Full',        storage: 'File', mediatype: 'File', lastwritten: '2026-03-21 08:12:44', volstatus: 'Full',     inchanger: false, retention: '365 days', maxvolbytes: '53G', volbytes: '50.1G' },
  { volumename: 'Incr-0042',  pool: 'Incremental', storage: 'File', mediatype: 'File', lastwritten: '2026-03-23 09:04:12', volstatus: 'Append',   inchanger: false, retention: '30 days',  maxvolbytes: '21G', volbytes: '2.1G' },
  { volumename: 'Incr-0043',  pool: 'Incremental', storage: 'File', mediatype: 'File', lastwritten: '2026-03-23 14:00:00', volstatus: 'Append',   inchanger: false, retention: '30 days',  maxvolbytes: '21G', volbytes: '0.3G' },
  { volumename: 'Tape-LTO-01',pool: 'Full',        storage: 'Tape', mediatype: 'LTO8', lastwritten: '2026-03-10 03:00:00', volstatus: 'Full',     inchanger: true,  retention: '365 days', maxvolbytes: '12T', volbytes: '11.8T' },
  { volumename: 'Scratch-001',pool: 'Scratch',     storage: 'File', mediatype: 'File', lastwritten: null,                  volstatus: 'Recycled', inchanger: false, retention: '0 days',   maxvolbytes: '53G', volbytes: '0' },
]

export const mockSchedules = [
  { name: 'WeeklyCycleAfterBackup', level: 'Incremental',  runs: 'Mon-Fri at 02:05',  enabled: true  },
  { name: 'WeeklyCycle',            level: 'Full',          runs: 'Sat at 21:00',       enabled: true  },
  { name: 'DifferentialCycle',      level: 'Differential',  runs: 'Sun at 23:00',       enabled: true  },
  { name: 'MonthlyFullCycle',       level: 'Full',          runs: '1st of month 00:05', enabled: true  },
  { name: 'HourlyCycle',            level: 'Incremental',  runs: 'Hourly at :30',      enabled: false },
]

export const mockFilesets = [
  {
    name: 'Linux All',
    description: 'All files on Linux',
    include: ['/'],
    exclude: ['/proc', '/sys', '/tmp', '/var/tmp'],
    options: 'compression=GZIP, signature=MD5',
  },
  {
    name: 'Windows All',
    description: 'All files on Windows',
    include: ['C:/'],
    exclude: ['C:/Windows/Temp'],
    options: 'compression=GZIP',
  },
  {
    name: 'Catalog',
    description: 'Bareos catalog backup',
    include: ['/var/lib/bareos/bareos.sql'],
    exclude: [],
    options: 'signature=SHA1',
  },
  {
    name: 'SelfTest',
    description: 'bareos-fd selftest',
    include: ['/usr/sbin'],
    exclude: [],
    options: '',
  },
]

export const jobStatusMap = {
  T: { label: 'OK',       color: 'positive' },
  W: { label: 'Warning',  color: 'warning'  },
  f: { label: 'Failed',   color: 'negative' },
  A: { label: 'Canceled', color: 'grey'     },
  R: { label: 'Running',  color: 'info'     },
  C: { label: 'Waiting',  color: 'grey'     },
  E: { label: 'Error',    color: 'negative' },
}

export const jobTypeMap = {
  B: 'Backup', R: 'Restore', V: 'Verify', A: 'Admin', D: 'Diagnostic', C: 'Copy', M: 'Migration',
}

export const jobLevelMap = {
  F: 'Full', I: 'Incremental', D: 'Differential',
}

export function formatBytes(bytes) {
  if (!bytes || bytes === 0) return '0 B'
  const num = typeof bytes === 'string' ? parseFloat(bytes) : bytes
  if (isNaN(num)) return bytes
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB']
  const i = Math.floor(Math.log(num) / Math.log(1024))
  return (num / Math.pow(1024, i)).toFixed(2) + ' ' + sizes[i]
}

export function parseDurationSecs(str) {
  if (!str) return 0
  const parts = String(str).split(':').map(Number)
  if (parts.length === 3) return parts[0] * 3600 + parts[1] * 60 + parts[2]
  if (parts.length === 2) return parts[0] * 60 + parts[1]
  return Number(str) || 0
}

export function formatSpeed(bytes, durationStr) {
  const secs = parseDurationSecs(durationStr)
  if (!secs || secs <= 0) return '—'
  const num = typeof bytes === 'string' ? parseFloat(bytes) : (bytes || 0)
  if (isNaN(num) || num <= 0) return '—'
  const bps = num / secs
  const sizes = ['B/s', 'KB/s', 'MB/s', 'GB/s', 'TB/s']
  const i = Math.floor(Math.log(bps) / Math.log(1024))
  return (bps / Math.pow(1024, i)).toFixed(2) + ' ' + sizes[i]
}

export function timeAgo(str) {
  if (!str) return '—'
  const ms = Date.now() - new Date(str.replace(' ', 'T')).getTime()
  if (isNaN(ms)) return str
  const s = Math.floor(ms / 1000)
  if (s <    60) return `${s}s ago`
  const m = Math.floor(s / 60)
  if (m <    60) return `${m}m ago`
  const h = Math.floor(m / 60)
  if (h <    24) return `${h}h ago`
  const d = Math.floor(h / 24)
  if (d <    30) return `${d}d ago`
  const mo = Math.floor(d / 30)
  if (mo <   12) return `${mo}mo ago`
  return `${Math.floor(mo / 12)}y ago`
}

export function formatDuration(seconds) {
  const s = typeof seconds === 'string' ? parseInt(seconds, 10) : seconds
  if (!s || isNaN(s) || s === 0) return '0 s'
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  if (d > 0) return h > 0 ? `${d}d ${h}h` : `${d}d`
  if (h > 0) return m > 0 ? `${h}h ${m}m` : `${h}h`
  return `${m}m`
}

export const directorStatus = `
*status director
Automatically selected Director: bareos-dir
Connecting to Director bareos-dir:9101
1000 OK: bareos-dir Version: 23.0.2 (01 March 2026)

You are logged in as: admin

Daemon started 22-Mar-26 08:00. Jobs: run=42, running=1.

Scheduled Jobs:
Level          Type     Pri  Scheduled          Name               Volume
===================================================================================
Incremental    Backup    10  23-Mar-26 15:00    BackupClient1      Incr-0043
Full           Backup    10  24-Mar-26 00:00    BackupClient3      *unknown*
Full           Backup    10  24-Mar-26 01:00    BackupClient6      *unknown*

Running Jobs:
Console connected at 23-Mar-26 14:02
 JobId  Level    Files      Bytes   Status   StartTime             Running time
======================================================================
     7  Incr     9,812    300.0 M  Running  23-Mar-26 14:00:00    0:14:22

No terminated jobs.
====
`.trim()
