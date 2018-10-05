/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, July MMII
 */
/**
 * @file
 * User Agent Database File tree for Restore
 *                    command. This file interacts with the user implementing the
 *                    UA tree commands.
 *
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#ifdef BAREOS_LIB_LIB_H_
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif
#include "findlib/find.h"
#include "dird/ua_input.h"
#include "dird/ua_server.h"
#include "lib/edit.h"

namespace directordaemon {

/* Forward referenced commands */
static int markcmd(UaContext *ua, TreeContext *tree);
static int Markdircmd(UaContext *ua, TreeContext *tree);
static int countcmd(UaContext *ua, TreeContext *tree);
static int findcmd(UaContext *ua, TreeContext *tree);
static int lscmd(UaContext *ua, TreeContext *tree);
static int Lsmarkcmd(UaContext *ua, TreeContext *tree);
static int dircmd(UaContext *ua, TreeContext *tree);
static int DotDircmd(UaContext *ua, TreeContext *tree);
static int Estimatecmd(UaContext *ua, TreeContext *tree);
static int HelpCmd(UaContext *ua, TreeContext *tree);
static int cdcmd(UaContext *ua, TreeContext *tree);
static int pwdcmd(UaContext *ua, TreeContext *tree);
static int DotPwdcmd(UaContext *ua, TreeContext *tree);
static int Unmarkcmd(UaContext *ua, TreeContext *tree);
static int UnMarkdircmd(UaContext *ua, TreeContext *tree);
static int QuitCmd(UaContext *ua, TreeContext *tree);
static int donecmd(UaContext *ua, TreeContext *tree);
static int DotLsdircmd(UaContext *ua, TreeContext *tree);
static int DotLscmd(UaContext *ua, TreeContext *tree);
static int DotHelpcmd(UaContext *ua, TreeContext *tree);
static int DotLsmarkcmd(UaContext *ua, TreeContext *tree);

struct cmdstruct {
   const char *key;
   int (*func)(UaContext *ua, TreeContext *tree);
   const char *help;
};
static struct cmdstruct commands[] = {
   { NT_("abort"), QuitCmd, _("abort and do not do restore") },
   { NT_("add"), markcmd, _("add dir/file to be restored recursively, wildcards allowed") },
   { NT_("cd"), cdcmd, _("change current directory") },
   { NT_("count"), countcmd, _("count marked files in and below the cd") },
   { NT_("delete"), Unmarkcmd, _("delete dir/file to be restored recursively in dir") },
   { NT_("dir"), dircmd, _("long list current directory, wildcards allowed") },
   { NT_(".dir"), DotDircmd, _("long list current directory, wildcards allowed") },
   { NT_("done"), donecmd, _("leave file selection mode") },
   { NT_("estimate"), Estimatecmd, _("estimate restore size") },
   { NT_("exit"), donecmd, _("same as done command") },
   { NT_("find"), findcmd, _("find files, wildcards allowed") },
   { NT_("help"), HelpCmd, _("print help") },
   { NT_("ls"), lscmd, _("list current directory, wildcards allowed") },
   { NT_(".ls"), DotLscmd, _("list current directory, wildcards allowed") },
   { NT_(".lsdir"), DotLsdircmd, _("list subdir in current directory, wildcards allowed") },
   { NT_("lsmark"), Lsmarkcmd, _("list the marked files in and below the cd") },
   { NT_(".lsmark"), DotLsmarkcmd,_("list the marked files in") },
   { NT_("mark"), markcmd, _("mark dir/file to be restored recursively, wildcards allowed") },
   { NT_("markdir"), Markdircmd, _("mark directory name to be restored (no files)") },
   { NT_("pwd"), pwdcmd, _("print current working directory") },
   { NT_(".pwd"), DotPwdcmd, _("print current working directory") },
   { NT_("unmark"), Unmarkcmd, _("unmark dir/file to be restored recursively in dir") },
   { NT_("unmarkdir"), UnMarkdircmd, _("unmark directory name only no recursion") },
   { NT_("quit"), QuitCmd, _("quit and do not do restore") },
   { NT_(".help"), DotHelpcmd, _("print help") },
   { NT_("?"), HelpCmd, _("print help") },
};
#define comsize ((int)(sizeof(commands)/sizeof(struct cmdstruct)))

