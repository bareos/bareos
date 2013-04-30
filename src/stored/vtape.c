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

Device {
  Name = Drive-1                      #
  Maximum File Size = 800M
  Maximum Volume Size = 3G
  Device Type = TAPE
  Archive Device = /tmp/fake
  Media Type = DLT-8000
  AutomaticMount = yes;               # when device opened, read it
  AlwaysOpen = yes;
  RemovableMedia = yes;
  RandomAccess = no;
}

  Block description :

  block {
    int32  size;
    void   *data;
  }

  EOF description :

  EOF {
    int32  size=0;
  }


 */

#include "bacula.h"             /* define 64bit file usage */
#include "stored.h"

#include "vtape.h"


#ifdef USE_VTAPE

static int dbglevel = 100;
#define FILE_OFFSET 30

void vtape_debug(int level)
{
   dbglevel = level;
}

int vtape::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   int result = 0;

   if (request == MTIOCTOP) {
      result = tape_op((mtop *)op);
   } else if (request == MTIOCGET) {
      result = tape_get((mtget *)op);
   } else if (request == MTIOCPOS) {
      result = tape_pos((mtpos *)op);
   } else {
      errno = ENOTTY;
      result = -1;
   }

   return result;
}

int vtape::tape_op(struct mtop *mt_com)
{
   int result=0;
   int count = mt_com->mt_count;

   if (!online) {
      errno = ENOMEDIUM;
      return -1;
   }
   
   switch (mt_com->mt_op)
   {
   case MTRESET:
   case MTNOP:
   case MTSETDRVBUFFER:
      break;

   default:
   case MTRAS1:
   case MTRAS2:
   case MTRAS3:
   case MTSETDENSITY:
      errno = ENOTTY;
      result = -1;
      break;

   case MTFSF:                  /* Forward space over mt_count filemarks. */
      do {
         result = fsf();
      } while (--count > 0 && result == 0);
      break;

   case MTBSF:                  /* Backward space over mt_count filemarks. */
      do {
         result = bsf();
      } while (--count > 0 && result == 0);
      break;

   case MTFSR:      /* Forward space over mt_count records (tape blocks). */
/*
    file number = 1
    block number = 0
   
    file number = 1
    block number = 1
   
    mt: /dev/lto2: Erreur d'entree/sortie
   
    file number = 2
    block number = 0
*/
      /* tester si on se trouve a la fin du fichier */
      result = fsr(mt_com->mt_count);
      break;

   case MTBSR:      /* Backward space over mt_count records (tape blocks). */
      result = bsr(mt_com->mt_count);
      break;

   case MTWEOF:                 /* Write mt_count filemarks. */
      do {
         result = weof();
      } while (result == 0 && --count > 0);
      break;

   case MTREW:                  /* Rewind. */
      Dmsg0(dbglevel, "rewind vtape\n");
      check_eof();
      atEOF = atEOD = false;
      atBOT = true;
      current_file = 0;
      current_block = 0;
      lseek(fd, 0, SEEK_SET);
      result = !read_fm(VT_READ_EOF);
      break;

   case MTOFFL:                 /* put tape offline */
      result = offline();
      break;

   case MTRETEN:                /* Re-tension tape. */
      result = 0;
      break;

   case MTBSFM:                 /* not used by bacula */
      errno = EIO;
      result = -1;
      break;

   case MTFSFM:                 /* not used by bacula */
      errno = EIO;
      result = -1;
      break;

   case MTEOM:/* Go to the end of the recorded media (for appending files). */
      while (next_FM) {
         lseek(fd, next_FM, SEEK_SET);
         if (read_fm(VT_READ_EOF)) {
            current_file++;
         }
      }
      boffset_t l;
      while (::read(fd, &l, sizeof(l)) > 0) {
         if (l) {
            lseek(fd, l, SEEK_CUR);
         } else {
            ASSERT(0);
         }
         Dmsg0(dbglevel, "skip 1 block\n");
      }
      current_block = -1;
      atEOF = false;
      atEOD = true;

/*
   file number = 3
   block number = -1
*/
      /* Can be at EOM */
      break;

   case MTERASE:                /* not used by bacula */
      atEOD = true;
      atEOF = false;
      atEOT = false;

      current_file = 0;
      current_block = -1;
      lseek(fd, 0, SEEK_SET);
      read_fm(VT_READ_EOF);
      truncate_file();
      break;

   case MTSETBLK:
      break;

   case MTSEEK:
      break;

   case MTTELL:
      break;

   case MTFSS:
      break;

   case MTBSS:
      break;

   case MTWSM:
      break;

   case MTLOCK:
      break;

   case MTUNLOCK:
      break;

   case MTLOAD:
      break;

   case MTUNLOAD:
      break;

   case MTCOMPRESSION:
      break;

   case MTSETPART:
      break;

   case MTMKPART:
      break;
   }

   return result == 0 ? 0 : -1;
}

