/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
//                              -*- Mode: C++ -*-
// compat.cpp -- compatibilty layer to make bacula-fd run
//               natively under windows
//
// Copyright transferred from Raider Solutions, Inc to
//   Kern Sibbald and John Walker by express permission.
//
// Author          : Christopher S. Hull
// Created On      : Sat Jan 31 15:55:00 2004


#include "bacula.h"
#include "compat.h"
#include "jcr.h"
#include "findlib/find.h"

/* Note, if you want to see what Windows variables and structures
 * are defined, bacula.h includes <windows.h>, which is found in:
 *
 *   cross-tools/mingw32/mingw32/include
 * or
 *   cross-tools/mingw-w64/x86_64-pc-mingw32/include
 * 
 * depending on whether we are building the 32 bit version or
 * the 64 bit version. 
 */

static const int dbglvl = 500;

#define b_errno_win32 (1<<29)

#define MAX_PATHLENGTH  1024

/**
   UTF-8 to UCS2 path conversion is expensive,
   so we cache the conversion. During backup the
   conversion is called 3 times (lstat, attribs, open),
   by using the cache this is reduced to 1 time
 */
static POOLMEM *g_pWin32ConvUTF8Cache = NULL;
static POOLMEM *g_pWin32ConvUCS2Cache = NULL;
static DWORD g_dwWin32ConvUTF8strlen = 0;
static pthread_mutex_t Win32Convmutex = PTHREAD_MUTEX_INITIALIZER;

static t_pVSSPathConvert   g_pVSSPathConvert;
static t_pVSSPathConvertW  g_pVSSPathConvertW;

/* Forward referenced functions */
static const char *errorString(void);


void SetVSSPathConvert(t_pVSSPathConvert pPathConvert, t_pVSSPathConvertW pPathConvertW)
{
   g_pVSSPathConvert = pPathConvert;
   g_pVSSPathConvertW = pPathConvertW;
}

static void Win32ConvInitCache()
{
   if (g_pWin32ConvUTF8Cache) {
      return;
   }
   g_pWin32ConvUTF8Cache = get_pool_memory(PM_FNAME);
   g_pWin32ConvUCS2Cache = get_pool_memory(PM_FNAME);
}

void Win32ConvCleanupCache()
{
   P(Win32Convmutex);
   if (g_pWin32ConvUTF8Cache) {
      free_pool_memory(g_pWin32ConvUTF8Cache);
      g_pWin32ConvUTF8Cache = NULL;
   }

   if (g_pWin32ConvUCS2Cache) {
      free_pool_memory(g_pWin32ConvUCS2Cache);   
      g_pWin32ConvUCS2Cache = NULL;
   }

   g_dwWin32ConvUTF8strlen = 0;
   V(Win32Convmutex);
}


/* to allow the usage of the original version in this file here */
#undef fputs


//#define USE_WIN32_COMPAT_IO 1
#define USE_WIN32_32KPATHCONVERSION 1

extern DWORD   g_platform_id;
extern DWORD   g_MinorVersion;

/* From Microsoft SDK (KES) is the diff between Jan 1 1601 and Jan 1 1970 */
#ifdef HAVE_MINGW
#define WIN32_FILETIME_ADJUST 0x19DB1DED53E8000ULL 
#else
#define WIN32_FILETIME_ADJUST 0x19DB1DED53E8000I64
#endif

#define WIN32_FILETIME_SCALE  10000000             // 100ns/second

/**
 * Convert from UTF-8 to VSS Windows path/file 
 *  Used by compatibility layer for Unix system calls
 */
static void conv_unix_to_vss_win32_path(const char *name, char *win32_name, DWORD dwSize)
{
    const char *fname = name;
    char *tname = win32_name;

    Dmsg0(dbglvl, "Enter convert_unix_to_win32_path\n");

    if (IsPathSeparator(name[0]) &&
        IsPathSeparator(name[1]) &&
        name[2] == '.' &&
        IsPathSeparator(name[3])) {

        *win32_name++ = '\\';
        *win32_name++ = '\\';
        *win32_name++ = '.';
        *win32_name++ = '\\';

        name += 4;
    } else if (g_platform_id != VER_PLATFORM_WIN32_WINDOWS &&
               g_pVSSPathConvert == NULL) {
        /* allow path to be 32767 bytes */
        *win32_name++ = '\\';
        *win32_name++ = '\\';
        *win32_name++ = '?';
        *win32_name++ = '\\';
    }

    while (*name) {
        /** Check for Unix separator and convert to Win32 */
        if (name[0] == '/' && name[1] == '/') {  /* double slash? */
           name++;                               /* yes, skip first one */
        }
        if (*name == '/') {
            *win32_name++ = '\\';     /* convert char */
        /* If Win32 separator that is "quoted", remove quote */
        } else if (*name == '\\' && name[1] == '\\') {
            *win32_name++ = '\\';
            name++;                   /* skip first \ */
        } else {
            *win32_name++ = *name;    /* copy character */
        }
        name++;
    }
    /** Strip any trailing slash, if we stored something
     * but leave "c:\" with backslash (root directory case
     */
    if (*fname != 0 && win32_name[-1] == '\\' && strlen (fname) != 3) {
        win32_name[-1] = 0;
    } else {
        *win32_name = 0;
    }

    /** here we convert to VSS specific file name which
       can get longer because VSS will make something like
       \\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1\\bacula\\uninstall.exe
       from c:\bacula\uninstall.exe
    */
    Dmsg1(dbglvl, "path=%s\n", tname);
    if (g_pVSSPathConvert != NULL) {
       POOLMEM *pszBuf = get_pool_memory (PM_FNAME);
       pszBuf = check_pool_memory_size(pszBuf, dwSize);
       bstrncpy(pszBuf, tname, strlen(tname)+1);
       g_pVSSPathConvert(pszBuf, tname, dwSize);
       free_pool_memory(pszBuf);
    }

    Dmsg1(dbglvl, "Leave cvt_u_to_win32_path path=%s\n", tname);
}

/** Conversion of a Unix filename to a Win32 filename */
void unix_name_to_win32(POOLMEM **win32_name, char *name)
{
   /* One extra byte should suffice, but we double it */
   /* add MAX_PATH bytes for VSS shadow copy name */
   DWORD dwSize = 2*strlen(name)+MAX_PATH;
   *win32_name = check_pool_memory_size(*win32_name, dwSize);
   conv_unix_to_vss_win32_path(name, *win32_name, dwSize);
}


/**
 * This function expects an UCS-encoded standard wchar_t in pszUCSPath and
 * will complete the input path to an absolue path of the form \\?\c:\path\file
 * 
 * With this trick, it is possible to have 32K characters long paths.
 *
 * Optionally one can use pBIsRawPath to determine id pszUCSPath contains a path
 * to a raw windows partition.  
 *
 *        created 02/27/2006 Thorsten Engel
 */
