/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
 * Copyright transferred from Raider Solutions, Inc to
 *   Kern Sibbald and John Walker by express permission.
 *
 * Author          : Christopher S. Hull
 * Created On      : Sat Jan 31 15:55:00 2004
 */
/**
 * @file
 * compatibilty layer to make bareos-fd run natively under windows
 */
#include "include/bareos.h"
#include "include/jcr.h"
#include "dlfcn.h"
#include "findlib/find.h"
#include "lib/btimers.h"
#include "lib/bsignal.h"
#include "lib/daemon.h"
#include "lib/berrno.h"
#include "lib/bpipe.h"
#include "vss.h"

#include <shared_mutex>

/**
 * Sanity check to make sure FILE_ATTRIBUTE_VALID_FLAGS is always smaller
 * than our define of FILE_ATTRIBUTE_VOLUME_MOUNT_POINT.
 */
#if FILE_ATTRIBUTE_VOLUME_MOUNT_POINT < FILE_ATTRIBUTE_VALID_FLAGS
#  error "FILE_ATTRIBUTE_VALID_FLAGS smaller than FILE_ATTRIBUTE_VALID_FLAGS"
#endif

/**
 * Note, if you want to see what Windows variables and structures
 * are defined, include/bareos.h includes <windows.h>, which is found in:
 *
 *   cross-tools/mingw32/mingw32/include
 * or
 *   cross-tools/mingw-w64/x86_64-pc-mingw32/include
 *
 * depending on whether we are building the 32 bit version or
 * the 64 bit version.
 */
static const int debuglevel = 500;

#define MAX_PATHLENGTH 1024

/**
 * The CoInitializeSecurity function initializes the security layer and sets the
 * specified values as the security default. If a process does not call
 * CoInitializeSecurity, COM calls it automatically the first time an interface
 * is marshaled or unmarshaled, registering the system default security. No
 * default security packages are registered until then.
 *
 * This function may be called exactly once per process.
 */
bool InitializeComSecurity()
{
  class ComSecurityInitializer {
   public:
    ComSecurityInitializer()
        : h{CoInitializeSecurity(
            NULL, /*  Allow *all* VSS writers to communicate back! */
            -1,   /*  Default COM authentication service */
            NULL, /*  Default COM authorization service */
            NULL, /*  reserved parameter */
            RPC_C_AUTHN_LEVEL_PKT_PRIVACY, /*  Strongest COM authentication
                                              level */
            RPC_C_IMP_LEVEL_IDENTIFY, /*  Minimal impersonation abilities */
            NULL,                     /*  Default COM authentication settings */
            EOAC_NONE,                /*  No special options */
            NULL) /* reserved */}
    {
      if (!InitSuccessFull()) {
        Dmsg1(0,
              "InitializeComSecurity: CoInitializeSecurity returned 0x%08X\n",
              h);
      }
    }

    bool InitSuccessFull() const { return !FAILED(h); }

   private:
    HRESULT h;
  };
  // CoInitializeSecurity can only fail if the system is running out of memory
  // in that case retrying it will probably not be a good idea anyways;
  // as such we only attempt to initialize once
  static const ComSecurityInitializer security{};

  if (!security.InitSuccessFull()) {
    errno = b_errno_win32;
    return false;
  }

  return true;
}

/**
 * UTF-8 to UCS2 path conversion is expensive,
 * so we cache the conversion. During backup the
 * conversion is called 3 times (lstat, attribs, open),
 * by using the cache this is reduced to 1 time
 */
struct thread_conversion_cache {
  std::string utf8{};
  std::wstring utf16{};
};


static class VssPathConverter {
 public:
  void SetConversions(t_pVSSPathConvert Convert, t_pVSSPathConvertW ConvertW)
  {
    std::unique_lock write_lock(rw_mut);  // unique write lock
    convert_fn = Convert;
    convert_w_fn = ConvertW;
  }

  struct free_deleter {
    void operator()(void* mem) { free(mem); }
  };

  template <typename T> using c_ptr = std::unique_ptr<T, free_deleter>;

  c_ptr<wchar_t> Convert(std::wstring_view str)
  {
    std::shared_lock read_lock(rw_mut);  // shared read lock
    if (convert_w_fn) {
      return c_ptr<wchar_t>(convert_w_fn(str.data()));
    } else {
      return nullptr;
    }
  }

  c_ptr<char> Convert(std::string_view str)
  {
    std::shared_lock read_lock(rw_mut);  // shared read lock
    if (convert_fn) {
      return c_ptr<char>(convert_fn(str.data()));
    } else {
      return nullptr;
    }
  }

 private:
  // used for a read-write-lock that protects writes to
  // convert_fn/convert_w_fn
  std::shared_mutex rw_mut{};

  t_pVSSPathConvert convert_fn{nullptr};
  t_pVSSPathConvertW convert_w_fn{nullptr};
} vss_path_converter;

bool SetVSSPathConvert(t_pVSSPathConvert Convert, t_pVSSPathConvertW ConvertW)
{
  vss_path_converter.SetConversions(Convert, ConvertW);
  return true;
}

// UTF-8 to UCS2 path conversion caching.
static void Win32ConvCleanupCache(void* arg)
{
  thread_conversion_cache* tcc = (thread_conversion_cache*)arg;

  Dmsg1(
      debuglevel,
      "Win32ConvCleanupCache: Cleanup of thread specific cache at address %p\n",
      tcc);

  delete tcc;
}

static class PathConversionCache {
 public:
  PathConversionCache()
  {
    status = pthread_key_create(&key, Win32ConvCleanupCache);
  }
  ~PathConversionCache()
  {
    if (status == 0) { pthread_key_delete(key); }
  }

  thread_conversion_cache* GetThreadLocal()
  {
    if (status != 0) return nullptr;  // could not init thread specific data
    auto tcc = static_cast<thread_conversion_cache*>(pthread_getspecific(key));
    if (!tcc) {
      return CreateThreadLocal();
    } else {
      return tcc;
    }
  }

  void ResetThreadLocal()
  {
    if (status != 0) return;  // could not init thread specific data
    auto tcc = static_cast<thread_conversion_cache*>(pthread_getspecific(key));
    if (tcc) {
      tcc->utf8.clear();
      tcc->utf16.clear();
    }
  }

 private:
  thread_conversion_cache* CreateThreadLocal()
  {
    ASSERT(status == 0);
    auto tcc = std::make_unique<thread_conversion_cache>();
    if (pthread_setspecific(key, tcc.get()) == 0) {
      Dmsg1(
          debuglevel,
          "Win32ConvInitCache: Setup of thread specific cache at address %p\n",
          tcc.get());
      return tcc.release();
    } else {
      return nullptr;
    }
  }
  pthread_key_t key;
  int status;
} path_conversion_cache;

void Win32ResetConversionCache() { path_conversion_cache.ResetThreadLocal(); }

// Forward referenced functions
const char* errorString(void);

// To allow the usage of the original version in this file here
#undef fputs

extern DWORD g_platform_id;
extern DWORD g_MinorVersion;

// From Microsoft SDK (KES) is the diff between Jan 1 1601 and Jan 1 1970
// see
// https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
#define WIN32_FILETIME_ADJUST 0x19DB1DED53E8000ULL

#define WIN32_FILETIME_SCALE 10000000  // 100ns/second

/**
 * Convert from UTF-8 to VSS Windows path/file
 * Used by compatibility layer for Unix system calls
 */
static inline void conv_unix_to_vss_win32_path(const char* name,
                                               char* win32_name,
                                               DWORD dwSize)
{
  const char* fname = name;
  char* tname = win32_name;

  int offset = 0;

  Dmsg0(debuglevel, "Enter convert_unix_to_win32_path\n");

  if (IsPathSeparator(name[0]) && IsPathSeparator(name[1]) && name[2] == '.'
      && IsPathSeparator(name[3])) {
    *win32_name++ = '\\';
    *win32_name++ = '\\';
    *win32_name++ = '.';
    *win32_name++ = '\\';

    name += 4;
  } else if (g_platform_id != VER_PLATFORM_WIN32_WINDOWS) {
    // Allow path to be 32767 bytes
    *win32_name++ = '\\';
    *win32_name++ = '\\';
    *win32_name++ = '?';
    *win32_name++ = '\\';

    offset = 4;  // skip this part during vss conversion
  }

  while (*name) {
    // Check for Unix separator and convert to Win32
    if (name[0] == '/' && name[1] == '/') { /* double slash? */
      name++;                               /* yes, skip first one */
    }
    if (*name == '/') {
      *win32_name++ = '\\'; /* convert char */
      // If Win32 separator that is "quoted", remove quote
    } else if (*name == '\\' && name[1] == '\\') {
      *win32_name++ = '\\';
      name++; /* skip first \ */
    } else {
      *win32_name++ = *name; /* copy character */
    }
    name++;
  }

  /* Strip any trailing slash, if we stored something
   * but leave "c:\" with backslash (root directory case) */
  if (*fname != 0 && win32_name[-1] == '\\' && strlen(fname) != 3) {
    win32_name[-1] = 0;
  } else {
    *win32_name = 0;
  }
  Dmsg1(debuglevel, "path = %s\n", tname);
  if (auto shadow_path = vss_path_converter.Convert(tname + offset);
      shadow_path) {
    bstrncpy(tname, shadow_path.get(), dwSize);
  }

  Dmsg1(debuglevel, "Leave cvt_u_to_win32_path path=%s\n", tname);
}

// Conversion of a Unix filename to a Win32 filename
void unix_name_to_win32(POOLMEM*& win32_name, const char* name)
{
  DWORD dwSize;

  /* One extra byte should suffice, but we double it
   * add MAX_PATH bytes for VSS shadow copy name. */
  dwSize = 2 * strlen(name) + MAX_PATH;
  win32_name = CheckPoolMemorySize(win32_name, dwSize);
  conv_unix_to_vss_win32_path(name, win32_name, dwSize);
}

std::wstring FromUtf8(std::string_view utf8)
{
  // MultiByteToWideChar does not handle empty strings
  if (utf8.size() == 0) { return {}; }
  // if the buffer is to small the function returns the number of characters
  // required
  DWORD required
      = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), utf8.size(), nullptr, 0);
  if (required == 0) {
    errno = b_errno_win32;
    BErrNo be;
    Dmsg2(300, "Can not convert %s to wide string: %s\n", utf8.data(),
          be.bstrerror());
    return {};
  }
  std::wstring utf16(required, '\0');

  // if the buffer is big enough the function returns the number of
  // characters written
  DWORD written = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), utf8.size(),
                                      utf16.data(), utf16.size());

  if (written != required) {
    errno = b_errno_win32;
    BErrNo be;
    Dmsg3(300,
          "Error during conversion! Expected %d chars but only got %d: %s\n",
          required, written, be.bstrerror());

    return {};
  }

  utf16.resize(written);
  return utf16;
}

