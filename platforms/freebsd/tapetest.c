/*
 *
 * Program to test loss of data at EOM on
 *  FreeBSD systems.
 *
 *	 Kern Sibbald, August 2003
 *
 *  If you build this program with:
 *
 *  c++ -g -O2 -Wall -c tapetest.c
 *  c++ -g -O2 -Wall tapetest.o -o tapetest
 *
 *  Procedure for testing tape
 *  ./tapetest /dev/your-tape-device
 *  rewind
 *  rawfill
 *  rewind
 *  scan
 *  quit
 *
 *  The output will be:
 * 
 * ========
 *  Rewound /dev/nsa0
 *  *Begin writing blocks of 64512 bytes.
 *  ++++++++++++++++++++ ...
 *  Write failed.  Last block written=17294. stat=0 ERR=Unknown error: 0
 *  weof_dev
 *  Wrote EOF to /dev/nsa0
 *  *Rewound /dev/nsa0
 *  *Starting scan at file 0
 *  17294 blocks of 64512 bytes in file 0
 *  End of File mark.
 *  End of File mark.
 *  End of tape
 *  Total files=1, blocks=17294, bytes = 1115670528
 * ========
 *
 *  which is correct. Notice that the return status is
 *  0, while in the example below, which fails, the return
 *  status is -1.
 *

 *  If you build this program with:
 *
 *  c++ -g -O2 -Wall -pthread -c tapetest.c
 *  c++ -g -O2 -Wall -pthread tapetest.o -o tapetest
 *    Note, we simply added -pthread compared to the
 *    previous example.
 *			
 *  Procedure for testing tape
 *  ./tapetest /dev/your-tape-device
 *  rewind
 *  rawfill
 *  rewind
 *  scan
 *  quit
 *
 *  The output will be:
 *
 * ========
 *    Rewound /dev/nsa0
 *    *Begin writing blocks of 64512 bytes.
 *    +++++++++++++++++++++++++++++ ...
 *    Write failed.  Last block written=17926. stat=-1 ERR=No space left on device
 *    weof_dev
 *    Wrote EOF to /dev/nsa0
 *    *Rewound /dev/nsa0
 *    *Starting scan at file 0
 *    17913 blocks of 64512 bytes in file 0
 *    End of File mark.
 *    End of File mark.
 *    End of tape
 *    Total files=1, blocks=17913, bytes = 1155603456
 * ========
 *
 * which is incroorect because it wrote 17,926 blocks but read
 * back only 17,913 blocks, AND because the return status on 
 * the last block written was -1 when it should have been
 * 0 (ie. stat=0 above).
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>
#include <sys/mtio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define FALSE 0
#define TRUE  1

#define dev_state(dev, state) ((dev)->state & (state))

/* Device state bits */
#define ST_OPENED	   (1<<0)     /* set when device opened */
#define ST_TAPE 	   (1<<1)     /* is a tape device */  
#define ST_FILE 	   (1<<2)     /* is a file device */
#define ST_FIFO 	   (1<<3)     /* is a fifo device */
#define ST_PROG 	   (1<<4)     /* is a program device */
#define ST_LABEL	   (1<<5)     /* label found */
#define ST_MALLOC          (1<<6)     /* dev packet malloc'ed in init_dev() */
#define ST_APPEND	   (1<<7)     /* ready for Bacula append */
#define ST_READ 	   (1<<8)     /* ready for Bacula read */
#define ST_EOT		   (1<<9)     /* at end of tape */
#define ST_WEOT 	   (1<<10)    /* Got EOT on write */
#define ST_EOF		   (1<<11)    /* Read EOF i.e. zero bytes */
#define ST_NEXTVOL	   (1<<12)    /* Start writing on next volume */
#define ST_SHORT	   (1<<13)    /* Short block read */

#define BLOCK_SIZE (512 * 126)


/* Exported variables */
int quit = 0;
char buf[100000];
int verbose = 0;
int debug_level = 0;
int fd = 0;

