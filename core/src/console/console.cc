/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, September MM
 */
/**
 * @file
 * Bareos Console interface to the Director
 */

#include "include/bareos.h"
#include "console_conf.h"
#include "jcr.h"
#include "lib/bnet.h"

#ifdef HAVE_CONIO
#include "conio.h"
//#define CONIO_FIX 1
#else
#define con_init(x)
#define con_term()
#define con_set_zed_keys();
#define trapctlc()
#define clrbrk()
#define usrbrk() 0
#endif

#if defined(HAVE_WIN32)
#define isatty(fd) (fd==0)
#endif

/* Exported variables */
ConsoleResource *me = NULL;                    /* Our Global resource */
ConfigurationParser *my_config = NULL;             /* Our Global config */

//extern int rl_catch_signals;

/* Imported functions */
extern bool parse_cons_config(ConfigurationParser *config, const char *configfile, int exit_code);

/* Forward referenced functions */
static void terminate_console(int sig);
static int check_resources();
int get_cmd(FILE *input, const char *prompt, BareosSocket *sock, int sec);
static int do_outputcmd(FILE *input, BareosSocket *UA_sock);
void senditf(const char *fmt, ...);
void sendit(const char *buf);

extern "C" void got_sigstop(int sig);
extern "C" void got_sigcontinue(int sig);
extern "C" void got_sigtout(int sig);
extern "C" void got_sigtin(int sig);

/* Static variables */
static char *configfile = NULL;
static BareosSocket *UA_sock = NULL;
static DirectorResource *dir = NULL;
static ConsoleResource *cons = NULL;
static FILE *output = stdout;
static bool teeout = false;               /* output to output and stdout */
static bool stop = false;
static bool no_conio = false;
static int timeout = 0;
static int argc;
static int numdir;
static POOLMEM *args;
static char *argk[MAX_CMD_ARGS];
static char *argv[MAX_CMD_ARGS];
static bool file_selection = false;

/* Command prototypes */
static int versioncmd(FILE *input, BareosSocket *UA_sock);
static int inputcmd(FILE *input, BareosSocket *UA_sock);
static int outputcmd(FILE *input, BareosSocket *UA_sock);
static int teecmd(FILE *input, BareosSocket *UA_sock);
static int quitcmd(FILE *input, BareosSocket *UA_sock);
static int helpcmd(FILE *input, BareosSocket *UA_sock);
static int echocmd(FILE *input, BareosSocket *UA_sock);
static int timecmd(FILE *input, BareosSocket *UA_sock);
static int sleepcmd(FILE *input, BareosSocket *UA_sock);
static int execcmd(FILE *input, BareosSocket *UA_sock);
#ifdef HAVE_READLINE
static int eolcmd(FILE *input, BareosSocket *UA_sock);

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

#endif

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: " VERSION " (" BDATE ") %s %s %s\n\n"
"Usage: bconsole [-s] [-c config_file] [-d debug_level]\n"
"        -D <dir>    select a Director\n"
"        -l          list Directors defined\n"
"        -c <path>   specify configuration file or directory\n"
"        -d <nn>     set debug level to <nn>\n"
"        -dt         print timestamp in debug output\n"
"        -n          no conio\n"
"        -s          no signals\n"
"        -u <nn>     set command execution timeout to <nn> seconds\n"
"        -t          test - read configuration and exit\n"
"        -xc         print configuration and exit\n"
"        -xs         print configuration file schema in JSON format and exit\n"
"        -?          print this message.\n"
"\n"), 2000, HOST_OS, DISTNAME, DISTVER);
}

extern "C"
void got_sigstop(int sig)
{
   stop = true;
}

extern "C"
void got_sigcontinue(int sig)
{
   stop = false;
}

extern "C"
void got_sigtout(int sig)
{
// printf("Got tout\n");
}

extern "C"
void got_sigtin(int sig)
{
// printf("Got tin\n");
}

static int zed_keyscmd(FILE *input, BareosSocket *UA_sock)
{
   con_set_zed_keys();
   return 1;
}

/**
 * These are the @command
 */
struct cmdstruct {
   const char *key;
   int (*func)(FILE *input, BareosSocket *UA_sock);
   const char *help;
};
static struct cmdstruct commands[] = {
   { N_("input"), inputcmd, _("input from file")},
   { N_("output"), outputcmd, _("output to file")},
   { N_("quit"), quitcmd, _("quit")},
   { N_("tee"), teecmd, _("output to file and terminal")},
   { N_("sleep"), sleepcmd, _("sleep specified time")},
   { N_("time"), timecmd, _("print current time")},
   { N_("version"), versioncmd, _("print Console's version")},
   { N_("echo"), echocmd, _("echo command string")},
   { N_("exec"), execcmd, _("execute an external command")},
   { N_("exit"), quitcmd, _("exit = quit")},
   { N_("zed_keys"), zed_keyscmd, _("zed_keys = use zed keys instead of bash keys")},
   { N_("help"), helpcmd, _("help listing")},
#ifdef HAVE_READLINE
   { N_("separator"), eolcmd, _("set command separator")},
#endif
};
#define comsize ((int)(sizeof(commands)/sizeof(struct cmdstruct)))