static POOLMEM* 
make_wchar_win32_path(POOLMEM *pszUCSPath, BOOL *pBIsRawPath /*= NULL*/)
{

   Dmsg0(dbglvl, "Enter wchar_win32_path\n");
   if (pBIsRawPath) {
      *pBIsRawPath = FALSE;              /* Initialize, set later */
   }

   if (!p_GetCurrentDirectoryW) {
      Dmsg0(dbglvl, "Leave wchar_win32_path no change \n");
      return pszUCSPath;
   }
   
   wchar_t *name = (wchar_t *)pszUCSPath;

   /* if it has already the desired form, exit without changes */
   if (wcslen(name) > 3 && wcsncmp(name, L"\\\\?\\", 4) == 0) {
      Dmsg0(dbglvl, "Leave wchar_win32_path no change \n");
      return pszUCSPath;
   }

   wchar_t *pwszBuf = (wchar_t *)get_pool_memory(PM_FNAME);
   wchar_t *pwszCurDirBuf = (wchar_t *)get_pool_memory(PM_FNAME);
   DWORD dwCurDirPathSize = 0;

   /* get buffer with enough size (name+max 6. wchars+1 null terminator */
   DWORD dwBufCharsNeeded = (wcslen(name)+7);
   pwszBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pwszBuf, dwBufCharsNeeded*sizeof(wchar_t));
      
   /* add \\?\ to support 32K long filepaths 
      it is important to make absolute paths, so we add drive and
      current path if necessary */

   BOOL bAddDrive = TRUE;
   BOOL bAddCurrentPath = TRUE;
   BOOL bAddPrefix = TRUE;

   /* does path begin with drive? if yes, it is absolute */
   if (iswalpha(name[0]) && name[1] == ':' && IsPathSeparator(name[2])) {
      bAddDrive = FALSE;
      bAddCurrentPath = FALSE;
   }

   /* is path absolute? */
   if (IsPathSeparator(name[0]))
      bAddCurrentPath = FALSE; 

   /* is path relative to itself?, if yes, skip ./ */
   if (name[0] == '.' && IsPathSeparator(name[1])) {
      name += 2;
   }

   /* is path of form '//./'? */
   if (IsPathSeparator(name[0]) && 
       IsPathSeparator(name[1]) && 
       name[2] == '.' && 
       IsPathSeparator(name[3])) {
      bAddDrive = FALSE;
      bAddCurrentPath = FALSE;
      bAddPrefix = FALSE;
      if (pBIsRawPath) {
         *pBIsRawPath = TRUE;
      }
   }

   int nParseOffset = 0;
   
   /* add 4 bytes header */
   if (bAddPrefix) {
      nParseOffset = 4;
      wcscpy(pwszBuf, L"\\\\?\\");
   }

   /* get current path if needed */
   if (bAddDrive || bAddCurrentPath) {
      dwCurDirPathSize = p_GetCurrentDirectoryW(0, NULL);
      if (dwCurDirPathSize > 0) {
         /* get directory into own buffer as it may either return c:\... or \\?\C:\.... */         
         pwszCurDirBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pwszCurDirBuf, (dwCurDirPathSize+1)*sizeof(wchar_t));
         p_GetCurrentDirectoryW(dwCurDirPathSize, pwszCurDirBuf);
      } else {
         /* we have no info for doing so */
         bAddDrive = FALSE;
         bAddCurrentPath = FALSE;
      }
   }
      

   /* add drive if needed */
   if (bAddDrive && !bAddCurrentPath) {
      wchar_t szDrive[3];

      if (IsPathSeparator(pwszCurDirBuf[0]) && 
          IsPathSeparator(pwszCurDirBuf[1]) && 
          pwszCurDirBuf[2] == '?' && 
          IsPathSeparator(pwszCurDirBuf[3])) {
         /* copy drive character */
         szDrive[0] = pwszCurDirBuf[4];
      } else {
         /* copy drive character */
         szDrive[0] = pwszCurDirBuf[0];
      }

      szDrive[1] = ':';
      szDrive[2] = 0;

      wcscat(pwszBuf, szDrive);
      nParseOffset +=2;
   }

   /* add path if needed */
   if (bAddCurrentPath) {
      /* the 1 add. character is for the eventually added backslash */
      dwBufCharsNeeded += dwCurDirPathSize+1; 
      pwszBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pwszBuf, dwBufCharsNeeded*sizeof(wchar_t));
      /* get directory into own buffer as it may either return c:\... or \\?\C:\.... */
      
      if (IsPathSeparator(pwszCurDirBuf[0]) && 
          IsPathSeparator(pwszCurDirBuf[1]) && 
          pwszCurDirBuf[2] == '?' && 
          IsPathSeparator(pwszCurDirBuf[3])) {
         /* copy complete string */
         wcscpy(pwszBuf, pwszCurDirBuf);
      } else {
         /* append path  */
         wcscat(pwszBuf, pwszCurDirBuf);
      }

      nParseOffset = wcslen((LPCWSTR) pwszBuf);

      /* check if path ends with backslash, if not, add one */
      if (!IsPathSeparator(pwszBuf[nParseOffset-1])) {
         wcscat(pwszBuf, L"\\");
         nParseOffset++;
      }
   }

   wchar_t *win32_name = &pwszBuf[nParseOffset];
   wchar_t *name_start = name;

   while (*name) {
      /* Check for Unix separator and convert to Win32, eliminating 
       * duplicate separators.
       */
      if (IsPathSeparator(*name)) {
         *win32_name++ = '\\';     /* convert char */

         /* Eliminate consecutive slashes, but not at the start so that 
          * \\.\ still works.
          */
         if (name_start != name && IsPathSeparator(name[1])) {
            name++;
         }
      } else {
         *win32_name++ = *name;    /* copy character */
      }
      name++;
   }

   /* null terminate string */
   *win32_name = 0;

   /* here we convert to VSS specific file name which
    * can get longer because VSS will make something like
    * \\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1\\bacula\\uninstall.exe
    * from c:\bacula\uninstall.exe
   */ 
   if (g_pVSSPathConvertW != NULL) {
      /* is output buffer large enough? */
      pwszBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pwszBuf, 
                                                  (dwBufCharsNeeded+MAX_PATH)*sizeof(wchar_t));
      /* create temp. buffer */
      wchar_t *pszBuf = (wchar_t *)get_pool_memory(PM_FNAME);
      pszBuf = (wchar_t *)check_pool_memory_size((POOLMEM *)pszBuf, 
                                                 (dwBufCharsNeeded+MAX_PATH)*sizeof(wchar_t));
      if (bAddPrefix)
         nParseOffset = 4;
      else
         nParseOffset = 0; 
      wcsncpy(pszBuf, &pwszBuf[nParseOffset], wcslen(pwszBuf)+1-nParseOffset);
      g_pVSSPathConvertW(pszBuf, pwszBuf, dwBufCharsNeeded+MAX_PATH);
      free_pool_memory((POOLMEM *)pszBuf);
   }   

   free_pool_memory(pszUCSPath);
   free_pool_memory((POOLMEM *)pwszCurDirBuf);

   Dmsg1(dbglvl, "Leave wchar_win32_path=%s\n", pwszBuf);
   return (POOLMEM *)pwszBuf;
}

/*
 * Convert from WCHAR (UCS) to UTF-8
 */
int
wchar_2_UTF8(POOLMEM **pszUTF, const wchar_t *pszUCS)
{
   /**
    * The return value is the number of bytes written to the buffer.
    * The number includes the byte for the null terminator.
    */

   if (p_WideCharToMultiByte) {
      int nRet = p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,NULL,0,NULL,NULL);
      *pszUTF = check_pool_memory_size(*pszUTF, nRet);
      return p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,*pszUTF,nRet,NULL,NULL);

   } else {
      return 0;
   }
}

/*
 * Convert from WCHAR (UCS) to UTF-8
 */
int
wchar_2_UTF8(char *pszUTF, const wchar_t *pszUCS, int cchChar)
{
   /**
    * The return value is the number of bytes written to the buffer.
    * The number includes the byte for the null terminator.
    */

   if (p_WideCharToMultiByte) {
      int nRet = p_WideCharToMultiByte(CP_UTF8,0,pszUCS,-1,pszUTF,cchChar,NULL,NULL);
      ASSERT (nRet > 0);
      return nRet;
   } else {
      return 0;
   }
}

int
UTF8_2_wchar(POOLMEM **ppszUCS, const char *pszUTF)
{
   /* the return value is the number of wide characters written to the buffer. */
   /* convert null terminated string from utf-8 to ucs2, enlarge buffer if necessary */

   if (p_MultiByteToWideChar) {
      /* strlen of UTF8 +1 is enough */
      DWORD cchSize = (strlen(pszUTF)+1);
      *ppszUCS = check_pool_memory_size(*ppszUCS, cchSize*sizeof (wchar_t));

      int nRet = p_MultiByteToWideChar(CP_UTF8, 0, pszUTF, -1, (LPWSTR) *ppszUCS,cchSize);
      ASSERT (nRet > 0);
      return nRet;
   } else {
      return 0;
   }
}


void
wchar_win32_path(const char *name, wchar_t *win32_name)
{
    const char *fname = name;
    while (*name) {
        /* Check for Unix separator and convert to Win32 */
        if (*name == '/') {
            *win32_name++ = '\\';     /* convert char */
        /* If Win32 separated that is "quoted", remove quote */
        } else if (*name == '\\' && name[1] == '\\') {
            *win32_name++ = '\\';
            name++;                   /* skip first \ */
        } else {
            *win32_name++ = *name;    /* copy character */
        }
        name++;
    }
    /* Strip any trailing slash, if we stored something */
    if (*fname != 0 && win32_name[-1] == '\\') {
        win32_name[-1] = 0;
    } else {
        *win32_name = 0;
    }
}

int 
make_win32_path_UTF8_2_wchar(POOLMEM **pszUCS, const char *pszUTF, BOOL* pBIsRawPath /*= NULL*/)
{
   P(Win32Convmutex);
   /* if we find the utf8 string in cache, we use the cached ucs2 version.
      we compare the stringlength first (quick check) and then compare the content.            
   */
   if (!g_pWin32ConvUTF8Cache) {
      Win32ConvInitCache();
   } else if (g_dwWin32ConvUTF8strlen == strlen(pszUTF)) {
      if (bstrcmp(pszUTF, g_pWin32ConvUTF8Cache)) {
         /* Return cached value */
         int32_t nBufSize = sizeof_pool_memory(g_pWin32ConvUCS2Cache);
         *pszUCS = check_pool_memory_size(*pszUCS, nBufSize);      
         wcscpy((LPWSTR) *pszUCS, (LPWSTR)g_pWin32ConvUCS2Cache);
         V(Win32Convmutex);
         return nBufSize / sizeof (WCHAR);
      }
   }

   /* helper to convert from utf-8 to UCS-2 and to complete a path for 32K path syntax */
   int nRet = UTF8_2_wchar(pszUCS, pszUTF);

#ifdef USE_WIN32_32KPATHCONVERSION
   /* add \\?\ to support 32K long filepaths */
   *pszUCS = make_wchar_win32_path(*pszUCS, pBIsRawPath);
#else
   if (pBIsRawPath)
      *pBIsRawPath = FALSE;
#endif

   /* populate cache */      
   g_pWin32ConvUCS2Cache = check_pool_memory_size(g_pWin32ConvUCS2Cache, sizeof_pool_memory(*pszUCS));
   wcscpy((LPWSTR) g_pWin32ConvUCS2Cache, (LPWSTR) *pszUCS);
   
   g_dwWin32ConvUTF8strlen = strlen(pszUTF);
   g_pWin32ConvUTF8Cache = check_pool_memory_size(g_pWin32ConvUTF8Cache, g_dwWin32ConvUTF8strlen+2);
   bstrncpy(g_pWin32ConvUTF8Cache, pszUTF, g_dwWin32ConvUTF8strlen+1);
   V(Win32Convmutex);

   return nRet;
}

#if !defined(_MSC_VER) || (_MSC_VER < 1400) // VC8+
int umask(int)
{
   return 0;
}
#endif

#ifndef LOAD_WITH_ALTERED_SEARCH_PATH
#define LOAD_WITH_ALTERED_SEARCH_PATH 0x00000008
#endif

void *dlopen(const char *file, int mode)
{
   void *handle;

   handle = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
   return handle;
}

void *dlsym(void *handle, const char *name)
{
   void *symaddr;
   symaddr = (void *)GetProcAddress((HMODULE)handle, name);
   return symaddr;
}

int dlclose(void *handle) 
{
   if (handle && !FreeLibrary((HMODULE)handle)) {
      errno = b_errno_win32;
      return 1;        /* failed */
   }
   return 0;           /* OK */
}

char *dlerror(void) 
{
   static char buf[200];
   const char *err = errorString();
   bstrncpy(buf, (char *)err, sizeof(buf));
   LocalFree((void *)err);
   return buf;
}

int fcntl(int fd, int cmd)
{
   return 0;
}

int chown(const char *k, uid_t, gid_t)
{
   return 0;
}

int lchown(const char *k, uid_t, gid_t)
{
   return 0;
}

long int
random(void)
{
    return rand();
}

