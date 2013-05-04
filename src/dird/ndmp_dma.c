/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * ndmp_dma.c implements the NDMP Data Management Application (DMA)
 * which controls all NDMP backups and restores.
 *
 * Marco van Wieringen, May 2012
 */

#include "bareos.h"
#include "dird.h"

#if HAVE_NDMP

#include "ndmp/ndmagents.h"

#define B_PAGE_SIZE 4096
#define MAX_PAGES 2400
#define MAX_BUF_SIZE (MAX_PAGES * B_PAGE_SIZE)  /* approx 10MB */

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Imported variables */
extern DIRRES *director;

/* Forward referenced functions */

/*
 * Lightweight version of Bareos tree layout for holding the NDMP
 * filehandle index database. See lib/tree.[ch] for the full version.
 */
struct ndmp_fhdb_mem {
   struct ndmp_fhdb_mem *next;        /* next buffer */
   int rem;                           /* remaining bytes */
   char *mem;                         /* memory pointer */
   char first[1];                     /* first byte */
};

struct ndmp_fhdb_node {
   /*
    * KEEP sibling as the first member to avoid having to
    * do initialization of child
    */
   rblink sibling;
   rblist child;
   char *fname;                       /* file name */
   int32_t FileIndex;                 /* file index */
   uint16_t fname_len;                /* filename length */
   struct ndmp_fhdb_node *parent;
   struct ndmp_fhdb_node *next;       /* next hash of FileIndex */
};

struct ndmp_fhdb_root {
   /*
    * KEEP sibling as the first member to avoid having to
    * do initialization of child
    */
   rblink sibling;
   rblist child;
   char *fname;                       /* file name */
   int32_t FileIndex;                 /* file index */
   uint16_t fname_len;                /* filename length */
   struct ndmp_fhdb_node *parent;
   struct ndmp_fhdb_node *next;       /* next hash of FileIndex */

   /* The above ^^^ must be identical to a ndmp_fhdb_node structure */
   struct ndmp_fhdb_node *first;      /* first entry in the tree */
   struct ndmp_fhdb_node *last;       /* last entry in the tree */
   struct ndmp_fhdb_mem *mem;         /* tree memory */
   uint32_t total_size;               /* total bytes allocated */
   uint32_t blocks;                   /* total mallocs */
};

/*
 * Internal structure to keep track of private data for logging.
 */
struct ndmp_log_cookie {
   uint32_t LogLevel;
   JCR *jcr;
   UAContext *ua;
   struct ndmp_fhdb_root *fhdb_root;
};

static char OKbootstrap[] = "3000 OK bootstrap\n";

/*
 * Array used for storing fixed NDMP env keywords.
 * Anything special should go into a so called meta-tag in the fileset options.
 */
static char *ndmp_env_keywords[] = {
   (char *)"HIST",
   (char *)"TYPE",
   (char *)"DIRECT",
   (char *)"LEVEL",
   (char *)"UPDATE",
   (char *)"EXCLUDE",
   (char *)"INCLUDE",
   (char *)"FILESYSTEM",
   (char *)"PREFIX"
};

/*
 * Index values for above keyword.
 */
enum {
   NDMP_ENV_KW_HIST = 0,
   NDMP_ENV_KW_TYPE,
   NDMP_ENV_KW_DIRECT,
   NDMP_ENV_KW_LEVEL,
   NDMP_ENV_KW_UPDATE,
   NDMP_ENV_KW_EXCLUDE,
   NDMP_ENV_KW_INCLUDE,
   NDMP_ENV_KW_FILESYSTEM,
   NDMP_ENV_KW_PREFIX
};

/*
 * Array used for storing fixed NDMP env values.
 * Anything special should go into a so called meta-tag in the fileset options.
 */
static char *ndmp_env_values[] = {
   (char *)"n",
   (char *)"y"
};

/*
 * Index values for above values.
 */
enum {
   NDMP_ENV_VALUE_NO = 0,
   NDMP_ENV_VALUE_YES
};

/*
 * Lightweight version of Bareos tree functions for holding the NDMP
 * filehandle index database. See lib/tree.[ch] for the full version.
 */
static void malloc_buf(struct ndmp_fhdb_root *root, int size)
{
   struct ndmp_fhdb_mem *mem;

   mem = (struct ndmp_fhdb_mem *)malloc(size);
   root->total_size += size;
   root->blocks++;
   mem->next = root->mem;
   root->mem = mem;
   mem->mem = mem->first;
   mem->rem = (char *)mem + size - mem->mem;
}

/*
 * Note, we allocate a big buffer in the tree root
 *  from which we allocate nodes. This runs more
 *  than 100 times as fast as directly using malloc()
 *  for each of the nodes.
 */
static inline struct ndmp_fhdb_root *ndmp_fhdb_new_tree(int count)
{
   struct ndmp_fhdb_root *root;
   uint32_t size;

   if (count < 1000) {                /* minimum tree size */
      count = 1000;
   }
   root = (struct ndmp_fhdb_root *)malloc(sizeof(struct ndmp_fhdb_root));
   memset(root, 0, sizeof(struct ndmp_fhdb_root));

   /*
    * Assume filename + node  = 40 characters average length
    */
   size = count * (BALIGN(sizeof(struct ndmp_fhdb_root)) + 40);
   if (count > 1000000 || size > (MAX_BUF_SIZE / 2)) {
      size = MAX_BUF_SIZE;
   }
   Dmsg2(400, "count=%d size=%d\n", count, size);
   malloc_buf(root, size);

   return root;
}

/*
 * Allocate bytes for filename in tree structure.
 *  Keep the pointers properly aligned by allocating
 *  sizes that are aligned.
 */
static inline char *ndmp_fhdb_tree_alloc(struct ndmp_fhdb_root *root, int size)
{
   char *buf;
   int asize = BALIGN(size);

   if (root->mem->rem < asize) {
      uint32_t mb_size;

      if (root->total_size >= (MAX_BUF_SIZE / 2)) {
         mb_size = MAX_BUF_SIZE;
      } else {
         mb_size = MAX_BUF_SIZE / 2;
      }
      malloc_buf(root, mb_size);
   }
   root->mem->rem -= asize;
   buf = root->mem->mem;
   root->mem->mem += asize;

   return buf;
}

/*
 * This routine can be called to release the
 *  previously allocated tree node.
 */
static inline void ndmp_fhdb_free_tree_node(struct ndmp_fhdb_root *root)
{
   int asize = BALIGN(sizeof(struct ndmp_fhdb_node));

   root->mem->rem += asize;
   root->mem->mem -= asize;
}

/*
 * Create a new tree node.
 */
static struct ndmp_fhdb_node *ndmp_fhdb_new_tree_node(struct ndmp_fhdb_root *root)
{
   struct ndmp_fhdb_node *node;
   int size = sizeof(struct ndmp_fhdb_node);

   node = (struct ndmp_fhdb_node *)ndmp_fhdb_tree_alloc(root, size);
   memset(node, 0, size);

   return node;
}

/*
 * This routine frees the whole tree
 */
