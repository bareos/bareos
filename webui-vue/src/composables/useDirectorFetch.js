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
import { ref, onMounted } from 'vue'
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
      // Unwrap the requested key, or return the full dict
      data.value = key ? (result?.[key] ?? init) : (result ?? init)
    } catch (e) {
      error.value = e.message
    } finally {
      loading.value = false
    }
  }

  onMounted(refresh)

  return { data, loading, error, refresh }
}

// ── Field-name normalisers for catalog data ──────────────────────────────────

/**
 * Normalise a raw director job record (from "list jobs") to the shape used
 * by the UI tables.
 */
export function normaliseJob(j) {
  return {
    id:        Number(j.jobid ?? j.id),
    name:      j.name       ?? '',
    client:    j.client     ?? j.clientname ?? '',
    type:      j.jobtype    ?? j.type       ?? '',
    level:     j.joblevel   ?? j.level      ?? '',
    status:    j.jobstatus  ?? j.status     ?? '',
    starttime: j.starttime  ?? '',
    endtime:   j.endtime    ?? '',
    duration:  j.duration   ?? '',
    files:     Number(j.jobfiles   ?? j.files   ?? 0),
    bytes:     Number(j.jobbytes   ?? j.bytes   ?? 0),
    errors:    Number(j.joberrors  ?? j.errors  ?? 0),
  }
}

/**
 * Normalise a raw director client record (from "list clients").
 */
export function normaliseClient(c) {
  return {
    clientid:  c.clientid ?? '',
    name:      c.name ?? '',
    uname:     c.uname ?? c.uagent ?? '',
    version:   c.version ?? '',
    enabled:   c.enabled !== '0' && c.enabled !== false,
    address:   c.address ?? '',
    port:      c.fdport ? Number(c.fdport) : 9102,
    fileretention: c.fileretention ?? '',
    jobretention:  c.jobretention  ?? '',
    os:        c.os ?? 'linux',
  }
}

/**
 * Normalise a raw volume record.  Volumes can come from either
 * "list volumes" (nested by pool) or "llist volumes".
 */
export function normaliseVolume(v) {
  return {
    volumename:  v.volumename ?? v.volume ?? '',
    pool:        v.pool ?? v.poolname ?? '',
    storage:     v.storage ?? v.storagename ?? '',
    mediatype:   v.mediatype ?? '',
    volstatus:   v.volstatus ?? v.status ?? '',
    volbytes:    Number(v.volbytes   ?? v.bytes ?? 0),
    volfiles:    Number(v.volfiles   ?? 0),
    lastwritten: v.lastwritten ?? '',
    inchanger:   v.inchanger === '1' || v.inchanger === true,
    retention:   v.volretention ?? '',
    slot:        Number(v.slot ?? 0),
    enabled:     v.enabled !== '0' && v.enabled !== false,
  }
}