std::string FromUtf16(std::wstring_view utf16)
{
  // WideCharToMultiByte does not handle empty strings
  if (utf16.size() == 0) { return {}; }

  // if the buffer is to small (or not supplied) the function returns
  // the number of bytes required
  DWORD required = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), utf16.size(),
                                       nullptr, 0, nullptr, nullptr);
  if (required == 0) {
    errno = b_errno_win32;
    BErrNo be;
    Dmsg0(300, "Encountered error in utf16 -> utf8 conversion: %s\n",
          be.bstrerror());
    return {};
  }
  std::string utf8(required, '\0');

  // if the buffer is big enough the function returns the number of
  // bytes written
  DWORD written
      = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), utf16.size(), utf8.data(),
                            utf8.size(), nullptr, nullptr);

  if (written != required) {
    errno = b_errno_win32;
    BErrNo be;
    Dmsg1(300,
          "Error during conversion! Expected %d chars but only got %d: %s\n",
          required, written, be.bstrerror());

    return {};
  }

  utf8.resize(written);
  return utf8;
}

static bool IsLiteralPath(std::wstring_view path)
{
  // check if the path starts with //?/
  return path.size() >= 4 && IsPathSeparator(path[0])
         && IsPathSeparator(path[1]) && IsPathSeparator(path[3])
         && path[2] == L'?';
}

static bool IsNormalizedPath(std::wstring_view path)
{
  // check if the path starts with //./
  return path.size() >= 4 && IsPathSeparator(path[0])
         && IsPathSeparator(path[1]) && IsPathSeparator(path[3])
         && path[2] == L'.';
}

/**
 * Removes all trailing slashes.  If the string only contains slashes
 * all but the first one are removed!
 * We also do not remove any slash if its preceded by a colon, i.e.
 * 'C:\' does not get changed.
 * If always is set to true it will even strip slashes that are preceded
 * by colons.
 */
static void RemoveTrailingSlashes(std::wstring& str, bool always = false)
{
  while (str.size() > 1 && IsPathSeparator(str.back())) {
    if (str[str.size() - 2] == L':' && !always) {
      break;
    } else {
      str.pop_back();
    }
  }
}

static std::wstring NormalizePath(std::wstring_view p)
{
  DWORD required = GetFullPathNameW(p.data(), 0, NULL, NULL);
  if (required == 0) {
    errno = b_errno_win32;
    BErrNo be;
    Dmsg0(300, "Could not get full path length of path %s: %s\n",
          FromUtf16(p).c_str(), be.bstrerror());
  }
  std::wstring literal(required, L'\0');
  DWORD written = GetFullPathNameW(p.data(), required, literal.data(), NULL);

  // required contains the terminating 0 but
  // written will not *if* the operation was successful.
  if (written != required - 1) {
    errno = b_errno_win32;
    BErrNo be;
    Dmsg3(300,
          "Error while getting full path of %s; allocated %d chars but needed "
          "%d: %s\n",
          FromUtf16(p).c_str(), required, written, be.bstrerror());
  }

  literal.resize(written);

  return literal;
}

struct Encoder {
  /* we use this construct because designated initializers are not
   * available in c++ */
  char table[256] = {};

  Encoder()
  {
    table['!'] = '0';
    table['<'] = '1';
    table['>'] = '2';
    table[':'] = '3';
    table['"'] = '4';
    table['|'] = '5';
    table['?'] = '6';
    table['*'] = '7';
    table[' '] = '8';
    table['.'] = '9';
  }
};

static Encoder enc;

static std::wstring Encode(std::wstring_view p)
{
  std::wstring str;

  /* if we want to handle the illegal characters \x01-\x1F, we can do so
   * in a similar way; just translate it like so:
   *    # <-> #00
   * \xXY <-> #XY
   * or even better:
   *    # <-> #0
   * \x0Y <-> #('A' + Y)
   * \x1Y <-> #('a' + Y)
   */

  bool inside_root = p.size() >= 2 && p[1] == ':';
  for (wchar_t c : p) {
    switch (c) {
      case '.':
        [[fallthrough]];
      case '!':
        [[fallthrough]];
      case '<':
        [[fallthrough]];
      case '>':
        [[fallthrough]];
      case ':':
        [[fallthrough]];
      case '"':
        [[fallthrough]];
      case '|':
        [[fallthrough]];
      case '?':
        [[fallthrough]];
      case '*':
        [[fallthrough]];
      case ' ': {
        // escape special characters, but not at the root of the path
        if (inside_root) {
          str += c;
        } else {
          str += '!';
          str += enc.table[c];
        }
      } break;
      case '/':
        [[fallthrough]];
      case '\\': {
        inside_root = false;
        str += c;
      } break;
      default: {
        str += c;
      }
    }
  }

  return str;
}

static std::wstring Decode(std::wstring_view p)
{
  static constexpr char translation_table[] = {
      '!', '<', '>', ':', '"', '|', '?', '*', ' ', '.',
  };

  std::wstring str;

  for (auto iter = p.begin(); iter != p.end(); ++iter) {
    switch (*iter) {
      case '!': {
        ++iter;
        if (iter == p.end()) {
          str += '!';
          continue;
        }

        char c = *iter;
        if ('0' <= c && c <= '9') {
          str += translation_table[c - '0'];
          continue;
        }

        str += '!';
        str += *iter;
      } break;

      default: {
        str += *iter;
      } break;
    }
  }
  return str;
}

static std::wstring AsFullPath(std::wstring_view p)
{
  /* this function basically does what GetFullPathName() would do
   * except that it also handles invalid paths.
   * The following characters are not allowed:
   * <, >, :, ", /, \\, |, ?, *, \0--\31, (' ' and . at end of dir)
   * We want to handle
   * <,>,:,",|,?,*, (<SPC> and . at end of dir)
   * Idea: Instead of normalizing p, we instead do
   * encode -> normalize -> decode
   * where encoding/decoding is replacing these nine chars with !<num>,
   * where 1 <= num <= 9.  Obviously we also need to "escape" ! -- in this case
   * with !0.  This gives us the following translation:
   * !     <-> !0
   * <     <-> !1
   * >     <-> !2
   * :     <-> !3
   * "     <-> !4
   * |     <-> !5
   * ?     <-> !6
   * *     <-> !7
   * <SPC> <-> !8
   * .     <-> !9 */

  std::wstring encoded = Encode(p);
  std::wstring normalized = NormalizePath(encoded);
  std::wstring decoded = Decode(normalized);

  return decoded;
}

/**
 * Replace all forward slashes by backwards slashes.
 * Also replaces all consecutive slashes by a single one unless they are
 * right at the start.
 */
std::wstring ReplaceSlashes(std::wstring_view path)
{
  using namespace std::literals;
  constexpr std::wstring_view path_separators = L"\\/"sv;

  std::size_t end = path.size();
  std::size_t head = std::min(path.find_first_not_of(path_separators), end);

  std::wstring result(head, L'\\');

  for (;;) {
    std::size_t copy_until
        = std::min(path.find_first_of(path_separators, head), end);
    result.append(std::begin(path) + head, std::begin(path) + copy_until);
    head = std::min(path.find_first_not_of(path_separators, copy_until), end);

    // if we encountered any slashes, replace them by a single backslash
    if (head != copy_until) {
      result.append(L"\\"sv);
    } else {
      ASSERT(head == end);
      break;
    }
  }

  return result;
}

/**
 * This function converts relative paths to absolute paths and prepends \\?\ if
 * needed.  It also replaces all '/' with '\' and removes duplicates of them.
 *
 * With this trick, it is possible to have 32K characters long paths.
 *
 * If availalbe it also converts the path to a vss path.
 *
 * Created 02/27/2006 Thorsten Engel
 * Updated 05/03/2023 Sebastian Sura
 */
static inline std::wstring make_wchar_win32_path(std::wstring_view path)
{
  // we want
  // '/' -> //?/C:
  // 'C:/' -> //?/C:/
  using namespace std::literals;
  Dmsg0(debuglevel, "Enter make_wchar_win32_path\n");

  // If it has already the desired form, exit without changes
  if (IsLiteralPath(path) || IsNormalizedPath(path)) {
    Dmsg0(debuglevel, "Leave make_wchar_win32_path no change \n");
    return ReplaceSlashes(path);
  }

  bool is_root = path.size() == 1 && IsPathSeparator(path[0]);

  std::wstring converted = AsFullPath(path);
  // for legacy reasons we do not want to have a trailing slash if
  // we were only given the "root" path, i.e. a path containing only a single
  // '/'
  RemoveTrailingSlashes(converted, is_root);

  if (auto shadow_path = vss_path_converter.Convert(converted); shadow_path) {
    // we sadly need to copy here
    converted.assign(shadow_path.get());
  } else {
    // add literal path prefix to allow 32k paths
    converted.insert(0, L"\\\\?\\"sv);
  }


  Dmsg1(debuglevel, "Leave make_wchar_win32_path=%s\n",
        FromUtf16(converted).c_str());
  return converted;
}

// Convert from WCHAR (UTF-16) to UTF-8
int wchar_2_UTF8(POOLMEM*& pszUTF, const WCHAR* pszUCS)
{
  /* The return value is the number of bytes written to the buffer.
   * The number includes the byte for the null terminator. */
  if (p_WideCharToMultiByte) {
    int nRet
        = p_WideCharToMultiByte(CP_UTF8, 0, pszUCS, -1, NULL, 0, NULL, NULL);
    pszUTF = CheckPoolMemorySize(pszUTF, nRet);
    return p_WideCharToMultiByte(CP_UTF8, 0, pszUCS, -1, pszUTF, nRet, NULL,
                                 NULL);
  } else {
    return 0;
  }
}

// Convert from WCHAR (UTF-16) to UTF-8
int wchar_2_UTF8(char* pszUTF, const WCHAR* pszUCS, int cchChar)
{
  /* The return value is the number of bytes written to the buffer.
   * The number includes the byte for the null terminator. */
  if (p_WideCharToMultiByte) {
    int nRet = p_WideCharToMultiByte(CP_UTF8, 0, pszUCS, -1, pszUTF, cchChar,
                                     NULL, NULL);
    ASSERT(nRet > 0);
    return nRet;
  } else {
    return 0;
  }
}

int UTF8_2_wchar(POOLMEM*& ppszUCS, const char* pszUTF)
{
  /* The return value is the number of wide characters written to the buffer.
   * convert null terminated string from utf-8 to ucs2, enlarge buffer if
   * necessary */
  if (p_MultiByteToWideChar) {
    // strlen of UTF8 +1 is enough
    DWORD cchSize = (strlen(pszUTF) + 1);
    ppszUCS = CheckPoolMemorySize(ppszUCS, cchSize * sizeof(wchar_t));

    int nRet = p_MultiByteToWideChar(CP_UTF8, 0, pszUTF, -1, (LPWSTR)ppszUCS,
                                     cchSize);
    ASSERT(nRet > 0);
    return nRet;
  } else {
    return 0;
  }
}