static int do_a_command(FILE *input, BareosSocket *UA_sock)
{
   unsigned int i;
   int status;
   int found;
   int len;
   char *cmd;

   found = 0;
   status = 1;

   Dmsg1(120, "Command: %s\n", UA_sock->msg);
   if (argc == 0) {
      return 1;
   }

   cmd = argk[0]+1;
   if (*cmd == '#') {                 /* comment */
      return 1;
   }
   len = strlen(cmd);
   for (i=0; i<comsize; i++) {     /* search for command */
      if (bstrncasecmp(cmd,  _(commands[i].key), len)) {
         status = (*commands[i].func)(input, UA_sock);   /* go execute command */
         found = 1;
         break;
      }
   }
   if (!found) {
      pm_strcat(UA_sock->msg, _(": is an invalid command\n"));
      UA_sock->msglen = strlen(UA_sock->msg);
      sendit(UA_sock->msg);
   }
   return status;
}

static void read_and_process_input(FILE *input, BareosSocket *UA_sock)
{
   const char *prompt = "*";
   bool at_prompt = false;
   int tty_input = isatty(fileno(input));
   int status;
   btimer_t *tid = NULL;

   while (1) {
      if (at_prompt) {                /* don't prompt multiple times */
         prompt = "";
      } else {
         prompt = "*";
         at_prompt = true;
      }
      if (tty_input) {
         status = get_cmd(input, prompt, UA_sock, 30);
         if (usrbrk() == 1) {
            clrbrk();
         }
         if (usrbrk()) {
            break;
         }
      } else {
         /*
          * Reading input from a file
          */
         int len = sizeof_pool_memory(UA_sock->msg) - 1;
         if (usrbrk()) {
            break;
         }
         if (fgets(UA_sock->msg, len, input) == NULL) {
            status = -1;
         } else {
            sendit(UA_sock->msg);     /* echo to terminal */
            strip_trailing_junk(UA_sock->msg);
            UA_sock->msglen = strlen(UA_sock->msg);
            status = 1;
         }
      }
      if (status < 0) {
         break;                       /* error or interrupt */
      } else if (status == 0) {       /* timeout */
         if (bstrcmp(prompt, "*")) {
            tid = start_bsock_timer(UA_sock, timeout);
            UA_sock->fsend(".messages");
            stop_bsock_timer(tid);
         } else {
            continue;
         }
      } else {
         at_prompt = false;
         /*
          * @ => internal command for us
          */
         if (UA_sock->msg[0] == '@') {
            parse_args(UA_sock->msg, args, &argc, argk, argv, MAX_CMD_ARGS);
            if (!do_a_command(input, UA_sock)) {
               break;
            }
            continue;
         }
         tid = start_bsock_timer(UA_sock, timeout);
         if (!UA_sock->send()) {      /* send command */
            stop_bsock_timer(tid);
            break;                    /* error */
         }
         stop_bsock_timer(tid);
      }

      if (bstrcmp(UA_sock->msg, ".quit") || bstrcmp(UA_sock->msg, ".exit")) {
         break;
      }

      tid = start_bsock_timer(UA_sock, timeout);
      while ((status = UA_sock->recv()) >= 0 ||
             ((status == BNET_SIGNAL) && (
              (UA_sock->msglen != BNET_EOD) &&
              (UA_sock->msglen != BNET_MAIN_PROMPT) &&
              (UA_sock->msglen != BNET_SUB_PROMPT)))) {
         if (status == BNET_SIGNAL) {
            if (UA_sock->msglen == BNET_START_RTREE) {
               file_selection = true;
            } else if (UA_sock->msglen == BNET_END_RTREE) {
               file_selection = false;
            }
            continue;
         }

         if (at_prompt) {
            if (!stop) {
               sendit("\n");
            }
            at_prompt = false;
         }

         /*
          * Suppress output if running in background or user hit ctl-c
          */
         if (!stop && !usrbrk()) {
            if (UA_sock->msg) {
               sendit(UA_sock->msg);
            }
         }
      }
      stop_bsock_timer(tid);

      if (usrbrk() > 1) {
         break;
      } else {
         clrbrk();
      }
      if (!stop) {
         fflush(stdout);
      }

      if (is_bnet_stop(UA_sock)) {
         break;                       /* error or term */
      } else if (status == BNET_SIGNAL) {
         if (UA_sock->msglen == BNET_SUB_PROMPT) {
            at_prompt = true;
         }
         Dmsg1(100, "Got poll %s\n", bnet_sig_to_ascii(UA_sock));
      }
   }
}

/**
 * Call-back for reading a passphrase for an encrypted PEM file
 * This function uses getpass(),
 *  which uses a static buffer and is NOT thread-safe.
 */
static int tls_pem_callback(char *buf, int size, const void *userdata)
{
#ifdef HAVE_TLS
   const char *prompt = (const char *)userdata;
#if defined(HAVE_WIN32)
   sendit(prompt);
   if (win32_cgets(buf, size) == NULL) {
      buf[0] = 0;
      return 0;
   } else {
      return strlen(buf);
   }
#else
   char *passwd;

   passwd = getpass(prompt);
   bstrncpy(buf, passwd, size);
   return strlen(buf);
#endif
#else
   buf[0] = 0;
   return 0;
#endif
}

#ifdef HAVE_READLINE
#define READLINE_LIBRARY 1
#include "readline/readline.h"
#include "readline/history.h"
#include "lib/edit.h"
#include "lib/tls_openssl.h"
#include "lib/bsignal.h"

