/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 *   Bacula Director daemon -- this is the main program
 *
 *     Kern Sibbald, March MM
 *
 */

#include "bacula.h"
#include "dird.h"
#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif
#ifndef HAVE_READDIR_R
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif


#ifdef HAVE_PYTHON

#undef _POSIX_C_SOURCE
#include <Python.h>

#include "lib/pythonlib.h"

/* Imported Functions */
extern PyObject *job_getattr(PyObject *self, char *attrname);
extern int job_setattr(PyObject *self, char *attrname, PyObject *value);

#endif /* HAVE_PYTHON */

/* Forward referenced subroutines */
void terminate_dird(int sig);
static bool check_resources();
static void cleanup_old_files();
  
/* Exported subroutines */
extern "C" void reload_config(int sig);
extern void invalidate_schedules();
extern bool parse_dir_config(CONFIG *config, const char *configfile, int exit_code);

/* Imported subroutines */
JCR *wait_for_next_job(char *runjob);
void term_scheduler();
void term_ua_server();
void start_UA_server(dlist *addrs);
void init_job_server(int max_workers);
void term_job_server();
void store_jobtype(LEX *lc, RES_ITEM *item, int index, int pass);
void store_level(LEX *lc, RES_ITEM *item, int index, int pass);
void store_replace(LEX *lc, RES_ITEM *item, int index, int pass);
void store_migtype(LEX *lc, RES_ITEM *item, int index, int pass);
void init_device_resources();

static char *runjob = NULL;
static bool background = true;
static void init_reload(void);
static CONFIG *config;
 
/* Globals Exported */
DIRRES *director;                     /* Director resource */
int FDConnectTimeout;
int SDConnectTimeout;
char *configfile = NULL;
void *start_heap;

/* Globals Imported */
extern RES_ITEM job_items[];
#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
   extern URES res_all;
}
#else
extern URES res_all;
#endif

typedef enum {
   CHECK_CONNECTION,  /* Check catalog connection */
   UPDATE_CATALOG,    /* Ensure that catalog is ok with conf */
   UPDATE_AND_FIX     /* Ensure that catalog is ok, and fix old jobs */
} cat_op;
static bool check_catalog(cat_op mode);

#define CONFIG_FILE "bacula-dir.conf" /* default configuration file */

/*
 * This allows the message handler to operate on the database
 *   by using a pointer to this function. The pointer is
 *   needed because the other daemons do not have access
 *   to the database.  If the pointer is
 *   not defined (other daemons), then writing the database
 *   is disabled.
 */
static bool dir_sql_query(JCR *jcr, const char *cmd)
{
   if (!jcr || !jcr->db || !jcr->db->is_connected()) {
      return false;
   }

   return db_sql_query(jcr->db, cmd);
}

static bool dir_sql_escape(JCR *jcr, B_DB *mdb, char *snew, char *old, int len)
{
   if (!jcr || !jcr->db || !jcr->db->is_connected()) {
      return false;
   }

   db_escape_string(jcr, mdb, snew, old, len);
   return true;
}

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bacula-dir [-f -s] [-c config_file] [-d debug_level] [config_file]\n"
"       -c <file>   set configuration file to file\n"
"       -d <nn>     set debug level to <nn>\n"
"       -dt         print timestamp in debug output\n"
"       -f          run in foreground (for debugging)\n"
"       -g          groupid\n"
"       -m          print kaboom output (for debugging)\n"
"       -r <job>    run <job> now\n"
"       -s          no signals\n"
"       -t          test - read configuration and exit\n"
"       -u          userid\n"
"       -v          verbose user messages\n"
"       -?          print this message.\n"
"\n"), 2000, VERSION, BDATE);

   exit(1);
}


/*********************************************************************
 *
 *         Main Bacula Director Server program
 *
 */
#if defined(HAVE_WIN32)
/* For Win32 main() is in src/win32 code ... */
#define main BaculaMain
#endif