// Convert a C character string into an BSTR.
BSTR str_2_BSTR(const char* pSrc)
{
  DWORD cwch;
  BSTR wsOut(NULL);

  if (!pSrc) { return NULL; }

  if (p_MultiByteToWideChar) {
    if ((cwch = p_MultiByteToWideChar(CP_ACP, 0, pSrc, -1, NULL, 0))) {
      cwch--;
      wsOut = SysAllocStringLen(NULL, cwch);
      if (wsOut) {
        if (!p_MultiByteToWideChar(CP_ACP, 0, pSrc, -1, wsOut, cwch)) {
          if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) { return wsOut; }
          SysFreeString(wsOut);
          wsOut = NULL;
        }
      }
    }
  }

  return wsOut;
}

// Convert a BSTR into an C character string.
char* BSTR_2_str(const BSTR pSrc)
{
  DWORD cb, cwch;
  char* szOut = NULL;

  if (!pSrc) { return NULL; }

  if (p_WideCharToMultiByte) {
    cwch = SysStringLen(pSrc);
    if ((cb
         = p_WideCharToMultiByte(CP_ACP, 0, pSrc, cwch + 1, NULL, 0, 0, 0))) {
      szOut = (char*)malloc(cb);
      szOut[cb - 1] = '\0';
      if (!p_WideCharToMultiByte(CP_ACP, 0, pSrc, cwch + 1, szOut, cb, 0, 0)) {
        free(szOut);
        szOut = NULL;
      }
    }
  }

  return szOut;
}

std::wstring make_win32_path_UTF8_2_wchar(std::string_view utf8)
{
  thread_conversion_cache* tcc = nullptr;

  /* If we find the utf8 string in cache, we use the cached ucs2 version.
   * we compare the stringlength first (quick check) and then compare the
   * content. */
  tcc = path_conversion_cache.GetThreadLocal();
  if (tcc && tcc->utf8 == utf8) {
    // this creates a copy!
    return tcc->utf16;
  }

  /* Helper to convert from utf-8 to utf-16 and to complete a path for 32K path
   * syntax */
  std::wstring utf16 = FromUtf8(utf8);

  {
    // Add \\?\ to support 32K long filepaths
    std::wstring converted = make_wchar_win32_path(utf16);
    // this is done for debugability;
    // the compiler should optimize it as if it were
    // utf16 = make_wchar_[...]
    std::swap(converted, utf16);
  }

  // Populate cache
  if (tcc) {
    tcc->utf8.assign(utf8);
    tcc->utf16.assign(utf16);
  }

  return utf16;
}

#if !defined(_MSC_VER) || (_MSC_VER < 1400)  // VC8+
int umask(int) { return 0; }
#endif

#ifndef LOAD_WITH_ALTERED_SEARCH_PATH
#  define LOAD_WITH_ALTERED_SEARCH_PATH 0x00000008
#endif

void* dlopen(const char* filename, int mode)
{
  void* handle;
  DWORD dwFlags = 0;

  dwFlags |= LOAD_WITH_ALTERED_SEARCH_PATH;
  if (mode & RTLD_NOLOAD) { dwFlags |= LOAD_LIBRARY_AS_DATAFILE; }

  handle = LoadLibraryEx(filename, NULL, dwFlags);

  return handle;
}

void* dlsym(void* handle, const char* name)
{
  void* symaddr;

  symaddr = (void*)GetProcAddress((HMODULE)handle, name);

  return symaddr;
}

int dlclose(void* handle)
{
  if (handle && !FreeLibrary((HMODULE)handle)) {
    errno = b_errno_win32;
    return 1; /* failed */
  }
  return 0; /* OK */
}

char* dlerror(void)
{
  static char buf[200];
  const char* err = errorString();

  bstrncpy(buf, (char*)err, sizeof(buf));
  LocalFree((void*)err);

  return buf;
}

int fcntl(int, int) { return 0; }

int chown(const char*, uid_t, gid_t) { return 0; }

int lchown(const char*, uid_t, gid_t) { return 0; }

long int random(void) { return rand(); }

void srandom(unsigned int seed) { srand(seed); }