/**
 * Enter a prompt mode where the user can select/deselect
 * files to be restored. This is sort of like a mini-shell
 * that allows "cd", "pwd", "add", "rm", ...
 */
bool UserSelectFilesFromTree(TreeContext *tree)
{
   POOLMEM *cwd;
   UaContext *ua;
   BareosSocket *user;
   bool status;

   /*
    * Get a new context so we don't destroy restore command args
    */
   ua = new_ua_context(tree->ua->jcr);
   ua->UA_sock = tree->ua->UA_sock;   /* patch in UA socket */
   ua->api = tree->ua->api;           /* keep API flag too */
   user = ua->UA_sock;

   ua->SendMsg(_("\nYou are now entering file selection mode where you add (mark) and\n"
                  "remove (unmark) files to be restored. No files are initially added, unless\n"
                  "you used the \"all\" keyword on the command line.\n"
                  "Enter \"done\" to leave this mode.\n\n"));
   user->signal(BNET_START_RTREE);

   /*
    * Enter interactive command handler allowing selection of individual files.
    */
   tree->node = (TREE_NODE *)tree->root;
   cwd = tree_getpath(tree->node);
   if (cwd) {
      ua->SendMsg(_("cwd is: %s\n"), cwd);
      FreePoolMemory(cwd);
   }

   while (1) {
      int found, len, i;
      if (!GetCmd(ua, "$ ", true)) {
         break;
      }

      if (ua->api) {
         user->signal(BNET_CMD_BEGIN);
      }

      ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
      if (ua->argc == 0) {
         ua->WarningMsg(_("Invalid command \"%s\". Enter \"done\" to exit.\n"), ua->cmd);
         if (ua->api) {
            user->signal(BNET_CMD_FAILED);
         }
         continue;
      }

      found = 0;
      status = false;
      len = strlen(ua->argk[0]);
      for (i = 0; i < comsize; i++) {     /* search for command */
         if (bstrncasecmp(ua->argk[0],  commands[i].key, len)) {
            status = (*commands[i].func)(ua, tree);   /* go execute command */
            found = 1;
            break;
         }
      }

      if (!found) {
         if (*ua->argk[0] == '.') {
            /* Some unknow dot command -- probably .messages, ignore it */
            continue;
         }
         ua->WarningMsg(_("Invalid command \"%s\". Enter \"done\" to exit.\n"), ua->cmd);
         if (ua->api) {
            user->signal(BNET_CMD_FAILED);
         }
         continue;
      }

      if (ua->api) {
         user->signal(BNET_CMD_OK);
      }

      if (!status) {
         break;
      }
   }

   user->signal(BNET_END_RTREE);

   ua->UA_sock = NULL;                /* don't release restore socket */
   status = !ua->quit;
   ua->quit = false;
   FreeUaContext(ua);               /* get rid of temp UA context */

   return status;
}

/**
 * This callback routine is responsible for inserting the
 * items it gets into the directory tree. For each JobId selected
 * this routine is called once for each file. We do not allow
 * duplicate filenames, but instead keep the info from the most
 * recent file entered (i.e. the JobIds are assumed to be sorted)
 *
 * See uar_sel_files in sql_cmds.c for query that calls us.
 * row[0]=Path, row[1]=Filename, row[2]=FileIndex
 * row[3]=JobId row[4]=LStat row[5]=DeltaSeq row[6]=Fhinfo row[7]=Fhnode
 */
