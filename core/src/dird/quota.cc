/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
// Matthew Ife Matthew.Ife@ukfast.co.uk
/**
 * @file
 * Quota processing routines.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/director_jcr_impl.h"

namespace directordaemon {

#define debuglevel 100
/**
 * This function returns the total number of bytes difference remaining before
 * going over quota. Returns: unsigned long long containing remaining bytes
 * before going over quota. 0 if over quota
 */
uint64_t FetchRemainingQuotas(JobControlRecord* jcr)
{
  uint64_t remaining = 0;
  uint64_t now = (uint64_t)time(NULL);

  // Quotas not being used ?
  if (!jcr->dir_impl->HasQuota) { return 0; }

  Dmsg2(debuglevel, "JobSumTotalBytes for JobId %d is %" PRIu64 "\n",
        jcr->JobId, jcr->dir_impl->jr.JobSumTotalBytes);
  Dmsg1(debuglevel, "Fetching remaining quotas for JobId %d\n", jcr->JobId);

  // If strict quotas on and grace exceeded, enforce the softquota
  if (jcr->dir_impl->res.client->StrictQuotas
      && jcr->dir_impl->res.client->SoftQuota
      && jcr->dir_impl->res.client->GraceTime > 0
      && (now - (uint64_t)jcr->dir_impl->res.client->GraceTime)
             > (uint64_t)jcr->dir_impl->res.client->SoftQuotaGracePeriod
      && jcr->dir_impl->res.client->SoftQuotaGracePeriod > 0) {
    remaining = jcr->dir_impl->res.client->SoftQuota
                - jcr->dir_impl->jr.JobSumTotalBytes;
  } else if (!jcr->dir_impl->res.client->StrictQuotas
             && jcr->dir_impl->res.client->SoftQuota
             && jcr->dir_impl->res.client->GraceTime > 0
             && jcr->dir_impl->res.client->SoftQuotaGracePeriod > 0
             && (now - (uint64_t)jcr->dir_impl->res.client->GraceTime)
                    > (uint64_t)
                          jcr->dir_impl->res.client->SoftQuotaGracePeriod) {
    // If strict quotas turned off and grace exceeded use the last known limit
    if (jcr->dir_impl->res.client->QuotaLimit
        > jcr->dir_impl->res.client->SoftQuota) {
      remaining = jcr->dir_impl->res.client->QuotaLimit
                  - jcr->dir_impl->jr.JobSumTotalBytes;
    } else {
      remaining = jcr->dir_impl->res.client->SoftQuota
                  - jcr->dir_impl->jr.JobSumTotalBytes;
    }
  } else if (jcr->dir_impl->jr.JobSumTotalBytes
             < jcr->dir_impl->res.client->HardQuota) {
    // If within the hardquota.
    remaining = jcr->dir_impl->res.client->HardQuota
                - jcr->dir_impl->jr.JobSumTotalBytes;
  } else {
    /* If just over quota return 0. This shouldnt happen because quotas
     * are checked properly prior to this code. */
    remaining = 0;
  }

  Dmsg4(debuglevel,
        "Quota for %s is %" PRIu64 ". Remainder is %" PRIu64
        ", QuotaLimit: %" PRIu64 "\n",
        jcr->dir_impl->jr.Name, jcr->dir_impl->jr.JobSumTotalBytes, remaining,
        jcr->dir_impl->res.client->QuotaLimit);

  return remaining;
}

/**
 * This function returns a truth value depending on the state of hard quotas.
 * The function compares the total jobbytes against the hard quota.
 * If the value is true, the quota is reached and termination of the job should
 * occur.
 *
 * Returns: true on reaching quota
 *          false on not reaching quota.
 */
bool CheckHardquotas(JobControlRecord* jcr)
{
  bool retval = false;

  // Do not check if hardquota is not set
  if (jcr->dir_impl->res.client->HardQuota == 0) { goto bail_out; }

  Dmsg1(debuglevel, "Checking hard quotas for JobId %d\n", jcr->JobId);
  if (!jcr->dir_impl->HasQuota) {
    if (jcr->dir_impl->res.client->QuotaIncludeFailedJobs) {
      if (!jcr->db->get_quota_jobbytes(
              jcr, &jcr->dir_impl->jr,
              jcr->dir_impl->res.client->JobRetention)) {
        Jmsg(jcr, M_WARNING, 0, T_("Error getting Quota value: ERR=%s\n"),
             jcr->db->strerror());
        goto bail_out;
      }
    } else {
      if (!jcr->db->get_quota_jobbytes_nofailed(
              jcr, &jcr->dir_impl->jr,
              jcr->dir_impl->res.client->JobRetention)) {
        Jmsg(jcr, M_WARNING, 0, T_("Error getting Quota value: ERR=%s\n"),
             jcr->db->strerror());
        goto bail_out;
      }
    }
    jcr->dir_impl->HasQuota = true;
  }

  if (jcr->dir_impl->jr.JobSumTotalBytes
      > jcr->dir_impl->res.client->HardQuota) {
    retval = true;
    goto bail_out;
  }

  Dmsg2(debuglevel, "Quota for JobID: %d is %" PRIu64 "\n",
        jcr->dir_impl->jr.JobId, jcr->dir_impl->jr.JobSumTotalBytes);

bail_out:
  return retval;
}

