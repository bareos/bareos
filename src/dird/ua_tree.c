/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- User Agent Database File tree for Restore
 *      command. This file interacts with the user implementing the
 *      UA tree commands.
 *
 *     Kern Sibbald, July MMII
 *
 */

#include "bacula.h"
#include "dird.h"
#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif
#include "findlib/find.h"


/* Forward referenced commands */

static int markcmd(UAContext *ua, TREE_CTX *tree);
static int markdircmd(UAContext *ua, TREE_CTX *tree);
static int countcmd(UAContext *ua, TREE_CTX *tree);
static int findcmd(UAContext *ua, TREE_CTX *tree);
static int lscmd(UAContext *ua, TREE_CTX *tree);
static int lsmarkcmd(UAContext *ua, TREE_CTX *tree);
static int dircmd(UAContext *ua, TREE_CTX *tree);
static int dot_dircmd(UAContext *ua, TREE_CTX *tree);
static int estimatecmd(UAContext *ua, TREE_CTX *tree);
static int helpcmd(UAContext *ua, TREE_CTX *tree);
static int cdcmd(UAContext *ua, TREE_CTX *tree);
static int pwdcmd(UAContext *ua, TREE_CTX *tree);
static int dot_pwdcmd(UAContext *ua, TREE_CTX *tree);
static int unmarkcmd(UAContext *ua, TREE_CTX *tree);
static int unmarkdircmd(UAContext *ua, TREE_CTX *tree);
static int quitcmd(UAContext *ua, TREE_CTX *tree);
static int donecmd(UAContext *ua, TREE_CTX *tree);
static int dot_lsdircmd(UAContext *ua, TREE_CTX *tree);
static int dot_lscmd(UAContext *ua, TREE_CTX *tree);
static int dot_helpcmd(UAContext *ua, TREE_CTX *tree);
static int dot_lsmarkcmd(UAContext *ua, TREE_CTX *tree);

struct cmdstruct { const char *key; int (*func)(UAContext *ua, TREE_CTX *tree); const char *help; };
static struct cmdstruct commands[] = {
 { NT_("add"),        markcmd,      _("add dir/file to be restored recursively, wildcards allowed")},
 { NT_("cd"),         cdcmd,        _("change current directory")},
 { NT_("count"),      countcmd,     _("count marked files in and below the cd")},
 { NT_("delete"),     unmarkcmd,    _("delete dir/file to be restored recursively in dir")},
 { NT_("dir"),        dircmd,       _("long list current directory, wildcards allowed")},
 { NT_(".dir"),       dot_dircmd,   _("long list current directory, wildcards allowed")},
 { NT_("done"),       donecmd,      _("leave file selection mode")},
 { NT_("estimate"),   estimatecmd,  _("estimate restore size")},
 { NT_("exit"),       donecmd,      _("same as done command")},
 { NT_("find"),       findcmd,      _("find files, wildcards allowed")},
 { NT_("help"),       helpcmd,      _("print help")},
 { NT_("ls"),         lscmd,        _("list current directory, wildcards allowed")},
 { NT_(".ls"),        dot_lscmd,    _("list current directory, wildcards allowed")},
 { NT_(".lsdir"),     dot_lsdircmd, _("list subdir in current directory, wildcards allowed")},
 { NT_("lsmark"),     lsmarkcmd,    _("list the marked files in and below the cd")},
 { NT_(".lsmark"),    dot_lsmarkcmd,_("list the marked files in")},
 { NT_("mark"),       markcmd,      _("mark dir/file to be restored recursively, wildcards allowed")},
 { NT_("markdir"),    markdircmd,   _("mark directory name to be restored (no files)")},
 { NT_("pwd"),        pwdcmd,       _("print current working directory")},
 { NT_(".pwd"),       dot_pwdcmd,   _("print current working directory")},
 { NT_("unmark"),     unmarkcmd,    _("unmark dir/file to be restored recursively in dir")},
 { NT_("unmarkdir"),  unmarkdircmd, _("unmark directory name only no recursion")},
 { NT_("quit"),       quitcmd,      _("quit and do not do restore")},
 { NT_(".help"),      dot_helpcmd,  _("print help")},
 { NT_("?"),          helpcmd,      _("print help")},
             };
#define comsize ((int)(sizeof(commands)/sizeof(struct cmdstruct)))