void
srandom(unsigned int seed)
{
   srand(seed);
}
// /////////////////////////////////////////////////////////////////
// convert from Windows concept of time to Unix concept of time
// /////////////////////////////////////////////////////////////////
void
cvt_utime_to_ftime(const time_t  &time, FILETIME &wintime)
{
   uint64_t mstime = time;
   mstime *= WIN32_FILETIME_SCALE;
   mstime += WIN32_FILETIME_ADJUST;

#if defined(_MSC_VER)
   wintime.dwLowDateTime = (DWORD)(mstime & 0xffffffffI64);
#else
   wintime.dwLowDateTime = (DWORD)(mstime & 0xffffffffUL);
#endif
   wintime.dwHighDateTime = (DWORD) ((mstime>>32)& 0xffffffffUL);
}

time_t
cvt_ftime_to_utime(const FILETIME &time)
{
    uint64_t mstime = time.dwHighDateTime;
    mstime <<= 32;
    mstime |= time.dwLowDateTime;

    mstime -= WIN32_FILETIME_ADJUST;
    mstime /= WIN32_FILETIME_SCALE; // convert to seconds.

    return (time_t) (mstime & 0xffffffff);
}

static const char *errorString(void)
{
   LPVOID lpMsgBuf;

   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 GetLastError(),
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default lang
                 (LPTSTR) &lpMsgBuf,
                 0,
                 NULL);

   /* Strip any \r or \n */
   char *rval = (char *) lpMsgBuf;
   char *cp = strchr(rval, '\r');
   if (cp != NULL) {
      *cp = 0;
   } else {
      cp = strchr(rval, '\n');
      if (cp != NULL)
         *cp = 0;
   }
   return rval;
}


/*
 * This is only called for directories, and is used to get the directory
 *  attributes and find out if we have a junction point or a mount point
 *  or other kind of "funny" directory.
 */
static int
statDir(const char *file, struct stat *sb)
{
   WIN32_FIND_DATAW info_w;       // window's file info
   WIN32_FIND_DATAA info_a;       // window's file info

   // cache some common vars to make code more transparent
   DWORD *pdwFileAttributes;
   DWORD *pnFileSizeHigh;
   DWORD *pnFileSizeLow;
   DWORD *pdwReserved0;
   FILETIME *pftLastAccessTime;
   FILETIME *pftLastWriteTime;
   FILETIME *pftCreationTime;
   HANDLE h = INVALID_HANDLE_VALUE;

   /* 
    * Oh, cool, another exception: Microsoft doesn't let us do 
    *  FindFile operations on a Drive, so simply fake root attibutes.
    */
   if (file[1] == ':' && file[2] == 0) {
      time_t now = time(NULL);
      Dmsg1(dbglvl, "faking ROOT attrs(%s).\n", file);
      sb->st_mode = S_IFDIR;
      sb->st_mode |= S_IREAD|S_IEXEC|S_IWRITE;
      sb->st_ctime = now;
      sb->st_mtime = now;
      sb->st_atime = now;
      sb->st_rdev = 0;
      return 0;
    }


   // use unicode
   if (p_FindFirstFileW) {
      POOLMEM* pwszBuf = get_pool_memory (PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, file);

      Dmsg1(dbglvl, "FindFirstFileW=%s\n", file);
      h = p_FindFirstFileW((LPCWSTR)pwszBuf, &info_w);
      free_pool_memory(pwszBuf);

      pdwFileAttributes = &info_w.dwFileAttributes;
      pdwReserved0      = &info_w.dwReserved0;
      pnFileSizeHigh    = &info_w.nFileSizeHigh;
      pnFileSizeLow     = &info_w.nFileSizeLow;
      pftLastAccessTime = &info_w.ftLastAccessTime;
      pftLastWriteTime  = &info_w.ftLastWriteTime;
      pftCreationTime   = &info_w.ftCreationTime;

   // use ASCII
   } else if (p_FindFirstFileA) {
      Dmsg1(dbglvl, "FindFirstFileA=%s\n", file);
      h = p_FindFirstFileA(file, &info_a);

      pdwFileAttributes = &info_a.dwFileAttributes;
      pdwReserved0      = &info_a.dwReserved0;
      pnFileSizeHigh    = &info_a.nFileSizeHigh;
      pnFileSizeLow     = &info_a.nFileSizeLow;
      pftLastAccessTime = &info_a.ftLastAccessTime;
      pftLastWriteTime  = &info_a.ftLastWriteTime;
      pftCreationTime   = &info_a.ftCreationTime;
   } else {
      Dmsg0(dbglvl, "No findFirstFile A or W found\n");
   }

   if (h == INVALID_HANDLE_VALUE) {
      const char *err = errorString();
      /*
       * Note, in creating leading paths, it is normal that
       * the file does not exist.
       */
      Dmsg2(2099, "FindFirstFile(%s):%s\n", file, err);
      LocalFree((void *)err);
      errno = b_errno_win32;
      return -1;
   } else {
      FindClose(h);
   }

   sb->st_mode = 0777;               /* start with everything */
   if (*pdwFileAttributes & FILE_ATTRIBUTE_READONLY)
       sb->st_mode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
   if (*pdwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
       sb->st_mode &= ~S_IRWXO; /* remove everything for other */
   if (*pdwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
       sb->st_mode |= S_ISVTX; /* use sticky bit -> hidden */
   sb->st_mode |= S_IFDIR;

   /* 
    * Store reparse/mount point info in st_rdev.  Note a
    *  Win32 reparse point (junction point) is like a link
    *  though it can have many properties (directory link,
    *  soft link, hard link, HSM, ...
    *  A mount point is a reparse point where another volume
    *  is mounted, so it is like a Unix mount point (change of
    *  filesystem).
    */
   if (*pdwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      sb->st_rdev = WIN32_MOUNT_POINT;
   } else {
      sb->st_rdev = 0;
   }
   if ((*pdwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
        (*pdwReserved0 & IO_REPARSE_TAG_MOUNT_POINT)) {
      sb->st_rdev = WIN32_MOUNT_POINT;           /* mount point */
      /* 
       * Now to find out if the directory is a mount point or
       * a reparse point, we must do a song and a dance.
       * Explicitly open the file to read the reparse point, then
       * call DeviceIoControl to find out if it points to a Volume
       * or to a directory.
       */
      h = INVALID_HANDLE_VALUE;
      if (p_GetFileAttributesW) {
         POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
         make_win32_path_UTF8_2_wchar(&pwszBuf, file);
         if (p_CreateFileW) {
            h = CreateFileW((LPCWSTR)pwszBuf, GENERIC_READ,
                   FILE_SHARE_READ, NULL, OPEN_EXISTING, 
                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                   NULL);
         }
         free_pool_memory(pwszBuf);
      } else if (p_GetFileAttributesA) {
         h = CreateFileA(file, GENERIC_READ,
                FILE_SHARE_READ, NULL, OPEN_EXISTING, 
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                NULL);         
      }
      if (h != INVALID_HANDLE_VALUE) {
         char dummy[1000];
         REPARSE_DATA_BUFFER *rdb = (REPARSE_DATA_BUFFER *)dummy;
         rdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
         DWORD bytes;
         bool ok;
         ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, 
                 NULL, 0,                           /* in buffer, bytes */
                 (LPVOID)rdb, (DWORD)sizeof(dummy), /* out buffer, btyes */
                 (LPDWORD)&bytes, (LPOVERLAPPED)0);
         if (ok) {
            POOLMEM *utf8 = get_pool_memory(PM_NAME);
            wchar_2_UTF8(&utf8, (wchar_t *)rdb->SymbolicLinkReparseBuffer.PathBuffer);
            Dmsg2(dbglvl, "Junction %s points to: %s\n", file, utf8);
            if (strncasecmp(utf8, "\\??\\volume{", 11) == 0) {
               sb->st_rdev = WIN32_MOUNT_POINT;
            } else { 
               /* It points to a directory so we ignore it. */
               sb->st_rdev = WIN32_JUNCTION_POINT;
            }
            free_pool_memory(utf8);
         }
         CloseHandle(h);
      } else {
         Dmsg1(dbglvl, "Invalid handle from CreateFile(%s)\n", file);
      }
   }  
   Dmsg2(dbglvl, "st_rdev=%d file=%s\n", sb->st_rdev, file);
   sb->st_size = *pnFileSizeHigh;
   sb->st_size <<= 32;
   sb->st_size |= *pnFileSizeLow;
   sb->st_blksize = 4096;
   sb->st_blocks = (uint32_t)(sb->st_size + 4095)/4096;

   sb->st_atime = cvt_ftime_to_utime(*pftLastAccessTime);
   sb->st_mtime = cvt_ftime_to_utime(*pftLastWriteTime);
   sb->st_ctime = cvt_ftime_to_utime(*pftCreationTime);

   return 0;
}

int
fstat(intptr_t fd, struct stat *sb)
{
   BY_HANDLE_FILE_INFORMATION info;

   if (!GetFileInformationByHandle((HANDLE)_get_osfhandle(fd), &info)) {
       const char *err = errorString();
       Dmsg1(2099, "GetfileInformationByHandle: %s\n", err);
       LocalFree((void *)err);
       errno = b_errno_win32;
       return -1;
   }

   sb->st_dev = info.dwVolumeSerialNumber;
   sb->st_ino = info.nFileIndexHigh;
   sb->st_ino <<= 32;
   sb->st_ino |= info.nFileIndexLow;
   sb->st_nlink = (short)info.nNumberOfLinks;
   if (sb->st_nlink > 1) {
      Dmsg1(dbglvl,  "st_nlink=%d\n", sb->st_nlink);
   }

   sb->st_mode = 0777;               /* start with everything */
   if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
       sb->st_mode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
   if (info.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
       sb->st_mode &= ~S_IRWXO; /* remove everything for other */
   if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
       sb->st_mode |= S_ISVTX; /* use sticky bit -> hidden */
   sb->st_mode |= S_IFREG;

   /* Use st_rdev to store reparse attribute */
   if  (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      sb->st_rdev = WIN32_REPARSE_POINT;
   }
   Dmsg3(dbglvl, "st_rdev=%d sizino=%d ino=%lld\n", sb->st_rdev, sizeof(sb->st_ino),
      (long long)sb->st_ino);

   sb->st_size = info.nFileSizeHigh;
   sb->st_size <<= 32;
   sb->st_size |= info.nFileSizeLow;
   sb->st_blksize = 4096;
   sb->st_blocks = (uint32_t)(sb->st_size + 4095)/4096;
   sb->st_atime = cvt_ftime_to_utime(info.ftLastAccessTime);
   sb->st_mtime = cvt_ftime_to_utime(info.ftLastWriteTime);
   sb->st_ctime = cvt_ftime_to_utime(info.ftCreationTime);

   return 0;
}

static int
stat2(const char *file, struct stat *sb)
{
   HANDLE h = INVALID_HANDLE_VALUE;
   int rval = 0;
   char tmpbuf[5000];
   conv_unix_to_vss_win32_path(file, tmpbuf, 5000);

   DWORD attr = (DWORD)-1;

   if (p_GetFileAttributesW) {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, tmpbuf);

      attr = p_GetFileAttributesW((LPCWSTR) pwszBuf);
      if (p_CreateFileW) {
         h = CreateFileW((LPCWSTR)pwszBuf, GENERIC_READ,
                FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
      }
      free_pool_memory(pwszBuf);
   } else if (p_GetFileAttributesA) {
      attr = p_GetFileAttributesA(tmpbuf);
      h = CreateFileA(tmpbuf, GENERIC_READ,
               FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
   }

   if (attr == (DWORD)-1) {
      const char *err = errorString();
      Dmsg2(2099, "GetFileAttributes(%s): %s\n", tmpbuf, err);
      LocalFree((void *)err);
      if (h != INVALID_HANDLE_VALUE) {
         CloseHandle(h);
      }
      errno = b_errno_win32;
      return -1;
   }

   if (h == INVALID_HANDLE_VALUE) {
      const char *err = errorString();
      Dmsg2(2099, "Cannot open file for stat (%s):%s\n", tmpbuf, err);
      LocalFree((void *)err);
      errno = b_errno_win32;
      return -1;
   }

   rval = fstat((intptr_t)h, sb);
   CloseHandle(h);

   if (attr & FILE_ATTRIBUTE_DIRECTORY &&
        file[1] == ':' && file[2] != 0) {
      rval = statDir(file, sb);
   }
   return rval;
}

int
stat(const char *file, struct stat *sb)
{
   WIN32_FILE_ATTRIBUTE_DATA data;
   errno = 0;

   memset(sb, 0, sizeof(*sb));

   if (p_GetFileAttributesExW) {
      /* dynamically allocate enough space for UCS2 filename */
      POOLMEM *pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, file);

      BOOL b = p_GetFileAttributesExW((LPCWSTR)pwszBuf, GetFileExInfoStandard, &data);
      free_pool_memory(pwszBuf);

      if (!b) {
         return stat2(file, sb);
      }

   } else if (p_GetFileAttributesExA) {
      if (!p_GetFileAttributesExA(file, GetFileExInfoStandard, &data)) {
         return stat2(file, sb);
       }
   } else {
      return stat2(file, sb);
   }

   sb->st_mode = 0777;               /* start with everything */
   if (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
      sb->st_mode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
   }
   if (data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
      sb->st_mode &= ~S_IRWXO; /* remove everything for other */
   }
   if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
      sb->st_mode |= S_ISVTX; /* use sticky bit -> hidden */
   }
   if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      sb->st_mode |= S_IFDIR;
   } else {
      sb->st_mode |= S_IFREG;
   }

   /* Use st_rdev to store reparse attribute */
   sb->st_rdev = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; 

   sb->st_nlink = 1;
   sb->st_size = data.nFileSizeHigh;
   sb->st_size <<= 32;
   sb->st_size |= data.nFileSizeLow;
   sb->st_blksize = 4096;
   sb->st_blocks = (uint32_t)(sb->st_size + 4095)/4096;
   sb->st_atime = cvt_ftime_to_utime(data.ftLastAccessTime);
   sb->st_mtime = cvt_ftime_to_utime(data.ftLastWriteTime);
   sb->st_ctime = cvt_ftime_to_utime(data.ftCreationTime);

   /*
    * If we are not at the root, then to distinguish a reparse 
    *  point from a mount point, we must call FindFirstFile() to
    *  get the WIN32_FIND_DATA, which has the bit that indicates
    *  that this directory is a mount point -- aren't Win32 APIs
    *  wonderful? (sarcasm).  The code exists in the statDir
    *  subroutine.
    */
   if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && 
        file[1] == ':' && file[2] != 0) {
      statDir(file, sb);
   }
   Dmsg3(dbglvl, "sizino=%d ino=%lld file=%s\n", sizeof(sb->st_ino),
                       (long long)sb->st_ino, file);
   return 0;
}

