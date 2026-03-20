/*
 * Copyright (c) 2009-2017, Farooq Mela
 * Copyright (C) 2019-2025 Bareos GmbH & Co. KG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "include/bareos.h"
#include "backtrace.h"

#include <vector>

#if __has_include(<execinfo.h>) && defined HAVE_BACKTRACE \
    && defined HAVE_BACKTRACE_SYMBOLS

#  include <stdlib.h>
#  include <execinfo.h>  // for backtrace
#  include <dlfcn.h>     // for dladdr
#  include <cxxabi.h>    // for __cxa_demangle
#  include <string>

std::vector<BacktraceInfo> Backtrace(int skip, int amount)
{
  void* callstack[128];
  const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);

  std::vector<BacktraceInfo> trace_buf;

  int nFrames = backtrace(callstack, nMaxFrames);

  if (amount == 0) {
    return trace_buf;
  } else if (amount > 0) {
    // limit the number of frames
    nFrames = (skip + amount) < nFrames ? skip + amount : nFrames;
  }

  char** symbols = backtrace_symbols(callstack, nFrames);

  for (int i = skip; i < nFrames; i++) {
    Dl_info info;
    if (dladdr(callstack[i], &info)) {
      int status;
      char* demangled
          = abi::__cxa_demangle(info.dli_sname, nullptr, 0, &status);
      const char* name;
      if (status == 0) {
        name = demangled ? demangled : "(no demangeled name)";
      } else {
        name = info.dli_sname ? info.dli_sname : "(no dli_sname)";
      }
      trace_buf.emplace_back(i, name);
      if (demangled) { free(demangled); }
    } else {
      trace_buf.emplace_back(i, "unknown");
    }
  }
  if (symbols) { free(symbols); }
  if (nFrames == nMaxFrames) {
    trace_buf.emplace_back(nMaxFrames + 1, "[truncated]");
  }
  return trace_buf;
}
#else
std::vector<BacktraceInfo> Backtrace(int, int)
{
  return std::vector<BacktraceInfo>();
}
#endif /* __has_include(<execinfo.h>) && defined HAVE_BACKTRACE && \
   defined HAVE_BACKTRACE_SYMBOLS */
