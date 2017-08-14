/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * Routines common to NDMP_BAREOS and NDMP_NATIVE backups
 * extracted from ndmp_dma_backup
 *
 * Philipp Storz, April 2017
 */

#include "bareos.h"
#include "dird.h"

#if HAVE_NDMP

#define NDMP_NEED_ENV_KEYWORDS 1

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"

/*
 * Fill the NDMP backup environment table with the data for the data agent to act on.
 */
bool fill_backup_environment(JCR *jcr,
                             INCEXE *ie,
                             char *filesystem,
                             struct ndm_job_param *job)
{
   int i, j, cnt;
   bool exclude;
   FOPTS *fo;
   ndmp9_pval pv;
   POOL_MEM pattern;
   POOL_MEM tape_device;
   ndmp_backup_format_option *nbf_options;

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = ndmp_lookup_backup_format_options(job->bu_type);

   if (!nbf_options || nbf_options->uses_file_history) {
      /*
       * We want to receive file history info from the NDMP backup.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_HIST];
      pv.value = ndmp_env_values[NDMP_ENV_VALUE_YES];
      ndma_store_env_list(&job->env_tab, &pv);
   } else {
      /*
       * We don't want to receive file history info from the NDMP backup.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_HIST];
      pv.value = ndmp_env_values[NDMP_ENV_VALUE_NO];
      ndma_store_env_list(&job->env_tab, &pv);
   }

   /*
    * Tell the data agent what type of backup to make.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_TYPE];
   pv.value = job->bu_type;
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * See if we are doing a backup type that uses dumplevels.
    */
   if (nbf_options && nbf_options->uses_level) {
      char text_level[50];

      /*
       * Set the dump level for the backup.
       */
      jcr->DumpLevel = native_to_ndmp_level(jcr, filesystem);
      job->bu_level = jcr->DumpLevel;
      if (job->bu_level == -1) {
         return false;
      }

      pv.name = ndmp_env_keywords[NDMP_ENV_KW_LEVEL];
      pv.value = edit_uint64(job->bu_level, text_level);
      ndma_store_env_list(&job->env_tab, &pv);

      /*
       * Update the dumpdates
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_UPDATE];
      pv.value = ndmp_env_values[NDMP_ENV_VALUE_YES];
      ndma_store_env_list(&job->env_tab, &pv);
   }

   /*
    * Tell the data engine what to backup.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_FILESYSTEM];
   pv.value = filesystem;
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * Loop over each option block for this fileset and append any
    * INCLUDE/EXCLUDE and/or META tags to the env_tab of the NDMP backup.
    */
   for (i = 0; i < ie->num_opts; i++) {
      fo = ie->opts_list[i];

      /*
       * Pickup any interesting patterns.
       */
      cnt = 0;
      pm_strcpy(pattern, "");
      for (j = 0; j < fo->wild.size(); j++) {
         if (cnt != 0) {
            pm_strcat(pattern, ",");
         }
         pm_strcat(pattern, (char *)fo->wild.get(j));
         cnt++;
      }
      for (j = 0; j < fo->wildfile.size(); j++) {
         if (cnt != 0) {
            pm_strcat(pattern, ",");
         }
         pm_strcat(pattern, (char *)fo->wildfile.get(j));
         cnt++;
      }
      for (j = 0; j < fo->wilddir.size(); j++) {
         if (cnt != 0) {
            pm_strcat(pattern, ",");
         }
         pm_strcat(pattern, (char *)fo->wilddir.get(j));
         cnt++;
      }

      /*
       * See if this is a INCLUDE or EXCLUDE block.
       */
      if (cnt > 0) {
         exclude = false;
         for (j = 0; fo->opts[j] != '\0'; j++) {
            if (fo->opts[j] == 'e') {
               exclude = true;
               break;
            }
         }

         if (exclude) {
            pv.name = ndmp_env_keywords[NDMP_ENV_KW_EXCLUDE];
         } else {
            pv.name = ndmp_env_keywords[NDMP_ENV_KW_INCLUDE];
         }

         pv.value = pattern.c_str();
         ndma_store_env_list(&job->env_tab, &pv);
      }

      /*
       * Parse all specific META tags for this option block.
       */
      for (j = 0; j < fo->meta.size(); j++) {
         ndmp_parse_meta_tag(&job->env_tab, (char *)fo->meta.get(j));
      }
   }

   /*
    * If we have a paired storage definition we put the
    * - Storage Daemon Auth Key
    * - Filesystem
    * - Dumplevel
    * into the tape device name of the  NDMP session. This way the storage
    * daemon can link the NDMP data and the normal save session together.
    */
   if (jcr->store_bsock) {
      if (nbf_options && nbf_options->uses_level) {
         Mmsg(tape_device, "%s@%s%%%d", jcr->sd_auth_key, filesystem, jcr->DumpLevel);
      } else {
         Mmsg(tape_device, "%s@%s", jcr->sd_auth_key, filesystem);
      }
      job->tape_device = bstrdup(tape_device.c_str());
   }

   return true;
}


/*
 * Translate bareos native backup level to NDMP backup level
 */
int native_to_ndmp_level(JCR *jcr, char *filesystem)
{
   int level = -1;

   if (!jcr->db->create_ndmp_level_mapping(jcr, &jcr->jr, filesystem)) {
      return -1;
   }

   switch (jcr->getJobLevel()) {
   case L_FULL:
      level = 0;
      break;
   case L_DIFFERENTIAL:
      level = 1;
      break;
   case L_INCREMENTAL:
      level = jcr->db->get_ndmp_level_mapping(jcr, &jcr->jr, filesystem);
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Illegal Job Level %c for NDMP Job\n"), jcr->getJobLevel());
      break;
   }

   /*
    * Dump level can be from 0 - 9
    */
   if (level < 0 || level > 9) {
      Jmsg(jcr, M_FATAL, 0, _("NDMP dump format doesn't support more than 8 "
                              "incrementals, please run a Differential or a Full Backup\n"));
      level = -1;
   }

   return level;
}

/*
 * This glues the NDMP File Handle DB with internal code.
 */
void register_callback_hooks(struct ndmlog *ixlog)
{
#ifdef HAVE_LMDB
   NIS *nis = (NIS *)ixlog->ctx;

   if (nis->jcr->res.client->ndmp_use_lmdb) {
      ndmp_fhdb_lmdb_register(ixlog);
   } else {
      ndmp_fhdb_mem_register(ixlog);
   }
#else
   ndmp_fhdb_mem_register(ixlog);
#endif
}

void unregister_callback_hooks(struct ndmlog *ixlog)
{
#ifdef HAVE_LMDB
   NIS *nis = (NIS *)ixlog->ctx;

   if (nis->jcr->res.client->ndmp_use_lmdb) {
      ndmp_fhdb_lmdb_unregister(ixlog);
   } else {
      ndmp_fhdb_mem_unregister(ixlog);
   }
#else
   ndmp_fhdb_mem_unregister(ixlog);
#endif
}

void process_fhdb(struct ndmlog *ixlog)
{
#ifdef HAVE_LMDB
   NIS *nis = (NIS *)ixlog->ctx;

   if (nis->jcr->res.client->ndmp_use_lmdb) {
      ndmp_fhdb_lmdb_process_db(ixlog);
   } else {
      ndmp_fhdb_mem_process_db(ixlog);
   }
#else
   ndmp_fhdb_mem_process_db(ixlog);
#endif
}


#endif /* HAVE_NDMP */