int main (int argc, char *argv[])
{
   int ch;
   JCR *jcr;
   bool no_signals = false;
   bool test_config = false;
   char *uid = NULL;
   char *gid = NULL;
#ifdef HAVE_PYTHON
   init_python_interpreter_args python_args;
#endif /* HAVE_PYTHON */

   start_heap = sbrk(0);
   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");

   init_stack_dump();
   my_name_is(argc, argv, "bacula-dir");
   init_msg(NULL, NULL);              /* initialize message handler */
   init_reload();
   daemon_start_time = time(NULL);

   console_command = run_console_command;

   while ((ch = getopt(argc, argv, "c:d:fg:mr:stu:v?")) != -1) {
      switch (ch) {
      case 'c':                    /* specify config file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'd':                    /* set debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         Dmsg1(10, "Debug level = %d\n", debug_level);
         break;

      case 'f':                    /* run in foreground */
         background = false;
         break;

      case 'g':                    /* set group id */
         gid = optarg;
         break;

      case 'm':                    /* print kaboom output */
         prt_kaboom = true;
         break;

      case 'r':                    /* run job */
         if (runjob != NULL) {
            free(runjob);
         }
         if (optarg) {
            runjob = bstrdup(optarg);
         }
         break;

      case 's':                    /* turn off signals */
         no_signals = true;
         break;

      case 't':                    /* test config */
         test_config = true;
         break;

      case 'u':                    /* set uid */
         uid = optarg;
         break;

      case 'v':                    /* verbose */
         verbose++;
         break;

      case '?':
      default:
         usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (!no_signals) {
      init_signals(terminate_dird);
   }

   if (argc) {
      if (configfile != NULL) {
         free(configfile);
      }
      configfile = bstrdup(*argv);
      argc--;
      argv++;
   }
   if (argc) {
      usage();
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   config = new_config_parser();
   parse_dir_config(config, configfile, M_ERROR_TERM);

   if (init_crypto() != 0) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
   }

   if (!check_resources()) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
   }

   if (!test_config) {                /* we don't need to do this block in test mode */
      if (background) {
         daemon_start();
         init_stack_dump();              /* grab new pid */
      }   
      /* Create pid must come after we are a daemon -- so we have our final pid */
      create_pid_file(director->pid_directory, "bacula-dir", 
                      get_first_port_host_order(director->DIRaddrs));
      read_state_file(director->working_directory, "bacula-dir",
                      get_first_port_host_order(director->DIRaddrs));
   }

   set_jcr_in_tsd(INVALID_JCR);
   set_thread_concurrency(director->MaxConcurrentJobs * 2 +
                          4 /* UA */ + 5 /* sched+watchdog+jobsvr+misc */);
   lmgr_init_thread(); /* initialize the lockmanager stack */

   load_dir_plugins(director->plugin_directory);

   drop(uid, gid, false);                    /* reduce privileges if requested */

   /* If we are in testing mode, we don't try to fix the catalog */
   cat_op mode=(test_config)?CHECK_CONNECTION:UPDATE_AND_FIX;

   if (!check_catalog(mode)) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
   }
   
   if (test_config) {      
      terminate_dird(0);
   }

   my_name_is(0, NULL, director->name());    /* set user defined name */

   cleanup_old_files();

   /* Plug database interface for library routines */
   p_sql_query = (sql_query_func)dir_sql_query;
   p_sql_escape = (sql_escape_func)dir_sql_escape;

   FDConnectTimeout = (int)director->FDConnectTimeout;
   SDConnectTimeout = (int)director->SDConnectTimeout;

#if !defined(HAVE_WIN32)
   signal(SIGHUP, reload_config);
#endif

   init_console_msg(working_directory);

#ifdef HAVE_PYTHON
   python_args.progname = director->name();
   python_args.scriptdir = director->scripts_directory;
   python_args.modulename = "DirStartUp";
   python_args.configfile = configfile;
   python_args.workingdir = director->working_directory;
   python_args.job_getattr = job_getattr;
   python_args.job_setattr = job_setattr;

   init_python_interpreter(&python_args);
#endif /* HAVE_PYTHON */

   Dmsg0(200, "Start UA server\n");
   start_UA_server(director->DIRaddrs);

   start_watchdog();                  /* start network watchdog thread */

   init_jcr_subsystem();              /* start JCR watchdogs etc. */

   init_job_server(director->MaxConcurrentJobs);

   dbg_jcr_add_hook(db_debug_print); /* used to debug B_DB connexion after fatal signal */

//   init_device_resources();

   Dmsg0(200, "wait for next job\n");
   /* Main loop -- call scheduler to get next job to run */
   while ( (jcr = wait_for_next_job(runjob)) ) {
      run_job(jcr);                   /* run job */
      free_jcr(jcr);                  /* release jcr */
      set_jcr_in_tsd(INVALID_JCR);
      if (runjob) {                   /* command line, run a single job? */
         break;                       /* yes, terminate */
      }
   }

   terminate_dird(0);

   return 0;
}

