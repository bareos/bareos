/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010-2011 Bacula Systems(R) SA

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the Free
   Software Foundation, which is listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/* You can include this file to your plugin to have
 * access to some common tools and utilities provided by Bareos
 */

#ifndef PCOMMON_H
#define PCOMMON_H

#define JT_BACKUP                'B'  /* Backup Job */
#define JT_RESTORE               'R'  /* Restore Job */

#define L_FULL                   'F'  /* Full backup */
#define L_INCREMENTAL            'I'  /* since last backup */
#define L_DIFFERENTIAL           'D'  /* since last full backup */

#ifndef DLL_IMP_EXP
# if defined(BUILDING_DLL)
#   define DLL_IMP_EXP   __declspec(dllexport)
# elif defined(USING_DLL)
#   define DLL_IMP_EXP   __declspec(dllimport)
# else
#   define DLL_IMP_EXP
# endif
#endif

DLL_IMP_EXP void *sm_malloc(const char *fname, int lineno, unsigned int nbytes);
DLL_IMP_EXP void sm_free(const char *file, int line, void *fp);
DLL_IMP_EXP void *reallymalloc(const char *fname, int lineno, unsigned int nbytes);
DLL_IMP_EXP void reallyfree(const char *file, int line, void *fp);

#ifndef bmalloc
# define bmalloc(s)      sm_malloc(__FILE__, __LINE__, (s))
# define bfree(o)        sm_free(__FILE__, __LINE__, (o))
#endif

#define SM_CHECK sm_check(__FILE__, __LINE__, false)

#ifdef malloc
#undef malloc
#undef free
#endif

#define malloc(s)    sm_malloc(__FILE__, __LINE__, (s))
#define free(o)      sm_free(__FILE__, __LINE__, (o))

inline void *operator new(size_t size, char const * file, int line)
{
   void *pnew = sm_malloc(file,line, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

inline void *operator new[](size_t size, char const * file, int line)
{
   void *pnew = sm_malloc(file, line, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

inline void *operator new(size_t size)
{
   void *pnew = sm_malloc(__FILE__, __LINE__, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

inline void *operator new[](size_t size)
{
   void *pnew = sm_malloc(__FILE__, __LINE__, size);
   memset((char *)pnew, 0, size);
   return pnew;
}

#define new   new(__FILE__, __LINE__)

inline void operator delete(void *buf)
{
   sm_free( __FILE__, __LINE__, buf);
}

inline void operator delete[] (void *buf)
{
  sm_free(__FILE__, __LINE__, buf);
}

#define Dmsg(context, level,  ...) bfuncs->DebugMessage(context, __FILE__, __LINE__, level, __VA_ARGS__ )
#define Jmsg(context, type,  ...) bfuncs->JobMessage(context, __FILE__, __LINE__, type, 0, __VA_ARGS__ )

#ifdef USE_ADD_DRIVE
/* Keep drive letters for windows vss snapshot */
static void add_drive(char *drives, int *nCount, char *fname) {
   if (strlen(fname) >= 2 && B_ISALPHA(fname[0]) && fname[1] == ':') {
      /* always add in uppercase */
      char ch = toupper(fname[0]);
      /* if not found in string, add drive letter */
      if (!strchr(drives,ch)) {
         drives[*nCount] = ch;
         drives[*nCount+1] = 0;
         (*nCount)++;
      }
   }
}

/* Copy our drive list to Bareos core list */
static void copy_drives(char *drives, char *dest) {
   int last = strlen(dest);     /* dest is 27 bytes long */
   for (char *p = drives; *p && last < 26; p++) {
      if (!strchr(dest, *p)) {
         dest[last++] = *p;
         dest[last] = 0;
      }
   }
}
#endif
#endif
