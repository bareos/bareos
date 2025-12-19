/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, November MM
/**
 * @file
 * Create a file, and reset the modes
 */

#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/jcr.h"
#include "find.h"
#include "findlib/makepath.h"
#include "findlib/create_file.h"
#include "lib/attr.h"
#include "lib/path_list.h"
#include "lib/btimers.h"
#include "lib/berrno.h"

#ifndef S_IRWXUGO
#  define S_IRWXUGO (S_IRWXU | S_IRWXG | S_IRWXO)
#endif

#ifndef IS_CTG
#  define IS_CTG(x) 0
#  define O_CTG 0
#endif

static int SeparatePathAndFile(JobControlRecord* jcr, char* fname, char* ofile);
static int PathAlreadySeen(JobControlRecord* jcr, char* path, int pnl);

/**
 * Create the file, or the directory
 *
 * fname is the original filename
 * ofile is the output filename (may be in a different directory)
 *
 * Returns:  CF_SKIP     if file should be skipped
 *           CF_ERROR    on error
 *           CF_EXTRACT  file created and data to restore
 *           CF_CREATED  file created no data to restore
 *
 * Note, we create the file here, except for special files,
 * we do not set the attributes because we want to first
 * write the file, then when the writing is done, set the
 * attributes.
 *
 * So, we return with the file descriptor open for normal files.
 */