/* Cleanup and then exit */
void terminate_dird(int sig)
{
   static bool already_here = false;

   if (already_here) {                /* avoid recursive temination problems */
      bmicrosleep(2, 0);              /* yield */
      exit(1);
   }
   already_here = true;
   debug_level = 0;                   /* turn off debug */
   stop_watchdog();
   generate_daemon_event(NULL, "Exit");
   unload_plugins();
   write_state_file(director->working_directory, "bacula-dir", get_first_port_host_order(director->DIRaddrs));
   delete_pid_file(director->pid_directory, "bacula-dir", get_first_port_host_order(director->DIRaddrs));
   term_scheduler();
   term_job_server();
   if (runjob) {
      free(runjob);
   }
   if (configfile != NULL) {
      free(configfile);
   }
   if (debug_level > 5) {
      print_memory_pool_stats();
   }
   if (config) {
      config->free_resources();
      free(config);
      config = NULL;
   }
   term_ua_server();
   term_msg();                        /* terminate message handler */
   cleanup_crypto();
   close_memory_pool();               /* release free memory in pool */
   lmgr_cleanup_main();
   sm_dump(false);
   exit(sig);
}

struct RELOAD_TABLE {
   int job_count;
   RES **res_table;
};

static const int max_reloads = 32;
static RELOAD_TABLE reload_table[max_reloads];

static void init_reload(void)
{
   for (int i=0; i < max_reloads; i++) {
      reload_table[i].job_count = 0;
      reload_table[i].res_table = NULL;
   }
}

static void free_saved_resources(int table)
{
   int num = r_last - r_first + 1;
   RES **res_tab = reload_table[table].res_table;
   if (!res_tab) {
      Dmsg1(100, "res_tab for table %d already released.\n", table);
      return;
   }
   Dmsg1(100, "Freeing resources for table %d\n", table);
   for (int j=0; j<num; j++) {
      free_resource(res_tab[j], r_first + j);
   }
   free(res_tab);
   reload_table[table].job_count = 0;
   reload_table[table].res_table = NULL;
}

/*
 * Called here at the end of every job that was
 * hooked decrementing the active job_count. When
 * it goes to zero, no one is using the associated
 * resource table, so free it.
 */
static void reload_job_end_cb(JCR *jcr, void *ctx)
{
   int reload_id = (int)((intptr_t)ctx);
   Dmsg3(100, "reload job_end JobId=%d table=%d cnt=%d\n", jcr->JobId,
      reload_id, reload_table[reload_id].job_count);
   lock_jobs();
   LockRes();
   if (--reload_table[reload_id].job_count <= 0) {
      free_saved_resources(reload_id);
   }
   UnlockRes();
   unlock_jobs();
}

static int find_free_reload_table_entry()
{
   int table = -1;
   for (int i=0; i < max_reloads; i++) {
      if (reload_table[i].res_table == NULL) {
         table = i;
         break;
      }
   }
   return table;
}

/*
 * If we get here, we have received a SIGHUP, which means to
 *    reread our configuration file.
 *
 * The algorithm used is as follows: we count how many jobs are
 *   running and mark the running jobs to make a callback on
 *   exiting. The old config is saved with the reload table
 *   id in a reload table. The new config file is read. Now, as
 *   each job exits, it calls back to the reload_job_end_cb(), which
 *   decrements the count of open jobs for the given reload table.
 *   When the count goes to zero, we release those resources.
 *   This allows us to have pointers into the resource table (from
 *   jobs), and once they exit and all the pointers are released, we
 *   release the old table. Note, if no new jobs are running since the
 *   last reload, then the old resources will be immediately release.
 *   A console is considered a job because it may have pointers to
 *   resources, but a SYSTEM job is not since it *should* not have any
 *   permanent pointers to jobs.
 */
