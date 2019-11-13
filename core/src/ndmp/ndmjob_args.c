/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *
 */

#include "lib/version.h"
#include "ndmjob.h"


char* help_text[] = {
    "ndmjob -v  -- print version and configuration info",
    "ndmjob OPTIONS ... FILES ...",
    "      FILES can be FILEPATH or NEWFILEPATH=OLDFILEPATH with",
    "      '=' quoted by backslash.",
    "Modes (exactly one required)",
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
    "  -c       -- create a backup",
    "  -t       -- list contents on a backup",
    "  -x       -- extract from a backup",
    "  -l       -- list media labels",
    "  -q       -- query agent(s)",
    "  -Z       -- clean up zee mess (put robot right)",
    "  -o init-labels -- init media labels",
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
#ifndef NDMOS_EFFECT_NO_SERVER_AGENTS
    "  -o daemon      -- launch session for incomming connections",
    "  -o tape-limit=SIZE -- specify the length, in bytes of the simulated "
    "tape",
#endif /* !NDMOS_EFFECT_NO_SERVER_AGENTS */
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
    "  -o rewind      -- rewind tape in drive, need -T and -f",
    "  -o eject       -- eject tape in drive, need -T and -f",
    "  -o move        -- cmd ROBOT to move tape, need -o from/to-addr",
    "  -o import=ELEMADDR -- cmd ROBOT to import tape from door to slot",
    "  -o export=ELEMADDR -- cmd ROBOT to export tape from slot to door",
    "  -o load=ELEMADDR   -- cmd ROBOT to load tape from slot to drive",
    "  -o unload[=ELEMADDR]-- cmd ROBOT to unload tape, sometimes auto",
    "  -o init-elem-status -- cmd ROBOT to rescan tape slots",
#ifndef NDMOS_OPTION_NO_TEST_AGENTS
    "  -o test-tape   -- test TAPE agent NDMP_TAPE functions",
    "  -o test-mover  -- test TAPE agent NDMP_MOVER functions",
    "  -o test-data   -- test DATA agent NDMP_DATA functions",
#endif /* NDMOS_OPTION_NO_TEST_AGENTS */
    "  -o time-limit=N",
    "           -- check for reply within specified seconds (default 360)",
    "  -o swap-connect -- perform DATA LISTEN & MOVER CONNECT",
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
    "General and Logging parameters",
    " --MACRO   -- expand MACRO from ndmjob-args file",
    "  -d N     -- set debug level to N (default 0, max 9)",
    "  -L FILE  -- set log file (default stderr, includes debug)",
    "  -n       -- no-op, just show how args were handled",
    "  -v       -- verbose, same messages as -d1 to standard out",
    "  -S       -- Perform DATA listen and MOVER CONNECT",
    "  -p PORT  -- NDMP port to listen on (for -o daemon)",
    "  -o no-time-stamps -- log w/o time stamps, makes diff(1)s easier",
    "  -o config-file=PATH",
    "           -- set config file ($NDMJOB_CONFIG, "
    "/usr/local/etc/ndmjob.conf)",
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
    "CONTROL of DATA agent parameters",
    "  -D AGENT -- data agent (see AGENT below)",
    "  -B TYPE  -- set backup format (default tar)",
    "  -C DIR   -- change directory on data agent before operation",
    "  -e PATN  -- exclude files matching pattern",
    "  -E NAME=VAL  -- add to data agent environment",
    "  -F FILE  -- add FILE arg (used to not confuse arg processing)",
    "  -o load-files=PATHNAME",
    "           -- load FILES from the specified PATHANME",
    "  -o import=ELEMADDR -- cmd ROBOT to import tape from door to slot",

    "  -I FILE  -- set output index file, enable FILEHIST (default to log)",
    "  -J FILE  -- set input index file (default none)",
    "  -U USER  -- user rights to use on data agent",
    "  -o rules=RULES -- apply RULES to job (see RULES below)",
    "CONTROL of TAPE agent parameters",
    "  -T AGENT -- tape agent if different than -D (see AGENT below)",
    "  -b N     -- block size in 512-byte records (default 20)",
    "  -f TAPE  -- tape drive device name",
    "  -o tape-timeout=SECONDS",
    "           -- how long to retry opening drive (await tape)",
    "  -o use-eject=N",
    "           -- use eject when unloading tapes (default 0)",
    "  -o tape-tcp=hostname:port -- send the data directly to that tcp port.",
    "  -o D-agent-fd=<fd> -- file descriptor to read the -D agent.",
    "CONTROL of ROBOT agent parameters",
    "  -R AGENT -- robot agent if different than -T (see AGENT below)",
    "  -m MEDIA -- add entry to media table (see below)",
    "  -o tape-addr=ELEMADDR",
    "           -- robot element address of drive (default first)",
    "  -o tape-scsi=SCSI",
    "           -- tape drive SCSI target (see below)",
    "  -o robot-timeout=SECONDS",
    "           -- how long to retry moving tapes (await robot)",
    "  -r SCSI  -- tape robot target (see below)",
    "",
    "Definitions:",
    "  AGENT      HOST[:PORT][/FLAGS][,USERNAME,PASSWORD]",
    "    FLAGS    [234][ntm] 2->v2 3->v3 4->v4  n->AUTH_NONE t->TEXT m->MD5",
    "  AGENT      .  (resident)",
    "  SCSI       DEVICE[,[CNUM,]SID[,LUN]]",
    "  MEDIA      [TAPE-LABEL][+SKIP-FILEMARKS][@ELEMADDR][/WINDOW-SIZE]",
    "",
    "RULES:",
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
    0};