static time_t CvtFtimeToUtime(const FILETIME& time)
{
  uint64_t mstime;

  mstime = time.dwHighDateTime;
  mstime <<= 32;
  mstime |= time.dwLowDateTime;
  mstime -= WIN32_FILETIME_ADJUST;
  mstime /= WIN32_FILETIME_SCALE; /* convert to seconds. */

  if constexpr (sizeof(time_t) < sizeof(mstime)) {
    // take care of 32bit time_ts (i.e. on 32bit windows)
    return static_cast<time_t>(mstime & 0xffff'ffff);
  } else {
    return static_cast<time_t>(mstime);
  }
}

static time_t CvtFtimeToUtime(const LARGE_INTEGER& time)
{
  uint64_t mstime;

  mstime = time.HighPart;
  mstime <<= 32;
  mstime |= time.LowPart;
  mstime -= WIN32_FILETIME_ADJUST;
  mstime /= WIN32_FILETIME_SCALE; /* convert to seconds. */

  if constexpr (sizeof(time_t) < sizeof(mstime)) {
    // take care of 32bit time_ts (i.e. on 32bit windows)
    return static_cast<time_t>(mstime & 0xffff'ffff);
  } else {
    return static_cast<time_t>(mstime);
  }
}

bool CreateJunction(const char* szJunction, const char* szPath)
{
  DWORD dwRet;
  HANDLE hDir;
  bool retval = false;
  int buf_length, data_length;
  int SubstituteNameLength, PrintNameLength; /* length in bytes */
  REPARSE_DATA_BUFFER* rdb;
  PoolMem szDestDir(PM_FNAME);
  POOLMEM *buf, *szJunctionW, *szPrintName, *szSubstituteName;

  /* We only implement a Wide string version of CreateJunction.
   * So if p_CreateDirectoryW is not available we refuse to do anything. */
  if (!p_CreateDirectoryW) {
    Dmsg0(debuglevel,
          "CreateJunction: CreateDirectoryW not found, doing nothing\n");
    return false;
  }

  // Create three buffers to hold the wide char strings.
  szJunctionW = GetPoolMemory(PM_FNAME);

  /* With \\?\\ */
  szSubstituteName = GetPoolMemory(PM_FNAME);

  /* Simple path */
  szPrintName = GetPoolMemory(PM_FNAME);

  // Create a buffer big enough to hold all data.
  buf_length = sizeof(REPARSE_DATA_BUFFER) + 2 * MAX_PATH * sizeof(WCHAR);
  buf = GetPoolMemory(PM_NAME);
  buf = CheckPoolMemorySize(buf, buf_length);
  rdb = (REPARSE_DATA_BUFFER*)buf;

  // Create directory
  if (!UTF8_2_wchar(szJunctionW, szJunction)) { goto bail_out; }

  if (!p_CreateDirectoryW((LPCWSTR)szJunctionW, NULL)) {
    Dmsg1(debuglevel, "CreateDirectory Failed:%s\n", errorString());
    goto bail_out;
  }

  // Create file/open reparse point
  hDir = p_CreateFileW(
      (LPCWSTR)szJunctionW, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (hDir == INVALID_HANDLE_VALUE) {
    Dmsg0(debuglevel, "INVALID_FILE_HANDLE");
    RemoveDirectoryW((LPCWSTR)szJunctionW);
    goto bail_out;
  }

  if (!UTF8_2_wchar(szPrintName, szPath)) { goto bail_out; }

  // Add  \??\ prefix.
  Mmsg(szDestDir, "\\??\\%s", szPath);
  if (!UTF8_2_wchar(szSubstituteName, szDestDir.c_str())) { goto bail_out; }

  // Put data junction target into reparse buffer
  memset(buf, 0, buf_length);

#define MOUNTPOINTREPARSEBUFFER_FIXED_HEADER_SIZE 8

  SubstituteNameLength = wcslen((LPWSTR)szSubstituteName) * sizeof(WCHAR);
  PrintNameLength = wcslen((LPWSTR)szPrintName) * sizeof(WCHAR);

  wcsncpy((LPWSTR)rdb->MountPointReparseBuffer.PathBuffer,
          (LPWSTR)szSubstituteName, SubstituteNameLength);

  wcsncpy((LPWSTR)rdb->MountPointReparseBuffer.PathBuffer
              + wcslen((LPWSTR)szSubstituteName) + 1,
          (LPWSTR)szPrintName, PrintNameLength);

  rdb->MountPointReparseBuffer.SubstituteNameLength = SubstituteNameLength;
  rdb->MountPointReparseBuffer.PrintNameOffset = SubstituteNameLength + 2;
  rdb->MountPointReparseBuffer.PrintNameLength = PrintNameLength;
  rdb->ReparseDataLength = SubstituteNameLength + PrintNameLength + 12;
  data_length
      = rdb->ReparseDataLength + MOUNTPOINTREPARSEBUFFER_FIXED_HEADER_SIZE;
  rdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;

  /* Write reparse point
   * For debugging use "fsutil reparsepoint query" */
  if (!DeviceIoControl(hDir, FSCTL_SET_REPARSE_POINT, (LPVOID)buf, data_length,
                       NULL, 0, &dwRet, 0)) {
    Dmsg1(debuglevel, "DeviceIoControl Failed:%s\n", errorString());
    CloseHandle(hDir);
    RemoveDirectoryW((LPCWSTR)szJunctionW);
    goto bail_out;
  }

  CloseHandle(hDir);

  retval = true;

bail_out:
  FreePoolMemory(buf);
  FreePoolMemory(szJunctionW);
  FreePoolMemory(szPrintName);
  FreePoolMemory(szSubstituteName);

  return retval;
}

const char* errorString(void)
{
  LPVOID lpMsgBuf;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default lang */
                (LPTSTR)&lpMsgBuf, 0, NULL);

  // Strip any \r or \n
  char* rval = (char*)lpMsgBuf;
  char* cp = strchr(rval, '\r');
  if (cp != NULL) {
    *cp = 0;
  } else {
    cp = strchr(rval, '\n');
    if (cp != NULL) *cp = 0;
  }
  return rval;
}

// Retrieve the unique devicename of a Volume MountPoint.
static inline bool GetVolumeMountPointData(const char* filename,
                                           POOLMEM*& devicename)
{
  HANDLE h = INVALID_HANDLE_VALUE;

  /* Now to find out if the directory is a mount point or a reparse point.
   * Explicitly open the file to read the reparse point, then call
   * DeviceIoControl to find out if it points to a Volume or to a directory. */
  if (p_GetFileAttributesW) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(filename);

    if (p_CreateFileW) {
      h = CreateFileW(
          utf16.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    }

    if (h == INVALID_HANDLE_VALUE) {
      Dmsg1(debuglevel, "Invalid handle from CreateFileW(%s)\n", utf16.c_str());
      return false;
    }

  } else if (p_GetFileAttributesA) {
    PoolMem win32_fname(PM_FNAME);
    unix_name_to_win32(win32_fname.addr(), filename);

    h = CreateFileA(
        win32_fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

    if (h == INVALID_HANDLE_VALUE) {
      Dmsg1(debuglevel, "Invalid handle from CreateFile(%s)\n",
            win32_fname.c_str());
      return false;
    }
  }

  if (h != INVALID_HANDLE_VALUE) {
    bool ok;
    POOLMEM* buf;
    int buf_length;
    REPARSE_DATA_BUFFER* rdb;
    DWORD bytes;

    // Create a buffer big enough to hold all data.
    buf_length = sizeof(REPARSE_DATA_BUFFER) + MAX_PATH * sizeof(wchar_t);
    buf = GetPoolMemory(PM_NAME);
    buf = CheckPoolMemorySize(buf, buf_length);
    rdb = (REPARSE_DATA_BUFFER*)buf;

    memset(buf, 0, buf_length);
    rdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL,
                         0,                              /* in buffer, bytes */
                         (LPVOID)buf, (DWORD)buf_length, /* out buffer, btyes */
                         (LPDWORD)&bytes, (LPOVERLAPPED)0);
    if (ok) {
      wchar_2_UTF8(devicename,
                   (wchar_t*)rdb->MountPointReparseBuffer.PathBuffer);
    }

    CloseHandle(h);
    FreePoolMemory(buf);
  } else {
    return false;
  }

  return true;
}

// Retrieve the symlink target
static inline ssize_t GetSymlinkData(const char* filename,
                                     POOLMEM*& symlinktarget)
{
  ssize_t nrconverted = -1;
  HANDLE h = INVALID_HANDLE_VALUE;

  if (p_GetFileAttributesW) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(filename);

    if (p_CreateFileW) {
      h = CreateFileW(
          utf16.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    }

    if (h == INVALID_HANDLE_VALUE) {
      Dmsg1(debuglevel, "Invalid handle from CreateFileW(%s)\n", utf16.c_str());
      return -1;
    }
  } else if (p_GetFileAttributesA) {
    PoolMem win32_fname(PM_FNAME);
    unix_name_to_win32(win32_fname.addr(), filename);
    h = CreateFileA(
        win32_fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

    if (h == INVALID_HANDLE_VALUE) {
      Dmsg1(debuglevel, "Invalid handle from CreateFile(%s)\n",
            win32_fname.c_str());
      return -1;
    }
  }
  if (h != INVALID_HANDLE_VALUE) {
    bool ok;
    POOLMEM* buf;
    int buf_length;
    REPARSE_DATA_BUFFER* rdb;
    DWORD bytes;

    // Create a buffer big enough to hold all data.
    buf_length = sizeof(REPARSE_DATA_BUFFER) + MAX_PATH * sizeof(wchar_t);
    buf = GetPoolMemory(PM_NAME);
    buf = CheckPoolMemorySize(buf, buf_length);
    rdb = (REPARSE_DATA_BUFFER*)buf;

    memset(buf, 0, buf_length);
    rdb->ReparseTag = IO_REPARSE_TAG_SYMLINK;
    ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL,
                         0,                              /* in buffer, bytes */
                         (LPVOID)buf, (DWORD)buf_length, /* out buffer, btyes */
                         (LPDWORD)&bytes, (LPOVERLAPPED)0);
    if (ok) {
      POOLMEM* path;
      int len, offset, ofs;

      len = rdb->SymbolicLinkReparseBuffer.SubstituteNameLength
            / sizeof(wchar_t);
      offset = rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset
               / sizeof(wchar_t);

      // null-Terminate pathbuffer
      *(wchar_t*)(rdb->SymbolicLinkReparseBuffer.PathBuffer + offset + len)
          = L'\0';

      // convert to UTF-8
      path = GetPoolMemory(PM_FNAME);
      nrconverted = wchar_2_UTF8(
          path, (wchar_t*)(rdb->SymbolicLinkReparseBuffer.PathBuffer + offset));

      ofs = 0;
      if (bstrncasecmp(path, "\\??\\", 4)) { /* skip \\??\\ if exists */
        ofs = 4;
        Dmsg0(debuglevel, "\\??\\ was in filename, skipping\n");
      }

      if (bstrncasecmp(path, "?\\",
                       2)) { /* skip ?\\ if exists, this is a MountPoint that we
                                opened as Symlink */
        ofs = 2;
        Dmsg0(debuglevel,
              "?\\ was in filename, skipping, use of "
              "MountPointReparseDataBuffer is needed\n");
      }

      PmStrcpy(symlinktarget, path + ofs);
      FreePoolMemory(path);
    } else {
      Dmsg1(debuglevel, "DeviceIoControl failed:%s\n", errorString());
    }

    CloseHandle(h);
    FreePoolMemory(buf);
  } else {
    return -1;
  }

  return nrconverted;
}

/**
 * This is called for directories, and reparse points and is used to
 * get some special attributes like the type of reparse point etc..
 */
static int GetWindowsFileInfo(const char* filename,
                              struct stat* sb,
                              bool is_directory)
{
  bool use_fallback_data = true;
  WIN32_FIND_DATAW info_w;  // window's file info
  WIN32_FIND_DATAA info_a;  // window's file info
#if (_WIN32_WINNT >= 0x0600)
  FILE_BASIC_INFO basic_info;  // window's basic file info
  HANDLE h = INVALID_HANDLE_VALUE;
#endif
  HANDLE fh = INVALID_HANDLE_VALUE;

  // Cache some common vars to make code more transparent.
  DWORD* pdwFileAttributes;
  DWORD* pnFileSizeHigh;
  DWORD* pnFileSizeLow;
  DWORD* pdwReserved0;
  FILETIME* pftLastAccessTime;
  FILETIME* pftLastWriteTime;
  FILETIME* pftChangeTime;


  // First get a findhandle and a file handle to the file.
  if (p_FindFirstFileW) { /* use unicode */
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(filename);

    Dmsg1(debuglevel, "FindFirstFileW=%s\n", utf16.c_str());
    fh = p_FindFirstFileW(utf16.c_str(), &info_w);
#if (_WIN32_WINNT >= 0x0600)
    if (fh != INVALID_HANDLE_VALUE) {
      h = p_CreateFileW(
          utf16.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS, /* Required for directories */
          NULL);
    }
#endif

  } else if (p_FindFirstFileA) {  // use ASCII
    PoolMem win32_fname(PM_FNAME);
    unix_name_to_win32(win32_fname.addr(), filename);
    Dmsg1(debuglevel, "FindFirstFileA=%s\n", win32_fname.c_str());
    fh = p_FindFirstFileA(win32_fname.c_str(), &info_a);
#if (_WIN32_WINNT >= 0x0600)
    if (h != INVALID_HANDLE_VALUE) {
      h = CreateFileA(win32_fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS, /* Required for directories */
                      NULL);
    }
#endif
  } else {
    Dmsg0(debuglevel, "No findFirstFile A or W found\n");
  }

  // If we got a valid handle start processing the info.
  if (fh != INVALID_HANDLE_VALUE) {
    if (p_FindFirstFileW) { /* use unicode */
      pdwFileAttributes = &info_w.dwFileAttributes;
      pdwReserved0 = &info_w.dwReserved0;
      pnFileSizeHigh = &info_w.nFileSizeHigh;
      pnFileSizeLow = &info_w.nFileSizeLow;
    } else {
      pdwFileAttributes = &info_a.dwFileAttributes;
      pdwReserved0 = &info_a.dwReserved0;
      pnFileSizeHigh = &info_a.nFileSizeHigh;
      pnFileSizeLow = &info_a.nFileSizeLow;
    }

#if (_WIN32_WINNT >= 0x0600)
    // As this is retrieved by handle it has no specific A or W call.
    if (h != INVALID_HANDLE_VALUE) {
      if (p_GetFileInformationByHandleEx) {
        if (p_GetFileInformationByHandleEx(h, FileBasicInfo, &basic_info,
                                           sizeof(basic_info))) {
          pftLastAccessTime = (FILETIME*)&basic_info.LastAccessTime;
          pftLastWriteTime = (FILETIME*)&basic_info.LastWriteTime;
          // changetime is not updated when a file is copied to a new location
          // only creation time is updated (dont ask me how you update
          // the creation time without updating the meta data)
          // as such we need to take the maximum here!
          if (CompareFileTime((FILETIME*)&basic_info.ChangeTime,
                              (FILETIME*)&basic_info.CreationTime)
              < 0) {
            pftChangeTime = (FILETIME*)&basic_info.CreationTime;
          } else {
            pftChangeTime = (FILETIME*)&basic_info.ChangeTime;
          }
          use_fallback_data = false;
        }
      }
      CloseHandle(h);
    }
#endif

    /* See if we got anything from the GetFileInformationByHandleEx() call if
     * not fallback to the normal info data returned by FindFirstFileW() or
     * FindFirstFileA() */
    if (use_fallback_data) {
      if (p_FindFirstFileW) { /* use unicode */
        pftLastAccessTime = &info_w.ftLastAccessTime;
        pftLastWriteTime = &info_w.ftLastWriteTime;
        // if change time is not available we fallback to
        // using lastwritetime instead
        if (CompareFileTime(&info_w.ftLastWriteTime, &info_w.ftCreationTime)
            < 0) {
          pftChangeTime = &info_w.ftCreationTime;
        } else {
          pftChangeTime = &info_w.ftLastWriteTime;
        }
      } else {
        pftLastAccessTime = &info_a.ftLastAccessTime;
        pftLastWriteTime = &info_a.ftLastWriteTime;
        if (CompareFileTime(&info_a.ftLastWriteTime, &info_a.ftCreationTime)
            < 0) {
          pftChangeTime = &info_a.ftCreationTime;
        } else {
          pftChangeTime = &info_a.ftLastWriteTime;
        }
      }
    }

    FindClose(fh);
  } else {
    const char* err = errorString();

    /* Note, in creating leading paths, it is normal that the file does not
     * exist. */
    Dmsg2(2099, "FindFirstFile(%s):%s\n", filename, err);
    LocalFree((void*)err);
    errno = b_errno_win32;

    return -1;
  }

  sb->st_mode = 0777;

  // We store the full windows file attributes into st_rdev.
  sb->st_rdev = *pdwFileAttributes;

  if (is_directory) {
    // Directory
    sb->st_mode |= S_IFDIR;

    /* Store reparse/mount point info in st_rdev.  Note a Win32 reparse point
     * (junction point) is like a link though it can have many properties
     * (directory link, soft link, hard link, HSM, ...
     * The IO_REPARSE_TAG_* tag values describe what type of reparse point
     * it actually is.
     *
     * A mount point is a reparse point where another volume is mounted,
     * so it is like a Unix mount point (change of filesystem). */
    if ((*pdwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      switch (*pdwReserved0) {
        case IO_REPARSE_TAG_MOUNT_POINT: {
          /* A mount point can be:
           * Volume Mount Point "\\??\\volume{..."
           * Junction Point     "\\??\\..." */
          POOLMEM* vmp = GetPoolMemory(PM_NAME);
          if (GetVolumeMountPointData(filename, vmp)) {
            if (bstrncasecmp(vmp, "\\??\\volume{", 11)) {
              Dmsg2(debuglevel, "Volume Mount Point %s points to: %s\n",
                    filename, vmp);
              sb->st_rdev |= FILE_ATTRIBUTE_VOLUME_MOUNT_POINT;
            } else {
              Dmsg2(debuglevel, "Junction Point %s points to: %s\n", filename,
                    vmp);
              sb->st_rdev |= FILE_ATTRIBUTES_JUNCTION_POINT;
              sb->st_mode |= S_IFLNK;
              sb->st_mode &= ~S_IFDIR;
            }
          }
          FreePoolMemory(vmp);
          break;
        }
        case IO_REPARSE_TAG_SYMLINK: {
          POOLMEM* slt;

          Dmsg0(debuglevel, "We have a symlinked directory!\n");
          sb->st_rdev |= FILE_ATTRIBUTES_SYMBOLIC_LINK;
          sb->st_mode |= S_IFLNK;
          sb->st_mode &= ~S_IFDIR;

          slt = GetPoolMemory(PM_NAME);
          slt = CheckPoolMemorySize(slt, MAX_PATH * sizeof(wchar_t));

          if (GetSymlinkData(filename, slt)) {
            Dmsg2(debuglevel, "Symlinked Directory %s points to: %s\n",
                  filename, slt);
          }
          FreePoolMemory(slt);
          break;
        }
        default:
          Dmsg1(debuglevel,
                "IO_REPARSE_TAG_MOUNT_POINT with unhandled IO_REPARSE_TAG %d\n",
                *pdwReserved0);
          break;
      }
    }
  } else {
    // No directory
    if ((*pdwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      switch (*pdwReserved0) {
        case IO_REPARSE_TAG_SYMLINK: {
          POOLMEM* slt = GetPoolMemory(PM_NAME);

          Dmsg0(debuglevel, "We have a symlinked file!\n");
          sb->st_mode |= S_IFLNK;

          if (GetSymlinkData(filename, slt)) {
            Dmsg2(debuglevel, "Symlinked File %s points to: %s\n", filename,
                  slt);
          }
          FreePoolMemory(slt);
          break;
        }
        case IO_REPARSE_TAG_DEDUP:
          Dmsg0(debuglevel, "We have a deduplicated file!\n");
          sb->st_rdev |= FILE_ATTRIBUTES_DEDUPED_ITEM;

          // We treat a deduped file as a normal file.
          sb->st_mode |= S_IFREG;
          break;
        default:
          Dmsg1(debuglevel,
                "IO_REPARSE_TAG_MOUNT_POINT with unhandled IO_REPARSE_TAG %d\n",
                *pdwReserved0);
          break;
      }
    }
  }

  Dmsg2(debuglevel, "st_rdev=%d filename=%s\n", sb->st_rdev, filename);

  sb->st_size = *pnFileSizeHigh;
  sb->st_size <<= 32;
  sb->st_size |= *pnFileSizeLow;
  sb->st_blksize = 4096;
  sb->st_blocks = (uint32_t)(sb->st_size + 4095) / 4096;

  sb->st_atime = CvtFtimeToUtime(*pftLastAccessTime);
  sb->st_mtime = CvtFtimeToUtime(*pftLastWriteTime);
  sb->st_ctime = CvtFtimeToUtime(*pftChangeTime);

  return 0;
}

int fstat(intptr_t fd, struct stat* sb)
{
  bool use_fallback_data = true;
  BY_HANDLE_FILE_INFORMATION info;

  if (!GetFileInformationByHandle((HANDLE)_get_osfhandle(fd), &info)) {
    const char* err = errorString();

    Dmsg1(2099, "GetfileInformationByHandle: %s\n", err);
    LocalFree((void*)err);
    errno = b_errno_win32;

    return -1;
  }

  // This can become a problem in the future:
  // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information
  // The identifier (low and high parts) and the volume serial number uniquely
  // identify a file on a single computer.  To determine whether two open
  // handles represent the same file, combine the identifier and the volume
  // serial number for each file and compare them.
  // The ReFS file system, introduced with Windows Server 2012, includes
  // 128-bit file identifiers.  To retrieve the 128-bit file identifier use the
  // GetFileInformationByHandleEx function with FileIdInfo to retrieve the
  // FILE_ID_INFO structure. The 64-bit identifier in this structure is not
  // guaranteed to be unique on ReFS.
  // This means that we need to make st_ino at least 128 bit big.
  // The API itself is only available on windows server as it seems:
  // https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_id_info
  // Since the stat struct is something that we serialize, we have to ensure
  // that making st_ino 128 does not mess anything up; especially for
  // "portable" windows backups that you are supposed to be able to restore
  // on other operating systems.

  sb->st_dev = info.dwVolumeSerialNumber;
  sb->st_ino = info.nFileIndexHigh;
  sb->st_ino <<= 32;
  sb->st_ino |= info.nFileIndexLow;
  sb->st_nlink = (short)info.nNumberOfLinks;
  if (sb->st_nlink > 1) { Dmsg1(debuglevel, "st_nlink=%d\n", sb->st_nlink); }

  sb->st_mode = 0777;
  sb->st_mode |= S_IFREG;

  // We store the full windows file attributes into st_rdev.
  sb->st_rdev = info.dwFileAttributes;

  Dmsg3(debuglevel, "st_rdev=%d sizino=%d ino=%lld\n", sb->st_rdev,
        sizeof(sb->st_ino), (long long)sb->st_ino);

  sb->st_size = info.nFileSizeHigh;
  sb->st_size <<= 32;
  sb->st_size |= info.nFileSizeLow;
  sb->st_blksize = 4096;
  sb->st_blocks = (uint32_t)(sb->st_size + 4095) / 4096;

#if (_WIN32_WINNT >= 0x0600)
  if (p_GetFileInformationByHandleEx) {
    FILE_BASIC_INFO basic_info;

    if (p_GetFileInformationByHandleEx((HANDLE)_get_osfhandle(fd),
                                       FileBasicInfo, &basic_info,
                                       sizeof(basic_info))) {
      sb->st_atime = CvtFtimeToUtime(basic_info.LastAccessTime);
      sb->st_mtime = CvtFtimeToUtime(basic_info.LastWriteTime);

      // changetime is not updated when a file is copied to a new location
      // only creation time is updated (dont ask me how you update
      // the creation time without updating the meta data)
      // as such we need to take the maximum here!
      if (CompareFileTime((FILETIME*)&basic_info.ChangeTime,
                          (FILETIME*)&basic_info.CreationTime)
          < 0) {
        sb->st_ctime = CvtFtimeToUtime(basic_info.CreationTime);
      } else {
        sb->st_ctime = CvtFtimeToUtime(basic_info.ChangeTime);
      }
      use_fallback_data = false;
    }
  }
#endif

  if (use_fallback_data) {
    sb->st_atime = CvtFtimeToUtime(info.ftLastAccessTime);
    sb->st_mtime = CvtFtimeToUtime(info.ftLastWriteTime);
    // if changetime is not available we fallback to LastWriteTime
    if (CompareFileTime(&info.ftLastWriteTime, &info.ftCreationTime) < 0) {
      sb->st_ctime = CvtFtimeToUtime(info.ftCreationTime);
    } else {
      sb->st_ctime = CvtFtimeToUtime(info.ftLastWriteTime);
    }
  }

  return 0;
}

static inline bool IsDriveLetterOnly(const char* filename)
{
  return ((filename[1] == ':' && filename[2] == 0)
          || (filename[1] == ':' && filename[2] == '/' && filename[3] == 0));
}

static int stat2(const char* filename, struct stat* sb)
{
  HANDLE h = INVALID_HANDLE_VALUE;
  int rval = 0;
  DWORD attr = (DWORD)-1;


  if (p_GetFileAttributesW) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(filename);
    attr = p_GetFileAttributesW(utf16.c_str());
    if (p_CreateFileW) {
      h = CreateFileW(utf16.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, 0, NULL);
    }
  } else if (p_GetFileAttributesA) {
    PoolMem win32_fname(PM_FNAME);
    unix_name_to_win32(win32_fname.addr(), filename);
    attr = p_GetFileAttributesA(win32_fname.c_str());
    h = CreateFileA(win32_fname.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, 0, NULL);

  } else {
    Emsg0(M_FATAL, 0,
          T_("p_GetFileAttributesW and p_GetFileAttributesA undefined. "
             "probably missing OSDependentInit() call\n"));
  }

  if (attr == (DWORD)-1) {
    const char* err = errorString();

    Dmsg2(2099, "GetFileAttributes(%s): %s\n", filename, err);

    LocalFree((void*)err);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); }
    errno = b_errno_win32;

    rval = -1;
    goto bail_out;
  }

  if (h == INVALID_HANDLE_VALUE) {
    const char* err = errorString();

    Dmsg2(2099, "Cannot open file for stat (%s):%s\n", filename, err);
    LocalFree((void*)err);
    errno = b_errno_win32;

    rval = -1;
    goto bail_out;
  }

  rval = fstat((intptr_t)h, sb);
  CloseHandle(h);

  /* See if this is a directory or a reparse point and its not a single drive
   * letter. If so we call GetWindowsFileInfo() which retrieves the lowlevel
   * information we need. */
  if (((attr & FILE_ATTRIBUTE_DIRECTORY)
       || (attr & FILE_ATTRIBUTE_REPARSE_POINT))
      && !IsDriveLetterOnly(filename)) {
    rval = GetWindowsFileInfo(filename, sb, (attr & FILE_ATTRIBUTE_DIRECTORY));
  }

bail_out:
  return rval;
}

int stat(const char* filename, struct stat* sb)
{
  int rval = 0;
  WIN32_FILE_ATTRIBUTE_DATA data;

  errno = 0;
  memset(sb, 0, sizeof(*sb));

  PoolMem win32_fname(PM_FNAME);
  unix_name_to_win32(win32_fname.addr(), filename);

  std::wstring utf16 = make_win32_path_UTF8_2_wchar(filename);

  if (p_GetFileAttributesExW) {
    BOOL b
        = p_GetFileAttributesExW(utf16.c_str(), GetFileExInfoStandard, &data);
    if (!b) { goto bail_out; }
  } else if (p_GetFileAttributesExA) {
    if (!p_GetFileAttributesExA(win32_fname.c_str(), GetFileExInfoStandard,
                                &data)) {
      goto bail_out;
    }
  } else {
    Emsg0(M_FATAL, 0,
          T_("p_GetFileAttributesExW and p_GetFileAttributesExA undefined. "
             "probably missing OSDependentInit() call\n"));
    goto bail_out;
  }

  /* See if this is a directory or a reparse point and not a single drive
   * letter. If so we call GetWindowsFileInfo() which retrieves the lowlevel
   * information we need. As GetWindowsFileInfo() fills the complete stat
   * structs we only need to perform the other part of the code when we don't
   * call the GetWindowsFileInfo() function. */
  if (((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
       || (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
      && !IsDriveLetterOnly(filename)) {
    rval = GetWindowsFileInfo(
        filename, sb, (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
  } else {
    bool use_fallback_data = true;

    sb->st_mode = 0777;

    // We store the full windows file attributes into st_rdev.
    sb->st_rdev = data.dwFileAttributes;

    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      sb->st_mode |= S_IFDIR;
    } else {
      sb->st_mode |= S_IFREG;
    }

    sb->st_nlink = 1;
    sb->st_size = data.nFileSizeHigh;
    sb->st_size <<= 32;
    sb->st_size |= data.nFileSizeLow;
    sb->st_blksize = 4096;
    sb->st_blocks = (uint32_t)(sb->st_size + 4095) / 4096;

#if (_WIN32_WINNT >= 0x0600)
    // See if GetFileInformationByHandleEx API is available.
    if (p_GetFileInformationByHandleEx) {
      HANDLE h = INVALID_HANDLE_VALUE;

      /* The GetFileInformationByHandleEx need a file handle so we have to
       * open the file or directory read-only. */
      if (p_CreateFileW) {
        h = p_CreateFileW(utf16.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, 0, NULL);
      } else {
        h = CreateFileA(win32_fname.c_str(), GENERIC_READ, FILE_SHARE_READ,
                        NULL, OPEN_EXISTING, 0, NULL);
      }

      if (h != INVALID_HANDLE_VALUE) {
        FILE_BASIC_INFO basic_info;

        if (p_GetFileInformationByHandleEx(h, FileBasicInfo, &basic_info,
                                           sizeof(basic_info))) {
          sb->st_atime = CvtFtimeToUtime(basic_info.LastAccessTime);
          sb->st_mtime = CvtFtimeToUtime(basic_info.LastWriteTime);
          sb->st_ctime = CvtFtimeToUtime(basic_info.ChangeTime);
          use_fallback_data = false;
        }

        CloseHandle(h);
      }
    }
#endif

    if (use_fallback_data) {
      // Fall back to the GetFileAttributesEx data.
      sb->st_atime = CvtFtimeToUtime(data.ftLastAccessTime);
      sb->st_mtime = CvtFtimeToUtime(data.ftLastWriteTime);
      sb->st_ctime = CvtFtimeToUtime(data.ftLastWriteTime);
    }
  }
  rval = 0;

  Dmsg3(debuglevel, "sizino=%d ino=%lld filename=%s\n", sizeof(sb->st_ino),
        (long long)sb->st_ino, filename);

  return rval;

bail_out:

  return stat2(filename, sb);
}

/**
 * Get the Volume MountPoint unique devicename for the given filename.
 * This is a wrapper around GetVolumeMountPointData() used for retrieving
 * the VMP data used in the VSS snapshotting.
 */
bool win32_get_vmp_devicename(const char* filename, POOLMEM*& devicename)
{
  return GetVolumeMountPointData(filename, devicename);
}

/**
 * We write our own ftruncate because the one in the
 *  Microsoft library mrcrt.dll does not truncate
 *  files greater than 2GB.
 *  KES - May 2007
 */
int win32_ftruncate(int fd, int64_t length)
{
  /* Set point we want to truncate file */
  __int64 pos = _lseeki64(fd, (__int64)length, SEEK_SET);

  if (pos != (__int64)length) {
    errno = EACCES; /* truncation failed, get out */
    return -1;
  }

  /* Truncate file */
  if (SetEndOfFile((HANDLE)_get_osfhandle(fd)) == 0) {
    errno = b_errno_win32;
    return -1;
  }
  errno = 0;
  return 0;
}

int fcntl(int, int cmd, long)
{
  int rval = 0;

  switch (cmd) {
    case F_GETFL:
      rval = O_NONBLOCK;
      break;

    case F_SETFL:
      rval = 0;
      break;

    default:
      errno = EINVAL;
      rval = -1;
      break;
  }

  return rval;
}

int lstat(const char* filename, struct stat* sb) { return stat(filename, sb); }

void sleep(int sec) { Sleep(sec * 1000); }

int geteuid(void) { return 0; }

int execvp(const char*, char*[])
{
  errno = ENOSYS;
  return -1;
}

int fork(void)
{
  errno = ENOSYS;
  return -1;
}

int pipe(int[])
{
  errno = ENOSYS;
  return -1;
}

int waitpid(int, int*, int)
{
  errno = ENOSYS;
  return -1;
}

// Read contents of symbolic link
ssize_t readlink(const char* path, char* buf, size_t bufsiz)
{
  POOLMEM* slt = GetPoolMemory(PM_NAME);

  Dmsg1(debuglevel, "readlink called for path %s\n", path);
  GetSymlinkData(path, slt);

  strncpy(buf, slt, bufsiz - 1);
  buf[bufsiz] = '\0';
  FreePoolMemory(slt);

  return strlen(buf);
}

// Create a directory symlink / file symlink/junction
int win32_symlink(const char* name1, const char* name2, _dev_t st_rdev)
{
#if (_WIN32_WINNT >= 0x0600)
  int dwFlags = 0x0;

  if (st_rdev & FILE_ATTRIBUTES_JUNCTION_POINT) {
    Dmsg0(130, "We have a Junction Point \n");
    if (!CreateJunction(name2, name1)) {
      return -1;
    } else {
      return 0;
    }
  } else if (st_rdev & FILE_ATTRIBUTE_VOLUME_MOUNT_POINT) {
    Dmsg0(130, "We have a Volume Mount Point \n");
    return 0;
  } else if (st_rdev & FILE_ATTRIBUTES_SYMBOLIC_LINK) {
    Dmsg0(130, "We have a Directory Symbolic Link\n");
    dwFlags = SYMBOLIC_LINK_FLAG_DIRECTORY;
  } else {
    Dmsg0(130, "We have a File Symbolic Link \n");
  }

  Dmsg2(debuglevel, "symlink called name1=%s, name2=%s\n", name1, name2);
  if (p_CreateSymbolicLinkW) {
    std::wstring target = FromUtf8(name1);
    if (target.size() == 0) { goto bail_out; }
    std::wstring symlink = make_win32_path_UTF8_2_wchar(name2);

    BOOL b = p_CreateSymbolicLinkW(symlink.c_str(), target.c_str(), dwFlags);

    if (!b) {
      Dmsg1(debuglevel, "CreateSymbolicLinkW failed:%s\n", errorString());
      goto bail_out;
    }
  } else if (p_CreateSymbolicLinkA) {
    POOLMEM* win32_name1 = GetPoolMemory(PM_FNAME);
    POOLMEM* win32_name2 = GetPoolMemory(PM_FNAME);
    unix_name_to_win32(win32_name1, name1);
    unix_name_to_win32(win32_name2, name2);

    BOOL b = p_CreateSymbolicLinkA(win32_name2, win32_name1, dwFlags);

    FreePoolMemory(win32_name1);
    FreePoolMemory(win32_name2);

    if (!b) {
      Dmsg1(debuglevel, "CreateSymbolicLinkA failed:%s\n", errorString());
      goto bail_out;
    }
  } else {
    errno = ENOSYS;
    goto bail_out;
  }

  return 0;

bail_out:
  return -1;
#else
  errno = ENOSYS;
  return -1;
#endif
}

// Create a hardlink
int link(const char*, const char*)
{
  errno = ENOSYS;
  return -1;
}

int gettimeofday(struct timeval* tv, struct timezone* tz)
{
  return mingw_gettimeofday(tv, tz);
}

// Write in Windows System log
extern "C" void syslog(int, const char* fmt, ...)
{
  va_list arg_ptr;
  int len, maxlen;
  POOLMEM* msg;

  msg = GetPoolMemory(PM_EMSG);

  for (;;) {
    maxlen = SizeofPoolMemory(msg) - 1;
    va_start(arg_ptr, fmt);
    len = Bvsnprintf(msg, maxlen, fmt, arg_ptr);
    va_end(arg_ptr);
    if (len < 0 || len >= (maxlen - 5)) {
      msg = ReallocPoolMemory(msg, maxlen + maxlen / 2);
      continue;
    }
    break;
  }
  LogErrorMsg((const char*)msg);
  FreeMemory(msg);
}

extern "C" void closelog() {}

struct passwd* getpwuid(uid_t) { return NULL; }

struct group* getgrgid(uid_t) { return NULL; }

// Implement opendir/readdir/closedir on top of window's API
typedef struct _dir {
  WIN32_FIND_DATAA data_a; /* window's file info (ansii version) */
  WIN32_FIND_DATAW data_w; /* window's file info (wchar version) */
  const char* spec;        /* the directory we're traversing */
  HANDLE dirh;             /* the search handle */
  BOOL valid_a;            /* the info in data_a field is valid */
  BOOL valid_w;            /* the info in data_w field is valid */
  UINT32 offset;           /* pseudo offset for d_off */
} _dir;

DIR* opendir(const char* path)
{
  if (path == NULL) {
    errno = ENOENT;
    return NULL;
  }

  // unix_name_to_win32 works like this:
  // C:/mountpoint -> ShadowCopyOfC/mountpoint
  // C:/mountpoint/stuff -> ShadowCopyOfMountpoint/stuff
  // since we are inspecting the contents of the mountpoint
  // we want to ensure we are in the second case!
  std::string dir_path{path};

  if (!IsPathSeparator(dir_path.back())) { dir_path.push_back('/'); }
  dir_path.push_back('*');

  Dmsg1(debuglevel, "Opendir path=%s\n", dir_path.c_str());
  _dir* rval = (_dir*)malloc(sizeof(_dir));
  memset(rval, 0, sizeof(_dir));

  POOLMEM* win32_path = GetPoolMemory(PM_FNAME);
  unix_name_to_win32(win32_path, dir_path.c_str());
  Dmsg1(debuglevel, "win32 path=%s\n", win32_path);

  rval->spec = win32_path;

  // convert to wchar_t
  if (p_FindFirstFileW) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(dir_path.c_str());
    rval->dirh = p_FindFirstFileW(utf16.c_str(), &rval->data_w);
    if (rval->dirh != INVALID_HANDLE_VALUE) { rval->valid_w = 1; }
  } else if (p_FindFirstFileA) {
    rval->dirh = p_FindFirstFileA(rval->spec, &rval->data_a);
    if (rval->dirh != INVALID_HANDLE_VALUE) { rval->valid_a = 1; }
  } else {
    goto bail_out;
  }

  Dmsg3(debuglevel, "opendir(%s)\n\tspec=%s,\n\tFindFirstFile returns %d\n",
        path, rval->spec, rval->dirh);

  rval->offset = 0;
  if (rval->dirh == INVALID_HANDLE_VALUE) { goto bail_out; }

  if (rval->valid_w) {
    Dmsg1(debuglevel, "\tFirstFile=%s\n", rval->data_w.cFileName);
  }

  if (rval->valid_a) {
    Dmsg1(debuglevel, "\tFirstFile=%s\n", rval->data_a.cFileName);
  }

  return (DIR*)rval;

bail_out:
  if (rval) { free(rval); }

  FreePoolMemory(win32_path);

  errno = b_errno_win32;
  return NULL;
}

int closedir(DIR* dirp)
{
  _dir* dp = (_dir*)dirp;
  FindClose(dp->dirh);
  FreePoolMemory((POOLMEM*)dp->spec);
  free((void*)dp);
  return 0;
}

static int copyin(struct dirent& dp, const char* fname)
{
  char* cp;

  dp.d_ino = 0;
  dp.d_reclen = 0;
  cp = dp.d_name;
  while (*fname) {
    *cp++ = *fname++;
    dp.d_reclen++;
  }
  *cp = 0;

  return dp.d_reclen;
}

#ifdef USE_READDIR_R
int Readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result)
{
  _dir* dp = (_dir*)dirp;
  if (dp->valid_w || dp->valid_a) {
    entry->d_off = dp->offset;

    // copy unicode
    if (dp->valid_w) {
      POOLMEM* szBuf;

      szBuf = GetPoolMemory(PM_NAME);
      wchar_2_UTF8(szBuf, dp->data_w.cFileName);
      dp->offset += copyin(*entry, szBuf);
      FreePoolMemory(szBuf);
    } else if (dp->valid_a) {  // copy ansi (only 1 will be valid)
      dp->offset += copyin(*entry, dp->data_a.cFileName);
    }

    *result = entry; /* return entry address */
    Dmsg4(debuglevel, "Readdir_r(%p, { d_name=\"%s\", d_reclen=%d, d_off=%d\n",
          dirp, entry->d_name, entry->d_reclen, entry->d_off);
  } else {
    errno = b_errno_win32;
    return -1;
  }

  // get next file, try unicode first
  if (p_FindNextFileW) {
    dp->valid_w = p_FindNextFileW(dp->dirh, &dp->data_w);
  } else if (p_FindNextFileA) {
    dp->valid_a = p_FindNextFileA(dp->dirh, &dp->data_a);
  } else {
    dp->valid_a = FALSE;
    dp->valid_w = FALSE;
  }

  return 0;
}
#endif
/**
 * Dotted IP address to network address
 *
 * Returns 1 if  OK
 *         0 on error
 */
int inet_aton(const char* a, struct in_addr* inp)
{
  const char* cp = a;
  uint32_t acc = 0, tmp = 0;
  int dotc = 0;

  if (!isdigit(*cp)) { /* first char must be digit */
    return 0;          /* error */
  }
  do {
    if (isdigit(*cp)) {
      tmp = (tmp * 10) + (*cp - '0');
    } else if (*cp == '.' || *cp == 0) {
      if (tmp > 255) { return 0; /* error */ }
      acc = (acc << 8) + tmp;
      dotc++;
      tmp = 0;
    } else {
      return 0; /* error */
    }
  } while (*cp++ != 0);
  if (dotc != 4) { /* want 3 .'s plus EOS */
    return 0;      /* error */
  }
  inp->s_addr = htonl(acc); /* store addr in network format */
  return 1;
}

void InitSignals([[maybe_unused]] void Terminate(int sig)) {}

void InitStackDump(void) {}

long pathconf(const char* path, int name)
{
  switch (name) {
    case _PC_PATH_MAX:
      if (strncmp(path, "\\\\?\\", 4) == 0) return 32767;
      [[fallthrough]];
    case _PC_NAME_MAX:
      return 255;
  }
  errno = ENOSYS;
  return -1;
}

int WSA_Init(void)
{
  WORD wVersionRequested = MAKEWORD(1, 1);
  WSADATA wsaData;

  int err = WSAStartup(wVersionRequested, &wsaData);

  if (err != 0) {
    printf("Can not start Windows Sockets\n");
    errno = ENOSYS;
    return -1;
  }

  return 0;
}

static DWORD fill_attribute(DWORD attr, mode_t mode, _dev_t rdev)
{
  bool compatible = false;

  Dmsg1(debuglevel, "  before attr=%lld\n", (uint64_t)attr);

  // First see if there are any old encoded attributes in the mode.

  // If file is readable then this is not READONLY
  if (mode & (S_IRUSR | S_IRGRP | S_IROTH)) {
    attr &= ~FILE_ATTRIBUTE_READONLY;
  } else {
    attr |= FILE_ATTRIBUTE_READONLY;
    compatible = true;
  }

  // The sticky bit <=> HIDDEN
  if (mode & S_ISVTX) {
    attr |= FILE_ATTRIBUTE_HIDDEN;
    compatible = true;
  } else {
    attr &= ~FILE_ATTRIBUTE_HIDDEN;
  }

  /* Other can read/write/execute ?
   * => Not system */
  if (mode & S_IRWXO) {
    attr &= ~FILE_ATTRIBUTE_SYSTEM;
  } else {
    attr |= FILE_ATTRIBUTE_SYSTEM;
    compatible = true;
  }

  /* See if there are any compatible mode settings in the to restore mode.
   * If so we don't trust the rdev setting as it is from an Bacula compatible
   * Filedaemon and as such doesn't encode the windows file flags in st_rdev
   * but uses st_rdev for marking reparse points. */
  if (!compatible) {
    // Based on the setting of rdev restore any special file flags.
    if (rdev & FILE_ATTRIBUTE_READONLY) {
      attr |= FILE_ATTRIBUTE_READONLY;
    } else {
      attr &= ~FILE_ATTRIBUTE_READONLY;
    }

    if (rdev & FILE_ATTRIBUTE_HIDDEN) {
      attr |= FILE_ATTRIBUTE_HIDDEN;
    } else {
      attr &= ~FILE_ATTRIBUTE_HIDDEN;
    }

    if (rdev & FILE_ATTRIBUTE_SYSTEM) {
      attr |= FILE_ATTRIBUTE_SYSTEM;
    } else {
      attr &= ~FILE_ATTRIBUTE_SYSTEM;
    }

    if (rdev & FILE_ATTRIBUTE_ARCHIVE) {
      attr |= FILE_ATTRIBUTE_ARCHIVE;
    } else {
      attr &= ~FILE_ATTRIBUTE_ARCHIVE;
    }

    if (rdev & FILE_ATTRIBUTE_TEMPORARY) {
      attr |= FILE_ATTRIBUTE_TEMPORARY;
    } else {
      attr &= ~FILE_ATTRIBUTE_TEMPORARY;
    }

    if (rdev & FILE_ATTRIBUTE_OFFLINE) {
      attr |= FILE_ATTRIBUTE_OFFLINE;
    } else {
      attr &= ~FILE_ATTRIBUTE_OFFLINE;
    }

    if (rdev & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) {
      attr |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    } else {
      attr &= ~FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    }
  }

  Dmsg1(debuglevel, "  after attr=%lld\n", (uint64_t)attr);

  return attr;
}

int win32_chmod(const char* path, mode_t mode, _dev_t rdev)
{
  bool ret = false;
  DWORD attr;

  Dmsg3(debuglevel, "win32_chmod(path=%s mode=%lld, rdev=%lld)\n", path,
        (uint64_t)mode, (uint64_t)rdev);

  if (p_GetFileAttributesW) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(path);
    attr = p_GetFileAttributesW(utf16.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
      // Use Bareos mappings define in stat() above
      attr = fill_attribute(attr, mode, rdev);
      ret = p_SetFileAttributesW(utf16.c_str(), attr);
    }
    Dmsg0(debuglevel, "Leave win32_chmod. AttributesW\n");
  } else if (p_GetFileAttributesA) {
    attr = p_GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES) {
      attr = fill_attribute(attr, mode, rdev);
      ret = p_SetFileAttributesA(path, attr);
    }
    Dmsg0(debuglevel, "Leave win32_chmod did AttributesA\n");
  } else {
    Dmsg0(debuglevel, "Leave win32_chmod did nothing\n");
  }

  if (!ret) {
    const char* err = errorString();

    Dmsg2(debuglevel, "Get/SetFileAttributes(%s): %s\n", path, err);
    LocalFree((void*)err);
    errno = b_errno_win32;

    return -1;
  }

  return 0;
}

int win32_chdir(const char* dir)
{
  if (p_SetCurrentDirectoryW) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(dir);
    if (!p_SetCurrentDirectoryW(utf16.c_str())) {
      errno = b_errno_win32;
      return -1;
    }
  } else if (p_SetCurrentDirectoryA) {
    if (0 == p_SetCurrentDirectoryA(dir)) {
      errno = b_errno_win32;
      return -1;
    }
  } else {
    return -1;
  }

  return 0;
}