/**
 * Get the first keyword of the line
 */
static char *get_first_keyword()
{
   char *ret = NULL;
   int len;
   char *first_space = strchr(rl_line_buffer, ' ');
   if (first_space) {
      len = first_space - rl_line_buffer;
      ret = (char *) malloc((len + 1) * sizeof(char));
      memcpy(ret, rl_line_buffer, len);
      ret[len]=0;
   }
   return ret;
}

/**
 * Return the command before the current point.
 * Set nb to the number of command to skip
 */
static char *get_previous_keyword(int current_point, int nb)
{
   int i, end=-1, start, inquotes=0;
   char *s = NULL;

   while (nb-- >= 0) {
      /*
       * First we look for a space before the current word
       */
      for (i = current_point; i >= 0; i--) {
         if (rl_line_buffer[i] == ' ' || rl_line_buffer[i] == '=') {
            break;
         }
      }

      /*
       * Find the end of the command
       */
      for (; i >= 0; i--) {
         if (rl_line_buffer[i] != ' ') {
            end = i;
            break;
         }
      }

      /*
       * No end of string
       */
      if (end == -1) {
         return NULL;
      }

      /*
       * Look for the start of the command
       */
      for (start = end; start > 0; start--) {
         if (rl_line_buffer[start] == '"') {
            inquotes = !inquotes;
         }
         if ((rl_line_buffer[start - 1] == ' ') && inquotes == 0) {
            break;
         }
         current_point = start;
      }
   }

   s = (char *)malloc(end - start + 2);
   memcpy(s, rl_line_buffer + start, end - start + 1);
   s[end - start + 1] = 0;

   //  printf("=======> %i:%i <%s>\n", start, end, s);

   return s;
}

/**
 * Simple structure that will contain the completion list
 */
struct ItemList {
   alist list;
};

static ItemList *items = NULL;
void init_items()
{
   if (!items) {
      items = (ItemList*) malloc(sizeof(ItemList));
      memset(items, 0, sizeof(ItemList));

   } else {
      items->list.destroy();
   }

   items->list.init();
}

/**
 * Match a regexp and add the result to the items list
 * This function is recursive
 */
static void match_kw(regex_t *preg, const char *what, int len, POOLMEM *&buf)
{
   int rc, size;
   int nmatch = 20;
   regmatch_t pmatch[20];

   if (len <= 0) {
      return;
   }
   rc = regexec(preg, what, nmatch, pmatch, 0);
   if (rc == 0) {
#if 0
      Pmsg1(0, "\n\n%s\n0123456789012345678901234567890123456789\n        10         20         30\n", what);
      Pmsg2(0, "%i-%i\n", pmatch[0].rm_so, pmatch[0].rm_eo);
      Pmsg2(0, "%i-%i\n", pmatch[1].rm_so, pmatch[1].rm_eo);
      Pmsg2(0, "%i-%i\n", pmatch[2].rm_so, pmatch[2].rm_eo);
      Pmsg2(0, "%i-%i\n", pmatch[3].rm_so, pmatch[3].rm_eo);
#endif
      size = pmatch[1].rm_eo - pmatch[1].rm_so;
      buf = check_pool_memory_size(buf, size + 1);
      memcpy(buf, what + pmatch[1].rm_so, size);
      buf[size] = '\0';

      items->list.append(bstrdup(buf));

      /*
       * We search for the next keyword in the line
       */
      match_kw(preg, what + pmatch[1].rm_eo, len - pmatch[1].rm_eo, buf);
   }
}

/* fill the items list with the output of the help command */
void get_arguments(const char *what)
{
   regex_t preg;
   POOLMEM *buf;
   int rc;
   init_items();

   rc = regcomp(&preg, "(([a-z_]+=)|([a-z]+)( |$))", REG_EXTENDED);
   if (rc != 0) {
      return;
   }

   buf = get_pool_memory(PM_MESSAGE);
   UA_sock->fsend(".help item=%s", what);
   while (UA_sock->recv() > 0) {
      strip_trailing_junk(UA_sock->msg);
      match_kw(&preg, UA_sock->msg, UA_sock->msglen, buf);
   }
   free_pool_memory(buf);
   regfree(&preg);
}

/* retreive a simple list (.pool, .client) and store it into items */
void get_items(const char *what)
{
   init_items();

   UA_sock->fsend("%s", what);
   while (UA_sock->recv() > 0) {
      strip_trailing_junk(UA_sock->msg);
      items->list.append(bstrdup(UA_sock->msg));
   }
}

typedef enum
{
   ITEM_ARG,       /* item with simple list like .jobs */
   ITEM_HELP       /* use help item=xxx and detect all arguments */
} cpl_item_t;

/* Generator function for command completion.  STATE lets us know whether
 * to start from scratch; without any state (i.e. STATE == 0), then we
 * start at the top of the list.
 */
static char *item_generator(const char *text, int state,
                            const char *item, cpl_item_t type)
{
  static int list_index, len;
  char *name;

  /* If this is a new word to complete, initialize now.  This includes
   * saving the length of TEXT for efficiency, and initializing the index
   *  variable to 0.
   */
  if (!state)
  {
     list_index = 0;
     len = strlen(text);
     switch(type) {
     case ITEM_ARG:
        get_items(item);
        break;
     case ITEM_HELP:
        get_arguments(item);
        break;
     }
  }

  /* Return the next name which partially matches from the command list. */
  while (items && list_index < items->list.size())
  {
     name = (char *)items->list[list_index];
     list_index++;

     if (bstrncmp(name, text, len)) {
        char *ret = (char *) actuallymalloc(strlen(name)+1);
        strcpy(ret, name);
        return ret;
     }
  }

  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}

