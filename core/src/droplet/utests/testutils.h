/* check-sources:disable-copyright-check */
#ifndef BAREOS_DROPLET_UTESTS_TESTUTILS_H_
#define BAREOS_DROPLET_UTESTS_TESTUTILS_H_

#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>

extern char* strconcat(const char* s, ...);
extern char* make_unique_directory(void);
extern char* make_sub_directory(const char* parent, const char* child);
extern void rmtree(const char* path);
extern char* write_file(const char* parent,
                        const char* child,
                        const char* contents);
extern off_t file_size(FILE* fp);

#ifdef __linux__
extern char* home; /* assign this to pervert getpwuid() */
#endif             /* __linux__ */

#endif  // BAREOS_DROPLET_UTESTS_TESTUTILS_H_