/**
 * This function returns a truth value depending on the state of soft quotas.
 * The function compares the total jobbytes against the soft quota.
 *
 * If the quotas are not strict (the default) it checks the jobbytes value
 * against the quota limit previously when running in burst mode during the
 * grace period.
 *
 * It checks if we have exceeded our grace time.
 *
 * If the value is true, the quota is reached and termination of the job should
 * occur.
 *
 * Returns: true on reaching soft quota
 *          false on not reaching soft quota.
 */
bool CheckSoftquotas(JobControlRecord* jcr)
{
  bool retval = false;
  uint64_t now = (uint64_t)time(NULL);

  // Do not check if the softquota is not set
  if (jcr->dir_impl->res.client->SoftQuota == 0) { goto bail_out; }

  Dmsg1(debuglevel, "Checking soft quotas for JobId %d\n", jcr->JobId);
  if (!jcr->dir_impl->HasQuota) {
    if (jcr->dir_impl->res.client->QuotaIncludeFailedJobs) {
      if (!jcr->db->get_quota_jobbytes(
              jcr, &jcr->dir_impl->jr,
              jcr->dir_impl->res.client->JobRetention)) {
        Jmsg(jcr, M_WARNING, 0, T_("Error getting Quota value: ERR=%s\n"),
             jcr->db->strerror());
        goto bail_out;
      }
      Dmsg0(debuglevel, "Quota Includes Failed Jobs\n");
    } else {
      if (!jcr->db->get_quota_jobbytes_nofailed(
              jcr, &jcr->dir_impl->jr,
              jcr->dir_impl->res.client->JobRetention)) {
        Jmsg(jcr, M_WARNING, 0, T_("Error getting Quota value: ERR=%s\n"),
             jcr->db->strerror());
        goto bail_out;
      }
      Jmsg(jcr, M_INFO, 0, T_("Quota does NOT include Failed Jobs\n"));
    }
    jcr->dir_impl->HasQuota = true;
  }

  Dmsg2(debuglevel, "Quota for %s is %" PRIu64 "\n", jcr->dir_impl->jr.Name,
        jcr->dir_impl->jr.JobSumTotalBytes);
  Dmsg2(debuglevel, "QuotaLimit for %s is %" PRIu64 "\n",
        jcr->dir_impl->jr.Name, jcr->dir_impl->res.client->QuotaLimit);
  Dmsg2(debuglevel, "HardQuota for %s is %" PRIu64 "\n", jcr->dir_impl->jr.Name,
        jcr->dir_impl->res.client->HardQuota);
  Dmsg2(debuglevel, "SoftQuota for %s is %" PRIu64 "\n", jcr->dir_impl->jr.Name,
        jcr->dir_impl->res.client->SoftQuota);
  Dmsg2(debuglevel, "SoftQuota Grace Period for %s is %" PRId64 "\n",
        jcr->dir_impl->jr.Name,
        jcr->dir_impl->res.client->SoftQuotaGracePeriod);
  Dmsg2(debuglevel, "SoftQuota Grace Time for %s is %" PRId64 "\n",
        jcr->dir_impl->jr.Name, jcr->dir_impl->res.client->GraceTime);

  if ((jcr->dir_impl->jr.JobSumTotalBytes + jcr->dir_impl->SDJobBytes)
      > jcr->dir_impl->res.client->SoftQuota) {
    /* Only warn once about softquotas in the job
     * Check if gracetime has been set */
    if (jcr->dir_impl->res.client->GraceTime == 0
        && jcr->dir_impl->res.client->SoftQuotaGracePeriod) {
      Dmsg1(debuglevel, "UpdateQuotaGracetime: %" PRId64 "\n", now);
      if (!jcr->db->UpdateQuotaGracetime(jcr, &jcr->dir_impl->jr)) {
        Jmsg(jcr, M_WARNING, 0, T_("Error setting Quota gracetime: ERR=%s\n"),
             jcr->db->strerror());
      } else {
        Jmsg(jcr, M_ERROR, 0,
             T_("Softquota Exceeded, Grace Period starts now.\n"));
      }
      jcr->dir_impl->res.client->GraceTime = now;
      goto bail_out;
    } else if (jcr->dir_impl->res.client->SoftQuotaGracePeriod
               && (now - (uint64_t)jcr->dir_impl->res.client->GraceTime)
                      < (uint64_t)
                            jcr->dir_impl->res.client->SoftQuotaGracePeriod) {
      Jmsg(jcr, M_ERROR, 0,
           T_("Softquota Exceeded, will be enforced after Grace Period "
              "expires.\n"));
    } else if (jcr->dir_impl->res.client->SoftQuotaGracePeriod
               && (now - (uint64_t)jcr->dir_impl->res.client->GraceTime)
                      > (uint64_t)
                            jcr->dir_impl->res.client->SoftQuotaGracePeriod) {
      /* If gracetime has expired update else check more if not set softlimit
       * yet then set and bail out. */
      if (jcr->dir_impl->res.client->QuotaLimit < 1) {
        if (!jcr->db->UpdateQuotaSoftlimit(jcr, &jcr->dir_impl->jr)) {
          Jmsg(jcr, M_WARNING, 0, T_("Error setting Quota Softlimit: ERR=%s\n"),
               jcr->db->strerror());
        }
        Jmsg(jcr, M_WARNING, 0,
             T_("Softquota Exceeded and Grace Period expired.\n"));
        Jmsg(jcr, M_INFO, 0, T_("Setting Burst Quota to %" PRIu64 " Bytes.\n"),
             jcr->dir_impl->jr.JobSumTotalBytes);
        jcr->dir_impl->res.client->QuotaLimit
            = jcr->dir_impl->jr.JobSumTotalBytes;
        retval = true;
        goto bail_out;
      } else {
        /* If gracetime has expired update else check more if not set softlimit
         * yet then set and bail out. */
        if (jcr->dir_impl->res.client->QuotaLimit < 1) {
          if (!jcr->db->UpdateQuotaSoftlimit(jcr, &jcr->dir_impl->jr)) {
            Jmsg(jcr, M_WARNING, 0,
                 T_("Error setting Quota Softlimit: ERR=%s\n"),
                 jcr->db->strerror());
          }
          Jmsg(jcr, M_WARNING, 0,
               T_("Soft Quota exceeded and Grace Period expired.\n"));
          Jmsg(jcr, M_INFO, 0,
               T_("Setting Burst Quota to %" PRIu64 " Bytes.\n"),
               jcr->dir_impl->jr.JobSumTotalBytes);
          jcr->dir_impl->res.client->QuotaLimit
              = jcr->dir_impl->jr.JobSumTotalBytes;
          retval = true;
          goto bail_out;
        } else {
          // If we use strict quotas enforce the pure soft quota limit.
          if (jcr->dir_impl->res.client->StrictQuotas) {
            if (jcr->dir_impl->jr.JobSumTotalBytes
                > jcr->dir_impl->res.client->SoftQuota) {
              Dmsg0(debuglevel,
                    "Soft Quota exceeded, enforcing Strict Quota Limit.\n");
              retval = true;
              goto bail_out;
            }
          } else {
            if (jcr->dir_impl->jr.JobSumTotalBytes
                >= jcr->dir_impl->res.client->QuotaLimit) {
              // If strict quotas turned off use the last known limit
              Jmsg(jcr, M_WARNING, 0,
                   T_("Soft Quota exceeded, enforcing Burst Quota Limit.\n"));
              retval = true;
              goto bail_out;
            }
          }
        }
      }
    }
  } else if (jcr->dir_impl->res.client->GraceTime != 0) {
    // Reset softquota
    ClientDbRecord cr;
    cr.ClientId = jcr->dir_impl->jr.ClientId;
    if (!jcr->db->ResetQuotaRecord(jcr, &cr)) {
      Jmsg(jcr, M_WARNING, 0, T_("Error setting Quota gracetime: ERR=%s\n"),
           jcr->db->strerror());
    } else {
      jcr->dir_impl->res.client->GraceTime = 0;
      Jmsg(jcr, M_INFO, 0, T_("Soft Quota reset, Grace Period ends now.\n"));
    }
  }

bail_out:
  return retval;
}

} /* namespace directordaemon */
