/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
 * BAREOS Director -- fd_cmds.c -- send commands to File daemon
 *
 * Kern Sibbald, October MM
 *
 * This routine is run as a separate thread.  There may be more
 * work to be done to make it totally reentrant!!!!
 *
 * Utility functions for sending info to File Daemon.
 * These functions are used by both backup and verify.
 */

#include "bareos.h"
#include "dird.h"
#include "findlib/find.h"

const int dbglvl = 400;

/* Commands sent to File daemon */
static char filesetcmd[] = "fileset%s\n"; /* set full fileset */
static char jobcmd[] = "JobId=%s Job=%s SDid=%u SDtime=%u Authorization=%s\n";
/* Note, mtime_only is not used here -- implemented as file option */
static char levelcmd[] = "level = %s%s%s mtime_only=%d %s%s\n";
static char runscript[] = "Run OnSuccess=%u OnFailure=%u AbortOnError=%u When=%u Command=%s\n";
static char runbeforenow[]= "RunBeforeNow\n";
static char bandwidthcmd[] = "setbandwidth=%lld Job=%s\n";

/* Responses received from File daemon */
static char OKinc[] = "2000 OK include\n";
static char OKjob[] = "2000 OK Job";
static char OKlevel[] = "2000 OK level\n";
static char OKRunScript[] = "2000 OK RunScript\n";
static char OKRunBeforeNow[] = "2000 OK RunBeforeNow\n";
static char OKRestoreObject[] = "2000 OK ObjectRestored\n";
static char OKBandwidth[] = "2000 OK Bandwidth\n";

/* Forward referenced functions */
static bool send_list_item(JCR *jcr, const char *code, char *item, BSOCK *fd);

/* External functions */
extern DIRRES *director;
extern int FDConnectTimeout;

#define INC_LIST 0
#define EXC_LIST 1

/*
 * Open connection with File daemon.
 * Try connecting every retry_interval (default 10 sec), and
 *   give up after max_retry_time (default 30 mins).
 */

int connect_to_file_daemon(JCR *jcr, int retry_interval, int max_retry_time, int verbose)
{
   BSOCK *fd = new_bsock();
   char ed1[30];
   utime_t heart_beat;

   if (jcr->res.client->heartbeat_interval) {
      heart_beat = jcr->res.client->heartbeat_interval;
   } else {
      heart_beat = director->heartbeat_interval;
   }

   if (!jcr->file_bsock) {
      char name[MAX_NAME_LENGTH + 100];
      bstrncpy(name, _("Client: "), sizeof(name));
      bstrncat(name, jcr->res.client->name(), sizeof(name));

      fd->set_source_address(director->DIRsrc_addr);
      if (!fd->connect(jcr,retry_interval,max_retry_time, heart_beat, name, jcr->res.client->address,
           NULL, jcr->res.client->FDport, verbose)) {
        fd->destroy();
        fd = NULL;
      }

      if (fd == NULL) {
         jcr->setJobStatus(JS_ErrorTerminated);
         return 0;
      }
      Dmsg0(10, "Opened connection with File daemon\n");
   } else {
      fd = jcr->file_bsock;           /* use existing connection */
   }
   fd->res = (RES *)jcr->res.client;  /* save resource in BSOCK */
   jcr->file_bsock = fd;
   jcr->setJobStatus(JS_Running);

   if (!authenticate_file_daemon(jcr)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      return 0;
   }

   /*
    * Now send JobId and authorization key
    */
   if (jcr->sd_auth_key == NULL) {
      jcr->sd_auth_key = bstrdup("dummy");
   }
   fd->fsend(jobcmd, edit_int64(jcr->JobId, ed1), jcr->Job, jcr->VolSessionId,
             jcr->VolSessionTime, jcr->sd_auth_key);
   if (!jcr->keep_sd_auth_key && !bstrcmp(jcr->sd_auth_key, "dummy")) {
      memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
   }
   Dmsg1(100, ">filed: %s", fd->msg);
   if (bget_dirmsg(fd) > 0) {
       Dmsg1(110, "<filed: %s", fd->msg);
       if (!bstrncmp(fd->msg, OKjob, strlen(OKjob))) {
          Jmsg(jcr, M_FATAL, 0, _("File daemon \"%s\" rejected Job command: %s\n"),
             jcr->res.client->hdr.name, fd->msg);
          jcr->setJobStatus(JS_ErrorTerminated);
          return 0;
       } else if (jcr->db) {
          CLIENT_DBR cr;
          memset(&cr, 0, sizeof(cr));
          bstrncpy(cr.Name, jcr->res.client->hdr.name, sizeof(cr.Name));
          cr.AutoPrune = jcr->res.client->AutoPrune;
          cr.FileRetention = jcr->res.client->FileRetention;
          cr.JobRetention = jcr->res.client->JobRetention;
          bstrncpy(cr.Uname, fd->msg+strlen(OKjob)+1, sizeof(cr.Uname));
          if (!db_update_client_record(jcr, jcr->db, &cr)) {
             Jmsg(jcr, M_WARNING, 0, _("Error updating Client record. ERR=%s\n"),
                db_strerror(jcr->db));
          }
       }
   } else {
      Jmsg(jcr, M_FATAL, 0, _("FD gave bad response to JobId command: %s\n"),
         bnet_strerror(fd));
      jcr->setJobStatus(JS_ErrorTerminated);
      return 0;
   }
   return 1;
}