int CreateFile(JobControlRecord* jcr,
               Attributes* attr,
               BareosFilePacket* bfd,
               int replace)
{
  mode_t new_mode, parent_mode;
  int flags;
  uid_t uid;
  gid_t gid;
  int pnl;
  bool exists = false;
  struct stat mstatp;
#ifndef HAVE_WIN32
  bool isOnRoot;
#endif

  bfd->reparse_point = false;
  if (is_win32_stream(attr->data_stream)) {
    set_win32_backup(bfd);
  } else {
    SetPortableBackup(bfd);
  }

  new_mode = attr->statp.st_mode;
  Dmsg3(200, "type=%d newmode=%04o file=%s\n", attr->type, (new_mode & ~S_IFMT),
        attr->ofname);
  parent_mode = S_IWUSR | S_IXUSR | new_mode;
  gid = attr->statp.st_gid;
  uid = attr->statp.st_uid;

#ifdef HAVE_WIN32
  if (!bfd->use_backup_api) {
    // Eliminate invalid windows filename characters from foreign filenames
    char* ch = (char*)attr->ofname;
    if (ch[0] != 0 && ch[1] != 0) {
      ch += 2;
      while (*ch) {
        switch (*ch) {
          case ':':
          case '<':
          case '>':
          case '*':
          case '?':
          case '|':
            *ch = '_';
            break;
        }
        ch++;
      }
    }
  }
#endif

  Dmsg2(400, "Replace=%c %d\n", (char)replace, replace);
  if (lstat(attr->ofname, &mstatp) == 0) {
    exists = true;
    switch (replace) {
      case REPLACE_IFNEWER:
        if (attr->statp.st_mtime <= mstatp.st_mtime) {
          Qmsg(jcr, M_INFO, 0, T_("File skipped. Not newer: %s\n"),
               attr->ofname);
          return CF_SKIP;
        }
        break;
      case REPLACE_IFOLDER:
        if (attr->statp.st_mtime >= mstatp.st_mtime) {
          Qmsg(jcr, M_INFO, 0, T_("File skipped. Not older: %s\n"),
               attr->ofname);
          return CF_SKIP;
        }
        break;
      case REPLACE_NEVER:
        // Set attributes if we created this directory
        if (attr->type == FT_DIREND
            && PathListLookup(jcr->path_list, attr->ofname)) {
          break;
        }
        Qmsg(jcr, M_INFO, 0, T_("File skipped. Already exists: %s\n"),
             attr->ofname);
        return CF_SKIP;
      case REPLACE_ALWAYS:
        break;
    }
  }

  switch (attr->type) {
    case FT_RAW:      /* Raw device to be written */
    case FT_FIFO:     /* FIFO to be written to */
    case FT_LNKSAVED: /* Hard linked, file already saved */
    case FT_LNK:
    case FT_SPEC: /* Fifo, ... to be backed up */
    case FT_REGE: /* Empty file */
    case FT_REG:  /* Regular file */
      /* Note, we do not delete FT_RAW because these are device files
       * or FIFOs that should already exist. If we blow it away,
       * we may blow away a FIFO that is being used to read the
       * restore data, or we may blow away a partition definition. */
      if (exists && attr->type != FT_RAW && attr->type != FT_FIFO) {
        /* Get rid of old copy */
        Dmsg1(400, "unlink %s\n", attr->ofname);
        if (SecureErase(jcr, attr->ofname) == -1) {
          BErrNo be;

          Qmsg(
              jcr, M_ERROR, 0,
              T_("File %s already exists and could not be replaced. ERR=%s.\n"),
              attr->ofname, be.bstrerror());
          /* Continue despite error */
        }
      }

      /* Here we do some preliminary work for all the above
       *   types to create the path to the file if it does
       *   not already exist.  Below, we will split to
       *   do the file type specific work */
      pnl = SeparatePathAndFile(jcr, attr->fname, attr->ofname);
      if (pnl < 0) { return CF_ERROR; }

      /* If path length is <= 0 we are making a file in the root
       *  directory. Assume that the directory already exists. */
      if (pnl > 0) {
        char savechr;
        savechr = attr->ofname[pnl];
        attr->ofname[pnl] = 0; /* Terminate path */

        if (!PathAlreadySeen(jcr, attr->ofname, pnl)) {
          Dmsg1(400, "Make path %s\n", attr->ofname);
          /* If we need to make the directory, ensure that it is with
           * execute bit set (i.e. parent_mode), and preserve what already
           * exists. Normally, this should do nothing. */
          if (!makepath(attr, attr->ofname, parent_mode, parent_mode, uid, gid,
                        1)) {
            Dmsg1(10, "Could not make path. %s\n", attr->ofname);
            attr->ofname[pnl] = savechr; /* restore full name */
            return CF_ERROR;
          }
        }
        attr->ofname[pnl] = savechr; /* restore full name */
      }

      // Now we do the specific work for each file type
      switch (attr->type) {
        case FT_REGE:
        case FT_REG:
          Dmsg1(100, "Create=%s\n", attr->ofname);
          flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY; /*  O_NOFOLLOW; */
          if (IS_CTG(attr->statp.st_mode)) {
            flags |= O_CTG; /* set contiguous bit if needed */
          }

          if (IsBopen(bfd)) {
            Qmsg1(jcr, M_ERROR, 0, T_("bpkt already open filedes=%d\n"),
                  Bgetfd(bfd));
            bclose(bfd);
          }

          if (bopen(bfd, attr->ofname, flags, 0, attr->statp.st_rdev) < 0) {
            BErrNo be;

            be.SetErrno(bfd->BErrNo);
            Qmsg2(jcr, M_ERROR, 0, T_("Could not create %s: ERR=%s\n"),
                  attr->ofname, be.bstrerror());
            Dmsg2(100, "Could not create %s: ERR=%s\n", attr->ofname,
                  be.bstrerror());

            return CF_ERROR;
          }

          return CF_EXTRACT;

#ifndef HAVE_WIN32    /* None of these exist in MS Windows */
        case FT_RAW:  /* Bareos raw device e.g. /dev/sda1 */
        case FT_FIFO: /* Bareos fifo to save data */
        case FT_SPEC:
          flags = O_WRONLY | O_BINARY;

          isOnRoot = bstrcmp(attr->fname, attr->ofname) ? 1 : 0;
          if (S_ISFIFO(attr->statp.st_mode)) {
            Dmsg1(400, "Restore fifo: %s\n", attr->ofname);
            if (mkfifo(attr->ofname, attr->statp.st_mode) != 0
                && errno != EEXIST) {
              BErrNo be;
              Qmsg2(jcr, M_ERROR, 0, T_("Cannot make fifo %s: ERR=%s\n"),
                    attr->ofname, be.bstrerror());
              return CF_ERROR;
            }
          } else if (S_ISSOCK(attr->statp.st_mode)) {
            Dmsg1(200, "Skipping restore of socket: %s\n", attr->ofname);
#  ifdef S_IFDOOR /* Solaris high speed RPC mechanism */
          } else if (S_ISDOOR(attr->statp.st_mode)) {
            Dmsg1(200, "Skipping restore of door file: %s\n", attr->ofname);
#  endif
#  ifdef S_IFPORT /* Solaris event port for handling AIO */
          } else if (S_ISPORT(attr->statp.st_mode)) {
            Dmsg1(200, "Skipping restore of event port file: %s\n",
                  attr->ofname);
#  endif
          } else if ((S_ISBLK(attr->statp.st_mode)
                      || S_ISCHR(attr->statp.st_mode))
                     && !exists && isOnRoot) {
            /* Fatal: Restoring a device on root-file system, but device node
             * does not exist. Should not create a dump file. */
            Qmsg1(jcr, M_ERROR, 0,
                  T_("Device restore on root failed, device %s missing.\n"),
                  attr->fname);
            return CF_ERROR;
          } else if (S_ISBLK(attr->statp.st_mode)
                     || S_ISCHR(attr->statp.st_mode)) {
            Dmsg1(400, "Restoring a device as a file: %s\n", attr->ofname);
            flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
          } else {
            Dmsg1(400, "Restore node: %s\n", attr->ofname);
            if (mknod(attr->ofname, attr->statp.st_mode, attr->statp.st_rdev)
                    != 0
                && errno != EEXIST) {
              BErrNo be;
              Qmsg2(jcr, M_ERROR, 0, T_("Cannot make node %s: ERR=%s\n"),
                    attr->ofname, be.bstrerror());
              return CF_ERROR;
            }
          }

          /* Here we are going to attempt to restore to a FIFO, which
           * means that the FIFO must already exist, AND there must
           * be some process already attempting to read from the
           * FIFO, so we open it write-only. */
          if (attr->type == FT_RAW || attr->type == FT_FIFO) {
            btimer_t* tid;
            Dmsg1(400, "FT_RAW|FT_FIFO %s\n", attr->ofname);
            // Timeout open() in 60 seconds
            if (attr->type == FT_FIFO) {
              Dmsg0(400, "Set FIFO timer\n");
              tid = StartThreadTimer(jcr, pthread_self(), 60);
            } else {
              tid = NULL;
            }
            if (IsBopen(bfd)) {
              Qmsg1(jcr, M_ERROR, 0, T_("bpkt already open filedes=%d\n"),
                    Bgetfd(bfd));
            }
            Dmsg2(400, "open %s flags=%08o\n", attr->ofname, flags);
            if ((bopen(bfd, attr->ofname, flags, 0, 0)) < 0) {
              BErrNo be;
              be.SetErrno(bfd->BErrNo);
              Qmsg2(jcr, M_ERROR, 0, T_("Could not open %s: ERR=%s\n"),
                    attr->ofname, be.bstrerror());
              Dmsg2(400, "Could not open %s: ERR=%s\n", attr->ofname,
                    be.bstrerror());
              StopThreadTimer(tid);
              return CF_ERROR;
            }
            StopThreadTimer(tid);
            return CF_EXTRACT;
          }
          Dmsg1(400, "FT_SPEC %s\n", attr->ofname);
          return CF_CREATED;

        case FT_LNKSAVED: /* Hard linked, file already saved */
          Dmsg2(130, "Hard link %s => %s\n", attr->ofname, attr->olname);
          if (link(attr->olname, attr->ofname) != 0) {
            BErrNo be;
#  if defined(HAVE_CHFLAGS) && !defined(__stub_chflags)
            struct stat s;

            /* If using BSD user flags, maybe has a file flag preventing this.
             * So attempt to disable, retry link, and reset flags.
             * Note that BSD securelevel may prevent disabling flag. */
            if (stat(attr->olname, &s) == 0 && s.st_flags != 0) {
              if (chflags(attr->olname, 0) == 0) {
                if (link(attr->olname, attr->ofname) != 0) {
                  // Restore original file flags even when linking failed
                  if (chflags(attr->olname, s.st_flags) < 0) {
                    Qmsg2(jcr, M_ERROR, 0,
                          T_("Could not restore file flags for file %s: "
                             "ERR=%s\n"),
                          attr->olname, be.bstrerror());
                  }
#  endif /* HAVE_CHFLAGS */
                  Qmsg3(jcr, M_ERROR, 0,
                        T_("Could not hard link %s -> %s: ERR=%s\n"),
                        attr->ofname, attr->olname, be.bstrerror());
                  Dmsg3(200, "Could not hard link %s -> %s: ERR=%s\n",
                        attr->ofname, attr->olname, be.bstrerror());
                  return CF_ERROR;
#  if defined(HAVE_CHFLAGS) && !defined(__stub_chflags)
                }
                // Finally restore original file flags
                if (chflags(attr->olname, s.st_flags) < 0) {
                  Qmsg2(
                      jcr, M_ERROR, 0,
                      T_("Could not restore file flags for file %s: ERR=%s\n"),
                      attr->olname, be.bstrerror());
                }
              } else {
                Qmsg2(jcr, M_ERROR, 0,
                      T_("Could not reset file flags for file %s: ERR=%s\n"),
                      attr->olname, be.bstrerror());
              }
            } else {
              Qmsg3(jcr, M_ERROR, 0,
                    T_("Could not hard link %s -> %s: ERR=%s\n"), attr->ofname,
                    attr->olname, be.bstrerror());
              return CF_ERROR;
            }
#  endif /* HAVE_CHFLAGS */
          }
          return CF_CREATED;
        case FT_LNK:
          // Unix/Linux symlink handling
          Dmsg2(130, "FT_LNK should restore: %s -> %s\n", attr->ofname,
                attr->olname);
          if (symlink(attr->olname, attr->ofname) != 0 && errno != EEXIST) {
            BErrNo be;
            Qmsg3(jcr, M_ERROR, 0, T_("Could not symlink %s -> %s: ERR=%s\n"),
                  attr->ofname, attr->olname, be.bstrerror());
            return CF_ERROR;
          }
          return CF_CREATED;
#else /* HAVE_WIN32 */
        case FT_LNK:
          /* Handle Windows Symlink-Like Reparse Points
           * - Directory Symlinks
           * - File Symlinks
           * - Volume Mount Points
           * - Junctions */
          Dmsg2(130, "FT_LNK should restore: %s -> %s\n", attr->ofname,
                attr->olname);
          if (attr->statp.st_rdev & FILE_ATTRIBUTE_VOLUME_MOUNT_POINT) {
            // We do not restore volume mount points
            Dmsg0(130, "Skipping Volume Mount Point\n");
            return CF_SKIP;
          }
          if (win32_symlink(attr->olname, attr->ofname, attr->statp.st_rdev)
                  != 0
              && errno != EEXIST) {
            BErrNo be;
            Qmsg3(jcr, M_ERROR, 0, T_("Could not symlink %s -> %s: ERR=%s\n"),
                  attr->ofname, attr->olname, be.bstrerror());
            return CF_ERROR;
          }
          return CF_CREATED;
        case FT_LNKSAVED: {
          Dmsg2(130, "Hard link %s => %s\n", attr->ofname, attr->olname);
          if (win32_link(attr->olname, attr->ofname) != 0) {
            BErrNo be;
            Qmsg3(jcr, M_ERROR, 0, T_("Could not hard link %s -> %s: ERR=%s\n"),
                  attr->ofname, attr->olname, be.bstrerror());
            Dmsg3(200, "Could not hard link %s -> %s: ERR=%s\n", attr->ofname,
                  attr->olname, be.bstrerror());
            return CF_ERROR;
          }
          return CF_CREATED;
        } break;

#endif /* HAVE_WIN32 */
      } /* End inner switch */
      Qmsg3(jcr, M_ERROR, 0, T_("unhandled type: %d\n"), attr->type);
      return CF_ERROR;
    case FT_REPARSE:
    case FT_JUNCTION:
      bfd->reparse_point = true;
      [[fallthrough]];
    case FT_DIRBEGIN:
    case FT_DIREND:
      Dmsg2(200, "Make dir mode=%04o dir=%s\n", (new_mode & ~S_IFMT),
            attr->ofname);
      if (!makepath(attr, attr->ofname, new_mode, parent_mode, uid, gid, 0)) {
        return CF_ERROR;
      }
      /* If we are using the Win32 Backup API, we open the directory so
       * that the security info will be read and saved.  */
      if (!IsPortableBackup(bfd)) {
        if (IsBopen(bfd)) {
          Qmsg1(jcr, M_ERROR, 0, T_("bpkt already open filedes=%d\n"),
                Bgetfd(bfd));
        }
        if (bopen(bfd, attr->ofname, O_WRONLY | O_BINARY, 0,
                  attr->statp.st_rdev)
            < 0) {
          BErrNo be;
          be.SetErrno(bfd->BErrNo);
#ifdef HAVE_WIN32
          // Check for trying to create a drive, if so, skip
          if (attr->ofname[1] == ':' && IsPathSeparator(attr->ofname[2])
              && attr->ofname[3] == '\0') {
            return CF_SKIP;
          }
#endif
          Qmsg2(jcr, M_ERROR, 0, T_("Could not open %s: ERR=%s\n"),
                attr->ofname, be.bstrerror());
          return CF_ERROR;
        }
        return CF_EXTRACT;
      } else {
        return CF_CREATED;
      }

    case FT_DELETED:
      Qmsg2(jcr, M_INFO, 0, T_("Original file %s have been deleted: type=%d\n"),
            attr->fname, attr->type);
      break;
    // The following should not occur
    case FT_NOACCESS:
    case FT_NOFOLLOW:
    case FT_NOSTAT:
    case FT_DIRNOCHG:
    case FT_NOCHG:
    case FT_ISARCH:
    case FT_NORECURSE:
    case FT_NOFSCHG:
    case FT_NOOPEN:
      Qmsg2(jcr, M_ERROR, 0, T_("Original file %s not saved: type=%d\n"),
            attr->fname, attr->type);
      break;
    default:
      Qmsg2(jcr, M_ERROR, 0, T_("Unknown file type %d; not restored: %s\n"),
            attr->type, attr->fname);
      break;
  }
  return CF_ERROR;
}

