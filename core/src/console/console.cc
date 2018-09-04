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
#include "console/console_conf.h"
#include "console_globals.h"
#include "include/jcr.h"
#include "lib/bnet.h"
#include "lib/qualified_resource_name_type_converter.h"

#ifdef HAVE_CONIO
#include "conio.h"
#else
#define ConInit(x)
#define ConTerm()
#define ConSetZedKeys()
#define trapctlc()
#define clrbrk()
#define usrbrk() 0
#endif

#if defined(HAVE_WIN32)
#define isatty(fd) (fd == 0)
#endif

using namespace console;

ConfigurationParser *my_config = nullptr;

// extern int rl_catch_signals;

/* Forward referenced functions */
static void TerminateConsole(int sig);
static int CheckResources();
int GetCmd(FILE *input, const char *prompt, BareosSocket *sock, int sec);
static int DoOutputcmd(FILE *input, BareosSocket *UA_sock);
void senditf(const char *fmt, ...);
void sendit(const char *buf);

extern "C" void GotSigstop(int sig);
extern "C" void GotSigcontinue(int sig);
extern "C" void GotSigtout(int sig);
extern "C" void GotSigtin(int sig);

/* Static variables */
static char *configfile      = NULL;
static BareosSocket *UA_sock = NULL;
static FILE *output          = stdout;
static bool teeout           = false; /* output to output and stdout */
static bool stop             = false;
static bool no_conio         = false;
static int timeout           = 0;
static int argc;
static int numdir;
static POOLMEM *args;
static char *argk[MAX_CMD_ARGS];
static char *argv[MAX_CMD_ARGS];
static bool file_selection = false;

/* Command prototypes */
static int Versioncmd(FILE *input, BareosSocket *UA_sock);
static int InputCmd(FILE *input, BareosSocket *UA_sock);
static int OutputCmd(FILE *input, BareosSocket *UA_sock);
static int TeeCmd(FILE *input, BareosSocket *UA_sock);
static int QuitCmd(FILE *input, BareosSocket *UA_sock);
static int HelpCmd(FILE *input, BareosSocket *UA_sock);
static int EchoCmd(FILE *input, BareosSocket *UA_sock);
static int TimeCmd(FILE *input, BareosSocket *UA_sock);
static int SleepCmd(FILE *input, BareosSocket *UA_sock);
static int ExecCmd(FILE *input, BareosSocket *UA_sock);
#ifdef HAVE_READLINE
static int EolCmd(FILE *input, BareosSocket *UA_sock);

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

#endif

static void usage()
{
  fprintf(stderr,
          _(PROG_COPYRIGHT "\nVersion: " VERSION " (" BDATE ") %s %s %s\n\n"
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
                           "\n"),
          2000, HOST_OS, DISTNAME, DISTVER);
}

extern "C" void GotSigstop(int sig) { stop = true; }

extern "C" void GotSigcontinue(int sig) { stop = false; }

extern "C" void GotSigtout(int sig)
{
  // printf("Got tout\n");
}

extern "C" void GotSigtin(int sig)
{
  // printf("Got tin\n");
}

static int ZedKeyscmd(FILE *input, BareosSocket *UA_sock)
{
  ConSetZedKeys();
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
    {N_("input"), InputCmd, _("input from file")},
    {N_("output"), OutputCmd, _("output to file")},
    {N_("quit"), QuitCmd, _("quit")},
    {N_("tee"), TeeCmd, _("output to file and terminal")},
    {N_("sleep"), SleepCmd, _("sleep specified time")},
    {N_("time"), TimeCmd, _("print current time")},
    {N_("version"), Versioncmd, _("print Console's version")},
    {N_("echo"), EchoCmd, _("echo command string")},
    {N_("exec"), ExecCmd, _("execute an external command")},
    {N_("exit"), QuitCmd, _("exit = quit")},
    {N_("zed_keys"), ZedKeyscmd, _("zed_keys = use zed keys instead of bash keys")},
    {N_("help"), HelpCmd, _("help listing")},
#ifdef HAVE_READLINE
    {N_("separator"), EolCmd, _("set command separator")},
#endif
};
#define comsize ((int)(sizeof(commands) / sizeof(struct cmdstruct)))

