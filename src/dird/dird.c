/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * BAREOS Director daemon -- this is the main program
 *
 * Kern Sibbald, March MM
 */

#include "bareos.h"
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

/* Forward referenced subroutines */
#if !defined(HAVE_WIN32)
static
#endif
void terminate_dird(int sig);
static bool check_resources();
static bool initialize_sql_pooling(void);
static void cleanup_old_files();

/* Exported subroutines */
extern "C" void reload_config(int sig);
extern void invalidate_schedules();
extern bool parse_dir_config(CONFIG *config, const char *configfile, int exit_code);

/* Imported subroutines */
void start_UA_server(dlist *addrs);
void stop_UA_server(void);
void init_job_server(int max_workers);
void term_job_server();
void store_jobtype(LEX *lc, RES_ITEM *item, int index, int pass);
void store_protocoltype(LEX *lc, RES_ITEM *item, int index, int pass);
void store_level(LEX *lc, RES_ITEM *item, int index, int pass);
void store_replace(LEX *lc, RES_ITEM *item, int index, int pass);
void store_migtype(LEX *lc, RES_ITEM *item, int index, int pass);
void init_device_resources();

static char *runjob = NULL;
static bool background = true;
static bool test_config = false;
static void init_reload(void);

/* Globals Exported */
DIRRES *me = NULL;                    /* Our Global resource */
CONFIG *my_config = NULL;             /* Our Global config */
char *configfile = NULL;
void *start_heap;

/* Globals Imported */
extern RES_ITEM job_items[];

typedef enum {
   CHECK_CONNECTION,  /* Check catalog connection */
   UPDATE_CATALOG,    /* Ensure that catalog is ok with conf */
   UPDATE_AND_FIX     /* Ensure that catalog is ok, and fix old jobs */
} cat_op;
static bool check_catalog(cat_op mode);

#define CONFIG_FILE "bareos-dir.conf" /* default configuration file */

/*
 * This allows the message handler to operate on the database by using a pointer
 * to this function. The pointer is needed because the other daemons do not have
 * access to the database. If the pointer is not defined (other daemons), then
 * writing the database is disabled.
 */
static bool dir_db_log_insert(JCR *jcr, utime_t mtime, char *msg)
{
   int length;
   char ed1[50];
   char dt[MAX_TIME_LENGTH];
   POOL_MEM query(PM_MESSAGE),
            esc_msg(PM_MESSAGE);

   if (!jcr || !jcr->db || !jcr->db->is_connected()) {
      return false;
   }
   length = strlen(msg);
   esc_msg.check_size(length * 2 + 1);
   db_escape_string(jcr, jcr->db, esc_msg.c_str(), msg, length);

   bstrutime(dt, sizeof(dt), mtime);
   Mmsg(query, "INSERT INTO Log (JobId, Time, LogText) VALUES (%s,'%s','%s')",
        edit_int64(jcr->JobId, ed1), dt, esc_msg.c_str());

   return db_sql_query(jcr->db, query.c_str());
}

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bareos-dir [-f -s] [-c config_file] [-d debug_level] [config_file]\n"
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
"       -x          print configuration file schema in JSON format and exit\n"
"       -?          print this message.\n"
"\n"), 2000, VERSION, BDATE);

   exit(1);
}


/*********************************************************************
 *
 *         Main BAREOS Director Server program
 *
 */
#if defined(HAVE_WIN32)
#define main BareosMain
#endif