int process_args(int argc, char* argv[])
{
  int c;
  char options[100];
  char** pp;
  char* p;
  char* op;
  char* av[1000];
  int ac = 0;

  progname = argv[0];

  if (argc == 2 && strcmp(argv[1], "-help") == 0) {
    help();
    exit(0);
  }

  if (argc == 2 && strcmp(argv[1], "-v") == 0) {
    ndmjob_version_info();
    exit(0);
  }

  if (argc < 2) usage();

  o_config_file = "./ndmjob.conf";
  if ((p = getenv("NDMJOB_CONF")) != 0) { o_config_file = p; }

  op = options;
  for (pp = help_text; *pp; pp++) {
    p = *pp;

    if (strncmp(p, "  -", 3) != 0) continue;
    if (p[3] == 'o') continue; /* don't include o: repeatedly */
    *op++ = p[3];
    if (p[5] != ' ') *op++ = ':';
  }
  *op++ = 'o'; /* include o: once */
  *op++ = ':';
  *op = 0;

  ac = copy_args_expanding_macros(argc, argv, av, 1000);

  while ((c = getopt(ac, av, options)) != EOF) {
    switch (c) {
      case 'o':
        handle_long_option(optarg);
        break;

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
      case 'c': /* -c       -- create a backup */
        set_job_mode(NDM_JOB_OP_BACKUP);
        break;

      case 't': /* -t       -- list contents on a backup */
        set_job_mode(NDM_JOB_OP_TOC);
        break;

      case 'x': /* -x       -- extract from a backup */
        set_job_mode(NDM_JOB_OP_EXTRACT);
        break;

      case 'l': /* -l       -- list media labels */
        set_job_mode(NDM_JOB_OP_LIST_LABELS);
        break;

      case 'q': /* -q       -- query agent(s) */
        set_job_mode(NDM_JOB_OP_QUERY_AGENTS);
        break;

      case 'Z': /* -Z       -- clean up zee mess */
        set_job_mode(NDM_JOB_OP_REMEDY_ROBOT);
        break;

      case 'B': /* -B TYPE  -- set backup format (default tar) */
        if (B_bu_type) { error_byebye("more than one of -B"); }
        B_bu_type = optarg;
        break;

      case 'b': /* -b N -- block size in 512-byte records (20) */
      {
        long b = strtol(optarg, NULL, 10);
        if (b < 1 || b > 200 || (!b && EINVAL == errno)) {
          error_byebye("bad -b option");
        }
        b_bsize = (int)b;
        break;
      }

      case 'p': /* -p N -- port number for daemon mode (10000) */
      {
        long p = strtol(optarg, NULL, 10);
        if (p < 1 || p > 65535 || (!p && EINVAL == errno)) {
          error_byebye("bad -p option");
        }
        p_ndmp_port = (int)p;
        break;
      }

      case 'C': /* -C DIR   -- change directory on data agent */
        C_chdir = optarg;
        break;

      case 'D': /* -D AGENT -- data agent (see below) */
        if (AGENT_GIVEN(D_data_agent)) { error_byebye("more than one of -D"); }
        if (ndmagent_from_str(&D_data_agent, optarg)) {
          error_byebye("bad -D argument");
        }
        break;
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

      case 'd': /* -d N     -- set debug level to N */
        d_debug = atoi(optarg);
        break;

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
      case 'E': /* -E NAME=VAL  -- add to data agent environment */
        if (E_environment.n_env >= NDM_MAX_ENV) {
          error_byebye("too many of -E");
        }
        {
          char* p;
          ndmp9_pval pv;

          p = optarg;
          pv.name = p;
          while (*p && *p != '=') p++;
          if (*p != '=') { error_byebye("missing value in -E"); }
          *p++ = 0;
          pv.value = p;
          ndma_store_env_list(&E_environment, &pv);
        }
        break;

      case 'e': /* -e PATN  -- exclude files matching pattern */
        if (n_e_exclude_pattern >= MAX_EXCLUDE_PATTERN) {
          error_byebye("too many of -e");
        }
        e_exclude_pattern[n_e_exclude_pattern++] = optarg;
        break;

      case 'F': /* -F FILE -- add to list of files */
        if (n_file_arg >= MAX_FILE_ARG) { error_byebye("too many FILE args"); }
        if (strchr(optarg, '=')) {
          char* p = strchr(optarg, '=');
          *p++ = 0;
          file_arg[n_file_arg] = p;
          file_arg_new[n_file_arg] = optarg;
          n_file_arg++;
        } else {
          file_arg[n_file_arg] = optarg;
          file_arg_new[n_file_arg] = 0;
          n_file_arg++;
        }

        break;

      case 'f': /* -f TAPE  -- tape drive device name */
        if (f_tape_device) { error_byebye("more than one of -f"); }
        f_tape_device = optarg;
        break;

      case 'I': /* -I FILE  -- output index, enab FILEHIST */
        if (I_index_file) { error_byebye("more than one of -I"); }
        I_index_file = optarg;
        break;

      case 'J': /* -J FILE  -- input index */
        if (J_index_file) { error_byebye("more than one of -J"); }
        J_index_file = optarg;
        break;

      case 'L': /* -L FILE  -- set log file (def stderr, incl. dbg) */
        if (L_log_file) { error_byebye("more than one of -L"); }
        L_log_file = optarg;
        if (d_debug < 2) d_debug = 2;
        break;

      case 'm': /* -m MEDIA -- add entry to media table (see below) */
        if (m_media.n_media >= NDM_MAX_MEDIA) {
          error_byebye("too many of -m");
        }
        {
          struct ndmmedia me;

          if (ndmmedia_from_str(&me, optarg)) {
            error_byebye("bad -m argument: %s", optarg);
          }

          ndma_clone_media_entry(&m_media, &me);
        }
        break;
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

      case 'n': /* -n       -- no-op, show how args were handled */
        n_noop++;
        break;

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
      case 'R': /* -R AGENT -- robot agent if different than -T */
        if (AGENT_GIVEN(R_robot_agent)) { error_byebye("more than one of -R"); }
        if (ndmagent_from_str(&R_robot_agent, optarg)) {
          error_byebye("bad -R argument");
        }
        break;

      case 'r': /* -r SCSI  -- tape robot target (see below) */
        if (ROBOT_GIVEN()) { error_byebye("more than one of -r"); }
        r_robot_target = NDMOS_API_MALLOC(sizeof(struct ndmscsi_target));
        if (!r_robot_target) { error_byebye("No memory for robot target"); }
        if (ndmscsi_target_from_str(r_robot_target, optarg)) {
          error_byebye("bad -r argument");
        }
        break;

      case 'T': /* -T AGENT -- tape agent if different than -D */
        if (AGENT_GIVEN(T_tape_agent)) { error_byebye("more than one of -T"); }
        if (ndmagent_from_str(&T_tape_agent, optarg)) {
          error_byebye("bad -T argument");
        }
        break;

      case 'U': /* -U USER  -- user rights to use on data agent */
        if (U_user) { error_byebye("more than one of -U"); }
        U_user = optarg;
        break;
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

      case 'v': /* -v       -- verbose */
        v_verbose++;
        break;

      default:
        usage();
        break;
    }
  }

  if (n_noop && d_debug > 1) {
    int i;

    for (i = 0; i < ac; i++) { printf(" av[%d] = '%s'\n", i, av[i]); }
  }

  if (!the_mode) {
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
    printf("must specify one of -[ctxlqZ] or other mode\n");
#else  /* !NDMOS_OPTION_NO_CONTROL_AGENT */
    printf("must specify -o daemon\n");
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
    usage();
  }

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
  for (c = optind; c < ac; c++) {
    if (n_file_arg >= MAX_FILE_ARG) { error_byebye("too many file args"); }
    if (strchr(av[c], '=')) {
      char* p = strchr(av[c], '=');
      *p++ = 0;
      file_arg[n_file_arg] = p;
      file_arg_new[n_file_arg] = av[c];
    } else {
      file_arg[n_file_arg] = av[c];
      file_arg_new[n_file_arg] = 0;
    }
    n_file_arg++;
  }

  if (o_load_files_file) {
    char buf[2048];
    FILE* fp;
    static struct load_file_entry {
      struct load_file_entry* next;
      char name[1];
    }* load_files_list = 0;

    /* clean up old load_files_list */
    while (load_files_list) {
      struct load_file_entry* p;
      p = load_files_list;
      load_files_list = p->next;
      p->next = 0;
      free(p);
    }

    fp = fopen(o_load_files_file, "r");
    if (!fp) {
      perror(o_load_files_file);
      error_byebye("can't open load_files file %s", o_load_files_file);
      /* no return */
    }
    while (fgets(buf, sizeof buf, fp) != NULL) {
      char *bp = buf, *p, *ep;
      int len, slen;
      struct load_file_entry* lfe;

      bp = buf;
      while (*bp && isspace(*bp)) bp++;
      ep = bp;
      while (*ep && (*ep != '\n') && (*ep != '\r')) ep++;
      *ep = 0;
      if (bp >= ep) continue;

      if (n_file_arg >= MAX_FILE_ARG) { error_byebye("too many FILE args"); }

      /* allocate memory */
      slen = (ep - bp) + 2;
      len = sizeof(struct load_file_entry) + (ep - bp) + 1;
      lfe = malloc(len);
      if (lfe == 0) {
        error_byebye("can't allocate entry for load_files file line %s", bp);
        /* no return */
      }
      lfe->next = 0;

      /* see if we have destination */
      if ((p = strchr(bp, '=')) != 0) {
        int plen;
        char ch = *p;
        *p = 0;

        /* double conversion -- assume the strings shrink */
        plen = (p - bp);
        ndmcstr_to_str(p, &lfe->name[plen + 2], slen - plen - 2);
        ndmcstr_to_str(bp, lfe->name, plen + 1);
        file_arg[n_file_arg] = &lfe->name[plen + 2];
        file_arg_new[n_file_arg] = lfe->name;
        *p = ch;
      } else {
        /* simple conversion copy */
        ndmcstr_to_str(bp, lfe->name, slen - 1);
        file_arg[n_file_arg] = lfe->name;
        file_arg_new[n_file_arg] = 0;
      }
      n_file_arg++;

      /* link into list */
      lfe->next = load_files_list;
      load_files_list = lfe;
    }

    fclose(fp);
  } /* end of load_files option */

  if (!B_bu_type) B_bu_type = "tar";

  /*
   * A quirk of the NDMP protocol is that the robot
   * should be accessed over a different connection
   * than the TAPE agent. (See the Workflow document).
   */
  if (ROBOT_GIVEN()) {
    if (!AGENT_GIVEN(R_robot_agent)) {
      if (AGENT_GIVEN(T_tape_agent))
        R_robot_agent = T_tape_agent;
      else
        R_robot_agent = D_data_agent;

      if (!AGENT_GIVEN(R_robot_agent)) {
        error_byebye("-r given, can't determine -R");
      }
    }
  } else if (AGENT_GIVEN(R_robot_agent)) {
    if (the_mode != NDM_JOB_OP_QUERY_AGENTS) { error_byebye("-R but no -r"); }
  }
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

  return 0;
}