/* global variables for the type and the item to search
 * the readline API doesn't permit to pass user data.
 */
static const char *cpl_item;
static cpl_item_t cpl_type;

static char *cpl_generator(const char *text, int state)
{
   return item_generator(text, state, cpl_item, cpl_type);
}

/* this function is used to not use the default filename completion */
static char *dummy_completion_function(const char *text, int state)
{
   return NULL;
}

struct cpl_keywords_t {
   const char *key;
   const char *cmd;
   bool file_selection;
};

static struct cpl_keywords_t cpl_keywords[] = {
   { "pool=", ".pool", false },
   { "nextpool=", ".pool", false },
   { "fileset=", ".fileset", false },
   { "client=", ".client", false },
   { "jobdefs=", ".jobdefs", false },
   { "job=", ".jobs", false },
   { "restore_job=",".jobs type=R", false },
   { "level=", ".level", false },
   { "storage=", ".storage", false },
   { "schedule=", ".schedule", false },
   { "volume=", ".media", false },
   { "oldvolume=", ".media", false },
   { "volstatus=", ".volstatus", false },
   { "catalog=", ".catalogs", false },
   { "message=", ".msgs", false },
   { "profile=", ".profiles", false },
   { "actiononpurge=", ".actiononpurge", false },
   { "ls", ".ls", true },
   { "cd", ".lsdir", true },
   { "add", ".ls", true },
   { "mark", ".ls", true },
   { "m", ".ls", true },
   { "delete", ".lsmark", true },
   { "unmark", ".lsmark", true }
};
#define key_size ((int)(sizeof(cpl_keywords)/sizeof(struct cpl_keywords_t)))

/* Attempt to complete on the contents of TEXT.  START and END bound the
 * region of rl_line_buffer that contains the word to complete.  TEXT is
 * the word to complete.  We can use the entire contents of rl_line_buffer
 * in case we want to do some simple parsing.  Return the array of matches,
 * or NULL if there aren't any.
 */
static char **readline_completion(const char *text, int start, int end)
{
   bool found = false;
   char **matches;
   char *s, *cmd;
   matches = (char **)NULL;

   /* If this word is at the start of the line, then it is a command
    * to complete. Otherwise it is the name of a file in the current
    * directory.
    */
   s = get_previous_keyword(start, 0);
   cmd = get_first_keyword();
   if (s) {
      for (int i = 0; i < key_size; i++) {
         /*
          * See if this keyword is allowed with the current file_selection setting.
          */
         if (cpl_keywords[i].file_selection != file_selection) {
            continue;
         }

         if (bstrcasecmp(s, cpl_keywords[i].key)) {
            cpl_item = cpl_keywords[i].cmd;
            cpl_type = ITEM_ARG;
            matches = rl_completion_matches(text, cpl_generator);
            found = true;
            break;
         }
      }

      if (!found) {             /* we try to get help with the first command */
         cpl_item = cmd;
         cpl_type = ITEM_HELP;
         /* we don't want to append " " at the end */
         rl_completion_suppress_append = true;
         matches = rl_completion_matches(text, cpl_generator);
      }
      free(s);
   } else {                     /* nothing on the line, display all commands */
      cpl_item = ".help all";
      cpl_type = ITEM_ARG;
      matches = rl_completion_matches(text, cpl_generator);
   }
   if (cmd) {
      free(cmd);
   }
   return (matches);
}

static char eol = '\0';
static int eolcmd(FILE *input, BareosSocket *UA_sock)
{
   if ((argc > 1) && (strchr("!$%&'()*+,-/:;<>?[]^`{|}~", argk[1][0]) != NULL)) {
      eol = argk[1][0];
   } else if (argc == 1) {
      eol = '\0';
   } else {
      sendit(_("Illegal separator character.\n"));
   }
   return 1;
}

/**
 * Return 1 if OK
 *        0 if no input
 *       -1 error (must stop)
 */
int get_cmd(FILE *input, const char *prompt, BareosSocket *sock, int sec)
{
   static char *line = NULL;
   static char *next = NULL;
   static int do_history = 0;
   char *command;

   if (line == NULL) {
      do_history = 0;
      rl_catch_signals = 0;              /* do it ourselves */
      /* Here, readline does ***real*** malloc
       * so, be we have to use the real free
       */
      line = readline((char *)prompt);   /* cast needed for old readlines */
      if (!line) {
         return -1;                      /* error return and exit */
      }
      strip_trailing_junk(line);
      command = line;
   } else if (next) {
      command = next + 1;
   } else {
     sendit(_("Command logic problem\n"));
     sock->msglen = 0;
     sock->msg[0] = 0;
     return 0;                  /* No input */
   }

   /*
    * Split "line" into multiple commands separated by the eol character.
    *   Each part is pointed to by "next" until finally it becomes null.
    */
   if (eol == '\0') {
      next = NULL;
   } else {
      next = strchr(command, eol);
      if (next) {
         *next = '\0';
      }
   }
   if (command != line && isatty(fileno(input))) {
      senditf("%s%s\n", prompt, command);
   }

   sock->msglen = pm_strcpy(sock->msg, command);
   if (sock->msglen) {
      do_history++;
   }

   if (!next) {
      if (do_history) {
        add_history(line);
      }
      actuallyfree(line);       /* allocated by readline() malloc */
      line = NULL;
   }
   return 1;                    /* OK */
}

