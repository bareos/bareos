/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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
 * Kern Sibbald, August 2007
 *
 * Note, some of the original Bareos Windows startup and service handling code
 * was derived from VNC code that was used in apcupsd then ported to
 * Bareos.  However, since then the code has been significantly enhanced
 * and largely rewritten.
 *
 * Evidently due to the nature of Windows startup code and service
 * handling code, certain similarities remain. Thanks to the original
 * VNC authors.
 *
 * This is a generic main routine, which is used by all three
 * of the daemons. Each one compiles it with slightly different
 * #defines.
 */

#include "include/bareos.h"
#include "win32.h"
#include "who.h"
#include <signal.h>
#include <pthread.h>

#undef _WIN32_IE
#ifdef MINGW64
#  define _WIN32_IE 0x0501
#else
#  define _WIN32_IE 0x0401
#endif  // MINGW64
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#include <commctrl.h>


/* Globals */
HINSTANCE appInstance;
DWORD mainthreadId;
bool opt_debug = false;
bool have_service_api;
DWORD service_thread_id = 0;

#define MAX_COMMAND_ARGS 100
static char* command_args[MAX_COMMAND_ARGS] = {(char*)LC_APP_NAME, NULL};
static int num_command_args = 1;
static pid_t main_pid;
static pthread_t main_tid;

const char usage[] = APP_NAME
    "[/debug] [/service] [/run] [/kill] [/install] [/remove] [/help]\n";

/*
 *
 * Main Windows entry point.
 *
 * We parse the command line and either calls the main App
 *   or starts up the service.
 */