struct ndmp_enum_str_table mode_long_name_table[] = {
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
    {"init-labels", NDM_JOB_OP_INIT_LABELS},
#ifndef NDMOS_OPTION_NO_TEST_AGENTS
    {"test-tape", NDM_JOB_OP_TEST_TAPE},
    {"test-mover", NDM_JOB_OP_TEST_MOVER},
    {"test-data", NDM_JOB_OP_TEST_DATA},
#endif /* NDMOS_OPTION_NO_TEST_AGENTS */
    {"eject", NDM_JOB_OP_EJECT_TAPE},
    {"rewind", NDM_JOB_OP_REWIND_TAPE},
    {"move", NDM_JOB_OP_MOVE_TAPE},
    {"import", NDM_JOB_OP_IMPORT_TAPE},
    {"export", NDM_JOB_OP_EXPORT_TAPE},
    {"load", NDM_JOB_OP_LOAD_TAPE},
    {"unload", NDM_JOB_OP_UNLOAD_TAPE},
    {"init-elem-status", NDM_JOB_OP_INIT_ELEM_STATUS},
    {"-c", NDM_JOB_OP_BACKUP},
    {"-t", NDM_JOB_OP_TOC},
    {"-x", NDM_JOB_OP_EXTRACT},
    {"-l", NDM_JOB_OP_LIST_LABELS},
    {"-q", NDM_JOB_OP_QUERY_AGENTS},
    {"-Z", NDM_JOB_OP_REMEDY_ROBOT},
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
#ifndef NDMOS_EFFECT_NO_SERVER_AGENTS
    {"daemon", NDM_JOB_OP_DAEMON},
#endif /* !NDMOS_EFFECT_NO_SERVER_AGENTS */
    {0}};