static void send_since_time(JCR *jcr)
{
   BSOCK   *fd = jcr->file_bsock;
   utime_t stime;
   char ed1[50];

   stime = str_to_utime(jcr->stime);
   fd->fsend(levelcmd, "", NT_("since_utime "), edit_uint64(stime, ed1), 0,
             NT_("prev_job="), jcr->PrevJob);
   while (bget_dirmsg(fd) >= 0) {  /* allow him to poll us to sync clocks */
      Jmsg(jcr, M_INFO, 0, "%s\n", fd->msg);
   }
}

bool send_bwlimit(JCR *jcr, const char *Job)
{
   BSOCK *fd = jcr->file_bsock;

   if (jcr->FDVersion >= 4) {
      fd->fsend(bandwidthcmd, jcr->max_bandwidth, Job);
      if (!response(jcr, fd, OKBandwidth, "Bandwidth", DISPLAY_ERROR)) {
         jcr->max_bandwidth = 0;      /* can't set bandwidth limit */
         return false;
      }
   }

   return true;
}

/*
 * Send level command to FD.
 * Used for backup jobs and estimate command.
 */
bool send_level_command(JCR *jcr)
{
   int JobLevel;
   BSOCK *fd = jcr->file_bsock;
   const char *accurate = jcr->accurate?"accurate_":"";
   const char *not_accurate = "";
   const char *rerunning = jcr->rerunning?" rerunning ":" ";
   /*
    * Send Level command to File daemon
    */
   JobLevel = jcr->getJobLevel();
   switch (JobLevel) {
   case L_BASE:
      fd->fsend(levelcmd, not_accurate, "base", rerunning, 0, "", "");
      break;
   /* L_NONE is the console, sending something off to the FD */
   case L_NONE:
   case L_FULL:
      fd->fsend(levelcmd, not_accurate, "full", rerunning, 0, "", "");
      break;
   case L_DIFFERENTIAL:
      fd->fsend(levelcmd, accurate, "differential", rerunning, 0, "", "");
      send_since_time(jcr);
      break;
   case L_INCREMENTAL:
      fd->fsend(levelcmd, accurate, "incremental", rerunning, 0, "", "");
      send_since_time(jcr);
      break;
   case L_SINCE:
   default:
      Jmsg2(jcr, M_FATAL, 0, _("Unimplemented backup level %d %c\n"), JobLevel, JobLevel);
      return 0;
   }
   Dmsg1(120, ">filed: %s", fd->msg);
   if (!response(jcr, fd, OKlevel, "Level", DISPLAY_ERROR)) {
      return false;
   }
   return true;
}

/*
 * Send either an Included or an Excluded list to FD
 */