#else /* no readline, do it ourselves */

#ifdef HAVE_CONIO
static bool bisatty(int fd)
{
   if (no_conio) {
      return false;
   }
   return isatty(fd);
}
#endif

#ifdef HAVE_WIN32 /* use special console for input on win32 */
/**
 * Get next input command from terminal.
 *
 *   Returns: 1 if got input
 *           -1 if EOF or error
 */
int get_cmd(FILE *input, const char *prompt, BareosSocket *sock, int sec)
{
   int len;

   if (!stop) {
      if (output == stdout || teeout) {
         sendit(prompt);
      }
   }

again:
   len = sizeof_pool_memory(sock->msg) - 1;
   if (stop) {
      sleep(1);
      goto again;
   }

#ifdef HAVE_CONIO
   if (bisatty(fileno(input))) {
      input_line(sock->msg, len);
      goto ok_out;
   }
#endif

   if (input == stdin) {
      if (win32_cgets(sock->msg, len) == NULL) {
         return -1;
      }
   } else {
      if (fgets(sock->msg, len, input) == NULL) {
         return -1;
      }
   }

   if (usrbrk()) {
      clrbrk();
   }

   strip_trailing_junk(sock->msg);
   sock->msglen = strlen(sock->msg);

   return 1;
}
#else
/**
 * Get next input command from terminal.
 *
 *   Returns: 1 if got input
 *            0 if timeout
 *           -1 if EOF or error
 */
int get_cmd(FILE *input, const char *prompt, BareosSocket *sock, int sec)
{
   int len;

   if (!stop) {
      if (output == stdout || teeout) {
         sendit(prompt);
      }
   }

again:
   switch (wait_for_readable_fd(fileno(input), sec, true)) {
   case 0:
      return 0;                    /* timeout */
   case -1:
      return -1;                   /* error */
   default:
      len = sizeof_pool_memory(sock->msg) - 1;
      if (stop) {
         sleep(1);
         goto again;
      }
#ifdef HAVE_CONIO
      if (bisatty(fileno(input))) {
         input_line(sock->msg, len);
         break;
      }
#endif
      if (fgets(sock->msg, len, input) == NULL) {
         return -1;
      }
      break;
   }

   if (usrbrk()) {
      clrbrk();
   }

   strip_trailing_junk(sock->msg);
   sock->msglen = strlen(sock->msg);

   return 1;
}
#endif /* HAVE_WIN32 */
#endif /* ! HAVE_READLINE */

static int console_update_history(const char *histfile)
{
   int ret = 0;

#ifdef HAVE_READLINE
   int max_history_length,
       truncate_entries;

   /*
    * Calculate how much we should keep in the current history file.
    */
   max_history_length = (me) ? me->history_length : 100;
   truncate_entries = max_history_length - history_length;
   if (truncate_entries < 0) {
      truncate_entries = 0;
   }

   /*
    * First, try to truncate the history file, and if it
    * fails, the file is probably not present, and we
    * can use write_history to create it.
    */
   if (history_truncate_file(histfile, truncate_entries) == 0) {
      ret = append_history(history_length, histfile);
   } else {
      ret = write_history(histfile);
   }
#endif

   return ret;
}

static int console_init_history(const char *histfile)
{
   int ret = 0;

#ifdef HAVE_READLINE
   int max_history_length;

   using_history();

   /*
    * First truncate the history size to an reasonable size.
    */
   max_history_length = (me) ? me->history_length : 100;
   history_truncate_file(histfile, max_history_length);

   /*
    * Read the content of the history file into memory.
    */
   ret = read_history(histfile);

   /*
    * Tell the completer that we want a complete.
    */
   rl_completion_entry_function = dummy_completion_function;
   rl_attempted_completion_function = readline_completion;
   rl_filename_completion_desired = 0;
   stifle_history(max_history_length);
#endif

   return ret;
}