static inline void ndmp_fhdb_free_tree(struct ndmp_fhdb_root *root)
{
   struct ndmp_fhdb_mem *mem, *rel;
   uint32_t freed_blocks = 0;

   for (mem = root->mem; mem; ) {
      rel = mem;
      mem = mem->next;
      free(rel);
      freed_blocks++;
   }

   Dmsg3(100, "Total size=%u blocks=%u freed_blocks=%u\n", root->total_size, root->blocks, freed_blocks);

   free(root);
   garbage_collect_memory();

   return;
}

/*
 * Fill a ndmagent structure with the correct info. Instead of calling ndmagent_from_str
 * we fill the structure ourself from info provides in a resource.
 */
static inline bool fill_ndmp_agent_config(JCR *jcr,
                                          struct ndmagent *agent,
                                          uint32_t protocol,
                                          uint32_t authtype,
                                          char *address,
                                          uint32_t port,
                                          char *username,
                                          char *password)
{
   agent->conn_type = NDMCONN_TYPE_REMOTE;
   switch (protocol) {
   case APT_NDMPV2:
      agent->protocol_version = 2;
      break;
   case APT_NDMPV3:
      agent->protocol_version = 3;
      break;
   case APT_NDMPV4:
      agent->protocol_version = 4;
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Illegal protocol %d for NDMP Job\n"), protocol);
      return false;
   }

   switch (authtype) {
   case AT_NONE:
      agent->auth_type = 'n';
      break;
   case AT_CLEAR:
      agent->auth_type = 't';
      break;
   case AT_MD5:
      agent->auth_type = 'm';
      break;
   case AT_VOID:
      agent->auth_type = 'v';
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Illegal authtype %d for NDMP Job\n"), authtype);
      return false;
   }

   agent->port = port;
   bstrncpy(agent->host, address, sizeof(agent->host));
   bstrncpy(agent->account, username, sizeof(agent->account));
   bstrncpy(agent->password, password, sizeof(agent->password));

   return true;
}