/*
 * Enter a prompt mode where the user can select/deselect
 *  files to be restored. This is sort of like a mini-shell
 *  that allows "cd", "pwd", "add", "rm", ...
 */
bool user_select_files_from_tree(TREE_CTX *tree)
{
   char cwd[2000];
   bool stat;
   /* Get a new context so we don't destroy restore command args */
   UAContext *ua = new_ua_context(tree->ua->jcr);
   ua->UA_sock = tree->ua->UA_sock;   /* patch in UA socket */
   ua->api = tree->ua->api;           /* keep API flag too */
   BSOCK *user = ua->UA_sock;

   ua->send_msg(_(
      "\nYou are now entering file selection mode where you add (mark) and\n"
      "remove (unmark) files to be restored. No files are initially added, unless\n"
      "you used the \"all\" keyword on the command line.\n"
      "Enter \"done\" to leave this mode.\n\n"));
   if (ua->api) user->signal(BNET_START_RTREE);
   /*
    * Enter interactive command handler allowing selection
    *  of individual files.
    */
   tree->node = (TREE_NODE *)tree->root;
   tree_getpath(tree->node, cwd, sizeof(cwd));
   ua->send_msg(_("cwd is: %s\n"), cwd);
   for ( ;; ) {
      int found, len, i;
      if (!get_cmd(ua, "$ ", true)) {
         break;
      }
      if (ua->api) user->signal(BNET_CMD_BEGIN);
      parse_args_only(ua->cmd, &ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
      if (ua->argc == 0) {
         ua->warning_msg(_("Invalid command \"%s\". Enter \"done\" to exit.\n"), ua->cmd);
         if (ua->api) user->signal(BNET_CMD_FAILED);
         continue;
      }

      len = strlen(ua->argk[0]);
      found = 0;
      stat = false;
      for (i=0; i<comsize; i++)       /* search for command */
         if (strncasecmp(ua->argk[0],  commands[i].key, len) == 0) {
            stat = (*commands[i].func)(ua, tree);   /* go execute command */
            found = 1;
            break;
         }
      if (!found) {
         if (*ua->argk[0] == '.') {
            /* Some unknow dot command -- probably .messages, ignore it */
            continue;
         }
         ua->warning_msg(_("Invalid command \"%s\". Enter \"done\" to exit.\n"), ua->cmd);
         if (ua->api) user->signal(BNET_CMD_FAILED);
         continue;
      }
      if (ua->api) user->signal(BNET_CMD_OK);
      if (!stat) {
         break;
      }
   }
   if (ua->api) user->signal(BNET_END_RTREE);
   ua->UA_sock = NULL;                /* don't release restore socket */
   stat = !ua->quit;
   ua->quit = false;
   free_ua_context(ua);               /* get rid of temp UA context */
   return stat;
}


/*
 * This callback routine is responsible for inserting the
 *  items it gets into the directory tree. For each JobId selected
 *  this routine is called once for each file. We do not allow
 *  duplicate filenames, but instead keep the info from the most
 *  recent file entered (i.e. the JobIds are assumed to be sorted)
 *
 *   See uar_sel_files in sql_cmds.c for query that calls us.
 *      row[0]=Path, row[1]=Filename, row[2]=FileIndex
 *      row[3]=JobId row[4]=LStat row[5]=DeltaSeq
 */
int insert_tree_handler(void *ctx, int num_fields, char **row)
{
   struct stat statp;
   TREE_CTX *tree = (TREE_CTX *)ctx;
   TREE_NODE *node;
   int type;
   bool hard_link, ok;
   int FileIndex;
   int32_t delta_seq;
   JobId_t JobId;

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
   hard_link = (decode_LinkFI(row[4], &statp, sizeof(statp)) != 0);
   node = insert_tree_node(row[0], row[1], type, tree->root, NULL);
   JobId = str_to_int64(row[3]);
   FileIndex = str_to_int64(row[2]);
   delta_seq = str_to_int64(row[5]);
   Dmsg5(150, "node=0x%p JobId=%s FileIndex=%s Delta=%s node.delta=%d\n",
         node, row[3], row[2], row[5], node->delta_seq);

   /* TODO: check with hardlinks */
   if (delta_seq > 0) {
      if (delta_seq == (node->delta_seq + 1)) {
         tree_add_delta_part(tree->root, node, node->JobId, node->FileIndex);

      } else {
         /* File looks to be deleted */
         if (node->delta_seq == -1) { /* just created */
            tree_remove_node(tree->root, node);

         } else {
            tree->ua->warning_msg(_("Something is wrong with the Delta sequence of %s, "
                                    "skiping new parts. Current sequence is %d\n"),
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
    *  line, but it would be even harder to read.
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
   }
   if (node->inserted) {
      tree->FileCount++;
      if (tree->DeltaCount > 0 && (tree->FileCount-tree->LastCount) > tree->DeltaCount) {
         tree->ua->send_msg("+");
         tree->LastCount = tree->FileCount;
      }
   }
   tree->cnt++;
   return 0;
}


/*
 * Set extract to value passed. We recursively walk
 *  down the tree setting all children if the
 *  node is a directory.
 */
static int set_extract(UAContext *ua, TREE_NODE *node, TREE_CTX *tree, bool extract)
{
   TREE_NODE *n;
   FILE_DBR fdbr;
   struct stat statp;
   int count = 0;

   node->extract = extract;
   if (node->type == TN_DIR || node->type == TN_DIR_NLS) {
      node->extract_dir = extract;    /* set/clear dir too */
   }
   if (node->type != TN_NEWDIR) {
      count++;
   }
   /* For a non-file (i.e. directory), we see all the children */
   if (node->type != TN_FILE || (node->soft_link && tree_node_has_child(node))) {
      /* Recursive set children within directory */
      foreach_child(n, node) {
         count += set_extract(ua, n, tree, extract);
      }
      /*
       * Walk up tree marking any unextracted parent to be
       * extracted.
       */
      if (extract) {
         while (node->parent && !node->parent->extract_dir) {
            node = node->parent;
            node->extract_dir = true;
         }
      }
   } else if (extract) {
      char cwd[2000];
      /*
       * Ordinary file, we get the full path, look up the
       * attributes, decode them, and if we are hard linked to
       * a file that was saved, we must load that file too.
       */
      tree_getpath(node, cwd, sizeof(cwd));
      fdbr.FileId = 0;
      fdbr.JobId = node->JobId;
      if (node->hard_link && db_get_file_attributes_record(ua->jcr, ua->db, cwd, NULL, &fdbr)) {
         int32_t LinkFI;
         decode_stat(fdbr.LStat, &statp, sizeof(statp), &LinkFI); /* decode stat pkt */
         /*
          * If we point to a hard linked file, traverse the tree to
          * find that file, and mark it to be restored as well. It
          * must have the Link we just obtained and the same JobId.
          */
         if (LinkFI) {
            for (n=first_tree_node(tree->root); n; n=next_tree_node(n)) {
               if (n->FileIndex == LinkFI && n->JobId == node->JobId) {
                  n->extract = true;
                  if (n->type == TN_DIR || n->type == TN_DIR_NLS) {
                     n->extract_dir = true;
                  }
                  break;
               }
            }
         }
      }
   }
   return count;
}

static void strip_trailing_slash(char *arg)
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

/*
 * Recursively mark the current directory to be restored as
 *  well as all directories and files below it.
 */
static int markcmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   int count = 0;
   char ec1[50];

   if (ua->argc < 2 || !tree_node_has_child(tree->node)) {
      ua->send_msg(_("No files marked.\n"));
      return 1;
   }
   for (int i=1; i < ua->argc; i++) {
      strip_trailing_slash(ua->argk[i]);
      foreach_child(node, tree->node) {
         if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
            count += set_extract(ua, node, tree, true);
         }
      }
   }
   if (count == 0) {
      ua->send_msg(_("No files marked.\n"));
   } else if (count == 1) {
      ua->send_msg(_("1 file marked.\n"));
   } else {
      ua->send_msg(_("%s files marked.\n"),
               edit_uint64_with_commas(count, ec1));
   }
   return 1;
}

static int markdircmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   int count = 0;
   char ec1[50];

   if (ua->argc < 2 || !tree_node_has_child(tree->node)) {
      ua->send_msg(_("No files marked.\n"));
      return 1;
   }
   for (int i=1; i < ua->argc; i++) {
      strip_trailing_slash(ua->argk[i]);
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
      ua->send_msg(_("No directories marked.\n"));
   } else if (count == 1) {
      ua->send_msg(_("1 directory marked.\n"));
   } else {
      ua->send_msg(_("%s directories marked.\n"),
               edit_uint64_with_commas(count, ec1));
   }
   return 1;
}


static int countcmd(UAContext *ua, TREE_CTX *tree)
{
   int total, num_extract;
   char ec1[50], ec2[50];

   total = num_extract = 0;
   for (TREE_NODE *node=first_tree_node(tree->root); node; node=next_tree_node(node)) {
      if (node->type != TN_NEWDIR) {
         total++;
         if (node->extract || node->extract_dir) {
            num_extract++;
         }
      }
   }
   ua->send_msg(_("%s total files/dirs. %s marked to be restored.\n"),
            edit_uint64_with_commas(total, ec1),
            edit_uint64_with_commas(num_extract, ec2));
   return 1;
}

static int findcmd(UAContext *ua, TREE_CTX *tree)
{
   char cwd[2000];

   if (ua->argc == 1) {
      ua->send_msg(_("No file specification given.\n"));
      return 1;      /* make it non-fatal */
   }

   for (int i=1; i < ua->argc; i++) {
      for (TREE_NODE *node=first_tree_node(tree->root); node; node=next_tree_node(node)) {
         if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
            const char *tag;
            tree_getpath(node, cwd, sizeof(cwd));
            if (node->extract) {
               tag = "*";
            } else if (node->extract_dir) {
               tag = "+";
            } else {
               tag = "";
            }
            ua->send_msg("%s%s\n", tag, cwd);
         }
      }
   }
   return 1;
}

