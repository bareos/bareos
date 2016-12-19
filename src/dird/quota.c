/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
/*
 * Quota procesing routines.
 *
 * Matthew Ife Matthew.Ife@ukfast.co.uk
 */

#include "bareos.h"
#include "dird.h"

#define dbglvl 100
/*
 * This function returns the total number of bytes difference remaining before going over quota.
 * Returns: unsigned long long containing remaining bytes before going over quota.
 *          0 if over quota
 */
uint64_t fetch_remaining_quotas(JCR *jcr)
{
   uint64_t remaining = 0;
   uint64_t now = (uint64_t)time(NULL);

   /*
    * Quotas not being used ?
    */
   if (!jcr->HasQuota) {
      return 0;
   }

   Dmsg2(dbglvl, "JobSumTotalBytes for JobId %d is %llu\n", jcr->JobId, jcr->jr.JobSumTotalBytes);
   Dmsg1(dbglvl, "Fetching remaining quotas for JobId %d\n", jcr->JobId);

   /*
    * If strict quotas on and grace exceeded, enforce the softquota
    */
   if (jcr->res.client->StrictQuotas &&
       jcr->res.client->SoftQuota &&
       jcr->res.client->GraceTime > 0 &&
      (now - (uint64_t)jcr->res.client->GraceTime) > (uint64_t)jcr->res.client->SoftQuotaGracePeriod &&
       jcr->res.client->SoftQuotaGracePeriod > 0) {
      remaining = jcr->res.client->SoftQuota - jcr->jr.JobSumTotalBytes;
   } else if (!jcr->res.client->StrictQuotas &&
              jcr->res.client->SoftQuota &&
              jcr->res.client->GraceTime > 0 &&
              jcr->res.client->SoftQuotaGracePeriod > 0 &&
             (now - (uint64_t)jcr->res.client->GraceTime) > (uint64_t)jcr->res.client->SoftQuotaGracePeriod) {
      /*
       * If strict quotas turned off and grace exceeded use the last known limit
       */
      if (jcr->res.client->QuotaLimit > jcr->res.client->SoftQuota) {
         remaining = jcr->res.client->QuotaLimit - jcr->jr.JobSumTotalBytes;
      } else {
         remaining = jcr->res.client->SoftQuota - jcr->jr.JobSumTotalBytes;
      }
   } else if (jcr->jr.JobSumTotalBytes < jcr->res.client->HardQuota) {
      /*
       * If within the hardquota.
       */
      remaining = jcr->res.client->HardQuota - jcr->jr.JobSumTotalBytes;
   } else {
      /*
       * If just over quota return 0. This shouldnt happen because quotas
       * are checked properly prior to this code.
       */
      remaining = 0;
   }

   Dmsg4(dbglvl, "Quota for %s is %llu. Remainder is %llu, QuotaLimit: %llu\n",
         jcr->jr.Name, jcr->jr.JobSumTotalBytes, remaining, jcr->res.client->QuotaLimit);

   return remaining;
}

/*
 * This function returns a truth value depending on the state of hard quotas.
 * The function compares the total jobbytes against the hard quota.
 * If the value is true, the quota is reached and termination of the job should occur.
 *
 * Returns: true on reaching quota
 *          false on not reaching quota.
 */
bool check_hardquotas(JCR *jcr)
{
   bool retval = false;

   /*
    * Do not check if hardquota is not set
    */
   if (jcr->res.client->HardQuota == 0) {
      goto bail_out;
   }

   Dmsg1(dbglvl, "Checking hard quotas for JobId %d\n", jcr->JobId);
   if (!jcr->HasQuota) {
      if (jcr->res.client->QuotaIncludeFailedJobs) {
         if (!jcr->db->get_quota_jobbytes(jcr, &jcr->jr, jcr->res.client->JobRetention)) {
            Jmsg(jcr, M_WARNING, 0, _("Error getting Quota value: ERR=%s"), jcr->db->strerror());
            goto bail_out;
         }
      } else {
         if (!jcr->db->get_quota_jobbytes_nofailed(jcr, &jcr->jr, jcr->res.client->JobRetention)) {
            Jmsg(jcr, M_WARNING, 0, _("Error getting Quota value: ERR=%s"), jcr->db->strerror());
            goto bail_out;
         }
      }
      jcr->HasQuota = true;
   }

   if (jcr->jr.JobSumTotalBytes > jcr->res.client->HardQuota) {
      retval = true;
      goto bail_out;
   }

   Dmsg2(dbglvl, "Quota for JobID: %d is %llu\n", jcr->jr.JobId, jcr->jr.JobSumTotalBytes);

bail_out:
   return retval;
}