static inline int native_to_ndmp_level(JCR *jcr, char *filesystem)
{
   int level = -1;

   if (!db_create_ndmp_level_mapping(jcr, jcr->db, &jcr->jr, filesystem)) {
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
      level = db_get_ndmp_level_mapping(jcr, jcr->db, &jcr->jr, filesystem);
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
 * Parse a meta-tag and convert it into a ndmp_pval
 */
static inline void parse_meta_tag(struct ndm_env_table *env_tab,
                                  char *meta_tag)
{
   char *p;
   ndmp9_pval pv;

   /*
    * See if the meta-tag is parseable.
    */
   if ((p = strchr(meta_tag, '=')) == NULL) {
      return;
   }

   /*
    * Split the tag on the '='
    */
   *p = '\0';
   pv.name = meta_tag;
   pv.value = p + 1;
   ndma_store_env_list(env_tab, &pv);

   /*
    * Restore the '='
    */
   *p = '=';
}

/*
 * Fill the NDMP backup environment table with the data for the data agent to act on.
 */
static inline bool fill_backup_environment(JCR *jcr,
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

   /*
    * We want to receive file history info from the NDMP backup.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_HIST];
   pv.value = ndmp_env_values[NDMP_ENV_VALUE_YES];
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * Tell the data agent what type of backup to make.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_TYPE];
   pv.value = job->bu_type;
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * See if we are doing a dump backup type and set any specific keywords
    * for that backup type.
    */
   if (bstrcasecmp(job->bu_type, "dump")) {
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
         parse_meta_tag(&job->env_tab, (char *)fo->meta.get(j));
      }
   }

   /*
    * If we have a paired storage definition we put the storage daemon
    * auth key and the filesystem into the tape device name of the
    * NDMP session. This way the storage daemon can link the NDMP
    * data and the normal save session together.
    */
   if (jcr->store_bsock) {
      Mmsg(tape_device, "%s@%s", jcr->sd_auth_key, filesystem);
      job->tape_device = bstrdup(tape_device.c_str());
   }

   return true;
}

/*
 * Add a filename to the files we want to restore. Whe need
 * to create both the original path and the destination patch
 * of the file to restore in a ndmp9_name structure.
 */
static inline void add_to_namelist(struct ndm_job_param *job,
                                   char *filename,
                                   char *restore_prefix)
{
   ndmp9_name nl;
   POOL_MEM destination_path;

   /*
    * See if the filename is an absolute pathname.
    */
   if (*filename == '/') {
      Mmsg(destination_path, "%s%s", restore_prefix, filename);
   } else {
      Mmsg(destination_path, "%s/%s", restore_prefix, filename);
   }

   memset(&nl, 0, sizeof(ndmp9_name));
   nl.original_path = filename;
   nl.destination_path = destination_path.c_str();
   ndma_store_nlist(&job->nlist_tab, &nl);
}

/*
 * Walk the tree of selected files for restore and lookup the
 * correct fileid. Return the actual full pathname of the file
 * corresponding to the given fileid.
 */
static inline char *lookup_fileindex(JCR *jcr, int32_t FileIndex)
{
   TREE_NODE *node;
   POOL_MEM restore_pathname, tmp;

   for (node = first_tree_node(jcr->restore_tree_root); node; node = next_tree_node(node)) {
      /*
       * See if this is the wanted FileIndex.
       */
      if (node->FileIndex == FileIndex) {
         pm_strcpy(restore_pathname, node->fname);

         /*
          * Walk up the parent until we hit the head of the list.
          */
         for (node = node->parent; node; node = node->parent) {
            pm_strcpy(tmp, restore_pathname.c_str());
            Mmsg(restore_pathname, "%s/%s", node->fname, tmp.c_str());
         }

         return bstrdup(restore_pathname.c_str());
      }
   }

   return NULL;
}

/*
 * Fill the NDMP restore environment table with the data for the data agent to act on.
 */
static inline bool fill_restore_environment(JCR *jcr,
                                            int32_t current_fi,
                                            struct ndm_job_param *job)
{
   int i;
   char *bp;
   ndmp9_pval pv;
   FILESETRES *fileset;
   char *restore_pathname,
        *restore_prefix;
   POOL_MEM tape_device;
   POOL_MEM destination_path;

   /*
    * Tell the data agent what type of restore stream to expect.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_TYPE];
   pv.value = job->bu_type;
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * Lookup the current fileindex and map it to an actual pathname.
    */
   restore_pathname = lookup_fileindex(jcr, current_fi);
   if (!restore_pathname) {
      return false;
   }

   /*
    * Make sure the restored file pattern is in the pseudo NDMP naming space.
    */
   if (!bstrncmp(restore_pathname, "/@NDMP/", 7)) {
      return false;
   }

   /*
    * See if there is a level embedded in the pathname.
    */
   bp = strrchr(restore_pathname, '%');
   if (bp) {
      *bp = '\0';
   }

   /*
    * Lookup any meta tags that need to be added.
    */
   fileset = jcr->res.fileset;
   for (i = 0; i < fileset->num_includes; i++) {
      int j;
      char *item;
      INCEXE *ie = fileset->include_items[i];

      /*
       * Loop over each file = entry of the fileset.
       */
      for (j = 0; j < ie->name_list.size(); j++) {
         item = (char *)ie->name_list.get(j);

         /*
          * See if the restore path matches.
          */
         if (bstrcasecmp(item, restore_pathname)) {
            int k, l;
            FOPTS *fo;

            for (k = 0; k < ie->num_opts; k++) {
               fo = ie->opts_list[i];

               /*
                * Parse all specific META tags for this option block.
                */
               for (l = 0; l < fo->meta.size(); l++) {
                  parse_meta_tag(&job->env_tab, (char *)fo->meta.get(l));
               }
            }
         }
      }
   }

   /*
    * See where to restore the data.
    */
   restore_prefix = NULL;
   if (jcr->where) {
      restore_prefix = jcr->where;
   } else {
      restore_prefix = jcr->res.job->RestoreWhere;
   }

   if (!restore_prefix) {
      return false;
   }

   Mmsg(destination_path, "%s/%s", restore_prefix, restore_pathname + 7);

   /*
    * Tell the data engine where to restore.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_PREFIX];
   pv.value = destination_path.c_str();
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * The smtape NDMP backup type doesn't need a namelist to restore the
    * data as it restores all data from the stream anyhow.
    */
   if (!bstrcasecmp(job->bu_type, "smtape")) {
      /*
       * FIXME: For now we say we want to restore everything later on it would
       * be nice to only restore parts of the whole backup.
       */
      add_to_namelist(job, (char *)"/", destination_path.c_str());
   }

   /*
    * If we have a paired storage definition we put the storage daemon
    * auth key and the filesystem into the tape device name of the
    * NDMP session. This way the storage daemon can link the NDMP
    * data and the normal save session together.
    */
   if (jcr->store_bsock) {
      Mmsg(tape_device, "%s@%s", jcr->sd_auth_key, restore_pathname + 6);
      job->tape_device = bstrdup(tape_device.c_str());
   }

   free(restore_pathname);
   return true;
}

static inline bool build_ndmp_job(JCR *jcr,
                                  CLIENTRES *client,
                                  STORERES *store,
                                  int operation,
                                  struct ndm_job_param *job)
{
   memset(job, 0, sizeof(struct ndm_job_param));

   job->operation = operation;
   job->bu_type = jcr->backup_format;

   /*
    * For NDMP the backupformat is a prerequite abort the backup job when
    * it is not supplied in the config definition.
    */
   if (!job->bu_type) {
      Jmsg(jcr, M_FATAL, 0, _("No backup type specified in NDMP job\n"));
      goto bail_out;
   }

   /*
    * The data_agent is the client being backuped or restored using NDMP.
    */
   if (!fill_ndmp_agent_config(jcr, &job->data_agent, client->Protocol,
                               client->AuthType, client->address,
                               client->FDport, client->username,
                               client->password)) {
      goto bail_out;
   }

   /*
    * The tape_agent is the storage daemon via the NDMP protocol.
    */
   if (!fill_ndmp_agent_config(jcr, &job->tape_agent, store->Protocol,
                               store->AuthType, store->address,
                               store->SDport, store->username,
                               store->password)) {
      goto bail_out;
   }

   job->record_size = jcr->res.client->ndmp_blocksize;

   return true;

bail_out:
   return false;
}

static inline bool validate_ndmp_job(JCR *jcr,
                                     struct ndm_job_param *job)
{
   int n_err, i;
   char audit_buffer[256];

   /*
    * Audit the job so we only submit a valid NDMP job.
    */
   n_err = 0;
   i = 0;
   do {
      n_err = ndma_job_audit(job, audit_buffer, i);
      if (n_err) {
         Jmsg(jcr, M_ERROR, 0, _("NDMP Job validation error = %s\n"), audit_buffer);
      }
      i++;
   } while (i < n_err);

   if (n_err) {
      return false;
   }

   return true;
}

extern "C" int bndmp_add_file(struct ndmlog *ixlog, int tagc, char *raw_name,
                              ndmp9_file_stat *fstat)
{
   struct ndmp_log_cookie *log_cookie;
   char namebuf[NDMOS_CONST_PATH_MAX];

   ndmcstr_from_str(raw_name, namebuf, sizeof(namebuf));

   log_cookie = (struct ndmp_log_cookie *)ixlog->cookie;
   log_cookie->jcr->lock();
   log_cookie->jcr->JobFiles++;
   log_cookie->jcr->unlock();

   return 0;
}

extern "C" int bndmp_add_dir(struct ndmlog *ixlog, int tagc, char *raw_name,
                             ndmp9_u_quad dir_node, ndmp9_u_quad node)
{
   struct ndmp_log_cookie *log_cookie;
   char namebuf[NDMOS_CONST_PATH_MAX];

   ndmcstr_from_str(raw_name, namebuf, sizeof(namebuf));

   /*
    * Ignore . and .. directory entries.
    */
   if (bstrcmp(namebuf, ".") || bstrcmp(namebuf, "..")) {
      return 0;
   }

   log_cookie = (struct ndmp_log_cookie *)ixlog->cookie;
   log_cookie->jcr->lock();
   log_cookie->jcr->JobFiles++;
   log_cookie->jcr->unlock();

   return 0;
}

extern "C" int bndmp_add_node(struct ndmlog *ixlog, int tagc,
                              ndmp9_u_quad node, ndmp9_file_stat *fstat)
{
   struct ndmp_log_cookie *log_cookie;

   log_cookie = (struct ndmp_log_cookie *)ixlog->cookie;

   return 0;
}

extern "C" int bndmp_add_dirnode_root(struct ndmlog *ixlog, int tagc,
                                      ndmp9_u_quad root_node)
{
   struct ndmp_log_cookie *log_cookie;

   log_cookie = (struct ndmp_log_cookie *)ixlog->cookie;

   return 0;
}

/*
 * This glues the NDMP File Handle DB with internal code.
 */
static inline void register_callback_hooks(void)
{
   struct ndm_fhdb_callbacks fhdb_callbacks;

   /*
    * Register the FileHandleDB callbacks.
    */
   fhdb_callbacks.add_file = bndmp_add_file;
   fhdb_callbacks.add_dir = bndmp_add_dir;
   fhdb_callbacks.add_node = bndmp_add_node;
   fhdb_callbacks.add_dirnode_root = bndmp_add_dirnode_root;
   ndmfhdb_register_callbacks(&fhdb_callbacks);
}

static inline void unregister_callback_hooks(void)
{
   ndmfhdb_unregister_callbacks();
}

/*
 * Extract any post backup statistics.
 */
static inline bool extract_post_backup_stats(JCR *jcr,
                                             char *filesystem,
                                             struct ndm_session *sess)
{
   bool retval = true;
   struct ndmmedia *me;

   /*
    * See if an error was raised during the backup session.
    */
   if (sess->error_raised) {
      return false;
   }

   /*
    * See if there is any media error.
    */
   for (me = sess->control_acb->job.result_media_tab.head; me; me = me->next) {
      if (me->media_open_error ||
          me->media_io_error ||
          me->label_io_error ||
          me->label_mismatch ||
          me->fmark_error) {
         retval = false;
      }
   }

   /*
    * Update the Job statistics from the NDMP statistics.
    */
   jcr->JobBytes += sess->control_acb->job.bytes_written;

   /*
    * If this was a NDMP backup with backup type dump save the last used dump level.
    */
   if (bstrcasecmp(sess->control_acb->job.bu_type, "dump")) {
      db_update_ndmp_level_mapping(jcr, jcr->db, &jcr->jr,
                                   filesystem, sess->control_acb->job.bu_level);
   }

   return retval;
}

/*
 * Extract any post backup statistics.
 */
static inline bool extract_post_restore_stats(JCR *jcr,
                                              struct ndm_session *sess)
{
   bool retval = true;
   struct ndmmedia *me;

   /*
    * See if an error was raised during the backup session.
    */
   if (sess->error_raised) {
      return false;
   }

   /*
    * See if there is any media error.
    */
   for (me = sess->control_acb->job.result_media_tab.head; me; me = me->next) {
      if (me->media_open_error ||
          me->media_io_error ||
          me->label_io_error ||
          me->label_mismatch ||
          me->fmark_error) {
         retval = false;
      }
   }

   /*
    * Update the Job statistics from the NDMP statistics.
    */
   jcr->JobBytes += sess->control_acb->job.bytes_read;
   jcr->JobFiles++;

   return retval;
}

/*
 * Helper function to stuff an JCR in the opaque log cookie.
 */
static inline void set_jcr_in_cookie(JCR *jcr, void *cookie)
{
   struct ndmp_log_cookie *log_cookie;

   log_cookie = (struct ndmp_log_cookie *)cookie;
   log_cookie->jcr = jcr;
}

/*
 * Helper function to stuff an UAContext in the opaque log cookie.
 */
static inline void set_ua_in_cookie(UAContext *ua, void *cookie)
{
   struct ndmp_log_cookie *log_cookie;

   log_cookie = (struct ndmp_log_cookie *)cookie;
   log_cookie->ua = ua;
}

/*
 * Calculate the wanted NDMP loglevel from the current debug level and
 * any configure minimum level.
 */
static inline int native_to_ndmp_loglevel(CLIENTRES *client, int debuglevel, void *cookie)
{
   unsigned int level;
   struct ndmp_log_cookie *log_cookie;

   log_cookie = (struct ndmp_log_cookie *)cookie;
   memset(log_cookie, 0, sizeof(struct ndmp_log_cookie));

   /*
    * Take the highest loglevel from either the Director config or the client config.
    */
   if (client && (client->ndmp_loglevel > director->ndmp_loglevel)) {
      log_cookie->LogLevel = client->ndmp_loglevel;
   } else {
      log_cookie->LogLevel = director->ndmp_loglevel;
   }

   /*
    * NDMP loglevels run from 0 - 9 so we take a look at the
    * current debug level and divide it by 100 to get a proper
    * value. If the debuglevel is below the wanted initial level
    * we set the loglevel to the wanted initial level. As the
    * debug logging takes care of logging messages that are
    * unwanted we can set the loglevel higher and still don't
    * get debug messages.
    */
   level = debuglevel / 100;
   if (level < log_cookie->LogLevel) {
      level = log_cookie->LogLevel;
   }

   /*
    * Make sure the level is in the wanted range.
    */
   if (level < 0) {
      level = 0;
   } else if (level > 9) {
      level = 9;
   }

   return level;
}

/*
 * Interface function which glues the logging infra of the NDMP lib with the daemon.
 */
extern "C" void ndmp_loghandler(struct ndmlog *log, char *tag, int level, char *msg)
{
   unsigned int internal_level = level * 100;
   struct ndmp_log_cookie *log_cookie;

   /*
    * Make sure if the logging system was setup properly.
    */
   log_cookie = (struct ndmp_log_cookie *)log->cookie;
   if (!log_cookie) {
      return;
   }

   /*
    * If the log level of this message is under our logging treshold we
    * log it as part of the Job.
    */
   if ((internal_level / 100) <= log_cookie->LogLevel) {
      if (log_cookie->jcr) {
         /*
          * Look at the tag field to see what is logged.
          */
         if (bstrncmp(tag + 1, "LM", 2)) {
            /*
             * *LM* messages. E.g. log message NDMP protocol msgs.
             * First character of the tag is the agent sending the
             * message e.g. 'D' == Data Agent
             *              'T' == Tape Agent
             *              'R' == Robot Agent
             *              'C' == Control Agent (DMA)
             *
             * Last character is the type of message e.g.
             * 'n' - normal message
             * 'd' - debug message
             * 'e' - error message
             * 'w' - warning message
             * '?' - unknown message level
             */
            switch (*(tag + 3)) {
            case 'n':
               Jmsg(log_cookie->jcr, M_INFO, 0, "%s\n", msg);
               break;
            case 'e':
               Jmsg(log_cookie->jcr, M_ERROR, 0, "%s\n", msg);
               break;
            case 'w':
               Jmsg(log_cookie->jcr, M_WARNING, 0, "%s\n", msg);
               break;
            case '?':
               Jmsg(log_cookie->jcr, M_INFO, 0, "%s\n", msg);
               break;
            default:
               break;
            }
         } else {
            Jmsg(log_cookie->jcr, M_INFO, 0, "%s\n", msg);
         }
      }
   }

   /*
    * Print any debug message we convert the NDMP level back to an internal
    * level and let the normal debug logging handle if it needs to be printed
    * or not.
    */
   Dmsg3(internal_level, "NDMP: [%s] [%d] %s\n", tag, (internal_level / 100), msg);
}

/*
 * Setup a NDMP backup session.
 */
bool do_ndmp_backup_init(JCR *jcr)
{
   free_rstorage(jcr);                   /* we don't read so release */

   if (!allow_duplicate_job(jcr)) {
      return false;
   }

   jcr->jr.PoolId = get_or_create_pool_record(jcr, jcr->res.pool->name());
   if (jcr->jr.PoolId == 0) {
      return false;
   }

   jcr->start_time = time(NULL);
   jcr->jr.StartTime = jcr->start_time;
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
   }

   /*
    * If pool storage specified, use it instead of job storage
    */
   copy_wstorage(jcr, jcr->res.pool->storage, _("Pool resource"));

   if (!jcr->wstorage) {
      Jmsg(jcr, M_FATAL, 0, _("No Storage specification found in Job or Pool.\n"));
      return false;
   }

   /*
    * For now we only allow NDMP backups to bareos SD's
    * so we need a paired storage definition.
    */
   if (!has_paired_storage(jcr)) {
      Jmsg(jcr, M_FATAL, 0,
           _("Write storage doesn't point to storage definition with paired storage option.\n"));
      return false;
   }

   return true;
}

