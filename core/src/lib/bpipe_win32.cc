/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
 * this code was moved from win32/compat/compat.cc. The original file included
 * the following copyright message:
 *
 * Copyright transferred from Raider Solutions, Inc to
 *   Kern Sibbald and John Walker by express permission.
 *
 * Author          : Christopher S. Hull
 * Created On      : Sat Jan 31 15:55:00 2004
 */
/**
 * @file
 * compatibility layer to make bareos-fd run natively under windows
 */
#include "lib/berrno.h"
#include "bpipe.h"

static const int debuglevel = 500;
#define MAX_PATHLENGTH 1024

BOOL CreateChildProcess(VOID);
static VOID ErrorExit(LPCSTR);

static void ErrorExit(LPCSTR lpszMessage) { Dmsg1(0, "%s", lpszMessage); }

/**
 * Extracts the executable or script name from the first string in cmdline.
 *
 * If the name contains blanks then it must be quoted with double quotes,
 * otherwise quotes are optional.  If the name contains blanks then it
 * will be converted to a short name.
 *
 * The optional quotes will be removed.  The result is copied to a malloc'ed
 * buffer and returned through the pexe argument.  The pargs parameter is set
 * to the address of the character in cmdline located after the name.
 *
 * The malloc'ed buffer returned in *pexe must be freed by the caller.
 */
static bool GetApplicationName(const char* cmdline,
                               char** pexe,
                               const char** pargs)
{
  const char* pExeStart = NULL; /* Start of executable name in cmdline */
  const char* pExeEnd = NULL; /* Character after executable name (separator) */

  const char* pBasename = NULL;  /* Character after last path separator */
  const char* pExtension = NULL; /* Period at start of extension */

  const char* current = cmdline;

  bool bQuoted = false;

  /* Skip initial whitespace */

  while (*current == ' ' || *current == '\t') { current++; }

  /* Calculate start of name and determine if quoted */

  if (*current == '"') {
    pExeStart = ++current;
    bQuoted = true;
  } else {
    pExeStart = current;
    bQuoted = false;
  }

  *pargs = NULL;
  *pexe = NULL;

  /* Scan command line looking for path separators (/ and \\) and the
   * terminator, either a quote or a blank.  The location of the
   * extension is also noted. */
  for (; *current != '\0'; current++) {
    if (*current == '.') {
      pExtension = current;
    } else if (IsPathSeparator(*current) && current[1] != '\0') {
      pBasename = &current[1];
      pExtension = NULL;
    }

    // Check for terminator, either quote or blank
    if (bQuoted) {
      if (*current != '"') { continue; }
    } else {
      if (*current != ' ') { continue; }
    }

    /* Hit terminator, remember end of name (address of terminator) and start of
     * arguments */
    pExeEnd = current;

    if (bQuoted && *current == '"') {
      *pargs = &current[1];
    } else {
      *pargs = current;
    }

    break;
  }

  if (pBasename == NULL) { pBasename = pExeStart; }

  if (pExeEnd == NULL) { pExeEnd = current; }

  if (*pargs == NULL) { *pargs = current; }

  bool bHasPathSeparators = pExeStart != pBasename;

  /* We have pointers to all the useful parts of the name
   * Default extensions in the order cmd.exe uses to search */
  static const char ExtensionList[][5] = {".com", ".exe", ".bat", ".cmd"};
  DWORD dwBasePathLength = pExeEnd - pExeStart;

  DWORD dwAltNameLength = 0;
  char* pPathname = (char*)alloca(MAX_PATHLENGTH + 1);
  char* pAltPathname = (char*)alloca(MAX_PATHLENGTH + 1);

  pPathname[MAX_PATHLENGTH] = '\0';
  pAltPathname[MAX_PATHLENGTH] = '\0';

  memcpy(pPathname, pExeStart, dwBasePathLength);
  pPathname[dwBasePathLength] = '\0';

  if (pExtension == NULL) {
    // Try appending extensions
    for (int index = 0;
         index < (int)(sizeof(ExtensionList) / sizeof(ExtensionList[0]));
         index++) {
      if (!bHasPathSeparators) {
        // There are no path separators, search in the standard locations
        dwAltNameLength = SearchPath(NULL, pPathname, ExtensionList[index],
                                     MAX_PATHLENGTH, pAltPathname, NULL);
        if (dwAltNameLength > 0 && dwAltNameLength <= MAX_PATHLENGTH) {
          memcpy(pPathname, pAltPathname, dwAltNameLength);
          pPathname[dwAltNameLength] = '\0';
          break;
        }
      } else {
        bstrncpy(&pPathname[dwBasePathLength], ExtensionList[index],
                 MAX_PATHLENGTH - dwBasePathLength);
        if (GetFileAttributes(pPathname) != INVALID_FILE_ATTRIBUTES) { break; }
        pPathname[dwBasePathLength] = '\0';
      }
    }
  } else if (!bHasPathSeparators) {
    // There are no path separators, search in the standard locations
    dwAltNameLength
        = SearchPath(NULL, pPathname, NULL, MAX_PATHLENGTH, pAltPathname, NULL);
    if (dwAltNameLength > 0 && dwAltNameLength < MAX_PATHLENGTH) {
      memcpy(pPathname, pAltPathname, dwAltNameLength);
      pPathname[dwAltNameLength] = '\0';
    }
  }

  if (strchr(pPathname, ' ') != NULL) {
    dwAltNameLength = GetShortPathName(pPathname, pAltPathname, MAX_PATHLENGTH);

    if (dwAltNameLength > 0 && dwAltNameLength <= MAX_PATHLENGTH) {
      *pexe = (char*)malloc(dwAltNameLength + 1);
      if (*pexe == NULL) { return false; }
      memcpy(*pexe, pAltPathname, dwAltNameLength + 1);
    }
  }

  if (*pexe == NULL) {
    DWORD dwPathnameLength = strlen(pPathname);
    *pexe = (char*)malloc(dwPathnameLength + 1);
    if (*pexe == NULL) { return false; }
    memcpy(*pexe, pPathname, dwPathnameLength + 1);
  }

  return true;
}