int main (int argc, char *argv[])
{
   int ch;
   JCR *jcr;
   cat_op mode;
   bool no_signals = false;
   bool export_config_schema = false;
   char *uid = NULL;
   char *gid = NULL;

   start_heap = sbrk(0);
   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   init_stack_dump();
   my_name_is(argc, argv, "bareos-dir");
   init_msg(NULL, NULL);              /* initialize message handler */
   init_reload();
   daemon_start_time = time(NULL);

   console_command = run_console_command;

   while ((ch = getopt(argc, argv, "c:d:fg:mr:stu:vx?")) != -1) {
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

      case 'x':                    /* export configuration file schema (json) and exit */
         export_config_schema = true;
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

   /*
    * See if we want to drop privs.
    */
   if (geteuid() == 0) {
      drop(uid, gid, false);                    /* reduce privileges if requested */
   }

   if (export_config_schema) {
      my_config = new_config_parser();
      init_dir_config(my_config, configfile, M_ERROR_TERM);
      POOL_MEM buffer;
      print_config_schema_json(buffer);
      printf( "%s\n", buffer.c_str() );
      goto bail_out;
   }

   my_config = new_config_parser();
   parse_dir_config(my_config, configfile, M_ERROR_TERM);

   if (!test_config) {                /* we don't need to do this block in test mode */
      if (background) {
         daemon_start();
         init_stack_dump();              /* grab new pid */
      }
   }

   if (init_crypto() != 0) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
      goto bail_out;
   }

   if (!check_resources()) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
      goto bail_out;
   }

   if (!test_config) {                /* we don't need to do this block in test mode */
      /* Create pid must come after we are a daemon -- so we have our final pid */
      create_pid_file(me->pid_directory, "bareos-dir",
                      get_first_port_host_order(me->DIRaddrs));
      read_state_file(me->working_directory, "bareos-dir",
                      get_first_port_host_order(me->DIRaddrs));
   }

   set_jcr_in_tsd(INVALID_JCR);
   set_thread_concurrency(me->MaxConcurrentJobs * 2 +
                          4 /* UA */ + 5 /* sched+watchdog+jobsvr+misc */);
   lmgr_init_thread(); /* initialize the lockmanager stack */

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   char *backend_dir;

   foreach_alist(backend_dir, me->backend_directories) {
        Dmsg1(100, "backend path: %s\n", backend_dir);
   }

   db_set_backend_dirs(me->backend_directories);
#endif

   load_dir_plugins(me->plugin_directory, me->plugin_names);

   /*
    * If we are in testing mode, we don't try to fix the catalog
    */
   mode = (test_config) ? CHECK_CONNECTION : UPDATE_AND_FIX;

   if (!check_catalog(mode)) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
      goto bail_out;
   }

   if (test_config) {
      terminate_dird(0);
   }

   if (!initialize_sql_pooling()) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
      goto bail_out;
   }

   my_name_is(0, NULL, me->name());    /* set user defined name */

   cleanup_old_files();

   p_db_log_insert = (db_log_insert_func)dir_db_log_insert;

#if !defined(HAVE_WIN32)
   signal(SIGHUP, reload_config);
#endif

   init_console_msg(working_directory);

   Dmsg0(200, "Start UA server\n");
   start_UA_server(me->DIRaddrs);

   start_watchdog();                  /* start network watchdog thread */

   if (me->jcr_watchdog_time) {
      init_jcr_subsystem(me->jcr_watchdog_time); /* start JCR watchdogs etc. */
   }

   init_job_server(me->MaxConcurrentJobs);

   dbg_jcr_add_hook(db_debug_print); /* used to debug B_DB connexion after fatal signal */

//   init_device_resources();

   start_statistics_thread();

   Dmsg0(200, "wait for next job\n");
   /* Main loop -- call scheduler to get next job to run */
   while ((jcr = wait_for_next_job(runjob))) {
      run_job(jcr);                   /* run job */
      free_jcr(jcr);                  /* release jcr */
      set_jcr_in_tsd(INVALID_JCR);
      if (runjob) {                   /* command line, run a single job? */
         break;                       /* yes, terminate */
      }
   }

   terminate_dird(0);

bail_out:
   return 0;
}