/*
 * This function returns a truth value depending on the state of soft quotas.
 * The function compares the total jobbytes against the soft quota.
 *
 * If the quotas are not strict (the default) it checks the jobbytes value against
 * the quota limit previously when running in burst mode during the grace period.
 *
 * It checks if we have exceeded our grace time.
 *
 * If the value is true, the quota is reached and termination of the job should occur.
 *
 * Returns: true on reaching soft quota
 *          false on not reaching soft quota.
 */
bool check_softquotas(JCR *jcr)
{
   bool retval = false;
   uint64_t now = (uint64_t)time(NULL);

   /*
    * Do not check if the softquota is not set
    */
   if (jcr->res.client->SoftQuota == 0) {
      goto bail_out;
   }

   Dmsg1(dbglvl, "Checking soft quotas for JobId %d\n", jcr->JobId);
   if (!jcr->HasQuota) {
      if (jcr->res.client->QuotaIncludeFailedJobs) {
         if (!jcr->db->get_quota_jobbytes(jcr, &jcr->jr, jcr->res.client->JobRetention)) {
            Jmsg(jcr, M_WARNING, 0, _("Error getting Quota value: ERR=%s"), jcr->db->strerror());
            goto bail_out;
         }
         Dmsg0(dbglvl, "Quota Includes Failed Jobs\n");
      } else {
         if (!jcr->db->get_quota_jobbytes_nofailed(jcr, &jcr->jr, jcr->res.client->JobRetention)) {
            Jmsg(jcr, M_WARNING, 0, _("Error getting Quota value: ERR=%s"), jcr->db->strerror());
            goto bail_out;
         }
         Jmsg(jcr, M_INFO, 0, _("Quota does NOT include Failed Jobs\n"));
      }
      jcr->HasQuota = true;
   }

   Dmsg2(dbglvl, "Quota for %s is %llu\n", jcr->jr.Name, jcr->jr.JobSumTotalBytes);
   Dmsg2(dbglvl, "QuotaLimit for %s is %llu\n", jcr->jr.Name, jcr->res.client->QuotaLimit);
   Dmsg2(dbglvl, "HardQuota for %s is %llu\n", jcr->jr.Name, jcr->res.client->HardQuota);
   Dmsg2(dbglvl, "SoftQuota for %s is %llu\n", jcr->jr.Name, jcr->res.client->SoftQuota);
   Dmsg2(dbglvl, "SoftQuota Grace Period for %s is %d\n", jcr->jr.Name, jcr->res.client->SoftQuotaGracePeriod);
   Dmsg2(dbglvl, "SoftQuota Grace Time for %s is %d\n", jcr->jr.Name, jcr->res.client->GraceTime);

   if ((jcr->jr.JobSumTotalBytes + jcr->SDJobBytes) > jcr->res.client->SoftQuota) {
      /*
       * Only warn once about softquotas in the job
       * Check if gracetime has been set
       */
      if (jcr->res.client->GraceTime == 0 && jcr->res.client->SoftQuotaGracePeriod) {
         Dmsg1(dbglvl, "update_quota_gracetime: %d\n", now);
         if (!jcr->db->update_quota_gracetime(jcr, &jcr->jr)) {
            Jmsg(jcr, M_WARNING, 0, _("Error setting Quota gracetime: ERR=%s"), jcr->db->strerror());
         } else {
             Jmsg(jcr, M_ERROR, 0, _("Softquota Exceeded, Grace Period starts now.\n"));
         }
         jcr->res.client->GraceTime = now;
         goto bail_out;
      } else if (jcr->res.client->SoftQuotaGracePeriod &&
                (now - (uint64_t)jcr->res.client->GraceTime) < (uint64_t)jcr->res.client->SoftQuotaGracePeriod) {
         Jmsg(jcr, M_ERROR, 0, _("Softquota Exceeded, will be enforced after Grace Period expires.\n"));
      } else if (jcr->res.client->SoftQuotaGracePeriod &&
                (now - (uint64_t)jcr->res.client->GraceTime) > (uint64_t)jcr->res.client->SoftQuotaGracePeriod) {
         /*
          * If gracetime has expired update else check more if not set softlimit yet then set and bail out.
          */
         if (jcr->res.client->QuotaLimit < 1) {
           if (!jcr->db->update_quota_softlimit(jcr, &jcr->jr)) {
               Jmsg(jcr, M_WARNING, 0, _("Error setting Quota Softlimit: ERR=%s"), jcr->db->strerror());
           }
           Jmsg(jcr, M_WARNING, 0, _("Softquota Exceeded and Grace Period expired.\n"));
           Jmsg(jcr, M_INFO, 0, _("Setting Burst Quota to %d Bytes.\n"),
                jcr->jr.JobSumTotalBytes);
           jcr->res.client->QuotaLimit = jcr->jr.JobSumTotalBytes;
           retval = true;
           goto bail_out;
         } else {
            /*
             * If gracetime has expired update else check more if not set softlimit yet then set and bail out.
             */
            if (jcr->res.client->QuotaLimit < 1) {
               if (!db_update_quota_softlimit(jcr, jcr->db, &jcr->jr)) {
                  Jmsg(jcr, M_WARNING, 0, _("Error setting Quota Softlimit: ERR=%s"),
                        db_strerror(jcr->db));
               }
               Jmsg(jcr, M_WARNING, 0, _("Soft Quota exceeded and Grace Period expired.\n"));
               Jmsg(jcr, M_INFO, 0, _("Setting Burst Quota to %d Bytes.\n"),
                     jcr->jr.JobSumTotalBytes);
               jcr->res.client->QuotaLimit = jcr->jr.JobSumTotalBytes;
               retval = true;
               goto bail_out;
            } else {
               /*
                * If we use strict quotas enforce the pure soft quota limit.
                */
               if (jcr->res.client->StrictQuotas) {
                  if (jcr->jr.JobSumTotalBytes > jcr->res.client->SoftQuota) {
                     Dmsg0(dbglvl, "Soft Quota exceeded, enforcing Strict Quota Limit.\n");
                     retval = true;
                     goto bail_out;
                  }
               } else {
                  if (jcr->jr.JobSumTotalBytes >= jcr->res.client->QuotaLimit) {
                     /*
                      * If strict quotas turned off use the last known limit
                      */
                     Jmsg(jcr, M_WARNING, 0, _("Soft Quota exceeded, enforcing Burst Quota Limit.\n"));
                     retval = true;
                     goto bail_out;
                  }
               }
            }
         }
      }
   } else if (jcr->res.client->GraceTime != 0) {
      /*
       * Reset softquota
       */
      CLIENT_DBR cr;
      memset(&cr, 0, sizeof(cr));
      cr.ClientId = jcr->jr.ClientId;
      if (!db_reset_quota_record(jcr, jcr->db, &cr)) {
         Jmsg(jcr, M_WARNING, 0, _("Error setting Quota gracetime: ERR=%s\n"),
            db_strerror(jcr->db));
      } else {
         jcr->res.client->GraceTime = 0;
         Jmsg(jcr, M_INFO, 0, _("Soft Quota reset, Grace Period ends now.\n"));
      }
   }

bail_out:
   return retval;
}
