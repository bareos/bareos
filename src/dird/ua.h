/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.

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
 * Includes specific to the Director User Agent Server
 *
 *     Kern Sibbald, August MMI
 *
 */

#ifndef __UA_H_
#define __UA_H_ 1

class UAContext {
public:
   BSOCK *UA_sock;
   BSOCK *sd;
   JCR *jcr;
   B_DB *db;
   CAT *catalog;
   CONRES *cons;                      /* console resource */
   POOLMEM *cmd;                      /* return command/name buffer */
   POOLMEM *args;                     /* command line arguments */
   POOLMEM *errmsg;                   /* store error message */
   char *argk[MAX_CMD_ARGS];          /* argument keywords */
   char *argv[MAX_CMD_ARGS];          /* argument values */
   int argc;                          /* number of arguments */
   char **prompt;                     /* list of prompts */
   int max_prompts;                   /* max size of list */
   int num_prompts;                   /* current number in list */
   int api;                           /* For programs want an API */
   int cmd_index;                     /* Index in command table */
   bool force_mult_db_connections;    /* overwrite cat.mult_db_connections */
   bool auto_display_messages;        /* if set, display messages */
   bool user_notified_msg_pending;    /* set when user notified */
   bool automount;                    /* if set, mount after label */
   bool quit;                         /* if set, quit */
   bool verbose;                      /* set for normal UA verbosity */
   bool batch;                        /* set for non-interactive mode */
   bool gui;                          /* set if talking to GUI program */
   bool runscript;                    /* set if we are in runscript */
   uint32_t pint32_val;               /* positive integer */
   int32_t  int32_val;                /* positive/negative */
   int64_t  int64_val;                /* big int */

   void signal(int sig) { UA_sock->signal(sig); };

   /* The below are in ua_output.c */
   void send_msg(const char *fmt, ...);
   void error_msg(const char *fmt, ...);
   void warning_msg(const char *fmt, ...);
   void info_msg(const char *fmt, ...);
};

/* Context for insert_tree_handler() */
struct TREE_CTX {
   TREE_ROOT *root;                   /* root */
   TREE_NODE *node;                   /* current node */
   TREE_NODE *avail_node;             /* unused node last insert */
   int cnt;                           /* count for user feedback */
   bool all;                          /* if set mark all as default */
   UAContext *ua;
   uint32_t FileEstimate;             /* estimate of number of files */
   uint32_t FileCount;                /* current count of files */
   uint32_t LastCount;                /* last count of files */
   uint32_t DeltaCount;               /* trigger for printing */
};

struct NAME_LIST {
   char **name;                       /* list of names */
   int num_ids;                       /* ids stored */
   int max_ids;                       /* size of array */
   int num_del;                       /* number deleted */
   int tot_ids;                       /* total to process */
};


/* Main structure for obtaining JobIds or Files to be restored */
struct RESTORE_CTX {
   utime_t JobTDate;
   uint32_t TotalFiles;
   JobId_t JobId;
   char ClientName[MAX_NAME_LENGTH];  /* backup client */
   char RestoreClientName[MAX_NAME_LENGTH];  /* restore client */
   char last_jobid[20];
   POOLMEM *JobIds;                   /* User entered string of JobIds */
   POOLMEM *BaseJobIds;               /* Base jobids */
   STORE  *store;
   JOB *restore_job;
   POOL *pool;
   int restore_jobs;
   uint32_t selected_files;
   char *comment;
   char *where;
   char *RegexWhere;
   char *replace;
   RBSR *bsr;
   POOLMEM *fname;                    /* filename only */
   POOLMEM *path;                     /* path only */
   POOLMEM *query;
   int fnl;                           /* filename length */
   int pnl;                           /* path length */
   bool found;
   bool all;                          /* mark all as default */
   NAME_LIST name_list;
};

#define MAX_ID_LIST_LEN 2000000

#endif
