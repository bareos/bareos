/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.

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
/*                               -*- Mode: C -*-
 * compat.h --
 */
// Copyright transferred from Raider Solutions, Inc to
//   Kern Sibbald and John Walker by express permission.
//
/*
 * Author          : Christopher S. Hull
 * Created On      : Fri Jan 30 13:00:51 2004
 */


#if !defined(__COMPAT_H_)
#define __COMPAT_H_
#if !defined(_STAT_H)
#define _STAT_H         /* don't pull in MinGW stat.h */
#endif

#ifndef _STAT_DEFINED
#define _STAT_DEFINED 1 /* don't pull in MinGW struct stat from wchar.h */
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1400) // VC8+
#pragma warning(disable : 4996) // Either disable all deprecation warnings,
// #define _CRT_SECURE_NO_DEPRECATE // Or just turn off warnings about the newly deprecated CRT functions.
#elif !defined(HAVE_MINGW) && !defined(HAVE_WXCONSOLE)
#define __STDC__ 1
#endif

#include <malloc.h>

#ifdef MINGW64
#include <direct.h>
#define _declspec __declspec

/* Missing in 64 bit mingw */
typedef struct _REPARSE_DATA_BUFFER {
        DWORD  ReparseTag;
        WORD   ReparseDataLength;
        WORD   Reserved;
        union {
                struct {
                        WORD   SubstituteNameOffset;
                        WORD   SubstituteNameLength;
                        WORD   PrintNameOffset;
                        WORD   PrintNameLength;
                        WCHAR PathBuffer[1];
                } SymbolicLinkReparseBuffer;
                struct {
                        WORD   SubstituteNameOffset;
                        WORD   SubstituteNameLength;
                        WORD   PrintNameOffset;
                        WORD   PrintNameLength;
                        WCHAR PathBuffer[1];
                } MountPointReparseBuffer;
                struct {
                        BYTE   DataBuffer[1];
                } GenericReparseBuffer;
        } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#endif

#include <winioctl.h>

#ifdef _WIN64
# define GWL_USERDATA  GWLP_USERDATA
#endif

#ifndef INT64
#define INT64 long long int
#endif

typedef UINT64 u_int64_t;
typedef UINT64 uint64_t;
typedef INT64 int64_t;
typedef UINT32 uint32_t;
typedef INT64 intmax_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed char int8_t;
typedef int __daddr_t;

#if !defined(HAVE_MINGW)
typedef long int32_t;
typedef float float32_t;
typedef double float64_t;
#endif

#if !defined(_MSC_VER) || (_MSC_VER < 1400) // VC8+
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef long time_t;
#endif
#endif

#if __STDC__ && !defined(HAVE_MINGW)
typedef _dev_t dev_t;
#if !defined(HAVE_WXCONSOLE)
typedef __int64 ino_t;
#endif
#endif

typedef UINT32 u_int32_t;
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;

#if !defined(HAVE_MINGW)
#undef uint32_t
#endif

void sleep(int);

typedef UINT32 key_t;

#if defined(HAVE_MINGW)
#if !defined(uid_t)
typedef UINT32 uid_t;
typedef UINT32 gid_t;
#endif
#else
typedef UINT32 uid_t;
typedef UINT32 gid_t;
typedef UINT32 mode_t;
typedef INT32  ssize_t;
typedef UINT32 size_t;
#define HAVE_SSIZE_T 1

#endif /* HAVE_MINGW */

struct dirent {
    uint64_t    d_ino;
    uint32_t    d_off;
    uint16_t    d_reclen;
    char        d_name[256];
};
typedef void DIR;


#if !defined(__cplusplus)
#if !defined(true)
#define true 1
#endif
#if !defined(false)
#define false 0
#endif
#endif

#ifndef _TIMEZONE_DEFINED /* also in sys/time.h */
#define _TIMEZONE_DEFINED
struct timezone {
    int foo;
};
#endif

int strcasecmp(const char*, const char *);
int gettimeofday(struct timeval *, struct timezone *);

#if !defined(EETXTBUSY)
#define EETXTBUSY 26
#endif

#if !defined(ETIMEDOUT)
#define ETIMEDOUT 55
#endif

#if !defined(ENOMEDIUM)
#define ENOMEDIUM 123
#endif

#if !defined(ENODATA)
#define ENODATA 61
#endif

struct stat
{
    _dev_t      st_dev;
    uint64_t    st_ino;
    uint16_t    st_mode;
    int16_t     st_nlink;
    uint32_t    st_uid;
    uint32_t    st_gid;
    _dev_t      st_rdev;
    uint64_t    st_size;
    time_t      st_atime;
    time_t      st_mtime;
    time_t      st_ctime;
    uint32_t    st_blksize;
    uint64_t    st_blocks;
};

#undef  S_IFMT
#define S_IFMT         0170000         /* file type mask */
#undef  S_IFDIR
#define S_IFDIR        0040000         /* directory */
#define S_IFCHR        0020000         /* character special */
#define S_IFBLK        0060000         /* block special */
#define S_IFIFO        0010000         /* pipe */
#undef  S_IFREG
#define S_IFREG        0100000         /* regular */
#define S_IREAD        0000400         /* read permission, owner */
#define S_IWRITE       0000200         /* write permission, owner */
#define S_IEXEC        0000100         /* execute/search permission, owner */

#define S_IRUSR         S_IREAD
#define S_IWUSR         S_IWRITE
#define S_IXUSR         S_IEXEC
#define S_ISREG(x)  (((x) & S_IFMT) == S_IFREG)
#define S_ISDIR(x)  (((x) & S_IFMT) == S_IFDIR)
#define S_ISCHR(x) 0
#define S_ISBLK(x)  (((x) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(x) 0

#define S_IRGRP         000040
#define S_IWGRP         000020
#define S_IXGRP         000010

#define S_IROTH         00004
#define S_IWOTH         00002
#define S_IXOTH         00001

#define S_IRWXO         000007
#define S_IRWXG         000070
#define S_ISUID         004000
#define S_ISGID         002000
#define S_ISVTX         001000
#define S_ISSOCK(x) 0
#define S_ISLNK(x)      0

#if __STDC__
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR   _O_RDWR
#define O_CREAT  _O_CREAT
#define O_TRUNC  _O_TRUNC

#define isascii __isascii
#define toascii __toascii
#define iscsymf __iscsymf
#define iscsym  __iscsym
#endif

typedef  BOOL (*t_pVSSPathConvert)(const char *szFilePath, char *szShadowPath, int nBuflen);
typedef  BOOL (*t_pVSSPathConvertW)(const wchar_t  *szFilePath, wchar_t  *szShadowPath, int nBuflen);

void SetVSSPathConvert(t_pVSSPathConvert pPathConvert, t_pVSSPathConvertW pPathConvertW);

int lchown(const char *, uid_t uid, gid_t gid);
int chown(const char *, uid_t uid, gid_t gid);
#if !defined(HAVE_MINGW)
int chmod(const char *, mode_t mode);
#endif
#define O_NONBLOCK   04000
#define F_GETFL      3
#define F_SETFL      4

int win32_tape_open(const char *file, int flags, ...);
int win32_tape_ioctl(int fd, unsigned long int request, ...);
int win32_tape_close(int fd);
ssize_t win32_tape_read(int fd, void *buffer, size_t count);
ssize_t win32_tape_write(int fd, const void *buffer, size_t count);

ssize_t win32_read(int fd, void *buffer, size_t count);
ssize_t win32_write(int fd, const void *buffer, size_t count);
int win32_ioctl(int fd, unsigned long int req, ...);

#ifndef MINGW64
#define open   _open
#endif

int fcntl(int fd, int cmd, long arg);
int fstat(intptr_t fd, struct stat *sb);

int inet_aton(const char *cp, struct in_addr *inp);
int kill(int pid, int signo);
int pipe(int []);
int fork();
int waitpid(int, int *, int);

#if !defined(HAVE_MINGW)
#define strncasecmp strnicmp
//int strncasecmp(const char*, const char *, int);
int utime(const char *filename, struct utimbuf *buf);
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif //HAVE_MINGW


#define WNOHANG 0
#define WIFEXITED(x) 0
#define WEXITSTATUS(x) x
#define WIFSIGNALED(x) 0
#define WTERMSIG(x) x
#define SIGKILL 9
#define SIGUSR2 9999

#define HAVE_OLD_SOCKOPT

struct timespec;
int readdir(unsigned int fd, struct dirent *dirp, unsigned int count);
int nanosleep(const struct timespec*, struct timespec *);
long int random(void);
void srandom(unsigned int seed);
int lstat(const char *, struct stat *);
int stat(const char *file, struct stat *sb);
long pathconf(const char *, int);
int readlink(const char *, char *, int);
#define _PC_PATH_MAX 1
#define _PC_NAME_MAX 2

int geteuid();

DIR *opendir(const char *name);
int closedir(DIR *dir);

struct passwd {
    char *foo;
};

struct group {
    char *foo;
};

struct passwd *getpwuid(uid_t);
struct group *getgrgid(uid_t);

struct sigaction {
    int sa_flags;
    void (*sa_handler)(int);
};
#define sigfillset(x)
#define sigaction(a, b, c)

#define mkdir(p, m) win32_mkdir(p)
#define unlink win32_unlink
#define chdir win32_chdir
#define chmod win32_chmod
extern "C" void syslog(int type, const char *fmt, ...);
#if !defined(LOG_DAEMON)
#define LOG_DAEMON 0
#endif

#if !defined(HAVE_MINGW)
#define R_OK 04
#define W_OK 02
int stat(const char *, struct stat *);
#if defined(__cplusplus)
#define access _access
extern "C" _CRTIMP int __cdecl _access(const char *, int);
int execvp(const char *, char *[]);
extern "C" void *  __cdecl _alloca(size_t);
#endif
#endif //HAVE_MINGW

#define getpid _getpid

#define getppid() 0
#define gethostid() 0
#define getuid() 0
#define getgid() 0

#define getcwd win32_getcwd
#define chdir win32_chdir
#define chmod win32_chmod
#define fputs win32_fputs
char *win32_getcwd(char *buf, int maxlen);
int win32_chdir(const char *buf);
int win32_mkdir(const char *buf);
int win32_fputs(const char *string, FILE *stream);
int win32_unlink(const char *filename);
int win32_chmod(const char *, mode_t);


char* win32_cgets (char* buffer, int len);

int WSA_Init(void);
void Win32ConvCleanupCache();

#if defined(HAVE_MINGW)
void closelog();
void openlog(const char *ident, int option, int facility);
#endif //HAVE_MINGW

/* Don't let OS go to sleep (usually a Laptop) while we are backing up */
void prevent_os_suspensions();
void allow_os_suspensions();

typedef DWORD EXECUTION_STATE;
#ifndef ES_CONTINUOUS
#define ES_CONTINUOUS            0x80000000
#endif
#ifndef ES_SYSTEM_REQUIRED
#define ES_SYSTEM_REQUIRED       0x00000001
#endif
#ifndef ES_DISPLAY_REQUIRED
#define ES_DISPLAY_REQUIRED      0x00000002
#endif
#ifndef ES_USER_PRESENT
# define ES_USER_PRESENT          0x00000004
#endif

WINBASEAPI EXECUTION_STATE WINAPI SetThreadExecutionState(EXECUTION_STATE esFlags);


extern void LogErrorMsg(const char *message);

#if !defined(INVALID_FILE_ATTRIBUTES)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

#if defined(_MSC_VER)
inline unsigned long ffs(unsigned long word)
{
   unsigned long  index;
   
   if (_BitScanForward(&index, word) != 0)
      return index + 1;
   else
      return 0;
}

#else
#define  ffs   __builtin_ffs
#endif


int win32_ftruncate(int fd, int64_t length);

#undef ftruncate
#define ftruncate win32_ftruncate

#endif /* __COMPAT_H_ */