/*
 * Run a NDMP backup session.
 */
bool do_ndmp_backup(JCR *jcr)
{
   unsigned int cnt;
   int i, status;
   char ed1[100];
   FILESETRES *fileset;
   struct ndm_job_param ndmp_job;
   struct ndm_session ndmp_sess;
   bool session_initialized = false;
   bool retval = false;

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start NDMP Backup JobId %s, Job=%s\n"),
        edit_uint64(jcr->JobId, ed1), jcr->Job);

   jcr->setJobStatus(JS_Running);
   Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->jr.JobId, jcr->jr.JobLevel);
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      return false;
   }

   if (quota_check_hardquotas(jcr)) {
      Jmsg(jcr, M_FATAL, 0, "Quota Exceeded. Job terminated.");
      return false;
   }

   if (quota_check_softquotas(jcr)) {
      Dmsg0(10, "Quota exceeded\n");
      Jmsg(jcr, M_FATAL, 0, "Soft Quota Exceeded / Grace Time expired. Job terminated.");
      return false;
   }

   /*
    * If we have a paired storage definition create a native connection
    * to a Storage daemon and make it ready to receive a backup.
    * The setup is more or less the same as for a normal non NDMP backup
    * only the data doesn't come in from a FileDaemon but from a NDMP
    * data mover which moves the data from the NDMP DATA AGENT to the NDMP
    * TAPE AGENT.
    */
   if (jcr->res.wstore->paired_storage) {
      set_paired_storage(jcr);

      jcr->setJobStatus(JS_WaitSD);
      if (!connect_to_storage_daemon(jcr, 10, SDConnectTimeout, 1)) {
         return false;
      }

      /*
       * Now start a job with the Storage daemon
       */
      if (!start_storage_daemon_job(jcr, NULL, jcr->wstorage)) {
         return false;
      }

      /*
       * Start the job prior to starting the message thread below
       * to avoid two threads from using the BSOCK structure at
       * the same time.
       */
      if (!jcr->store_bsock->fsend("run")) {
         return false;
      }

      /*
       * Now start a Storage daemon message thread.  Note,
       *   this thread is used to provide the catalog services
       *   for the backup job.
       */
      if (!start_storage_daemon_message_thread(jcr)) {
         return false;
      }
      Dmsg0(150, "Storage daemon connection OK\n");
   }

   status = 0;

   /*
    * Initialize the ndmp backup job. We build the generic job only once
    * and reuse the job definition for each seperate sub-backup we perform as
    * part of the whole job. We only free the env_table between every sub-backup.
    */
   if (!build_ndmp_job(jcr, jcr->res.client, jcr->res.pstore, NDM_JOB_OP_BACKUP, &ndmp_job)) {
      goto bail_out;
   }

   register_callback_hooks();

   /*
    * Loop over each include set of the fileset and fire off a NDMP backup of the included fileset.
    */
   cnt = 0;
   fileset = jcr->res.fileset;
   for (i = 0; i < fileset->num_includes; i++) {
      int j;
      char *item;
      INCEXE *ie = fileset->include_items[i];

      /*
       * Loop over each file = entry of the fileset.
       */
      for (j = 0; j < ie->name_list.size(); j++) {
         item = (char *)ie->name_list.get(j);

         /*
          * See if this is the first Backup run or not. For NDMP we can have multiple Backup
          * runs as part of the same Job. When we are saving data to a Native Storage Daemon
          * we let it know to expect a new backup session. It will generate a new authentication
          * id so we wait for the nextrun_ready conditional variable to be raised by the msg_thread.
          */
         if (jcr->store_bsock && j > 0) {
            jcr->store_bsock->fsend("nextrun");
            P(mutex);
            pthread_cond_wait(&jcr->nextrun_ready, &mutex);
            V(mutex);
         }

         /*
          * Perform the actual NDMP job.
          * Initialize a new NDMP session
          */
         memset(&ndmp_sess, 0, sizeof(ndmp_sess));
         ndmp_sess.conn_snooping = (director->ndmp_snooping) ? 1 : 0;
         ndmp_sess.control_agent_enabled = 1;

         ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
         memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
         ndmp_sess.param->log.deliver = ndmp_loghandler;
         ndmp_sess.param->log.cookie = malloc(sizeof(struct ndmp_log_cookie));
         ndmp_sess.param->log_level = native_to_ndmp_loglevel(jcr->res.client, debug_level, ndmp_sess.param->log.cookie);
         set_jcr_in_cookie(jcr, ndmp_sess.param->log.cookie);
         ndmp_sess.param->log_tag = bstrdup("DIR-NDMP");

         /*
          * Initialize the session structure.
          */
         if (ndma_session_initialize(&ndmp_sess)) {
            goto cleanup;
         }
         session_initialized = true;

         /*
          * Copy the actual job to perform.
          */
         memcpy(&ndmp_sess.control_acb->job, &ndmp_job, sizeof(struct ndm_job_param));
         if (!fill_backup_environment(jcr,
                                      ie,
                                      item,
                                      &ndmp_sess.control_acb->job)) {
            goto cleanup;
         }

         ndma_job_auto_adjust(&ndmp_sess.control_acb->job);
         if (!validate_ndmp_job(jcr, &ndmp_sess.control_acb->job)) {
            goto cleanup;
         }

         /*
          * Commission the session for a run.
          */
         if (ndma_session_commission(&ndmp_sess)) {
            goto cleanup;
         }

         /*
          * Setup the DMA.
          */
         if (ndmca_connect_control_agent(&ndmp_sess)) {
            goto cleanup;
         }

         ndmp_sess.conn_open = 1;
         ndmp_sess.conn_authorized = 1;

         /*
          * We can use the same cookie used in the logging with the JCR in
          * the file index generation. We don't setup a index_log.deliver
          * function as we catch the index information via callbacks.
          */
         ndmp_sess.control_acb->job.index_log.cookie = ndmp_sess.param->log.cookie;

         /*
          * Let the DMA perform its magic.
          */
         if (ndmca_control_agent(&ndmp_sess) != 0) {
            goto cleanup;
         }

         /*
          * See if there were any errors during the backup.
          */
         if (!extract_post_backup_stats(jcr, item, &ndmp_sess)) {
            goto cleanup;
         }

         /*
          * Reset the NDMP session states.
          */
         ndma_session_decommission(&ndmp_sess);

         /*
          * Cleanup the job after it has run.
          */
         ndma_destroy_env_list(&ndmp_sess.control_acb->job.env_tab);
         ndma_destroy_env_list(&ndmp_sess.control_acb->job.result_env_tab);
         ndma_destroy_nlist(&ndmp_sess.control_acb->job.nlist_tab);

         /*
          * Release any tape device name allocated.
          */
         if (ndmp_sess.control_acb->job.tape_device) {
            free(ndmp_sess.control_acb->job.tape_device);
            ndmp_sess.control_acb->job.tape_device = NULL;
         }

         /*
          * Destroy the session.
          */
         ndma_session_destroy(&ndmp_sess);

         /*
          * Free the param block.
          */
         free(ndmp_sess.param->log_tag);
         free(ndmp_sess.param->log.cookie);
         free(ndmp_sess.param);
         ndmp_sess.param = NULL;

         /*
          * Reset the initialized state so we don't try to cleanup again.
          */
         session_initialized = false;

         cnt++;
      }
   }

   status = JS_Terminated;
   retval = true;

   /*
    * Tell the storage daemon we are done.
    */
   if (jcr->store_bsock) {
      jcr->store_bsock->fsend("finish");
      wait_for_storage_daemon_termination(jcr);
      db_write_batch_file_records(jcr);    /* used by bulk batch file insert */
   }

   /*
    * If we do incremental backups it can happen that the backup is empty if
    * nothing changed but we always write a filestream. So we use the counter
    * which counts the number of actual NDMP backup sessions we run to completion.
    */
   if (jcr->JobFiles < cnt) {
      jcr->JobFiles = cnt;
   }

   /*
    * Jump to the generic cleanup done for every Job.
    */
   goto ok_out;