int win32_mkdir(const char* dir)
{
  Dmsg1(debuglevel, "enter win32_mkdir. dir=%s\n", dir);
  if (p_wmkdir) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(dir);
    int n = p_wmkdir(utf16.c_str());
    Dmsg0(debuglevel, "Leave win32_mkdir did wmkdir\n");

    return n;
  }

  Dmsg0(debuglevel, "Leave win32_mkdir did _mkdir\n");
  return _mkdir(dir);
}

char* win32_getcwd(char* buf, int maxlen)
{
  int n = 0;

  if (p_GetCurrentDirectoryW) {
    POOLMEM* pwszBuf;

    pwszBuf = GetPoolMemory(PM_FNAME);
    pwszBuf = CheckPoolMemorySize(pwszBuf, maxlen * sizeof(wchar_t));
    n = p_GetCurrentDirectoryW(maxlen, (LPWSTR)pwszBuf);
    if (n != 0) { n = wchar_2_UTF8(buf, (wchar_t*)pwszBuf, maxlen) - 1; }
    FreePoolMemory(pwszBuf);
  } else if (p_GetCurrentDirectoryA) {
    n = p_GetCurrentDirectoryA(maxlen, buf);
  }

  if (n <= 0 || n > maxlen) { return NULL; }

  if (n + 1 > maxlen) { return NULL; }

  if (n != 3) {
    buf[n] = '\\';
    buf[n + 1] = 0;
  }

  return buf;
}