/*
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
      errno = EACCES;         /* truncation failed, get out */
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

int fcntl(int fd, int cmd, long arg)
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

int
lstat(const char *file, struct stat *sb)
{
   return stat(file, sb);
}

void
sleep(int sec)
{
   Sleep(sec * 1000);
}

int
geteuid(void)
{
   return 0;
}

int
execvp(const char *, char *[]) {
   errno = ENOSYS;
   return -1;
}


int
fork(void)
{
   errno = ENOSYS;
   return -1;
}

int
pipe(int[])
{
   errno = ENOSYS;
   return -1;
}

int
waitpid(int, int*, int)
{
   errno = ENOSYS;
   return -1;
}

int
readlink(const char *, char *, int)
{
   errno = ENOSYS;
   return -1;
}


#ifndef HAVE_MINGW
int
strcasecmp(const char *s1, const char *s2)
{
   register int ch1, ch2;

   if (s1==s2)
      return 0;       /* strings are equal if same object. */
   else if (!s1)
      return -1;
   else if (!s2)
      return 1;
   do {
      ch1 = *s1;
      ch2 = *s2;
      s1++;
      s2++;
   } while (ch1 != 0 && tolower(ch1) == tolower(ch2));

   return(ch1 - ch2);
}
#endif //HAVE_MINGW

int
strncasecmp(const char *s1, const char *s2, int len)
{
   register int ch1 = 0, ch2 = 0;

   if (s1==s2)
      return 0;       /* strings are equal if same object. */
   else if (!s1)
      return -1;
   else if (!s2)
      return 1;

   while (len--) {
      ch1 = *s1;
      ch2 = *s2;
      s1++;
      s2++;
      if (ch1 == 0 || tolower(ch1) != tolower(ch2)) break;
   }

   return (ch1 - ch2);
}

int
gettimeofday(struct timeval *tv, struct timezone *)
{
    SYSTEMTIME now;
    FILETIME tmp;

    GetSystemTime(&now);

    if (tv == NULL) {
       errno = EINVAL;
       return -1;
    }
    if (!SystemTimeToFileTime(&now, &tmp)) {
       errno = b_errno_win32;
       return -1;
    }

    int64_t _100nsec = tmp.dwHighDateTime;
    _100nsec <<= 32;
    _100nsec |= tmp.dwLowDateTime;
    _100nsec -= WIN32_FILETIME_ADJUST;

    tv->tv_sec = (long)(_100nsec / 10000000);
    tv->tv_usec = (long)((_100nsec % 10000000)/10);
    return 0;

}

/* 
 * Write in Windows System log 
 */
extern "C" void syslog(int type, const char *fmt, ...) 
{
   va_list   arg_ptr;
   int len, maxlen;
   POOLMEM *msg;

   msg = get_pool_memory(PM_EMSG);

   for (;;) {
      maxlen = sizeof_pool_memory(msg) - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         msg = realloc_pool_memory(msg, maxlen + maxlen/2);
         continue;
      }
      break;
   }
   LogErrorMsg((const char *)msg);
   free_memory(msg);
}

void
closelog()
{
}

struct passwd *
getpwuid(uid_t)
{
    return NULL;
}

struct group *
getgrgid(uid_t)
{
    return NULL;
}

// implement opendir/readdir/closedir on top of window's API

typedef struct _dir
{
    WIN32_FIND_DATAA data_a;    // window's file info (ansii version)
    WIN32_FIND_DATAW data_w;    // window's file info (wchar version)
    const char *spec;           // the directory we're traversing
    HANDLE      dirh;           // the search handle
    BOOL        valid_a;        // the info in data_a field is valid
    BOOL        valid_w;        // the info in data_w field is valid
    UINT32      offset;         // pseudo offset for d_off
} _dir;

DIR *
opendir(const char *path)
{
    /* enough space for VSS !*/
    int max_len = strlen(path) + MAX_PATH;
    char *tspec = NULL;
    _dir *rval = NULL;
    if (path == NULL) {
       errno = ENOENT;
       return NULL;
    }

    Dmsg1(dbglvl, "Opendir path=%s\n", path);
    rval = (_dir *)malloc(sizeof(_dir));
    if (!rval) {
       goto err;
    }
    memset (rval, 0, sizeof (_dir));

    tspec = (char *)malloc(max_len);
    if (!tspec) { 
       goto err;
    }

    conv_unix_to_vss_win32_path(path, tspec, max_len);
    Dmsg1(dbglvl, "win32 path=%s\n", tspec);

    // add backslash only if there is none yet (think of c:\)
    if (tspec[strlen(tspec)-1] != '\\')
      bstrncat(tspec, "\\*", max_len);
    else
      bstrncat(tspec, "*", max_len);

    rval->spec = tspec;

    // convert to wchar_t
    if (p_FindFirstFileW) {
      POOLMEM* pwcBuf = get_pool_memory(PM_FNAME);;
      make_win32_path_UTF8_2_wchar(&pwcBuf, rval->spec);

      rval->dirh = p_FindFirstFileW((LPCWSTR)pwcBuf, &rval->data_w);

      free_pool_memory(pwcBuf);

      if (rval->dirh != INVALID_HANDLE_VALUE)
        rval->valid_w = 1;
    } else if (p_FindFirstFileA) {
      rval->dirh = p_FindFirstFileA(rval->spec, &rval->data_a);

      if (rval->dirh != INVALID_HANDLE_VALUE)
        rval->valid_a = 1;
    } else goto err;


    Dmsg3(dbglvl, "opendir(%s)\n\tspec=%s,\n\tFindFirstFile returns %d\n",
          path, rval->spec, rval->dirh);

    rval->offset = 0;
    if (rval->dirh == INVALID_HANDLE_VALUE)
        goto err;

    if (rval->valid_w) {
      Dmsg1(dbglvl, "\tFirstFile=%s\n", rval->data_w.cFileName);
    }

    if (rval->valid_a) {
      Dmsg1(dbglvl, "\tFirstFile=%s\n", rval->data_a.cFileName);
    }

    return (DIR *)rval;

err:
    if (rval) {
       free(rval);
    }
    if (tspec) {
       free(tspec);
    }
    errno = b_errno_win32;
    return NULL;
}