static bool select_director(const char *director, DirectorResource **ret_dir, ConsoleResource **ret_cons)
{
   int numcon=0, numdir=0;
   int i=0, item=0;
   BareosSocket *UA_sock;
   DirectorResource *dir = NULL;
   ConsoleResource *cons = NULL;

   *ret_cons = NULL;
   *ret_dir = NULL;

   LockRes();
   numdir = 0;
   foreach_res(dir, R_DIRECTOR) {
      numdir++;
   }
   numcon = 0;
   foreach_res(cons, R_CONSOLE) {
      numcon++;
   }
   UnlockRes();

   if (numdir == 1) {           /* No choose */
      dir = (DirectorResource *)GetNextRes(R_DIRECTOR, NULL);
   }

   if (director) {    /* Command line choice overwrite the no choose option */
      LockRes();
      foreach_res(dir, R_DIRECTOR) {
         if (bstrcmp(dir->name(), director)) {
            break;
         }
      }
      UnlockRes();
      if (!dir) {               /* Can't find Director used as argument */
         senditf(_("Can't find %s in Director list\n"), director);
         return 0;
      }
   }

   if (!dir) {                  /* prompt for director */
      UA_sock = New(BareosSocketTCP);
try_again:
      sendit(_("Available Directors:\n"));
      LockRes();
      numdir = 0;
      foreach_res(dir, R_DIRECTOR) {
         senditf( _("%2d:  %s at %s:%d\n"), 1+numdir++, dir->name(), dir->address, dir->DIRport);
      }
      UnlockRes();
      if (get_cmd(stdin, _("Select Director by entering a number: "),
                  UA_sock, 600) < 0)
      {
         WSACleanup();               /* Cleanup Windows sockets */
         return 0;
      }
      if (!is_a_number(UA_sock->msg)) {
         senditf(_("%s is not a number. You must enter a number between "
                   "1 and %d\n"),
                 UA_sock->msg, numdir);
         goto try_again;
      }
      item = atoi(UA_sock->msg);
      if (item < 0 || item > numdir) {
         senditf(_("You must enter a number between 1 and %d\n"), numdir);
         goto try_again;
      }
      delete UA_sock;
      LockRes();
      for (i=0; i<item; i++) {
         dir = (DirectorResource *)GetNextRes(R_DIRECTOR, (CommonResourceHeader *)dir);
      }
      UnlockRes();
   }

   /*
    * Look for a console linked to this director
    */
   LockRes();
   for (i=0; i<numcon; i++) {
      cons = (ConsoleResource *)GetNextRes(R_CONSOLE, (CommonResourceHeader *)cons);
      if (cons->director && bstrcmp(cons->director, dir->name())) {
         break;
      }
      cons = NULL;
   }

   /*
    * Look for the first non-linked console
    */
   if (cons == NULL) {
      for (i=0; i<numcon; i++) {
         cons = (ConsoleResource *)GetNextRes(R_CONSOLE, (CommonResourceHeader *)cons);
         if (cons->director == NULL)
            break;
         cons = NULL;
      }
   }

   /*
    * If no console, take first one
    */
   if (!cons) {
      cons = (ConsoleResource *)GetNextRes(R_CONSOLE, (CommonResourceHeader *)NULL);
   }
   UnlockRes();

   *ret_dir = dir;
   *ret_cons = cons;

   return 1;
}

/**
 * Callback calling director console connection
*/
static int dir_psk_client_callback(char *identity,
                                   unsigned int max_identity_len,
                                   unsigned char *psk,
                                   unsigned int max_psk_len) {

   Dmsg0(100, "dir_psk_client_callback");

   int result = 0;

   if (NULL != dir && NULL != dir->password.value) {
      if (p_encoding_clear == dir->password.encoding) {
         /* plain password */
      } else if (p_encoding_md5 == dir->password.encoding) {
         /* md5 string */
         /* convert the PSK key to binary */
         result = hex2bin(dir->password.value, psk, max_psk_len);
      } else {
         /* hex password encoding? */
      }
   } else {
      Dmsg0(100, "Passowrd not set in Director Config\n");
      result = 0;
   }

   if (result == 0 || result > (int)max_psk_len) {
      Dmsg1(100, "Error, Could not convert PSK key '%s' to binary key\n", dir->password.value);
      result = 0;
   } else {
      static char *psk_identity = (char *)"Nasenbaer";

      int ret = bsnprintf(identity, max_identity_len, "%s", psk_identity);
      if (ret < 0 || (unsigned int)ret > max_identity_len) {
         Dmsg0(100, "Error, psk_identify too long\n");
         result = 0;
      }
   }

   return result;
}

/*
 * Main Bareos Console -- User Interface Program
 */