int WINAPI WinMain(HINSTANCE Instance,
                   HINSTANCE /*PrevInstance*/,
                   PSTR CmdLine,
                   int /*show*/)
{
  char* cmdLine = CmdLine;
  char *wordPtr, *tempPtr;
  int i, quote;
  OSVERSIONINFO osversioninfo;
  osversioninfo.dwOSVersionInfoSize = sizeof(osversioninfo);


  /* Save the application instance and main thread id */
  appInstance = Instance;
  mainthreadId = GetCurrentThreadId();

  if (GetVersionEx(&osversioninfo)
      && osversioninfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    have_service_api = true;
  }

  main_pid = getpid();
  main_tid = pthread_self();

  INITCOMMONCONTROLSEX initCC
      = {sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES};

  InitCommonControlsEx(&initCC);

  /*
   * Funny things happen with the command line if the
   * execution comes from c:/Program Files/bareos/bareos.exe
   * We get a command line like: Files/bareos/bareos.exe" options
   * I.e. someone stops scanning command line on a space, not
   * realizing that the filename is quoted!!!!!!!!!!
   * So if first character is not a double quote and
   * the last character before first space is a double
   * quote, we throw away the junk.
   */

  wordPtr = cmdLine;
  while (*wordPtr && *wordPtr != ' ') wordPtr++;
  if (wordPtr > cmdLine) /* backup to char before space */
    wordPtr--;
  /* if first character is not a quote and last is, junk it */
  if (*cmdLine != '"' && *wordPtr == '"') { cmdLine = wordPtr + 1; }

  /*
   * Build Unix style argc *argv[] for the main "Unix" code
   *  stripping out any Windows options
   */

  /* Don't NULL command_args[0] !!! */
  for (i = 1; i < MAX_COMMAND_ARGS; i++) { command_args[i] = NULL; }

  char* pszArgs = strdup(cmdLine);
  wordPtr = pszArgs;
  quote = 0;
  while (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t')) wordPtr++;
  if (*wordPtr == '\"') {
    quote = 1;
    wordPtr++;
  } else if (*wordPtr == '/') {
    /* Skip Windows options */
    while (*wordPtr && (*wordPtr != ' ' && *wordPtr != '\t')) wordPtr++;
    while (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t')) wordPtr++;
  }
  if (*wordPtr) {
    while (*wordPtr && num_command_args < MAX_COMMAND_ARGS) {
      tempPtr = wordPtr;
      if (quote) {
        while (*tempPtr && *tempPtr != '\"') tempPtr++;
        quote = 0;
      } else {
        while (*tempPtr && *tempPtr != ' ') tempPtr++;
      }
      if (*tempPtr) *(tempPtr++) = '\0';
      command_args[num_command_args++] = wordPtr;
      wordPtr = tempPtr;
      while (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t')) wordPtr++;
      if (*wordPtr == '\"') {
        quote = 1;
        wordPtr++;
      }
    }
  }

  /*
   * Now process Windows command line options. Most of these options
   *  are single shot -- i.e. we accept one option, do something and
   *  Terminate.
   */
  for (i = 0; i < (int)strlen(cmdLine); i++) {
    char* p = &cmdLine[i];

    if (*p <= ' ') { continue; /* toss junk */ }

    if (*p != '/') { break; /* syntax error */ }

    /* Start as a service? */
    if (strncasecmp(p, "/service", 8) == 0) {
      return bareosServiceMain(); /* yes, run as a service */
    }

    /* Stop any running copy? */
    if (strncasecmp(p, "/kill", 5) == 0) { return stopRunningBareos(); }

    /* Run app as user program? */
    if (strncasecmp(p, "/run", 4) == 0) {
      return BareosAppMain(); /* yes, run as a user program */
    }

    /* Install Bareos in registry? */
    if (strncasecmp(p, "/install", 8) == 0) {
      return installService(p + 8); /* Pass command options */
    }

    /* Remove Bareos registry entry? */
    if (strncasecmp(p, "/remove", 7) == 0) { return removeService(); }

    /* Set debug mode? -- causes more dialogs to be displayed */
    if (strncasecmp(p, "/debug", 6) == 0) {
      opt_debug = true;
      i += 6; /* skip /debug */
      continue;
    }

    /* Display help? -- displays usage */
    if (strncasecmp(p, "/help", 5) == 0) {
      MessageBox(NULL, usage, APP_DESC, MB_OK | MB_ICONINFORMATION);
      return 0;
    }

    MessageBox(NULL, cmdLine, _("Bad Command Line Option"), MB_OK);

    /* Show the usage dialog */
    MessageBox(NULL, usage, APP_DESC, MB_OK | MB_ICONINFORMATION);

    return 1;
  }
  return BareosAppMain();
}

// Minimalist winproc when don't have tray monitor
LRESULT CALLBACK bacWinProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
  switch (iMsg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

/*
 * Called as a thread from BareosAppMain()
 * Here we handle the Windows messages
 */
void* Main_Msg_Loop([[maybe_unused]] LPVOID lpwThreadParam)
{
  MSG msg;

  pthread_detach(pthread_self());

  /*
   * Since we are the only thread with a message loop
   * mark ourselves as the service thread so that
   * we can receive all the window events.
   */
  service_thread_id = GetCurrentThreadId();

  /* Create a window to handle Windows messages */
  WNDCLASSEX baclass;

  baclass.cbSize = sizeof(baclass);
  baclass.style = 0;
  baclass.lpfnWndProc = bacWinProc;
  baclass.cbClsExtra = 0;
  baclass.cbWndExtra = 0;
  baclass.hInstance = appInstance;
  baclass.hIcon = NULL;
  baclass.hCursor = NULL;
  baclass.hbrBackground = NULL;
  baclass.lpszMenuName = NULL;
  baclass.lpszClassName = APP_NAME;
  baclass.hIconSm = NULL;

  RegisterClassEx(&baclass);

  if (CreateWindow(APP_NAME, APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                   CW_USEDEFAULT, 0, 0, NULL, NULL, appInstance, NULL)
      == NULL) {
    PostQuitMessage(0);
  }

  /* Now enter the Windows message handling loop until told to quit! */
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  /* If we get here, we are shutting down */

  if (have_service_api) {
    /* Mark that we're no longer running */
    service_thread_id = 0;
    /* Tell the service manager that we've stopped. */
    ReportStatus(SERVICE_STOPPED, service_error, 0);
  }
  /* Tell main "Unix" program to go away */
  TerminateApp(0);

  /* Should not get here */
  pthread_kill(main_tid, SIGTERM); /* ask main thread to Terminate */
  sleep(1);
  kill(main_pid, SIGTERM); /* kill main thread */
  _exit(0);
}


/*
 * This is the main routine for Bareos when running as an application,
 *  or after the service has started up.
 */
int BareosAppMain()
{
  pthread_t tid;
  DWORD dwCharsWritten;

  OSDependentInit();

  /* If no arguments were given then just run */
  if (p_AttachConsole == NULL || !p_AttachConsole(ATTACH_PARENT_PROCESS)) {
    if (opt_debug) { AllocConsole(); }
  }
  WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &dwCharsWritten,
               NULL);

  /* Startup networking */
  WSA_Init();

  /* Set this process to be the last application to be shut down. */
  if (p_SetProcessShutdownParameters) {
    p_SetProcessShutdownParameters(0x100, 0);
  }

  /* Create a thread to handle the Windows messages */
  pthread_create(&tid, NULL, Main_Msg_Loop, (void*)0);

  /* Call the Unix Bareos daemon */
  BareosMain(num_command_args, command_args);
  PostQuitMessage(0); /* Terminate our main message loop */

  WSACleanup();
  _exit(0);
}


void PauseMsg(const char* file, const char* func, int line, const char* msg)
{
  char buf[1000];
  if (msg) {
    Bsnprintf(buf, sizeof(buf), "%s:%s:%d %s", file, func, line, msg);
  } else {
    Bsnprintf(buf, sizeof(buf), "%s:%s:%d", file, func, line);
  }
  MessageBox(NULL, buf, "Pause", MB_OK);
}

typedef void(WINAPI* PGNSI)(LPSYSTEM_INFO);
typedef BOOL(WINAPI* PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);