int handle_long_option(char* str)
{
  char* name;
  char* value;
  int mode;

  name = str;
  for (value = str; *value; value++)
    if (*value == '=') break;
  if (*value)
    *value++ = 0;
  else
    value = 0;

  if (ndmp_enum_from_str(&mode, name, mode_long_name_table)) {
    set_job_mode(mode);
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
    if (value) {
      switch (mode) {
        default: /* value part ignored */
          break;

        case NDM_JOB_OP_LOAD_TAPE:
        case NDM_JOB_OP_EXPORT_TAPE:
          o_from_addr = atoi(value);
          break;
        case NDM_JOB_OP_UNLOAD_TAPE:
        case NDM_JOB_OP_IMPORT_TAPE:
          o_to_addr = atoi(value);
          break;
      }
    }
  } else if (strcmp(name, "swap-connect") == 0) {
    /* value part ignored */
    o_swap_connect++;
  } else if (strcmp(name, "time-limit") == 0) {
    if (!value) {
      o_time_limit = 5 * 60;
    } else {
      o_time_limit = atoi(value);
    }
  } else if (strcmp(name, "use-eject") == 0) {
    if (!value) {
      o_use_eject = 1;
    } else {
      o_use_eject = atoi(value);
    }
  } else if (strcmp(name, "tape-addr") == 0 && value) {
    o_tape_addr = atoi(value);
  } else if (strcmp(name, "from-addr") == 0 && value) {
    o_from_addr = atoi(value);
  } else if (strcmp(name, "to-addr") == 0 && value) {
    o_to_addr = atoi(value);
  } else if (strcmp(name, "tape-timeout") == 0 && value) {
    o_tape_timeout = atoi(value);
  } else if (strcmp(name, "robot-timeout") == 0 && value) {
    o_robot_timeout = atoi(value);
  } else if (strcmp(name, "tape-scsi") == 0 && value) {
    o_tape_scsi = NDMOS_API_MALLOC(sizeof(struct ndmscsi_target));
    if (!o_tape_scsi) { error_byebye("No memory for tape-scsi target"); }
    if (ndmscsi_target_from_str(o_tape_scsi, value)) {
      error_byebye("bad -otape-scsi argument");
    }
  } else if (strcmp(name, "rules") == 0 && value) {
    if (!value) error_byebye("missing RULES in -o rules");
    o_rules = value;
  } else if (strcmp(name, "load-files") == 0 && value) {
    o_load_files_file = value;
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
  } else if (strcmp(name, "no-time-stamps") == 0) {
    /* value part ignored */
    o_no_time_stamps++;
  } else if (strcmp(name, "config-file") == 0 && value) {
    o_config_file = value;
  } else if (strcmp(name, "tape-tcp") == 0 && value) {
    o_tape_tcp = value;
  } else if (strcmp(name, "D-agent-fd") == 0 && value) {
    char d_agent[1025];
    int fd = atoi(value);
    int size;

    if (AGENT_GIVEN(D_data_agent)) {
      error_byebye("more than one of -D or -D-agent-fd");
    }

    size = read(fd, d_agent, 1024);
    d_agent[size] = '\0';
    if (size > 0 && d_agent[size - 1] == '\n') d_agent[size - 1] = '\0';
    close(fd);
    if (ndmagent_from_str(&D_data_agent, d_agent)) {
      error_byebye("bad -D-agent-fd argument");
    }
  } else if (strcmp(name, "tape-limit") == 0) {
    if (!value) {
      error_byebye("tape-limit argument is required");
    } else {
      o_tape_limit = atoi(value);
    }
  } else {
    if (value) value[-1] = '=';
    error_byebye("unknown/bad long option -o%s", str);
  }

  if (value) value[-1] = '=';
  return 0;
}

