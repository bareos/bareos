/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
/*
 * vtape.h - Emulate the Linux st (scsi tape) driver on file.
 * for regression and bug hunting purpose
 *
 */

#ifndef VTAPE_H
#define VTAPE_H

#include <stdarg.h>
#include <stddef.h>
#include "bacula.h"

void vtape_debug(int level);

#ifdef USE_VTAPE

#define FTAPE_MAX_DRIVE 50

#define VTAPE_MAX_BLOCK 20*1024*2048;      /* 20GB */

typedef enum {
   VT_READ_EOF,                 /* Need to read the entire EOF struct */
   VT_SKIP_EOF                  /* Have already read the EOF byte */
} VT_READ_FM_MODE;

class vtape: public DEVICE {
private:
   int         fd;              /* Our file descriptor */

   boffset_t   file_block;      /* size */
   boffset_t   max_block;

   boffset_t   last_FM;         /* last file mark (last file) */
   boffset_t   next_FM;         /* next file mark (next file) */
   boffset_t   cur_FM;          /* current file mark */

   bool        atEOF;           /* End of file */
   bool        atEOT;           /* End of media */
   bool        atEOD;           /* End of data */
   bool        atBOT;           /* Begin of tape */
   bool        online;          /* volume online */
   bool        needEOF;         /* check if last operation need eof */

   int32_t     last_file;       /* last file of the volume */
   int32_t     current_file;    /* current position */
   int32_t     current_block;   /* current position */

   void destroy();
   int offline();
   int truncate_file();
   void check_eof() { if(needEOF) weof();};
   void update_pos();
   bool read_fm(VT_READ_FM_MODE readfirst);

public:
   int fsf();
   int fsr(int count);
   int weof();
   int bsf();
   int bsr(int count);

   vtape();
   ~vtape();
   int get_fd();
   void dump();

   /* interface from DEVICE */
   int d_close(int);
   int d_open(const char *pathname, int flags);
   int d_ioctl(int fd, ioctl_req_t request, char *mt=NULL);
   ssize_t d_read(int, void *buffer, size_t count);
   ssize_t d_write(int, const void *buffer, size_t count);

   boffset_t lseek(DCR *dcr, off_t offset, int whence) { return -1; }
   boffset_t lseek(int fd, off_t offset, int whence);

   int tape_op(struct mtop *mt_com);
   int tape_get(struct mtget *mt_com);
   int tape_pos(struct mtpos *mt_com);
};


#else  /*!USE_VTAPE */

class vtape: public DEVICE {
public:
   int d_open(const char *pathname, int flags) { return -1; }
   ssize_t d_read(void *buffer, size_t count) { return -1; }
   ssize_t d_write(const void *buffer, size_t count) { return -1; }
   int d_close(int) { return -1; }
   int d_ioctl(int fd, ioctl_req_t request, char *mt=NULL) { return -1; }
   boffset_t lseek(DCR *dcr, off_t offset, int whence) { return -1; }
};

#endif  /* USE_VTAPE */

#endif /* !VTAPE_H */