static bool send_fileset(JCR *jcr)
{
   FILESETRES *fileset = jcr->res.fileset;
   BSOCK *fd = jcr->file_bsock;
   STORERES *store = jcr->res.wstore;
   int num;
   bool include = true;

   for ( ;; ) {
      if (include) {
         num = fileset->num_includes;
      } else {
         num = fileset->num_excludes;
      }
      for (int i=0; i<num; i++) {
         char *item;
         INCEXE *ie;
         int j, k;

         if (include) {
            ie = fileset->include_items[i];
            fd->fsend("I\n");
         } else {
            ie = fileset->exclude_items[i];
            fd->fsend("E\n");
         }

         for (j = 0; j < ie->ignoredir.size(); j++) {
            bnet_fsend(fd, "Z %s\n", ie->ignoredir.get(j));
         }

         for (j=0; j<ie->num_opts; j++) {
            FOPTS *fo = ie->opts_list[j];

            bool enhanced_wild = false;
            for (k=0; fo->opts[k]!='\0'; k++) {
               if (fo->opts[k]=='W') {
                  enhanced_wild = true;
                  break;
               }
            }

            /* Strip out compression option Zn if disallowed for this Storage */
            if (store && !store->AllowCompress) {
               char newopts[MAX_FOPTS];
               bool done=false;         /* print warning only if compression enabled in FS */
               int j = 0;
               for (k=0; fo->opts[k]!='\0'; k++) {
                 /* Z compress option is followed by the single-digit compress level or 'o' */
                 if (fo->opts[k]=='Z') {
                    done=true;
                    k++;                /* skip option and level */
                 } else {
                    newopts[j] = fo->opts[k];
                    j++;
                 }
               }
               newopts[j] = '\0';

               if (done) {
                  Jmsg(jcr, M_INFO, 0,
                      _("FD compression disabled for this Job because AllowCompress=No in Storage resource.\n") );
               }
               /* Send the new trimmed option set without overwriting fo->opts */
               fd->fsend("O %s\n", newopts);
            } else {
               /* Send the original options */
               fd->fsend("O %s\n", fo->opts);
            }

            for (k=0; k<fo->regex.size(); k++) {
               fd->fsend("R %s\n", fo->regex.get(k));
            }
            for (k=0; k<fo->regexdir.size(); k++) {
               fd->fsend("RD %s\n", fo->regexdir.get(k));
            }
            for (k=0; k<fo->regexfile.size(); k++) {
               fd->fsend("RF %s\n", fo->regexfile.get(k));
            }
            for (k=0; k<fo->wild.size(); k++) {
               fd->fsend("W %s\n", fo->wild.get(k));
            }
            for (k=0; k<fo->wilddir.size(); k++) {
               fd->fsend("WD %s\n", fo->wilddir.get(k));
            }
            for (k=0; k<fo->wildfile.size(); k++) {
               fd->fsend("WF %s\n", fo->wildfile.get(k));
            }
            for (k=0; k<fo->wildbase.size(); k++) {
               fd->fsend("W%c %s\n", enhanced_wild ? 'B' : 'F', fo->wildbase.get(k));
            }
            for (k=0; k<fo->base.size(); k++) {
               fd->fsend("B %s\n", fo->base.get(k));
            }
            for (k=0; k<fo->fstype.size(); k++) {
               fd->fsend("X %s\n", fo->fstype.get(k));
            }
            for (k=0; k<fo->drivetype.size(); k++) {
               fd->fsend("XD %s\n", fo->drivetype.get(k));
            }
            if (fo->plugin) {
               fd->fsend("G %s\n", fo->plugin);
            }
            if (fo->reader) {
               fd->fsend("D %s\n", fo->reader);
            }
            if (fo->writer) {
               fd->fsend("T %s\n", fo->writer);
            }
            fd->fsend("N\n");
         }

         for (j=0; j<ie->name_list.size(); j++) {
            item = (char *)ie->name_list.get(j);
            if (!send_list_item(jcr, "F ", item, fd)) {
               goto bail_out;
            }
         }
         fd->fsend("N\n");
         for (j=0; j<ie->plugin_list.size(); j++) {
            item = (char *)ie->plugin_list.get(j);
            if (!send_list_item(jcr, "P ", item, fd)) {
               goto bail_out;
            }
         }
         fd->fsend("N\n");
      }
      if (!include) {                 /* If we just did excludes */
         break;                       /*   all done */
      }
      include = false;                /* Now do excludes */
   }

   fd->signal(BNET_EOD);              /* end of data */
   if (!response(jcr, fd, OKinc, "Include", DISPLAY_ERROR)) {
      goto bail_out;
   }
   return true;

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);
   return false;

}