int win32_fputs(const char* string, FILE* stream)
{
  /* We use WriteConsoleW / WriteConsoleA so we can be sure that unicode support
   * works on win32. with fallback if something fails */
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut && (hOut != INVALID_HANDLE_VALUE) && p_WideCharToMultiByte
      && p_MultiByteToWideChar && (stream == stdout)) {
    POOLMEM* pszBuf;
    DWORD dwCharsWritten;
    DWORD dwChars;

    std::wstring utf16 = FromUtf8(string);

    // Try WriteConsoleW
    if (WriteConsoleW(hOut, utf16.c_str(), utf16.size(), &dwCharsWritten,
                      NULL)) {
      return dwCharsWritten;
    }


    DWORD needed = p_WideCharToMultiByte(GetConsoleOutputCP(), 0, utf16.c_str(),
                                         -1, nullptr, 0, nullptr, nullptr);
    // Convert to local codepage and try WriteConsoleA
    pszBuf = GetPoolMemory(PM_MESSAGE);
    pszBuf = CheckPoolMemorySize(pszBuf, needed);
    dwChars = p_WideCharToMultiByte(GetConsoleOutputCP(), 0, utf16.c_str(), -1,
                                    pszBuf, needed, NULL, NULL);
    if (WriteConsoleA(hOut, pszBuf, dwChars - 1, &dwCharsWritten, NULL)) {
      FreePoolMemory(pszBuf);
      return dwCharsWritten;
    }
    FreePoolMemory(pszBuf);
  }

  // Fall back
  return fputs(string, stream);
}