static int Do_a_command(FILE *input, BareosSocket *UA_sock)
{
  unsigned int i;
  int status;
  int found;
  int len;
  char *cmd;

  found  = 0;
  status = 1;

  Dmsg1(120, "Command: %s\n", UA_sock->msg);
  if (argc == 0) { return 1; }

  cmd = argk[0] + 1;
  if (*cmd == '#') { /* comment */
    return 1;
  }
  len = strlen(cmd);
  for (i = 0; i < comsize; i++) { /* search for command */
    if (bstrncasecmp(cmd, _(commands[i].key), len)) {
      status = (*commands[i].func)(input, UA_sock); /* go execute command */
      found  = 1;
      break;
    }
  }
  if (!found) {
    PmStrcat(UA_sock->msg, _(": is an invalid command\n"));
    UA_sock->message_length = strlen(UA_sock->msg);
    sendit(UA_sock->msg);
  }
  return status;
}

static void ReadAndProcessInput(FILE *input, BareosSocket *UA_sock)
{
  const char *prompt = "*";
  bool at_prompt     = false;
  int tty_input      = isatty(fileno(input));
  int status;
  btimer_t *tid = NULL;

  while (1) {
    if (at_prompt) { /* don't prompt multiple times */
      prompt = "";
    } else {
      prompt    = "*";
      at_prompt = true;
    }
    if (tty_input) {
      status = GetCmd(input, prompt, UA_sock, 30);
      if (usrbrk() == 1) { clrbrk(); }
      if (usrbrk()) { break; }
    } else {
      /*
       * Reading input from a file
       */
      int len = SizeofPoolMemory(UA_sock->msg) - 1;
      if (usrbrk()) { break; }
      if (fgets(UA_sock->msg, len, input) == NULL) {
        status = -1;
      } else {
        sendit(UA_sock->msg); /* echo to terminal */
        StripTrailingJunk(UA_sock->msg);
        UA_sock->message_length = strlen(UA_sock->msg);
        status                  = 1;
      }
    }
    if (status < 0) {
      break;                  /* error or interrupt */
    } else if (status == 0) { /* timeout */
      if (bstrcmp(prompt, "*")) {
        tid = StartBsockTimer(UA_sock, timeout);
        UA_sock->fsend(".messages");
        StopBsockTimer(tid);
      } else {
        continue;
      }
    } else {
      at_prompt = false;
      /*
       * @ => internal command for us
       */
      if (UA_sock->msg[0] == '@') {
        ParseArgs(UA_sock->msg, args, &argc, argk, argv, MAX_CMD_ARGS);
        if (!Do_a_command(input, UA_sock)) { break; }
        continue;
      }
      tid = StartBsockTimer(UA_sock, timeout);
      if (!UA_sock->send()) { /* send command */
        StopBsockTimer(tid);
        break; /* error */
      }
      StopBsockTimer(tid);
    }

    if (bstrcmp(UA_sock->msg, ".quit") || bstrcmp(UA_sock->msg, ".exit")) { break; }

    tid = StartBsockTimer(UA_sock, timeout);
    while ((status = UA_sock->recv()) >= 0 ||
           ((status == BNET_SIGNAL) &&
            ((UA_sock->message_length != BNET_EOD) && (UA_sock->message_length != BNET_MAIN_PROMPT) &&
             (UA_sock->message_length != BNET_SUB_PROMPT)))) {
      if (status == BNET_SIGNAL) {
        if (UA_sock->message_length == BNET_START_RTREE) {
          file_selection = true;
        } else if (UA_sock->message_length == BNET_END_RTREE) {
          file_selection = false;
        }
        continue;
      }

      if (at_prompt) {
        if (!stop) { sendit("\n"); }
        at_prompt = false;
      }

      /*
       * Suppress output if running in background or user hit ctl-c
       */
      if (!stop && !usrbrk()) {
        if (UA_sock->msg) { sendit(UA_sock->msg); }
      }
    }
    StopBsockTimer(tid);

    if (usrbrk() > 1) {
      break;
    } else {
      clrbrk();
    }
    if (!stop) { fflush(stdout); }

    if (IsBnetStop(UA_sock)) {
      break; /* error or term */
    } else if (status == BNET_SIGNAL) {
      if (UA_sock->message_length == BNET_SUB_PROMPT) { at_prompt = true; }
      Dmsg1(100, "Got poll %s\n", BnetSigToAscii(UA_sock));
    }
  }
}