void set_job_mode(int mode)
{
  if (the_mode) {
    printf("more than one -[ctxlqZ] or other mode");
    usage();
  }
  the_mode = mode;
}

void usage(void) { error_byebye("bad usage, use -help"); }

void help(void)
{
  char* p;
  char** pp;

  for (pp = help_text; *pp; pp++) {
    p = *pp;
    printf("%s\n", p);
  }
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
  help_rules();
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
}

void ndmjob_version_info(void)
{
  char vbuf[100];
  char abuf[100];
  char obuf[5];

  *vbuf = 0;
#ifndef NDMOS_OPTION_NO_NDMP2
  strcat(vbuf, " NDMPv2");
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
  strcat(vbuf, " NDMPv3");
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
  strcat(vbuf, " NDMPv4");
#endif /* !NDMOS_OPTION_NO_NDMP4 */

  *abuf = 0;
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
  strcat(abuf, " CONTROL");
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
#ifndef NDMOS_OPTION_NO_DATA_AGENT
  strcat(abuf, " DATA");
#endif /* !NDMOS_OPTION_NO_DATA_AGENT */
#ifndef NDMOS_OPTION_NO_TAPE_AGENT
  strcat(abuf, " TAPE");
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#ifndef NDMOS_OPTION_NO_ROBOT_AGENT
  strcat(abuf, " ROBOT");
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */

  obuf[0] = (char)(NDMOS_ID >> 24);
  obuf[1] = (char)(NDMOS_ID >> 16);
  obuf[2] = (char)(NDMOS_ID >> 8);
  obuf[3] = (char)(NDMOS_ID >> 0);
  obuf[4] = 0;

  printf("%s (%s)\n", NDMOS_CONST_PRODUCT_NAME, NDMOS_CONST_VENDOR_NAME);

  printf("  Rev %s LIB:%d.%d/%s OS:%s (%s)\n", kBareosVersionStrings.Full,
         NDMJOBLIB_VERSION, NDMJOBLIB_RELEASE, kBareosVersionStrings.Full,
         NDMOS_CONST_NDMOS_REVISION, obuf);

  printf("  Agents:   %s\n", abuf);
  printf("  Protocols:%s\n", vbuf);
}