struct DEVICE {
   int fd;
   int dev_errno;
   int file;
   int block_num;
   int state;
   char *buf;
   int buf_len;
   char *dev_name;
   int file_addr;
};

DEVICE *dev;

#define uint32_t unsigned long
#define uint64_t unsigned long long
	    
/* Forward referenced subroutines */
static void do_tape_cmds();
static void helpcmd();
static void scancmd();
static void rewindcmd();
static void rawfill_cmd();


/* Static variables */

static char cmd[1000];

static void usage();
int get_cmd(char *prompt);


/*********************************************************************
 *
 *	   Main Bacula Pool Creation Program
 *
 */
int main(int argc, char *argv[])
{
   int ch;

   while ((ch = getopt(argc, argv, "d:v?")) != -1) {
      switch (ch) {
         case 'd':                    /* set debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0) {
	       debug_level = 1; 
	    }
	    break;

         case 'v':
	    verbose++;
	    break;

         case '?':
	 default:
	    helpcmd();
	    exit(0);

      }  
   }
   argc -= optind;
   argv += optind;


   /* See if we can open a device */
   if (argc == 0) {
      printf("No archive name specified.\n");
      usage();
      exit(1);
   } else if (argc != 1) {
      printf("Improper number of arguments specified.\n");
      usage();
      exit(1);
   }

   fd = open(argv[0], O_RDWR);	 
   if (fd < 0) {
      printf("Error opening %s ERR=%s\n", argv[0], strerror(errno));
      exit(1);
   }
   dev = (DEVICE *)malloc(sizeof(DEVICE));
   memset(dev, 0, sizeof(DEVICE));
   dev->fd = fd;
   dev->dev_name = strdup(argv[0]);
   dev->buf_len = BLOCK_SIZE;
   dev->buf = (char *)malloc(BLOCK_SIZE);

   do_tape_cmds();
   return 0;
}


int rewind_dev(DEVICE *dev)
{
   struct mtop mt_com;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      printf("Bad call to rewind_dev. Device %s not open\n",
	    dev->dev_name);
      return 0;
   }
   dev->state &= ~(ST_APPEND|ST_READ|ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   dev->block_num = dev->file = 0;
   mt_com.mt_op = MTREW;
   mt_com.mt_count = 1;
   if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
      dev->dev_errno = errno;
      printf("Rewind error on %s. ERR=%s.\n",
	       dev->dev_name, strerror(dev->dev_errno));
      return 0;
   }
   return 1;
}

/*
 * Write an end of file on the device
 *   Returns: 0 on success
 *	      non-zero on failure
 */
int 
weof_dev(DEVICE *dev, int num)
{ 
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      printf("Bad call to fsf_dev. Archive not open\n");
      return -1;
   }

   dev->state &= ~(ST_EOT | ST_EOF);  /* remove EOF/EOT flags */
   dev->block_num = 0;
   printf("weof_dev\n");
   mt_com.mt_op = MTWEOF;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      dev->file++;
      dev->file_addr = 0;
   } else {
      dev->dev_errno = errno;
      printf("ioctl MTWEOF error on %s. ERR=%s.\n",
	 dev->dev_name, strerror(dev->dev_errno));
   }
   return stat;
}





void quitcmd()
{
   quit = 1;
}


/*
 * Rewind the tape.   
 */
static void rewindcmd()
{
   if (!rewind_dev(dev)) {
      printf("Bad status from rewind. ERR=%s\n", strerror(dev->dev_errno));
   } else {
      printf("Rewound %s\n", dev->dev_name);
   }
}

/*
 * Write and end of file on the tape   
 */
static void weofcmd()
{
   int stat;

   if ((stat = weof_dev(dev, 1)) < 0) {
      printf("Bad status from weof %d. ERR=%s\n", stat, strerror(dev->dev_errno));
      return;
   } else {
      printf("Wrote EOF to %s\n", dev->dev_name);
   }
}