cleanup:
   /*
    * Only need to cleanup when things are initialized.
    */
   if (session_initialized) {
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.env_tab);
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.result_env_tab);
      ndma_destroy_nlist(&ndmp_sess.control_acb->job.nlist_tab);

      if (ndmp_sess.control_acb->job.tape_device) {
         free(ndmp_sess.control_acb->job.tape_device);
      }

      /*
       * Destroy the session.
       */
      ndma_session_destroy(&ndmp_sess);
   }

   if (ndmp_sess.param) {
      free(ndmp_sess.param->log_tag);
      free(ndmp_sess.param->log.cookie);
      free(ndmp_sess.param);
   }

bail_out:
   /*
    * Error handling of failed Job.
    */
   status = JS_ErrorTerminated;
   jcr->setJobStatus(JS_ErrorTerminated);

   if (jcr->store_bsock) {
      cancel_storage_daemon_job(jcr);
      wait_for_storage_daemon_termination(jcr);
   }

ok_out:
   unregister_callback_hooks();
   free_paired_storage(jcr);

   if (status == JS_Terminated) {
      ndmp_backup_cleanup(jcr, status);
   }

   return retval;
}

/*
 * Cleanup a NDMP backup session.
 */
void ndmp_backup_cleanup(JCR *jcr, int TermCode)
{
   const char *term_msg;
   char term_code[100];
   int msg_type = M_INFO;
   MEDIA_DBR mr;
   CLIENT_DBR cr;

   Dmsg2(100, "Enter ndmp_backup_cleanup %d %c\n", TermCode, TermCode);
   memset(&cr, 0, sizeof(cr));

#ifdef xxxx
   /* The current implementation of the JS_Warning status is not
    * completed. SQL part looks to be ok, but the code is using
    * JS_Terminated almost everywhere instead of (JS_Terminated || JS_Warning)
    * as we do with is_canceled()
    */
   if (jcr->getJobStatus() == JS_Terminated &&
        (jcr->JobErrors || jcr->SDErrors || jcr->JobWarnings)) {
      TermCode = JS_Warnings;
   }
#endif

   update_job_end(jcr, TermCode);

   if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"),
         db_strerror(jcr->db));
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   bstrncpy(cr.Name, jcr->res.client->name(), sizeof(cr.Name));
   if (!db_get_client_record(jcr, jcr->db, &cr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Client record for Job report: ERR=%s"),
         db_strerror(jcr->db));
   }

   bstrncpy(mr.VolumeName, jcr->VolumeName, sizeof(mr.VolumeName));
   if (!db_get_media_record(jcr, jcr->db, &mr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Media record for Volume \"%s\": ERR=%s"),
         mr.VolumeName, db_strerror(jcr->db));
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   update_bootstrap_file(jcr);

   switch (jcr->JobStatus) {
      case JS_Terminated:
         if (jcr->JobErrors || jcr->SDErrors) {
            term_msg = _("Backup OK -- with warnings");
         } else {
            term_msg = _("Backup OK");
         }
         break;
      case JS_Warnings:
         term_msg = _("Backup OK -- with warnings");
         break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         term_msg = _("*** Backup Error ***");
         msg_type = M_ERROR;          /* Generate error message */
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      case JS_Canceled:
         term_msg = _("Backup Canceled");
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      default:
         term_msg = term_code;
         sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
         break;
   }

   generate_backup_summary(jcr, &mr, &cr, msg_type, term_msg);

   Dmsg0(100, "Leave ndmp_backup_cleanup\n");
}

