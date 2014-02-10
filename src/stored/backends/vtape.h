/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2010 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * vtape.h - Emulate the Linux st (scsi tape) driver on file, for regression and bug hunting purpose
 */

#ifndef VTAPE_H
#define VTAPE_H

#include "bareos.h"

void vtape_debug(int level);

#define FTAPE_MAX_DRIVE 50

#define VTAPE_MAX_BLOCK 20 * 1024 * 2048;  /* 20GB */

typedef enum {
   VT_READ_EOF,                 /* Need to read the entire EOF struct */
   VT_SKIP_EOF                  /* Have already read the EOF byte */
} VT_READ_FM_MODE;

class vtape: public DEVICE {
private:
   /*
    * Members.
    */
   int fd;                      /* Our file descriptor */
   boffset_t file_block;        /* Size */
   boffset_t max_block;
   boffset_t last_FM;           /* Last file mark (last file) */
   boffset_t next_FM;           /* Next file mark (next file) */
   boffset_t cur_FM;            /* Current file mark */
   bool atEOF;                  /* End of file */
   bool atEOT;                  /* End of media */
   bool atEOD;                  /* End of data */
   bool atBOT;                  /* Begin of tape */
   bool online;                 /* Volume online */
   bool needEOF;                /* Check if last operation need eof */
   int32_t last_file;           /* Last file of the volume */
   int32_t current_file;        /* Current position */
   int32_t current_block;       /* Current position */

   /*
    * Methods.
    */
   void destroy();
   int offline();
   int truncate_file();
   void check_eof() { if (needEOF) weof(); };
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
   int d_open(const char *pathname, int flags, int mode);
   int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   ssize_t d_read(int fd, void *buffer, size_t count);
   ssize_t d_write(int fd, const void *buffer, size_t count);
   bool d_truncate(DCR *dcr);

   boffset_t d_lseek(DCR *dcr, off_t offset, int whence);
   boffset_t lseek(int fd, off_t offset, int whence);

   int tape_op(struct mtop *mt_com);
   int tape_get(struct mtget *mt_com);
   int tape_pos(struct mtpos *mt_com);
};
#endif /* !VTAPE_H */