static bool send_list_item(JCR *jcr, const char *code, char *item, BSOCK *fd)
{
   BPIPE *bpipe;
   FILE *ffd;
   char buf[2000];
   int optlen, status;
   char *p = item;

   switch (*p) {
   case '|':
      p++;                      /* skip over the | */
      fd->msg = edit_job_codes(jcr, fd->msg, p, "");
      bpipe = open_bpipe(fd->msg, 0, "r");
      if (!bpipe) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Cannot run program: %s. ERR=%s\n"),
            p, be.bstrerror());
         return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf+optlen, sizeof(buf)-optlen, bpipe->rfd)) {
         fd->msglen = Mmsg(fd->msg, "%s", buf);
         Dmsg2(500, "Inc/exc len=%d: %s", fd->msglen, fd->msg);
         if (!bnet_send(fd)) {
            Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
            return false;
         }
      }
      if ((status = close_bpipe(bpipe)) != 0) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Error running program: %s. ERR=%s\n"),
            p, be.bstrerror(status));
         return false;
      }
      break;
   case '<':
      p++;                      /* skip over < */
      if ((ffd = fopen(p, "rb")) == NULL) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Cannot open included file: %s. ERR=%s\n"),
            p, be.bstrerror());
         return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf+optlen, sizeof(buf)-optlen, ffd)) {
         fd->msglen = Mmsg(fd->msg, "%s", buf);
         if (!bnet_send(fd)) {
            Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
            fclose(ffd);
            return false;
         }
      }
      fclose(ffd);
      break;
   case '\\':
      p++;                      /* skip over \ */
      /* Note, fall through wanted */
   default:
      pm_strcpy(fd->msg, code);
      fd->msglen = pm_strcat(fd->msg, p);
      Dmsg1(500, "Inc/Exc name=%s\n", fd->msg);
      if (!fd->send()) {
         Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
         return false;
      }
      break;
   }
   return true;
}

/*
 * Send include list to File daemon
 */
bool send_include_list(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   if (jcr->res.fileset->new_include) {
      fd->fsend(filesetcmd, jcr->res.fileset->enable_vss ? " vss=1" : "");
      return send_fileset(jcr);
   }
   return true;
}

/*
 * Send exclude list to File daemon
 *   Under the new scheme, the Exclude list
 *   is part of the FileSet sent with the
 *   "include_list" above.
 */
bool send_exclude_list(JCR *jcr)
{
   return true;
}

/*
 * This checks to see if there are any non local runscripts for this job.
 */
static inline bool have_client_runscripts(alist *run_scripts)
{
   RUNSCRIPT *cmd;
   bool retval = false;

   if (run_scripts->empty()) {
      return false;
   }

   foreach_alist(cmd, run_scripts) {
      if (!cmd->is_local()) {
         retval = true;
      }
   }

   return retval;
}

/*
 * Send RunScripts to File daemon
 * 1) We send all runscript to FD, they can be executed Before, After, or twice
 * 2) Then, we send a "RunBeforeNow" command to the FD to tell him to do the
 *    first run_script() call. (ie ClientRunBeforeJob)
 */