/**
 * Call-back for reading a passphrase for an encrypted PEM file
 * This function uses getpass(),
 *  which uses a static buffer and is NOT thread-safe.
 */
static int TlsPemCallback(char *buf, int size, const void *userdata)
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
#include "lib/bsignal.h"
#include "lib/edit.h"
#include "lib/tls_openssl.h"
#include "readline/history.h"
#include "readline/readline.h"

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
    ret = (char *)malloc((len + 1) * sizeof(char));
    memcpy(ret, rl_line_buffer, len);
    ret[len] = 0;
  }
  return ret;
}

/**
 * Return the command before the current point.
 * Set nb to the number of command to skip
 */
static char *get_previous_keyword(int current_point, int nb)
{
  int i, end = -1, start, inquotes = 0;
  char *s = NULL;

  while (nb-- >= 0) {
    /*
     * First we look for a space before the current word
     */
    for (i = current_point; i >= 0; i--) {
      if (rl_line_buffer[i] == ' ' || rl_line_buffer[i] == '=') { break; }
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
    if (end == -1) { return NULL; }

    /*
     * Look for the start of the command
     */
    for (start = end; start > 0; start--) {
      if (rl_line_buffer[start] == '"') { inquotes = !inquotes; }
      if ((rl_line_buffer[start - 1] == ' ') && inquotes == 0) { break; }
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
    items = (ItemList *)malloc(sizeof(ItemList));
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

  if (len <= 0) { return; }
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
    buf  = CheckPoolMemorySize(buf, size + 1);
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
void GetArguments(const char *what)
{
  regex_t preg;
  POOLMEM *buf;
  int rc;
  init_items();

  rc = regcomp(&preg, "(([a-z_]+=)|([a-z]+)( |$))", REG_EXTENDED);
  if (rc != 0) { return; }

  buf = GetPoolMemory(PM_MESSAGE);
  UA_sock->fsend(".help item=%s", what);
  while (UA_sock->recv() > 0) {
    StripTrailingJunk(UA_sock->msg);
    match_kw(&preg, UA_sock->msg, UA_sock->message_length, buf);
  }
  FreePoolMemory(buf);
  regfree(&preg);
}

/* retreive a simple list (.pool, .client) and store it into items */
void GetItems(const char *what)
{
  init_items();

  UA_sock->fsend("%s", what);
  while (UA_sock->recv() > 0) {
    StripTrailingJunk(UA_sock->msg);
    items->list.append(bstrdup(UA_sock->msg));
  }
}

typedef enum
{
  ITEM_ARG, /* item with simple list like .jobs */
  ITEM_HELP /* use help item=xxx and detect all arguments */
} cpl_item_t;

/* Generator function for command completion.  STATE lets us know whether
 * to start from scratch; without any state (i.e. STATE == 0), then we
 * start at the top of the list.
 */
static char *item_generator(const char *text, int state, const char *item, cpl_item_t type)
{
  static int list_index, len;
  char *name;

  /* If this is a new word to complete, initialize now.  This includes
   * saving the length of TEXT for efficiency, and initializing the index
   *  variable to 0.
   */
  if (!state) {
    list_index = 0;
    len        = strlen(text);
    switch (type) {
      case ITEM_ARG:
        GetItems(item);
        break;
      case ITEM_HELP:
        GetArguments(item);
        break;
    }
  }

  /* Return the next name which partially matches from the command list. */
  while (items && list_index < items->list.size()) {
    name = (char *)items->list[list_index];
    list_index++;

    if (bstrncmp(name, text, len)) {
      char *ret = (char *)actuallymalloc(strlen(name) + 1);
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
static char *dummy_completion_function(const char *text, int state) { return NULL; }

struct cpl_keywords_t {
  const char *key;
  const char *cmd;
  bool file_selection;
};

static struct cpl_keywords_t cpl_keywords[] = {{"pool=", ".pool", false},
                                               {"nextpool=", ".pool", false},
                                               {"fileset=", ".fileset", false},
                                               {"client=", ".client", false},
                                               {"jobdefs=", ".jobdefs", false},
                                               {"job=", ".jobs", false},
                                               {"restore_job=", ".jobs type=R", false},
                                               {"level=", ".level", false},
                                               {"storage=", ".storage", false},
                                               {"schedule=", ".schedule", false},
                                               {"volume=", ".media", false},
                                               {"oldvolume=", ".media", false},
                                               {"volstatus=", ".volstatus", false},
                                               {"catalog=", ".catalogs", false},
                                               {"message=", ".msgs", false},
                                               {"profile=", ".profiles", false},
                                               {"actiononpurge=", ".actiononpurge", false},
                                               {"ls", ".ls", true},
                                               {"cd", ".lsdir", true},
                                               {"add", ".ls", true},
                                               {"mark", ".ls", true},
                                               {"m", ".ls", true},
                                               {"delete", ".lsmark", true},
                                               {"unmark", ".lsmark", true}};
#define key_size ((int)(sizeof(cpl_keywords) / sizeof(struct cpl_keywords_t)))

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
  s   = get_previous_keyword(start, 0);
  cmd = get_first_keyword();
  if (s) {
    for (int i = 0; i < key_size; i++) {
      /*
       * See if this keyword is allowed with the current file_selection setting.
       */
      if (cpl_keywords[i].file_selection != file_selection) { continue; }

      if (Bstrcasecmp(s, cpl_keywords[i].key)) {
        cpl_item = cpl_keywords[i].cmd;
        cpl_type = ITEM_ARG;
        matches  = rl_completion_matches(text, cpl_generator);
        found    = true;
        break;
      }
    }

    if (!found) { /* we try to get help with the first command */
      cpl_item = cmd;
      cpl_type = ITEM_HELP;
      /* we don't want to append " " at the end */
      rl_completion_suppress_append = true;
      matches                       = rl_completion_matches(text, cpl_generator);
    }
    free(s);
  } else { /* nothing on the line, display all commands */
    cpl_item = ".help all";
    cpl_type = ITEM_ARG;
    matches  = rl_completion_matches(text, cpl_generator);
  }
  if (cmd) { free(cmd); }
  return (matches);
}

static char eol = '\0';
static int EolCmd(FILE *input, BareosSocket *UA_sock)
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
int GetCmd(FILE *input, const char *prompt, BareosSocket *sock, int sec)
{
  static char *line     = NULL;
  static char *next     = NULL;
  static int do_history = 0;
  char *command;

  do_history       = 0;
  rl_catch_signals = 0; /* do it ourselves */
  /* Here, readline does ***real*** malloc
   * so, be we have to use the real free
   */
  line = readline((char *)prompt); /* cast needed for old readlines */
  if (!line) { return -1; /* error return and exit */ }
  StripTrailingJunk(line);
  command = line;

  /*
   * Split "line" into multiple commands separated by the eol character.
   *   Each part is pointed to by "next" until finally it becomes null.
   */
  if (eol == '\0') {
    next = NULL;
  } else {
    next = strchr(command, eol);
    if (next) { *next = '\0'; }
  }
  if (command != line && isatty(fileno(input))) { senditf("%s%s\n", prompt, command); }

  sock->message_length = PmStrcpy(sock->msg, command);
  if (sock->message_length) { do_history++; }

  if (!next) {
    if (do_history) { add_history(line); }
    Actuallyfree(line); /* allocated by readline() malloc */
    line = NULL;
  }
  return 1; /* OK */
}

#else /* no readline, do it ourselves */

#ifdef HAVE_CONIO
static bool bisatty(int fd)
{
  if (no_conio) { return false; }
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
int GetCmd(FILE *input, const char *prompt, BareosSocket *sock, int sec)
{
  int len;

  if (!stop) {
    if (output == stdout || teeout) { sendit(prompt); }
  }

again:
  len = SizeofPoolMemory(sock->msg) - 1;
  if (stop) {
    sleep(1);
    goto again;
  }

#ifdef HAVE_CONIO
  if (bisatty(fileno(input))) {
    InputLine(sock->msg, len);
    goto ok_out;
  }
#endif

  if (input == stdin) {
    if (win32_cgets(sock->msg, len) == NULL) { return -1; }
  } else {
    if (fgets(sock->msg, len, input) == NULL) { return -1; }
  }

  if (usrbrk()) { clrbrk(); }

  StripTrailingJunk(sock->msg);
  sock->message_length = strlen(sock->msg);

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
int GetCmd(FILE *input, const char *prompt, BareosSocket *sock, int sec)
{
  int len;

  if (!stop) {
    if (output == stdout || teeout) { sendit(prompt); }
  }

again:
  switch (WaitForReadableFd(fileno(input), sec, true)) {
    case 0:
      return 0; /* timeout */
    case -1:
      return -1; /* error */
    default:
      len = SizeofPoolMemory(sock->msg) - 1;
      if (stop) {
        sleep(1);
        goto again;
      }
#ifdef HAVE_CONIO
      if (bisatty(fileno(input))) {
        InputLine(sock->msg, len);
        break;
      }
#endif
      if (fgets(sock->msg, len, input) == NULL) { return -1; }
      break;
  }

  if (usrbrk()) { clrbrk(); }

  StripTrailingJunk(sock->msg);
  sock->message_length = strlen(sock->msg);

  return 1;
}
#endif /* HAVE_WIN32 */
#endif /* ! HAVE_READLINE */

static int ConsoleUpdateHistory(const char *histfile)
{
  int ret = 0;

#ifdef HAVE_READLINE
  int max_history_length, truncate_entries;

  /*
   * Calculate how much we should keep in the current history file.
   */
  max_history_length = (me) ? me->history_length : 100;
  truncate_entries   = max_history_length - history_length;
  if (truncate_entries < 0) { truncate_entries = 0; }

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

static int ConsoleInitHistory(const char *histfile)
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
  rl_completion_entry_function     = dummy_completion_function;
  rl_attempted_completion_function = readline_completion;
  rl_filename_completion_desired   = 0;
  stifle_history(max_history_length);
#endif

  return ret;
}

static bool SelectDirector(const char *director, DirectorResource **ret_dir, ConsoleResource **ret_cons)
{
  int numcon = 0, numdir = 0;
  int i = 0, item = 0;
  BareosSocket *UA_sock;
  DirectorResource *director_resource = NULL;
  ConsoleResource *console_resource   = NULL;

  *ret_cons = NULL;
  *ret_dir  = NULL;

  LockRes();
  numdir = 0;
  foreach_res(director_resource, R_DIRECTOR) { numdir++; }
  numcon = 0;
  foreach_res(console_resource, R_CONSOLE) { numcon++; }
  UnlockRes();

  if (numdir == 1) { /* No choose */
    director_resource = (DirectorResource *)my_config->GetNextRes(R_DIRECTOR, NULL);
  }

  if (director) { /* Command line choice overwrite the no choose option */
    LockRes();
    foreach_res(director_resource, R_DIRECTOR)
    {
      if (bstrcmp(director_resource->name(), director)) { break; }
    }
    UnlockRes();
    if (!director_resource) { /* Can't find Director used as argument */
      senditf(_("Can't find %s in Director list\n"), director);
      return 0;
    }
  }

  if (!director_resource) { /* prompt for director */
    UA_sock = New(BareosSocketTCP);
  try_again:
    sendit(_("Available Directors:\n"));
    LockRes();
    numdir = 0;
    foreach_res(director_resource, R_DIRECTOR)
    {
      senditf(_("%2d:  %s at %s:%d\n"), 1 + numdir++, director_resource->name(), director_resource->address,
              director_resource->DIRport);
    }
    UnlockRes();
    if (GetCmd(stdin, _("Select Director by entering a number: "), UA_sock, 600) < 0) {
      WSACleanup(); /* Cleanup Windows sockets */
      return 0;
    }
    if (!Is_a_number(UA_sock->msg)) {
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
    for (i = 0; i < item; i++) {
      director_resource =
          (DirectorResource *)my_config->GetNextRes(R_DIRECTOR, (CommonResourceHeader *)director_resource);
    }
    UnlockRes();
  }

  /*
   * Look for a console linked to this director
   */
  LockRes();
  for (i = 0; i < numcon; i++) {
    console_resource =
        (ConsoleResource *)my_config->GetNextRes(R_CONSOLE, (CommonResourceHeader *)console_resource);
    if (console_resource->director && bstrcmp(console_resource->director, director_resource->name())) {
      break;
    }
    console_resource = NULL;
  }

  /*
   * Look for the first non-linked console
   */
  if (console_resource == NULL) {
    for (i = 0; i < numcon; i++) {
      console_resource =
          (ConsoleResource *)my_config->GetNextRes(R_CONSOLE, (CommonResourceHeader *)console_resource);
      if (console_resource->director == NULL) break;
      console_resource = NULL;
    }
  }

  /*
   * If no console, take first one
   */
  if (!console_resource) {
    console_resource = (ConsoleResource *)my_config->GetNextRes(R_CONSOLE, (CommonResourceHeader *)NULL);
  }
  UnlockRes();

  *ret_dir  = director_resource;
  *ret_cons = console_resource;

  return 1;
}

namespace console {
BareosSocket *ConnectToDirector(JobControlRecord &jcr, utime_t heart_beat, char *errmsg, int errmsg_len)
{
  BareosSocketTCP *UA_sock = New(BareosSocketTCP);
  if (!UA_sock->connect(NULL, 5, 15, heart_beat, "Director daemon", director_resource->address, NULL,
                        director_resource->DIRport, false)) {
    delete UA_sock;
    TerminateConsole(0);
    return nullptr;
  }
  jcr.dir_bsock = UA_sock;

  const char *name;
  s_password *password = NULL;
  TlsResource *tls_configuration;

  if (console_resource) {
    name = console_resource->name();
    ASSERT(console_resource->password.encoding == p_encoding_md5);
    password          = &console_resource->password;
    tls_configuration = dynamic_cast<TlsResource *>(console_resource);
  } else { /* default console */
    name = "*UserAgent*";
    ASSERT(director_resource->password.encoding == p_encoding_md5);
    password          = &director_resource->password;
    tls_configuration = dynamic_cast<TlsResource *>(director_resource);
  }

  std::string qualified_resource_name;
  if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(name, my_config->r_own_,
                                                                            qualified_resource_name)) {
    sendit("Could not generate qualified resource name\n");
    TerminateConsole(0);
    return nullptr;
  }

  if (!UA_sock->DoTlsHandshake(4, tls_configuration, false, qualified_resource_name.c_str(), password->value,
                               &jcr)) {
    sendit(errmsg);
    TerminateConsole(0);
    return nullptr;
  }

  if (!UA_sock->AuthenticateWithDirector(&jcr, name, *password, errmsg, errmsg_len, director_resource)) {
    sendit(errmsg);
    TerminateConsole(0);
    return nullptr;
  }
  return UA_sock;
}
} /* namespace console */
/*
 * Main Bareos Console -- User Interface Program
 */
int main(int argc, char *argv[])
{
  int ch;
  int errmsg_len;
  char *director = NULL;
  char errmsg[1024];
  bool list_directors       = false;
  bool no_signals           = false;
  bool test_config          = false;
  bool export_config        = false;
  bool export_config_schema = false;
  JobControlRecord jcr;
  PoolMem history_file;
  utime_t heart_beat;

  errmsg_len = sizeof(errmsg);
  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  LmgrInitThread();
  MyNameIs(argc, argv, "bconsole");
  InitMsg(NULL, NULL);
  working_directory = "/tmp";
  args              = GetPoolMemory(PM_FNAME);

  while ((ch = getopt(argc, argv, "D:lc:d:nstu:x:?")) != -1) {
    switch (ch) {
      case 'D': /* Director */
        if (director) { free(director); }
        director = bstrdup(optarg);
        break;

      case 'l':
        list_directors = true;
        test_config    = true;
        break;

      case 'c': /* configuration file */
        if (configfile != NULL) { free(configfile); }
        configfile = bstrdup(optarg);
        break;

      case 'd':
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 'n': /* no conio */
        no_conio = true;
        break;

      case 's': /* turn off signals */
        no_signals = true;
        break;

      case 't':
        test_config = true;
        break;

      case 'u':
        timeout = atoi(optarg);
        break;

      case 'x': /* export configuration/schema and exit */
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

  if (!no_signals) { InitSignals(TerminateConsole); }

#if !defined(HAVE_WIN32)
  /* Override Bareos default signals */
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, GotSigstop);
  signal(SIGCONT, GotSigcontinue);
  signal(SIGTTIN, GotSigtin);
  signal(SIGTTOU, GotSigtout);
  trapctlc();
#endif

  OSDependentInit();

  if (argc) {
    usage();
    exit(1);
  }

  if (export_config_schema) {
    PoolMem buffer;

    my_config = InitConsConfig(configfile, M_ERROR_TERM);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());
    exit(0);
  }

  my_config = InitConsConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  if (export_config) {
    my_config->DumpResources(PrintMessage, NULL);
    exit(0);
  }

  if (InitCrypto() != 0) { Emsg0(M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n")); }

  if (!CheckResources()) {
    Emsg1(M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"),
          my_config->get_base_config_path().c_str());
  }

  if (!no_conio) { ConInit(stdin); }

  if (list_directors) {
    LockRes();
    foreach_res(director_resource, R_DIRECTOR) { senditf("%s\n", director_resource->name()); }
    UnlockRes();
  }

  if (test_config) {
    TerminateConsole(0);
    exit(0);
  }

  memset(&jcr, 0, sizeof(jcr));

  (void)WSA_Init(); /* Initialize Windows sockets */

  StartWatchdog(); /* Start socket watchdog */

  if (!SelectDirector(director, &director_resource, &console_resource)) { return 1; }

  senditf(_("Connecting to Director %s:%d\n"), director_resource->address, director_resource->DIRport);

  if (director_resource->heartbeat_interval) {
    heart_beat = director_resource->heartbeat_interval;
  } else if (console_resource) {
    heart_beat = console_resource->heartbeat_interval;
  } else {
    heart_beat = 0;
  }

  UA_sock = ConnectToDirector(jcr, heart_beat, errmsg, errmsg_len);
  if (!UA_sock) { return 1; }

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

    PmStrcpy(UA_sock->msg, env);
    PmStrcat(UA_sock->msg, "/.bconsolerc");
    fp = fopen(UA_sock->msg, "rb");
    if (fp) {
      ReadAndProcessInput(fp, UA_sock);
      fclose(fp);
    }
  }

  /*
   * See if there is an explicit setting of a history file to use.
   */
  if (me && me->history_file) {
    PmStrcpy(history_file, me->history_file);
    ConsoleInitHistory(history_file.c_str());
  } else {
    if (env) {
      PmStrcpy(history_file, env);
      PmStrcat(history_file, "/.bconsole_history");
      ConsoleInitHistory(history_file.c_str());
    } else {
      PmStrcpy(history_file, "");
    }
  }

  ReadAndProcessInput(stdin, UA_sock);

  if (UA_sock) {
    UA_sock->signal(BNET_TERMINATE); /* send EOF */
    UA_sock->close();
  }

  if (history_file.size()) { ConsoleUpdateHistory(history_file.c_str()); }

  TerminateConsole(0);
  return 0;
}

/* Cleanup and then exit */
static void TerminateConsole(int sig)
{
  static bool already_here = false;

  if (already_here) { /* avoid recursive temination problems */
    exit(1);
  }
  already_here = true;
  StopWatchdog();
  delete my_config;
  my_config = NULL;
  CleanupCrypto();
  FreePoolMemory(args);
  if (!no_conio) { ConTerm(); }
  WSACleanup(); /* Cleanup Windows sockets */
  LmgrCleanupMain();

  if (sig != 0) { exit(1); }
  return;
}

/**
 * Make a quick check to see that we have all the resources needed.
 */
static int CheckResources()
{
  bool OK = true;
  DirectorResource *director;

  LockRes();

  numdir = 0;
  foreach_res(director, R_DIRECTOR) { numdir++; }

  if (numdir == 0) {
    const std::string &configfile = my_config->get_base_config_path();
    Emsg1(M_FATAL, 0,
          _("No Director resource defined in %s\n"
            "Without that I don't how to speak to the Director :-(\n"),
          configfile.c_str());
    OK = false;
  }

  me = (ConsoleResource *)my_config->GetNextRes(R_CONSOLE, NULL);

  UnlockRes();

  return OK;
}

/* @version */
static int Versioncmd(FILE *input, BareosSocket *UA_sock)
{
  senditf("Version: " VERSION " (" BDATE ") %s %s %s\n", HOST_OS, DISTNAME, DISTVER);
  return 1;
}

/* @input <input-filename> */
static int InputCmd(FILE *input, BareosSocket *UA_sock)
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
    BErrNo be;
    senditf(_("Cannot open file %s for input. ERR=%s\n"), argk[1], be.bstrerror());
    return 1;
  }
  ReadAndProcessInput(fd, UA_sock);
  fclose(fd);
  return 1;
}

/* @tee <output-filename> */
/* Send output to both terminal and specified file */
static int TeeCmd(FILE *input, BareosSocket *UA_sock)
{
  teeout = true;
  return DoOutputcmd(input, UA_sock);
}

/* @output <output-filename> */
/* Send output to specified "file" */
static int OutputCmd(FILE *input, BareosSocket *UA_sock)
{
  teeout = false;
  return DoOutputcmd(input, UA_sock);
}

static int DoOutputcmd(FILE *input, BareosSocket *UA_sock)
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
  if (argc == 3) { mode = argk[2]; }
  fd = fopen(argk[1], mode);
  if (!fd) {
    BErrNo be;
    senditf(_("Cannot open file %s for output. ERR=%s\n"), argk[1], be.bstrerror(errno));
    return 1;
  }
  output = fd;
  return 1;
}

/**
 * @exec "some-command" [wait-seconds]
 */
static int ExecCmd(FILE *input, BareosSocket *UA_sock)
{
  Bpipe *bpipe;
  char line[5000];
  int status;
  int wait = 0;

  if (argc > 3) {
    sendit(_("Too many arguments. Enclose command in double quotes.\n"));
    return 1;
  }
  if (argc == 3) { wait = atoi(argk[2]); }
  bpipe = OpenBpipe(argk[1], wait, "r");
  if (!bpipe) {
    BErrNo be;
    senditf(_("Cannot popen(\"%s\", \"r\"): ERR=%s\n"), argk[1], be.bstrerror(errno));
    return 1;
  }

  while (fgets(line, sizeof(line), bpipe->rfd)) { senditf("%s", line); }
  status = CloseBpipe(bpipe);
  if (status != 0) {
    BErrNo be;
    be.SetErrno(status);
    senditf(_("Autochanger error: ERR=%s\n"), be.bstrerror());
  }
  return 1;
}

/* @echo xxx yyy */
static int EchoCmd(FILE *input, BareosSocket *UA_sock)
{
  for (int i = 1; i < argc; i++) { senditf("%s ", argk[i]); }
  sendit("\n");
  return 1;
}

/* @quit */
static int QuitCmd(FILE *input, BareosSocket *UA_sock) { return 0; }

/* @help */
static int HelpCmd(FILE *input, BareosSocket *UA_sock)
{
  int i;
  for (i = 0; i < comsize; i++) { senditf("  %-10s %s\n", commands[i].key, commands[i].help); }
  return 1;
}

/* @sleep secs */
static int SleepCmd(FILE *input, BareosSocket *UA_sock)
{
  if (argc > 1) { sleep(atoi(argk[1])); }
  return 1;
}

/* @time */
static int TimeCmd(FILE *input, BareosSocket *UA_sock)
{
  char sdt[50];

  bstrftimes(sdt, sizeof(sdt), time(NULL));
  senditf("%s\n", sdt);
  return 1;
}

/**
 * Send a line to the output file and or the terminal
 */
void senditf(const char *fmt, ...)
{
  char buf[3000];
  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  Bvsnprintf(buf, sizeof(buf), (char *)fmt, arg_ptr);
  va_end(arg_ptr);
  sendit(buf);
}

void sendit(const char *buf)
{
  fputs(buf, output);
  fflush(output);
  if (teeout) {
    fputs(buf, stdout);
    fflush(stdout);
  }
}