void dump_settings(void)
{
  int i;
  char buf[100];
  struct ndmmedia* me;
  struct ndm_env_entry* env;

  *buf = 0; /* shuts up -Wall */
  i = 0;    /* shuts up -Wall */
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
  switch (the_mode) {
    case 'x':
      printf("mode = x (extract)\n");
      break;

    case 'c':
      printf("mode = c (create)\n");
      break;

    case 't':
      printf("mode = t (table-of-contents)\n");
      break;

    case 'q':
      printf("mode = q (query-agents)\n");
      break;

    default:
      printf("mode = %c (unknown)\n", the_mode);
      break;
  }
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

  if (v_verbose)
    printf("verbose %d\n", v_verbose);
  else
    printf("not verbose\n");

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
  printf("blocksize = %d (%dkb, %db)\n", b_bsize, b_bsize / 2, b_bsize * 512);
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

  if (d_debug)
    printf("debug %d\n", d_debug);
  else
    printf("no debug\n");

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
  printf("Data agent %s\n", D_data_agent.host);
  if (AGENT_GIVEN(T_tape_agent))
    printf("Tape agent %s\n", T_tape_agent.host);
  else
    printf("Tape agent same as data agent\n");

  printf("tape device %s\n", f_tape_device);

  printf("tape format %s\n", B_bu_type);

  if (C_chdir)
    printf("Chdir %s\n", C_chdir);
  else
    printf("Chdir / (default)\n");
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

  if (L_log_file)
    printf("Log to file %s\n", L_log_file);
  else
    printf("Log to stderr (default)\n");

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
  if (I_index_file) {
    if (strcmp(I_index_file, "-") == 0) {
      printf("Index to log, enable FILEHIST\n");
    } else {
      printf("Index to file %s, enable FILEHIST\n", I_index_file);
    }
  } else {
    printf("Index off (default), no FILEHIST\n");
  }

  printf("%d media entries\n", m_media.n_media);
  for (me = m_media.head; me; me = me->next) {
    ndmmedia_to_str(me, buf);
    printf("  %2d: %s\n", i, buf);
  }

  printf("%d excludes\n", n_e_exclude_pattern);
  for (i = 0; i < n_e_exclude_pattern; i++) {
    printf("  %2d: %s\n", i, e_exclude_pattern[i]);
  }

  printf("%d environment values\n", E_environment.n_env);
  for (env = E_environment.head; env; env = env->next) {
    printf("  %2d: %s=%s\n", i, env->pval.name, env->pval.value);
  }

  printf("%d files\n", n_file_arg);
  for (i = 0; i < n_file_arg; i++) {
    printf(
        "  %2d: @%-8lld %s\n", i,
        nlist[i].fh_info.valid ? nlist[i].fh_info.value : NDMP9_INVALID_U_QUAD,
        file_arg[i]);
  }
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

  return;
}