int send_runscripts_commands(JCR *jcr)
{
   int result;
   RUNSCRIPT *cmd;
   POOLMEM *msg, *ehost;
   BSOCK *fd = jcr->file_bsock;
   bool has_before_jobs = false;

   /*
    * See if there are any runscripts that need to be ran on the client.
    */
   if (!have_client_runscripts(jcr->res.job->RunScripts)) {
      return 1;
   }

   Dmsg0(120, "bdird: sending runscripts to fd\n");

   msg = get_pool_memory(PM_FNAME);
   ehost = get_pool_memory(PM_FNAME);
   foreach_alist(cmd, jcr->res.job->RunScripts) {
      if (cmd->can_run_at_level(jcr->getJobLevel()) && cmd->target) {
         ehost = edit_job_codes(jcr, ehost, cmd->target, "");
         Dmsg2(200, "bdird: runscript %s -> %s\n", cmd->target, ehost);

         if (bstrcmp(ehost, jcr->res.client->name())) {
            pm_strcpy(msg, cmd->command);
            bash_spaces(msg);

            Dmsg1(120, "bdird: sending runscripts to fd '%s'\n", cmd->command);

            fd->fsend(runscript,
                      cmd->on_success,
                      cmd->on_failure,
                      cmd->fail_on_error,
                      cmd->when,
                      msg);

            result = response(jcr, fd, OKRunScript, "RunScript", DISPLAY_ERROR);

            if (!result) {
               goto bail_out;
            }
         }
         /* TODO : we have to play with other client */
         /*
           else {
           send command to an other client
           }
         */
      }

      /*
       * See if this is a ClientRunBeforeJob.
       */
      if (cmd->when & SCRIPT_Before || cmd->when & SCRIPT_AfterVSS) {
         has_before_jobs = true;
      }
   }

   /*
    * Tell the FD to execute the ClientRunBeforeJob
    */
   if (has_before_jobs) {
      fd->fsend(runbeforenow);
      if (!response(jcr, fd, OKRunBeforeNow, "RunBeforeNow", DISPLAY_ERROR)) {
        goto bail_out;
      }
   }

   free_pool_memory(msg);
   free_pool_memory(ehost);

   return 1;

bail_out:
   Jmsg(jcr, M_FATAL, 0, _("Client \"%s\" RunScript failed.\n"), ehost);
   free_pool_memory(msg);
   free_pool_memory(ehost);

   return 0;
}

struct OBJ_CTX {
   JCR *jcr;
   int count;
};