// Create the process with WCHAR API
static BOOL CreateChildProcessW(const char* comspec,
                                const char* cmdLine,
                                PROCESS_INFORMATION* hProcInfo,
                                HANDLE in,
                                HANDLE out,
                                HANDLE err)
{
  STARTUPINFOW siStartInfo;
  BOOL bFuncRetn = FALSE;
  POOLMEM *cmdLine_wchar, *comspec_wchar;

  // Setup members of the STARTUPINFO structure.
  ZeroMemory(&siStartInfo, sizeof(siStartInfo));
  siStartInfo.cb = sizeof(siStartInfo);

  // Setup new process to use supplied handles for stdin,stdout,stderr
  siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;

  siStartInfo.hStdInput = in;
  siStartInfo.hStdOutput = out;
  siStartInfo.hStdError = err;

  // Convert argument to WCHAR
  cmdLine_wchar = GetPoolMemory(PM_FNAME);
  comspec_wchar = GetPoolMemory(PM_FNAME);

  UTF8_2_wchar(cmdLine_wchar, cmdLine);
  UTF8_2_wchar(comspec_wchar, comspec);

  // Create the child process.
  Dmsg2(debuglevel, "Calling CreateProcess(%s, %s, ...)\n", comspec_wchar,
        cmdLine_wchar);

  // Try to execute program
  bFuncRetn = p_CreateProcessW((wchar_t*)comspec_wchar,
                               (wchar_t*)cmdLine_wchar, /* Command line */
                               NULL, /* Process security attributes */
                               NULL, /* Primary thread security attributes */
                               TRUE, /* Handles are inherited */
                               0,    /* Creation flags */
                               NULL, /* Use parent's environment */
                               NULL, /* Use parent's current directory */
                               &siStartInfo, /* STARTUPINFO pointer */
                               hProcInfo);   /* Receives PROCESS_INFORMATION */
  FreePoolMemory(cmdLine_wchar);
  FreePoolMemory(comspec_wchar);

  return bFuncRetn;
}


// Create the process with ANSI API
static BOOL CreateChildProcessA(const char* comspec,
                                char* cmdLine,
                                PROCESS_INFORMATION* hProcInfo,
                                HANDLE in,
                                HANDLE out,
                                HANDLE err)
{
  STARTUPINFOA siStartInfo;
  BOOL bFuncRetn = FALSE;

  // Set up members of the STARTUPINFO structure.
  ZeroMemory(&siStartInfo, sizeof(siStartInfo));
  siStartInfo.cb = sizeof(siStartInfo);

  // setup new process to use supplied handles for stdin,stdout,stderr
  siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;

  siStartInfo.hStdInput = in;
  siStartInfo.hStdOutput = out;
  siStartInfo.hStdError = err;

  // Create the child process.
  Dmsg2(debuglevel, "Calling CreateProcess(%s, %s, ...)\n", comspec, cmdLine);

  // Try to execute program
  bFuncRetn = p_CreateProcessA(comspec, cmdLine, /* Command line */
                               NULL, /* Process security attributes */
                               NULL, /* Primary thread security attributes */
                               TRUE, /* Handles are inherited */
                               0,    /* Creation flags */
                               NULL, /* Use parent's environment */
                               NULL, /* Use parent's current directory */
                               &siStartInfo, /* STARTUPINFO pointer */
                               hProcInfo);   /* Receives PROCESS_INFORMATION */
  return bFuncRetn;
}

