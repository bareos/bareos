/**
 * Generic composable for fetching data from the Bareos Director via the
 * WebSocket proxy.  Provides reactive loading / error / data state.
 *
 * Usage:
 *   const { data, loading, error, refresh } = useDirectorFetch('list jobs', 'jobs')
 *
 * @param {string}  command   Director command to run (e.g. "list jobs")
 * @param {string}  [key]     Top-level key to unwrap from the response dict.
 *                            If omitted the full response dict is returned as data.
 * @param {*}       [init]    Initial value for data while loading (default: [])
 */
import { ref, onMounted, watch } from 'vue'
import { useDirectorStore } from '../stores/director.js'

export function useDirectorFetch(command, key = null, init = []) {
  const director = useDirectorStore()
  const data    = ref(init)
  const loading = ref(false)
  const error   = ref(null)

  async function refresh() {
    if (!director.isConnected) {
      error.value = 'Not connected to director'
      return
    }
    loading.value = true
    error.value   = null
    try {
      const result = await director.call(command)
      data.value = key ? (result?.[key] ?? init) : (result ?? init)
    } catch (e) {
      error.value = e.message
    } finally {
      loading.value = false
    }
  }

  // Fetch immediately if already connected, otherwise wait for connection.
  // The watch handles both the initial connect and reconnects after a reload.
  onMounted(refresh)
  watch(() => director.isConnected, (connected) => { if (connected) refresh() })

  return { data, loading, error, refresh }
}

// ── Field-name normalisers for catalog data ──────────────────────────────────

export function directorCollection(value) {
  if (!value) return []
  if (Array.isArray(value)) return value
  if (typeof value !== 'object') return []
  return Object.values(value).flatMap((item) => (
    Array.isArray(item) ? item : [item]
  ))
}

export function directorCommandAllowed(commands, command) {
  return commands?.[command]?.permission === true
}

export function missingDirectorCommands(commands, requiredCommands) {
  return requiredCommands.filter(command => !directorCommandAllowed(commands, command))
}

export function directorCommandCategory(command) {
  if (command.startsWith('.bvfs_')) return 'BVFS'
  if (command.startsWith('.')) return 'Dot commands'
  return 'Commands'
}

export function normaliseDirectorCommandPermissions(commands) {
  return Object.entries(commands ?? {})
    .map(([command, meta]) => ({
      command,
      description: meta?.description ?? '',
      arguments: meta?.arguments ?? '',
      permission: meta?.permission === true,
      category: directorCommandCategory(command),
    }))
    .sort((a, b) => a.command.localeCompare(b.command))
}

/**
 * Normalise a raw director job record (from "list jobs") to the shape used
 * by the UI tables.
 */
export function normaliseJob(j) {
  const endtime = j.endtime ?? j.realendtime ?? ''
  return {
    id:        Number(j.jobid ?? j.id),
    name:      j.name       ?? '',
    client:    j.client     ?? j.clientname ?? '',
    type:      j.jobtype    ?? j.type       ?? '',
    level:     j.joblevel   ?? j.level      ?? '',
    status:    j.jobstatus  ?? j.status     ?? '',
    starttime: j.starttime  ?? '',
    endtime:   endtime.startsWith('0000') ? '' : endtime,
    duration:  j.duration   ?? '',
    files:     Number(j.jobfiles   ?? j.files   ?? 0),
    bytes:     Number(j.jobbytes   ?? j.bytes   ?? 0),
    errors:    Number(j.joberrors  ?? j.errors  ?? 0),
  }
}

/**
 * Normalise a raw director client record (from "list clients").
 */
// Parse the uname string sent by the file daemon during job handshake.
// Format: "{bareos_version} ({date}) {os_info},{platform}"
// e.g.  "23.0.0 (01Jan24) Debian GNU/Linux 12 (bookworm),x86_64-pc-linux-gnu"
function parseUname(raw) {
  if (!raw || !raw.trim()) return { version: '', buildDate: '', os: 'linux', osInfo: '', arch: '' }
  const lower = raw.toLowerCase()
  const os = lower.includes('windows') ? 'windows'
           : lower.includes('darwin') || lower.includes('macos') || lower.includes('mac os') ? 'macos'
           : 'linux'
  // Version: first token, strip optional "bareos-fd-" prefix
  const tokens = raw.trimStart().split(/\s+/)
  const version = (tokens[0] ?? '').replace(/^bareos-fd-/i, '')
  // Build date: second token if it matches "(DDMonYY)" pattern
  const buildDate = /^\(\d{2}\w{3}\d{2,4}\)$/.test(tokens[1] ?? '') ? tokens[1].replace(/[()]/g, '') : ''
  // OS info + arch: everything after version + date
  const afterDate = buildDate ? raw.slice(raw.indexOf(tokens[1]) + tokens[1].length).trim() : ''
  const commaIdx = afterDate.lastIndexOf(',')
  const osInfo = commaIdx >= 0 ? afterDate.slice(0, commaIdx).trim() : afterDate
  // Arch: first segment of platform string (e.g. "x86_64" from "x86_64-pc-linux-gnu")
  const platform = commaIdx >= 0 ? afterDate.slice(commaIdx + 1).trim() : ''
  const arch = platform.split('-')[0] ?? ''
  return { version, buildDate, os, osInfo, arch }
}

export function normaliseClient(c) {
  const uname = c.uname ?? c.uagent ?? ''
  const { version, buildDate, os, osInfo, arch } = parseUname(uname)
  return {
    clientid:  c.clientid ?? '',
    name:      c.name ?? '',
    uname,
    version:   c.version || version,
    buildDate,
    enabled:   c.enabled !== '0' && c.enabled !== false,
    address:   c.address ?? '',
    port:      c.fdport ? Number(c.fdport) : (c.port ? Number(c.port) : 9102),
    fileretention: c.fileretention ?? '',
    jobretention:  c.jobretention  ?? '',
    os,
    osInfo,
    arch,
  }
}

/**
 * Normalise a raw volume record.  Volumes can come from either
 * "list volumes" (nested by pool) or "llist volumes".
 */
export function normaliseVolume(v) {
  return {
    volumename:  v.volumename  ?? v.volume ?? '',
    pool:        v.pool        ?? v.Pool   ?? v.poolname ?? '',
    storage:     v.storage     ?? v.storagename ?? '',
    mediatype:   v.mediatype   ?? v.MediaType ?? '',
    volstatus:   v.volstatus   ?? v.VolStatus ?? v.status ?? '',
    volbytes:    Number(v.volbytes   ?? v.VolBytes ?? v.bytes ?? 0),
    volfiles:    Number(v.volfiles   ?? v.VolFiles ?? 0),
    maxvolbytes: Number(v.maxvolbytes ?? v.MaxVolBytes ?? 0),
    lastwritten: v.lastwritten ?? v.LastWritten ?? '',
    inchanger:   v.inchanger === '1' || v.inchanger === true,
    retention:   v.volretention ?? v.VolRetention ?? '',
    slot:        Number(v.slot ?? v.Slot ?? 0),
    enabled:     v.enabled !== '0' && v.enabled !== false,
  }
}