int InsertTreeHandler(void *ctx, int num_fields, char **row)
{
   struct stat statp;
   TreeContext *tree = (TreeContext *)ctx;
   TREE_NODE *node;
   int type;
   bool hard_link, ok;
   int FileIndex;
   int32_t delta_seq;
   JobId_t JobId;
   HL_ENTRY *entry = NULL;
   int32_t LinkFI;

   Dmsg4(150, "Path=%s%s FI=%s JobId=%s\n", row[0], row[1],
         row[2], row[3]);
   if (*row[1] == 0) {                 /* no filename => directory */
      if (!IsPathSeparator(*row[0])) { /* Must be Win32 directory */
         type = TN_DIR_NLS;
      } else {
         type = TN_DIR;
      }
   } else {
      type = TN_FILE;
   }
   DecodeStat(row[4], &statp, sizeof(statp), &LinkFI);
   hard_link = (LinkFI != 0);
   node = insert_tree_node(row[0], row[1], type, tree->root, NULL);
   JobId = str_to_int64(row[3]);
   FileIndex = str_to_int64(row[2]);
   delta_seq = str_to_int64(row[5]);
   node->fhinfo = str_to_int64(row[6]);
   node->fhnode = str_to_int64(row[7]);
   Dmsg8(150, "node=0x%p JobId=%s FileIndex=%s Delta=%s node.delta=%d LinkFI=%d, fhinfo=%d, fhnode=%d\n",
         node, row[3], row[2], row[5], node->delta_seq, LinkFI, node->fhinfo, node->fhnode);

   /*
    * TODO: check with hardlinks
    */
   if (delta_seq > 0) {
      if (delta_seq == (node->delta_seq + 1)) {
         TreeAddDeltaPart(tree->root, node, node->JobId, node->FileIndex);

      } else {
         /* File looks to be deleted */
         if (node->delta_seq == -1) { /* just created */
            TreeRemoveNode(tree->root, node);

         } else {
            tree->ua->WarningMsg(_("Something is wrong with the Delta sequence of %s, "
                                    "skipping new parts. Current sequence is %d\n"),
                                  row[1], node->delta_seq);

            Dmsg3(0, "Something is wrong with Delta, skip it "
                  "fname=%s d1=%d d2=%d\n", row[1], node->delta_seq, delta_seq);
         }
         return 0;
      }
   }

   /*
    * - The first time we see a file (node->inserted==true), we accept it.
    * - In the same JobId, we accept only the first copy of a
    *   hard linked file (the others are simply pointers).
    * - In the same JobId, we accept the last copy of any other
    *   file -- in particular directories.
    *
    * All the code to set ok could be condensed to a single
    * line, but it would be even harder to read.
    */
   ok = true;
   if (!node->inserted && JobId == node->JobId) {
      if ((hard_link && FileIndex > node->FileIndex) ||
          (!hard_link && FileIndex < node->FileIndex)) {
         ok = false;
      }
   }
   if (ok) {
      node->hard_link = hard_link;
      node->FileIndex = FileIndex;
      node->JobId = JobId;
      node->type = type;
      node->soft_link = S_ISLNK(statp.st_mode) != 0;
      node->delta_seq = delta_seq;

      if (tree->all) {
         node->extract = true;          /* extract all by default */
         if (type == TN_DIR || type == TN_DIR_NLS) {
            node->extract_dir = true;   /* if dir, extract it */
         }
      }

      /*
       * Insert file having hardlinks into hardlink hashtable.
       */
      if (statp.st_nlink > 1 && type != TN_DIR && type != TN_DIR_NLS) {
         if (!LinkFI) {
            /*
             * First occurence - file hardlinked to
             */
            entry = (HL_ENTRY *)tree->root->hardlinks.hash_malloc(sizeof(HL_ENTRY));
            entry->key = (((uint64_t) JobId) << 32) + FileIndex;
            entry->node = node;
            tree->root->hardlinks.insert(entry->key, entry);
         } else {
            /*
             * See if we are optimizing for speed or size.
             */
            if (!me->optimize_for_size && me->optimize_for_speed) {
               /*
                * Hardlink to known file index: lookup original file
                */
               uint64_t file_key = (((uint64_t) JobId) << 32) + LinkFI;
               HL_ENTRY *first_hl = (HL_ENTRY *) tree->root->hardlinks.lookup(file_key);

               if (first_hl && first_hl->node) {
                  /*
                   * Then add hardlink entry to linked node.
                   */
                  entry = (HL_ENTRY *)tree->root->hardlinks.hash_malloc(sizeof(HL_ENTRY));
                  entry->key = (((uint64_t) JobId) << 32) + FileIndex;
                  entry->node = first_hl->node;
                  tree->root->hardlinks.insert(entry->key, entry);
               }
            }
         }
      }
   }

   if (node->inserted) {
      tree->FileCount++;
      if (tree->DeltaCount > 0 && (tree->FileCount-tree->LastCount) > tree->DeltaCount) {
         tree->ua->SendMsg("+");
         tree->LastCount = tree->FileCount;
      }
   }

   tree->cnt++;
   return 0;
}

/**
 * Set extract to value passed. We recursively walk down the tree setting all children
 * if the node is a directory.
 */
