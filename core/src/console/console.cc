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
#include "console/console_globals.h"
#include "console/auth_pam.h"
#include "console/console_output.h"
#include "console/connect_to_director.h"
#include "include/jcr.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/bsock_tcp.h"
#include "lib/bstringlist.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/watchdog.h"
#include <stdio.h>
#include <fstream>
#include <string>

#define ConInit(x)
#define ConTerm()
#define ConSetZedKeys()
#define trapctlc()
#define clrbrk()
#define usrbrk() 0

#if defined(HAVE_WIN32)
#define isatty(fd) (fd == 0)
#endif

using namespace console;

static void TerminateConsole(int sig);
static int CheckResources();
int GetCmd(FILE* input, const char* prompt, BareosSocket* sock, int sec);
static int DoOutputcmd(FILE* input, BareosSocket* UA_sock);

extern "C" void GotSigstop(int sig);
extern "C" void GotSigcontinue(int sig);
extern "C" void GotSigtout(int sig);
extern "C" void GotSigtin(int sig);

static char* configfile = NULL;
static BareosSocket* UA_sock = NULL;
static bool stop = false;
static int timeout = 0;
static int argc;
static int numdir;
static POOLMEM* args;
static char* argk[MAX_CMD_ARGS];
static char* argv[MAX_CMD_ARGS];
static bool file_selection = false;

#if defined(HAVE_PAM)
static bool force_send_pam_credentials_unencrypted = false;
static bool use_pam_credentials_file = false;
static std::string pam_credentials_filename;
static const std::string program_arguments{"D:lc:d:np:ostu:x:?"};
#else
static const std::string program_arguments{"D:lc:d:nstu:x:?"};
#endif

/* Command prototypes */
static int Versioncmd(FILE* input, BareosSocket* UA_sock);
static int InputCmd(FILE* input, BareosSocket* UA_sock);
static int OutputCmd(FILE* input, BareosSocket* UA_sock);
static int TeeCmd(FILE* input, BareosSocket* UA_sock);
static int QuitCmd(FILE* input, BareosSocket* UA_sock);
static int HelpCmd(FILE* input, BareosSocket* UA_sock);
static int EchoCmd(FILE* input, BareosSocket* UA_sock);
static int TimeCmd(FILE* input, BareosSocket* UA_sock);
static int SleepCmd(FILE* input, BareosSocket* UA_sock);
static int ExecCmd(FILE* input, BareosSocket* UA_sock);
static int EolCmd(FILE* input, BareosSocket* UA_sock);

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