char* win32_cgets(char* buffer, int len)
{
  /* We use console ReadConsoleA / ReadConsoleW to be able to read unicode
   * from the win32 console and fallback if seomething fails */
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if (hIn && (hIn != INVALID_HANDLE_VALUE) && p_WideCharToMultiByte
      && p_MultiByteToWideChar) {
    DWORD dwRead;
    wchar_t wszBuf[1024];
    char szBuf[1024];

    // NT and unicode conversion
    if (ReadConsoleW(hIn, wszBuf, 1024, &dwRead, NULL)) {
      // NULL Terminate at end
      if (wszBuf[dwRead - 1] == L'\n') {
        wszBuf[dwRead - 1] = L'\0';
        dwRead--;
      }

      if (wszBuf[dwRead - 1] == L'\r') {
        wszBuf[dwRead - 1] = L'\0';
        dwRead--;
      }

      wchar_2_UTF8(buffer, wszBuf, len);
      return buffer;
    }

    // WIN 9x and unicode conversion
    if (ReadConsoleA(hIn, szBuf, 1024, &dwRead, NULL)) {
      // NULL Terminate at end
      if (szBuf[dwRead - 1] == L'\n') {
        szBuf[dwRead - 1] = L'\0';
        dwRead--;
      }

      if (szBuf[dwRead - 1] == L'\r') {
        szBuf[dwRead - 1] = L'\0';
        dwRead--;
      }

      // Convert from ansii to wchar_t
      p_MultiByteToWideChar(GetConsoleCP(), 0, szBuf, -1, wszBuf, 1024);

      // Convert from wchar_t to UTF-8
      if (wchar_2_UTF8(buffer, wszBuf, len)) return buffer;
    }
  }

  // Fallback
  if (fgets(buffer, len, stdin))
    return buffer;
  else
    return NULL;
}