/* Cleanup and then exit */
#if !defined(HAVE_WIN32)
static
#endif
void terminate_dird(int sig)
{
   static bool already_here = false;

   if (already_here) {                /* avoid recursive temination problems */
      bmicrosleep(2, 0);              /* yield */
      exit(1);
   }

   already_here = true;
   debug_level = 0;                   /* turn off debug */

   stop_statistics_thread();
   stop_watchdog();
   db_sql_pool_destroy();
   db_flush_backends();
   unload_dir_plugins();
   if (!test_config) {                /* we don't need to do this block in test mode */
      write_state_file(me->working_directory, "bareos-dir", get_first_port_host_order(me->DIRaddrs));
      delete_pid_file(me->pid_directory, "bareos-dir", get_first_port_host_order(me->DIRaddrs));
   }
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
   if (my_config) {
      my_config->free_resources();
      free(my_config);
      my_config = NULL;
   }

   stop_UA_server();
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
   int num = my_config->m_r_last - my_config->m_r_first + 1;
   RES **res_tab = reload_table[table].res_table;

   if (!res_tab) {
      Dmsg1(100, "res_tab for table %d already released.\n", table);
      return;
   }
   Dmsg1(100, "Freeing resources for table %d\n", table);
   for (int j=0; j<num; j++) {
      free_resource(res_tab[j], my_config->m_r_first + j);
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

   /**
    * Flush the sql connection pools.
    */
   db_sql_pool_flush();

   Dmsg1(100, "Reload_config njobs=%d\n", njobs);
   reload_table[table].res_table = my_config->save_resources();
   Dmsg1(100, "Saved old config in table %d\n", table);

   ok = parse_dir_config(my_config, configfile, M_ERROR);

   Dmsg0(100, "Reloaded config file\n");
   if (!ok || !check_resources() || !check_catalog(UPDATE_CATALOG) || !initialize_sql_pooling()) {
      rtable = find_free_reload_table_entry();    /* save new, bad table */
      if (rtable < 0) {
         Jmsg(NULL, M_ERROR, 0, _("Please correct configuration file: %s\n"), configfile);
         Jmsg(NULL, M_ERROR_TERM, 0, _("Out of reload table entries. Giving up.\n"));
         goto bail_out;
      } else {
         Jmsg(NULL, M_ERROR, 0, _("Please correct configuration file: %s\n"), configfile);
         Jmsg(NULL, M_ERROR, 0, _("Resetting previous configuration.\n"));
      }
      reload_table[rtable].res_table = my_config->save_resources();
      /* Now restore old resource values */
      int num = my_config->m_r_last - my_config->m_r_first + 1;
      RES **res_tab = reload_table[table].res_table;
      for (int i=0; i<num; i++) {
         my_config->m_res_head[i] = res_tab[i];
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
   set_working_directory(me->working_directory);
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
 * See if two storage definitions point to the same Storage Daemon.
 *
 * We compare:
 *  - address
 *  - SDport
 *  - password
 */
static inline bool is_same_storage_daemon(STORERES *store1, STORERES *store2)
{
   return store1->SDport == store2->SDport &&
          bstrcasecmp(store1->address, store2->address) &&
          bstrcasecmp(store1->password.value, store2->password.value);
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
   JOBRES *job;
   bool need_tls;

   LockRes();

   job = (JOBRES *)GetNextRes(R_JOB, NULL);
   me = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   if (!me) {
      Jmsg(NULL, M_FATAL, 0, _("No Director resource defined in %s\n"
                               "Without that I don't know who I am :-(\n"), configfile);
      OK = false;
      goto bail_out;
   } else {
      my_config->m_omit_defaults = me->omit_defaults;
      set_working_directory(me->working_directory);
      if (!me->messages) {       /* If message resource not specified */
         me->messages = (MSGSRES *)GetNextRes(R_MSGS, NULL);
         if (!me->messages) {
            Jmsg(NULL, M_FATAL, 0, _("No Messages resource defined in %s\n"), configfile);
            OK = false;
            goto bail_out;
         }
      }

      /*
       * When the user didn't force us we optimize for size.
       */
      if (!me->optimize_for_size && !me->optimize_for_speed) {
         me->optimize_for_size = true;
      } else if (me->optimize_for_size && me->optimize_for_speed) {
         Jmsg(NULL, M_FATAL, 0, _("Cannot optimize for speed and size define only one in %s\n"), configfile);
         OK = false;
         goto bail_out;
      }

      if (GetNextRes(R_DIRECTOR, (RES *)me) != NULL) {
         Jmsg(NULL, M_FATAL, 0, _("Only one Director resource permitted in %s\n"),
            configfile);
         OK = false;
         goto bail_out;
      }

      /*
       * tls_require implies tls_enable
       */
      if (me->tls_require) {
         if (have_tls) {
            me->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in BAREOS.\n"));
            OK = false;
            goto bail_out;
         }
      }

      need_tls = me->tls_enable || me->tls_authenticate;

      if (!me->tls_certfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Certificate\" file not defined for Director \"%s\" in %s.\n"), me->name(), configfile);
         OK = false;
         goto bail_out;
      }

      if (!me->tls_keyfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Key\" file not defined for Director \"%s\" in %s.\n"), me->name(), configfile);
         OK = false;
         goto bail_out;
      }

      if ((!me->tls_ca_certfile && !me->tls_ca_certdir) &&
           need_tls && me->tls_verify_peer) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\" or \"TLS CA"
              " Certificate Dir\" are defined for Director \"%s\" in %s."
              " At least one CA certificate store is required"
              " when using \"TLS Verify Peer\".\n"),
              me->name(), configfile);
         OK = false;
         goto bail_out;
      }

      /*
       * If everything is well, attempt to initialize our per-resource TLS context
       */
      if (OK && (need_tls || me->tls_require)) {
         /*
          * Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer
          */
         me->tls_ctx = new_tls_context(me->tls_ca_certfile,
            me->tls_ca_certdir, me->tls_crlfile, me->tls_certfile,
            me->tls_keyfile, NULL, NULL, me->tls_dhfile,
            me->tls_verify_peer);

         if (!me->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for Director \"%s\" in %s.\n"),
                 me->name(), configfile);
            OK = false;
            goto bail_out;
         }
      }
   }

   if (!job) {
      Jmsg(NULL, M_FATAL, 0, _("No Job records defined in %s\n"), configfile);
      OK = false;
      goto bail_out;
   }

   if (!populate_jobdefs()) {
      OK = false;
      goto bail_out;
   }

   /*
    * Loop over Consoles
    */
   CONRES *cons;
   foreach_res(cons, R_CONSOLE) {
      /*
       * tls_require implies tls_enable
       */
      if (cons->tls_require) {
         if (have_tls) {
            cons->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in BAREOS.\n"));
            OK = false;
            goto bail_out;
         }
      }

      need_tls = cons->tls_enable || cons->tls_authenticate;

      if (!cons->tls_certfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Certificate\" file not defined for Console \"%s\" in %s.\n"),
            cons->name(), configfile);
         OK = false;
         goto bail_out;
      }

      if (!cons->tls_keyfile && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Key\" file not defined for Console \"%s\" in %s.\n"),
            cons->name(), configfile);
         OK = false;
         goto bail_out;
      }

      if ((!cons->tls_ca_certfile && !cons->tls_ca_certdir)
            && need_tls && cons->tls_verify_peer) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\" or \"TLS CA"
            " Certificate Dir\" are defined for Console \"%s\" in %s."
            " At least one CA certificate store is required"
            " when using \"TLS Verify Peer\".\n"),
            cons->name(), configfile);
         OK = false;
         goto bail_out;
      }

      /*
       * If everything is well, attempt to initialize our per-resource TLS context
       */
      if (OK && (need_tls || cons->tls_require)) {
         /*
          * Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer
          */
         cons->tls_ctx = new_tls_context(cons->tls_ca_certfile,
                                         cons->tls_ca_certdir, cons->tls_crlfile, cons->tls_certfile,
                                         cons->tls_keyfile, NULL, NULL,
                                         cons->tls_dhfile, cons->tls_verify_peer);
         if (!cons->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for File daemon \"%s\" in %s.\n"),
               cons->name(), configfile);
            OK = false;
            goto bail_out;
         }
      }

   }

   /*
    * Loop over Clients
    */
   me->subscriptions_used = 0;
   CLIENTRES *client;
   foreach_res(client, R_CLIENT) {
      /*
       * Count the number of clients
       *
       * Only used as indication not an enforced limit.
       */
      me->subscriptions_used++;

      /*
       * tls_require implies tls_enable
       */
      if (client->tls_require) {
         if (have_tls) {
            client->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in BAREOS.\n"));
            OK = false;
            goto bail_out;
         }
      }
      need_tls = client->tls_enable || client->tls_authenticate;
      if ((!client->tls_ca_certfile && !client->tls_ca_certdir) && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
            " or \"TLS CA Certificate Dir\" are defined for File daemon \"%s\" in %s.\n"),
            client->name(), configfile);
         OK = false;
         goto bail_out;
      }

      /*
       * If everything is well, attempt to initialize our per-resource TLS context
       */
      if (OK && (need_tls || client->tls_require)) {
         /*
          * Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer
          */
         client->tls_ctx = new_tls_context(client->tls_ca_certfile,
                                           client->tls_ca_certdir, client->tls_crlfile, client->tls_certfile,
                                           client->tls_keyfile, NULL, NULL, NULL,
                                           true);
         if (!client->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for File daemon \"%s\" in %s.\n"),
               client->name(), configfile);
            OK = false;
            goto bail_out;
         }
      }
   }

   /*
    * Loop over Storages
    */
   STORERES *store, *nstore;
   foreach_res(store, R_STORAGE) {
      /*
       * tls_require implies tls_enable
       */
      if (store->tls_require) {
         if (have_tls) {
            store->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in BAREOS.\n"));
            OK = false;
            goto bail_out;
         }
      }

      need_tls = store->tls_enable || store->tls_authenticate;

      if ((!store->tls_ca_certfile && !store->tls_ca_certdir) && need_tls) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
              " or \"TLS CA Certificate Dir\" are defined for Storage \"%s\" in %s.\n"),
              store->name(), configfile);
         OK = false;
         goto bail_out;
      }

      /*
       * If everything is well, attempt to initialize our per-resource TLS context
       */
      if (OK && (need_tls || store->tls_require)) {
        /*
         * Initialize TLS context:
         * Args: CA certfile, CA certdir, Certfile, Keyfile,
         * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer
         */
         store->tls_ctx = new_tls_context(store->tls_ca_certfile,
                                          store->tls_ca_certdir, store->tls_crlfile, store->tls_certfile,
                                          store->tls_keyfile, NULL, NULL, NULL, true);
         if (!store->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for Storage \"%s\" in %s.\n"),
                 store->name(), configfile);
            OK = false;
            goto bail_out;
         }
      }

      /*
       * If we collect statistics on this SD make sure any other entry pointing to the same SD does not
       * collect statistics otherwise we collect the same data multiple times.
       */
      if (store->collectstats) {
         nstore = store;
         while ((nstore = (STORERES *)GetNextRes(R_STORAGE, (RES *)nstore))) {
            if (is_same_storage_daemon(store, nstore) && nstore->collectstats) {
               nstore->collectstats = false;
               Dmsg1(200, _("Disabling collectstats for storage \"%s\""
                            " as other storage already collects from this SD.\n"), nstore->name());
            }
         }
      }
   }

   UnlockRes();
   if (OK) {
      close_msg(NULL);                    /* close temp message handler */
      init_msg(NULL, me->messages); /* open daemon message handler */
   }