/*
 * Setup a NDMP restore session.
 */
bool do_ndmp_restore_init(JCR *jcr)
{
   free_wstorage(jcr);                /* we don't write */

   if (!jcr->restore_tree_root) {
      Jmsg(jcr, M_FATAL, 0, _("Cannot NDMP restore without a file selection.\n"));
      return false;
   }

   return true;
}

static inline int ndmp_wait_for_job_termination(JCR *jcr)
{
   jcr->setJobStatus(JS_Running);

   /*
    * Force cancel in SD if failing, but not for Incomplete jobs
    *  so that we let the SD despool.
    */
   Dmsg4(100, "cancel=%d FDJS=%d JS=%d SDJS=%d\n",
         jcr->is_canceled(), jcr->FDJobStatus,
         jcr->JobStatus, jcr->SDJobStatus);
   if (jcr->is_canceled() || (!jcr->res.job->RescheduleIncompleteJobs)) {
      Dmsg3(100, "FDJS=%d JS=%d SDJS=%d\n",
            jcr->FDJobStatus, jcr->JobStatus, jcr->SDJobStatus);
      cancel_storage_daemon_job(jcr);
   }

   /* Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/JobErrors */
   wait_for_storage_daemon_termination(jcr);

   jcr->FDJobStatus = JS_Terminated;
   if (jcr->JobStatus != JS_Terminated) {
      return jcr->JobStatus;
   }
   if (jcr->FDJobStatus != JS_Terminated) {
      return jcr->FDJobStatus;
   }
   return jcr->SDJobStatus;
}

/*
 * The bootstrap is stored in a file, so open the file, and loop
 *   through it processing each storage device in turn. If the
 *   storage is different from the prior one, we open a new connection
 *   to the new storage and do a restore for that part.
 *
 * This permits handling multiple storage daemons for a single
 *   restore.  E.g. your Full is stored on tape, and Incrementals
 *   on disk.
 */
