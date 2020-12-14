#ifndef __TESTUTIL_H__
#define __TESTUTIL_H__ 1

#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>

extern char *strconcat(const char *s, ...);
extern char *make_unique_directory(void);
extern char *make_sub_directory(const char *parent, const char *child);
extern void rmtree(const char *path);
extern char *write_file(const char *parent, const char *child, const char *contents);
extern off_t file_size(FILE *fp);

#ifdef __linux__
extern char *home;	/* assign this to pervert getpwuid() */
#endif /* __linux__ */

#endif /* __TESTUTIL_H__ */