static int restore_object_handler(void *ctx, int num_fields, char **row)
{
   OBJ_CTX *octx = (OBJ_CTX *)ctx;
   JCR *jcr = octx->jcr;
   BSOCK *fd;

   fd = jcr->file_bsock;
   if (jcr->is_job_canceled()) {
      return 1;
   }
   /* Old File Daemon doesn't handle restore objects */
   if (jcr->FDVersion < 3) {
      Jmsg(jcr, M_WARNING, 0, _("Client \"%s\" may not be used to restore "
                                "this job. Please upgrade your client.\n"),
           jcr->res.client->name());
      return 1;
   }

   if (jcr->FDVersion < 5) {    /* Old version without PluginName */
      fd->fsend("restoreobject JobId=%s %s,%s,%s,%s,%s,%s\n",
                row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
   } else {
      /* bash spaces from PluginName */
      bash_spaces(row[9]);
      fd->fsend("restoreobject JobId=%s %s,%s,%s,%s,%s,%s,%s\n",
                row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[9]);
   }
   Dmsg1(010, "Send obj hdr=%s", fd->msg);

   fd->msglen = pm_strcpy(fd->msg, row[7]);
   fd->send();                            /* send Object name */

   Dmsg1(010, "Send obj: %s\n", fd->msg);

//   fd->msglen = str_to_uint64(row[1]);   /* object length */
//   Dmsg1(000, "obj size: %lld\n", (uint64_t)fd->msglen);

   /* object */
   db_unescape_object(jcr, jcr->db,
                      row[8],                /* Object  */
                      str_to_uint64(row[1]), /* Object length */
                      &fd->msg, &fd->msglen);
   fd->send();                           /* send object */
   octx->count++;

   if (debug_level) {
      for (int i=0; i < fd->msglen; i++)
         if (!fd->msg[i])
            fd->msg[i] = ' ';
      Dmsg1(000, "Send obj: %s\n", fd->msg);
   }

   return 0;
}

bool send_restore_objects(JCR *jcr)
{
   char ed1[50];
   POOL_MEM query(PM_MESSAGE);
   BSOCK *fd;
   OBJ_CTX octx;

   if (!jcr->JobIds || !jcr->JobIds[0]) {
      return true;
   }
   octx.jcr = jcr;
   octx.count = 0;

   /* restore_object_handler is called for each file found */

   /* send restore objects for all jobs involved  */
   Mmsg(query, get_restore_objects, jcr->JobIds, FT_RESTORE_FIRST);
   db_sql_query(jcr->db, query.c_str(), restore_object_handler, (void *)&octx);

   /* send config objects for the current restore job */
   Mmsg(query, get_restore_objects,
        edit_uint64(jcr->JobId, ed1), FT_PLUGIN_CONFIG_FILLED);
   db_sql_query(jcr->db, query.c_str(), restore_object_handler, (void *)&octx);

   /*
    * Send to FD only if we have at least one restore object.
    * This permits backward compatibility with older FDs.
    */
   if (octx.count > 0) {
      fd = jcr->file_bsock;
      fd->fsend("restoreobject end\n");
      if (!response(jcr, fd, OKRestoreObject, "RestoreObject", DISPLAY_ERROR)) {
         Jmsg(jcr, M_FATAL, 0, _("RestoreObject failed.\n"));
         return false;
      }
   }
   return true;
}

/*
 * Read the attributes from the File daemon for
 * a Verify job and store them in the catalog.
 */
int get_attributes_and_put_in_catalog(JCR *jcr)
{
   BSOCK   *fd;
   int n = 0;
   ATTR_DBR *ar = NULL;
   char digest[MAXSTRING];

   fd = jcr->file_bsock;
   jcr->jr.FirstIndex = 1;
   jcr->FileIndex = 0;
   /* Start transaction allocates jcr->attr and jcr->ar if needed */
   db_start_transaction(jcr, jcr->db);     /* start transaction if not already open */
   ar = jcr->ar;

   Dmsg0(120, "bdird: waiting to receive file attributes\n");
   /* Pickup file attributes and digest */
   while (!fd->errors && (n = bget_dirmsg(fd)) > 0) {
      uint32_t file_index;
      int stream, len;
      char *p, *fn;
      char Digest[MAXSTRING];      /* either Verify opts or MD5/SHA1 digest */

      if ((len = sscanf(fd->msg, "%ld %d %s", &file_index, &stream, Digest)) != 3) {
         Jmsg(jcr, M_FATAL, 0, _("<filed: bad attributes, expected 3 fields got %d\n"
"msglen=%d msg=%s\n"), len, fd->msglen, fd->msg);
         jcr->setJobStatus(JS_ErrorTerminated);
         return 0;
      }
      p = fd->msg;
      /* The following three fields were sscanf'ed above so skip them */
      skip_nonspaces(&p);             /* skip FileIndex */
      skip_spaces(&p);
      skip_nonspaces(&p);             /* skip Stream */
      skip_spaces(&p);
      skip_nonspaces(&p);             /* skip Opts_Digest */
      p++;                            /* skip space */
      Dmsg1(dbglvl, "Stream=%d\n", stream);
      if (stream == STREAM_UNIX_ATTRIBUTES || stream == STREAM_UNIX_ATTRIBUTES_EX) {
         if (jcr->cached_attribute) {
            Dmsg3(dbglvl, "Cached attr. Stream=%d fname=%s\n", ar->Stream, ar->fname,
               ar->attr);
            if (!db_create_file_attributes_record(jcr, jcr->db, ar)) {
               Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), db_strerror(jcr->db));
            }
         }
         /* Any cached attr is flushed so we can reuse jcr->attr and jcr->ar */
         fn = jcr->fname = check_pool_memory_size(jcr->fname, fd->msglen);
         while (*p != 0) {
            *fn++ = *p++;                /* copy filename */
         }
         *fn = *p++;                     /* term filename and point p to attribs */
         pm_strcpy(jcr->attr, p);        /* save attributes */
         jcr->JobFiles++;
         jcr->FileIndex = file_index;
         ar->attr = jcr->attr;
         ar->fname = jcr->fname;
         ar->FileIndex = file_index;
         ar->Stream = stream;
         ar->link = NULL;
         ar->JobId = jcr->JobId;
         ar->ClientId = jcr->ClientId;
         ar->PathId = 0;
         ar->FilenameId = 0;
         ar->Digest = NULL;
         ar->DigestType = CRYPTO_DIGEST_NONE;
         ar->DeltaSeq = 0;
         jcr->cached_attribute = true;

         Dmsg2(dbglvl, "dird<filed: stream=%d %s\n", stream, jcr->fname);
         Dmsg1(dbglvl, "dird<filed: attr=%s\n", ar->attr);
         jcr->FileId = ar->FileId;
      /*
       * First, get STREAM_UNIX_ATTRIBUTES and fill ATTR_DBR structure
       * Next, we CAN have a CRYPTO_DIGEST, so we fill ATTR_DBR with it (or not)
       * When we get a new STREAM_UNIX_ATTRIBUTES, we known that we can add file to the catalog
       * At the end, we have to add the last file
       */
      } else if (crypto_digest_stream_type(stream) != CRYPTO_DIGEST_NONE) {
         if (jcr->FileIndex != (uint32_t)file_index) {
            Jmsg3(jcr, M_ERROR, 0, _("%s index %d not same as attributes %d\n"),
               stream_to_ascii(stream), file_index, jcr->FileIndex);
            continue;
         }
         ar->Digest = digest;
         ar->DigestType = crypto_digest_stream_type(stream);
         db_escape_string(jcr, jcr->db, digest, Digest, strlen(Digest));
         Dmsg4(dbglvl, "stream=%d DigestLen=%d Digest=%s type=%d\n", stream,
               strlen(digest), digest, ar->DigestType);
      }
      jcr->jr.JobFiles = jcr->JobFiles = file_index;
      jcr->jr.LastIndex = file_index;
   }
   if (is_bnet_error(fd)) {
      Jmsg1(jcr, M_FATAL, 0, _("<filed: Network error getting attributes. ERR=%s\n"),
            fd->bstrerror());
      return 0;
   }
   if (jcr->cached_attribute) {
      Dmsg3(dbglvl, "Cached attr with digest. Stream=%d fname=%s attr=%s\n", ar->Stream,
         ar->fname, ar->attr);
      if (!db_create_file_attributes_record(jcr, jcr->db, ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), db_strerror(jcr->db));
      }
      jcr->cached_attribute = false;
   }
   jcr->setJobStatus(JS_Terminated);
   return 1;
}

