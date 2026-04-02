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
 */

#include "dird/job_rrd.h"

#include <rrd.h>

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "lib/bsys.h"   /* SecureErase / Dmsg / Jmsg */
#include "lib/message.h"

namespace directordaemon {

/* Seconds per step — must match ProgressInterval default (30s). */
static constexpr unsigned kStep = 30;

/* Heartbeat: three missed steps before marking UNKNOWN. */
static constexpr unsigned kHeartbeat = kStep * 3;

/* ── helpers ─────────────────────────────────────────────────────────── */

/** Replace characters that are illegal in file-system names with '_'. */
static std::string SanitiseName(const char* raw)
{
  std::string out(raw);
  for (char& c : out) {
    if (c == '/' || c == '\\' || c == ':' || c == '"' || c == ' ') { c = '_'; }
  }
  return out;
}

/** mkdir -p for a single path component (ignores EEXIST). */
static bool EnsureDir(const std::string& path)
{
  if (mkdir(path.c_str(), 0750) != 0 && errno != EEXIST) {
    Dmsg2(100, "RRD: mkdir(%s) failed: %s\n", path.c_str(), strerror(errno));
    return false;
  }
  return true;
}

/** Return true if the file at @path already exists. */
static bool FileExists(const std::string& path)
{
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

/**
 * Wrapper around rrd_update_r().
 * @param path   Full path to the .rrd file.
 * @param tmpl   DS template string, e.g. "job_files:job_bytes:read_bytes".
 * @param values Update string, e.g. "N:10:1234:5678".
 * Returns true on success.
 */
static bool RrdUpdate(const std::string& path,
                      const char* tmpl,
                      const std::string& values)
{
  const char* argv[2] = {values.c_str(), nullptr};
  rrd_clear_error();
  int rc = rrd_update_r(path.c_str(), tmpl, 1, argv);
  if (rc != 0) {
    Dmsg2(100, "RRD update %s failed: %s\n", path.c_str(), rrd_get_error());
    return false;
  }
  return true;
}

/* ── per-job RRD ─────────────────────────────────────────────────────── */

/**
 * Create a new per-job RRD.
 *
 * DS:  job_files   GAUGE  heartbeat  0  U
 *      job_bytes   GAUGE  heartbeat  0  U
 *      read_bytes  GAUGE  heartbeat  0  U
 *
 * RRA: AVERAGE  0.5  1    (1 row per step)  5760  → 48 h at 30 s/step
 *      AVERAGE  0.5  10   (10 steps = 5 min) 4032 → 2 weeks
 */
static bool CreateJobRrd(const std::string& path)
{
  char hb[32];
  snprintf(hb, sizeof(hb), "%u", kHeartbeat);

  char step_s[16];
  snprintf(step_s, sizeof(step_s), "%u", kStep);

  /* rrd_create_r args: filename, step, start_time, argc, argv */
  const char* argv[] = {
      "DS:job_files:GAUGE:" ,  hb, ":0:U",
      "DS:job_bytes:GAUGE:" ,  hb, ":0:U",
      "DS:read_bytes:GAUGE:",  hb, ":0:U",
      /* 48h at 30s */
      "RRA:AVERAGE:0.5:1:5760",
      /* 2 weeks at 5min */
      "RRA:AVERAGE:0.5:10:4032",
      nullptr
  };

  /* Build a flat argv array (rrd_create_r takes const char**). */
  std::vector<const char*> flat;
  flat.push_back("DS:job_files:GAUGE:" + std::string(hb) + ":0:U");
  flat.push_back("DS:job_bytes:GAUGE:" + std::string(hb) + ":0:U");
  flat.push_back("DS:read_bytes:GAUGE:" + std::string(hb) + ":0:U");
  flat.push_back("RRA:AVERAGE:0.5:1:5760");
  flat.push_back("RRA:AVERAGE:0.5:10:4032");
  (void)argv; /* suppress unused-variable warning */

  std::vector<const char*> raw;
  for (const auto& s : flat) { raw.push_back(s.c_str()); }

  rrd_clear_error();
  int rc = rrd_create_r(path.c_str(), kStep, time(nullptr) - 1,
                        static_cast<int>(raw.size()), raw.data());
  if (rc != 0) {
    Dmsg2(100, "RRD create %s failed: %s\n", path.c_str(), rrd_get_error());
    return false;
  }
  return true;
}

void RrdUpdateJobProgress(const char* rrd_dir,
                          uint32_t jobid,
                          uint32_t files,
                          uint64_t bytes,
                          uint64_t read_bytes)
{
  if (!rrd_dir || rrd_dir[0] == '\0') { return; }

  const std::string base(rrd_dir);
  EnsureDir(base);
  const std::string jobs_dir = base + "/jobs";
  EnsureDir(jobs_dir);

  char name[64];
  snprintf(name, sizeof(name), "job_%u.rrd", jobid);
  const std::string path = jobs_dir + "/" + name;

  if (!FileExists(path)) {
    if (!CreateJobRrd(path)) { return; }
  }

  char values[128];
  snprintf(values, sizeof(values), "N:%" PRIu32 ":%" PRIu64 ":%" PRIu64,
           files, bytes, read_bytes);

  RrdUpdate(path, "job_files:job_bytes:read_bytes", values);
}

/* ── per-device RRD ──────────────────────────────────────────────────── */

/**
 * Create a new per-device RRD.
 *
 * DS:  write_rate  GAUGE  heartbeat  0  U
 *      read_rate   GAUGE  heartbeat  0  U
 *      vol_bytes   GAUGE  heartbeat  0  U
 *      spool_size  GAUGE  heartbeat  0  U
 *
 * RRA: AVERAGE  0.5  1     5760  → 48 h at 30 s/step
 *      AVERAGE  0.5  10    8928  → 1 month at 5 min
 *      AVERAGE  0.5  120  17520  → 2 years at 1 h
 */
static bool CreateDeviceRrd(const std::string& path)
{
  char hb[32];
  snprintf(hb, sizeof(hb), "%u", kHeartbeat);

  std::vector<const char*> flat;
  flat.push_back("DS:write_rate:GAUGE:" + std::string(hb) + ":0:U");
  flat.push_back("DS:read_rate:GAUGE:"  + std::string(hb) + ":0:U");
  flat.push_back("DS:vol_bytes:GAUGE:"  + std::string(hb) + ":0:U");
  flat.push_back("DS:spool_size:GAUGE:" + std::string(hb) + ":0:U");
  flat.push_back("RRA:AVERAGE:0.5:1:5760");
  flat.push_back("RRA:AVERAGE:0.5:10:8928");
  flat.push_back("RRA:AVERAGE:0.5:120:17520");

  std::vector<const char*> raw;
  for (const auto& s : flat) { raw.push_back(s.c_str()); }

  rrd_clear_error();
  int rc = rrd_create_r(path.c_str(), kStep, time(nullptr) - 1,
                        static_cast<int>(raw.size()), raw.data());
  if (rc != 0) {
    Dmsg2(100, "RRD create %s failed: %s\n", path.c_str(), rrd_get_error());
    return false;
  }
  return true;
}

void RrdUpdateDeviceProgress(const char* rrd_dir,
                             const char* devname,
                             uint64_t write_rate,
                             uint64_t read_rate,
                             uint64_t vol_bytes,
                             uint64_t spool_size)
{
  if (!rrd_dir || rrd_dir[0] == '\0') { return; }

  const std::string base(rrd_dir);
  EnsureDir(base);
  const std::string dev_dir = base + "/devices";
  EnsureDir(dev_dir);

  const std::string safe = SanitiseName(devname);
  const std::string path = dev_dir + "/dev_" + safe + ".rrd";

  if (!FileExists(path)) {
    if (!CreateDeviceRrd(path)) { return; }
  }

  char values[128];
  snprintf(values, sizeof(values),
           "N:%" PRIu64 ":%" PRIu64 ":%" PRIu64 ":%" PRIu64,
           write_rate, read_rate, vol_bytes, spool_size);

  RrdUpdate(path, "write_rate:read_rate:vol_bytes:spool_size", values);
}

/* ── purge ───────────────────────────────────────────────────────────── */

void RrdDeleteJobFiles(const char* rrd_dir, const char* jobids)
{
  if (!rrd_dir || rrd_dir[0] == '\0') { return; }
  if (!jobids || jobids[0] == '\0') { return; }

  const std::string jobs_dir = std::string(rrd_dir) + "/jobs";

  /* Parse the comma-separated jobids string. */
  const char* p = jobids;
  while (*p) {
    while (*p == ' ' || *p == ',') { ++p; }
    if (*p == '\0') { break; }

    char* end;
    unsigned long id = strtoul(p, &end, 10);
    p = end;

    if (id == 0) { continue; }

    char name[64];
    snprintf(name, sizeof(name), "/job_%lu.rrd", id);
    const std::string path = jobs_dir + name;

    if (FileExists(path)) {
      if (unlink(path.c_str()) != 0) {
        Dmsg2(100, "RRD: unlink(%s) failed: %s\n", path.c_str(),
              strerror(errno));
      } else {
        Dmsg1(200, "RRD: deleted %s\n", path.c_str());
      }
    }
  }
}

}  // namespace directordaemon
