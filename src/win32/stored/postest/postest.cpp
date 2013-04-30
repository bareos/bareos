#ifdef HAVE_WIN32
#include <windows.h>
typedef int __daddr_t;
#else
#define tape_open open
#define tape_write write
#define tape_ioctl ioctl
#define tape_close close

typedef  unsigned char  UCHAR, *PUCHAR;
typedef  unsigned int   UINT, *PUINT;
typedef  unsigned long  ULONG, *PULONG;
typedef  unsigned long long   ULONGLONG, *PULONGLONG;
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/mtio.h>
#include <errno.h>
#include <string.h>

char *szCommands[] =
{
   "MTRESET",
   "MTFSF",
   "MTBSF",
   "MTFSR",
   "MTBSR",
   "MTWEOF",
   "MTREW",
   "MTOFFL",
   "MTNOP",
   "MTRETEN",
   "MTBSFM",
   "MTFSFM ",
   "MTEOM",
   "MTERASE",
   "MTRAS1",
   "MTRAS2",
   "MTRAS3",
   "UNKNOWN_17",
   "UNKNOWN_18",
   "UNKNOWN_19",
   "MTSETBLK",
   "MTSETDENSITY",
   "MTSEEK",
   "MTTELL",
   "MTSETDRVBUFFER",
   "MTFSS",
   "MTBSS",
   "MTWSM",
   "MTLOCK",
   "MTUNLOCK",
   "MTLOAD",
   "MTUNLOAD",
   "MTCOMPRESSION",
   "MTSETPART",
   "MTMKPART",
};

#define NUMBER_COMMANDS (sizeof(szCommands) / sizeof(szCommands[0]))

typedef  struct _SCRIPT_ENTRY {
   short       Command;
   int         Count;
   PUCHAR      pszDescription;
   ULONG       ExpectedFile;
   ULONGLONG   ExpectedBlock;
}  SCRIPT_ENTRY, *PSCRIPT_ENTRY;

SCRIPT_ENTRY   TestScript[] = 
{
   { MTREW, 1, 0, 0 },
   { MTFSF, 2, 0, 0 },
   { MTBSR, 1, 0, 0 },
   { MTBSR, 3, 0, 0 },
   { MTFSR, 6, 0, 0 },
   { MTREW, 1, 0, 0 },
   { MTFSF, 3, 0, 0 },
   { MTFSR, 8, 0, 0 },
   { MTFSF, 1, 0, 0 },
   { MTBSF, 1, 0, 0 }
};

#define SCRIPT_LENGTH (sizeof(TestScript) / sizeof(TestScript[0]))

void printpos(int fd, ULONG ulFile, ULONG ulBlock);

void
run_script(int fd, PSCRIPT_ENTRY entries, size_t count)
{
   mtop  op;

   for (size_t idxScript = 0; idxScript < count; idxScript++)
   {
      PSCRIPT_ENTRY   pEntry = &entries[idxScript];

      fprintf(stderr, "%s %d: ", szCommands[pEntry->Command], pEntry->Count);

      op.mt_op = pEntry->Command;
      op.mt_count = pEntry->Count;

      int iResult = tape_ioctl(fd, MTIOCTOP, &op);

      if (iResult >= 0)
      {
         printpos(fd, pEntry->ExpectedFile, (ULONG)pEntry->ExpectedBlock);
      }
      else
      {
         fprintf(stderr, "tape_ioctl returned %d, error = %s\n", errno, strerror(errno));
      }
   }
}

void
weof(int fd)
{
   mtop   op;

   op.mt_op = MTWEOF;
   op.mt_count = 1;

   if (tape_ioctl(fd, MTIOCTOP, &op) != 0)
   {
      fprintf(stderr, "tape_ioctl return error %d - %s", errno, strerror(errno));
   }
}

void
wdata(int fd, ULONG ulBufferNumber, void *pBuffer, size_t size)
{
   ((PUCHAR)pBuffer)[0] = (UCHAR)ulBufferNumber;
   ((PUCHAR)pBuffer)[1] = (UCHAR)(ulBufferNumber >> 8);
   ((PUCHAR)pBuffer)[2] = (UCHAR)(ulBufferNumber >> 16);
   ((PUCHAR)pBuffer)[3] = (UCHAR)(ulBufferNumber >> 24);

   UCHAR    ucChar = (UCHAR)ulBufferNumber;
   UCHAR    ucIncrement = (UCHAR)(ulBufferNumber >> 8);

   if (ucIncrement == 0)
   {
      ucIncrement++;
   }

   for (size_t index = 4; index < size; index++)
   {
      ((PUCHAR)pBuffer)[index] = ucChar;
      ucChar += ucIncrement;
   }

   
   if (tape_write(fd, pBuffer, (UINT)size) < 0)
   {
      fprintf(stderr, "tape_write returned error %d - %s", errno, strerror(errno));
   }
}

