/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2023 Bareos GmbH & Co. KG

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
// Kern Sibbald, June MMII
/**
 * @file
 * Directory tree build/traverse routines
 */

#ifndef BAREOS_LIB_TREE_H_
#define BAREOS_LIB_TREE_H_

#include "lib/htable.h"
#include "lib/rblist.h"

#include "include/config.h"

struct s_mem {
  struct s_mem* next; /* next buffer */
  int rem;            /* remaining bytes */
  void* mem;          /* memory pointer */
  char first[1];      /* first byte */
};

#define USE_DLIST

#define foreach_child(var, list) \
  for ((var) = NULL;             \
       (*((TREE_NODE**)&(var)) = (TREE_NODE*)(list->child.next(var)));)

#define TreeNodeHasChild(node) ((node)->child.size() > 0)

#define first_child(node) \
        ((TREE_NODE *)(node->child.first())

struct delta_list {
  delta_list* next;
  JobId_t JobId;
  int32_t FileIndex;
};

/* type values */
enum class TreeNodeType
{
  UNKNOWN = 0,
  ROOT,     // root node
  NEWDIR,   // created directory to fill path
  DIR,      // directory entry
  DIR_NLS,  // directory -- no leading slash -- win32
  FILE,     // file entry
};

/**
 * Keep this node as small as possible because
 *   there is one for each file.
 */
struct s_tree_node {
  s_tree_node()
      : type{false}
      , extract{false}
      , extract_dir{false}
      , hard_link{false}
      , soft_link{false}
      , inserted{false}
      , loaded{false}
  {
  }
  /* KEEP sibling as the first member to avoid having to
   *  do initialization of child */
  rblink sibling;
  rblist child;
  char* fname{};                            /* file name */
  int32_t FileIndex{};                      /* file index */
  uint32_t JobId{};                         /* JobId */
  int32_t delta_seq{};                      /* current delta sequence */
  uint16_t fname_len{};                     /* filename length */
  TreeNodeType type{TreeNodeType::UNKNOWN}; /* node type */
  unsigned int extract : 1;                 /* extract item */
  unsigned int extract_dir : 1;             /* extract dir entry only */
  unsigned int hard_link : 1;               /* set if have hard link */
  unsigned int soft_link : 1;               /* set if is soft link */
  unsigned int inserted : 1;                /* set when node newly inserted */
  unsigned int loaded : 1; /* set when the dir is in the tree */
  struct s_tree_node* parent{};
  struct s_tree_node* next{};      /* next hash of FileIndex */
  struct delta_list* delta_list{}; /* delta parts for this node */
  uint64_t fhinfo{};               /* NDMP Fh_info */
  uint64_t fhnode{};               /* NDMP Fh_node */
};
typedef struct s_tree_node TREE_NODE;

/* hardlink hashtable entry */
struct s_hl_entry {
  uint64_t key;
  hlink link;
  TREE_NODE* node;
};
typedef struct s_hl_entry HL_ENTRY;

using HardlinkTable = htable<uint64_t, HL_ENTRY, MonotonicBuffer::Size::Small>;

struct s_tree_root {
  s_tree_root()
      : type{false}
      , extract{false}
      , extract_dir{false}
      , have_link{false}
      , inserted{false}
      , loaded{false}
  {
  }
  /* KEEP sibling as the first member to avoid having to
   *  do initialization of child */
  rblink sibling{};
  rblist child;
  const char* fname{};                      /* file name */
  int32_t FileIndex{};                      /* file index */
  uint32_t JobId{};                         /* JobId */
  int32_t delta_seq{};                      /* current delta sequence */
  uint16_t fname_len{};                     /* filename length */
  TreeNodeType type{TreeNodeType::UNKNOWN}; /* node type */
  unsigned int extract : 1;                 /* extract item */
  unsigned int extract_dir : 1;             /* extract dir entry only */
  unsigned int have_link : 1;               /* set if have hard link */
  unsigned int inserted : 1;                /* set when newly inserted */
  unsigned int loaded : 1; /* set when the dir is in the tree */
  struct s_tree_node* parent{};
  struct s_tree_node* next{};      /* next hash of FileIndex */
  struct delta_list* delta_list{}; /* delta parts for this node */

  /* The above ^^^ must be identical to a TREE_NODE structure */
  struct s_tree_node* first{}; /* first entry in the tree */
  struct s_tree_node* last{};  /* last entry in tree */
  struct s_mem* mem{};         /* tree memory */
  uint32_t total_size{};       /* total bytes allocated */
  uint32_t blocks{};           /* total mallocs */
  int cached_path_len{};       /* length of cached path */
  char* cached_path{};         /* cached current path */
  TREE_NODE* cached_parent{};  /* cached parent for above path */
  HardlinkTable hardlinks;     /* references to first occurence of hardlinks */
};
typedef struct s_tree_root TREE_ROOT;

/* External interface */
TREE_ROOT* new_tree(int count);
TREE_NODE* insert_tree_node(char* path,
                            char* fname,
                            TREE_ROOT* root,
                            TREE_NODE* parent);
TREE_NODE* make_tree_path(char* path, TREE_ROOT* root);
TREE_NODE* tree_cwd(char* path, TREE_ROOT* root, TREE_NODE* node);
TREE_NODE* tree_relcwd(char* path, TREE_ROOT* root, TREE_NODE* node);
void TreeAddDeltaPart(TREE_ROOT* root,
                      TREE_NODE* node,
                      JobId_t JobId,
                      int32_t FileIndex);
void FreeTree(TREE_ROOT* root);
POOLMEM* tree_getpath(TREE_NODE* node);
void TreeRemoveNode(TREE_ROOT* root, TREE_NODE* node);

/**
 * Use the following for traversing the whole tree. It will be
 *   traversed in the order the entries were inserted into the
 *   tree.
 */
#define FirstTreeNode(r) (r)->first
#define NextTreeNode(n) (n)->next

#endif  // BAREOS_LIB_TREE_H_
