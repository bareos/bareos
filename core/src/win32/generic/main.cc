/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
#include <psapi.h>
#include <dbghelp.h>
#include "fill_proc_address.h"

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

using namespace std;

// #pragma comment(lib,"Dbghelp.lib")

#include <sstream>

class DbgHelp {
 public:
  DbgHelp()
  {
    library_handle = LoadLibraryA("DBGHELP.DLL");
    if (library_handle) {
      bool ok = true;
#define SETUP_FUNCTION(Name)                                                  \
  do {                                                                        \
    if (!BareosFillProcAddress(Name##Ptr, library_handle, #Name)) ok = false; \
  } while (0)
      SETUP_FUNCTION(SymInitialize);
      SETUP_FUNCTION(SymGetModuleBase);
      SETUP_FUNCTION(SymFunctionTableAccess64);
      SETUP_FUNCTION(StackWalk64);
      SETUP_FUNCTION(SymFromAddr);
      SETUP_FUNCTION(SymGetLineFromAddr64);
      SETUP_FUNCTION(SymCleanup);
      SETUP_FUNCTION(SymSetOptions);
      SETUP_FUNCTION(ImageNtHeader);
      SETUP_FUNCTION(EnumerateLoadedModules64);
#undef SETUP_FUNCTION

      if (ok) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        initialised = SymInitialize(::GetCurrentProcess(), NULL, TRUE);
      }
    }
  }

  ~DbgHelp()
  {
    if (initialised) { SymCleanup(::GetCurrentProcess()); }
    if (library_handle) { FreeLibrary(library_handle); }
  }

  operator bool() const { return initialised; }

  BOOL StackWalk64(DWORD MachineType,
                   HANDLE hProcess,
                   HANDLE hThread,
                   LPSTACKFRAME64 StackFrame,
                   PVOID ContextRecord)
  {
    std::unique_lock l{mut};
    return StackWalk64Ptr(MachineType, hProcess, hThread, StackFrame,
                          ContextRecord, NULL, SymFunctionTableAccess64Ptr,
                          SymGetModuleBasePtr, NULL);
  }

  BOOL SymFromAddr(HANDLE hProcess,
                   DWORD64 Address,
                   PDWORD64 Displacement,
                   PSYMBOL_INFO Symbol)
  {
    std::unique_lock l{mut};
    return SymFromAddrPtr(hProcess, Address, Displacement, Symbol);
  }

  BOOL SymGetLineFromAddr64(HANDLE hProcess,
                            DWORD64 qwAddr,
                            PDWORD pdwDisplacement,
                            PIMAGEHLP_LINE64 Line64)
  {
    std::unique_lock l{mut};
    return SymGetLineFromAddr64Ptr(hProcess, qwAddr, pdwDisplacement, Line64);
  }

  BOOL EnumerateLoadedModules64(HANDLE hProcess,
                                PENUMLOADED_MODULES_CALLBACK64 cb,
                                PVOID UserContext)
  {
    // std::unique_lock l{mut};
    return EnumerateLoadedModules64Ptr(hProcess, cb, UserContext);
  }

  PIMAGE_NT_HEADERS ImageNtHeader(PVOID Base)
  {
    std::unique_lock l{mut};
    return ImageNtHeaderPtr(Base);
  }

 private:
  DWORD SymSetOptions(DWORD Options)
  {
    std::unique_lock l{mut};
    return SymSetOptionsPtr(Options);
  }

  BOOL SymInitialize(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess)
  {
    std::unique_lock l{mut};
    return SymInitializePtr(hProcess, UserSearchPath, fInvadeProcess);
  }

  BOOL SymCleanup(HANDLE hProcess)
  {
    std::unique_lock l{mut};
    return SymCleanupPtr(hProcess);
  }

  using t_SymInitialize = decltype(&::SymInitialize);
  using t_SymGetModuleBase = decltype(&::SymGetModuleBase);
  using t_SymFunctionTableAccess64 = decltype(&::SymFunctionTableAccess64);
  using t_StackWalk64 = decltype(&::StackWalk64);
  using t_SymFromAddr = decltype(&::SymFromAddr);
  using t_SymGetLineFromAddr64 = decltype(&::SymGetLineFromAddr64);
  using t_SymCleanup = decltype(&::SymCleanup);
  using t_SymSetOptions = decltype(&::SymSetOptions);
  using t_ImageNtHeader = decltype(&::ImageNtHeader);
  using t_EnumerateLoadedModules64 = decltype(&::EnumerateLoadedModules64);

  HMODULE library_handle{nullptr};
  t_SymInitialize SymInitializePtr{nullptr};
  t_SymGetModuleBase SymGetModuleBasePtr{nullptr};
  t_SymFunctionTableAccess64 SymFunctionTableAccess64Ptr{nullptr};
  t_StackWalk64 StackWalk64Ptr{nullptr};
  t_SymFromAddr SymFromAddrPtr{nullptr};
  t_SymGetLineFromAddr64 SymGetLineFromAddr64Ptr{nullptr};
  t_SymCleanup SymCleanupPtr{nullptr};
  t_SymSetOptions SymSetOptionsPtr{nullptr};
  t_ImageNtHeader ImageNtHeaderPtr{nullptr};
  t_EnumerateLoadedModules64 EnumerateLoadedModules64Ptr{nullptr};
  bool initialised{false};
  /* All DbgHelp functions are single threaded. Therefore, calls from more than
   * one thread to this function will likely result in unexpected behavior or
   * memory corruption. To avoid this, you must synchronize all concurrent
   * calls. */
  std::mutex mut{};
};

DbgHelp dbg;

void backtrace(std::stringstream& bt, CONTEXT* ctx)
{
  constexpr size_t max_function_name_length = 256;

  BOOL result;
  HANDLE process;
  HANDLE thread;
  HMODULE hModule;

  STACKFRAME64 stack;
  ULONG frame;
  DWORD64 displacement;

  DWORD disp;

  char buffer[offsetof(SYMBOL_INFO, Name[MAX_SYM_NAME])] alignas(SYMBOL_INFO);
  char module[max_function_name_length];
  PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

  // On x64, StackWalk64 modifies the context record, that could
  // cause crashes, so we create a copy to prevent it
  CONTEXT ctxCopy;
  memcpy(&ctxCopy, ctx, sizeof(CONTEXT));

  memset(&stack, 0, sizeof(STACKFRAME64));

  process = GetCurrentProcess();
  thread = GetCurrentThread();
  displacement = 0;
  DWORD image;
#if defined(_M_AMD64) || defined(_M_X64)
  image = IMAGE_FILE_MACHINE_AMD64;
  stack.AddrPC.Offset = ctxCopy.Rip;
  stack.AddrPC.Mode = AddrModeFlat;
  stack.AddrStack.Offset = ctxCopy.Rsp;
  stack.AddrStack.Mode = AddrModeFlat;
  stack.AddrFrame.Offset = ctxCopy.Rbp;
  stack.AddrFrame.Mode = AddrModeFlat;
#elif defined(_M_IX86)
  image = IMAGE_FILE_MACHINE_I386;
  frame.AddrPC.Offset = ctxCopy.Eip;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = ctxCopy.Ebp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Offset = ctxCopy.Esp;
  frame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_IA64)
  image = IMAGE_FILE_MACHINE_IA64;
  frame.AddrPC.Offset = ctxCopy.StIIP;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Offset = ctxCopy.IntSp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrBStore.Offset = ctxCopy.RsBSP;
  frame.AddrBStore.Mode = AddrModeFlat;
  frame.AddrStack.Offset = ctxCopy.IntSp;
  frame.AddrStack.Mode = AddrModeFlat;
#else
#  error "This platform is not supported."
#endif