static int dot_lsdircmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;

   if (!tree_node_has_child(tree->node)) {
      return 1;
   }

   foreach_child(node, tree->node) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
         if (tree_node_has_child(node)) {
            ua->send_msg("%s/\n", node->fname);
         }
      }
   }
 
   return 1;
}

static int dot_helpcmd(UAContext *ua, TREE_CTX *tree)
{
   for (int i=0; i<comsize; i++) {
      /* List only non-dot commands */
      if (commands[i].key[0] != '.') {
         ua->send_msg("%s\n", commands[i].key);
      }
   }
   return 1;
}

static int dot_lscmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;

   if (!tree_node_has_child(tree->node)) {
      return 1;
   }

   foreach_child(node, tree->node) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
         ua->send_msg("%s%s\n", node->fname, tree_node_has_child(node)?"/":"");
      }
   }
 
   return 1;
}

static int lscmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;

   if (!tree_node_has_child(tree->node)) {
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
         ua->send_msg("%s%s%s\n", tag, node->fname, tree_node_has_child(node)?"/":"");
      }
   }
   return 1;
}

/*
 * Ls command that lists only the marked files
 */
static int dot_lsmarkcmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   if (!tree_node_has_child(tree->node)) {
      return 1;
   }
   foreach_child(node, tree->node) {
      if ((ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) &&
          (node->extract || node->extract_dir)) {
         ua->send_msg("%s%s\n", node->fname, tree_node_has_child(node)?"/":"");
      }
   }
   return 1;
}

