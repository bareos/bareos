/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Planets Communications B.V.
   Copyright (C) 2015-2019 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, May 2015
 */
/**
 * @file
 * In memory FHDB for NDMP Data Management Application (DMA)
 */

#include "include/bareos.h"
#include "dird.h"

#if HAVE_NDMP
#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"
#endif /* HAVE_NDMP */

namespace directordaemon {

#if HAVE_NDMP

#define B_PAGE_SIZE 4096
#define MIN_PAGES 128
#define MAX_PAGES 2400
#define MAX_BUF_SIZE (MAX_PAGES * B_PAGE_SIZE) /* approx 10MB */

/*
 * Lightweight version of Bareos tree layout for holding the NDMP
 * filehandle index database. See lib/tree.[ch] for the full version.
 */
struct ndmp_fhdb_mem {
  struct ndmp_fhdb_mem* next; /* Next buffer */
  int rem;                    /* Remaining bytes */
  char* mem;                  /* Memory pointer */
  char first[1];              /* First byte */
};

struct ndmp_fhdb_node {
  /*
   * KEEP sibling as the first member to avoid having to do initialization of
   * child
   */
  rblink sibling;
  rblist child;
  char* fname;        /* File name */
  char* attr;         /* Encoded stat struct */
  int8_t FileType;    /* Type of File */
  int32_t FileIndex;  /* File index */
  uint64_t Offset;    /* File Offset in NDMP stream */
  uint64_t inode;     /* Inode nr */
  uint16_t fname_len; /* Filename length */
  ndmp_fhdb_node* next;
  ndmp_fhdb_node* parent;
};
typedef struct ndmp_fhdb_node N_TREE_NODE;

struct ndmp_fhdb_root {
  /*
   * KEEP sibling as the first member to avoid having to do initialization of
   * child
   */
  rblink sibling;
  rblist child;
  char* fname{nullptr};  /**< File name */
  char* attr{nullptr};   /**< Encoded stat struct */
  int8_t FileType{0};    /**< Type of File */
  int32_t FileIndex{0};  /**< File index */
  uint64_t Offset{0};    /**< File Offset in NDMP stream */
  uint64_t inode{0};     /**< Inode nr */
  uint16_t fname_len{0}; /**< Filename length */
  ndmp_fhdb_node* next{nullptr};
  ndmp_fhdb_node* parent{nullptr};