/*
 * Cancel a job running in the File daemon
 */
bool cancel_file_daemon_job(UAContext *ua, JCR *jcr)
{
   BSOCK *fd;

   ua->jcr->res.client = jcr->res.client;
   if (!connect_to_file_daemon(ua->jcr, 10, FDConnectTimeout, 1)) {
      ua->error_msg(_("Failed to connect to File daemon.\n"));
      return false;
   }
   Dmsg0(200, "Connected to file daemon\n");
   fd = ua->jcr->file_bsock;
   fd->fsend("cancel Job=%s\n", jcr->Job);
   while (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }
   fd->signal(BNET_TERMINATE);
   fd->close();
   ua->jcr->file_bsock = NULL;
   jcr->file_bsock->set_terminated();
   jcr->my_thread_send_signal(TIMEOUT_SIGNAL);
   return true;
}

/*
 * Get the status of a remote File Daemon.
 */
void do_native_client_status(UAContext *ua, CLIENTRES *client, char *cmd)
{
   BSOCK *fd;

   /*
    * Connect to File daemon
    */
   ua->jcr->res.client = client;

   /*
    * Release any old dummy key
    */
   if (ua->jcr->sd_auth_key) {
      free(ua->jcr->sd_auth_key);
   }

   /*
    * Create a new dummy SD auth key
    */
   ua->jcr->sd_auth_key = bstrdup("dummy");

   /*
    * Try to connect for 15 seconds
    */
   if (!ua->api) ua->send_msg(_("Connecting to Client %s at %s:%d\n"),
      client->name(), client->address, client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
      ua->send_msg(_("Failed to connect to Client %s.\n====\n"),
         client->name());
      if (ua->jcr->file_bsock) {
         ua->jcr->file_bsock->close();
         ua->jcr->file_bsock = NULL;
      }
      return;
   }
   Dmsg0(20, _("Connected to file daemon\n"));
   fd = ua->jcr->file_bsock;
   if (cmd) {
      fd->fsend(".status %s", cmd);
   } else {
      fd->fsend("status");
   }
   while (fd->recv() >= 0) {
      ua->send_msg("%s", fd->msg);
   }
   fd->signal(BNET_TERMINATE);
   fd->close();
   ua->jcr->file_bsock = NULL;

   return;
}