int vtape::tape_get(struct mtget *mt_get)
{
   int density = 1;
   int block_size = 1024;

   mt_get->mt_type = MT_ISSCSI2;
   mt_get->mt_blkno = current_block;
   mt_get->mt_fileno = current_file;

   mt_get->mt_resid = -1;
//   pos_info.PartitionBlockValid ? pos_info.Partition : (ULONG)-1;

   /* TODO */
   mt_get->mt_dsreg = 
      ((density << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK) |
      ((block_size << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK);


   mt_get->mt_gstat = 0x00010000;  /* Immediate report mode.*/

   if (atEOF) {
      mt_get->mt_gstat |= 0x80000000;     // GMT_EOF
   }

   if (atBOT) {
      mt_get->mt_gstat |= 0x40000000;     // GMT_BOT
   }
   if (atEOT) {
      mt_get->mt_gstat |= 0x20000000;     // GMT_EOT
   }

   if (atEOD) {
      mt_get->mt_gstat |= 0x08000000;     // GMT_EOD
   }

   if (0) { //WriteProtected) {
      mt_get->mt_gstat |= 0x04000000;     // GMT_WR_PROT
   }

   if (online) {
      mt_get->mt_gstat |= 0x01000000;     // GMT_ONLINE
   } else {
      mt_get->mt_gstat |= 0x00040000;  // GMT_DR_OPEN
   }
   mt_get->mt_erreg = 0;

   return 0;
}

int vtape::tape_pos(struct mtpos *mt_pos)
{
   if (current_block >= 0) {
      mt_pos->mt_blkno = current_block;
      return 0;
   }

   return -1;
}

/*
 * This function try to emulate the append only behavior
 * of a tape. When you wrote something, data after the
 * current position are discarded.
 */
int vtape::truncate_file()
{  
   Dmsg2(dbglevel, "truncate %i:%i\n", current_file, current_block);
   ftruncate(fd, lseek(fd, 0, SEEK_CUR));
   last_file = current_file;
   atEOD=true;
   update_pos();
   return 0;
}

vtape::~vtape()
{
}

vtape::vtape()
{
   fd = -1;

   atEOF = false;
   atBOT = false;
   atEOT = false;
   atEOD = false;
   online = false;
   needEOF = false;

   file_block = 0;
   last_file = 0;
   current_file = 0;
   current_block = -1;

   max_block = VTAPE_MAX_BLOCK;
}

int vtape::get_fd()
{
   return this->fd;
}

/*
 * Write a variable block of count size.
 * block = vtape_header + data
 * vtape_header = sizeof(data)
 * if vtape_header == 0, this is a EOF
 */
ssize_t vtape::d_write(int, const void *buffer, size_t count)
{
   ASSERT(online);
   ASSERT(current_file >= 0);
   ASSERT(count > 0);
   ASSERT(buffer);

   ssize_t nb;
   Dmsg3(dbglevel*2, "write len=%i %i:%i\n", 
         count, current_file,current_block);

   if (atEOT) {
      Dmsg0(dbglevel, "write nothing, EOT !\n");
      errno = ENOSPC;
      return -1;
   }

   if (!atEOD) {                /* if not at the end of the data */
      truncate_file();
   }
 
   if (current_block != -1) {
      current_block++;
   }

   atBOT = false;
   atEOF = false;
   atEOD = true;                /* End of data */

   needEOF = true;              /* next operation need EOF mark */

   uint32_t size = count;
   ::write(fd, &size, sizeof(uint32_t));
   nb = ::write(fd, buffer, count);
   
   if (nb != (ssize_t)count) {
      atEOT = true;
      Dmsg2(dbglevel, 
            "Not enough space writing only %i of %i requested\n", 
            nb, count);
   }

   update_pos();

   return nb;
}

/*
 *  +---+---------+---+------------------+---+-------------------+
 *  |00N|  DATA   |0LN|   DATA           |0LC|     DATA          |
 *  +---+---------+---+------------------+---+-------------------+
 *
 *  0 : zero
 *  L : Last FileMark offset
 *  N : Next FileMark offset
 *  C : Current FileMark Offset
 */
int vtape::weof()
{
   ASSERT(online);
   ASSERT(current_file >= 0);

   if (atEOT) {
      errno = ENOSPC;
      current_block = -1;
      return -1;
   }

   if (!atEOD) {
      truncate_file();             /* nothing after this point */
   }

   last_FM = cur_FM;
   cur_FM = lseek(fd, 0, SEEK_CUR); // current position
   
   /* update previous next_FM  */
   lseek(fd, last_FM + sizeof(uint32_t)+sizeof(boffset_t), SEEK_SET);
   ::write(fd, &cur_FM, sizeof(boffset_t));
   lseek(fd, cur_FM, SEEK_SET);

   next_FM = 0;

   uint32_t c=0;
   ::write(fd, &c,       sizeof(uint32_t)); // EOF
   ::write(fd, &last_FM, sizeof(last_FM));  // F-1
   ::write(fd, &next_FM, sizeof(next_FM));  // F   (will be updated next time)

   current_file++;
   current_block = 0;

   needEOF = false;
   atEOD = false;
   atBOT = false;
   atEOF = true;

   last_file = MAX(current_file, last_file);

   Dmsg4(dbglevel, "Writing EOF %i:%i last=%lli cur=%lli next=0\n", 
         current_file, current_block, last_FM, cur_FM);

   return 0;
}

/*
 * Go to next FM
 */
int vtape::fsf()
{   
   ASSERT(online);
   ASSERT(current_file >= 0);
   ASSERT(fd >= 0);
/*
 * 1 0 -> fsf -> 2 0 -> fsf -> 2 -1
 */

   int ret=0;
   if (atEOT || atEOD) {
      errno = EIO;
      current_block = -1;
      return -1;
   }

   atBOT = false;
   Dmsg2(dbglevel+1, "fsf %i <= %i\n", current_file, last_file);

   if (next_FM > cur_FM) {      /* not the last file */
      lseek(fd, next_FM, SEEK_SET);
      read_fm(VT_READ_EOF);
      current_file++;
      atEOF = true;
      ret = 0;

   } else if (atEOF) {          /* last file mark */
      current_block=-1;
      errno = EIO;
      atEOF = false;
      atEOD = true;

   } else {                     /* last file, but no at the end */
      fsr(100000);

      Dmsg0(dbglevel, "Try to FSF after EOT\n");
      errno = EIO;
      current_file = last_file ;
      current_block = -1;
      atEOD=true;
      ret = -1;
   }
   return ret;
}

/* /------------\ /---------------\
 * +---+------+---+---------------+-+
 * |OLN|      |0LN|               | |
 * +---+------+---+---------------+-+
 */

bool vtape::read_fm(VT_READ_FM_MODE read_all)
{
   int ret;
   uint32_t c = 0;
   if (read_all == VT_READ_EOF) {
      ::read(fd, &c, sizeof(c));
      if (c != 0) {
         lseek(fd, cur_FM, SEEK_SET);
         return false;
      }
   }

   cur_FM = lseek(fd, 0, SEEK_CUR) - sizeof(c);

   ::read(fd, &last_FM, sizeof(last_FM));
   ret = ::read(fd, &next_FM, sizeof(next_FM));

   current_block=0;
   
   Dmsg3(dbglevel, "Read FM cur=%lli last=%lli next=%lli\n", 
         cur_FM, last_FM, next_FM);

   return (ret == sizeof(next_FM));
}

/*
 * TODO: Check fsr with EOF
 */
int vtape::fsr(int count)
{
   ASSERT(online);
   ASSERT(current_file >= 0);
   ASSERT(fd >= 0);
   
   int i,nb, ret=0;
// boffset_t where=0;
   uint32_t s;
   Dmsg4(dbglevel, "fsr %i:%i EOF=%i c=%i\n", 
         current_file,current_block,atEOF,count);

   check_eof();

   if (atEOT) {
      errno = EIO;
      current_block = -1;
      return -1;
   }

   if (atEOD) {
      errno = EIO;
      return -1;
   }

   atBOT = atEOF = false;   

   /* check all block record */
   for(i=0; (i < count) && !atEOF ; i++) {
      nb = ::read(fd, &s, sizeof(uint32_t)); /* get size of next block */
      if (nb == sizeof(uint32_t) && s) {
         current_block++;
         lseek(fd, s, SEEK_CUR);     /* seek after this block */
      } else {
         Dmsg4(dbglevel, "read EOF %i:%i nb=%i s=%i\n",
               current_file, current_block, nb,s);
         errno = EIO;
         ret = -1;
         if (next_FM) {
            current_file++;
            read_fm(VT_SKIP_EOF);
         }
         atEOF = true;          /* stop the loop */
      }
   }

   return ret;
}

/*
 * BSR + EOF => begin of EOF + EIO
 * BSR + BSR + EOF => last block
 * current_block = -1
 */
int vtape::bsr(int count)
{
   ASSERT(online);
   ASSERT(current_file >= 0);
   ASSERT(count == 1);
   ASSERT(fd >= 0);

   check_eof();

   if (!count) {
      return 0;
   }

   int ret=0;
   int last_f=0;
   int last_b=0;

   boffset_t last=-1, last2=-1;
   boffset_t orig = lseek(fd, 0, SEEK_CUR);
   int orig_f = current_file;
   int orig_b = current_block;

   Dmsg4(dbglevel, "bsr(%i) cur_blk=%i orig=%lli cur_FM=%lli\n", 
         count, current_block, orig, cur_FM);

   /* begin of tape, do nothing */
   if (atBOT) {
      errno = EIO;
      return -1;
   }

   /* at EOF 0:-1 BOT=0 EOD=0 EOF=0 ERR: Input/output error  */
   if (atEOF) {
      lseek(fd, cur_FM, SEEK_SET);
      atEOF = false;
      if (current_file > 0) {
         current_file--;
      }
      current_block=-1;
      errno = EIO;
      return -1;
   }

   /*
    * First, go to cur/last_FM and read all blocks to find the good one
    */
   if (cur_FM == orig) {        /* already just before  EOF */
      lseek(fd, last_FM, SEEK_SET);

   } else {
      lseek(fd, cur_FM, SEEK_SET);
   }

   ret = read_fm(VT_READ_EOF);

   do {
      if (!atEOF) {
         last2 = last;          /* keep track of the 2 last blocs position */
         last = lseek(fd, 0, SEEK_CUR);
         last_f = current_file;
         last_b = current_block;
         Dmsg6(dbglevel, "EOF=%i last2=%lli last=%lli < orig=%lli %i:%i\n", 
               atEOF, last2, last, orig, current_file, current_block);
      }
      ret = fsr(1);
   } while ((lseek(fd, 0, SEEK_CUR) < orig) && (ret == 0));

   if (last2 > 0 && atEOF) {    /* we take the previous position */
      lseek(fd, last2, SEEK_SET);
      current_file = last_f;
      current_block = last_b - 1;
      Dmsg3(dbglevel, "1 set offset2=%lli %i:%i\n", 
            last, current_file, current_block);

   } else if (last > 0) {
      lseek(fd, last, SEEK_SET);
      current_file = last_f;
      current_block = last_b;
      Dmsg3(dbglevel, "2 set offset=%lli %i:%i\n", 
            last, current_file, current_block);
   } else {
      lseek(fd, orig, SEEK_SET);
      current_file = orig_f;
      current_block = orig_b;
      return -1;
   }

   Dmsg2(dbglevel, "bsr %i:%i\n", current_file, current_block);
   errno=0;
   atEOT = atEOF = atEOD = false;
   atBOT = (lseek(fd, 0, SEEK_CUR) - (sizeof(uint32_t)+2*sizeof(boffset_t))) == 0;

   if (orig_b == -1) {
      current_block = orig_b;
   }

   return 0;
}

boffset_t vtape::lseek(int fd, off_t offset, int whence)
{
   return ::lseek(fd, offset, whence);
}

/* BSF => just before last EOF
 * EOF + BSF => just before EOF
 * file 0 + BSF => BOT + errno
 */
int vtape::bsf()
{
   ASSERT(online);
   ASSERT(current_file >= 0);
   Dmsg2(dbglevel, "bsf %i:%i count=%i\n", current_file, current_block);
   int ret = 0;

   check_eof();

   atBOT = atEOF = atEOT = atEOD = false;

   if (current_file == 0) {/* BOT + errno */      
      lseek(fd, 0, SEEK_SET);
      read_fm(VT_READ_EOF);
      current_file = 0;
      current_block = 0;
      atBOT = true;
      errno = EIO;
      ret = -1;
   } else {
      Dmsg1(dbglevel, "bsf last=%lli\n", last_FM);
      lseek(fd, cur_FM, SEEK_SET);
      current_file--;
      current_block=-1;
   }
   return ret;
}

/* 
 * Put vtape in offline mode
 */
int vtape::offline()
{
   close();
   
   atEOF = false;               /* End of file */
   atEOT = false;               /* End of tape */
   atEOD = false;               /* End of data */
   atBOT = false;               /* Begin of tape */
   online = false;

   file_block = 0;
   current_file = -1;
   current_block = -1;
   last_file = -1;
   return 0;
}

/* A filemark is automatically written to tape if the last tape operation
 * before close was a write.
 */
int vtape::d_close(int)
{
   check_eof();
   ::close(fd);
   fd = -1;
   return 0;
}

/*
 * When a filemark is encountered while reading, the following happens.  If
 * there are data remaining in the buffer when the filemark is found, the
 * buffered data is returned.  The next read returns zero bytes.  The following
 * read returns data from the next file.  The end of recorded data is signaled
 * by returning zero bytes for two consecutive read calls.  The third read
 * returns an error.
 */
ssize_t vtape::d_read(int, void *buffer, size_t count)
{
   ASSERT(online);
   ASSERT(current_file >= 0);
   ssize_t nb;
   uint32_t s;
   
   Dmsg2(dbglevel*2, "read %i:%i\n", current_file, current_block);

   if (atEOT || atEOD) {
      errno = EIO;
      return -1;
   }

   if (atEOF) {
      if (!next_FM) {
         atEOD = true;
         atEOF = false;
         current_block=-1;
         return 0;
      }
      atEOF=false;
   }

   check_eof();

   atEOD = atBOT = false;

   /* reading size of data */
   nb = ::read(fd, &s, sizeof(uint32_t));
   if (nb <= 0) {
      atEOF = true;             /* TODO: check this */
      return 0;
   }

   if (s > count) {             /* not enough buffer to read block */
      Dmsg2(dbglevel, "Need more buffer to read next block %i > %i\n",s,count);
      lseek(fd, s, SEEK_CUR);
      errno = ENOMEM;
      return -1;
   }

   if (!s) {                    /* EOF */
      atEOF = true;
      if (read_fm(VT_SKIP_EOF)) {
         current_file++;
      }

      return 0;
   }

   /* reading data itself */
   nb = ::read(fd, buffer, s);
   if (nb != (ssize_t)s) { /* read error */
      errno=EIO;
      atEOT=true;
      current_block = -1;
      Dmsg0(dbglevel, "EOT during reading\n");
      return -1;
   }                    /* read ok */

   if (current_block >= 0) {
      current_block++;
   }

   return nb;
}

int vtape::d_open(const char *pathname, int uflags)
{
   Dmsg2(dbglevel, "vtape::d_open(%s, %i)\n", pathname, uflags);

   online = true;               /* assume that drive contains a tape */

   struct stat statp;   
   if (stat(pathname, &statp) != 0) {
      fd = -1;
      Dmsg1(dbglevel, "Can't stat on %s\n", pathname);
      if (uflags & O_NONBLOCK) {
         online = false;
         fd = ::open("/dev/null", O_CREAT | O_RDWR | O_LARGEFILE, 0600);
      }
   } else {
      fd = ::open(pathname, O_CREAT | O_RDWR | O_LARGEFILE, 0600);
   }

   if (fd < 0) {
      errno = ENOMEDIUM;
      return -1;
   }

   file_block = 0;
   current_block = 0;
   current_file = 0;
   cur_FM = next_FM = last_FM = 0;
   needEOF = false;
   atBOT = true;
   atEOT = atEOD = false;

   /* If the vtape is empty, start by writing a EOF */
   if (online && !read_fm(VT_READ_EOF)) {
      lseek(fd, 0, SEEK_SET);          /* rewind */
      cur_FM = next_FM = last_FM = 0;  /* reset */
      weof();                          /* write the first EOF */
      last_file = current_file=0;
   }

   return fd;
}

/* use this to track file usage */
void vtape::update_pos()
{
   ASSERT(online);
   struct stat statp;
   if (fstat(fd, &statp) == 0) {
      file_block = statp.st_blocks;
   } 

   Dmsg1(dbglevel*2, "update_pos=%i\n", file_block);

   if (file_block > max_block) {
      atEOT = true;
   } else {
      atEOT = false;
   }
}

void vtape::dump()
{
   Dmsg0(dbglevel+1, "===================\n");
   Dmsg2(dbglevel, "file:block = %i:%i\n", current_file, current_block);
   Dmsg1(dbglevel+1, "last_file=%i\n", last_file);
   Dmsg1(dbglevel+1, "file_block=%i\n", file_block);  
   Dmsg4(dbglevel+1, "EOF=%i EOT=%i EOD=%i BOT=%i\n", 
         atEOF, atEOT, atEOD, atBOT);  
}

#endif  /* ! USE_VTAPE */