static inline bool do_ndmp_restore_bootstrap(JCR *jcr)
{
   BSOCK *sd;
   BSR *bsr;
   BSR_FINDEX *fileindex;
   int32_t current_fi;
   bootstrap_info info;
   struct ndm_job_param ndmp_job;
   struct ndm_session ndmp_sess;
   bool session_initialized = false;
   bool retval = false;

   /*
    * We first parse the BSR ourself so we know what to restore.
    */
   jcr->bsr = parse_bsr(jcr, jcr->RestoreBootstrap);
   if (!jcr->bsr) {
      Jmsg(jcr, M_FATAL, 0, _("Error parsing bootstrap file.\n"));
      goto bail_out;
   }

   /*
    * Setup all paired read storage.
    */
   set_paired_storage(jcr);

   /*
    * Open the bootstrap file
    */
   if (!open_bootstrap_file(jcr, info)) {
      goto bail_out;
   }

   /*
    * Read the bootstrap file
    */
   bsr = jcr->bsr;
   while (!feof(info.bs)) {
      if (!select_next_rstore(jcr, info)) {
         goto bail_out;
      }

      /*
       * Initialize the ndmp restore job. We build the generic job once per storage daemon
       * and reuse the job definition for each seperate sub-restore we perform as
       * part of the whole job. We only free the env_table between every sub-restore.
       */
      if (!build_ndmp_job(jcr, jcr->res.client, jcr->res.pstore, NDM_JOB_OP_EXTRACT, &ndmp_job)) {
         goto bail_out;
      }

      /*
       * Open a message channel connection with the Storage
       * daemon. This is to let him know that our client
       * will be contacting him for a backup session.
       *
       */
      Dmsg0(10, "Open connection with storage daemon\n");
      jcr->setJobStatus(JS_WaitSD);

      /*
       * Start conversation with Storage daemon
       */
      if (!connect_to_storage_daemon(jcr, 10, SDConnectTimeout, 1)) {
         goto bail_out;
      }
      sd = jcr->store_bsock;

      /*
       * Now start a job with the Storage daemon
       */
      if (!start_storage_daemon_job(jcr, jcr->rstorage, NULL)) {
         goto bail_out;
      }

      jcr->setJobStatus(JS_Running);

      /*
       * Send the bootstrap file -- what Volumes/files to restore
       */
      if (!send_bootstrap_file(jcr, sd, info) ||
          !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
         goto bail_out;
      }

      if (!sd->fsend("run")) {
         goto bail_out;
      }

      /*
       * Now start a Storage daemon message thread
       */
      if (!start_storage_daemon_message_thread(jcr)) {
         goto bail_out;
      }
      Dmsg0(50, "Storage daemon connection OK\n");

      /*
       * Walk each fileindex of the current BSR record. Each different fileindex is
       * a seperate NDMP stream.
       */
      for (fileindex = bsr->FileIndex; fileindex; fileindex = fileindex->next) {
         for (current_fi = fileindex->findex; current_fi <= fileindex->findex2; current_fi++) {
            /*
             * Perform the actual NDMP job.
             * Initialize a new NDMP session
             */
            memset(&ndmp_sess, 0, sizeof(ndmp_sess));
            ndmp_sess.conn_snooping = (director->ndmp_snooping) ? 1 : 0;
            ndmp_sess.control_agent_enabled = 1;

            ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
            memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
            ndmp_sess.param->log.deliver = ndmp_loghandler;
            ndmp_sess.param->log.cookie = malloc(sizeof(struct ndmp_log_cookie));
            ndmp_sess.param->log_level = native_to_ndmp_loglevel(jcr->res.client, debug_level, ndmp_sess.param->log.cookie);
            set_jcr_in_cookie(jcr, ndmp_sess.param->log.cookie);
            ndmp_sess.param->log_tag = bstrdup("DIR-NDMP");

            /*
             * Initialize the session structure.
             */
            if (ndma_session_initialize(&ndmp_sess)) {
               goto cleanup;
            }
            session_initialized = true;

            /*
             * Copy the actual job to perform.
             */
            memcpy(&ndmp_sess.control_acb->job, &ndmp_job, sizeof(struct ndm_job_param));
            if (!fill_restore_environment(jcr,
                                          current_fi,
                                          &ndmp_sess.control_acb->job)) {
               goto cleanup;
            }

            ndma_job_auto_adjust(&ndmp_sess.control_acb->job);
            if (!validate_ndmp_job(jcr, &ndmp_sess.control_acb->job)) {
               goto cleanup;
            }

            /*
             * Commission the session for a run.
             */
            if (ndma_session_commission(&ndmp_sess)) {
               goto cleanup;
            }

            /*
             * Setup the DMA.
             */
            if (ndmca_connect_control_agent(&ndmp_sess)) {
               goto cleanup;
            }

            ndmp_sess.conn_open = 1;
            ndmp_sess.conn_authorized = 1;

            /*
             * Let the DMA perform its magic.
             */
            if (ndmca_control_agent(&ndmp_sess) != 0) {
               goto cleanup;
            }

            /*
             * See if there were any errors during the restore.
             */
            if (!extract_post_restore_stats(jcr, &ndmp_sess)) {
               goto cleanup;
            }

            /*
             * Reset the NDMP session states.
             */
            ndma_session_decommission(&ndmp_sess);

            /*
             * Cleanup the job after it has run.
             */
            ndma_destroy_env_list(&ndmp_sess.control_acb->job.env_tab);
            ndma_destroy_env_list(&ndmp_sess.control_acb->job.result_env_tab);
            ndma_destroy_nlist(&ndmp_sess.control_acb->job.nlist_tab);

            /*
             * Release any tape device name allocated.
             */
            if (ndmp_sess.control_acb->job.tape_device) {
               free(ndmp_sess.control_acb->job.tape_device);
               ndmp_sess.control_acb->job.tape_device = NULL;
            }

            /*
             * Destroy the session.
             */
            ndma_session_destroy(&ndmp_sess);

            /*
             * Free the param block.
             */
            free(ndmp_sess.param->log_tag);
            free(ndmp_sess.param->log.cookie);
            free(ndmp_sess.param);
            ndmp_sess.param = NULL;

            /*
             * Reset the initialized state so we don't try to cleanup again.
             */
            session_initialized = false;
         }
      }

      /*
       * Tell the storage daemon we are done.
       */
      jcr->store_bsock->fsend("finish");
      wait_for_storage_daemon_termination(jcr);
   }

   /*
    * Jump to the generic cleanup done for every Job.
    */
   retval = true;
   goto bail_out;

cleanup:
   /*
    * Only need to cleanup when things are initialized.
    */
   if (session_initialized) {
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.env_tab);
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.result_env_tab);
      ndma_destroy_nlist(&ndmp_sess.control_acb->job.nlist_tab);

      if (ndmp_sess.control_acb->job.tape_device) {
         free(ndmp_sess.control_acb->job.tape_device);
      }

      /*
       * Destroy the session.
       */
      ndma_session_destroy(&ndmp_sess);
   }

   if (ndmp_sess.param) {
      free(ndmp_sess.param->log_tag);
      free(ndmp_sess.param->log.cookie);
      free(ndmp_sess.param);
   }

bail_out:
   free_paired_storage(jcr);
   close_bootstrap_file(info);
   free_tree(jcr->restore_tree_root);
   jcr->restore_tree_root = NULL;
   return retval;
}

/*
 * Run a NDMP restore session.
 */
bool do_ndmp_restore(JCR *jcr)
{
   JOB_DBR rjr;                       /* restore job record */
   int status;

   memset(&rjr, 0, sizeof(rjr));
   jcr->jr.JobLevel = L_FULL;         /* Full restore */
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }
   Dmsg0(20, "Updated job start record\n");

   Dmsg1(20, "RestoreJobId=%d\n", jcr->res.job->RestoreJobId);

   if (!jcr->RestoreBootstrap) {
      Jmsg(jcr, M_FATAL, 0, _("Cannot restore without a bootstrap file.\n"
           "You probably ran a restore job directly. All restore jobs must\n"
           "be run using the restore command.\n"));
      goto bail_out;
   }

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start Restore Job %s\n"), jcr->Job);

   /*
    * Read the bootstrap file and do the restore
    */
   if (!do_ndmp_restore_bootstrap(jcr)) {
      goto bail_out;
   }

   /*
    * Wait for Job Termination
    */
   status = ndmp_wait_for_job_termination(jcr);
   ndmp_restore_cleanup(jcr, status);
   return true;