  /*
   * The above ^^^ must be identical to a ndmp_fhdb_node structure
   * The below vvv is only for the root of the tree.
   */
  ndmp_fhdb_node* first{nullptr};         /**< first entry in the tree */
  ndmp_fhdb_node* last{nullptr};          /**< last entry in the tree */
  ndmp_fhdb_mem* mem{nullptr};            /**< tree memory */
  uint32_t total_size{0};                 /**< total bytes allocated */
  uint32_t blocks{0};                     /**< total mallocs */
  ndmp_fhdb_node* cached_parent{nullptr}; /**< cached parent */
};
typedef struct ndmp_fhdb_root N_TREE_ROOT;

struct ooo_metadata {
  hlink link;
  uint64_t dir_node;
  N_TREE_NODE* nt_node;
};
typedef struct ooo_metadata OOO_MD;

struct fhdb_state {
  N_TREE_ROOT* fhdb_root;
  htable* out_of_order_metadata;
};

/**
 * Lightweight version of Bareos tree functions for holding the NDMP
 * filehandle index database. See lib/tree.[ch] for the full version.
 */
static void MallocBuf(N_TREE_ROOT* root, int size)
{
  struct ndmp_fhdb_mem* mem;

  mem = (struct ndmp_fhdb_mem*)malloc(size);
  root->total_size += size;
  root->blocks++;
  mem->next = root->mem;
  root->mem = mem;
  mem->mem = mem->first;
  mem->rem = (char*)mem + size - mem->mem;
}

/*
 * Note, we allocate a big buffer in the tree root from which we
 * allocate nodes. This runs more than 100 times as fast as directly
 * using malloc() for each of the nodes.
 */
static inline N_TREE_ROOT* ndmp_fhdb_new_tree()
{
  int count = 512;
  N_TREE_ROOT* root;
  uint32_t size;

  root = (N_TREE_ROOT*)malloc(sizeof(N_TREE_ROOT));
  static const N_TREE_ROOT empty_N_TREE_ROOT{};
  *root = empty_N_TREE_ROOT;

  /*
   * Assume filename + node  = 40 characters average length
   */
  size = count * (BALIGN(sizeof(N_TREE_ROOT)) + 40);
  if (size > (MAX_BUF_SIZE / 2)) { size = MAX_BUF_SIZE; }

  Dmsg2(400, "count=%d size=%d\n", count, size);

  MallocBuf(root, size);

  return root;
}

/*
 * Allocate bytes for filename in tree structure.
 * Keep the pointers properly aligned by allocating sizes that are aligned.
 */
static inline char* ndmp_fhdb_tree_alloc(N_TREE_ROOT* root, int size)
{
  char* buf;
  int asize = BALIGN(size);

  if (root->mem->rem < asize) {
    uint32_t mb_size;

    if (root->total_size >= (MAX_BUF_SIZE / 2)) {
      mb_size = MAX_BUF_SIZE;
    } else {
      mb_size = MAX_BUF_SIZE / 2;
    }
    MallocBuf(root, mb_size);
  }

  root->mem->rem -= asize;
  buf = root->mem->mem;
  root->mem->mem += asize;

  return buf;
}

/*
 * This routine can be called to release the previously allocated tree node.
 */
static inline void NdmpFhdbFreeTreeNode(N_TREE_ROOT* root)
{
  int asize = BALIGN(sizeof(N_TREE_NODE));

  root->mem->rem += asize;
  root->mem->mem -= asize;
}

/*
 * Create a new tree node.
 */
static N_TREE_NODE* ndmp_fhdb_new_tree_node(N_TREE_ROOT* root)
{
  N_TREE_NODE* node;
  int size = sizeof(N_TREE_NODE);

  node = (N_TREE_NODE*)ndmp_fhdb_tree_alloc(root, size);
  static N_TREE_NODE empty_N_TREE_NODE{};
  *node = empty_N_TREE_NODE;

  return node;
}

/*
 * This routine frees the whole tree
 */
static inline void NdmpFhdbFreeTree(N_TREE_ROOT* root)
{
  struct ndmp_fhdb_mem *mem, *rel;
  uint32_t freed_blocks = 0;

  for (mem = root->mem; mem;) {
    rel = mem;
    mem = mem->next;
    free(rel);
    freed_blocks++;
  }

  Dmsg3(100, "Total size=%u blocks=%u freed_blocks=%u\n", root->total_size,
        root->blocks, freed_blocks);

  free(root);
  GarbageCollectMemory();

  return;
}

static int NodeCompareByName(void* item1, void* item2)
{
  N_TREE_NODE* tn1 = (N_TREE_NODE*)item1;
  N_TREE_NODE* tn2 = (N_TREE_NODE*)item2;

  if (tn1->fname[0] > tn2->fname[0]) {
    return 1;
  } else if (tn1->fname[0] < tn2->fname[0]) {
    return -1;
  }
  return strcmp(tn1->fname, tn2->fname);
}

static int NodeCompareById(void* item1, void* item2)
{
  N_TREE_NODE* tn1 = (N_TREE_NODE*)item1;
  N_TREE_NODE* tn2 = (N_TREE_NODE*)item2;

  if (tn1->inode > tn2->inode) {
    return 1;
  } else if (tn1->inode < tn2->inode) {
    return -1;
  } else {
    return 0;
  }
}

static N_TREE_NODE* search_and_insert_tree_node(char* fname,
                                                int32_t FileIndex,
                                                uint64_t inode,
                                                N_TREE_ROOT* root,
                                                N_TREE_NODE* parent)
{
  N_TREE_NODE *node, *found_node;

  node = ndmp_fhdb_new_tree_node(root);
  if (inode) {
    node->inode = inode;
    found_node = (N_TREE_NODE*)parent->child.insert(node, NodeCompareById);
  } else {
    node->fname = fname;
    found_node = (N_TREE_NODE*)parent->child.insert(node, NodeCompareByName);
  }

  /*
   * Already in list ?
   */
  if (found_node != node) {
    /*
     * Free node allocated above.
     */
    NdmpFhdbFreeTreeNode(root);
    return found_node;
  }

  /*
   * Its was not found, but now inserted.
   */
  node->parent = parent;
  node->FileIndex = FileIndex;
  node->fname_len = strlen(fname);
  /*
   * Allocate a new entry with 2 bytes extra e.g. the extra slash
   * needed for directories and the \0.
   */
  node->fname = ndmp_fhdb_tree_alloc(root, node->fname_len + 2);
  bstrncpy(node->fname, fname, node->fname_len + 1);

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

static N_TREE_NODE* search_and_insert_tree_node(N_TREE_NODE* node,
                                                N_TREE_ROOT* root,
                                                N_TREE_NODE* parent)
{
  N_TREE_NODE* found_node;

  /*
   * Insert the node into the right parent. We should always insert
   * this node and never get back a found_node that is not the same
   * as the original node but if we do we better return as then there
   * is nothing todo.
   */
  found_node = (N_TREE_NODE*)parent->child.insert(node, NodeCompareById);
  if (found_node != node) { return found_node; }

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
static N_TREE_NODE* find_tree_node(N_TREE_NODE* node, uint64_t inode)
{
  N_TREE_NODE match_node;
  N_TREE_NODE *found_node, *walker;

  match_node.inode = inode;

  /*
   * Start searching in the children of this node.
   */
  found_node = (N_TREE_NODE*)node->child.search(&match_node, NodeCompareById);
  if (found_node) { return found_node; }

  /*
   * The node we are searching for is not one of the top nodes so need to search
   * deeper.
   */
  foreach_rblist (walker, &node->child) {
    /*
     * See if the node has any children otherwise no need to search it.
     */
    if (walker->child.empty()) { continue; }

    found_node = find_tree_node(walker, inode);
    if (found_node) { return found_node; }
  }

  return (N_TREE_NODE*)NULL;
}

/*
 * Recursively search the tree for a certain inode number.
 */
static N_TREE_NODE* find_tree_node(N_TREE_ROOT* root, uint64_t inode)
{
  N_TREE_NODE match_node;
  N_TREE_NODE *found_node, *walker;

  /*
   * See if this is a request for the root of the tree.
   */
  if (root->inode == inode) { return (N_TREE_NODE*)root; }

  match_node.inode = inode;

  /*
   * First do the easy lookup e.g. is this inode part of the parent of the
   * current parent.
   */
  if (root->cached_parent && root->cached_parent->parent) {
    found_node = (N_TREE_NODE*)root->cached_parent->parent->child.search(
        &match_node, NodeCompareById);
    if (found_node) { return found_node; }
  }

  /*
   * Start searching from the root node.
   */
  found_node = (N_TREE_NODE*)root->child.search(&match_node, NodeCompareById);
  if (found_node) { return found_node; }

  /*
   * The node we are searching for is not one of the top nodes so need to search
   * deeper.
   */
  foreach_rblist (walker, &root->child) {
    /*
     * See if the node has any children otherwise no need to search it.
     */
    if (walker->child.empty()) { continue; }

    found_node = find_tree_node(walker, inode);
    if (found_node) { return found_node; }
  }

  return (N_TREE_NODE*)NULL;
}


/*
 * This inserts a piece of meta data we receive out or order in a hash table
 * for later processing. Most NDMP DMAs send things kind of in order some do not
 * and for those we have this workaround.
 */
static inline void add_out_of_order_metadata(NIS* nis,
                                             N_TREE_ROOT* fhdb_root,
                                             const char* raw_name,
                                             ndmp9_u_quad dir_node,
                                             ndmp9_u_quad node)
{
  N_TREE_NODE* nt_node;
  OOO_MD* md_entry = NULL;
  htable* meta_data =
      ((struct fhdb_state*)nis->fhdb_state)->out_of_order_metadata;

  nt_node = ndmp_fhdb_new_tree_node(fhdb_root);
  nt_node->inode = node;
  nt_node->FileIndex = fhdb_root->FileIndex;
  nt_node->fname_len = strlen(raw_name);
  /*
   * Allocate a new entry with 2 bytes extra e.g. the extra slash
   * needed for directories and the \0.
   */
  nt_node->fname = ndmp_fhdb_tree_alloc(fhdb_root, nt_node->fname_len + 2);
  bstrncpy(nt_node->fname, raw_name, nt_node->fname_len + 1);

  /*
   * See if we already allocated the htable.
   */
  if (!meta_data) {
    uint32_t nr_pages, nr_items, item_size;

    nr_pages = MIN_PAGES;
    item_size = sizeof(OOO_MD);
    nr_items = (nr_pages * B_PAGE_SIZE) / item_size;

    meta_data = (htable*)malloc(sizeof(htable));
    meta_data->init(md_entry, &md_entry->link, nr_items, nr_pages);
    ((struct fhdb_state*)nis->fhdb_state)->out_of_order_metadata = meta_data;
  }

  /*
   * Create a new entry and insert it into the hash with the node number as key.
   */
  md_entry = (OOO_MD*)meta_data->hash_malloc(sizeof(OOO_MD));
  md_entry->dir_node = dir_node;
  md_entry->nt_node = nt_node;

  meta_data->insert((uint64_t)node, (void*)md_entry);

  Dmsg2(100,
        "bndmp_fhdb_mem_add_dir: Added out of order metadata entry for node "
        "%llu with parent %llu\n",
        node, dir_node);
}

extern "C" int bndmp_fhdb_mem_add_dir(struct ndmlog* ixlog,
                                      int tagc,
                                      char* raw_name,
                                      ndmp9_u_quad dir_node,
                                      ndmp9_u_quad node)
{
  NIS* nis = (NIS*)ixlog->ctx;

  /*
   * Ignore . and .. directory entries.
   */
  if (bstrcmp(raw_name, ".") || bstrcmp(raw_name, "..")) { return 0; }

  if (nis->save_filehist) {
    N_TREE_ROOT* fhdb_root;

    Dmsg3(100, "bndmp_fhdb_mem_add_dir: New filename ==> %s [%llu] - [%llu]\n",
          raw_name, dir_node, node);

    fhdb_root = ((struct fhdb_state*)nis->fhdb_state)->fhdb_root;
    if (!fhdb_root) {
      Jmsg(nis->jcr, M_FATAL, 0,
           _("NDMP protocol error, FHDB add_dir call before "
             "add_dirnode_root.\n"));
      return 1;
    }

    /*
     * See if this entry is in the cached parent.
     */
    if (fhdb_root->cached_parent &&
        fhdb_root->cached_parent->inode == dir_node) {
      search_and_insert_tree_node(raw_name, fhdb_root->FileIndex, node,
                                  fhdb_root, fhdb_root->cached_parent);
    } else {
      /*
       * Not the cached parent search the tree where it need to be put.
       */
      fhdb_root->cached_parent = find_tree_node(fhdb_root, dir_node);
      if (fhdb_root->cached_parent) {
        search_and_insert_tree_node(raw_name, fhdb_root->FileIndex, node,
                                    fhdb_root, fhdb_root->cached_parent);
      } else {
        add_out_of_order_metadata(nis, fhdb_root, raw_name, dir_node, node);
      }
    }
  }

  return 0;
}

/*
 * This tries recursivly to add the missing parents to the tree.
 */
static N_TREE_NODE* insert_metadata_parent_node(htable* meta_data,
                                                N_TREE_ROOT* fhdb_root,
                                                uint64_t dir_node)
{
  N_TREE_NODE* parent;
  OOO_MD* md_entry;

  Dmsg1(100,
        "bndmp_fhdb_mem_add_dir: Inserting node for parent %llu into tree\n",
        dir_node);

  /*
   * lookup the dir_node
   */
  md_entry = (OOO_MD*)meta_data->lookup(dir_node);
  if (!md_entry || !md_entry->nt_node) {
    /*
     * If we got called the parent node is not in the current tree if we
     * also cannot find it in the metadata things are inconsistent so give up.
     */
    return (N_TREE_NODE*)NULL;
  }

  /*
   * Lookup the parent of this new node we are about to insert.
   */
  parent = find_tree_node(fhdb_root, md_entry->dir_node);
  if (!parent) {
    /*
     * If our parent doesn't exist try finding it and inserting it.
     */
    parent =
        insert_metadata_parent_node(meta_data, fhdb_root, md_entry->dir_node);
    if (!parent) {
      /*
       * If by recursive calling insert_metadata_parent_node we cannot create
       * linked set of parent nodes our metadata is really inconsistent so give
       * up.
       */
      return (N_TREE_NODE*)NULL;
    }
  }

  /*
   * Now we have a working parent in the current tree so we can add the this
   * parent node.
   */
  parent = search_and_insert_tree_node(md_entry->nt_node, fhdb_root, parent);

  /*
   * Keep track we used this entry.
   */
  md_entry->nt_node = (N_TREE_NODE*)NULL;

  return parent;
}

/*
 * This processes all saved out of order metadata and adds these entries to the
 * tree. Only used for NDMP DMAs which are sending their metadata fully at
 * random.
 */
static inline bool ProcessOutOfOrderMetadata(htable* meta_data,
                                             N_TREE_ROOT* fhdb_root)
{
  OOO_MD* md_entry;

  foreach_htable (md_entry, meta_data) {
    /*
     * Alread visited ?
     */
    if (!md_entry->nt_node) { continue; }

    Dmsg1(100, "bndmp_fhdb_mem_add_dir: Inserting node for %llu into tree\n",
          md_entry->nt_node->inode);

    /*
     * See if this entry is in the cached parent.
     */
    if (fhdb_root->cached_parent &&
        fhdb_root->cached_parent->inode == md_entry->dir_node) {
      search_and_insert_tree_node(md_entry->nt_node, fhdb_root,
                                  fhdb_root->cached_parent);
    } else {
      /*
       * See if parent exists in tree.
       */
      fhdb_root->cached_parent = find_tree_node(fhdb_root, md_entry->dir_node);
      if (fhdb_root->cached_parent) {
        search_and_insert_tree_node(md_entry->nt_node, fhdb_root,
                                    fhdb_root->cached_parent);
      } else {
        fhdb_root->cached_parent = insert_metadata_parent_node(
            meta_data, fhdb_root, md_entry->dir_node);
        if (!fhdb_root->cached_parent) {
          /*
           * The metadata seems to be fully inconsistent.
           */
          Dmsg0(100,
                "bndmp_fhdb_mem_add_dir: Inconsistent metadata, giving up\n");
          return false;
        }

        search_and_insert_tree_node(md_entry->nt_node, fhdb_root,
                                    fhdb_root->cached_parent);
      }
    }
  }

  return true;
}

extern "C" int bndmp_fhdb_mem_add_node(struct ndmlog* ixlog,
                                       int tagc,
                                       ndmp9_u_quad node,
                                       ndmp9_file_stat* fstat)
{
  NIS* nis = (NIS*)ixlog->ctx;

  nis->jcr->lock();
  nis->jcr->JobFiles++;
  nis->jcr->unlock();

  if (nis->save_filehist) {
    int attr_size;
    int8_t FileType = 0;
    N_TREE_ROOT* fhdb_root;
    N_TREE_NODE* wanted_node;
    PoolMem attribs(PM_FNAME);
    htable* meta_data =
        ((struct fhdb_state*)nis->fhdb_state)->out_of_order_metadata;

    Dmsg1(100, "bndmp_fhdb_mem_add_node: New node [%llu]\n", node);

    fhdb_root = ((struct fhdb_state*)nis->fhdb_state)->fhdb_root;
    if (!fhdb_root) {
      Jmsg(nis->jcr, M_FATAL, 0,
           _("NDMP protocol error, FHDB add_node call before add_dir.\n"));
      return 1;
    }

    if (meta_data) {
      if (!ProcessOutOfOrderMetadata(meta_data, fhdb_root)) {
        Jmsg(nis->jcr, M_FATAL, 0,
             _("NDMP protocol error, FHDB unable to process out of order "
               "metadata.\n"));
        meta_data->destroy();
        free(meta_data);
        ((struct fhdb_state*)nis->fhdb_state)->out_of_order_metadata = NULL;
        return 1;
      }

      meta_data->destroy();
      free(meta_data);
      ((struct fhdb_state*)nis->fhdb_state)->out_of_order_metadata = NULL;
    }

    wanted_node = find_tree_node(fhdb_root, node);
    if (!wanted_node) {
      Jmsg(nis->jcr, M_FATAL, 0,
           _("NDMP protocol error, FHDB add_node request for unknown node "
             "%llu.\n"),
           node);
      return 1;
    }

    NdmpConvertFstat(fstat, nis->FileIndex, &FileType, attribs);
    attr_size = strlen(attribs.c_str()) + 1;

    wanted_node->attr = ndmp_fhdb_tree_alloc(fhdb_root, attr_size);
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

extern "C" int bndmp_fhdb_mem_add_dirnode_root(struct ndmlog* ixlog,
                                               int tagc,
                                               ndmp9_u_quad root_node)
{
  NIS* nis = (NIS*)ixlog->ctx;

  if (nis->save_filehist) {
    N_TREE_ROOT* fhdb_root;
    struct fhdb_state* fhdb_state;

    Dmsg1(100, "bndmp_fhdb_mem_add_dirnode_root: New root node [%llu]\n",
          root_node);

    fhdb_state = ((struct fhdb_state*)nis->fhdb_state);
    fhdb_root = fhdb_state->fhdb_root;
    if (fhdb_root) {
      Jmsg(nis->jcr, M_FATAL, 0,
           _("NDMP protocol error, FHDB add_dirnode_root call more then "
             "once.\n"));
      return 1;
    }

    fhdb_state->fhdb_root = ndmp_fhdb_new_tree();

    fhdb_root = fhdb_state->fhdb_root;
    fhdb_root->inode = root_node;
    fhdb_root->FileIndex = nis->FileIndex;
    fhdb_root->fname_len = strlen(nis->filesystem);
    /*
     * Allocate a new entry with 2 bytes extra e.g. the extra slash
     * needed for directories and the \0.
     */
    fhdb_root->fname =
        ndmp_fhdb_tree_alloc(fhdb_root, fhdb_root->fname_len + 2);
    bstrncpy(fhdb_root->fname, nis->filesystem, fhdb_root->fname_len + 1);

    fhdb_root->cached_parent = (N_TREE_NODE*)fhdb_root;
  }

  return 0;
}

/*
 * This glues the NDMP File Handle DB with internal code.
 */
void NdmpFhdbMemRegister(struct ndmlog* ixlog)
{
  NIS* nis = (NIS*)ixlog->ctx;
  struct ndm_fhdb_callbacks fhdb_callbacks;

  /*
   * Register the FileHandleDB callbacks.
   */
  fhdb_callbacks.add_file = BndmpFhdbAddFile;
  fhdb_callbacks.add_dir = bndmp_fhdb_mem_add_dir;
  fhdb_callbacks.add_node = bndmp_fhdb_mem_add_node;
  fhdb_callbacks.add_dirnode_root = bndmp_fhdb_mem_add_dirnode_root;
  ndmfhdb_register_callbacks(ixlog, &fhdb_callbacks);

  nis->fhdb_state = malloc(sizeof(struct fhdb_state));
  memset(nis->fhdb_state, 0, sizeof(struct fhdb_state));
}

void NdmpFhdbMemUnregister(struct ndmlog* ixlog)
{
  NIS* nis = (NIS*)ixlog->ctx;
  if (nis && nis->fhdb_state) {
    N_TREE_ROOT* fhdb_root;

    fhdb_root = ((struct fhdb_state*)nis->fhdb_state)->fhdb_root;
    if (fhdb_root) { NdmpFhdbFreeTree(fhdb_root); }

    free(nis->fhdb_state);
    nis->fhdb_state = NULL;
  }

  ndmfhdb_unregister_callbacks(ixlog);
}

void NdmpFhdbMemProcessDb(struct ndmlog* ixlog)
{
  N_TREE_ROOT* fhdb_root;
  NIS* nis = (NIS*)ixlog->ctx;
  struct fhdb_state* fhdb_state;

  fhdb_state = ((struct fhdb_state*)nis->fhdb_state);
  fhdb_root = fhdb_state->fhdb_root;
  if (fhdb_root) {
    if (nis->jcr->ar) {
      N_TREE_NODE *node, *parent;
      PoolMem fname, tmp;

      /*
       * Store the toplevel entry of the tree.
       */
      Dmsg2(100, "==> %s [%s]\n", fhdb_root->fname, fhdb_root->attr);
      NdmpStoreAttributeRecord(
          nis->jcr, fhdb_root->fname, nis->virtual_filename, fhdb_root->attr,
          fhdb_root->FileType, fhdb_root->inode, fhdb_root->Offset);

      /*
       * Store all the other entries in the tree.
       */
      for (node = fhdb_root->first; node; node = node->next) {
        PmStrcpy(fname, node->fname);

        /*
         * Walk up the parent until we hit the head of the list.
         * As directories are store including there trailing slash we
         * can just concatenate the two parts.
         */
        for (parent = node->parent; parent; parent = parent->parent) {
          PmStrcpy(tmp, fname.c_str());
          PmStrcpy(fname, parent->fname);
          PmStrcat(fname, tmp.c_str());
        }

        /*
         * Now we have the full pathname of the file in fname.
         * Store the entry as a hardlinked entry to the original NDMP archive.
         *
         * Handling of incremental/differential backups:
         * During incremental backups, NDMP4_FH_ADD_DIR is sent for ALL files
         * Only for files being backed up, we also get NDMP4_FH_ADD_NODE
         * So we skip entries that do not have any attribute
         */
        if (node->attr) {
          Dmsg2(100, "==> %s [%s]\n", fname.c_str(), node->attr);
          NdmpStoreAttributeRecord(nis->jcr, fname.c_str(),
                                   nis->virtual_filename, node->attr,
                                   node->FileType, node->inode, node->Offset);
        } else {
          Dmsg1(100, "Skipping %s because it has no attributes\n",
                fname.c_str());
        }
      }
    }

    /*
     * Destroy the tree.
     */
    NdmpFhdbFreeTree(fhdb_root);
    fhdb_state->fhdb_root = NULL;
  }
}

#endif /* #if HAVE_NDMP */
} /* namespace directordaemon */