extern "C"
void reload_config(int sig)
{
   static bool already_here = false;
#if !defined(HAVE_WIN32)
   sigset_t set;
#endif
   JCR *jcr;
   int njobs = 0;                     /* number of running jobs */
   int table, rtable;
   bool ok;       

   if (already_here) {
      abort();                        /* Oops, recursion -> die */
   }
   already_here = true;

#if !defined(HAVE_WIN32)
   sigemptyset(&set);
   sigaddset(&set, SIGHUP);
   sigprocmask(SIG_BLOCK, &set, NULL);
#endif

   lock_jobs();
   LockRes();

   table = find_free_reload_table_entry();
   if (table < 0) {
      Jmsg(NULL, M_ERROR, 0, _("Too many open reload requests. Request ignored.\n"));
      goto bail_out;
   }

   Dmsg1(100, "Reload_config njobs=%d\n", njobs);
   reload_table[table].res_table = config->save_resources();
   Dmsg1(100, "Saved old config in table %d\n", table);

   ok = parse_dir_config(config, configfile, M_ERROR);

   Dmsg0(100, "Reloaded config file\n");
   if (!ok || !check_resources() || !check_catalog(UPDATE_CATALOG)) {
      rtable = find_free_reload_table_entry();    /* save new, bad table */
      if (rtable < 0) {
         Jmsg(NULL, M_ERROR, 0, _("Please correct configuration file: %s\n"), configfile);
         Jmsg(NULL, M_ERROR_TERM, 0, _("Out of reload table entries. Giving up.\n"));
      } else {
         Jmsg(NULL, M_ERROR, 0, _("Please correct configuration file: %s\n"), configfile);
         Jmsg(NULL, M_ERROR, 0, _("Resetting previous configuration.\n"));
      }
      reload_table[rtable].res_table = config->save_resources();
      /* Now restore old resource values */
      int num = r_last - r_first + 1;
      RES **res_tab = reload_table[table].res_table;
      for (int i=0; i<num; i++) {
         res_head[i] = res_tab[i];
      }
      table = rtable;                 /* release new, bad, saved table below */
   } else {
      invalidate_schedules();
      /*
       * Hook all active jobs so that they release this table
       */
      foreach_jcr(jcr) {
         if (jcr->getJobType() != JT_SYSTEM) {
            reload_table[table].job_count++;
            job_end_push(jcr, reload_job_end_cb, (void *)((long int)table));
            njobs++;
         }
      }
      endeach_jcr(jcr);
   }

   /* Reset globals */
   set_working_directory(director->working_directory);
   FDConnectTimeout = director->FDConnectTimeout;
   SDConnectTimeout = director->SDConnectTimeout;
   Dmsg0(10, "Director's configuration file reread.\n");

   /* Now release saved resources, if no jobs using the resources */
   if (njobs == 0) {
      free_saved_resources(table);
   }

bail_out:
   UnlockRes();
   unlock_jobs();
#if !defined(HAVE_WIN32)
   sigprocmask(SIG_UNBLOCK, &set, NULL);
   signal(SIGHUP, reload_config);
#endif
   already_here = false;
}

/*
 * Make a quick check to see that we have all the
 * resources needed.
 *
 *  **** FIXME **** this routine could be a lot more
 *   intelligent and comprehensive.
 */