static int SetExtract(UaContext *ua, TREE_NODE *node, TreeContext *tree, bool extract)
{
   TREE_NODE *n;
   int count = 0;

   node->extract = extract;
   if (node->type == TN_DIR || node->type == TN_DIR_NLS) {
      node->extract_dir = extract;    /* set/clear dir too */
   }

   if (node->type != TN_NEWDIR) {
      count++;
   }

   /*
    * For a non-file (i.e. directory), we see all the children
    */
   if (node->type != TN_FILE || (node->soft_link && TreeNodeHasChild(node))) {
      /*
       * Recursive set children within directory
       */
      foreach_child(n, node) {
         count += SetExtract(ua, n, tree, extract);
      }

      /*
       * Walk up tree marking any unextracted parent to be extracted.
       */
      if (extract) {
         while (node->parent && !node->parent->extract_dir) {
            node = node->parent;
            node->extract_dir = true;
         }
      }
   } else {
      if (extract) {
         uint64_t key = 0;
         bool is_hardlinked = false;

         /*
          * See if we are optimizing for speed or size.
          */
         if (!me->optimize_for_size && me->optimize_for_speed) {
            if (node->hard_link) {
               /*
                * Every hardlink is in hashtable, and it points to linked file.
                */
               key = (((uint64_t) node->JobId) << 32) + node->FileIndex;
               is_hardlinked = true;
            }
         } else {
            FileDbRecord fdbr;
            POOLMEM *cwd;

            /*
             * Ordinary file, we get the full path, look up the attributes, decode them,
             * and if we are hard linked to a file that was saved, we must load that file too.
             */
            cwd = tree_getpath(node);
            if (cwd) {
               fdbr.FileId = 0;
               fdbr.JobId = node->JobId;

               if (node->hard_link && ua->db->GetFileAttributesRecord(ua->jcr, cwd, NULL, &fdbr)) {
                  int32_t LinkFI;
                  struct stat statp;

                  DecodeStat(fdbr.LStat, &statp, sizeof(statp), &LinkFI); /* decode stat pkt */
                  key = (((uint64_t) node->JobId) << 32) + LinkFI;  /* lookup by linked file's fileindex */
                  is_hardlinked = true;
               }
               FreePoolMemory(cwd);
            }
         }

         if (is_hardlinked) {
            /*
             * If we point to a hard linked file, find that file in hardlinks hashmap,
             * and mark it to be restored as well.
             */
            HL_ENTRY *entry = (HL_ENTRY *) tree->root->hardlinks.lookup(key);
            if (entry && entry->node) {
               n = entry->node;
               n->extract = true;
               n->extract_dir = (n->type == TN_DIR || n->type == TN_DIR_NLS);
            }
         }
      }
   }

   return count;
}

static void StripTrailingSlash(char *arg)
{
   int len = strlen(arg);
   if (len == 0) {
      return;
   }
   len--;
   if (arg[len] == '/') {       /* strip any trailing slash */
      arg[len] = 0;
   }
}

/**
 * Recursively mark the current directory to be restored as
 *  well as all directories and files below it.
 */
static int markcmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   int count = 0;
   char ec1[50];
   POOLMEM *cwd;
   bool restore_cwd = false;

   if (ua->argc < 2 || !TreeNodeHasChild(tree->node)) {
      ua->SendMsg(_("No files marked.\n"));
      return 1;
   }

   /*
    * Save the current CWD.
    */
   cwd = tree_getpath(tree->node);

   for (int i = 1; i < ua->argc; i++) {
      StripTrailingSlash(ua->argk[i]);

      /*
       * See if this is a full path.
       */
      if (strchr(ua->argk[i], '/')) {
         int pnl, fnl;
         POOLMEM *file = GetPoolMemory(PM_FNAME);
         POOLMEM *path = GetPoolMemory(PM_FNAME);

         /*
          * Split the argument into a path and file part.
          */
         SplitPathAndFilename(ua->argk[i], path, &pnl, file, &fnl);

         /*
          * First change the CWD to the correct PATH.
          */
         node = tree_cwd(path, tree->root, tree->node);
         if (!node) {
            ua->WarningMsg(_("Invalid path %s given.\n"), path);
            FreePoolMemory(file);
            FreePoolMemory(path);
            continue;
         }
         tree->node = node;
         restore_cwd = true;

         foreach_child(node, tree->node) {
            if (fnmatch(file, node->fname, 0) == 0) {
               count += SetExtract(ua, node, tree, true);
            }
         }

         FreePoolMemory(file);
         FreePoolMemory(path);
      } else {
         /*
          * Only a pattern without a / so do things relative to CWD.
          */
         foreach_child(node, tree->node) {
            if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
               count += SetExtract(ua, node, tree, true);
            }
         }
      }
   }

   if (count == 0) {
      ua->SendMsg(_("No files marked.\n"));
   } else if (count == 1) {
      ua->SendMsg(_("1 file marked.\n"));
   } else {
      ua->SendMsg(_("%s files marked.\n"),
                   edit_uint64_with_commas(count, ec1));
   }

   /*
    * Restore the CWD when we changed it.
    */
   if (restore_cwd && cwd) {
      node = tree_cwd(cwd, tree->root, tree->node);
      if (!node) {
         ua->WarningMsg(_("Invalid path %s given.\n"), cwd);
      } else {
         tree->node = node;
      }
   }

   if (cwd) {
      FreePoolMemory(cwd);
   }

   return 1;
}