/* 
 * Read a record from the tape
 */
static void rrcmd()
{
   char *buf;
   int stat, len;

   if (!get_cmd("Enter length to read: ")) {
      return;
   }
   len = atoi(cmd);
   if (len < 0 || len > 1000000) {
      printf("Bad length entered, using default of 1024 bytes.\n");
      len = 1024;
   }
   buf = (char *)malloc(len);
   stat = read(fd, buf, len);
   if (stat > 0 && stat <= len) {
      errno = 0;
   }
   printf("Read of %d bytes gives stat=%d. ERR=%s\n",
      len, stat, strerror(errno));
   free(buf);
}

/* 
 * Write a record to the tape
 */
static void wrcmd()
{
   int stat;
   int rfd;

   rfd = open("/dev/urandom", O_RDONLY);
   if (rfd) {
      read(rfd, dev->buf, dev->buf_len);
   } else {
      printf("Cannot open /dev/urandom.\n");
      return;
   }
   printf("Write one block of %u bytes.\n", dev->buf_len);
   stat = write(dev->fd, dev->buf, dev->buf_len);
   if (stat != (int)dev->buf_len) {
      if (stat == -1) {
         printf("Bad status from write. ERR=%s\n", strerror(errno));
      } else {
         printf("Expected to write %d bytes but wrote only %d.\n",
	    dev->buf_len, stat);
      }
   }
}



/*
 * Scan tape by reading block by block. Report what is
 * on the tape.  Note, this command does raw reads, and as such
 * will not work with fixed block size devices.
 */
static void scancmd()
{
   int stat;
   int blocks, tot_blocks, tot_files;
   int block_size;
   uint64_t bytes;


   blocks = block_size = tot_blocks = 0;
   bytes = 0;
   if (dev->state & ST_EOT) {
      printf("End of tape\n");
      return; 
   }
   tot_files = dev->file;
   printf("Starting scan at file %u\n", dev->file);
   for (;;) {
      if ((stat = read(dev->fd, buf, sizeof(buf))) < 0) {
	 dev->dev_errno = errno;
         printf("Bad status from read %d. ERR=%s\n", stat, strerror(dev->dev_errno));
	 if (blocks > 0)
            printf("%d block%s of %d bytes in file %d\n",        
                    blocks, blocks>1?"s":"", block_size, dev->file);
	 return;
      }
      if (stat != block_size) {
	 if (blocks > 0) {
            printf("%d block%s of %d bytes in file %d\n", 
                 blocks, blocks>1?"s":"", block_size, dev->file);
	    blocks = 0;
	 }
	 block_size = stat;
      }
      if (stat == 0) {		      /* EOF */
         printf("End of File mark.\n");
	 /* Two reads of zero means end of tape */
	 if (dev->state & ST_EOF)
	    dev->state |= ST_EOT;
	 else {
	    dev->state |= ST_EOF;
	    dev->file++;
	 }
	 if (dev->state & ST_EOT) {
            printf("End of tape\n");
	    break;
	 }
      } else {			      /* Got data */
	 dev->state &= ~ST_EOF;
	 blocks++;
	 tot_blocks++;
	 bytes += stat;
      }
   }
   tot_files = dev->file - tot_files;
   printf("Total files=%d, blocks=%d, bytes = %d\n", tot_files, tot_blocks, 
      (int)bytes);			   
}


static void rawfill_cmd()
{
   int stat;
   int rfd;
   uint32_t block_num = 0;
   uint32_t *p;
   int my_errno;

   rfd = open("/dev/urandom", O_RDONLY);
   if (rfd) {
      read(rfd, dev->buf, dev->buf_len);
   } else {
      printf("Cannot open /dev/urandom.\n");
      return;
   }
   p = (uint32_t *)dev->buf;
   printf("Begin writing blocks of %u bytes.\n", dev->buf_len);
   for ( ;; ) {
      *p = block_num;
      stat = write(dev->fd, dev->buf, dev->buf_len);
      if (stat == (int)dev->buf_len) {
	 if ((block_num++ % 100) == 0) {
            printf("+");
	    fflush(stdout);
	 }
	 continue;
      }
      break;
   }
   my_errno = errno;
   printf("\n");
   weofcmd();
   printf("Write failed.  Last block written=%d. stat=%d ERR=%s\n", (int)block_num, stat,
      strerror(my_errno));

}