static bool check_resources()
{
   bool OK = true;
   JOB *job;
   bool need_tls;

   LockRes();

   job = (JOB *)GetNextRes(R_JOB, NULL);
   director = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   if (!director) {
      Jmsg(NULL, M_FATAL, 0, _("No Director resource defined in %s\n"
"Without that I don't know who I am :-(\n"), configfile);
      OK = false;
   } else {
      set_working_directory(director->working_directory);
      if (!director->messages) {       /* If message resource not specified */
         director->messages = (MSGS *)GetNextRes(R_MSGS, NULL);
         if (!director->messages) {
            Jmsg(NULL, M_FATAL, 0, _("No Messages resource defined in %s\n"), configfile);
            OK = false;
         }
      }
      if (GetNextRes(R_DIRECTOR, (RES *)director) != NULL) {
         Jmsg(NULL, M_FATAL, 0, _("Only one Director resource permitted in %s\n"),
            configfile);
         OK = false;
      }
      /* tls_require implies tls_enable */
      if (director->tls_require) {
         if (have_tls) {
            director->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            OK = false;
         }
      }

      need_tls = director->tls_enable || director->tls_authenticate;

      if (!director->tls_certfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Certificate\" file not defined for Director \"%s\" in %s.\n"),
            director->name(), configfile);
         OK = false;
      }

      if (!director->tls_keyfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Key\" file not defined for Director \"%s\" in %s.\n"),
            director->name(), configfile);
         OK = false;
      }

      if ((!director->tls_ca_certfile && !director->tls_ca_certdir) && 
           need_tls && director->tls_verify_peer) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\" or \"TLS CA"
              " Certificate Dir\" are defined for Director \"%s\" in %s."
              " At least one CA certificate store is required"
              " when using \"TLS Verify Peer\".\n"),
              director->name(), configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || director->tls_require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         director->tls_ctx = new_tls_context(director->tls_ca_certfile,
            director->tls_ca_certdir, director->tls_certfile,
            director->tls_keyfile, NULL, NULL, director->tls_dhfile,
            director->tls_verify_peer);
         
         if (!director->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for Director \"%s\" in %s.\n"),
                 director->name(), configfile);
            OK = false;
         }
      }
   }

   if (!job) {
      Jmsg(NULL, M_FATAL, 0, _("No Job records defined in %s\n"), configfile);
      OK = false;
   }
   foreach_res(job, R_JOB) {
      int i;

      if (job->jobdefs) {
         /* Handle Storage alists specifically */
         JOB *jobdefs = job->jobdefs;
         if (jobdefs->storage && !job->storage) {
            STORE *st;
            job->storage = New(alist(10, not_owned_by_alist));
            foreach_alist(st, jobdefs->storage) {
               job->storage->append(st);
            }
         }
         /* Handle RunScripts alists specifically */
         if (jobdefs->RunScripts) {
            RUNSCRIPT *rs, *elt;
            
            if (!job->RunScripts) {
               job->RunScripts = New(alist(10, not_owned_by_alist));
            }
           
            foreach_alist(rs, jobdefs->RunScripts) {
               elt = copy_runscript(rs);
               job->RunScripts->append(elt); /* we have to free it */
            }
         }

         /* Transfer default items from JobDefs Resource */
         for (i=0; job_items[i].name; i++) {
            char **def_svalue, **svalue;  /* string value */
            uint32_t *def_ivalue, *ivalue;     /* integer value */
            bool *def_bvalue, *bvalue;    /* bool value */
            int64_t *def_lvalue, *lvalue; /* 64 bit values */
            uint32_t offset;

            Dmsg4(1400, "Job \"%s\", field \"%s\" bit=%d def=%d\n",
                job->name(), job_items[i].name,
                bit_is_set(i, job->hdr.item_present),
                bit_is_set(i, job->jobdefs->hdr.item_present));

            if (!bit_is_set(i, job->hdr.item_present) &&
                 bit_is_set(i, job->jobdefs->hdr.item_present)) {
               Dmsg2(400, "Job \"%s\", field \"%s\": getting default.\n",
                 job->name(), job_items[i].name);
               offset = (char *)(job_items[i].value) - (char *)&res_all;
               /*
                * Handle strings and directory strings
                */
               if (job_items[i].handler == store_str ||
                   job_items[i].handler == store_dir) {
                  def_svalue = (char **)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_svalue=%s item %d offset=%u\n",
                       job->name(), job_items[i].name, *def_svalue, i, offset);
                  svalue = (char **)((char *)job + offset);
                  if (*svalue) {
                     Pmsg1(000, _("Hey something is wrong. p=0x%lu\n"), *svalue);
                  }
                  *svalue = bstrdup(*def_svalue);
                  set_bit(i, job->hdr.item_present);
               /*
                * Handle resources
                */
               } else if (job_items[i].handler == store_res) {
                  def_svalue = (char **)((char *)(job->jobdefs) + offset);
                  Dmsg4(400, "Job \"%s\", field \"%s\" item %d offset=%u\n",
                       job->name(), job_items[i].name, i, offset);
                  svalue = (char **)((char *)job + offset);
                  if (*svalue) {
                     Pmsg1(000, _("Hey something is wrong. p=0x%lu\n"), *svalue);
                  }
                  *svalue = *def_svalue;
                  set_bit(i, job->hdr.item_present);
               /*
                * Handle alist resources
                */
               } else if (job_items[i].handler == store_alist_res) {
                  if (bit_is_set(i, job->jobdefs->hdr.item_present)) {
                     set_bit(i, job->hdr.item_present);
                  }
               /*
                * Handle integer fields
                *    Note, our store_bit does not handle bitmaped fields
                */
               } else if (job_items[i].handler == store_bit     ||
                          job_items[i].handler == store_pint32  ||
                          job_items[i].handler == store_jobtype ||
                          job_items[i].handler == store_level   ||
                          job_items[i].handler == store_int32   ||
                          job_items[i].handler == store_size32  ||
                          job_items[i].handler == store_migtype ||
                          job_items[i].handler == store_replace) {
                  def_ivalue = (uint32_t *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_ivalue=%d item %d offset=%u\n",
                       job->name(), job_items[i].name, *def_ivalue, i, offset);
                  ivalue = (uint32_t *)((char *)job + offset);
                  *ivalue = *def_ivalue;
                  set_bit(i, job->hdr.item_present);
               /*
                * Handle 64 bit integer fields
                */
               } else if (job_items[i].handler == store_time   ||
                          job_items[i].handler == store_size64 ||
                          job_items[i].handler == store_int64) {
                  def_lvalue = (int64_t *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_lvalue=%" lld " item %d offset=%u\n",
                       job->name(), job_items[i].name, *def_lvalue, i, offset);
                  lvalue = (int64_t *)((char *)job + offset);
                  *lvalue = *def_lvalue;
                  set_bit(i, job->hdr.item_present);
               /*
                * Handle bool fields
                */
               } else if (job_items[i].handler == store_bool) {
                  def_bvalue = (bool *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_bvalue=%d item %d offset=%u\n",
                       job->name(), job_items[i].name, *def_bvalue, i, offset);
                  bvalue = (bool *)((char *)job + offset);
                  *bvalue = *def_bvalue;
                  set_bit(i, job->hdr.item_present);
               }
            }
         }
      }
      /*
       * Ensure that all required items are present
       */
      for (i=0; job_items[i].name; i++) {
         if (job_items[i].flags & ITEM_REQUIRED) {
               if (!bit_is_set(i, job->hdr.item_present)) {
                  Jmsg(NULL, M_ERROR_TERM, 0, _("\"%s\" directive in Job \"%s\" resource is required, but not found.\n"),
                    job_items[i].name, job->name());
                  OK = false;
                }
         }
         /* If this triggers, take a look at lib/parse_conf.h */
         if (i >= MAX_RES_ITEMS) {
            Emsg0(M_ERROR_TERM, 0, _("Too many items in Job resource\n"));
         }
      }
      if (!job->storage && !job->pool->storage) {
         Jmsg(NULL, M_FATAL, 0, _("No storage specified in Job \"%s\" nor in Pool.\n"),
            job->name());
         OK = false;
      }
   } /* End loop over Job res */


   /* Loop over Consoles */
   CONRES *cons;
   foreach_res(cons, R_CONSOLE) {
      /* tls_require implies tls_enable */
      if (cons->tls_require) {
         if (have_tls) {
            cons->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            OK = false;
            continue;
         }
      }

      need_tls = cons->tls_enable || cons->tls_authenticate;
      
      if (!cons->tls_certfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Certificate\" file not defined for Console \"%s\" in %s.\n"),
            cons->name(), configfile);
         OK = false;
      }

      if (!cons->tls_keyfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Key\" file not defined for Console \"%s\" in %s.\n"),
            cons->name(), configfile);
         OK = false;
      }

      if ((!cons->tls_ca_certfile && !cons->tls_ca_certdir) 
            && need_tls && cons->tls_verify_peer) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\" or \"TLS CA"
            " Certificate Dir\" are defined for Console \"%s\" in %s."
            " At least one CA certificate store is required"
            " when using \"TLS Verify Peer\".\n"),
            cons->name(), configfile);
         OK = false;
      }
      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || cons->tls_require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         cons->tls_ctx = new_tls_context(cons->tls_ca_certfile,
            cons->tls_ca_certdir, cons->tls_certfile,
            cons->tls_keyfile, NULL, NULL, cons->tls_dhfile, cons->tls_verify_peer);
         
         if (!cons->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for File daemon \"%s\" in %s.\n"),
               cons->name(), configfile);
            OK = false;
         }
      }

   }

   /* Loop over Clients */
   CLIENT *client;
   foreach_res(client, R_CLIENT) {
      /* tls_require implies tls_enable */
      if (client->tls_require) {
         if (have_tls) {
            client->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            OK = false;
            continue;
         }
      }
      need_tls = client->tls_enable || client->tls_authenticate;
      if ((!client->tls_ca_certfile && !client->tls_ca_certdir) && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
            " or \"TLS CA Certificate Dir\" are defined for File daemon \"%s\" in %s.\n"),
            client->name(), configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || client->tls_require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         client->tls_ctx = new_tls_context(client->tls_ca_certfile,
            client->tls_ca_certdir, client->tls_certfile,
            client->tls_keyfile, NULL, NULL, NULL,
            true);
         
         if (!client->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for File daemon \"%s\" in %s.\n"),
               client->name(), configfile);
            OK = false;
         }
      }
   }

   /* Loop over Storages */
   STORE *store;
   foreach_res(store, R_STORAGE) {
      /* tls_require implies tls_enable */
      if (store->tls_require) {
         if (have_tls) {
            store->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            OK = false;
            continue;
         }
      }

      need_tls = store->tls_enable || store->tls_authenticate;

      if ((!store->tls_ca_certfile && !store->tls_ca_certdir) && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
              " or \"TLS CA Certificate Dir\" are defined for Storage \"%s\" in %s.\n"),
              store->name(), configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || store->tls_require)) {
        /* Initialize TLS context:
         * Args: CA certfile, CA certdir, Certfile, Keyfile,
         * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         store->tls_ctx = new_tls_context(store->tls_ca_certfile,
            store->tls_ca_certdir, store->tls_certfile,
            store->tls_keyfile, NULL, NULL, NULL, true);

         if (!store->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for Storage \"%s\" in %s.\n"),
                 store->name(), configfile);
            OK = false;
         }
      }
   }

   UnlockRes();
   if (OK) {
      close_msg(NULL);                /* close temp message handler */
      init_msg(NULL, director->messages); /* open daemon message handler */
   }
   return OK;
}