int main(int argc, char *argv[])
{
   int ch;
   int errmsg_len;
   char *director = NULL;
   const char *name;
   s_password *password = NULL;
   char errmsg[1024];
   bool list_directors = false;
   bool no_signals = false;
   bool test_config = false;
   bool export_config = false;
   bool export_config_schema = false;
   JobControlRecord jcr;
   PoolMem history_file;
   utime_t heart_beat;

   errmsg_len = sizeof(errmsg);
   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   init_stack_dump();
   lmgr_init_thread();
   my_name_is(argc, argv, "bconsole");
   init_msg(NULL, NULL);
   working_directory = "/tmp";
   args = get_pool_memory(PM_FNAME);

   while ((ch = getopt(argc, argv, "D:lc:d:nstu:x:?")) != -1) {
      switch (ch) {
      case 'D':                    /* Director */
         if (director) {
            free(director);
         }
         director = bstrdup(optarg);
         break;

      case 'l':
         list_directors = true;
         test_config = true;
         break;

      case 'c':                    /* configuration file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'd':
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;

      case 'n':                    /* no conio */
         no_conio = true;
         break;

      case 's':                    /* turn off signals */
         no_signals = true;
         break;

      case 't':
         test_config = true;
         break;

      case 'u':
         timeout = atoi(optarg);
         break;

      case 'x':                    /* export configuration/schema and exit */
         if (*optarg == 's') {
            export_config_schema = true;
         } else if (*optarg == 'c') {
            export_config = true;
         } else {
            usage();
         }
         break;

      case '?':
      default:
         usage();
         exit(1);
      }
   }
   argc -= optind;
   argv += optind;

   if (!no_signals) {
      init_signals(terminate_console);
   }

#if !defined(HAVE_WIN32)
   /* Override Bareos default signals */
   signal(SIGQUIT, SIG_IGN);
   signal(SIGTSTP, got_sigstop);
   signal(SIGCONT, got_sigcontinue);
   signal(SIGTTIN, got_sigtin);
   signal(SIGTTOU, got_sigtout);
   trapctlc();
#endif

   OSDependentInit();

   if (argc) {
      usage();
      exit(1);
   }

   if (export_config_schema) {
      PoolMem buffer;

      my_config = new_config_parser();
      init_cons_config(my_config, configfile, M_ERROR_TERM);
      print_config_schema_json(buffer);
      printf("%s\n", buffer.c_str());
      exit(0);
   }

   my_config = new_config_parser();
   parse_cons_config(my_config, configfile, M_ERROR_TERM);

   if (export_config) {
      my_config->dump_resources(prtmsg, NULL);
      exit(0);
   }

   if (init_crypto() != 0) {
      Emsg0(M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
   }

   if (!check_resources()) {
      Emsg1(M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), my_config->get_base_config_path());
   }

   if (!no_conio) {
      con_init(stdin);
   }

   if (list_directors) {
      LockRes();
      foreach_res(dir, R_DIRECTOR) {
         senditf("%s\n", dir->name());
      }
      UnlockRes();
   }

   if (test_config) {
      terminate_console(0);
      exit(0);
   }

   memset(&jcr, 0, sizeof(jcr));

   (void)WSA_Init();                        /* Initialize Windows sockets */

   start_watchdog();                        /* Start socket watchdog */

   if (!select_director(director, &dir, &cons)) {
      return 1;
   }

   senditf(_("Connecting to Director %s:%d\n"), dir->address,dir->DIRport);

   if (dir->heartbeat_interval) {
      heart_beat = dir->heartbeat_interval;
   } else if (cons) {
      heart_beat = cons->heartbeat_interval;
   } else {
      heart_beat = 0;
   }

   UA_sock = New(BareosSocketTCP);
   if (!UA_sock->connect(NULL, 5, 15, heart_beat, "Director daemon", dir->address, NULL, dir->DIRport, false)) {
      delete UA_sock;
      terminate_console(0);
      return 1;
   }
   jcr.dir_bsock = UA_sock;

   /*
    * If cons == NULL, default console will be used
    */
   if (cons) {
      name = cons->name();
      ASSERT(cons->password.encoding == p_encoding_md5);
      password = &cons->password;
   } else {
      name = "*UserAgent*";
      ASSERT(dir->password.encoding == p_encoding_md5);
      password = &dir->password;
   }

   if (!UA_sock->authenticate_with_director(&jcr, name, *password, errmsg, errmsg_len, dir)) {
      sendit(errmsg);
      terminate_console(0);
      return 1;
   }

   sendit(errmsg);

   Dmsg0(40, "Opened connection with Director daemon\n");

   sendit(_("Enter a period to cancel a command.\n"));

#if defined(HAVE_WIN32)
   /*
    * Read/Update history file if USERPROFILE exists
    */
   char *env = getenv("USERPROFILE");
#else
   /*
    * Read/Update history file if HOME exists
    */
   char *env = getenv("HOME");
#endif

   /*
    * Run commands in ~/.bconsolerc if any
    */
   if (env) {
      FILE *fp;

      pm_strcpy(UA_sock->msg, env);
      pm_strcat(UA_sock->msg, "/.bconsolerc");
      fp = fopen(UA_sock->msg, "rb");
      if (fp) {
         read_and_process_input(fp, UA_sock);
         fclose(fp);
      }
   }

   /*
    * See if there is an explicit setting of a history file to use.
    */
   if (me && me->history_file) {
      pm_strcpy(history_file, me->history_file);
      console_init_history(history_file.c_str());
   } else {
      if (env) {
         pm_strcpy(history_file, env);
         pm_strcat(history_file, "/.bconsole_history");
         console_init_history(history_file.c_str());
      } else {
         pm_strcpy(history_file, "");
      }
   }

   read_and_process_input(stdin, UA_sock);

   if (UA_sock) {
      UA_sock->signal(BNET_TERMINATE); /* send EOF */
      UA_sock->close();
   }

   if (history_file.size()) {
      console_update_history(history_file.c_str());
   }

   terminate_console(0);
   return 0;
}

/* Cleanup and then exit */
static void terminate_console(int sig)
{

   static bool already_here = false;

   if (already_here) {                /* avoid recursive temination problems */
      exit(1);
   }
   already_here = true;
   stop_watchdog();
   my_config->free_resources();
   free(my_config);
   my_config = NULL;
   cleanup_crypto();
   free_pool_memory(args);
   if (!no_conio) {
      con_term();
   }
   WSACleanup();               /* Cleanup Windows sockets */
   lmgr_cleanup_main();

   if (sig != 0) {
      exit(1);
   }
   return;
}

/**
 * Make a quick check to see that we have all the resources needed.
 */
static int check_resources()
{
   bool OK = true;
   DirectorResource *director;
   bool tls_needed;

   LockRes();

   numdir = 0;
   foreach_res(director, R_DIRECTOR) {
      numdir++;
   }

   if (numdir == 0) {
      Emsg1(M_FATAL, 0, _("No Director resource defined in %s\n"
                          "Without that I don't how to speak to the Director :-(\n"), my_config->get_base_config_path());
      OK = false;
   }

   ConsoleResource *cons;
   me = (ConsoleResource *)GetNextRes(R_CONSOLE, NULL);

   UnlockRes();

   return OK;
}