void
printpos(int fd, ULONG ulExpectedFile, ULONG ulExpectedBlock)
{
   mtget  st;

   tape_ioctl(fd, MTIOCGET, &st);
   if (tape_ioctl(fd, MTIOCGET, &st) != 0)
   {
      fprintf(stderr, "tape_ioctl(MTIOCGET) returned error %d - %s\n", errno, strerror(errno));
   }

   mtpos pos;

   if (tape_ioctl(fd, MTIOCPOS, &pos) != 0)
   {
      fprintf(stderr, "tape_ioctl(MTIOCPOS) returned error %d - %s\n", errno, strerror(errno));
   }

   fprintf( stderr, "File = %d s/b %d, Block = %d, s/b %d, Absolute = %d, Flags =%s%s%s%s%s%s%s%s\n", 
      st.mt_fileno, ulExpectedFile, st.mt_blkno, ulExpectedBlock, pos.mt_blkno, 
      GMT_EOF(st.mt_gstat) ? " EOF" : "",
      GMT_BOT(st.mt_gstat) ? " BOT" : "",
      GMT_EOT(st.mt_gstat) ? " EOT" : "",
      GMT_EOD(st.mt_gstat) ? " EOD" : "",
      GMT_WR_PROT(st.mt_gstat) ? " WR_PROT" : "",
      GMT_ONLINE(st.mt_gstat) ? " ONLINE" : "",
      GMT_DR_OPEN(st.mt_gstat) ? " DR_OPEN" : "",
      GMT_IM_REP_EN(st.mt_gstat) ? " IM_REP_EN" : "");
}

void
rewind(int fd)
{
   mtop  op;

   op.mt_op = MTREW;
   op.mt_count = 1;

   if (tape_ioctl(fd, MTIOCTOP, &op) != 0)
   {
      fprintf(stderr, "tape_ioctl return error %d - %s", errno, strerror(errno));
   }
}

#define  BLOCK_SIZE  32768

int
main(int argc, char **argv)
{
   PUCHAR pBuffer;
   ULONG ulBlockNumber = 0;
   ULONG filenumber = 0;
   int index;

   OSDependentInit();

   int fd = tape_open(argv[1], O_RDWR, 0);

   if (fd == -1)
   {
      fprintf(stderr, "tape_open return error %d - %s", errno, strerror(errno));
      exit(1);
   }
   pBuffer = (PUCHAR)malloc(BLOCK_SIZE);

   rewind(fd);

   printpos(fd, 0, 0);

   fprintf(stderr, "file = %d, first block = %d\n", filenumber, ulBlockNumber);

   for (index = 0; index < 10; index++)
   {
      wdata(fd, ulBlockNumber++, pBuffer, BLOCK_SIZE);
   }

   weof(fd);
   filenumber++;
   ulBlockNumber++;

   fprintf(stderr, "file = %d, first block = %d\n", filenumber, ulBlockNumber);

   for (index = 0; index < 5; index++)
   {
      wdata(fd, ulBlockNumber++, pBuffer, BLOCK_SIZE);
   }

   weof(fd);
   filenumber++;
   ulBlockNumber++;

   fprintf(stderr, "file = %d, first block = %d\n", filenumber, ulBlockNumber);

      for (index = 0; index < 11; index++)
   {
      wdata(fd, ulBlockNumber++, pBuffer, BLOCK_SIZE);
   }

   weof(fd);
   filenumber++;
   ulBlockNumber++;

   fprintf(stderr, "file = %d, first block = %d\n", filenumber, ulBlockNumber);

   for (index = 0; index < 8; index++)
   {
      wdata(fd, ulBlockNumber++, pBuffer, BLOCK_SIZE);
   }

   weof(fd);
   filenumber++;
   ulBlockNumber++;

   fprintf(stderr, "file = %d, first block = %d\n", filenumber, ulBlockNumber);
   for (index = 0; index < 12; index++)
   {
      wdata(fd, ulBlockNumber++, pBuffer, BLOCK_SIZE);
   }

   weof(fd);
   filenumber++;
   ulBlockNumber++;

   fprintf(stderr, "file = %d, first block = %d\n", filenumber, ulBlockNumber);
   for (index = 0; index < 7; index++)
   {
      wdata(fd, ulBlockNumber++, pBuffer, BLOCK_SIZE);
   }

   weof(fd);
   filenumber++;
   ulBlockNumber++;

   run_script(fd, TestScript, SCRIPT_LENGTH);
   tape_close(fd);
   free(pBuffer);
   return 0;
}