bail_out:
   return OK;
}

/*
 * Initialize the sql pooling.
 */
static bool initialize_sql_pooling(void)
{
   bool retval = true;
   CATRES *catalog;

   foreach_res(catalog, R_CATALOG) {
      if (!db_sql_pool_initialize(catalog->db_driver,
                                  catalog->db_name,
                                  catalog->db_user,
                                  catalog->db_password.value,
                                  catalog->db_address,
                                  catalog->db_port,
                                  catalog->db_socket,
                                  catalog->disable_batch_insert,
                                  catalog->pooling_min_connections,
                                  catalog->pooling_max_connections,
                                  catalog->pooling_increment_connections,
                                  catalog->pooling_idle_timeout,
                                  catalog->pooling_validate_timeout)) {
         Jmsg(NULL, M_FATAL, 0, _("Could not setup sql pooling for Catalog \"%s\", database \"%s\".\n"),
              catalog->name(), catalog->db_name);
         retval = false;
         goto bail_out;
      }
   }

bail_out:
   return retval;
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
   CATRES *catalog;
   foreach_res(catalog, R_CATALOG) {
      B_DB *db;

      /*
       * Make sure we can open catalog, otherwise print a warning
       * message because the server is probably not running.
       */
      db = db_init_database(NULL,
                            catalog->db_driver,
                            catalog->db_name,
                            catalog->db_user,
                            catalog->db_password.value,
                            catalog->db_address,
                            catalog->db_port,
                            catalog->db_socket,
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
         goto bail_out;
      }

      /* Display a message if the db max_connections is too low */
      if (!db_check_max_connections(NULL, db, me->MaxConcurrentJobs)) {
         Pmsg1(000, "Warning, settings problem for Catalog=%s\n", catalog->name());
         Pmsg1(000, "%s", db_strerror(db));
      }

      /* we are in testing mode, so don't touch anything in the catalog */
      if (mode == CHECK_CONNECTION) {
         db_close_database(NULL, db);
         continue;
      }

      /* Loop over all pools, defining/updating them in each database */
      POOLRES *pool;
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
      CLIENTRES *client;
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
      STORERES *store;
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
            goto bail_out;
         }
         store->StorageId = sr.StorageId;   /* set storage Id */
         if (!sr.created) {                 /* if not created, update it */
            sr.AutoChanger = store->autochanger;
            if (!db_update_storage_record(NULL, db, &sr)) {
               Jmsg(NULL, M_FATAL, 0, _("Could not update storage record for %s\n"),
                    store->name());
               OK = false;
               goto bail_out;
            }
         }
      }

      /* Loop over all counters, defining them in each database */
      /* Set default value in all counters */
      COUNTERRES *counter;
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
         db_sql_query(db, cleanup_created_job);
         db_sql_query(db, cleanup_running_job);
      }

      /* Set type in global for debugging */
      set_db_type(db_get_type(db));

      db_close_database(NULL, db);
   }

bail_out:
   return OK;
}

static void cleanup_old_files()
{
   DIR* dp;
   struct dirent *entry, *result;
   int rc, name_max;
   int my_name_len = strlen(my_name);
   int len = strlen(me->working_directory);
   POOLMEM *cleanup = get_pool_memory(PM_MESSAGE);
   POOLMEM *basename = get_pool_memory(PM_MESSAGE);
   regex_t preg1;
   char prbuf[500];
   berrno be;

   /* Exclude spaces and look for .mail or .restore.xx.bsr files */
   const char *pat1 = "^[^ ]+\\.(restore\\.[^ ]+\\.bsr|mail)$";

   /* Setup working directory prefix */
   pm_strcpy(basename, me->working_directory);
   if (len > 0 && !IsPathSeparator(me->working_directory[len-1])) {
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

   if (!(dp = opendir(me->working_directory))) {
      berrno be;
      Pmsg2(000, "Failed to open working dir %s for cleanup: ERR=%s\n",
            me->working_directory, be.bstrerror());
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
      if (regexec(&preg1, result->d_name, 0, NULL, 0) == 0) {
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
