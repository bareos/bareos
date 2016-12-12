/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
/**
 @file
 Definitions for the smart memory allocator

*/

#ifndef SMARTALLOC_H
#define SMARTALLOC_H

extern uint64_t DLL_IMP_EXP sm_max_bytes;
extern uint64_t DLL_IMP_EXP sm_bytes;
extern uint32_t DLL_IMP_EXP sm_max_buffers;
extern uint32_t DLL_IMP_EXP sm_buffers;

#ifdef  SMARTALLOC
#undef  SMARTALLOC
#define SMARTALLOC SMARTALLOC


extern void *sm_malloc(const char *fname, int lineno, unsigned int nbytes),
            *sm_calloc(const char *fname, int lineno,
                unsigned int nelem, unsigned int elsize),
            *sm_realloc(const char *fname, int lineno, void *ptr, unsigned int size),
            *actuallymalloc(unsigned int size),
            *actuallycalloc(unsigned int nelem, unsigned int elsize),
            *actuallyrealloc(void *ptr, unsigned int size);
extern void sm_free(const char *fname, int lineno, void *fp);
extern void actuallyfree(void *cp),
            sm_dump(bool bufdump, bool in_use=false), sm_static(int mode);
extern void sm_new_owner(const char *fname, int lineno, char *buf);

#ifdef SMCHECK
#define Dsm_check(lvl) if ((lvl)<=debug_level) sm_check(__FILE__, __LINE__, true)
extern void sm_check(const char *fname, int lineno, bool bufdump);
extern int sm_check_rtn(const char *fname, int lineno, bool bufdump);
#else
#define Dsm_check(lvl)
#define sm_check(f, l, fl)
#define sm_check_rtn(f, l, fl) 1
#endif


/* Redefine standard memory allocator calls to use our routines
   instead. */

#define free(x)        sm_free(__FILE__, __LINE__, (x))
#define cfree(x)       sm_free(__FILE__, __LINE__, (x))
#define malloc(x)      sm_malloc(__FILE__, __LINE__, (x))
#define calloc(n,e)    sm_calloc(__FILE__, __LINE__, (n), (e))
#define realloc(p,x)   sm_realloc(__FILE__, __LINE__, (p), (x))

#else

/* If SMARTALLOC is disabled, define its special calls to default to
   the standard routines.  */

#define actuallyfree(x)      free(x)
#define actuallymalloc(x)    malloc(x)
#define actuallycalloc(x,y)  calloc(x,y)
#define actuallyrealloc(x,y) realloc(x,y)
#define sm_dump(x, false)
#define sm_static(x)
#define sm_new_owner(a, b, c)
#define sm_malloc(f, l, n)     malloc(n)
#define sm_free(f, l, n)       free(n)
#define sm_check(f, l, fl)
#define sm_check_rtn(f, l, fl) 1

#define Dsm_check(lvl)

extern void *b_malloc(const char *file, int line, size_t size);
#define malloc(x) b_malloc(__FILE__, __LINE__, (x))

#endif

#ifdef SMARTALLOC

#define New(type) new(__FILE__, __LINE__) type

/* We do memset(0) because it's not possible to memset a class when
 * using subclass with virtual functions
 */

class SMARTALLOC
{
public:

void *operator new(size_t s, const char *fname, int line)
{
   size_t size =  s > sizeof(int) ? (unsigned int)s : sizeof(int);
   void *p = sm_malloc(fname, line, size);
   memset(p, 0, size);
   return p;
}
void *operator new[](size_t s, const char *fname, int line)
{
   size_t size =  s > sizeof(int) ? (unsigned int)s : sizeof(int);
   void *p = sm_malloc(fname, line, size);
   memset(p, 0, size);
   return p;
}

void  operator delete(void *ptr)
{
   free(ptr);
}
void  operator delete[](void *ptr, size_t /*i*/)
{
   free(ptr);
}

void  operator delete(void *ptr, const char * /*fname*/, int /*line*/)
{
   free(ptr);
}
void  operator delete[](void *ptr, size_t /*i*/,
                        const char * /*fname*/, int /*line*/)
{
   free(ptr);
}

private:
void *operator new(size_t s) throw() { (void)s; return 0; }
void *operator new[](size_t s) throw() { (void)s; return 0; }
};

#else

#define New(type) new type

class SMARTALLOC
{
   public:
      void *operator new(size_t s) {
         void *p = malloc(s);
         memset(p, 0, s);
         return p;
      }
      void *operator new[](size_t s) {
         void *p = malloc(s);
         memset(p, 0, s);
         return p;
      }
      void  operator delete(void *ptr) {
          free(ptr);
      }
      void  operator delete[](void *ptr, size_t i) {
          free(ptr);
      }
};
#endif  /* SMARTALLOC */

#endif  /* !SMARTALLOC_H */