/**
 *  Returns: > 0 index into path where last path char is.
 *           0  no path
 *           -1 filename is zero length
 */
static int SeparatePathAndFile(JobControlRecord* jcr, char* fname, char* ofile)
{
  char *f, *p, *q;
  int fnl, pnl;

  /* Separate pathname and filename */
  for (q = p = f = ofile; *p; p++) {
#ifdef HAVE_WIN32
    if (IsPathSeparator(*p)) {
      f = q;
      if (IsPathSeparator(p[1])) { p++; }
    }
    *q++ = *p; /* copy data */
#else
    if (IsPathSeparator(*p)) { f = q; /* possible filename */ }
    q++;
#endif
  }

  if (IsPathSeparator(*f)) { f++; }
  *q = 0; /* Terminate string */

  fnl = q - f;
  if (fnl == 0) {
    /* The filename length must not be zero here because we
     *  are dealing with a file (i.e. FT_REGE or FT_REG).
     */
    Jmsg1(jcr, M_ERROR, 0, T_("Zero length filename: %s\n"), fname);
    return -1;
  }
  pnl = f - ofile - 1;
  return pnl;
}

/**
 * Primitive caching of path to prevent recreating a pathname
 *   each time as long as we remain in the same directory.
 */
static int PathAlreadySeen(JobControlRecord* jcr, char* path, int pnl)
{
  if (!jcr->cached_path) { jcr->cached_path = GetPoolMemory(PM_FNAME); }
  if (jcr->cached_pnl == pnl && bstrcmp(path, jcr->cached_path)) { return 1; }
  PmStrcpy(jcr->cached_path, path);
  jcr->cached_pnl = pnl;
  return 0;
}