/*
 * This recursive ls command that lists only the marked files
 */
static void rlsmark(UAContext *ua, TREE_NODE *tnode, int level)
{
   TREE_NODE *node;
   const int max_level = 100;
   char indent[max_level*2+1];
   int i, j;
   if (!tree_node_has_child(tnode)) {
      return;
   }
   level = MIN(level, max_level);
   j = 0;
   for (i=0; i<level; i++) {
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
         ua->send_msg("%s%s%s%s\n", indent, tag, node->fname, tree_node_has_child(node)?"/":"");
         if (tree_node_has_child(node)) {
            rlsmark(ua, node, level+1);
         }
      }
   }
}

static int lsmarkcmd(UAContext *ua, TREE_CTX *tree)
{
   rlsmark(ua, tree->node, 0);
   return 1;
}

/*
 * This is actually the long form used for "dir"
 */
static void ls_output(guid_list *guid, char *buf, const char *fname, const char *tag, 
                      struct stat *statp, bool dot_cmd) 
{
   char *p;
   const char *f;
   char ec1[30];
   char en1[30], en2[30];
   int n;
   time_t time;

   p = encode_mode(statp->st_mode, buf);
   if (dot_cmd) {
      *p++ = ',';
      n = sprintf(p, "%d,", (uint32_t)statp->st_nlink);
      p += n;
      n = sprintf(p, "%s,%s,", 
                  guid->uid_to_name(statp->st_uid, en1, sizeof(en1)),
                  guid->gid_to_name(statp->st_gid, en2, sizeof(en2)));
      p += n;
      n = sprintf(p, "%s,", edit_int64(statp->st_size, ec1));
      p += n;
      p = encode_time(statp->st_mtime, p);
      *p++ = ',';
      *p++ = *tag;
      *p++ = ',';
   } else {
      n = sprintf(p, "  %2d ", (uint32_t)statp->st_nlink);
      p += n;
      n = sprintf(p, "%-8.8s %-8.8s", 
                  guid->uid_to_name(statp->st_uid, en1, sizeof(en1)),
                  guid->gid_to_name(statp->st_gid, en2, sizeof(en2)));
      p += n;
      n = sprintf(p, "%12.12s  ", edit_int64(statp->st_size, ec1));
      p += n;
      if (statp->st_ctime > statp->st_mtime) {
         time = statp->st_ctime;
      } else {
         time = statp->st_mtime;
      }
      /* Display most recent time */
      p = encode_time(time, p);
      *p++ = ' ';
      *p++ = *tag;
   }
   for (f=fname; *f; ) {
      *p++ = *f++;
   }
   *p = 0;
}