/* @version */
static int versioncmd(FILE *input, BareosSocket *UA_sock)
{
   senditf("Version: " VERSION " (" BDATE ") %s %s %s\n",
      HOST_OS, DISTNAME, DISTVER);
   return 1;
}

/* @input <input-filename> */
static int inputcmd(FILE *input, BareosSocket *UA_sock)
{
   FILE *fd;

   if (argc > 2) {
      sendit(_("Too many arguments on input command.\n"));
      return 1;
   }
   if (argc == 1) {
      sendit(_("First argument to input command must be a filename.\n"));
      return 1;
   }
   fd = fopen(argk[1], "rb");
   if (!fd) {
      berrno be;
      senditf(_("Cannot open file %s for input. ERR=%s\n"),
         argk[1], be.bstrerror());
      return 1;
   }
   read_and_process_input(fd, UA_sock);
   fclose(fd);
   return 1;
}

/* @tee <output-filename> */
/* Send output to both terminal and specified file */
static int teecmd(FILE *input, BareosSocket *UA_sock)
{
   teeout = true;
   return do_outputcmd(input, UA_sock);
}

/* @output <output-filename> */
/* Send output to specified "file" */
static int outputcmd(FILE *input, BareosSocket *UA_sock)
{
   teeout = false;
   return do_outputcmd(input, UA_sock);
}

static int do_outputcmd(FILE *input, BareosSocket *UA_sock)
{
   FILE *fd;
   const char *mode = "a+b";

   if (argc > 3) {
      sendit(_("Too many arguments on output/tee command.\n"));
      return 1;
   }
   if (argc == 1) {
      if (output != stdout) {
         fclose(output);
         output = stdout;
         teeout = false;
      }
      return 1;
   }
   if (argc == 3) {
      mode = argk[2];
   }
   fd = fopen(argk[1], mode);
   if (!fd) {
      berrno be;
      senditf(_("Cannot open file %s for output. ERR=%s\n"),
         argk[1], be.bstrerror(errno));
      return 1;
   }
   output = fd;
   return 1;
}

/**
 * @exec "some-command" [wait-seconds]
*/
static int execcmd(FILE *input, BareosSocket *UA_sock)
{
   Bpipe *bpipe;
   char line[5000];
   int status;
   int wait = 0;

   if (argc > 3) {
      sendit(_("Too many arguments. Enclose command in double quotes.\n"));
      return 1;
   }
   if (argc == 3) {
      wait = atoi(argk[2]);
   }
   bpipe = open_bpipe(argk[1], wait, "r");
   if (!bpipe) {
      berrno be;
      senditf(_("Cannot popen(\"%s\", \"r\"): ERR=%s\n"),
         argk[1], be.bstrerror(errno));
      return 1;
   }

   while (fgets(line, sizeof(line), bpipe->rfd)) {
      senditf("%s", line);
   }
   status = close_bpipe(bpipe);
   if (status != 0) {
      berrno be;
      be.set_errno(status);
     senditf(_("Autochanger error: ERR=%s\n"), be.bstrerror());
   }
   return 1;
}

/* @echo xxx yyy */
static int echocmd(FILE *input, BareosSocket *UA_sock)
{
   for (int i=1; i < argc; i++) {
      senditf("%s ", argk[i]);
   }
   sendit("\n");
   return 1;
}

/* @quit */
static int quitcmd(FILE *input, BareosSocket *UA_sock)
{
   return 0;
}

/* @help */
static int helpcmd(FILE *input, BareosSocket *UA_sock)
{
   int i;
   for (i=0; i<comsize; i++) {
      senditf("  %-10s %s\n", commands[i].key, commands[i].help);
   }
   return 1;
}

/* @sleep secs */
static int sleepcmd(FILE *input, BareosSocket *UA_sock)
{
   if (argc > 1) {
      sleep(atoi(argk[1]));
   }
   return 1;
}

/* @time */
static int timecmd(FILE *input, BareosSocket *UA_sock)
{
   char sdt[50];

   bstrftimes(sdt, sizeof(sdt), time(NULL));
   senditf("%s\n", sdt);
   return 1;
}

/**
 * Send a line to the output file and or the terminal
 */
void senditf(const char *fmt,...)
{
   char buf[3000];
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), (char *)fmt, arg_ptr);
   va_end(arg_ptr);
   sendit(buf);
}

void sendit(const char *buf)
{
#ifdef CONIO_FIX
   char obuf[3000];
   if (output == stdout || teeout) {
      const char *p, *q;
      /*
       * Here, we convert every \n into \r\n because the
       *  terminal is in raw mode when we are using
       *  conio.
       */
      for (p=q=buf; (p=strchr(q, '\n')); ) {
         int len = p - q;
         if (len > 0) {
            memcpy(obuf, q, len);
         }
         memcpy(obuf+len, "\r\n", 3);
         q = ++p;                    /* point after \n */
         fputs(obuf, output);
      }
      if (*q) {
         fputs(q, output);
      }
      fflush(output);
   }
   if (output != stdout) {
      fputs(buf, output);
   }
#else
   fputs(buf, output);
   fflush(output);
   if (teeout) {
      fputs(buf, stdout);
      fflush(stdout);
   }
#endif
}