/* Strip any trailing junk from the command */
void strip_trailing_junk(char *cmd)
{
   char *p;
   p = cmd + strlen(cmd) - 1;

   /* strip trailing junk from command */
   while ((p >= cmd) && (*p == '\n' || *p == '\r' || *p == ' '))
      *p-- = 0;
}

/* folded search for string - case insensitive */
int
fstrsch(char *a, char *b)   /* folded case search */
{
   register char *s1,*s2;
   register char c1=0, c2=0;

   s1=a;
   s2=b;
   while (*s1) {		      /* do it the fast way */
      if ((*s1++ | 0x20) != (*s2++ | 0x20))
	 return 0;		      /* failed */
   }
   while (*a) { 		      /* do it over the correct slow way */
      if (isupper(c1 = *a)) {
	 c1 = tolower((int)c1);
      }
      if (isupper(c2 = *b)) {
	 c2 = tolower((int)c2);
      }
      if (c1 != c2) {
	 return 0;
      }
      a++;
      b++;
   }
   return 1;
}
   

struct cmdstruct { char *key; void (*func)(); char *help; }; 
static struct cmdstruct commands[] = {
 {"help",       helpcmd,      "print this command"},
 {"quit",       quitcmd,      "quit tapetest"},   
 {"rawfill",    rawfill_cmd,  "use write() to fill tape"},
 {"rewind",     rewindcmd,    "rewind the tape"},
 {"rr",         rrcmd,        "raw read the tape"},
 {"wr",         wrcmd,        "raw write one block to the tape"},
 {"scan",       scancmd,      "read() tape block by block to EOT and report"}, 
 {"weof",       weofcmd,      "write an EOF on the tape"},
	     };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))

static void
do_tape_cmds()
{
   unsigned int i;
   int found;

   while (get_cmd("*")) {
      found = 0;
      for (i=0; i<comsize; i++)       /* search for command */
	 if (fstrsch(cmd,  commands[i].key)) {
	    (*commands[i].func)();    /* go execute command */
	    found = 1;
	    break;
	 }
      if (!found) {
         printf("%s is an illegal command\n", cmd);
      }
      if (quit) {
	 break;
      }
   }
}

static void helpcmd()
{
   unsigned int i;
   usage();
   printf("  Command    Description\n  =======    ===========\n");
   for (i=0; i<comsize; i++)
      printf("  %-10s %s\n", commands[i].key, commands[i].help);
   printf("\n");
}

static void usage()
{
   fprintf(stderr,
"Usage: tapetest [-d debug_level] [device_name]\n"
"       -dnn        set debug level to nn\n"
"       -?          print this message.\n"  
"\n");

}



/*	
 * Get next input command from terminal.  This
 * routine is REALLY primitive, and should be enhanced
 * to have correct backspacing, etc.
 */
int 
get_cmd(char *prompt)
{
   int i = 0;
   int ch;
   fprintf(stdout, prompt);

   /* We really should turn off echoing and pretty this
    * up a bit.
    */
   cmd[i] = 0;
   while ((ch = fgetc(stdin)) != EOF) { 
      if (ch == '\n') {
	 strip_trailing_junk(cmd);
	 return 1;
      } else if (ch == 4 || ch == 0xd3 || ch == 0x8) {
	 if (i > 0)
	    cmd[--i] = 0;
	 continue;
      } 
	 
      cmd[i++] = ch;
      cmd[i] = 0;
   }
   quit = 1;
   return 0;
}
