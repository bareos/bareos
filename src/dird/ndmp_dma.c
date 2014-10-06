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
    * KEEP sibling as the first member to avoid having to do initialization of child
    */
   rblink sibling;
   rblist child;
   char *fname;                   /* File name */
   char *attr;                    /* Encoded stat struct */
   int8_t FileType;               /* Type of File */
   int32_t FileIndex;             /* File index */
   int32_t Offset;                /* File Offset in NDMP stream */
   uint32_t inode;                /* Inode nr */
   uint16_t fname_len;            /* Filename length */
   ndmp_fhdb_node *next;
   ndmp_fhdb_node *parent;
};
typedef struct ndmp_fhdb_node N_TREE_NODE;

struct ndmp_fhdb_root {
   /*
    * KEEP sibling as the first member to avoid having to do initialization of child
    */
   rblink sibling;
   rblist child;
   char *fname;                   /* File name */
   char *attr;                    /* Encoded stat struct */
   int8_t FileType;               /* Type of File */
   int32_t FileIndex;             /* File index */
   int32_t Offset;                /* File Offset in NDMP stream */
   int32_t inode;                 /* Inode nr */
   uint16_t fname_len;            /* Filename length */
   ndmp_fhdb_node *next;
   ndmp_fhdb_node *parent;

   /*
    * The above ^^^ must be identical to a ndmp_fhdb_node structure
    * The below vvv is only for the root of the tree.
    */
   ndmp_fhdb_node *first;         /* first entry in the tree */
   ndmp_fhdb_node *last;          /* last entry in the tree */
   ndmp_fhdb_mem *mem;            /* tree memory */
   uint32_t total_size;           /* total bytes allocated */
   uint32_t blocks;               /* total mallocs */
   ndmp_fhdb_node *cached_parent; /* cached parent */
};
typedef struct ndmp_fhdb_root N_TREE_ROOT;

/*
 * Internal structure to keep track of private data.
 */
struct ndmp_internal_state {
   uint32_t LogLevel;
   JCR *jcr;
   UAContext *ua;
   char *filesystem;
   int32_t FileIndex;
   char *virtual_filename;
   bool save_filehist;
   N_TREE_ROOT *fhdb_root;
};
typedef struct ndmp_internal_state NIS;

static char OKbootstrap[] =
   "3000 OK bootstrap\n";

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

struct ndmp_backup_format_option {
   char *format;
   bool uses_file_history;
   bool uses_level;
   bool restore_prefix_relative;
   bool needs_namelist;
};

static ndmp_backup_format_option ndmp_backup_format_options[] = {
   { (char *)"dump", true, true, true, true },
   { (char *)"tar", true, false, true, true },
   { (char *)"smtape", false, false, false, true },
   { (char *)"zfs", false, true, false, true },
   { NULL, false, false, false }
};

static ndmp_backup_format_option *lookup_backup_format_options(const char *backup_format)
{
   int i = 0;

   while (ndmp_backup_format_options[i].format) {
      if (bstrcasecmp(backup_format, ndmp_backup_format_options[i].format)) {
         break;
      }
      i++;
   }

   if (ndmp_backup_format_options[i].format) {
      return &ndmp_backup_format_options[i];
   }

   return (ndmp_backup_format_option *)NULL;
}

/*
 * Lightweight version of Bareos tree functions for holding the NDMP
 * filehandle index database. See lib/tree.[ch] for the full version.
 */
static void malloc_buf(N_TREE_ROOT *root, int size)
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
 * Note, we allocate a big buffer in the tree root from which we
 * allocate nodes. This runs more than 100 times as fast as directly
 * using malloc() for each of the nodes.
 */
static inline N_TREE_ROOT *ndmp_fhdb_new_tree()
{
   int count = 512;
   N_TREE_ROOT *root;
   uint32_t size;

   root = (N_TREE_ROOT *)malloc(sizeof(N_TREE_ROOT));
   memset(root, 0, sizeof(N_TREE_ROOT));

   /*
    * Assume filename + node  = 40 characters average length
    */
   size = count * (BALIGN(sizeof(N_TREE_ROOT)) + 40);
   if (count > 1000000 || size > (MAX_BUF_SIZE / 2)) {
      size = MAX_BUF_SIZE;
   }

   Dmsg2(400, "count=%d size=%d\n", count, size);

   malloc_buf(root, size);

   return root;
}

/*
 * Allocate bytes for filename in tree structure.
 * Keep the pointers properly aligned by allocating sizes that are aligned.
 */
static inline char *ndmp_fhdb_tree_alloc(N_TREE_ROOT *root, int size)
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
 * This routine can be called to release the previously allocated tree node.
 */
static inline void ndmp_fhdb_free_tree_node(N_TREE_ROOT *root)
{
   int asize = BALIGN(sizeof(N_TREE_NODE));

   root->mem->rem += asize;
   root->mem->mem -= asize;
}

/*
 * Create a new tree node.
 */
static N_TREE_NODE *ndmp_fhdb_new_tree_node(N_TREE_ROOT *root)
{
   N_TREE_NODE *node;
   int size = sizeof(N_TREE_NODE);

   node = (N_TREE_NODE *)ndmp_fhdb_tree_alloc(root, size);
   memset(node, 0, size);

   return node;
}

/*
 * This routine frees the whole tree
 */
static inline void ndmp_fhdb_free_tree(N_TREE_ROOT *root)
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

static int node_compare_by_name(void *item1, void *item2)
{
   N_TREE_NODE *tn1 = (N_TREE_NODE *)item1;
   N_TREE_NODE *tn2 = (N_TREE_NODE *)item2;

   if (tn1->fname[0] > tn2->fname[0]) {
      return 1;
   } else if (tn1->fname[0] < tn2->fname[0]) {
      return -1;
   }
   return strcmp(tn1->fname, tn2->fname);
}

static int node_compare_by_id(void *item1, void *item2)
{
   N_TREE_NODE *tn1 = (N_TREE_NODE *)item1;
   N_TREE_NODE *tn2 = (N_TREE_NODE *)item2;

   if (tn1->inode > tn2->inode) {
      return 1;
   } else if (tn1->inode < tn2->inode) {
      return -1;
   } else {
      return 0;
   }
}

