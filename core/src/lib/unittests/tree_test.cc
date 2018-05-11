/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2017 Bareos GmbH & Co. KG

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
/* originally was Kern Sibbald, June MMII */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for gtest
 *
 * Philipp Storz, November 2017
 */
#include "include/bareos.h"

#define B_PAGE_SIZE 4096
#define MAX_PAGES 2400
void FillDirectoryTree(char *path, TREE_ROOT *root, TREE_NODE *parent);
static uint32_t FileIndex = 0;

/*
 * Simple test program for tree routines
 */
int main(int argc, char *argv[])
{
    TREE_ROOT *root;
    TREE_NODE *node;
    char buf[MAXPATHLEN];

    root = new_tree();
    root->fname = tree_alloc(root, 1);
    *root->fname = 0;
    root->fname_len = 0;

    FillDirectoryTree("/home/user/bareos", root, NULL);

    for (node = FirstTreeNode(root); node; node=NextTreeNode(node)) {
       tree_getpath(node, buf, sizeof(buf));
       Dmsg2(100, "%d: %s\n", node->FileIndex, buf);
    }

    node = (TREE_NODE *)root;
    Pmsg0(000, "doing cd /home/user/bareos/techlogs\n");
    node = tree_cwd("/home/user/bareos/techlogs", root, node);
    if (node) {
       tree_getpath(node, buf, sizeof(buf));
       Dmsg2(100, "findex=%d: cwd=%s\n", node->FileIndex, buf);
    }

    Pmsg0(000, "doing cd /home/user/bareos/src/testprogs\n");
    node = tree_cwd("/home/user/bareos/src/testprogs", root, node);
    if (node) {
       tree_getpath(node, buf, sizeof(buf));
       Dmsg2(100, "findex=%d: cwd=%s\n", node->FileIndex, buf);
    } else {
       Dmsg0(100, "testprogs not found.\n");
    }

    FreeTree((TREE_NODE *)root);

    return 0;
}

void FillDirectoryTree(char *path, TREE_ROOT *root, TREE_NODE *parent)
{
   TREE_NODE *newparent = NULL;
   TREE_NODE *node;
   struct stat statbuf;
   DIR *dp;
   struct dirent *dir;
   char pathbuf[MAXPATHLEN];
   char file[MAXPATHLEN];
   int type;
   int i;

   Dmsg1(100, "FillDirectoryTree: %s\n", path);

   dp = opendir(path);
   if (!dp) {
      return;
   }

   while ((dir = readdir(dp))) {
      if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
         continue;
      }
      bstrncpy(file, dir->d_name, sizeof(file));
      snprintf(pathbuf, MAXPATHLEN-1, "%s/%s", path, file);
      if (lstat(pathbuf, &statbuf) < 0) {
         BErrNo be;
         printf("lstat() failed. ERR=%s\n", be.bstrerror(errno));
         continue;
      }
//      printf("got file=%s, pathbuf=%s\n", file, pathbuf);
      type = TN_FILE;
      if (S_ISLNK(statbuf.st_mode))
         type =  TN_FILE;  /* link */
      else if (S_ISREG(statbuf.st_mode))
         type = TN_FILE;
      else if (S_ISDIR(statbuf.st_mode)) {
         type = TN_DIR;
      } else if (S_ISCHR(statbuf.st_mode))
         type = TN_FILE; /* char dev */
      else if (S_ISBLK(statbuf.st_mode))
         type = TN_FILE; /* block dev */
      else if (S_ISFIFO(statbuf.st_mode))
         type = TN_FILE; /* fifo */
      else if (S_ISSOCK(statbuf.st_mode))
         type = TN_FILE; /* sock */
      else {
         type = TN_FILE;
         printf("Unknown file type: 0x%x\n", statbuf.st_mode);
      }

      Dmsg2(100, "Doing: %d %s\n", type, pathbuf);
      node = new_tree_node(root);
      node->FileIndex = ++FileIndex;
      parent = insert_tree_node(pathbuf, node, root, parent);

      if (S_ISDIR(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) {
         Dmsg2(100, "calling fill. pathbuf=%s, file=%s\n", pathbuf, file);
         FillDirectoryTree(pathbuf, root, node);
      }
   }
   closedir(dp);
}

#ifndef MAXPATHLEN
#define MAXPATHLEN 2000
#endif

void PrintTree(char *path, TREE_NODE *tree)
{
   char buf[MAXPATHLEN];
   char *termchr;

   if (!tree) {
      return;
   }

   switch (tree->type) {
   case TN_DIR_NLS:
   case TN_DIR:
   case TN_NEWDIR:
      termchr = "/";
      break;
   case TN_ROOT:
   case TN_FILE:
   default:
      termchr = "";
      break;
   }

   Pmsg3(-1, "%s/%s%s\n", path, tree->fname, termchr);

   switch (tree->type) {
   case TN_FILE:
   case TN_NEWDIR:
   case TN_DIR:
   case TN_DIR_NLS:
      Bsnprintf(buf, sizeof(buf), "%s/%s", path, tree->fname);
      PrintTree(buf, first_child(tree));
      break;
   case TN_ROOT:
      PrintTree(path, first_child(tree));
      break;
   default:
      Pmsg1(000, "Unknown node type %d\n", tree->type);
   }

   PrintTree(path, tree->sibling_);

   return;
}