bail_out:
   return false;
}

/*
 * Cleanup a NDMP restore session.
 */
void ndmp_restore_cleanup(JCR *jcr, int TermCode)
{
   char term_code[100];
   const char *term_msg;
   int msg_type = M_INFO;

   Dmsg0(20, "In ndmp_restore_cleanup\n");
   update_job_end(jcr, TermCode);

   if (jcr->unlink_bsr && jcr->RestoreBootstrap) {
      unlink(jcr->RestoreBootstrap);
      jcr->unlink_bsr = false;
   }

   if (job_canceled(jcr)) {
      cancel_storage_daemon_job(jcr);
   }

   switch (TermCode) {
   case JS_Terminated:
      if (jcr->ExpectedFiles > jcr->jr.JobFiles) {
         term_msg = _("Restore OK -- warning file count mismatch");
      } else {
         term_msg = _("Restore OK");
      }
      break;
   case JS_Warnings:
         term_msg = _("Restore OK -- with warnings");
         break;
   case JS_FatalError:
   case JS_ErrorTerminated:
      term_msg = _("*** Restore Error ***");
      msg_type = M_ERROR;          /* Generate error message */
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   case JS_Canceled:
      term_msg = _("Restore Canceled");
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   default:
      term_msg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), TermCode);
      break;
   }

   generate_restore_summary(jcr, msg_type, term_msg);

   Dmsg0(20, "Leaving ndmp_restore_cleanup\n");
}

/*
 * Interface function which glues the logging infra of the NDMP lib with the user context.
 */
extern "C" void ndmp_client_status_handler(struct ndmlog *log, char *tag, int lev, char *msg)
{
   struct ndmp_log_cookie *log_cookie;

   /*
    * Make sure if the logging system was setup properly.
    */
   log_cookie = (struct ndmp_log_cookie *)log->cookie;
   if (!log_cookie) {
      return;
   }

   log_cookie->ua->send_msg("%s\n", msg);
}

/*
 * Generic function to query the NDMP server using the NDM_JOB_OP_QUERY_AGENTS
 * operation. Callback is the above ndmp_client_status_handler which prints
 * the data to the user context.
 */
static void do_ndmp_query(UAContext *ua, ndm_job_param *ndmp_job, CLIENTRES *client)
{
   struct ndm_session ndmp_sess;

   /*
    * Initialize a new NDMP session
    */
   memset(&ndmp_sess, 0, sizeof(ndmp_sess));
   ndmp_sess.conn_snooping = (director->ndmp_snooping) ? 1 : 0;
   ndmp_sess.control_agent_enabled = 1;

   ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
   memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
   ndmp_sess.param->log.deliver = ndmp_client_status_handler;
   ndmp_sess.param->log.cookie = malloc(sizeof(struct ndmp_log_cookie));
   ndmp_sess.param->log_level = native_to_ndmp_loglevel(client, debug_level, ndmp_sess.param->log.cookie);
   set_ua_in_cookie(ua, ndmp_sess.param->log.cookie);
   ndmp_sess.param->log_tag = bstrdup("DIR-NDMP");

   /*
    * Initialize the session structure.
    */
   if (ndma_session_initialize(&ndmp_sess)) {
      goto bail_out;
   }

   memcpy(&ndmp_sess.control_acb->job, ndmp_job, sizeof(struct ndm_job_param));

   /*
    * Commission the session for a run.
    */
   if (ndma_session_commission(&ndmp_sess)) {
      goto cleanup;
   }

   /*
    * Setup the DMA.
    */
   if (ndmca_connect_control_agent(&ndmp_sess)) {
      goto cleanup;
   }

   ndmp_sess.conn_open = 1;
   ndmp_sess.conn_authorized = 1;

   /*
    * Let the DMA perform its magic.
    */
   if (ndmca_control_agent(&ndmp_sess) != 0) {
      goto cleanup;
   }

cleanup:
   /*
    * Destroy the session.
    */
   ndma_session_destroy(&ndmp_sess);

bail_out:

   /*
    * Free the param block.
    */
   free(ndmp_sess.param->log_tag);
   free(ndmp_sess.param->log.cookie);
   free(ndmp_sess.param);
   ndmp_sess.param = NULL;
}

/*
 * Output the status of a storage daemon when its a normal storage
 * daemon accessed via the NDMP protocol or query the TAPE and ROBOT
 * agent of a native NDMP server.
 */
void do_ndmp_storage_status(UAContext *ua, STORERES *store, char *cmd)
{
   struct ndm_job_param ndmp_job;

   /*
    * See if the storage is just a NDMP instance of a normal storage daemon.
    */
   if (store->paired_storage) {
      do_native_storage_status(ua, store->paired_storage, cmd);
   } else {
      memset(&ndmp_job, 0, sizeof(struct ndm_job_param));
      ndmp_job.operation = NDM_JOB_OP_QUERY_AGENTS;

      /*
       * Query the TAPE agent of the NDMP server.
       */
      if (!fill_ndmp_agent_config(ua->jcr, &ndmp_job.tape_agent,
                                  store->Protocol, store->AuthType,
                                  store->address, store->SDport,
                                  store->username, store->password)) {
         return;
      }

      /*
       * Query the ROBOT agent of the NDMP server.
       */
      if (!fill_ndmp_agent_config(ua->jcr, &ndmp_job.robot_agent,
                                  store->Protocol, store->AuthType,
                                  store->address, store->SDport,
                                  store->username, store->password)) {
         return;
      }

      do_ndmp_query(ua, &ndmp_job, NULL);
   }
}

/*
 * Output the status of a NDMP client. Query the DATA agent of a
 * native NDMP server to give some info.
 */
void do_ndmp_client_status(UAContext *ua, CLIENTRES *client, char *cmd)
{
   struct ndm_job_param ndmp_job;

   memset(&ndmp_job, 0, sizeof(struct ndm_job_param));
   ndmp_job.operation = NDM_JOB_OP_QUERY_AGENTS;

   /*
    * Query the DATA agent of the NDMP server.
    */
   if (!fill_ndmp_agent_config(ua->jcr, &ndmp_job.data_agent,
                               client->Protocol, client->AuthType,
                               client->address, client->FDport,
                               client->username, client->password)) {
      return;
   }

   do_ndmp_query(ua, &ndmp_job, client);
}
#else
bool do_ndmp_backup_init(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

bool do_ndmp_backup(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

void ndmp_backup_cleanup(JCR *jcr, int TermCode)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}

bool do_ndmp_restore_init(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

bool do_ndmp_restore(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

void ndmp_restore_cleanup(JCR *jcr, int TermCode)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}

void do_ndmp_storage_status(UAContext *ua, STORERES *store, char *cmd)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}

void do_ndmp_client_status(UAContext *ua, CLIENTRES *client, char *cmd)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}
#endif /* HAVE_NDMP */