int
closedir(DIR *dirp)
{
    _dir *dp = (_dir *)dirp;
    FindClose(dp->dirh);
    free((void *)dp->spec);
    free((void *)dp);
    return 0;
}

/*
  typedef struct _WIN32_FIND_DATA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    TCHAR cFileName[MAX_PATH];
    TCHAR cAlternateFileName[14];
} WIN32_FIND_DATA, *PWIN32_FIND_DATA;
*/

static int
copyin(struct dirent &dp, const char *fname)
{
    dp.d_ino = 0;
    dp.d_reclen = 0;
    char *cp = dp.d_name;
    while (*fname) {
        *cp++ = *fname++;
        dp.d_reclen++;
    }
        *cp = 0;
    return dp.d_reclen;
}

int
readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    _dir *dp = (_dir *)dirp;
    if (dp->valid_w || dp->valid_a) {
      entry->d_off = dp->offset;

      // copy unicode
      if (dp->valid_w) {
         POOLMEM *szBuf = get_pool_memory(PM_NAME);
         wchar_2_UTF8(&szBuf, dp->data_w.cFileName);
         dp->offset += copyin(*entry, szBuf);
         free_pool_memory(szBuf);
      } else if (dp->valid_a) { // copy ansi (only 1 will be valid)
         dp->offset += copyin(*entry, dp->data_a.cFileName);
      }

      *result = entry;              /* return entry address */
      Dmsg4(dbglvl, "readdir_r(%p, { d_name=\"%s\", d_reclen=%d, d_off=%d\n",
            dirp, entry->d_name, entry->d_reclen, entry->d_off);
    } else {
//      Dmsg0(dbglvl, "readdir_r !valid\n");
        errno = b_errno_win32;
        return -1;
    }

    // get next file, try unicode first
    if (p_FindNextFileW)
       dp->valid_w = p_FindNextFileW(dp->dirh, &dp->data_w);
    else if (p_FindNextFileA)
       dp->valid_a = p_FindNextFileA(dp->dirh, &dp->data_a);
    else {
       dp->valid_a = FALSE;
       dp->valid_w = FALSE;
    }

    return 0;
}

/*
 * Dotted IP address to network address
 *
 * Returns 1 if  OK
 *         0 on error
 */
int
inet_aton(const char *a, struct in_addr *inp)
{
   const char *cp = a;
   uint32_t acc = 0, tmp = 0;
   int dotc = 0;

   if (!isdigit(*cp)) {         /* first char must be digit */
      return 0;                 /* error */
   }
   do {
      if (isdigit(*cp)) {
         tmp = (tmp * 10) + (*cp -'0');
      } else if (*cp == '.' || *cp == 0) {
         if (tmp > 255) {
            return 0;           /* error */
         }
         acc = (acc << 8) + tmp;
         dotc++;
         tmp = 0;
      } else {
         return 0;              /* error */
      }
   } while (*cp++ != 0);
   if (dotc != 4) {              /* want 3 .'s plus EOS */
      return 0;                  /* error */
   }
   inp->s_addr = htonl(acc);     /* store addr in network format */
   return 1;
}

int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (rem)
        rem->tv_sec = rem->tv_nsec = 0;
    Sleep((req->tv_sec * 1000) + (req->tv_nsec/1000000));
    return 0;
}

void
init_signals(void terminate(int sig))
{

}

void
init_stack_dump(void)
{

}


long
pathconf(const char *path, int name)
{
    switch(name) {
    case _PC_PATH_MAX :
        if (strncmp(path, "\\\\?\\", 4) == 0)
            return 32767;
    case _PC_NAME_MAX :
        return 255;
    }
    errno = ENOSYS;
    return -1;
}

int
WSA_Init(void)
{
    WORD wVersionRequested = MAKEWORD( 1, 1);
    WSADATA wsaData;

    int err = WSAStartup(wVersionRequested, &wsaData);


    if (err != 0) {
        printf("Can not start Windows Sockets\n");
        errno = ENOSYS;
        return -1;
    }

    return 0;
}

static DWORD fill_attribute(DWORD attr, mode_t mode)
{
   Dmsg1(dbglvl, "  before attr=%lld\n", (uint64_t) attr);
   /* Use Bacula mappings define in stat() above */
   if (mode & (S_IRUSR|S_IRGRP|S_IROTH)) { // If file is readable
      attr &= ~FILE_ATTRIBUTE_READONLY;    // then this is not READONLY
   } else {
      attr |= FILE_ATTRIBUTE_READONLY;
   }
   if (mode & S_ISVTX) {                   // The sticky bit <=> HIDDEN 
      attr |= FILE_ATTRIBUTE_HIDDEN;
   } else {
      attr &= ~FILE_ATTRIBUTE_HIDDEN;
   }
   if (mode & S_IRWXO) {              // Other can read/write/execute ?
      attr &= ~FILE_ATTRIBUTE_SYSTEM; // => Not system
   } else {
      attr |= FILE_ATTRIBUTE_SYSTEM;
   }
   Dmsg1(dbglvl, "  after attr=%lld\n", (uint64_t)attr);
   return attr;
}

int win32_chmod(const char *path, mode_t mode)
{
   bool ret=false;
   DWORD attr;

   Dmsg2(dbglvl, "win32_chmod(path=%s mode=%lld)\n", path, (uint64_t)mode);
   if (p_GetFileAttributesW) {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, path);

      attr = p_GetFileAttributesW((LPCWSTR) pwszBuf);
      if (attr != INVALID_FILE_ATTRIBUTES) {
         /* Use Bacula mappings define in stat() above */
         attr = fill_attribute(attr, mode);
         ret = p_SetFileAttributesW((LPCWSTR)pwszBuf, attr);
      }
      free_pool_memory(pwszBuf);
      Dmsg0(dbglvl, "Leave win32_chmod. AttributesW\n");
   } else if (p_GetFileAttributesA) {
      attr = p_GetFileAttributesA(path);
      if (attr != INVALID_FILE_ATTRIBUTES) {
         attr = fill_attribute(attr, mode);
         ret = p_SetFileAttributesA(path, attr);
      }
      Dmsg0(dbglvl, "Leave win32_chmod did AttributesA\n");
   } else {
      Dmsg0(dbglvl, "Leave win32_chmod did nothing\n");
   }
    
   if (!ret) {
      const char *err = errorString();
      Dmsg2(dbglvl, "Get/SetFileAttributes(%s): %s\n", path, err);
      LocalFree((void *)err);
      errno = b_errno_win32;
      return -1;
   }
   return 0;
}