static int Markdircmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   int count = 0;
   char ec1[50];

   if (ua->argc < 2 || !TreeNodeHasChild(tree->node)) {
      ua->SendMsg(_("No files marked.\n"));
      return 1;
   }
   for (int i = 1; i < ua->argc; i++) {
      StripTrailingSlash(ua->argk[i]);
      foreach_child(node, tree->node) {
         if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
            if (node->type == TN_DIR || node->type == TN_DIR_NLS) {
               node->extract_dir = true;
               count++;
            }
         }
      }
   }
   if (count == 0) {
      ua->SendMsg(_("No directories marked.\n"));
   } else if (count == 1) {
      ua->SendMsg(_("1 directory marked.\n"));
   } else {
      ua->SendMsg(_("%s directories marked.\n"),
               edit_uint64_with_commas(count, ec1));
   }
   return 1;
}

static int countcmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   int total, num_extract;
   char ec1[50], ec2[50];

   total = num_extract = 0;
   for (node = FirstTreeNode(tree->root);
        node;
        node = NextTreeNode(node)) {
      if (node->type != TN_NEWDIR) {
         total++;
         if (node->extract || node->extract_dir) {
            num_extract++;
         }
      }
   }
   ua->SendMsg(_("%s total files/dirs. %s marked to be restored.\n"),
            edit_uint64_with_commas(total, ec1),
            edit_uint64_with_commas(num_extract, ec2));
   return 1;
}

static int findcmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   POOLMEM *cwd;

   if (ua->argc == 1) {
      ua->SendMsg(_("No file specification given.\n"));
      return 1;      /* make it non-fatal */
   }

   for (int i = 1; i < ua->argc; i++) {
      for (node = FirstTreeNode(tree->root);
           node;
           node = NextTreeNode(node)) {
         if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
            const char *tag;

            cwd = tree_getpath(node);
            if (node->extract) {
               tag = "*";
            } else if (node->extract_dir) {
               tag = "+";
            } else {
               tag = "";
            }
            ua->SendMsg("%s%s\n", tag, cwd);
            FreePoolMemory(cwd);
         }
      }
   }
   return 1;
}

static int DotLsdircmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;

   if (!TreeNodeHasChild(tree->node)) {
      return 1;
   }

   foreach_child(node, tree->node) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
         if (TreeNodeHasChild(node)) {
            ua->SendMsg("%s/\n", node->fname);
         }
      }
   }

   return 1;
}

static int DotHelpcmd(UaContext *ua, TreeContext *tree)
{
   for (int i = 0; i < comsize; i++) {
      /* List only non-dot commands */
      if (commands[i].key[0] != '.') {
         ua->SendMsg("%s\n", commands[i].key);
      }
   }
   return 1;
}

static int DotLscmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;

   if (!TreeNodeHasChild(tree->node)) {
      return 1;
   }

   foreach_child(node, tree->node) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
         ua->SendMsg("%s%s\n", node->fname, TreeNodeHasChild(node)?"/":"");
      }
   }

   return 1;
}

static int lscmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;

   if (!TreeNodeHasChild(tree->node)) {
      return 1;
   }
   foreach_child(node, tree->node) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
         const char *tag;
         if (node->extract) {
            tag = "*";
         } else if (node->extract_dir) {
            tag = "+";
         } else {
            tag = "";
         }
         ua->SendMsg("%s%s%s\n", tag, node->fname, TreeNodeHasChild(node)?"/":"");
      }
   }
   return 1;
}