int copy_args_expanding_macros(int argc, char* argv[], char* av[], int max_ac)
{
  int i, ac = 0, rc;
  char* arg;
  char* p;
  char env_name[50];

  /* expand macros */
  for (i = 0; i < argc; i++) {
    arg = argv[i];

    if (strncmp(arg, "--", 2) != 0 || arg[2] == 0) {
      av[ac++] = arg;
      continue;
    }

    snprintf(env_name, sizeof(env_name), "NDMJOB_%s", arg + 2);
    if ((p = getenv(env_name)) != 0) {
      ac += snarf_macro(&av[ac], p);
      continue;
    }

    rc = lookup_and_snarf(&av[ac], arg + 2);
    if (rc < 0) { error_byebye("bad arg macro --%s", arg + 2); }
    ac += rc;
  }

  av[ac] = 0;

  return ac;
}

int lookup_and_snarf(char* av[], char* name)
{
  FILE* fp;
  char buf[512];
  char* argfile;
  int ac = 0;
  int found = 0;

  argfile = o_config_file;
  assert(argfile);

  fp = fopen(argfile, "r");
  if (!fp) {
    perror(argfile);
    error_byebye("can't open config file %s", argfile);
  }

  while (ndmstz_getstanza(fp, buf, sizeof buf) >= 0) {
    if (buf[0] == '-' && buf[1] == '-' && strcmp(buf + 2, name) == 0) {
      found = 1;
      break;
    }
  }

  if (found) {
    while (ndmstz_getline(fp, buf, sizeof buf) >= 0) {
      if (*buf == 0) continue;
      ac += snarf_macro(&av[ac], buf);
    }
  }

  fclose(fp);

  if (!found) return -1;

  return ac;
}

int snarf_macro(char* av[], char* val)
{
  char* p;
  int ac = 0;
  char* tmp_av[100];
  int tmp_ac = 0;

  p = NDMOS_API_STRDUP(val);
  if (!p) { error_byebye("bad strdup macro"); }
  for (;;) {
    while (isspace((int)*p)) p++;
    if (*p == 0) break;
    tmp_av[tmp_ac++] = p;
    while (*p && !isspace((int)*p)) p++;
    if (*p) *p++ = 0;
  }

  ac = copy_args_expanding_macros(tmp_ac, tmp_av, av, 100);

  return ac;
}