static inline N_TREE_NODE *search_and_insert_tree_node(char *fname, int32_t FileIndex, uint32_t inode,
                                                       N_TREE_ROOT *root, N_TREE_NODE *parent)
{
   N_TREE_NODE *node, *found_node;

   node = ndmp_fhdb_new_tree_node(root);
   if (inode) {
      node->inode = inode;
      found_node = (N_TREE_NODE *)parent->child.insert(node, node_compare_by_id);
   } else {
      node->fname = fname;
      found_node = (N_TREE_NODE *)parent->child.insert(node, node_compare_by_name);
   }

   /*
    * Already in list ?
    */
   if (found_node != node) {
      /*
       * Free node allocated above.
       */
      ndmp_fhdb_free_tree_node(root);
      return found_node;
   }

   /*
    * Its was not found, but now inserted.
    *
    * Allocate a new entry with 2 bytes extra e.g. the extra slash
    * needed for directories and the \0.
    */
   node->FileIndex = FileIndex;
   node->fname_len = strlen(fname);
   node->fname = ndmp_fhdb_tree_alloc(root, node->fname_len + 2);
   bstrncpy(node->fname, fname, node->fname_len + 1);
   node->parent = parent;

   /*
    * Maintain a linear chain of nodes.
    */
   if (!root->first) {
      root->first = node;
      root->last = node;
   } else {
      root->last->next = node;
      root->last = node;
   }

   return node;
}

/*
 * Recursively search the tree for a certain inode number.
 */
static inline N_TREE_NODE *find_tree_node(N_TREE_NODE *node, uint32_t inode)
{
   N_TREE_NODE match_node;
   N_TREE_NODE *found_node, *walker;

   match_node.inode = inode;

   /*
    * Start searching in the children of this node.
    */
   found_node = (N_TREE_NODE *)node->child.search(&match_node, node_compare_by_id);
   if (found_node) {
      return found_node;
   }

   /*
    * The node we are searching for is not one of the top nodes so need to search deeper.
    */
   foreach_rblist(walker, &node->child) {
      /*
       * See if the node has any children otherwise no need to search it.
       */
      if (walker->child.empty()) {
         continue;
      }

      found_node = find_tree_node(walker, inode);
      if (found_node) {
         return found_node;
      }
   }

   return (N_TREE_NODE *)NULL;
}

/*
 * Recursively search the tree for a certain inode number.
 */
static inline N_TREE_NODE *find_tree_node(N_TREE_ROOT *root, uint32_t inode)
{
   N_TREE_NODE match_node;
   N_TREE_NODE *found_node, *walker;

   /*
    * See if this is a request for the root of the tree.
    */
   if (root->inode == inode) {
      return (N_TREE_NODE *)root;
   }

   match_node.inode = inode;

   /*
    * First do the easy lookup e.g. is this inode part of the parent of the current parent.
    */
   if (root->cached_parent && root->cached_parent->parent) {
      found_node = (N_TREE_NODE *)root->cached_parent->parent->child.search(&match_node, node_compare_by_id);
      if (found_node) {
         return found_node;
      }
   }

   /*
    * Start searching from the root node.
    */
   found_node = (N_TREE_NODE *)root->child.search(&match_node, node_compare_by_id);
   if (found_node) {
      return found_node;
   }

   /*
    * The node we are searching for is not one of the top nodes so need to search deeper.
    */
   foreach_rblist(walker, &root->child) {
      /*
       * See if the node has any children otherwise no need to search it.
       */
      if (walker->child.empty()) {
         continue;
      }

      found_node = find_tree_node(walker, inode);
      if (found_node) {
         return found_node;
      }
   }

   return (N_TREE_NODE *)NULL;
}

static inline bool validate_client(JCR *jcr)
{
   switch (jcr->res.client->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      break;
   default:
      Jmsg(jcr, M_FATAL, 0,
           _("Client %s, with backup protocol %s  not compatible for running NDMP backup.\n"),
           jcr->res.client->name(), auth_protocol_to_str(jcr->res.client->Protocol));
      return false;
   }

   return true;
}

static inline bool validate_storage(JCR *jcr, STORERES *store)
{
   switch (store->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Storage %s has illegal backup protocol %s for NDMP backup\n"),
           store->name(), auth_protocol_to_str(store->Protocol));
      return false;
   }

   return true;
}