int
win32_chdir(const char *dir)
{
   if (p_SetCurrentDirectoryW) {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, dir);

      BOOL b=p_SetCurrentDirectoryW((LPCWSTR)pwszBuf);
      
      free_pool_memory(pwszBuf);

      if (!b) {
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

int
win32_mkdir(const char *dir)
{
   Dmsg1(dbglvl, "enter win32_mkdir. dir=%s\n", dir);
   if (p_wmkdir){
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, dir);

      int n = p_wmkdir((LPCWSTR)pwszBuf);
      free_pool_memory(pwszBuf);
      Dmsg0(dbglvl, "Leave win32_mkdir did wmkdir\n");
      return n;
   }

   Dmsg0(dbglvl, "Leave win32_mkdir did _mkdir\n");
   return _mkdir(dir);
}


char *
win32_getcwd(char *buf, int maxlen)
{
   int n=0;

   if (p_GetCurrentDirectoryW) {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      pwszBuf = check_pool_memory_size (pwszBuf, maxlen*sizeof(wchar_t));

      n = p_GetCurrentDirectoryW(maxlen, (LPWSTR) pwszBuf);
      if (n!=0)
         n = wchar_2_UTF8 (buf, (wchar_t *)pwszBuf, maxlen)-1;
      free_pool_memory(pwszBuf);

   } else if (p_GetCurrentDirectoryA)
      n = p_GetCurrentDirectoryA(maxlen, buf);

   if (n <= 0 || n > maxlen) return NULL;

   if (n+1 > maxlen) return NULL;
   if (n != 3) {
       buf[n] = '\\';
       buf[n+1] = 0;
   }
   return buf;
}

int
win32_fputs(const char *string, FILE *stream)
{
   /* we use WriteConsoleA / WriteConsoleA
      so we can be sure that unicode support works on win32.
      with fallback if something fails
   */

   HANDLE hOut = GetStdHandle (STD_OUTPUT_HANDLE);
   if (hOut && (hOut != INVALID_HANDLE_VALUE) && p_WideCharToMultiByte &&
       p_MultiByteToWideChar && (stream == stdout)) {

      POOLMEM* pwszBuf = get_pool_memory(PM_MESSAGE);

      DWORD dwCharsWritten;
      DWORD dwChars;

      dwChars = UTF8_2_wchar(&pwszBuf, string);

      /* try WriteConsoleW */
      if (WriteConsoleW (hOut, pwszBuf, dwChars-1, &dwCharsWritten, NULL)) {
         free_pool_memory(pwszBuf);
         return dwCharsWritten;
      }

      /* convert to local codepage and try WriteConsoleA */
      POOLMEM* pszBuf = get_pool_memory(PM_MESSAGE);
      pszBuf = check_pool_memory_size(pszBuf, dwChars+1);

      dwChars = p_WideCharToMultiByte(GetConsoleOutputCP(),0,(LPCWSTR)pwszBuf,-1,pszBuf,dwChars,NULL,NULL);
      free_pool_memory(pwszBuf);

      if (WriteConsoleA (hOut, pszBuf, dwChars-1, &dwCharsWritten, NULL)) {
         free_pool_memory(pszBuf);
         return dwCharsWritten;
      }
      free_pool_memory(pszBuf);
   }
   /* Fall back */
   return fputs(string, stream);
}

char*
win32_cgets (char* buffer, int len)
{
   /* we use console ReadConsoleA / ReadConsoleW to be able to read unicode
      from the win32 console and fallback if seomething fails */

   HANDLE hIn = GetStdHandle (STD_INPUT_HANDLE);
   if (hIn && (hIn != INVALID_HANDLE_VALUE) && p_WideCharToMultiByte && p_MultiByteToWideChar) {
      DWORD dwRead;
      wchar_t wszBuf[1024];
      char  szBuf[1024];

      /* nt and unicode conversion */
      if (ReadConsoleW (hIn, wszBuf, 1024, &dwRead, NULL)) {

         /* null terminate at end */
         if (wszBuf[dwRead-1] == L'\n') {
            wszBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         if (wszBuf[dwRead-1] == L'\r') {
            wszBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         wchar_2_UTF8(buffer, wszBuf, len);
         return buffer;
      }

      /* win 9x and unicode conversion */
      if (ReadConsoleA (hIn, szBuf, 1024, &dwRead, NULL)) {

         /* null terminate at end */
         if (szBuf[dwRead-1] == L'\n') {
            szBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         if (szBuf[dwRead-1] == L'\r') {
            szBuf[dwRead-1] = L'\0';
            dwRead --;
         }

         /* convert from ansii to wchar_t */
         p_MultiByteToWideChar(GetConsoleCP(), 0, szBuf, -1, wszBuf,1024);
         /* convert from wchar_t to UTF-8 */
         if (wchar_2_UTF8(buffer, wszBuf, len))
            return buffer;
      }
   }

   /* fallback */
   if (fgets(buffer, len, stdin))
      return buffer;
   else
      return NULL;
}

int
win32_unlink(const char *filename)
{
   int nRetCode;
   if (p_wunlink) {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, filename);

      nRetCode = _wunlink((LPCWSTR) pwszBuf);

      /*
       * special case if file is readonly,
       * we retry but unset attribute before
       */
      if (nRetCode == -1 && errno == EACCES && p_SetFileAttributesW && p_GetFileAttributesW) {
         DWORD dwAttr =  p_GetFileAttributesW((LPCWSTR)pwszBuf);
         if (dwAttr != INVALID_FILE_ATTRIBUTES) {
            if (p_SetFileAttributesW((LPCWSTR)pwszBuf, dwAttr & ~FILE_ATTRIBUTE_READONLY)) {
               nRetCode = _wunlink((LPCWSTR) pwszBuf);
               /* reset to original if it didn't help */
               if (nRetCode == -1)
                  p_SetFileAttributesW((LPCWSTR)pwszBuf, dwAttr);
            }
         }
      }
      free_pool_memory(pwszBuf);
   } else {
      nRetCode = _unlink(filename);

      /* special case if file is readonly,
      we retry but unset attribute before */
      if (nRetCode == -1 && errno == EACCES && p_SetFileAttributesA && p_GetFileAttributesA) {
         DWORD dwAttr =  p_GetFileAttributesA(filename);
         if (dwAttr != INVALID_FILE_ATTRIBUTES) {
            if (p_SetFileAttributesA(filename, dwAttr & ~FILE_ATTRIBUTE_READONLY)) {
               nRetCode = _unlink(filename);
               /* reset to original if it didn't help */
               if (nRetCode == -1)
                  p_SetFileAttributesA(filename, dwAttr);
            }
         }
      }
   }
   return nRetCode;
}


#include "mswinver.h"

char WIN_VERSION_LONG[64];
char WIN_VERSION[32];
char WIN_RAWVERSION[32];

class winver {
public:
    winver(void);
};

static winver INIT;                     // cause constructor to be called before main()


winver::winver(void)
{
    const char *version = "";
    const char *platform = "";
    OSVERSIONINFO osvinfo;
    osvinfo.dwOSVersionInfoSize = sizeof(osvinfo);

    // Get the current OS version
    if (!GetVersionEx(&osvinfo)) {
        version = "Unknown";
        platform = "Unknown";
    }
        const int ver = _mkversion(osvinfo.dwPlatformId,
                                   osvinfo.dwMajorVersion,
                                   osvinfo.dwMinorVersion);
        snprintf(WIN_RAWVERSION, sizeof(WIN_RAWVERSION), "Windows %#08x", ver);
        switch (ver)
        {
        case MS_WINDOWS_95: (version =  "Windows 95"); break;
        case MS_WINDOWS_98: (version =  "Windows 98"); break;
        case MS_WINDOWS_ME: (version =  "Windows ME"); break;
        case MS_WINDOWS_NT4:(version =  "Windows NT 4.0"); platform = "NT"; break;
        case MS_WINDOWS_2K: (version =  "Windows 2000");platform = "NT"; break;
        case MS_WINDOWS_XP: (version =  "Windows XP");platform = "NT"; break;
        case MS_WINDOWS_S2003: (version =  "Windows Server 2003");platform = "NT"; break;
        default: version = WIN_RAWVERSION; break;
        }

    bstrncpy(WIN_VERSION_LONG, version, sizeof(WIN_VERSION_LONG));
    snprintf(WIN_VERSION, sizeof(WIN_VERSION), "%s %lu.%lu.%lu",
             platform, osvinfo.dwMajorVersion, osvinfo.dwMinorVersion, osvinfo.dwBuildNumber);

#if 0
    HANDLE h = CreateFile("G:\\foobar", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
#endif
#if 0
    BPIPE *b = open_bpipe("ls -l", 10, "r");
    char buf[1024];
    while (!feof(b->rfd)) {
        fgets(buf, sizeof(buf), b->rfd);
    }
    close_bpipe(b);
#endif
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
const char *
getArgv0(const char *cmdline)
{

    int inquote = 0;
    const char *cp;
    for (cp = cmdline; *cp; cp++)
    {
        if (*cp == '"') {
            inquote = !inquote;
        }
        if (!inquote && isspace(*cp))
            break;
    }


    int len = cp - cmdline;
    char *rval = (char *)malloc(len+1);

    cp = cmdline;
    char *rp = rval;

    while (len--)
        *rp++ = *cp++;

    *rp = 0;
    return rval;
}

/*
 * Extracts the executable or script name from the first string in 
 * cmdline.
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
bool
GetApplicationName(const char *cmdline, char **pexe, const char **pargs)
{
   const char *pExeStart = NULL;    /* Start of executable name in cmdline */
   const char *pExeEnd = NULL;      /* Character after executable name (separator) */

   const char *pBasename = NULL;    /* Character after last path separator */
   const char *pExtension = NULL;   /* Period at start of extension */

   const char *current = cmdline;

   bool bQuoted = false;

   /* Skip initial whitespace */

   while (*current == ' ' || *current == '\t')
   {
      current++;
   }

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

   /* 
    * Scan command line looking for path separators (/ and \\) and the 
    * terminator, either a quote or a blank.  The location of the 
    * extension is also noted.
    */

   for ( ; *current != '\0'; current++)
   {
      if (*current == '.') {
         pExtension = current;
      } else if (IsPathSeparator(*current) && current[1] != '\0') {
         pBasename = &current[1];
         pExtension = NULL;
      }

      /* Check for terminator, either quote or blank */
      if (bQuoted) {
         if (*current != '"') {
            continue;
         }
      } else {
         if (*current != ' ') {
            continue;
         }
      }

      /*
       * Hit terminator, remember end of name (address of terminator) and 
       * start of arguments 
       */
      pExeEnd = current;

      if (bQuoted && *current == '"') {
         *pargs = &current[1];
      } else {
         *pargs = current;
      }

      break;
   }

   if (pBasename == NULL) {
      pBasename = pExeStart;
   }

   if (pExeEnd == NULL) {
      pExeEnd = current;
   }

   if (*pargs == NULL)
   {
      *pargs = current;
   }

   bool bHasPathSeparators = pExeStart != pBasename;

   /* We have pointers to all the useful parts of the name */

   /* Default extensions in the order cmd.exe uses to search */

   static const char ExtensionList[][5] = { ".com", ".exe", ".bat", ".cmd" };
   DWORD dwBasePathLength = pExeEnd - pExeStart;

   DWORD dwAltNameLength = 0;
   char *pPathname = (char *)alloca(MAX_PATHLENGTH + 1);
   char *pAltPathname = (char *)alloca(MAX_PATHLENGTH + 1);

   pPathname[MAX_PATHLENGTH] = '\0';
   pAltPathname[MAX_PATHLENGTH] = '\0';

   memcpy(pPathname, pExeStart, dwBasePathLength);
   pPathname[dwBasePathLength] = '\0';

   if (pExtension == NULL) {
      /* Try appending extensions */
      for (int index = 0; index < (int)(sizeof(ExtensionList) / sizeof(ExtensionList[0])); index++) {

         if (!bHasPathSeparators) {
            /* There are no path separators, search in the standard locations */
            dwAltNameLength = SearchPath(NULL, pPathname, ExtensionList[index], MAX_PATHLENGTH, pAltPathname, NULL);
            if (dwAltNameLength > 0 && dwAltNameLength <= MAX_PATHLENGTH) {
               memcpy(pPathname, pAltPathname, dwAltNameLength);
               pPathname[dwAltNameLength] = '\0';
               break;
            }
         } else {
            bstrncpy(&pPathname[dwBasePathLength], ExtensionList[index], MAX_PATHLENGTH - dwBasePathLength);
            if (GetFileAttributes(pPathname) != INVALID_FILE_ATTRIBUTES) {
               break;
            }
            pPathname[dwBasePathLength] = '\0';
         }
      }
   } else if (!bHasPathSeparators) {
      /* There are no path separators, search in the standard locations */
      dwAltNameLength = SearchPath(NULL, pPathname, NULL, MAX_PATHLENGTH, pAltPathname, NULL);
      if (dwAltNameLength > 0 && dwAltNameLength < MAX_PATHLENGTH) {
         memcpy(pPathname, pAltPathname, dwAltNameLength);
         pPathname[dwAltNameLength] = '\0';
      }
   }

   if (strchr(pPathname, ' ') != NULL) {
      dwAltNameLength = GetShortPathName(pPathname, pAltPathname, MAX_PATHLENGTH);

      if (dwAltNameLength > 0 && dwAltNameLength <= MAX_PATHLENGTH) {
         *pexe = (char *)malloc(dwAltNameLength + 1);
         if (*pexe == NULL) {
            return false;
         }
         memcpy(*pexe, pAltPathname, dwAltNameLength + 1);
      }
   }

   if (*pexe == NULL) {
      DWORD dwPathnameLength = strlen(pPathname);
      *pexe = (char *)malloc(dwPathnameLength + 1);
      if (*pexe == NULL) {
         return false;
      }
      memcpy(*pexe, pPathname, dwPathnameLength + 1);
   }

   return true;
}

/**
 * Create the process with WCHAR API
 */
static BOOL
CreateChildProcessW(const char *comspec, const char *cmdLine,
                    PROCESS_INFORMATION *hProcInfo,
                    HANDLE in, HANDLE out, HANDLE err)
{
   STARTUPINFOW siStartInfo;
   BOOL bFuncRetn = FALSE;

   // Set up members of the STARTUPINFO structure.
   ZeroMemory( &siStartInfo, sizeof(siStartInfo) );
   siStartInfo.cb = sizeof(siStartInfo);
   // setup new process to use supplied handles for stdin,stdout,stderr

   siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
   siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;

   siStartInfo.hStdInput = in;
   siStartInfo.hStdOutput = out;
   siStartInfo.hStdError = err;
   
   // Convert argument to WCHAR
   POOLMEM *cmdLine_wchar = get_pool_memory(PM_FNAME);
   POOLMEM *comspec_wchar = get_pool_memory(PM_FNAME);

   UTF8_2_wchar(&cmdLine_wchar, cmdLine);
   UTF8_2_wchar(&comspec_wchar, comspec);

   // Create the child process.
   Dmsg2(dbglvl, "Calling CreateProcess(%s, %s, ...)\n", comspec_wchar, cmdLine_wchar);

   // try to execute program
   bFuncRetn = p_CreateProcessW((WCHAR*)comspec_wchar,
                                (WCHAR*)cmdLine_wchar,// command line
                                NULL,      // process security attributes
                                NULL,      // primary thread security attributes
                                TRUE,      // handles are inherited
                                0,         // creation flags
                                NULL,      // use parent's environment
                                NULL,      // use parent's current directory
                                &siStartInfo,  // STARTUPINFO pointer
                                hProcInfo);   // receives PROCESS_INFORMATION
   free_pool_memory(cmdLine_wchar);
   free_pool_memory(comspec_wchar);

   return bFuncRetn;
}


/**
 * Create the process with ANSI API
 */
static BOOL
CreateChildProcessA(const char *comspec, char *cmdLine,
                    PROCESS_INFORMATION *hProcInfo,
                    HANDLE in, HANDLE out, HANDLE err)
{
   STARTUPINFOA siStartInfo;
   BOOL bFuncRetn = FALSE;

   // Set up members of the STARTUPINFO structure.
   ZeroMemory( &siStartInfo, sizeof(siStartInfo) );
   siStartInfo.cb = sizeof(siStartInfo);
   // setup new process to use supplied handles for stdin,stdout,stderr
   siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
   siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;

   siStartInfo.hStdInput = in;
   siStartInfo.hStdOutput = out;
   siStartInfo.hStdError = err;

   // Create the child process.
   Dmsg2(dbglvl, "Calling CreateProcess(%s, %s, ...)\n", comspec, cmdLine);

   // try to execute program
   bFuncRetn = p_CreateProcessA(comspec,
                                cmdLine,  // command line
                                NULL,     // process security attributes
                                NULL,     // primary thread security attributes
                                TRUE,     // handles are inherited
                                0,        // creation flags
                                NULL,     // use parent's environment
                                NULL,     // use parent's current directory
                                &siStartInfo,// STARTUPINFO pointer
                                hProcInfo);// receives PROCESS_INFORMATION
   return bFuncRetn;
}

/**
 * OK, so it would seem CreateProcess only handles true executables:
 * .com or .exe files.  So grab $COMSPEC value and pass command line to it.
 */
HANDLE
CreateChildProcess(const char *cmdline, HANDLE in, HANDLE out, HANDLE err)
{
   static const char *comspec = NULL;
   PROCESS_INFORMATION piProcInfo;
   BOOL bFuncRetn = FALSE;

   if (!p_CreateProcessA || !p_CreateProcessW)
      return INVALID_HANDLE_VALUE;

   if (comspec == NULL) 
      comspec = getenv("COMSPEC");
   if (comspec == NULL) // should never happen
      return INVALID_HANDLE_VALUE;

   // Set up members of the PROCESS_INFORMATION structure.
   ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

   // if supplied handles are not used the send a copy of our STD_HANDLE
   // as appropriate
   if (in == INVALID_HANDLE_VALUE)
      in = GetStdHandle(STD_INPUT_HANDLE);

   if (out == INVALID_HANDLE_VALUE)
      out = GetStdHandle(STD_OUTPUT_HANDLE);

   if (err == INVALID_HANDLE_VALUE)
      err = GetStdHandle(STD_ERROR_HANDLE);

   char *exeFile;
   const char *argStart;

   if (!GetApplicationName(cmdline, &exeFile, &argStart)) {
      return INVALID_HANDLE_VALUE;
   }

   POOL_MEM cmdLine(PM_FNAME);
   Mmsg(cmdLine, "%s /c %s%s", comspec, exeFile, argStart);

   free(exeFile);

   // New function disabled
   if (p_CreateProcessW && p_MultiByteToWideChar) {
      bFuncRetn = CreateChildProcessW(comspec, cmdLine.c_str(), &piProcInfo,
                                      in, out, err);
   } else {
      bFuncRetn = CreateChildProcessA(comspec, cmdLine.c_str(), &piProcInfo,
                                      in, out, err);
   }

   if (bFuncRetn == 0) {
      ErrorExit("CreateProcess failed\n");
      const char *err = errorString();
      Dmsg3(dbglvl, "CreateProcess(%s, %s, ...)=%s\n",comspec,cmdLine.c_str(),err);
      LocalFree((void *)err);
      return INVALID_HANDLE_VALUE;
   }
   // we don't need a handle on the process primary thread so we close
   // this now.
   CloseHandle(piProcInfo.hThread);
   return piProcInfo.hProcess;
}

void
ErrorExit (LPCSTR lpszMessage)
{
    Dmsg1(0, "%s", lpszMessage);
}


/*
typedef struct s_bpipe {
   pid_t worker_pid;
   time_t worker_stime;
   int wait;
   btimer_t *timer_id;
   FILE *rfd;
   FILE *wfd;
} BPIPE;
*/

static void
CloseHandleIfValid(HANDLE handle)
{
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }
}

BPIPE *
open_bpipe(char *prog, int wait, const char *mode)
{
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup,
        hChildStdoutRd, hChildStdoutWr, hChildStdoutRdDup,
        hInputFile;

    SECURITY_ATTRIBUTES saAttr;

    BOOL fSuccess;

    hChildStdinRd = hChildStdinWr = hChildStdinWrDup =
        hChildStdoutRd = hChildStdoutWr = hChildStdoutRdDup =
        hInputFile = INVALID_HANDLE_VALUE;

    BPIPE *bpipe = (BPIPE *)malloc(sizeof(BPIPE));
    memset((void *)bpipe, 0, sizeof(BPIPE));

    int mode_read = (mode[0] == 'r');
    int mode_write = (mode[0] == 'w' || mode[1] == 'w');


    // Set the bInheritHandle flag so pipe handles are inherited.

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (mode_read) {

        // Create a pipe for the child process's STDOUT.
        if (! CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
            ErrorExit("Stdout pipe creation failed\n");
            goto cleanup;
        }
        // Create noninheritable read handle and close the inheritable read
        // handle.

        fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRd,
                                   GetCurrentProcess(), &hChildStdoutRdDup , 0,
                                   FALSE,
                                   DUPLICATE_SAME_ACCESS);
        if ( !fSuccess ) {
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
                                   GetCurrentProcess(), &hChildStdinWrDup,
                                   0,
                                   FALSE,                  // not inherited
                                   DUPLICATE_SAME_ACCESS);
        if (!fSuccess) {
            ErrorExit("DuplicateHandle failed");
            goto cleanup;
        }

        CloseHandle(hChildStdinWr);
        hChildStdinWr = INVALID_HANDLE_VALUE;
    }
    // spawn program with redirected handles as appropriate
    bpipe->worker_pid = (pid_t)
        CreateChildProcess(prog,             // commandline
                           hChildStdinRd,    // stdin HANDLE
                           hChildStdoutWr,   // stdout HANDLE
                           hChildStdoutWr);  // stderr HANDLE

    if ((HANDLE) bpipe->worker_pid == INVALID_HANDLE_VALUE)
        goto cleanup;

    bpipe->wait = wait;
    bpipe->worker_stime = time(NULL);

    if (mode_read) {
        CloseHandle(hChildStdoutWr); // close our write side so when
                                     // process terminates we can
                                     // detect eof.
        // ugly but convert WIN32 HANDLE to FILE*
        int rfd = _open_osfhandle((intptr_t)hChildStdoutRdDup, O_RDONLY | O_BINARY);
        if (rfd >= 0) {
           bpipe->rfd = _fdopen(rfd, "rb");
        }
    }
    if (mode_write) {
        CloseHandle(hChildStdinRd); // close our read side so as not
                                    // to interfre with child's copy
        // ugly but convert WIN32 HANDLE to FILE*
        int wfd = _open_osfhandle((intptr_t)hChildStdinWrDup, O_WRONLY | O_BINARY);
        if (wfd >= 0) {
           bpipe->wfd = _fdopen(wfd, "wb");
        }
    }

    if (wait > 0) {
        bpipe->timer_id = start_child_timer(NULL, bpipe->worker_pid, wait);
    }

    return bpipe;

cleanup:

    CloseHandleIfValid(hChildStdoutRd);
    CloseHandleIfValid(hChildStdoutRdDup);
    CloseHandleIfValid(hChildStdinWr);
    CloseHandleIfValid(hChildStdinWrDup);

    free((void *)bpipe);
    errno = b_errno_win32;            /* do GetLastError() for error code */
    return NULL;
}


int
kill(int pid, int signal)
{
   int rval = 0;
   if (!TerminateProcess((HANDLE)pid, (UINT) signal)) {
      rval = -1;
      errno = b_errno_win32;
   }
   CloseHandle((HANDLE)pid);
   return rval;
}


int
close_bpipe(BPIPE *bpipe)
{
   int rval = 0;
   int32_t remaining_wait = bpipe->wait;

   /* Close pipes */
   if (bpipe->rfd) {
      fclose(bpipe->rfd);
      bpipe->rfd = NULL;
   }
   if (bpipe->wfd) {
      fclose(bpipe->wfd);
      bpipe->wfd = NULL;
   }

   if (remaining_wait == 0) {         /* wait indefinitely */
      remaining_wait = INT32_MAX;
   }
   for ( ;; ) {
      DWORD exitCode;
      if (!GetExitCodeProcess((HANDLE)bpipe->worker_pid, &exitCode)) {
         const char *err = errorString();
         rval = b_errno_win32;
         Dmsg1(dbglvl, "GetExitCode error %s\n", err);
         LocalFree((void *)err);
         break;
      }
      if (exitCode == STILL_ACTIVE) {
         if (remaining_wait <= 0) {
            rval = ETIME;             /* timed out */
            break;
         }
         bmicrosleep(1, 0);           /* wait one second */
         remaining_wait--;
      } else if (exitCode != 0) {
         /* Truncate exit code as it doesn't seem to be correct */
         rval = (exitCode & 0xFF) | b_errno_exit;
         break;
      } else {
         break;                       /* Shouldn't get here */
      }
   }

   if (bpipe->timer_id) {
       stop_child_timer(bpipe->timer_id);
   }
   if (bpipe->rfd) fclose(bpipe->rfd);
   if (bpipe->wfd) fclose(bpipe->wfd);
   free((void *)bpipe);
   return rval;
}

int
close_wpipe(BPIPE *bpipe)
{
    int result = 1;

    if (bpipe->wfd) {
        fflush(bpipe->wfd);
        if (fclose(bpipe->wfd) != 0) {
            result = 0;
        }
        bpipe->wfd = NULL;
    }
    return result;
}

#ifndef MINGW64
int
utime(const char *fname, struct utimbuf *times)
{
    FILETIME acc, mod;
    char tmpbuf[5000];

    conv_unix_to_vss_win32_path(fname, tmpbuf, 5000);

    cvt_utime_to_ftime(times->actime, acc);
    cvt_utime_to_ftime(times->modtime, mod);

    HANDLE h = INVALID_HANDLE_VALUE;

    if (p_CreateFileW) {
       POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
       make_win32_path_UTF8_2_wchar(&pwszBuf, tmpbuf);

       h = p_CreateFileW((LPCWSTR)pwszBuf,
                        FILE_WRITE_ATTRIBUTES,
                        FILE_SHARE_WRITE|FILE_SHARE_READ|FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS, // required for directories
                        NULL);

       free_pool_memory(pwszBuf);
    } else if (p_CreateFileA) {
       h = p_CreateFileA(tmpbuf,
                        FILE_WRITE_ATTRIBUTES,
                        FILE_SHARE_WRITE|FILE_SHARE_READ|FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS, // required for directories
                        NULL);
    }

    if (h == INVALID_HANDLE_VALUE) {
       const char *err = errorString();
       Dmsg2(dbglvl, "Cannot open file \"%s\" for utime(): ERR=%s", tmpbuf, err);
       LocalFree((void *)err);
       errno = b_errno_win32;
       return -1;
    }

    int rval = SetFileTime(h, NULL, &acc, &mod) ? 0 : -1;
    CloseHandle(h);
    if (rval == -1) {
       errno = b_errno_win32;
    }
    return rval;
}
#endif

#if 0
int
file_open(const char *file, int flags, int mode)
{
   DWORD access = 0;
   DWORD shareMode = 0;
   DWORD create = 0;
   DWORD msflags = 0;
   HANDLE foo = INVALID_HANDLE_VALUE;
   const char *remap = file;

   if (flags & O_WRONLY) access = GENERIC_WRITE;
   else if (flags & O_RDWR) access = GENERIC_READ|GENERIC_WRITE;
   else access = GENERIC_READ;

   if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
      create = CREATE_NEW;
   else if ((flags & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC))
      create = CREATE_ALWAYS;
   else if (flags & O_CREAT)
      create = OPEN_ALWAYS;
   else if (flags & O_TRUNC)
      create = TRUNCATE_EXISTING;
   else 
      create = OPEN_EXISTING;

   shareMode = 0;

   if (flags & O_APPEND) {
      printf("open...APPEND not implemented yet.");
      exit(-1);
   }

   if (p_CreateFileW) {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(&pwszBuf, file);

      foo = p_CreateFileW((LPCWSTR) pwszBuf, access, shareMode, NULL, create, msflags, NULL);
      free_pool_memory(pwszBuf);
   } else if (p_CreateFileA)
      foo = CreateFile(file, access, shareMode, NULL, create, msflags, NULL);

   if (INVALID_HANDLE_VALUE == foo) {
      errno = b_errno_win32;
      return (int)-1;
   }
   return (int)foo;

}


int
file_close(int fd)
{
    if (!CloseHandle((HANDLE)fd)) {
        errno = b_errno_win32;
        return -1;
    }

    return 0;
}

ssize_t
file_write(int fd, const void *data, ssize_t len)
{
    BOOL status;
    DWORD bwrite;
    status = WriteFile((HANDLE)fd, data, len, &bwrite, NULL);
    if (status) return bwrite;
    errno = b_errno_win32;
    return -1;
}


ssize_t
file_read(int fd, void *data, ssize_t len)
{
    BOOL status;
    DWORD bread;

    status = ReadFile((HANDLE)fd, data, len, &bread, NULL);
    if (status) return bread;
    errno = b_errno_win32;
    return -1;
}

boffset_t
file_seek(int fd, boffset_t offset, int whence)
{
    DWORD method = 0;
    DWORD val;
    LONG  offset_low = (LONG)offset;
    LONG  offset_high = (LONG)(offset >> 32);

    switch (whence) {
    case SEEK_SET :
        method = FILE_BEGIN;
        break;
    case SEEK_CUR:
        method = FILE_CURRENT;
        break;
    case SEEK_END:
        method = FILE_END;
        break;
    default:
        errno = EINVAL;
        return -1;
    }


    if ((val=SetFilePointer((HANDLE)fd, offset_low, &offset_high, method)) == INVALID_SET_FILE_POINTER) {
       errno = b_errno_win32;
       return -1;
    }
    /* ***FIXME*** I doubt this works right */
    return val;
}

int
file_dup2(int, int)
{
    errno = ENOSYS;
    return -1;
}
#endif

#ifdef xxx
/* 
 * Emulation of mmap and unmmap for tokyo dbm
 */
void *mmap(void *start, size_t length, int prot, int flags,
           int fd, off_t offset)
{
   DWORD fm_access = 0;
   DWORD mv_access = 0;
   HANDLE h;
   HANDLE mv;

   if (length == 0) {
      return MAP_FAILED;
   }
   if (!fd) {
      return MAP_FAILED;
   }

   if (flags & PROT_WRITE) {
      fm_access |= PAGE_READWRITE;
   } else if (flags & PROT_READ) {
      fm_access |= PAGE_READONLY;
   }
   
   if (flags & PROT_READ) {
      mv_access |= FILE_MAP_READ;
   }
   if (flags & PROT_WRITE) {
      mv_access |= FILE_MAP_WRITE;
   }

   h = CreateFileMapping((HANDLE)_get_osfhandle (fd), 
                         NULL /* security */, 
                         fm_access, 
                         0 /* MaximumSizeHigh */, 
                         0 /* MaximumSizeLow */, 
                         NULL /* name of the file mapping object */);

   if (!h || h == INVALID_HANDLE_VALUE) {
      return MAP_FAILED;
   }

   mv = MapViewOfFile(h, mv_access, 
                      0 /* offset hi */, 
                      0 /* offset lo */,
                      length);
   CloseHandle(h);

   if (!mv || mv == INVALID_HANDLE_VALUE) {
      return MAP_FAILED;
   }

   return (void *) mv;
}

int munmap(void *start, size_t length)
{
   if (!start) {
      return -1;
   }
   UnmapViewOfFile(start);
   return 0;
}
#endif

#ifdef HAVE_MINGW
/* syslog function, added by Nicolas Boichat */
void openlog(const char *ident, int option, int facility) {}  
#endif //HAVE_MINGW

/* Log an error message */
void LogErrorMsg(const char *message)
{
   HANDLE eventHandler;
   const char *strings[2];

   /* Use the OS event logging to log the error */
   eventHandler = RegisterEventSource(NULL, "Bacula");

   strings[0] = _("\n\nBacula ERROR: ");
   strings[1] = message;

   if (eventHandler) {
      ReportEvent(eventHandler, EVENTLOG_ERROR_TYPE,
              0,                      /* category */
              0,                      /* ID */
              NULL,                   /* SID */
              2,                      /* Number of strings */
              0,                      /* raw data size */
              (const char **)strings, /* error strings */
              NULL);                  /* raw data */
      DeregisterEventSource(eventHandler);
   }
}

/*
 * Don't allow OS to suspend while backup running
 *   Note, the OS automatically tracks these for each thread
 */
void prevent_os_suspensions()
{
   /* not in mingw */
//   SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
}

void allow_os_suspensions()
{
//   SetThreadExecutionState(ES_CONTINUOUS);
}