/*
 * Like ls command, but give more detail on each file
 */
static int do_dircmd(UAContext *ua, TREE_CTX *tree, bool dot_cmd)
{
   TREE_NODE *node;
   FILE_DBR fdbr;
   struct stat statp;
   char buf[1100];
   char cwd[1100], *pcwd;
   guid_list *guid;

   if (!tree_node_has_child(tree->node)) {
      ua->send_msg(_("Node %s has no children.\n"), tree->node->fname);
      return 1;
   }

   guid = new_guid_list();
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
         tree_getpath(node, cwd, sizeof(cwd));
         fdbr.FileId = 0;
         fdbr.JobId = node->JobId;
         /*
          * Strip / from soft links to directories.
          *   This is because soft links to files have a trailing slash
          *   when returned from tree_getpath, but db_get_file_attr...
          *   treats soft links as files, so they do not have a trailing
          *   slash like directory names.
          */
         if (node->type == TN_FILE && tree_node_has_child(node)) {
            bstrncpy(buf, cwd, sizeof(buf));
            pcwd = buf;
            int len = strlen(buf);
            if (len > 1) {
               buf[len-1] = 0;        /* strip trailing / */
            }
         } else {
            pcwd = cwd;
         }
         if (db_get_file_attributes_record(ua->jcr, ua->db, pcwd, NULL, &fdbr)) {
            int32_t LinkFI;
            decode_stat(fdbr.LStat, &statp, sizeof(statp), &LinkFI); /* decode stat pkt */
         } else {
            /* Something went wrong getting attributes -- print name */
            memset(&statp, 0, sizeof(statp));
         }
         ls_output(guid, buf, cwd, tag, &statp, dot_cmd);
         ua->send_msg("%s\n", buf);
      }
   }
   free_guid_list(guid);
   return 1;
}

int dot_dircmd(UAContext *ua, TREE_CTX *tree)
{
   return do_dircmd(ua, tree, true/*dot command*/);
}

static int dircmd(UAContext *ua, TREE_CTX *tree)
{
   return do_dircmd(ua, tree, false/*not dot command*/);
}


static int estimatecmd(UAContext *ua, TREE_CTX *tree)
{
   int total, num_extract;
   uint64_t total_bytes = 0;
   FILE_DBR fdbr;
   struct stat statp;
   char cwd[1100];
   char ec1[50];

   total = num_extract = 0;
   for (TREE_NODE *node=first_tree_node(tree->root); node; node=next_tree_node(node)) {
      if (node->type != TN_NEWDIR) {
         total++;
         /* If regular file, get size */
         if (node->extract && node->type == TN_FILE) {
            num_extract++;
            tree_getpath(node, cwd, sizeof(cwd));
            fdbr.FileId = 0;
            fdbr.JobId = node->JobId;
            if (db_get_file_attributes_record(ua->jcr, ua->db, cwd, NULL, &fdbr)) {
               int32_t LinkFI;
               decode_stat(fdbr.LStat, &statp, sizeof(statp), &LinkFI); /* decode stat pkt */
               if (S_ISREG(statp.st_mode) && statp.st_size > 0) {
                  total_bytes += statp.st_size;
               }
            }
         /* Directory, count only */
         } else if (node->extract || node->extract_dir) {
            num_extract++;
         }
      }
   }
   ua->send_msg(_("%d total files; %d marked to be restored; %s bytes.\n"),
            total, num_extract, edit_uint64_with_commas(total_bytes, ec1));
   return 1;
}



