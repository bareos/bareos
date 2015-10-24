/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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
/*
 * Storage specific NDMP Data Management Application (DMA) routines
 *
 * Marco van Wieringen, May 2015
 */

#include "bareos.h"
#include "dird.h"

#if HAVE_NDMP

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"

/* Imported variables */

/* Forward referenced functions */

/*
 * Output the status of a storage daemon when its a normal storage
 * daemon accessed via the NDMP protocol or query the TAPE and ROBOT
 * agent of a native NDMP server.
 */
void do_ndmp_storage_status(UAContext *ua, STORERES *store, char *cmd)
{
   /*
    * See if the storage is just a NDMP instance of a normal storage daemon.
    */
   if (store->paired_storage) {
      do_native_storage_status(ua, store->paired_storage, cmd);
   } else {
      struct ndm_job_param ndmp_job;

      if (!ndmp_build_storage_job(ua->jcr,
                                  store,
                                  true, /* Query Tape Agent */
                                  true, /* Query Robot Agent */
                                  NDM_JOB_OP_QUERY_AGENTS,
                                  &ndmp_job)) {
         return;
      }

      ndmp_do_query(ua, &ndmp_job, me->ndmp_loglevel);
   }
}
#else
void do_ndmp_storage_status(UAContext *ua, STORERES *store, char *cmd)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}
#endif /* HAVE_NDMP */