/**
 * Ls command that lists only the marked files
 */
static int DotLsmarkcmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   if (!TreeNodeHasChild(tree->node)) {
      return 1;
   }
   foreach_child(node, tree->node) {
      if ((ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) &&
          (node->extract || node->extract_dir)) {
         ua->SendMsg("%s%s\n", node->fname, TreeNodeHasChild(node)?"/":"");
      }
   }
   return 1;
}

/**
 * This recursive ls command that lists only the marked files
 */
static void rlsmark(UaContext *ua, TREE_NODE *tnode, int level)
{
   TREE_NODE *node;
   const int max_level = 100;
   char indent[max_level*2+1];
   int i, j;

   if (!TreeNodeHasChild(tnode)) {
      return;
   }

   level = MIN(level, max_level);
   j = 0;
   for (i = 0; i < level; i++) {
      indent[j++] = ' ';
      indent[j++] = ' ';
   }
   indent[j] = 0;

   foreach_child(node, tnode) {
      if ((ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) &&
          (node->extract || node->extract_dir)) {
         const char *tag;
         if (node->extract) {
            tag = "*";
         } else if (node->extract_dir) {
            tag = "+";
         } else {
            tag = "";
         }
         ua->SendMsg("%s%s%s%s\n", indent, tag, node->fname, TreeNodeHasChild(node)?"/":"");
         if (TreeNodeHasChild(node)) {
            rlsmark(ua, node, level+1);
         }
      }
   }
}

static int Lsmarkcmd(UaContext *ua, TreeContext *tree)
{
   rlsmark(ua, tree->node, 0);
   return 1;
}

/**
 * This is actually the long form used for "dir"
 */
static inline void ls_output(guid_list *guid, POOLMEM *&buf,
                             const char *fname, const char *tag,
                             struct stat *statp, bool dot_cmd)
{
   char mode_str[11];
   char time_str[22];
   char ec1[30];
   char en1[30], en2[30];

   /*
    * Insert mode e.g. -r-xr-xr-x
    */
   encode_mode(statp->st_mode, mode_str);

   if (dot_cmd) {
      encode_time(statp->st_mtime, time_str);
      Mmsg(buf, "%s,%d,%s,%s,%s,%s,%c,%s",
           mode_str,
           (uint32_t)statp->st_nlink,
           guid->uid_to_name(statp->st_uid, en1, sizeof(en1)),
           guid->gid_to_name(statp->st_gid, en2, sizeof(en2)),
           edit_int64(statp->st_size, ec1),
           time_str,
           *tag,
           fname);
   } else {
      time_t time;

      if (statp->st_ctime > statp->st_mtime) {
         time = statp->st_ctime;
      } else {
         time = statp->st_mtime;
      }

      /*
       * Display most recent time
       */
      encode_time(time, time_str);

      Mmsg(buf, "%s  %2d %-8.8s %-8.8s  %12.12s  %s %c%s",
           mode_str,
           (uint32_t)statp->st_nlink,
           guid->uid_to_name(statp->st_uid, en1, sizeof(en1)),
           guid->gid_to_name(statp->st_gid, en2, sizeof(en2)),
           edit_int64(statp->st_size, ec1),
           time_str,
           *tag,
           fname);
   }
}

/**
 * Like ls command, but give more detail on each file
 */