  for (frame = 0;; frame++) {
    // get next call from stack
    result = dbg.StackWalk64(image, process, thread, &stack, &ctxCopy);

    if (!result) break;

    // get symbol name for address
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;


    // symbol->addr address of the symbol, displacement is the offset from
    // the PC to that address.
    // i.e. symbol->addr is the start of the function and
    // displacement is the offset of the pc into that function
    dbg.SymFromAddr(process, (ULONG64)stack.AddrPC.Offset, &displacement,
                    pSymbol);

    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);

    bt << "\tat " << pSymbol->Name;
    if (dbg.SymGetLineFromAddr64(process, stack.AddrPC.Offset, &disp, &line)) {
      bt << " in " << line.FileName << "@L" << std::dec << line.LineNumber;
    } else {
      // failed to get line
      hModule = NULL;
      lstrcpyA(module, "");
      GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        (LPCTSTR)(stack.AddrPC.Offset), &hModule);


      // at least print module name
      if (hModule != NULL) {
        GetModuleFileNameA(hModule, module, sizeof(module));
        bt << " in " << module;

        LPVOID text_section = nullptr;
        if (IMAGE_NT_HEADERS* nt = dbg.ImageNtHeader(hModule)) {
          IMAGE_SECTION_HEADER* current_section
              = reinterpret_cast<IMAGE_SECTION_HEADER*>(nt + 1);
          const char* base = reinterpret_cast<const char*>(hModule);

          for (int scn = 0; scn < nt->FileHeader.NumberOfSections; ++scn) {
            if (memcmp(".text", current_section->Name, sizeof(".text")) == 0) {
              text_section = (LPVOID)(base + current_section->VirtualAddress);
            }
            current_section++;
          }
        }

        bt << "@0x" << std::hex
           << ((long long int)pSymbol->Address - (long long int)text_section
               + (long long int)displacement);
      } else {
        bt << "at " << pSymbol->Name << " in <unknown>";
      }
    }
    bt << " (addr=0x" << std::hex << pSymbol->Address << ", disp=0x" << std::hex
       << displacement << ")\r\n";
  }
}