static void usage()
{
  fprintf(
      stderr,
      _(PROG_COPYRIGHT
        "\nVersion: " VERSION " (" BDATE ") %s %s %s\n\n"
        "Usage: bconsole [-s] [-c config_file] [-d debug_level]\n"
        "        -D <dir>    select a Director\n"
        "        -l          list Directors defined\n"
        "        -c <path>   specify configuration file or directory\n"
#if defined(HAVE_PAM)
        "        -p <file>   specify pam credentials file\n"
        "        -o          send pam credentials over unencrypted connection\n"
#endif
        "        -d <nn>     set debug level to <nn>\n"
        "        -dt         print timestamp in debug output\n"
        "        -s          no signals\n"
        "        -u <nn>     set command execution timeout to <nn> seconds\n"
        "        -t          test - read configuration and exit\n"
        "        -xc         print configuration and exit\n"
        "        -xs         print configuration file schema in JSON format "
        "and exit\n"
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

static int ZedKeyscmd(FILE* input, BareosSocket* UA_sock)
{
  ConSetZedKeys();
  return 1;
}

/**
 * These are the @commands that run only in bconsole
 */
struct cmdstruct {
  const char* key;
  int (*func)(FILE* input, BareosSocket* UA_sock);
  const char* help;
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
    {N_("zed_keys"), ZedKeyscmd,
     _("zed_keys = use zed keys instead of bash keys")},
    {N_("help"), HelpCmd, _("help listing")},
    {N_("separator"), EolCmd, _("set command separator")},
};
#define comsize ((int)(sizeof(commands) / sizeof(struct cmdstruct)))

static int Do_a_command(FILE* input, BareosSocket* UA_sock)
{
  unsigned int i;
  int status;
  int found;
  int len;
  char* cmd;

  found = 0;
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
      found = 1;
      break;
    }
  }
  if (!found) {
    PmStrcat(UA_sock->msg, _(": is an invalid command\n"));
    UA_sock->message_length = strlen(UA_sock->msg);
    ConsoleOutput(UA_sock->msg);
  }
  return status;
}

static void ReadAndProcessInput(FILE* input, BareosSocket* UA_sock)
{
  const char* prompt = "*";
  bool at_prompt = false;
  int tty_input = isatty(fileno(input));
  int status;
  btimer_t* tid = NULL;

  while (1) {
    if (at_prompt) { /* don't prompt multiple times */
      prompt = "";
    } else {
      prompt = "*";
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
        ConsoleOutput(UA_sock->msg); /* echo to terminal */
        StripTrailingJunk(UA_sock->msg);
        UA_sock->message_length = strlen(UA_sock->msg);
        status = 1;
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

    if (bstrcmp(UA_sock->msg, ".quit") || bstrcmp(UA_sock->msg, ".exit")) {
      break;
    }

    tid = StartBsockTimer(UA_sock, timeout);
    while ((status = UA_sock->recv()) >= 0 ||
           ((status == BNET_SIGNAL) &&
            ((UA_sock->message_length != BNET_EOD) &&
             (UA_sock->message_length != BNET_MAIN_PROMPT) &&
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
        if (!stop) { ConsoleOutput("\n"); }
        at_prompt = false;
      }

      /*
       * Suppress output if running
       * in background or user hit ctl-c
       */
      if (!stop && !usrbrk()) {
        if (UA_sock->msg) { ConsoleOutput(UA_sock->msg); }
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


#include <readline/readline.h>
#include <readline/history.h>
#include "lib/edit.h"
#include "lib/tls_openssl.h"
#include "lib/bsignal.h"

/**
 * Get the first keyword of the line
 */
static char* get_first_keyword()
{
  char* ret = NULL;
  int len;
  char* first_space = strchr(rl_line_buffer, ' ');
  if (first_space) {
    len = first_space - rl_line_buffer;
    ret = (char*)malloc((len + 1) * sizeof(char));
    memcpy(ret, rl_line_buffer, len);
    ret[len] = 0;
  }
  return ret;
}

/**
 * Return the command before the current point.
 * Set nb to the number of command to skip
 */
static char* get_previous_keyword(int current_point, int nb)
{
  int i, end = -1, start, inquotes = 0;
  char* s = NULL;

  while (nb-- >= 0) {
    /*
     * First we look for a space before the current word
     */
    for (i = current_point; i >= 0; i--) {
      if (rl_line_buffer[i] == ' ' || rl_line_buffer[i] == '=') { break; }
    }

    for (; i >= 0; i--) {
      if (rl_line_buffer[i] != ' ') {
        end = i; /* end of command */
        break;
      }
    }

    if (end == -1) { return NULL; /* no end found */ }

    for (start = end; start > 0; start--) {
      if (rl_line_buffer[start] == '"') { inquotes = !inquotes; }
      if ((rl_line_buffer[start - 1] == ' ') && inquotes == 0) { break; }
      current_point = start; /* start of command */
    }
  }

  s = (char*)malloc(end - start + 2);
  memcpy(s, rl_line_buffer + start, end - start + 1);
  s[end - start + 1] = 0;

  //  printf("=======> %i:%i <%s>\n", start, end, s);

  return s;
}

struct ItemList {
  alist list; /* holds the completion list */
};

static ItemList* items = NULL;
void init_items()
{
  if (!items) {
    items = (ItemList*)malloc(sizeof(ItemList));
    items = new (items) ItemList(); /* placement new instead of memset */
  } else {
    items->list.destroy();
    items->list.init();
  }
}

/**
 * Match a regexp and add the result to the items list
 * This function is recursive
 */
static void match_kw(regex_t* preg, const char* what, int len, POOLMEM*& buf)
{
  int rc, size;
  int nmatch = 20;
  regmatch_t pmatch[20];

  if (len <= 0) { return; }
  rc = regexec(preg, what, nmatch, pmatch, 0);
  if (rc == 0) {
    size = pmatch[1].rm_eo - pmatch[1].rm_so;
    buf = CheckPoolMemorySize(buf, size + 1);
    memcpy(buf, what + pmatch[1].rm_so, size);
    buf[size] = '\0';

    items->list.append(bstrdup(buf));

    /* search for next keyword */
    match_kw(preg, what + pmatch[1].rm_eo, len - pmatch[1].rm_eo, buf);
  }
}

/* fill the items list with the output of the help command */
void GetArguments(const char* what)
{
  regex_t preg;
  POOLMEM* buf;
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
static void GetItems(const char* what)
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

static char* item_generator(const char* text,
                            int state,
                            const char* item,
                            cpl_item_t type)
{
  static int list_index, len;
  char* name;

  if (!state) {
    list_index = 0;
    len = strlen(text);
    switch (type) {
      case ITEM_ARG:
        GetItems(item);
        break;
      case ITEM_HELP:
        GetArguments(item);
        break;
    }
  }

  while (items && list_index < items->list.size()) {
    name = (char*)items->list[list_index];
    list_index++;

    if (bstrncmp(name, text, len)) {
      char* ret = (char*)actuallymalloc(strlen(name) + 1);
      strcpy(ret, name);
      return ret;
    }
  }

  /* no match */
  return ((char*)NULL);
}

static const char* cpl_item;
static cpl_item_t cpl_type;

static char* cpl_generator(const char* text, int state)
{
  return item_generator(text, state, cpl_item, cpl_type);
}

/* do not use the default filename completion */
static char* dummy_completion_function(const char* text, int state)
{
  return NULL;
}

struct cpl_keywords_t {
  const char* key;
  const char* cmd;
  bool file_selection;
};

static struct cpl_keywords_t cpl_keywords[] = {
    {"pool=", ".pool", false},
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
static char** readline_completion(const char* text, int start, int end)
{
  bool found = false;
  char** matches;
  char *s, *cmd;
  matches = (char**)NULL;

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
      if (cpl_keywords[i].file_selection != file_selection) { continue; }

      if (Bstrcasecmp(s, cpl_keywords[i].key)) {
        cpl_item = cpl_keywords[i].cmd;
        cpl_type = ITEM_ARG;
        matches = rl_completion_matches(text, cpl_generator);
        found = true;
        break;
      }
    }

    if (!found) { /* try to get help with the first command */
      cpl_item = cmd;
      cpl_type = ITEM_HELP;
      /* do not append space at the end */
      rl_completion_suppress_append = true;
      matches = rl_completion_matches(text, cpl_generator);
    }
    free(s);
  } else { /* nothing on the line, display all commands */
    cpl_item = ".help all";
    cpl_type = ITEM_ARG;
    matches = rl_completion_matches(text, cpl_generator);
  }
  if (cmd) { free(cmd); }
  return (matches);
}

static char eol = '\0';
static int EolCmd(FILE* input, BareosSocket* UA_sock)
{
  if ((argc > 1) && (strchr("!$%&'()*+,-/:;<>?[]^`{|}~", argk[1][0]) != NULL)) {
    eol = argk[1][0];
  } else if (argc == 1) {
    eol = '\0';
  } else {
    ConsoleOutput(_("Illegal separator character.\n"));
  }
  return 1;
}

/**
 * Return 1 if OK
 *        0 if no input
 *       -1 error (must stop)
 */
int GetCmd(FILE* input, const char* prompt, BareosSocket* sock, int sec)
{
  static char* line = NULL;
  static char* next = NULL;
  static int do_history = 0;
  char* command;

  do_history = 0;
  rl_catch_signals = 0; /* do it ourselves */

  line = readline((char*)prompt); /* cast needed for old readlines */
  if (!line) { return -1; }
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
  if (command != line && isatty(fileno(input))) {
    ConsoleOutputFormat("%s%s\n", prompt, command);
  }

  sock->message_length = PmStrcpy(sock->msg, command);
  if (sock->message_length) { do_history++; }

  if (!next) {
    if (do_history) { add_history(line); }
    Actuallyfree(line); /* allocated by readline() malloc */
    line = NULL;
  }
  return 1; /* OK */
}

static int ConsoleUpdateHistory(const char* histfile)
{
  int ret = 0;

  int max_history_length, truncate_entries;

  max_history_length =
      (console_resource) ? console_resource->history_length : 100;
  truncate_entries = max_history_length - history_length;
  if (truncate_entries < 0) { truncate_entries = 0; }

  if (history_truncate_file(histfile, truncate_entries) == 0) {
    ret = append_history(history_length, histfile);
  } else {
    ret = write_history(histfile);
  }

  return ret;
}

static int ConsoleInitHistory(const char* histfile)
{
  int ret = 0;

  int max_history_length;

  using_history();

  max_history_length =
      (console_resource) ? console_resource->history_length : 100;
  history_truncate_file(histfile, max_history_length);

  ret = read_history(histfile);

  rl_completion_entry_function = dummy_completion_function;
  rl_attempted_completion_function = readline_completion;
  rl_filename_completion_desired = 0;
  stifle_history(max_history_length);

  return ret;
}

static bool SelectDirector(const char* director,
                           DirectorResource** ret_dir,
                           ConsoleResource** ret_cons)
{
  int numcon = 0, numdir = 0;
  int i = 0, item = 0;
  BareosSocket* UA_sock;
  DirectorResource* director_resource_tmp = NULL;
  ConsoleResource* console_resource_tmp = NULL;

  *ret_cons = NULL;
  *ret_dir = NULL;

  LockRes(console::my_config);
  numdir = 0;
  foreach_res (director_resource_tmp, R_DIRECTOR) {
    numdir++;
  }
  numcon = 0;
  foreach_res (console_resource_tmp, R_CONSOLE) {
    numcon++;
  }
  UnlockRes(my_config);

  if (numdir == 1) { /* No choose */
    director_resource_tmp =
        (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, NULL);
  }

  if (director) { /* Command line choice overwrite the no choose option */
    LockRes(my_config);
    foreach_res (director_resource_tmp, R_DIRECTOR) {
      if (bstrcmp(director_resource_tmp->resource_name_, director)) { break; }
    }
    UnlockRes(my_config);
    if (!director_resource_tmp) { /* Can't find Director used as argument */
      ConsoleOutputFormat(_("Can't find %s in Director list\n"), director);
      return 0;
    }
  }

  if (!director_resource_tmp) { /* prompt for director */
    UA_sock = New(BareosSocketTCP);
  try_again:
    ConsoleOutput(_("Available Directors:\n"));
    LockRes(my_config);
    numdir = 0;
    foreach_res (director_resource_tmp, R_DIRECTOR) {
      ConsoleOutputFormat(_("%2d:  %s at %s:%d\n"), 1 + numdir++,
                          director_resource_tmp->resource_name_,
                          director_resource_tmp->address,
                          director_resource_tmp->DIRport);
    }
    UnlockRes(my_config);
    if (GetCmd(stdin, _("Select Director by entering a number: "), UA_sock,
               600) < 0) {
      WSACleanup(); /* Cleanup Windows sockets */
      return 0;
    }
    if (!Is_a_number(UA_sock->msg)) {
      ConsoleOutputFormat(
          _("%s is not a number. You must enter a number between "
            "1 and %d\n"),
          UA_sock->msg, numdir);
      goto try_again;
    }
    item = atoi(UA_sock->msg);
    if (item < 0 || item > numdir) {
      ConsoleOutputFormat(_("You must enter a number between 1 and %d\n"),
                          numdir);
      goto try_again;
    }
    delete UA_sock;
    LockRes(my_config);
    for (i = 0; i < item; i++) {
      director_resource_tmp = (DirectorResource*)my_config->GetNextRes(
          R_DIRECTOR, (BareosResource*)director_resource_tmp);
    }
    UnlockRes(my_config);
  }

  /*
   * Look for a console linked to this director
   */
  LockRes(my_config);
  for (i = 0; i < numcon; i++) {
    console_resource_tmp = (ConsoleResource*)my_config->GetNextRes(
        R_CONSOLE, (BareosResource*)console_resource_tmp);
    if (console_resource_tmp->director &&
        bstrcmp(console_resource_tmp->director,
                director_resource_tmp->resource_name_)) {
      break;
    }
    console_resource_tmp = NULL;
  }

  /*
   * Look for the first non-linked console
   */
  if (console_resource_tmp == NULL) {
    for (i = 0; i < numcon; i++) {
      console_resource_tmp = (ConsoleResource*)my_config->GetNextRes(
          R_CONSOLE, (BareosResource*)console_resource_tmp);
      if (console_resource_tmp->director == NULL) break;
      console_resource_tmp = NULL;
    }
  }

  /*
   * If no console, take first one
   */
  if (!console_resource_tmp) {
    console_resource_tmp = (ConsoleResource*)my_config->GetNextRes(
        R_CONSOLE, (BareosResource*)NULL);
  }
  UnlockRes(my_config);

  *ret_dir = director_resource_tmp;
  *ret_cons = console_resource_tmp;

  return 1;
}

#if defined(HAVE_PAM)
static BStringList ReadPamCredentialsFile(
    const std::string& pam_credentials_filename)
{
  std::ifstream s(pam_credentials_filename);
  std::string user, pw;
  if (!s.is_open()) {
    Emsg0(M_ERROR_TERM, 0, _("Could not open PAM credentials file.\n"));
    return BStringList();
  } else {
    std::getline(s, user);
    std::getline(s, pw);
    if (user.empty() || pw.empty()) {
      Emsg0(M_ERROR_TERM, 0, _("Could not read user or password.\n"));
      return BStringList();
    }
  }
  BStringList args;
  args << user << pw;
  return args;
}

static bool ExaminePamAuthentication(
    bool use_pam_credentials_file,
    const std::string& pam_credentials_filename)
{
  if (!UA_sock->tls_conn && !force_send_pam_credentials_unencrypted) {
    ConsoleOutput("Canceled because password would be sent unencrypted!\n");
    return false;
  }
  if (use_pam_credentials_file) {
    BStringList args(ReadPamCredentialsFile(pam_credentials_filename));
    if (args.empty()) { return false; }
    UA_sock->FormatAndSendResponseMessage(kMessageIdPamUserCredentials, args);
  } else {
    UA_sock->FormatAndSendResponseMessage(kMessageIdPamInteractive,
                                          std::string());
    if (!ConsolePamAuthenticate(stdin, UA_sock)) {
      TerminateConsole(0);
      return false;
    }
  }
  return true;
}
#endif /* HAVE_PAM */

/*
 * Main Bareos Console -- User Interface Program
 */
int main(int argc, char* argv[])
{
  int ch;
  char* director = NULL;
  bool list_directors = false;
  bool no_signals = false;
  bool test_config = false;
  bool export_config = false;
  bool export_config_schema = false;
  JobControlRecord jcr;
  PoolMem history_file;

  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  MyNameIs(argc, argv, "bconsole");
  InitMsg(NULL, NULL);
  working_directory = "/tmp";
  args = GetPoolMemory(PM_FNAME);

  while ((ch = getopt(argc, argv, program_arguments.c_str())) != -1) {
    switch (ch) {
      case 'D': /* Director */
        if (director) { free(director); }
        director = bstrdup(optarg);
        break;

      case 'l':
        list_directors = true;
        test_config = true;
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

#if defined(HAVE_PAM)
      case 'p':
        pam_credentials_filename = optarg;
        if (pam_credentials_filename.empty()) {
          Emsg0(M_ERROR_TERM, 0, _("No filename given for -p.\n"));
          usage();
        } else {
          if (FILE* f = fopen(pam_credentials_filename.c_str(), "r+")) {
            use_pam_credentials_file = true;
            fclose(f);
          } else { /* file cannot be opened, i.e. does not exist */
            Emsg0(M_ERROR_TERM, 0, _("Could not open file for -p.\n"));
          }
        }
        break;

      case 'o':
        force_send_pam_credentials_unencrypted = true;
        break;
#endif /* HAVE_PAM */

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
    TerminateConsole(0);
    exit(0);
  }

  if (InitCrypto() != 0) {
    Emsg0(M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
  }

  if (!CheckResources()) {
    Emsg1(M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"),
          my_config->get_base_config_path().c_str());
  }

  ConInit(stdin);

  if (list_directors) {
    LockRes(my_config);
    foreach_res (director_resource, R_DIRECTOR) {
      ConsoleOutputFormat("%s\n", director_resource->resource_name_);
    }
    UnlockRes(my_config);
  }

  if (test_config) {
    TerminateConsole(0);
    exit(0);
  }

  memset(&jcr, 0, sizeof(jcr));

  (void)WSA_Init(); /* Initialize Windows sockets */

  StartWatchdog(); /* Start socket watchdog */

  if (!SelectDirector(director, &director_resource, &console_resource)) {
    return 1;
  }

  ConsoleOutputFormat(_("Connecting to Director %s:%d\n"),
                      director_resource->address, director_resource->DIRport);

  utime_t heart_beat;
  if (director_resource->heartbeat_interval) {
    heart_beat = director_resource->heartbeat_interval;
  } else if (console_resource) {
    heart_beat = console_resource->heartbeat_interval;
  } else {
    heart_beat = 0;
  }

  uint32_t response_id;
  BStringList response_args;

  UA_sock = ConnectToDirector(jcr, heart_beat, response_args, response_id);
  if (!UA_sock) { return 1; }

  UA_sock->OutputCipherMessageString(ConsoleOutput);

  if (response_id == kMessageIdPamRequired) {
#if defined(HAVE_PAM)
    if (!ExaminePamAuthentication(use_pam_credentials_file,
                                  pam_credentials_filename)) {
      TerminateConsole(0);
      return 1;
    }
    response_args.clear();
    if (!UA_sock->ReceiveAndEvaluateResponseMessage(response_id,
                                                    response_args)) {
      TerminateConsole(0);
      return 1;
    }
#else
    Dmsg0(200, "This console does not have the pam feature\n");
    TerminateConsole(0);
    return 1;
#endif /* HAVE_PAM */
  }    /* kMessageIdPamRequired */

  if (response_id == kMessageIdOk) {
    ConsoleOutput(response_args.JoinReadable().c_str());
    ConsoleOutput("\n");
  }

  response_args.clear();
  if (!UA_sock->ReceiveAndEvaluateResponseMessage(response_id, response_args)) {
    Dmsg0(200, "Could not receive the response message\n");
    TerminateConsole(0);
    return 1;
  }

  if (response_id != kMessageIdInfoMessage) {
    Dmsg0(200, "Could not receive the response message\n");
    TerminateConsole(0);
    return 1;
  }
  response_args.PopFront();
  ConsoleOutput(response_args.JoinReadable().c_str());
  ConsoleOutput("\n");

  Dmsg0(40, "Opened connection with Director daemon\n");

  ConsoleOutput(_("\nEnter a period to cancel a command.\n"));

#if defined(HAVE_WIN32)
  char* env = getenv("USERPROFILE");
#else
  char* env = getenv("HOME");
#endif

  /*
   * Run commands in ~/.bconsolerc if any
   */
  if (env) {
    FILE* fp;

    PmStrcpy(UA_sock->msg, env);
    PmStrcat(UA_sock->msg, "/.bconsolerc");
    fp = fopen(UA_sock->msg, "rb");
    if (fp) {
      ReadAndProcessInput(fp, UA_sock);
      fclose(fp);
    }
  }

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
  ConTerm();
  WSACleanup(); /* Cleanup Windows sockets */

  if (sig != 0) { exit(1); }
  return;
}

static int CheckResources()
{
  bool OK = true;
  DirectorResource* director;

  LockRes(my_config);

  numdir = 0;
  foreach_res (director, R_DIRECTOR) {
    numdir++;
  }

  if (numdir == 0) {
    const std::string& configfile = my_config->get_base_config_path();
    Emsg1(M_FATAL, 0,
          _("No Director resource defined in %s\n"
            "Without that I don't how to speak to the Director :-(\n"),
          configfile.c_str());
    OK = false;
  }

  me = (ConsoleResource*)my_config->GetNextRes(R_CONSOLE, NULL);

  UnlockRes(my_config);

  return OK;
}

/* @version */
static int Versioncmd(FILE* input, BareosSocket* UA_sock)
{
  ConsoleOutputFormat("Version: " VERSION " (" BDATE ") %s %s %s\n", HOST_OS,
                      DISTNAME, DISTVER);
  return 1;
}

/* @input <input-filename> */
static int InputCmd(FILE* input, BareosSocket* UA_sock)
{
  FILE* fd;

  if (argc > 2) {
    ConsoleOutput(_("Too many arguments on input command.\n"));
    return 1;
  }
  if (argc == 1) {
    ConsoleOutput(_("First argument to input command must be a filename.\n"));
    return 1;
  }
  fd = fopen(argk[1], "rb");
  if (!fd) {
    BErrNo be;
    ConsoleOutputFormat(_("Cannot open file %s for input. ERR=%s\n"), argk[1],
                        be.bstrerror());
    return 1;
  }
  ReadAndProcessInput(fd, UA_sock);
  fclose(fd);
  return 1;
}

/* @tee <output-filename> */
/* Send output to both terminal and specified file */
static int TeeCmd(FILE* input, BareosSocket* UA_sock)
{
  EnableTeeOut();
  return DoOutputcmd(input, UA_sock);
}

/* @output <output-filename> */
/* Send output to specified "file" */
static int OutputCmd(FILE* input, BareosSocket* UA_sock)
{
  DisableTeeOut();
  return DoOutputcmd(input, UA_sock);
}

static int DoOutputcmd(FILE* input, BareosSocket* UA_sock)
{
  FILE* file;
  const char* mode = "a+b";

  if (argc > 3) {
    ConsoleOutput(_("Too many arguments on output/tee command.\n"));
    return 1;
  }
  if (argc == 1) {
    CloseTeeFile();
    return 1;
  }
  if (argc == 3) { mode = argk[2]; }
  file = fopen(argk[1], mode);
  if (!file) {
    BErrNo be;
    ConsoleOutputFormat(_("Cannot open file %s for output. ERR=%s\n"), argk[1],
                        be.bstrerror(errno));
    return 1;
  }
  SetTeeFile(file);
  return 1;
}

/**
 * @exec "some-command" [wait-seconds]
 */
static int ExecCmd(FILE* input, BareosSocket* UA_sock)
{
  Bpipe* bpipe;
  char line[5000];
  int status;
  int wait = 0;

  if (argc > 3) {
    ConsoleOutput(_("Too many arguments. Enclose command in double quotes.\n"));
    return 1;
  }
  if (argc == 3) { wait = atoi(argk[2]); }
  bpipe = OpenBpipe(argk[1], wait, "r");
  if (!bpipe) {
    BErrNo be;
    ConsoleOutputFormat(_("Cannot popen(\"%s\", \"r\"): ERR=%s\n"), argk[1],
                        be.bstrerror(errno));
    return 1;
  }

  while (fgets(line, sizeof(line), bpipe->rfd)) {
    ConsoleOutputFormat("%s", line);
  }
  status = CloseBpipe(bpipe);
  if (status != 0) {
    BErrNo be;
    be.SetErrno(status);
    ConsoleOutputFormat(_("Autochanger error: ERR=%s\n"), be.bstrerror());
  }
  return 1;
}

/* @echo xxx yyy */
static int EchoCmd(FILE* input, BareosSocket* UA_sock)
{
  for (int i = 1; i < argc; i++) { ConsoleOutputFormat("%s ", argk[i]); }
  ConsoleOutput("\n");
  return 1;
}

/* @quit */
static int QuitCmd(FILE* input, BareosSocket* UA_sock) { return 0; }

/* @help */
static int HelpCmd(FILE* input, BareosSocket* UA_sock)
{
  int i;
  for (i = 0; i < comsize; i++) {
    ConsoleOutputFormat("  %-10s %s\n", commands[i].key, commands[i].help);
  }
  return 1;
}

/* @sleep secs */
static int SleepCmd(FILE* input, BareosSocket* UA_sock)
{
  if (argc > 1) { sleep(atoi(argk[1])); }
  return 1;
}

/* @time */
static int TimeCmd(FILE* input, BareosSocket* UA_sock)
{
  char sdt[50];

  bstrftimes(sdt, sizeof(sdt), time(NULL));
  ConsoleOutputFormat("%s\n", sdt);
  return 1;
}