static int DoDircmd(UaContext *ua, TreeContext *tree, bool dot_cmd)
{
   TREE_NODE *node;
   POOLMEM *cwd, *buf;
   FileDbRecord fdbr;
   struct stat statp;
   char *pcwd;

   if (!TreeNodeHasChild(tree->node)) {
      ua->SendMsg(_("Node %s has no children.\n"), tree->node->fname);
      return 1;
   }

   ua->guid = new_guid_list();
   buf = GetPoolMemory(PM_FNAME);

   foreach_child(node, tree->node) {
      const char *tag;
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
         if (node->extract) {
            tag = "*";
         } else if (node->extract_dir) {
            tag = "+";
         } else {
            tag = " ";
         }

         cwd = tree_getpath(node);

         fdbr.FileId = 0;
         fdbr.JobId = node->JobId;

         /*
          * Strip / from soft links to directories.
          *
          * This is because soft links to files have a trailing slash
          * when returned from tree_getpath, but get_file_attr...
          * treats soft links as files, so they do not have a trailing
          * slash like directory names.
          */
         if (node->type == TN_FILE && TreeNodeHasChild(node)) {
            PmStrcpy(buf, cwd);
            pcwd = buf;
            int len = strlen(buf);
            if (len > 1) {
               buf[len - 1] = '\0'; /* strip trailing / */
            }
         } else {
            pcwd = cwd;
         }

         if (ua->db->GetFileAttributesRecord(ua->jcr, pcwd, NULL, &fdbr)) {
            int32_t LinkFI;
            DecodeStat(fdbr.LStat, &statp, sizeof(statp), &LinkFI); /* decode stat pkt */
         } else {
            /* Something went wrong getting attributes -- print name */
            memset(&statp, 0, sizeof(statp));
         }

         ls_output(ua->guid, buf, cwd, tag, &statp, dot_cmd);
         ua->SendMsg("%s\n", buf);

         FreePoolMemory(cwd);
      }
   }

   FreePoolMemory(buf);

   return 1;
}

int DotDircmd(UaContext *ua, TreeContext *tree)
{
   return DoDircmd(ua, tree, true/*dot command*/);
}

static int dircmd(UaContext *ua, TreeContext *tree)
{
   return DoDircmd(ua, tree, false/*not dot command*/);
}

static int Estimatecmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   POOLMEM *cwd;
   int total, num_extract;
   uint64_t total_bytes = 0;
   FileDbRecord fdbr;
   struct stat statp;
   char ec1[50];

   total = num_extract = 0;
   for (node = FirstTreeNode(tree->root);
        node;
        node = NextTreeNode(node)) {
      if (node->type != TN_NEWDIR) {
         total++;
         if (node->extract && node->type == TN_FILE) {
            /*
             * If regular file, get size
             */
            num_extract++;
            cwd = tree_getpath(node);

            fdbr.FileId = 0;
            fdbr.JobId = node->JobId;

            if (ua->db->GetFileAttributesRecord(ua->jcr, cwd, NULL, &fdbr)) {
               int32_t LinkFI;
               DecodeStat(fdbr.LStat, &statp, sizeof(statp), &LinkFI); /* decode stat pkt */
               if (S_ISREG(statp.st_mode) && statp.st_size > 0) {
                  total_bytes += statp.st_size;
               }
            }

            FreePoolMemory(cwd);
         } else if (node->extract || node->extract_dir) {
            /*
             * Directory, count only
             */
            num_extract++;
         }
      }
   }
   ua->SendMsg(_("%d total files; %d marked to be restored; %s bytes.\n"),
            total, num_extract, edit_uint64_with_commas(total_bytes, ec1));
   return 1;
}

static int HelpCmd(UaContext *ua, TreeContext *tree)
{
   unsigned int i;

   ua->SendMsg(_("  Command    Description\n  =======    ===========\n"));
   for (i = 0; i < comsize; i++) {
      /*
       * List only non-dot commands
       */
      if (commands[i].key[0] != '.') {
         ua->SendMsg("  %-10s %s\n", _(commands[i].key), _(commands[i].help));
      }
   }
   ua->SendMsg("\n");
   return 1;
}

/**
 * Change directories.  Note, if the user specifies x: and it fails,
 * we assume it is a Win32 absolute cd rather than relative and
 * try a second time with /x: ...  Win32 kludge.
 */
static int cdcmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   POOLMEM *cwd;

   if (ua->argc != 2) {
      ua->ErrorMsg(_("Too few or too many arguments. Try using double quotes.\n"));
      return 1;
   }

   node = tree_cwd(ua->argk[1], tree->root, tree->node);
   if (!node) {
      /*
       * Try once more if Win32 drive -- make absolute
       */
      if (ua->argk[1][1] == ':') {  /* win32 drive */
         cwd = GetPoolMemory(PM_FNAME);
         PmStrcpy(cwd, "/");
         PmStrcat(cwd, ua->argk[1]);
         node = tree_cwd(cwd, tree->root, tree->node);
         FreePoolMemory(cwd);
      }

      if (!node) {
         ua->WarningMsg(_("Invalid path given.\n"));
      } else {
         tree->node = node;
      }
   } else {
      tree->node = node;
   }

   return pwdcmd(ua, tree);
}