/**
 * OK, so it would seem CreateProcess only handles true executables:
 * .com or .exe files.  So grab $COMSPEC value and pass command line to it.
 */
static HANDLE CreateChildProcess(const char* cmdline,
                                 HANDLE in,
                                 HANDLE out,
                                 HANDLE err)
{
  static const char* comspec = NULL;
  PROCESS_INFORMATION piProcInfo;
  BOOL bFuncRetn = FALSE;

  if (!p_CreateProcessA || !p_CreateProcessW) return INVALID_HANDLE_VALUE;

  if (comspec == NULL) comspec = getenv("COMSPEC");
  if (comspec == NULL)  // should never happen
    return INVALID_HANDLE_VALUE;

  // Set up members of the PROCESS_INFORMATION structure.
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

  /* if supplied handles are not used the send a copy of our STD_HANDLE as
   * appropriate. */
  if (in == INVALID_HANDLE_VALUE) in = GetStdHandle(STD_INPUT_HANDLE);

  if (out == INVALID_HANDLE_VALUE) out = GetStdHandle(STD_OUTPUT_HANDLE);

  if (err == INVALID_HANDLE_VALUE) err = GetStdHandle(STD_ERROR_HANDLE);

  char* exeFile;
  const char* argStart;

  if (!GetApplicationName(cmdline, &exeFile, &argStart)) {
    return INVALID_HANDLE_VALUE;
  }

  PoolMem cmdLine(PM_FNAME);
  Mmsg(cmdLine, "%s /c %s%s", comspec, exeFile, argStart);

  free(exeFile);

  // New function disabled
  if (p_CreateProcessW && p_MultiByteToWideChar) {
    bFuncRetn = CreateChildProcessW(comspec, cmdLine.c_str(), &piProcInfo, in,
                                    out, err);
  } else {
    bFuncRetn = CreateChildProcessA(comspec, cmdLine.c_str(), &piProcInfo, in,
                                    out, err);
  }

  if (bFuncRetn == 0) {
    ErrorExit("CreateProcess failed\n");
    const char* err_str = errorString();

    Dmsg3(debuglevel, "CreateProcess(%s, %s, ...)=%s\n", comspec,
          cmdLine.c_str(), err_str);
    LocalFree((void*)err_str);

    return INVALID_HANDLE_VALUE;
  }

  // we don't need a handle on the process primary thread so we close this now.
  CloseHandle(piProcInfo.hThread);

  return piProcInfo.hProcess;
}

static void CloseHandleIfValid(HANDLE handle)
{
  if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); }
}