void GatherModuleInfo(std::stringstream& info)
{
  auto callback = +[](PCSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize,
                      PVOID UserContext) -> int {
    (void)ModuleSize;

    auto& info = *reinterpret_cast<std::stringstream*>(UserContext);

    HANDLE hModule = (HANDLE)ModuleBase;

    info << "\"" << ModuleName << "\" {\r\n";
    if (IMAGE_NT_HEADERS* nt = dbg.ImageNtHeader(hModule)) {
      IMAGE_SECTION_HEADER* current_section
          = reinterpret_cast<IMAGE_SECTION_HEADER*>(nt + 1);
      const char* base = reinterpret_cast<const char*>(hModule);

      for (int scn = 0; scn < nt->FileHeader.NumberOfSections; ++scn) {
        char name[sizeof(current_section->Name) + 1] = {};
        memcpy(name, current_section->Name, sizeof(current_section->Name));
        info << "\t{name=\"" << name << "\", start=0x"
             << (void*)(base + current_section->VirtualAddress) << ", size=0x"
             << std::hex << current_section->Misc.VirtualSize << "},\n";
        current_section++;
      }
    }
    info << "}\r\n";
    return TRUE;
  };

  dbg.EnumerateLoadedModules64(::GetCurrentProcess(), callback,
                               static_cast<PVOID>(&info));
}

extern const char* progname;

static BOOL WriteString(HANDLE file, std::string_view str)
{
  DWORD written = 0;
  while (written != str.size()) {
    DWORD written_now = 0;
    if (!WriteFile(file, str.data() + written, str.size() - written,
                   &written_now, NULL)) {
      return FALSE;
    }

    written += written_now;
  }
  return TRUE;
}

LONG WINAPI
BareosBacktracingExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
  auto header = (std::stringstream{}
                 << "Fatal: Unhandled exception 0x" << std::hex
                 << pExceptionInfo->ExceptionRecord->ExceptionCode << "\r\n")
                    .str();

  bool write_success = true;
  char tmpfile[MAX_PATH];
  if (dbg) {
    std::stringstream btss;
    btss << "Backtrace:\r\n\r\n";
    backtrace(btss, pExceptionInfo->ContextRecord);
    btss << "\r\n";

    std::stringstream moduleinfo;
    moduleinfo << "Loaded Modules:\r\n\r\n";
    GatherModuleInfo(moduleinfo);
    moduleinfo << "\r\n";

    char tmppath[MAX_PATH];
    HANDLE tmphandle = INVALID_HANDLE_VALUE;
    if (GetTempPathA(MAX_PATH, tmppath) > 0
        && GetTempFileNameA(tmppath, "bareos", 0, tmpfile) > 0
        && (tmphandle = CreateFileA(tmpfile, GENERIC_WRITE, 0, NULL,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
               != INVALID_HANDLE_VALUE) {
      if (write_success) { write_success = WriteString(tmphandle, header); }
      if (write_success) { write_success = WriteString(tmphandle, btss.str()); }
      if (write_success) {
        write_success = WriteString(tmphandle, moduleinfo.str());
      }

      CloseHandle(tmphandle);
    }
  } else {
    write_success = false;
  }

  HANDLE eventhandler = RegisterEventSourceA(NULL, progname ?: "Bareos");
  if (eventhandler) {
    std::string location{};
    if (!dbg) {
      location = "Could not load dbghelp.dll.";
    } else if (!write_success) {
      location = "Could not write backtrace.";
    } else {
      location = "Backtrace written to: ";
      location += tmpfile;
    }
    std::array strings = {
        header.c_str(),
        location.c_str(),
    };

    ReportEventA(eventhandler, EVENTLOG_ERROR_TYPE, 0, 0, NULL, strings.size(),
                 0, strings.data(), NULL);
    DeregisterEventSource(eventhandler);
  } else {
    exit(2);
  }
  exit(1);

  return EXCEPTION_CONTINUE_SEARCH;
}

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

  SetUnhandledExceptionFilter(BareosBacktracingExceptionHandler);

  if (GetVersionEx(&osversioninfo)
      && osversioninfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    have_service_api = true;
  }

  main_pid = getpid();
  main_tid = pthread_self();

  INITCOMMONCONTROLSEX initCC
      = {sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES};

  InitCommonControlsEx(&initCC);

  /* Funny things happen with the command line if the
   * execution comes from c:/Program Files/bareos/bareos.exe
   * We get a command line like: Files/bareos/bareos.exe" options
   * I.e. someone stops scanning command line on a space, not
   * realizing that the filename is quoted!!!!!!!!!!
   * So if first character is not a double quote and
   * the last character before first space is a double
   * quote, we throw away the junk. */

  wordPtr = cmdLine;
  while (*wordPtr && *wordPtr != ' ') wordPtr++;
  if (wordPtr > cmdLine) /* backup to char before space */
    wordPtr--;
  /* if first character is not a quote and last is, junk it */
  if (*cmdLine != '"' && *wordPtr == '"') { cmdLine = wordPtr + 1; }

  /* Build Unix style argc *argv[] for the main "Unix" code
   *  stripping out any Windows options */

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

  /* Now process Windows command line options. Most of these options
   *  are single shot -- i.e. we accept one option, do something and
   *  Terminate. */
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
void* Main_Msg_Loop(LPVOID)
{
  MSG msg;

  pthread_detach(pthread_self());

  /* Since we are the only thread with a message loop
   * mark ourselves as the service thread so that
   * we can receive all the window events. */
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