int win32_unlink(const char* filename)
{
  int nRetCode;
  if (p_wunlink) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(filename);
    nRetCode = _wunlink(utf16.c_str());

    // Special case if file is readonly, we retry but unset attribute before
    if (nRetCode == -1 && errno == EACCES && p_SetFileAttributesW
        && p_GetFileAttributesW) {
      DWORD dwAttr = p_GetFileAttributesW(utf16.c_str());
      if (dwAttr != INVALID_FILE_ATTRIBUTES) {
        if (p_SetFileAttributesW(utf16.c_str(),
                                 dwAttr & ~FILE_ATTRIBUTE_READONLY)) {
          nRetCode = _wunlink(utf16.c_str());
          /* reset to original if it didn't help */
          if (nRetCode == -1) p_SetFileAttributesW(utf16.c_str(), dwAttr);
        }
      }
    }

    /* if deletion with _unlink failed, try to use DeleteFileW (which also can
     * remove file links) */
    if (nRetCode == -1) {
      Dmsg0(100, "_unlink failed, trying DeleteFileW \n");
      if (DeleteFileW(utf16.c_str()) == 0) {  // 0 = fail
        Dmsg0(100, "DeleteFileW failed\n");
        nRetCode = -1;
      } else {
        Dmsg0(100, "DeleteFileW success\n");
        nRetCode = 0;
      }
    }


    /* if deletion with DeleteFileW failed, try to use RemoveDirectoryW (which
     * also can remove directory links) */
    if (nRetCode == -1) {
      Dmsg0(100, "DeleteFileW failed, trying RemoveDirectoryW \n");
      if (RemoveDirectoryW(utf16.c_str()) == 0) {  // 0 = fail
        Dmsg0(100, "RemoveDirectoryW failed\n");
        nRetCode = -1;
      } else {
        Dmsg0(100, "RemoveDirectoryW success\n");
        nRetCode = 0;
      }
    }
  } else {
    nRetCode = _unlink(filename);

    // Special case if file is readonly, we retry but unset attribute before
    if (nRetCode == -1 && errno == EACCES && p_SetFileAttributesA
        && p_GetFileAttributesA) {
      DWORD dwAttr = p_GetFileAttributesA(filename);
      if (dwAttr != INVALID_FILE_ATTRIBUTES) {
        if (p_SetFileAttributesA(filename, dwAttr & ~FILE_ATTRIBUTE_READONLY)) {
          nRetCode = _unlink(filename);
          // Reset to original if it didn't help
          if (nRetCode == -1) p_SetFileAttributesA(filename, dwAttr);
        }
      }
    }
  }
  return nRetCode;
}

// Define attributes that are legal to set with SetFileAttributes()
#define SET_ATTRS                                                         \
  (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL \
   | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_OFFLINE          \
   | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM                      \
   | FILE_ATTRIBUTE_TEMPORARY)

bool win32_restore_file_attributes(POOLMEM* ofname,
                                   HANDLE handle,
                                   WIN32_FILE_ATTRIBUTE_DATA* atts)
{
  bool retval = false;

  Dmsg1(100, "SetFileAtts %s\n", ofname);

  if (p_SetFileAttributesW) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(ofname);

    BOOL b = p_SetFileAttributesW(utf16.c_str(),
                                  atts->dwFileAttributes & SET_ATTRS);
    if (!b) { goto bail_out; }
  } else {
    PoolMem win32_ofile(PM_FNAME);
    unix_name_to_win32(win32_ofile.addr(), ofname);
    BOOL b = p_SetFileAttributesA(win32_ofile.c_str(),
                                  atts->dwFileAttributes & SET_ATTRS);
    if (!b) { goto bail_out; }
  }

  if (handle != INVALID_HANDLE_VALUE) {
    if (atts->dwFileAttributes & FILE_ATTRIBUTES_DEDUPED_ITEM) {
      Dmsg1(100, "File %s is a FILE_ATTRIBUTES_DEDUPED_ITEM\n", ofname);
    }
    // Restore the sparse file attribute on the restored file.
    if (atts->dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) {
      DWORD bytesreturned;

      Dmsg1(100, "Restore FILE_ATTRIBUTE_SPARSE_FILE on %s\n", ofname);
      if (!DeviceIoControl(handle, FSCTL_SET_SPARSE, NULL, 0, NULL, 0,
                           &bytesreturned, NULL)) {
        goto bail_out;
      }
    }

    // Restore the compressed file attribute on the restored file.
    if (atts->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) {
      USHORT format = COMPRESSION_FORMAT_DEFAULT;
      DWORD bytesreturned;

      Dmsg1(100, "Restore FILE_ATTRIBUTE_COMPRESSED on %s\n", ofname);
      if (!DeviceIoControl(handle, FSCTL_SET_COMPRESSION, &format,
                           sizeof(format), NULL, 0, &bytesreturned, NULL)) {
        goto bail_out;
      }
    }

    // Restore file times on the restored file.
    Dmsg1(100, "SetFileTime %s\n", ofname);
    if (!SetFileTime(handle, &atts->ftCreationTime, &atts->ftLastAccessTime,
                     &atts->ftLastWriteTime)) {
      goto bail_out;
    }
  }

  retval = true;

bail_out:
  return retval;
}

BOOL CreateChildProcess(VOID);
VOID WriteToPipe(VOID);
VOID ReadFromPipe(VOID);
VOID ErrorExit(LPCSTR);
VOID ErrMsg(LPTSTR, BOOL);

/**
 * Check for a quoted path,  if an absolute path name is given and it contains
 * spaces it will need to be quoted.  i.e.  "c:/Program Files/foo/bar.exe"
 * CreateProcess() says the best way to ensure proper results with executables
 * with spaces in path or filename is to quote the string.
 */
const char* getArgv0(const char* cmdline)
{
  int inquote = 0;
  const char* cp;
  for (cp = cmdline; *cp; cp++) {
    if (*cp == '"') { inquote = !inquote; }
    if (!inquote && isspace(*cp)) break;
  }


  int len = cp - cmdline;
  char* rval = (char*)malloc(len + 1);

  cp = cmdline;
  char* rp = rval;

  while (len--) *rp++ = *cp++;

  *rp = 0;
  return rval;
}

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
bool GetApplicationName(const char* cmdline, char** pexe, const char** pargs)
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
HANDLE CreateChildProcess(const char* cmdline,
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
    const char* err = errorString();

    Dmsg3(debuglevel, "CreateProcess(%s, %s, ...)=%s\n", comspec,
          cmdLine.c_str(), err);
    LocalFree((void*)err);

    return INVALID_HANDLE_VALUE;
  }

  // we don't need a handle on the process primary thread so we close this now.
  CloseHandle(piProcInfo.hThread);

  return piProcInfo.hProcess;
}

void ErrorExit(LPCSTR lpszMessage) { Dmsg1(0, "%s", lpszMessage); }

static void CloseHandleIfValid(HANDLE handle)
{
  if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); }
}

Bpipe* OpenBpipe(char* prog, int wait, const char* mode, bool)
{
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
  bpipe->worker_pid
      = (pid_t)CreateChildProcess(prog,            /* Commandline */
                                  hChildStdinRd,   /* stdin HANDLE */
                                  hChildStdoutWr,  /* stdout HANDLE */
                                  hChildStdoutWr); /* stderr HANDLE */

  if ((HANDLE)bpipe->worker_pid == INVALID_HANDLE_VALUE) goto cleanup;

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
    bpipe->timer_id = start_child_timer(NULL, bpipe->worker_pid, wait);
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

int kill(int pid, int signal)
{
  int rval = 0;
  if (!TerminateProcess((HANDLE)(UINT_PTR)pid, (UINT)signal)) {
    rval = -1;
    errno = b_errno_win32;
  }
  CloseHandle((HANDLE)(UINT_PTR)pid);
  return rval;
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

// Syslog function, added by Nicolas Boichat
extern "C" void openlog(const char*, int, int) {}

// Log an error message
void LogErrorMsg(const char* message)
{
  HANDLE eventHandler;
  const char* strings[2];

  // Use the OS event logging to log the error
  strings[0] = T_("\n\nBareos ERROR: ");
  strings[1] = message;

  eventHandler = RegisterEventSource(NULL, "Bareos");
  if (eventHandler) {
    ReportEvent(eventHandler, EVENTLOG_ERROR_TYPE, 0, /* Category */
                0,                                    /* ID */
                NULL,                                 /* SID */
                2,                                    /* Number of strings */
                0,                                    /* Raw data size */
                (const char**)strings,                /* Error strings */
                NULL);                                /* Raw data */
    DeregisterEventSource(eventHandler);
  }
}

/**
 * Don't allow OS to suspend while backup running
 * Note, the OS automatically tracks these for each thread
 */
void PreventOsSuspensions()
{
  SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
}

void AllowOsSuspensions() { SetThreadExecutionState(ES_CONTINUOUS); }