static int helpcmd(UAContext *ua, TREE_CTX *tree)
{
   unsigned int i;

   ua->send_msg(_("  Command    Description\n  =======    ===========\n"));
   for (i=0; i<comsize; i++) {
      /* List only non-dot commands */
      if (commands[i].key[0] != '.') {
         ua->send_msg("  %-10s %s\n", _(commands[i].key), _(commands[i].help));
      }
   }
   ua->send_msg("\n");
   return 1;
}

/*
 * Change directories.  Note, if the user specifies x: and it fails,
 *   we assume it is a Win32 absolute cd rather than relative and
 *   try a second time with /x: ...  Win32 kludge.
 */
static int cdcmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   char cwd[2000];


   if (ua->argc != 2) {
      ua->error_msg(_("Too few or too many arguments. Try using double quotes.\n"));
      return 1;
   }

   node = tree_cwd(ua->argk[1], tree->root, tree->node);
   if (!node) {
      /* Try once more if Win32 drive -- make absolute */
      if (ua->argk[1][1] == ':') {  /* win32 drive */
         bstrncpy(cwd, "/", sizeof(cwd));
         bstrncat(cwd, ua->argk[1], sizeof(cwd));
         node = tree_cwd(cwd, tree->root, tree->node);
      }
      if (!node) {
         ua->warning_msg(_("Invalid path given.\n"));
      } else {
         tree->node = node;
      }
   } else {
      tree->node = node;
   }
   return pwdcmd(ua, tree);
}

static int pwdcmd(UAContext *ua, TREE_CTX *tree)
{
   char cwd[2000];
   tree_getpath(tree->node, cwd, sizeof(cwd));
   if (ua->api) {
      ua->send_msg("%s", cwd);
   } else {
      ua->send_msg(_("cwd is: %s\n"), cwd);
   }
   return 1;
}

static int dot_pwdcmd(UAContext *ua, TREE_CTX *tree)
{
   char cwd[2000];
   tree_getpath(tree->node, cwd, sizeof(cwd));
   ua->send_msg("%s", cwd);
   return 1;
}

static int unmarkcmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   int count = 0;

   if (ua->argc < 2 || !tree_node_has_child(tree->node)) {
      ua->send_msg(_("No files unmarked.\n"));
      return 1;
   }
   for (int i=1; i < ua->argc; i++) {
      strip_trailing_slash(ua->argk[i]);
      foreach_child(node, tree->node) {
         if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
            count += set_extract(ua, node, tree, false);
         }
      }
   }
   if (count == 0) {
      ua->send_msg(_("No files unmarked.\n"));
   } else if (count == 1) {
      ua->send_msg(_("1 file unmarked.\n"));
   } else {
      char ed1[50];
      ua->send_msg(_("%s files unmarked.\n"), edit_uint64_with_commas(count, ed1));
   }
   return 1;
}

static int unmarkdircmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   int count = 0;

   if (ua->argc < 2 || !tree_node_has_child(tree->node)) {
      ua->send_msg(_("No directories unmarked.\n"));
      return 1;
   }

   for (int i=1; i < ua->argc; i++) {
      strip_trailing_slash(ua->argk[i]);
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
      ua->send_msg(_("No directories unmarked.\n"));
   } else if (count == 1) {
      ua->send_msg(_("1 directory unmarked.\n"));
   } else {
      ua->send_msg(_("%d directories unmarked.\n"), count);
   }
   return 1;
}


static int donecmd(UAContext *ua, TREE_CTX *tree)
{
   return 0;
}

static int quitcmd(UAContext *ua, TREE_CTX *tree)
{
   ua->quit = true;
   return 0;
}