static int pwdcmd(UaContext *ua, TreeContext *tree)
{
   POOLMEM *cwd;

   cwd = tree_getpath(tree->node);
   if (cwd) {
      if (ua->api) {
         ua->SendMsg("%s", cwd);
      } else {
         ua->SendMsg(_("cwd is: %s\n"), cwd);
      }
      FreePoolMemory(cwd);
   }

   return 1;
}

static int DotPwdcmd(UaContext *ua, TreeContext *tree)
{
   POOLMEM *cwd;

   cwd = tree_getpath(tree->node);
   if (cwd) {
      ua->SendMsg("%s", cwd);
      FreePoolMemory(cwd);
   }

   return 1;
}

static int Unmarkcmd(UaContext *ua, TreeContext *tree)
{
   POOLMEM *cwd;
   TREE_NODE *node;
   int count = 0;
   bool restore_cwd = false;

   if (ua->argc < 2 || !TreeNodeHasChild(tree->node)) {
      ua->SendMsg(_("No files unmarked.\n"));
      return 1;
   }

   /*
    * Save the current CWD.
    */
   cwd = tree_getpath(tree->node);

   for (int i = 1; i < ua->argc; i++) {
      StripTrailingSlash(ua->argk[i]);

      /*
       * See if this is a full path.
       */
      if (strchr(ua->argk[i], '/')) {
         int pnl, fnl;
         POOLMEM *file = GetPoolMemory(PM_FNAME);
         POOLMEM *path = GetPoolMemory(PM_FNAME);

         /*
          * Split the argument into a path and file part.
          */
         SplitPathAndFilename(ua->argk[i], path, &pnl, file, &fnl);

         /*
          * First change the CWD to the correct PATH.
          */
         node = tree_cwd(path, tree->root, tree->node);
         if (!node) {
            ua->WarningMsg(_("Invalid path %s given.\n"), path);
            FreePoolMemory(file);
            FreePoolMemory(path);
            continue;
         }
         tree->node = node;
         restore_cwd = true;

         foreach_child(node, tree->node) {
            if (fnmatch(file, node->fname, 0) == 0) {
               count += SetExtract(ua, node, tree, false);
            }
         }

         FreePoolMemory(file);
         FreePoolMemory(path);
      } else {
         /*
          * Only a pattern without a / so do things relative to CWD.
          */
         foreach_child(node, tree->node) {
            if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
               count += SetExtract(ua, node, tree, false);
            }
         }
      }
   }

   if (count == 0) {
      ua->SendMsg(_("No files unmarked.\n"));
   } else if (count == 1) {
      ua->SendMsg(_("1 file unmarked.\n"));
   } else {
      char ed1[50];
      ua->SendMsg(_("%s files unmarked.\n"), edit_uint64_with_commas(count, ed1));
   }

   /*
    * Restore the CWD when we changed it.
    */
   if (restore_cwd && cwd) {
      node = tree_cwd(cwd, tree->root, tree->node);
      if (!node) {
         ua->WarningMsg(_("Invalid path %s given.\n"), cwd);
      } else {
         tree->node = node;
      }
   }

   if (cwd) {
      FreePoolMemory(cwd);
   }

   return 1;
}

static int UnMarkdircmd(UaContext *ua, TreeContext *tree)
{
   TREE_NODE *node;
   int count = 0;

   if (ua->argc < 2 || !TreeNodeHasChild(tree->node)) {
      ua->SendMsg(_("No directories unmarked.\n"));
      return 1;
   }

   for (int i = 1; i < ua->argc; i++) {
      StripTrailingSlash(ua->argk[i]);
      foreach_child(node, tree->node) {
         if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
            if (node->type == TN_DIR || node->type == TN_DIR_NLS) {
               node->extract_dir = false;
               count++;
            }
         }
      }
   }

   if (count == 0) {
      ua->SendMsg(_("No directories unmarked.\n"));
   } else if (count == 1) {
      ua->SendMsg(_("1 directory unmarked.\n"));
   } else {
      ua->SendMsg(_("%d directories unmarked.\n"), count);
   }
   return 1;
}

static int donecmd(UaContext *ua, TreeContext *tree)
{
   return 0;
}

static int QuitCmd(UaContext *ua, TreeContext *tree)
{
   ua->quit = true;
   return 0;
}
} /* namespace directordaemon */