Bpipe* OpenBpipe(const char* prog,
                 int wait,
                 const char* mode,
                 bool,
                 const std::unordered_map<std::string, std::string>& env_vars)
{
  // TODO: passing environment variables should be supported on Windows, too.
  //       for now, we will just crash intentionally.
  ASSERT(env_vars.empty());
  int mode_read, mode_write;
  SECURITY_ATTRIBUTES saAttr;
  BOOL fSuccess;
  Bpipe* bpipe;
  HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup, hChildStdoutRd,
      hChildStdoutWr, hChildStdoutRdDup;

  hChildStdinRd = INVALID_HANDLE_VALUE;
  hChildStdinWr = INVALID_HANDLE_VALUE;
  hChildStdinWrDup = INVALID_HANDLE_VALUE;
  hChildStdoutRd = INVALID_HANDLE_VALUE;
  hChildStdoutWr = INVALID_HANDLE_VALUE;
  hChildStdoutRdDup = INVALID_HANDLE_VALUE;

  bpipe = (Bpipe*)malloc(sizeof(Bpipe));
  memset((void*)bpipe, 0, sizeof(Bpipe));
  mode_read = (mode[0] == 'r');
  mode_write = (mode[0] == 'w' || mode[1] == 'w');

  // Set the bInheritHandle flag so pipe handles are inherited.
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (mode_read) {
    // Create a pipe for the child process's STDOUT.
    if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
      ErrorExit("Stdout pipe creation failed\n");
      goto cleanup;
    }

    // Create noninheritable read handle and close the inheritable read handle.
    fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRd,
                               GetCurrentProcess(), &hChildStdoutRdDup, 0,
                               FALSE, DUPLICATE_SAME_ACCESS);
    if (!fSuccess) {
      ErrorExit("DuplicateHandle failed");
      goto cleanup;
    }

    CloseHandle(hChildStdoutRd);
    hChildStdoutRd = INVALID_HANDLE_VALUE;
  }

  if (mode_write) {
    // Create a pipe for the child process's STDIN.
    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
      ErrorExit("Stdin pipe creation failed\n");
      goto cleanup;
    }

    // Duplicate the write handle to the pipe so it is not inherited.
    fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdinWr,
                               GetCurrentProcess(), &hChildStdinWrDup, 0,
                               FALSE,  // not inherited
                               DUPLICATE_SAME_ACCESS);
    if (!fSuccess) {
      ErrorExit("DuplicateHandle failed");
      goto cleanup;
    }

    CloseHandle(hChildStdinWr);
    hChildStdinWr = INVALID_HANDLE_VALUE;
  }

  // Spawn program with redirected handles as appropriate
  bpipe->worker_pid = CreateChildProcess(prog,            /* Commandline */
                                         hChildStdinRd,   /* stdin HANDLE */
                                         hChildStdoutWr,  /* stdout HANDLE */
                                         hChildStdoutWr); /* stderr HANDLE */

  if (bpipe->worker_pid == INVALID_HANDLE_VALUE) goto cleanup;

  bpipe->wait = wait;
  bpipe->worker_stime = time(NULL);

  if (mode_read) {
    // Close our write side so when process terminates we can detect eof.
    CloseHandle(hChildStdoutWr);
    hChildStdoutWr = INVALID_HANDLE_VALUE;

    int rfd = _open_osfhandle((intptr_t)hChildStdoutRdDup, O_RDONLY | O_BINARY);
    if (rfd >= 0) { bpipe->rfd = _fdopen(rfd, "rb"); }
  }

  if (mode_write) {
    // Close our read side so to not interfere with child's copy.
    CloseHandle(hChildStdinRd);
    hChildStdinRd = INVALID_HANDLE_VALUE;

    int wfd = _open_osfhandle((intptr_t)hChildStdinWrDup, O_WRONLY | O_BINARY);
    if (wfd >= 0) { bpipe->wfd = _fdopen(wfd, "wb"); }
  }

  if (wait > 0) {
    // the cast here is ok as the child timer only uses the pid for printing
    bpipe->timer_id = StartChildTimer(NULL, (pid_t)bpipe->worker_pid, wait);
  }

  return bpipe;

cleanup:

  CloseHandleIfValid(hChildStdoutRd);
  CloseHandleIfValid(hChildStdoutWr);
  CloseHandleIfValid(hChildStdoutRdDup);
  CloseHandleIfValid(hChildStdinRd);
  CloseHandleIfValid(hChildStdinWr);
  CloseHandleIfValid(hChildStdinWrDup);

  free((void*)bpipe);
  errno = b_errno_win32; /* Do GetLastError() for error code */
  return NULL;
}

int CloseBpipe(Bpipe* bpipe)
{
  int rval = 0;
  int32_t remaining_wait = bpipe->wait;

  // Close pipes
  if (bpipe->rfd) {
    fclose(bpipe->rfd);
    bpipe->rfd = NULL;
  }
  if (bpipe->wfd) {
    fclose(bpipe->wfd);
    bpipe->wfd = NULL;
  }

  if (remaining_wait == 0) { /* Wait indefinitely */
    remaining_wait = INT32_MAX;
  }
  for (;;) {
    DWORD exitCode;
    if (!GetExitCodeProcess((HANDLE)bpipe->worker_pid, &exitCode)) {
      const char* err = errorString();

      rval = b_errno_win32;
      Dmsg1(debuglevel, "GetExitCode error %s\n", err);
      LocalFree((void*)err);
      break;
    }
    if (exitCode == STILL_ACTIVE) {
      if (remaining_wait <= 0) {
        rval = ETIME; /* Timed out */
        break;
      }
      Bmicrosleep(1, 0); /* Wait one second */
      remaining_wait--;
    } else if (exitCode != 0) {
      // Truncate exit code as it doesn't seem to be correct
      rval = (exitCode & 0xFF) | b_errno_exit;
      break;
    } else {
      break; /* Shouldn't get here */
    }
  }

  if (bpipe->timer_id) { StopChildTimer(bpipe->timer_id); }

  if (bpipe->rfd) { fclose(bpipe->rfd); }

  if (bpipe->wfd) { fclose(bpipe->wfd); }

  free((void*)bpipe);
  return rval;
}


int CloseWpipe(Bpipe* bpipe)
{
  int result = 1;

  if (bpipe->wfd) {
    fflush(bpipe->wfd);
    if (fclose(bpipe->wfd) != 0) { result = 0; }
    bpipe->wfd = NULL;
  }
  return result;
}
