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
/** @file
 * RRD time-series recording for job progress and device throughput.
 *
 * Per-job RRDs (rrd_dir/jobs/job_<id>.rrd):
 *   - Lifetime tied to the job: created on first progress update, deleted
 *     when the job is purged from the catalog.
 *   - DS: job_files, job_bytes, read_bytes (all GAUGE)
 *   - RRA: 30s for 48h, 5min for 2 weeks
 *
 * Per-device RRDs (rrd_dir/devices/dev_<name>.rrd):
 *   - Long-lived; created on first use, never deleted automatically.
 *   - DS: write_rate, read_rate, vol_bytes, spool_size (all GAUGE)
 *   - RRA: 30s for 48h, 5min for 1 month, 1h for 2 years
 */

#ifndef BAREOS_DIRD_JOB_RRD_H_
#define BAREOS_DIRD_JOB_RRD_H_

#include <cstdint>

namespace directordaemon {

/**
 * Update (or create) the per-job RRD with latest progress counters.
 * @param rrd_dir  Base directory for RRD files (from Director config).
 * @param jobid    Numeric job ID.
 * @param files    Number of files processed so far.
 * @param bytes    Bytes written to media so far.
 * @param read_bytes  Uncompressed bytes read (from attribute st_size sum).
 */
void RrdUpdateJobProgress(const char* rrd_dir,
                          uint32_t jobid,
                          uint32_t files,
                          uint64_t bytes,
                          uint64_t read_bytes);

/**
 * Update (or create) the per-device RRD with latest throughput.
 * @param rrd_dir    Base directory for RRD files.
 * @param devname    Device name (will be sanitised for use as a filename).
 * @param write_rate Instantaneous write rate in bytes/s.
 * @param read_rate  Instantaneous read rate in bytes/s.
 * @param vol_bytes  Total bytes written to the current volume.
 * @param spool_size Current spool file size in bytes.
 */
void RrdUpdateDeviceProgress(const char* rrd_dir,
                             const char* devname,
                             uint64_t write_rate,
                             uint64_t read_rate,
                             uint64_t vol_bytes,
                             uint64_t spool_size);

/**
 * Delete per-job RRD files for the given comma-separated list of job IDs.
 * Called from PurgeJobsFromCatalog() before the catalog records are removed.
 * @param rrd_dir  Base directory for RRD files.
 * @param jobids   Comma-separated list of numeric job IDs, e.g. "1,5,42".
 */
void RrdDeleteJobFiles(const char* rrd_dir, const char* jobids);

}  // namespace directordaemon

#endif  // BAREOS_DIRD_JOB_RRD_H_