static inline bool validate_storage(JCR *jcr)
{
   STORERES *store;

   if (jcr->wstorage) {
      foreach_alist(store, jcr->wstorage) {
         if (!validate_storage(jcr, store)) {
            return false;
         }
      }
   } else {
      foreach_alist(store, jcr->rstorage) {
         if (!validate_storage(jcr, store)) {
            return false;
         }
      }
   }

   return true;
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
   ndma_update_env_list(env_tab, &pv);

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
   ndmp_backup_format_option *nbf_options;

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = lookup_backup_format_options(job->bu_type);

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
 * Walk the tree of selected files for restore and lookup the
 * correct fileid. Return the actual full pathname of the file
 * corresponding to the given fileid.
 */
static inline char *lookup_fileindex(JCR *jcr, int32_t FileIndex)
{
   TREE_NODE *node, *parent;
   POOL_MEM restore_pathname, tmp;

   node = first_tree_node(jcr->restore_tree_root);
   while (node) {
      /*
       * See if this is the wanted FileIndex.
       */
      if (node->FileIndex == FileIndex) {
         pm_strcpy(restore_pathname, node->fname);

         /*
          * Walk up the parent until we hit the head of the list.
          */
         for (parent = node->parent; parent; parent = parent->parent) {
            pm_strcpy(tmp, restore_pathname.c_str());
            Mmsg(restore_pathname, "%s/%s", parent->fname, tmp.c_str());
         }

         if (bstrncmp(restore_pathname.c_str(), "/@NDMP/", 7)) {
            return bstrdup(restore_pathname.c_str());
         }
      }

      node = next_tree_node(node);
   }

   return NULL;
}

/*
 * Add a filename to the files we want to restore.
 *
 * The RFC says this:
 *
 * original_path - The original path name of the data to be recovered,
 *                 relative to the backup root. If original_path is the null
 *                 string, the server shall recover all data contained in the
 *                 backup image.
 *
 * destination_path, name, other_name
 *               - Together, these identify the absolute path name to which
 *                 data are to be recovered.
 *
 *               If name is the null string:
 *                 - destination_path identifies the name to which the data
 *                   identified by original_path are to be recovered.
 *                 - other_name must be the null string.
 *
 *               If name is not the null string:
 *                 - destination_path, when concatenated with the server-
 *                   specific path name delimiter and name, identifies the
 *                   name to which the data identified by original_path are
 *                   to be recovered.
 *
 *               If other_name is not the null string:
 *                 - destination_path, when concatenated with the server-
 *                   specific path name delimiter and other_name,
 *                   identifies the alternate name-space name of the data
 *                   to be recovered. The definition of such alternate
 *                   name-space is server-specific.
 *
 * Neither name nor other_name may contain a path name delimiter.
 *
 * Under no circumstance may destination_path be the null string.
 *
 * If intermediate directories that lead to the path name to
 * recover do not exist, the server should create them.
 */
static inline void add_to_namelist(struct ndm_job_param *job,
                                   char *filename,
                                   char *restore_prefix,
                                   char *name,
                                   char *other_name,
                                   int64_t node)
{
   ndmp9_name nl;
   POOL_MEM destination_path;

   memset(&nl, 0, sizeof(ndmp9_name));

   /*
    * See if the filename is an absolute pathname.
    */
   if (*filename == '\0') {
      pm_strcpy(destination_path, restore_prefix);
   } else if (*filename == '/') {
      Mmsg(destination_path, "%s%s", restore_prefix, filename);
   } else {
      Mmsg(destination_path, "%s/%s", restore_prefix, filename);
   }

   nl.original_path = filename;
   nl.destination_path = destination_path.c_str();
   nl.name = name;
   nl.other_name = other_name;
   nl.node = node;

   ndma_store_nlist(&job->nlist_tab, &nl);
}

/*
 * See in the tree with selected files what files were selected to be restored.
 */
static inline int set_files_to_restore(JCR *jcr, struct ndm_job_param *job,
                                       int32_t FileIndex, char *restore_prefix)
{
   int len;
   int cnt = 0;
   TREE_NODE *node, *parent;
   POOL_MEM restore_pathname, tmp;

   node = first_tree_node(jcr->restore_tree_root);
   while (node) {
      /*
       * See if this is the wanted FileIndex and the user asked to extract it.
       */
      if (node->FileIndex == FileIndex && node->extract) {
         pm_strcpy(restore_pathname, node->fname);

         /*
          * Walk up the parent until we hit the head of the list.
          */
         for (parent = node->parent; parent; parent = parent->parent) {
            pm_strcpy(tmp, restore_pathname.c_str());
            Mmsg(restore_pathname, "%s/%s", parent->fname, tmp.c_str());
         }

         /*
          * We only want to restore the non pseudo NDMP names e.g. not the full backup stream name.
          */
         if (!bstrncmp(restore_pathname.c_str(), "/@NDMP/", 7)) {
            /*
             * See if we need to strip the prefix from the filename.
             */
            len = strlen(restore_prefix);
            if (bstrncmp(restore_pathname.c_str(), restore_prefix, len)) {
               add_to_namelist(job,  restore_pathname.c_str() + len, restore_prefix,
                               (char *)"", (char *)"", NDMP_INVALID_U_QUAD);
            } else {
               add_to_namelist(job,  restore_pathname.c_str(), restore_prefix,
                               (char *)"", (char *)"", NDMP_INVALID_U_QUAD);
            }
            cnt++;
         }
      }

      node = next_tree_node(node);
   }

   return cnt;
}

/*
 * Database handler that handles the returned environment data for a given JobId.
 */
static int ndmp_env_handler(void *ctx, int num_fields, char **row)
{
   struct ndm_env_table *envtab;
   ndmp9_pval pv;

   if (row[0] && row[1]) {
      envtab = (struct ndm_env_table *)ctx;
      pv.name = row[0];
      pv.value = row[1];

      ndma_store_env_list(envtab, &pv);
   }

   return 0;
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
        *original_pathname,
        *restore_prefix,
        *level;
   POOL_MEM tape_device;
   POOL_MEM destination_path;
   ndmp_backup_format_option *nbf_options;

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = lookup_backup_format_options(job->bu_type);

   /*
    * Lookup the current fileindex and map it to an actual pathname.
    */
   restore_pathname = lookup_fileindex(jcr, current_fi);
   if (!restore_pathname) {
      return false;
   } else {
      /*
       * Skip over the /@NDMP prefix.
       */
      original_pathname = restore_pathname + 6;
   }

   /*
    * See if there is a level embedded in the pathname.
    */
   bp = strrchr(original_pathname, '%');
   if (bp) {
      *bp++ = '\0';
      level = bp;
   } else {
      level = NULL;
   }

   /*
    * Lookup the environment stack saved during the backup so we can restore it.
    */
   if (!db_get_ndmp_environment_string(jcr, jcr->db, &jcr->jr,
                                       ndmp_env_handler, &job->env_tab)) {
      /*
       * Fallback code try to build a environment stack that is good enough to
       * restore this NDMP backup. This is used when the data is not available in
       * the database when its either expired or when an old NDMP backup is restored
       * where the whole environment was not saved.
       */

      if (!nbf_options || nbf_options->uses_file_history) {
         /*
          * We asked during the NDMP backup to receive file history info.
          */
         pv.name = ndmp_env_keywords[NDMP_ENV_KW_HIST];
         pv.value = ndmp_env_values[NDMP_ENV_VALUE_YES];
         ndma_store_env_list(&job->env_tab, &pv);
      }

      /*
       * Tell the data agent what type of restore stream to expect.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_TYPE];
      pv.value = job->bu_type;
      ndma_store_env_list(&job->env_tab, &pv);

      /*
       * Tell the data agent that this is a NDMP backup which uses a level indicator.
       */
      if (level) {
         pv.name = ndmp_env_keywords[NDMP_ENV_KW_LEVEL];
         pv.value = level;
         ndma_store_env_list(&job->env_tab, &pv);
      }

      /*
       * Tell the data engine what was backuped.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_FILESYSTEM];
      pv.value = original_pathname;
      ndma_store_env_list(&job->env_tab, &pv);
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
          * See if the original path matches.
          */
         if (bstrcasecmp(item, original_pathname)) {
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

   /*
    * Tell the data engine where to restore.
    */
   if (nbf_options && nbf_options->restore_prefix_relative) {
      switch (*restore_prefix) {
      case '^':
         /*
          * Use the restore_prefix as an absolute restore prefix.
          * We skip the leading ^ that is the trigger for absolute restores.
          */
         pm_strcpy(destination_path, restore_prefix + 1);
         break;
      default:
         /*
          * Use the restore_prefix as an relative restore prefix.
          */
         if (strlen(restore_prefix) == 1 && *restore_prefix == '/') {
            pm_strcpy(destination_path, original_pathname);
         } else {
            pm_strcpy(destination_path, restore_prefix);
            pm_strcat(destination_path, original_pathname);
         }
      }
   } else {
      if (strlen(restore_prefix) == 1 && *restore_prefix == '/') {
         /*
          * Use the original pathname as restore prefix.
          */
         pm_strcpy(destination_path, original_pathname);
      } else {
         /*
          * Use the restore_prefix as an absolute restore prefix.
          */
         pm_strcpy(destination_path, restore_prefix);
      }
   }

   pv.name = ndmp_env_keywords[NDMP_ENV_KW_PREFIX];
   pv.value = destination_path.c_str();
   ndma_store_env_list(&job->env_tab, &pv);

   if (!nbf_options || nbf_options->needs_namelist) {
      if (set_files_to_restore(jcr, job, current_fi,
                               destination_path.c_str()) == 0) {
         /*
          * There is no specific filename selected so restore everything.
          */
         add_to_namelist(job, (char *)"", destination_path.c_str(),
                         (char *)"", (char *)"", NDMP_INVALID_U_QUAD);
      }
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
   ASSERT(client->password.encoding == p_encoding_clear);
   if (!fill_ndmp_agent_config(jcr, &job->data_agent, client->Protocol,
                               client->AuthType, client->address,
                               client->FDport, client->username,
                               client->password.value)) {
      goto bail_out;
   }

   /*
    * The tape_agent is the storage daemon via the NDMP protocol.
    */
   ASSERT(store->password.encoding == p_encoding_clear);
   if (!fill_ndmp_agent_config(jcr, &job->tape_agent, store->Protocol,
                               store->AuthType, store->address,
                               store->SDport, store->username,
                               store->password.value)) {
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

/*
 * Store all entries in the FHDB as hardlinked items to the NDMP archive in the backup catalog.
 */
static inline void store_attribute_record(JCR *jcr, char *fname, char *linked_fname,
                                          char *attributes, int8_t FileType, int32_t Offset)
{
   ATTR_DBR *ar;

   ar = jcr->ar;
   if (jcr->cached_attribute) {
      Dmsg2(400, "Cached attr. Stream=%d fname=%s\n", ar->Stream, ar->fname);
      if (!db_create_attributes_record(jcr, jcr->db, ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error: ERR=%s"), db_strerror(jcr->db));
         return;
      }
      jcr->cached_attribute = false;
   }

   /*
    * We only update some fields of this structure the rest is already filled
    * before by initial attributes saved by the tape agent in the storage daemon.
    */
   jcr->ar->fname = fname;
   jcr->ar->link = linked_fname;
   jcr->ar->attr = attributes;
   jcr->ar->Stream = STREAM_UNIX_ATTRIBUTES;
   jcr->ar->FileType = FileType;
   jcr->ar->DeltaSeq = Offset;

   if (!db_create_attributes_record(jcr, jcr->db, ar)) {
      Jmsg1(jcr, M_FATAL, 0, _("Attribute create error: ERR=%s"), db_strerror(jcr->db));
      return;
   }
}

static inline void convert_fstat(ndmp9_file_stat *fstat, int32_t FileIndex,
                                 int8_t *FileType, POOL_MEM &attribs)
{
   struct stat statp;

   /*
    * Convert the NDMP file_stat structure into a UNIX one.
    */
   memset(&statp, 0, sizeof(statp));

   /*
    * If we got a valid mode of the file fill the UNIX stat struct.
    */
   if (fstat->mode.valid == NDMP9_VALIDITY_VALID) {
      switch (fstat->ftype) {
      case NDMP9_FILE_DIR:
         statp.st_mode = fstat->mode.value | S_IFDIR;
         *FileType = FT_DIREND;
         break;
      case NDMP9_FILE_FIFO:
         statp.st_mode = fstat->mode.value | S_IFIFO;
         *FileType = FT_FIFO;
         break;
      case NDMP9_FILE_CSPEC:
         statp.st_mode = fstat->mode.value | S_IFCHR;
         *FileType = FT_SPEC;
         break;
      case NDMP9_FILE_BSPEC:
         statp.st_mode = fstat->mode.value | S_IFBLK;
         *FileType = FT_SPEC;
         break;
      case NDMP9_FILE_REG:
         statp.st_mode = fstat->mode.value | S_IFREG;
         *FileType = FT_REG;
         break;
      case NDMP9_FILE_SLINK:
         statp.st_mode = fstat->mode.value | S_IFLNK;
         *FileType = FT_LNK;
         break;
      case NDMP9_FILE_SOCK:
         statp.st_mode = fstat->mode.value | S_IFSOCK;
         *FileType = FT_SPEC;
         break;
      case NDMP9_FILE_REGISTRY:
         statp.st_mode = fstat->mode.value | S_IFREG;
         *FileType = FT_REG;
         break;
      case NDMP9_FILE_OTHER:
         statp.st_mode = fstat->mode.value | S_IFREG;
         *FileType = FT_REG;
         break;
      default:
         break;
      }

      if (fstat->mtime.valid == NDMP9_VALIDITY_VALID) {
         statp.st_mtime = fstat->mtime.value;
      }

      if (fstat->atime.valid == NDMP9_VALIDITY_VALID) {
         statp.st_atime = fstat->atime.value;
      }

      if (fstat->ctime.valid == NDMP9_VALIDITY_VALID) {
         statp.st_ctime = fstat->ctime.value;
      }

      if (fstat->uid.valid == NDMP9_VALIDITY_VALID) {
         statp.st_uid = fstat->uid.value;
      }

      if (fstat->gid.valid == NDMP9_VALIDITY_VALID) {
         statp.st_gid = fstat->gid.value;
      }

      if (fstat->links.valid == NDMP9_VALIDITY_VALID) {
         statp.st_nlink = fstat->links.value;
      }
   }

   /*
    * Encode a stat structure into an ASCII string.
    */
   encode_stat(attribs.c_str(), &statp, sizeof(statp), FileIndex, STREAM_UNIX_ATTRIBUTES);
}

extern "C" int bndmp_add_file(struct ndmlog *ixlog, int tagc, char *raw_name,
                              ndmp9_file_stat *fstat)
{
   NIS *nis;

   nis = (NIS *)ixlog->ctx;
   nis->jcr->lock();
   nis->jcr->JobFiles++;
   nis->jcr->unlock();

   if (nis->save_filehist) {
      int8_t FileType = 0;
      char namebuf[NDMOS_CONST_PATH_MAX];
      POOL_MEM attribs(PM_FNAME),
               pathname(PM_FNAME);

      ndmcstr_from_str(raw_name, namebuf, sizeof(namebuf));

      /*
       * Every file entry is releative from the filesystem currently being backuped.
       */
      Dmsg2(100, "bndmp_add_file: New filename ==> %s/%s\n", nis->filesystem, namebuf);

      if (nis->jcr->ar) {
         /*
          * See if this is the top level entry of the tree e.g. len == 0
          */
         if (strlen(namebuf) == 0) {
            convert_fstat(fstat, nis->FileIndex, &FileType, attribs);

            pm_strcpy(pathname, nis->filesystem);
            pm_strcat(pathname, "/");
            return 0;
         } else {
            convert_fstat(fstat, nis->FileIndex, &FileType, attribs);

            pm_strcpy(pathname, nis->filesystem);
            pm_strcat(pathname, "/");
            pm_strcat(pathname, namebuf);

            if (FileType == FT_DIREND) {
               /*
                * A directory needs to end with a slash.
                */
               pm_strcat(pathname, "/");
            }
         }

         store_attribute_record(nis->jcr, pathname.c_str(), nis->virtual_filename, attribs.c_str(), FileType,
                               (fstat->fh_info.valid == NDMP9_VALIDITY_VALID) ? fstat->fh_info.value : 0);
      }
   }

   return 0;
}

extern "C" int bndmp_add_dir(struct ndmlog *ixlog, int tagc, char *raw_name,
                             ndmp9_u_quad dir_node, ndmp9_u_quad node)
{
   NIS *nis;
   char namebuf[NDMOS_CONST_PATH_MAX];

   ndmcstr_from_str(raw_name, namebuf, sizeof(namebuf));

   /*
    * Ignore . and .. directory entries.
    */
   if (bstrcmp(namebuf, ".") || bstrcmp(namebuf, "..")) {
      return 0;
   }

   nis = (NIS *)ixlog->ctx;
   nis->jcr->lock();
   nis->jcr->JobFiles++;
   nis->jcr->unlock();

   if (nis->save_filehist) {
      Dmsg3(100, "bndmp_add_dir: New filename ==> %s [%llu] - [%llu]\n", namebuf, dir_node, node);

      if (!nis->fhdb_root) {
         Jmsg(nis->jcr, M_FATAL, 0, _("NDMP protocol error, FHDB add_dir call before add_dirnode_root.\n"));
         return 1;
      }

      /*
       * See if this entry is in the cached parent.
       */
      if (nis->fhdb_root->cached_parent &&
          nis->fhdb_root->cached_parent->inode == dir_node) {
         search_and_insert_tree_node(namebuf, nis->fhdb_root->FileIndex, node,
                                     nis->fhdb_root, nis->fhdb_root->cached_parent);
      } else {
         /*
          * Not the cached parent search the tree where it need to be put.
          */
         nis->fhdb_root->cached_parent = find_tree_node(nis->fhdb_root, dir_node);
         if (nis->fhdb_root->cached_parent) {
            search_and_insert_tree_node(namebuf, nis->fhdb_root->FileIndex, node,
                                        nis->fhdb_root, nis->fhdb_root->cached_parent);
         } else {
            Jmsg(nis->jcr, M_FATAL, 0, _("NDMP protocol error, FHDB add_dir for unknown parent inode %d.\n"), dir_node);
            return 1;
         }
      }
   }

   return 0;
}

extern "C" int bndmp_add_node(struct ndmlog *ixlog, int tagc,
                              ndmp9_u_quad node, ndmp9_file_stat *fstat)
{
   NIS *nis;

   nis = (NIS *)ixlog->ctx;

   if (nis->save_filehist) {
      int attr_size;
      int8_t FileType = 0;
      N_TREE_NODE *wanted_node;
      POOL_MEM attribs(PM_FNAME);

      Dmsg1(100, "bndmp_add_node: New node [%llu]\n", node);

      if (!nis->fhdb_root) {
         Jmsg(nis->jcr, M_FATAL, 0, _("NDMP protocol error, FHDB add_node call before add_dir.\n"));
         return 1;
      }

      wanted_node = find_tree_node(nis->fhdb_root, node);
      if (!wanted_node) {
         Jmsg(nis->jcr, M_FATAL, 0, _("NDMP protocol error, FHDB add_node request for unknown node %llu.\n"), node);
         return 1;
      }

      convert_fstat(fstat, nis->FileIndex, &FileType, attribs);
      attr_size = strlen(attribs.c_str()) + 1;

      wanted_node->attr = ndmp_fhdb_tree_alloc(nis->fhdb_root, attr_size);
      bstrncpy(wanted_node->attr, attribs, attr_size);
      wanted_node->FileType = FileType;
      if (fstat->fh_info.valid == NDMP9_VALIDITY_VALID) {
         wanted_node->Offset = fstat->fh_info.value;
      }

      if (FileType == FT_DIREND) {
         /*
          * A directory needs to end with a slash.
          */
         strcat(wanted_node->fname, "/");
      }
   }

   return 0;
}

extern "C" int bndmp_add_dirnode_root(struct ndmlog *ixlog, int tagc,
                                      ndmp9_u_quad root_node)
{
   NIS *nis;

   nis = (NIS *)ixlog->ctx;

   if (nis->save_filehist) {
      Dmsg1(100, "bndmp_add_dirnode_root: New root node [%llu]\n", root_node);

      if (nis->fhdb_root) {
         Jmsg(nis->jcr, M_FATAL, 0, _("NDMP protocol error, FHDB add_dirnode_root call more then once.\n"));
         return 1;
      }

      nis->fhdb_root = ndmp_fhdb_new_tree();

      /*
       * Allocate a new entry with 2 bytes extra e.g. the extra slash
       * needed for directories and the \0.
       */
      nis->fhdb_root->fname_len = strlen(nis->filesystem);
      nis->fhdb_root->fname = ndmp_fhdb_tree_alloc(nis->fhdb_root, nis->fhdb_root->fname_len + 2);
      bstrncpy(nis->fhdb_root->fname, nis->filesystem, nis->fhdb_root->fname_len + 1);
      nis->fhdb_root->inode = root_node;
      nis->fhdb_root->FileIndex = nis->FileIndex;
      nis->fhdb_root->cached_parent = (N_TREE_NODE *)nis->fhdb_root;
   }

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
                                             struct ndm_session *sess,
                                             NIS *nis)
{
   bool retval = true;
   struct ndmmedia *me;
   ndmp_backup_format_option *nbf_options;
   struct ndm_env_entry *ndm_ee;

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = lookup_backup_format_options(sess->control_acb->job.bu_type);

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

   if (nis->fhdb_root) {
      if (nis->jcr->ar) {
         N_TREE_NODE *node, *parent;
         POOL_MEM fname, tmp;

         /*
          * Store the toplevel entry of the tree.
          */
         Dmsg2(100, "==> %s [%s]\n", nis->fhdb_root->fname, nis->fhdb_root->attr);
         store_attribute_record(nis->jcr, nis->fhdb_root->fname, nis->virtual_filename,
                                nis->fhdb_root->attr, nis->fhdb_root->FileType,
                                nis->fhdb_root->Offset);

         /*
          * Store all the other entries in the tree.
          */
         for (node = nis->fhdb_root->first; node; node = node->next) {
            pm_strcpy(fname, node->fname);

            /*
             * Walk up the parent until we hit the head of the list.
             * As directories are store including there trailing slash we
             * can just concatenate the two parts.
             */
            for (parent = node->parent; parent; parent = parent->parent) {
               pm_strcpy(tmp, fname.c_str());
               pm_strcpy(fname, parent->fname);
               pm_strcat(fname, tmp.c_str());
            }

            /*
             * Now we have the full pathname of the file in fname.
             * Store the entry as a hardlinked entry to the original NDMP archive.
             */
            Dmsg2(100, "==> %s [%s]\n", fname.c_str(), node->attr);
            store_attribute_record(nis->jcr, fname.c_str(), nis->virtual_filename,
                                   node->attr, node->FileType, node->Offset);
         }
      }

      /*
       * Destroy the tree.
       */
      ndmp_fhdb_free_tree(nis->fhdb_root);
      nis->fhdb_root = NULL;
   }

   /*
    * Update the Job statistics from the NDMP statistics.
    */
   jcr->JobBytes += sess->control_acb->job.bytes_written;

   /*
    * After a successfull backup we need to store all NDMP ENV variables
    * for doing a successfull restore operation.
    */
   ndm_ee = sess->control_acb->job.result_env_tab.head;
   while (ndm_ee) {
      if (!db_create_ndmp_environment_string(jcr, jcr->db, &jcr->jr,
                                             ndm_ee->pval.name, ndm_ee->pval.value)) {
         break;
      }
      ndm_ee = ndm_ee->next;
   }

   /*
    * If this was a NDMP backup with backup type dump save the last used dump level.
    */
   if (nbf_options && nbf_options->uses_level) {
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
 * Calculate the wanted NDMP loglevel from the current debug level and
 * any configure minimum level.
 */
static inline int native_to_ndmp_loglevel(CLIENTRES *client, int debuglevel, NIS *nis)
{
   unsigned int level;

   memset(nis, 0, sizeof(NIS));

   /*
    * Take the highest loglevel from either the Director config or the client config.
    */
   if (client && (client->ndmp_loglevel > me->ndmp_loglevel)) {
      nis->LogLevel = client->ndmp_loglevel;
   } else {
      nis->LogLevel = me->ndmp_loglevel;
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
   if (level < nis->LogLevel) {
      level = nis->LogLevel;
   }

   /*
    * Make sure the level is in the wanted range.
    */
   if (level > 9) {
      level = 9;
   }

   return level;
}

/*
 * Interface function which glues the logging infra of the NDMP lib with the daemon.
 */
extern "C" void ndmp_loghandler(struct ndmlog *log, char *tag, int level, char *msg)
{
   int internal_level = level * 100;
   NIS *nis;

   /*
    * We don't want any trailing newline in log messages.
    */
   strip_trailing_newline(msg);

   /*
    * Make sure if the logging system was setup properly.
    */
   nis = (NIS *)log->ctx;
   if (!nis) {
      return;
   }

   /*
    * If the log level of this message is under our logging treshold we
    * log it as part of the Job.
    */
   if ((internal_level / 100) <= nis->LogLevel) {
      if (nis->jcr) {
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
               Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
               break;
            case 'e':
               Jmsg(nis->jcr, M_ERROR, 0, "%s\n", msg);
               break;
            case 'w':
               Jmsg(nis->jcr, M_WARNING, 0, "%s\n", msg);
               break;
            case '?':
               Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
               break;
            default:
               break;
            }
         } else {
            Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
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
      return false;
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
    * Validate the Job to have a NDMP client and NDMP storage.
    */
   if (!validate_client(jcr)) {
      return false;
   }

   if (!validate_storage(jcr)) {
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

   if (check_hardquotas(jcr)) {
      Jmsg(jcr, M_FATAL, 0, "Quota Exceeded. Job terminated.");
      return false;
   }

   if (check_softquotas(jcr)) {
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
      if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
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
      NIS *nis;
      char *item;
      INCEXE *ie = fileset->include_items[i];
      POOL_MEM virtual_filename(PM_FNAME);

      /*
       * Loop over each file = entry of the fileset.
       */
      for (j = 0; j < ie->name_list.size(); j++) {
         item = (char *)ie->name_list.get(j);

         /*
          * See if this is the first Backup run or not. For NDMP we can have multiple Backup
          * runs as part of the same Job. When we are saving data to a Native Storage Daemon
          * we let it know to expect a new backup session. It will generate a new authorization
          * key so we wait for the nextrun_ready conditional variable to be raised by the msg_thread.
          */
         if (jcr->store_bsock && cnt > 0) {
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
         ndmp_sess.conn_snooping = (me->ndmp_snooping) ? 1 : 0;
         ndmp_sess.control_agent_enabled = 1;

         ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
         memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
         ndmp_sess.param->log.deliver = ndmp_loghandler;
         nis = (NIS *)malloc(sizeof(NIS));
         ndmp_sess.param->log_level = native_to_ndmp_loglevel(jcr->res.client, debug_level, nis);
         nis->filesystem = item;
         nis->FileIndex = cnt + 1;
         nis->jcr = jcr;
         nis->save_filehist = jcr->res.job->SaveFileHist;

         /*
          * The full ndmp archive has a virtual filename, we need it to hardlink the individual
          * file records to it. So we allocate it here once so its available during the whole
          * NDMP session.
          */
         if (bstrcasecmp(jcr->backup_format, "dump")) {
            Mmsg(virtual_filename, "/@NDMP%s%%%d", nis->filesystem, jcr->DumpLevel);
         } else {
            Mmsg(virtual_filename, "/@NDMP%s", nis->filesystem);
         }

         if (nis->virtual_filename) {
            free(nis->virtual_filename);
         }
         nis->virtual_filename = bstrdup(virtual_filename.c_str());

         ndmp_sess.param->log.ctx = nis;
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
                                      nis->filesystem,
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
          * We can use the same private pointer used in the logging with the JCR in
          * the file index generation. We don't setup a index_log.deliver
          * function as we catch the index information via callbacks.
          */
         ndmp_sess.control_acb->job.index_log.ctx = ndmp_sess.param->log.ctx;

         /*
          * Let the DMA perform its magic.
          */
         if (ndmca_control_agent(&ndmp_sess) != 0) {
            goto cleanup;
         }

         /*
          * See if there were any errors during the backup.
          */
         jcr->jr.FileIndex = cnt + 1;
         if (!extract_post_backup_stats(jcr, item, &ndmp_sess, (NIS *)ndmp_sess.param->log.ctx)) {
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
         free(ndmp_sess.param->log.ctx);
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
      NIS *nis = (NIS *)ndmp_sess.param->log.ctx;

      if (nis->fhdb_root) {
         ndmp_fhdb_free_tree(nis->fhdb_root);
      }

      if (nis->virtual_filename) {
         free(nis->virtual_filename);
      }

      free(ndmp_sess.param->log_tag);
      free(ndmp_sess.param->log.ctx);
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
   CLIENT_DBR cr;

   Dmsg2(100, "Enter ndmp_backup_cleanup %d %c\n", TermCode, TermCode);
   memset(&cr, 0, sizeof(cr));

   if (jcr->is_JobStatus(JS_Terminated) &&
      (jcr->JobErrors || jcr->SDErrors || jcr->JobWarnings)) {
      TermCode = JS_Warnings;
   }

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

   update_bootstrap_file(jcr);

   switch (jcr->JobStatus) {
      case JS_Terminated:
         term_msg = _("Backup OK");
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
            if (jcr->SD_msg_chan_started) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      case JS_Canceled:
         term_msg = _("Backup Canceled");
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan_started) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      default:
         term_msg = term_code;
         sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
         break;
   }

   generate_backup_summary(jcr, &cr, msg_type, term_msg);

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
    * so that we let the SD despool.
    */
   Dmsg4(100, "cancel=%d FDJS=%d JS=%d SDJS=%d\n",
         jcr->is_canceled(), jcr->FDJobStatus,
         jcr->JobStatus, jcr->SDJobStatus);
   if (jcr->is_canceled() || (!jcr->res.job->RescheduleIncompleteJobs)) {
      Dmsg3(100, "FDJS=%d JS=%d SDJS=%d\n",
            jcr->FDJobStatus, jcr->JobStatus, jcr->SDJobStatus);
      cancel_storage_daemon_job(jcr);
   }

   /*
    * Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/JobErrors
    */
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
 * through it processing each storage device in turn. If the
 * storage is different from the prior one, we open a new connection
 * to the new storage and do a restore for that part.
 *
 * This permits handling multiple storage daemons for a single
 * restore.  E.g. your Full is stored on tape, and Incrementals
 * on disk.
 */
static inline bool do_ndmp_restore_bootstrap(JCR *jcr)
{
   int cnt;
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
   if (!jcr->res.pstore) {
      Jmsg(jcr, M_FATAL, 0,
           _("Read storage %s doesn't point to storage definition with paired storage option.\n"),
           jcr->res.rstore->name());
      goto bail_out;
   }

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
         goto cleanup;
      }

      /*
       * Initialize the ndmp restore job. We build the generic job once per storage daemon
       * and reuse the job definition for each seperate sub-restore we perform as
       * part of the whole job. We only free the env_table between every sub-restore.
       */
      if (!build_ndmp_job(jcr, jcr->res.client, jcr->res.pstore, NDM_JOB_OP_EXTRACT, &ndmp_job)) {
         goto cleanup;
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
      if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
         goto cleanup;
      }
      sd = jcr->store_bsock;

      /*
       * Now start a job with the Storage daemon
       */
      if (!start_storage_daemon_job(jcr, jcr->rstorage, NULL)) {
         goto cleanup;
      }

      jcr->setJobStatus(JS_Running);

      /*
       * Send the bootstrap file -- what Volumes/files to restore
       */
      if (!send_bootstrap_file(jcr, sd, info) ||
          !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
         goto cleanup;
      }

      if (!sd->fsend("run")) {
         goto cleanup;
      }

      /*
       * Now start a Storage daemon message thread
       */
      if (!start_storage_daemon_message_thread(jcr)) {
         goto cleanup;
      }
      Dmsg0(50, "Storage daemon connection OK\n");

      /*
       * Walk each fileindex of the current BSR record. Each different fileindex is
       * a seperate NDMP stream.
       */
      cnt = 0;
      for (fileindex = bsr->FileIndex; fileindex; fileindex = fileindex->next) {
         for (current_fi = fileindex->findex; current_fi <= fileindex->findex2; current_fi++) {
            NIS *nis;

            /*
             * See if this is the first Restore NDMP stream or not. For NDMP we can have multiple Backup
             * runs as part of the same Job. When we are restoring data from a Native Storage Daemon
             * we let it know to expect a next restore session. It will generate a new authorization
             * key so we wait for the nextrun_ready conditional variable to be raised by the msg_thread.
             */
            if (jcr->store_bsock && cnt > 0) {
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
            ndmp_sess.conn_snooping = (me->ndmp_snooping) ? 1 : 0;
            ndmp_sess.control_agent_enabled = 1;

            ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
            memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
            ndmp_sess.param->log.deliver = ndmp_loghandler;
            nis = (NIS *)malloc(sizeof(NIS));
            ndmp_sess.param->log_level = native_to_ndmp_loglevel(jcr->res.client, debug_level, nis);
            nis->jcr = jcr;
            ndmp_sess.param->log.ctx = nis;
            ndmp_sess.param->log_tag = bstrdup("DIR-NDMP");

            /*
             * Initialize the session structure.
             */
            if (ndma_session_initialize(&ndmp_sess)) {
               goto cleanup_ndmp;
            }
            session_initialized = true;

            /*
             * Copy the actual job to perform.
             */
            jcr->jr.FileIndex = current_fi;
            if (bsr->sessid && bsr->sesstime) {
               jcr->jr.VolSessionId = bsr->sessid->sessid;
               jcr->jr.VolSessionTime = bsr->sesstime->sesstime;
            } else {
               Jmsg(jcr, M_FATAL, 0, _("Wrong BSR missing sessid and/or sesstime\n"));
               goto cleanup_ndmp;
            }

            memcpy(&ndmp_sess.control_acb->job, &ndmp_job, sizeof(struct ndm_job_param));
            if (!fill_restore_environment(jcr,
                                          current_fi,
                                          &ndmp_sess.control_acb->job)) {
               goto cleanup_ndmp;
            }

            ndma_job_auto_adjust(&ndmp_sess.control_acb->job);
            if (!validate_ndmp_job(jcr, &ndmp_sess.control_acb->job)) {
               goto cleanup_ndmp;
            }

            /*
             * Commission the session for a run.
             */
            if (ndma_session_commission(&ndmp_sess)) {
               goto cleanup_ndmp;
            }

            /*
             * Setup the DMA.
             */
            if (ndmca_connect_control_agent(&ndmp_sess)) {
               goto cleanup_ndmp;
            }

            ndmp_sess.conn_open = 1;
            ndmp_sess.conn_authorized = 1;

            /*
             * Let the DMA perform its magic.
             */
            if (ndmca_control_agent(&ndmp_sess) != 0) {
               goto cleanup_ndmp;
            }

            /*
             * See if there were any errors during the restore.
             */
            if (!extract_post_restore_stats(jcr, &ndmp_sess)) {
               goto cleanup_ndmp;
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
            free(ndmp_sess.param->log.ctx);
            free(ndmp_sess.param);
            ndmp_sess.param = NULL;

            /*
             * Reset the initialized state so we don't try to cleanup again.
             */
            session_initialized = false;

            /*
             * Keep track that we finished this part of the restore.
             */
            cnt++;
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
   goto cleanup;

cleanup_ndmp:
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
      free(ndmp_sess.param->log.ctx);
      free(ndmp_sess.param);
   }

cleanup:
   free_paired_storage(jcr);
   close_bootstrap_file(info);

bail_out:
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

   /*
    * Validate the Job to have a NDMP client.
    */
   if (!validate_client(jcr)) {
      return false;
   }

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
         if (jcr->SD_msg_chan_started) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   case JS_Canceled:
      term_msg = _("Restore Canceled");
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan_started) {
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
   NIS *nis;

   /*
    * Make sure if the logging system was setup properly.
    */
   nis = (NIS *)log->ctx;
   if (!nis) {
      return;
   }

   nis->ua->send_msg("%s\n", msg);
}

/*
 * Generic function to query the NDMP server using the NDM_JOB_OP_QUERY_AGENTS
 * operation. Callback is the above ndmp_client_status_handler which prints
 * the data to the user context.
 */
static void do_ndmp_query(UAContext *ua, ndm_job_param *ndmp_job, CLIENTRES *client)
{
   struct ndm_session ndmp_sess;
   NIS *nis;

   /*
    * Initialize a new NDMP session
    */
   memset(&ndmp_sess, 0, sizeof(ndmp_sess));
   ndmp_sess.conn_snooping = (me->ndmp_snooping) ? 1 : 0;
   ndmp_sess.control_agent_enabled = 1;

   ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
   memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
   ndmp_sess.param->log.deliver = ndmp_client_status_handler;
   nis = (NIS *)malloc(sizeof(NIS));
   memset(nis, 0, sizeof(NIS));
   ndmp_sess.param->log_level = native_to_ndmp_loglevel(client, debug_level, nis);
   nis->ua = ua;
   ndmp_sess.param->log.ctx = nis;
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
   free(ndmp_sess.param->log.ctx);
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
      ASSERT(store->password.encoding == p_encoding_clear);
      if (!fill_ndmp_agent_config(ua->jcr, &ndmp_job.tape_agent,
                                  store->Protocol, store->AuthType,
                                  store->address, store->SDport,
                                  store->username, store->password.value)) {
         return;
      }

      /*
       * Query the ROBOT agent of the NDMP server.
       */
      if (!fill_ndmp_agent_config(ua->jcr, &ndmp_job.robot_agent,
                                  store->Protocol, store->AuthType,
                                  store->address, store->SDport,
                                  store->username, store->password.value)) {
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
   ASSERT(client->password.encoding == p_encoding_clear);
   if (!fill_ndmp_agent_config(ua->jcr, &ndmp_job.data_agent,
                               client->Protocol, client->AuthType,
                               client->address, client->FDport,
                               client->username, client->password.value)) {
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