/* 
 * In this routine, 
 *  - we can check the connection (mode=CHECK_CONNECTION)
 *  - we can synchronize the catalog with the configuration (mode=UPDATE_CATALOG)
 *  - we can synchronize, and fix old job records (mode=UPDATE_AND_FIX)
 */
static bool check_catalog(cat_op mode)
{
   bool OK = true;

   /* Loop over databases */
   CAT *catalog;
   foreach_res(catalog, R_CATALOG) {
      B_DB *db;
      /*
       * Make sure we can open catalog, otherwise print a warning
       * message because the server is probably not running.
       */
      db = db_init_database(NULL, catalog->db_driver, catalog->db_name, catalog->db_user,
                            catalog->db_password, catalog->db_address,
                            catalog->db_port, catalog->db_socket,
                            catalog->mult_db_connections,
                            catalog->disable_batch_insert);
      if (!db || !db_open_database(NULL, db)) {
         Pmsg2(000, _("Could not open Catalog \"%s\", database \"%s\".\n"),
              catalog->name(), catalog->db_name);
         Jmsg(NULL, M_FATAL, 0, _("Could not open Catalog \"%s\", database \"%s\".\n"),
              catalog->name(), catalog->db_name);
         if (db) {
            Jmsg(NULL, M_FATAL, 0, _("%s"), db_strerror(db));
            Pmsg1(000, "%s", db_strerror(db));
            db_close_database(NULL, db);
         }
         OK = false;
         continue;
      }

      /* Display a message if the db max_connections is too low */
      if (!db_check_max_connections(NULL, db, director->MaxConcurrentJobs)) {
         Pmsg1(000, "Warning, settings problem for Catalog=%s\n", catalog->name());
         Pmsg1(000, "%s", db_strerror(db));
      }

      /* we are in testing mode, so don't touch anything in the catalog */
      if (mode == CHECK_CONNECTION) {
         db_close_database(NULL, db);
         continue;
      }

      /* Loop over all pools, defining/updating them in each database */
      POOL *pool;
      foreach_res(pool, R_POOL) {
         /*
          * If the Pool has a catalog resource create the pool only
          *   in that catalog.
          */
         if (!pool->catalog || pool->catalog == catalog) {
            create_pool(NULL, db, pool, POOL_OP_UPDATE);  /* update request */
         }
      }

      /* Once they are created, we can loop over them again, updating
       * references (RecyclePool)
       */
      foreach_res(pool, R_POOL) {
         /*
          * If the Pool has a catalog resource update the pool only
          *   in that catalog.
          */
         if (!pool->catalog || pool->catalog == catalog) {
            update_pool_references(NULL, db, pool);
         }
      }

      /* Ensure basic client record is in DB */
      CLIENT *client;
      foreach_res(client, R_CLIENT) {
         CLIENT_DBR cr;
         /* Create clients only if they use the current catalog */
         if (client->catalog != catalog) {
            Dmsg3(500, "Skip client=%s with cat=%s not catalog=%s\n",
                  client->name(), client->catalog->name(), catalog->name());
            continue;
         }
         Dmsg2(500, "create cat=%s for client=%s\n", 
               client->catalog->name(), client->name());
         memset(&cr, 0, sizeof(cr));
         bstrncpy(cr.Name, client->name(), sizeof(cr.Name));
         db_create_client_record(NULL, db, &cr);
      }

      /* Ensure basic storage record is in DB */
      STORE *store;
      foreach_res(store, R_STORAGE) {
         STORAGE_DBR sr;
         MEDIATYPE_DBR mtr;
         memset(&sr, 0, sizeof(sr));
         memset(&mtr, 0, sizeof(mtr));
         if (store->media_type) {
            bstrncpy(mtr.MediaType, store->media_type, sizeof(mtr.MediaType));
            mtr.ReadOnly = 0;
            db_create_mediatype_record(NULL, db, &mtr);
         } else {
            mtr.MediaTypeId = 0;
         }
         bstrncpy(sr.Name, store->name(), sizeof(sr.Name));
         sr.AutoChanger = store->autochanger;
         if (!db_create_storage_record(NULL, db, &sr)) {
            Jmsg(NULL, M_FATAL, 0, _("Could not create storage record for %s\n"), 
                 store->name());
            OK = false;
         }
         store->StorageId = sr.StorageId;   /* set storage Id */
         if (!sr.created) {                 /* if not created, update it */
            sr.AutoChanger = store->autochanger;
            if (!db_update_storage_record(NULL, db, &sr)) {
               Jmsg(NULL, M_FATAL, 0, _("Could not update storage record for %s\n"),
                    store->name());
               OK = false;
            }
         }
      }

      /* Loop over all counters, defining them in each database */
      /* Set default value in all counters */
      COUNTER *counter;
      foreach_res(counter, R_COUNTER) {
         /* Write to catalog? */
         if (!counter->created && counter->Catalog == catalog) {
            COUNTER_DBR cr;
            bstrncpy(cr.Counter, counter->name(), sizeof(cr.Counter));
            cr.MinValue = counter->MinValue;
            cr.MaxValue = counter->MaxValue;
            cr.CurrentValue = counter->MinValue;
            if (counter->WrapCounter) {
               bstrncpy(cr.WrapCounter, counter->WrapCounter->name(), sizeof(cr.WrapCounter));
            } else {
               cr.WrapCounter[0] = 0;  /* empty string */
            }
            if (db_create_counter_record(NULL, db, &cr)) {
               counter->CurrentValue = cr.CurrentValue;
               counter->created = true;
               Dmsg2(100, "Create counter %s val=%d\n", counter->name(), counter->CurrentValue);
            }
         }
         if (!counter->created) {
            counter->CurrentValue = counter->MinValue;  /* default value */
         }
      }
      /* cleanup old job records */
      if (mode == UPDATE_AND_FIX) {
         db_sql_query(db, cleanup_created_job, NULL, NULL);
         db_sql_query(db, cleanup_running_job, NULL, NULL);
      }

      /* Set type in global for debugging */
      set_db_type(db_get_type(db));

      db_close_database(NULL, db);
   }
   return OK;
}

static void cleanup_old_files()
{
   DIR* dp;
   struct dirent *entry, *result;
   int rc, name_max;
   int my_name_len = strlen(my_name);
   int len = strlen(director->working_directory);
   POOLMEM *cleanup = get_pool_memory(PM_MESSAGE);
   POOLMEM *basename = get_pool_memory(PM_MESSAGE);
   regex_t preg1;
   char prbuf[500];
   const int nmatch = 30;
   regmatch_t pmatch[nmatch];
   berrno be;

   /* Exclude spaces and look for .mail or .restore.xx.bsr files */
   const char *pat1 = "^[^ ]+\\.(restore\\.[^ ]+\\.bsr|mail)$";

   /* Setup working directory prefix */
   pm_strcpy(basename, director->working_directory);
   if (len > 0 && !IsPathSeparator(director->working_directory[len-1])) {
      pm_strcat(basename, "/");
   }

   /* Compile regex expressions */
   rc = regcomp(&preg1, pat1, REG_EXTENDED);
   if (rc != 0) {
      regerror(rc, &preg1, prbuf, sizeof(prbuf));
      Pmsg2(000,  _("Could not compile regex pattern \"%s\" ERR=%s\n"),
           pat1, prbuf);
      goto get_out2;
   }

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }
      
   if (!(dp = opendir(director->working_directory))) {
      berrno be;
      Pmsg2(000, "Failed to open working dir %s for cleanup: ERR=%s\n", 
            director->working_directory, be.bstrerror());
      goto get_out1;
      return;
   }

   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   while (1) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
         break;
      }
      /* Exclude any name with ., .., not my_name or containing a space */
      if (strcmp(result->d_name, ".") == 0 || strcmp(result->d_name, "..") == 0 ||
          strncmp(result->d_name, my_name, my_name_len) != 0) {
         Dmsg1(500, "Skipped: %s\n", result->d_name);
         continue;    
      }

      /* Unlink files that match regexes */
      if (regexec(&preg1, result->d_name, nmatch, pmatch,  0) == 0) {
         pm_strcpy(cleanup, basename);
         pm_strcat(cleanup, result->d_name);
         Dmsg1(100, "Unlink: %s\n", cleanup);
         unlink(cleanup);
      }
   }

   free(entry);
   closedir(dp);
/* Be careful to free up the correct resources */
get_out1:
   regfree(&preg1);
get_out2:
   free_pool_memory(cleanup);
   free_pool_memory(basename);
}
