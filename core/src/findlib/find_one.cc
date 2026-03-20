/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
// Kern Sibbald, MM
/**
 * @file
 * This file was derived from GNU TAR source code. Except for a few key
 * ideas, it has been entirely rewritten for Bareos.
 *
 * Thanks to the TAR programmers.
 */

#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include <assert.h>
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/jcr.h"
#include "find.h"
#include "findlib/match.h"
#include "findlib/find_one.h"
#include "findlib/hardlink.h"
#include "findlib/fstype.h"
#include "findlib/drivetype.h"
#include "lib/berrno.h"
#ifdef HAVE_DARWIN_OS
#  include <sys/param.h>
#  include <sys/mount.h>
#  include <sys/attr.h>
#endif

extern int32_t name_max; /* filename max length */
extern int32_t path_max; /* path name max length */

/**
 * Create a new directory Find File packet, but copy
 * some of the essential info from the current packet.
 * However, be careful to zero out the rest of the
 * packet.
 */
static inline FindFilesPacket* new_dir_ff_pkt(FindFilesPacket* ff_pkt)
{
  FindFilesPacket* dir_ff_pkt;

  dir_ff_pkt = (FindFilesPacket*)malloc(sizeof(FindFilesPacket));

  *dir_ff_pkt = *ff_pkt;
  dir_ff_pkt->fname = strdup(ff_pkt->fname);
  dir_ff_pkt->link_or_dir = strdup(ff_pkt->link_or_dir);
  dir_ff_pkt->sys_fname = GetPoolMemory(PM_FNAME);
  dir_ff_pkt->included_files_list = NULL;
  dir_ff_pkt->excluded_files_list = NULL;
  dir_ff_pkt->excluded_paths_list = NULL;
  dir_ff_pkt->linkhash = NULL;
  dir_ff_pkt->fname_save = NULL;
  dir_ff_pkt->link_save = NULL;
  dir_ff_pkt->ignoredir_fname = NULL;

  return dir_ff_pkt;
}

// Free the temp directory ff_pkt
static void FreeDirFfPkt(FindFilesPacket* dir_ff_pkt)
{
  free(dir_ff_pkt->fname);
  free(dir_ff_pkt->link_or_dir);
  if (dir_ff_pkt->sys_fname) { FreePoolMemory(dir_ff_pkt->sys_fname); }
  if (dir_ff_pkt->fname_save) { FreePoolMemory(dir_ff_pkt->fname_save); }
  if (dir_ff_pkt->link_save) { FreePoolMemory(dir_ff_pkt->link_save); }
  if (dir_ff_pkt->ignoredir_fname) {
    FreePoolMemory(dir_ff_pkt->ignoredir_fname);
  }
  free(dir_ff_pkt);
}

/**
 * Check to see if we allow the file system type of a file or directory.
 * If we do not have a list of file system types, we accept anything.
 */
#if defined(HAVE_WIN32)
static bool AcceptFstype(FindFilesPacket*, void*) { return true; }
#else
static bool AcceptFstype(FindFilesPacket* ff, void*)
{
  int i;
  char fs[1000];
  bool accept = true;

  if (ff->fstypes.size()) {
    accept = false;
    if (!fstype(ff->fname, fs, sizeof(fs))) {
      Dmsg1(50, "Cannot determine file system type for \"%s\"\n", ff->fname);
    } else {
      for (i = 0; i < ff->fstypes.size(); ++i) {
        if (bstrcmp(fs, (char*)ff->fstypes.get(i))) {
          Dmsg2(100, "Accepting fstype %s for \"%s\"\n", fs, ff->fname);
          accept = true;
          break;
        }
        Dmsg3(200, "fstype %s for \"%s\" does not match %s\n", fs, ff->fname,
              ff->fstypes.get(i));
      }
    }
  }

  return accept;
}
#endif

/**
 * Check to see if we allow the drive type of a file or directory.
 * If we do not have a list of drive types, we accept anything.
 */
#if defined(HAVE_WIN32)
static inline bool AcceptDrivetype(FindFilesPacket* ff, void*)
{
  int i;
  char dt[100];
  bool accept = true;

  if (ff->drivetypes.size()) {
    accept = false;
    if (!Drivetype(ff->fname, dt, sizeof(dt))) {
      Dmsg1(50, "Cannot determine drive type for \"%s\"\n", ff->fname);
    } else {
      for (i = 0; i < ff->drivetypes.size(); ++i) {
        if (bstrcmp(dt, (char*)ff->drivetypes.get(i))) {
          Dmsg2(100, "Accepting drive type %s for \"%s\"\n", dt, ff->fname);
          accept = true;
          break;
        }
        Dmsg3(200, "drive type %s for \"%s\" does not match %s\n", dt,
              ff->fname, ff->drivetypes.get(i));
      }
    }
  }

  return accept;
}
#else
static inline bool AcceptDrivetype(FindFilesPacket*, void*) { return true; }
#endif

/**
 * This function determines whether we can use getattrlist()
 * It's odd, but we have to use the function to determine that...
 * Also, the man pages talk about things as if they were implemented.
 *
 * On Mac OS X, this successfully differentiates between HFS+ and UFS
 * volumes, which makes me trust it is OK for others, too.
 */

#ifdef HAVE_DARWIN_OS
static bool VolumeHasAttrlist(const char* fname)
{
  struct statfs st;
  struct volinfo_struct {
    unsigned long length;         /* Mandatory field */
    vol_capabilities_attr_t info; /* Volume capabilities */
  } vol;
  struct attrlist attrList;

  memset(&attrList, 0, sizeof(attrList));
  attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
  attrList.volattr = ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES;
  if (statfs(fname, &st) == 0) {
    // We need to check on the mount point
    if (getattrlist(st.f_mntonname, &attrList, &vol, sizeof(vol),
                    FSOPT_NOFOLLOW)
            == 0
        && (vol.info.capabilities[VOL_CAPABILITIES_INTERFACES]
            & VOL_CAP_INT_ATTRLIST)
        && (vol.info.valid[VOL_CAPABILITIES_INTERFACES]
            & VOL_CAP_INT_ATTRLIST)) {
      return true;
    }
  }
  return false;
}
#else
static bool VolumeHasAttrlist(const char*) { return false; }
#endif

// check for BSD nodump flag
#if defined(HAVE_CHFLAGS) && !defined(__stub_chflags) && defined(UF_NODUMP)
static inline bool no_dump(JobControlRecord* jcr, FindFilesPacket* ff_pkt)
{
  if (BitIsSet(FO_HONOR_NODUMP, ff_pkt->flags)
      && (ff_pkt->statp.st_flags & UF_NODUMP)) {
    Jmsg(jcr, M_INFO, 1, T_("     NODUMP flag set - will not process %s\n"),
         ff_pkt->fname);
    return true; /* do not backup this file */
  }
  return false; /* do backup */
}
#else
static inline bool no_dump(JobControlRecord*, FindFilesPacket*)
{
  return false;
}
#endif

// check for sizes
static inline bool CheckSizeMatching(JobControlRecord*, FindFilesPacket* ff_pkt)
{
  int64_t begin_size, end_size, difference;

  // See if size matching is turned on.
  if (!ff_pkt->size_match) { return true; }

  /* Loose the unsigned bits to keep the compiler from warning
   * about comparing signed and unsigned. As a size of a file
   * can only be positive the unsigned is not really to interesting. */
  begin_size = ff_pkt->size_match->begin_size;
  end_size = ff_pkt->size_match->end_size;

  // See what kind of matching should be done.
  switch (ff_pkt->size_match->type) {
    case size_match_approx:
      // Calculate the fraction this size is of the wanted size.
      if ((int64_t)ff_pkt->statp.st_size > begin_size) {
        difference = ff_pkt->statp.st_size - begin_size;
      } else {
        difference = begin_size - ff_pkt->statp.st_size;
      }

      // See if the difference is less then 1% of the total.
      return (difference < (begin_size / 100));
    case size_match_smaller:
      return (int64_t)ff_pkt->statp.st_size < begin_size;
    case size_match_greater:
      return (int64_t)ff_pkt->statp.st_size > begin_size;
    case size_match_range:
      return ((int64_t)ff_pkt->statp.st_size >= begin_size)
             && ((int64_t)ff_pkt->statp.st_size <= end_size);
    default:
      return true;
  }
}

// Check if a file have changed during backup and display an error
bool HasFileChanged(JobControlRecord* jcr, FindFilesPacket* ff_pkt)
{
  struct stat statp;
  Dmsg1(500, "HasFileChanged fname=%s\n", ff_pkt->fname);

  if (ff_pkt->type != FT_REG) { /* not a regular file */
    return false;
  }

  if (lstat(ff_pkt->fname, &statp) != 0) {
    BErrNo be;
    Jmsg(jcr, M_WARNING, 0, T_("Cannot stat file %s: ERR=%s\n"), ff_pkt->fname,
         be.bstrerror());
    return true;
  }

  if (statp.st_mtime != ff_pkt->statp.st_mtime) {
    Jmsg(jcr, M_ERROR, 0, T_("%s: mtime changed during backup.\n"),
         ff_pkt->fname);
    Dmsg3(50, "%s mtime (%lld) changed during backup (%lld).\n", ff_pkt->fname,
          static_cast<long long>(ff_pkt->statp.st_mtime),
          static_cast<long long>(statp.st_mtime));
    return true;
  }

  if (statp.st_ctime != ff_pkt->statp.st_ctime) {
    Jmsg(jcr, M_ERROR, 0, T_("%s: ctime changed during backup.\n"),
         ff_pkt->fname);
    Dmsg3(50, "%s ctime (%lld) changed during backup (%lld).\n", ff_pkt->fname,
          static_cast<long long>(ff_pkt->statp.st_ctime),
          static_cast<long long>(statp.st_ctime));
    return true;
  }

  if (statp.st_size != ff_pkt->statp.st_size) {
    /* TODO: add size change */
    Jmsg(jcr, M_ERROR, 0, T_("%s: size changed during backup.\n"),
         ff_pkt->fname);
    Dmsg3(50, "%s size (%lld) changed during backup (%lld).\n", ff_pkt->fname,
          static_cast<long long>(ff_pkt->statp.st_size),
          static_cast<long long>(statp.st_size));
    return true;
  }

  if ((statp.st_blksize != ff_pkt->statp.st_blksize)
      || (statp.st_blocks != ff_pkt->statp.st_blocks)) {
    Jmsg(jcr, M_ERROR, 0, T_("%s: size changed during backup.\n"),
         ff_pkt->fname);
    Dmsg3(50, "%s size (%lld) changed during backup (%lld).\n", ff_pkt->fname,
          static_cast<long long>(ff_pkt->statp.st_blocks),
          static_cast<long long>(statp.st_blocks));
    return true;
  }

  return false;
}

/**
 * For incremental/diffential or accurate backups, we
 * determine if the current file has changed.
 */
bool CheckChanges(JobControlRecord* jcr, FindFilesPacket* ff_pkt)
{
  /* In special mode (like accurate backup), the programmer can
   * choose his comparison function. */
  if (ff_pkt->CheckFct) { return ff_pkt->CheckFct(jcr, ff_pkt); }

  // For normal backups (incr/diff), we use this default behaviour
  if (ff_pkt->incremental
      && (ff_pkt->statp.st_mtime < ff_pkt->save_time
          && (BitIsSet(FO_MTIMEONLY, ff_pkt->flags)
              || ff_pkt->statp.st_ctime < ff_pkt->save_time))) {
    return false;
  }

  return true;
}

static inline bool HaveIgnoredir(FindFilesPacket* ff_pkt)
{
  struct stat sb;
  char* ignoredir;

  // Ensure that pointers are defined
  if (!ff_pkt->fileset || !ff_pkt->fileset->incexe) { return false; }

  for (int i = 0; i < ff_pkt->fileset->incexe->ignoredir.size(); i++) {
    ignoredir = (char*)ff_pkt->fileset->incexe->ignoredir.get(i);

    if (ignoredir) {
      if (!ff_pkt->ignoredir_fname) {
        ff_pkt->ignoredir_fname = GetPoolMemory(PM_FNAME);
      }
      Mmsg(ff_pkt->ignoredir_fname, "%s/%s", ff_pkt->fname, ignoredir);
      if (stat(ff_pkt->ignoredir_fname, &sb) == 0) {
        Dmsg2(100, "Directory '%s' ignored (found %s)\n", ff_pkt->fname,
              ignoredir);
        return true; /* Just ignore this directory */
      }
    }
  }

  return false;
}

// Restore file times.
static inline void RestoreFileTimes(FindFilesPacket* ff_pkt, char* fname)
{
#if !defined(HAVE_WIN32)
  struct timeval restore_times[2];

  restore_times[0].tv_sec = ff_pkt->statp.st_atime;
  restore_times[0].tv_usec = 0;
  restore_times[1].tv_sec = ff_pkt->statp.st_mtime;
  restore_times[1].tv_usec = 0;

  lutimes(fname, restore_times);
#else
  struct utimbuf restore_times;

  restore_times.actime = ff_pkt->statp.st_atime;
  restore_times.modtime = ff_pkt->statp.st_mtime;
  utime(fname, &restore_times);
#endif
}

#ifdef HAVE_DARWIN_OS
// Handling of a HFS+ attributes.
static inline int process_hfsattributes(JobControlRecord* jcr,
                                        FindFilesPacket* ff_pkt,
                                        int HandleFile(JobControlRecord* jcr,
                                                       FindFilesPacket* ff,
                                                       bool top_level),
                                        char* fname,
                                        bool top_level)
{
  // TODO: initialise attrList once elsewhere?
  struct attrlist attrList;

  memset(&attrList, 0, sizeof(attrList));
  attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
  attrList.commonattr = ATTR_CMN_FNDRINFO;
  attrList.fileattr = ATTR_FILE_RSRCLENGTH;
  if (getattrlist(fname, &attrList, &ff_pkt->hfsinfo, sizeof(ff_pkt->hfsinfo),
                  FSOPT_NOFOLLOW)
      != 0) {
    ff_pkt->type = FT_NOSTAT;
    ff_pkt->ff_errno = errno;
    return HandleFile(jcr, ff_pkt, top_level);
  }

  return -1;
}
#endif

// Handling of a hardlinked file.
static inline int process_hardlink(JobControlRecord* jcr,
                                   FindFilesPacket* ff_pkt,
                                   int HandleFile(JobControlRecord* jcr,
                                                  FindFilesPacket* ff,
                                                  bool top_level),
                                   char* fname,
                                   bool top_level,
                                   bool* done)
{
  int rtn_stat = 0;

  if (!ff_pkt->linkhash) { ff_pkt->linkhash = new LinkHash(10000); }

  auto [iter, inserted] = ff_pkt->linkhash->try_emplace(
      Hardlink{ff_pkt->statp.st_dev, ff_pkt->statp.st_ino}, fname);
  CurLink& hl = iter->second;

  if (hl.FileIndex == 0) {
    // no file backed up yet
    ff_pkt->linked = &hl;
    *done = false;
  } else if (bstrcmp(hl.name.c_str(), fname)) {
    // If we have already backed up the hard linked file don't do it again
    Dmsg2(400, "== Name identical skip FI=%d file=%s\n", hl.FileIndex, fname);
    *done = true;
    rtn_stat = 1; /* ignore */
  } else {
    // some other file was already backed up!
    ff_pkt->link_or_dir = hl.name.data();
    ff_pkt->type = FT_LNKSAVED; /* Handle link, file already saved */
    ff_pkt->LinkFI = hl.FileIndex;
    ff_pkt->linked = NULL;
    ff_pkt->digest = hl.digest.data();
    ff_pkt->digest_stream = hl.digest_stream;
    ff_pkt->digest_len = hl.digest.size();

    rtn_stat = HandleFile(jcr, ff_pkt, top_level);
    Dmsg3(400, "FT_LNKSAVED FI=%d LinkFI=%d file=%s\n", ff_pkt->FileIndex,
          hl.FileIndex, hl.name.c_str());
    *done = true;
  }

  return rtn_stat;
}

// Handling of a regular file.
static inline int process_regular_file(JobControlRecord* jcr,
                                       FindFilesPacket* ff_pkt,
                                       int HandleFile(JobControlRecord* jcr,
                                                      FindFilesPacket* ff,
                                                      bool top_level),
                                       char* fname,
                                       bool top_level)
{
  int rtn_stat;
  boffset_t sizeleft;

  sizeleft = ff_pkt->statp.st_size;

  /* Don't bother opening empty, world readable files. Also do not open
   * files when archive is meant for /dev/null. */
  if (sizeleft == 0 && MODE_RALL == (MODE_RALL & ff_pkt->statp.st_mode)) {
    ff_pkt->type = FT_REGE;
  } else {
    ff_pkt->type = FT_REG;
  }

  rtn_stat = HandleFile(jcr, ff_pkt, top_level);
  if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }

  Dmsg3(400, "FT_REG FI=%d linked=%d file=%s\n", ff_pkt->FileIndex,
        ff_pkt->linked ? 1 : 0, fname);

  if (BitIsSet(FO_KEEPATIME, ff_pkt->flags)) {
    RestoreFileTimes(ff_pkt, fname);
  }

  return rtn_stat;
}

// Handling of a symlink.
static inline int process_symlink(JobControlRecord* jcr,
                                  FindFilesPacket* ff_pkt,
                                  int HandleFile(JobControlRecord* jcr,
                                                 FindFilesPacket* ff,
                                                 bool top_level),
                                  char* fname,
                                  bool top_level)
{
  int rtn_stat;
  int size;

  assert(path_max + name_max + 102 > 0);
  char* buffer = (char*)alloca(path_max + name_max + 102);
  size = readlink(fname, buffer, path_max + name_max + 101);
  if (size < 0) {
    // Could not follow link
    ff_pkt->type = FT_NOFOLLOW;
    ff_pkt->ff_errno = errno;
    rtn_stat = HandleFile(jcr, ff_pkt, top_level);
    if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }
    return rtn_stat;
  }

  buffer[size] = 0;
  ff_pkt->link_or_dir = buffer; /* point to link */
  ff_pkt->type = FT_LNK;        /* got a real link */

  rtn_stat = HandleFile(jcr, ff_pkt, top_level);
  if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }

  return rtn_stat;
}

// Handling of a directory.
static inline int process_directory(JobControlRecord* jcr,
                                    FindFilesPacket* ff_pkt,
                                    int HandleFile(JobControlRecord* jcr,
                                                   FindFilesPacket* ff,
                                                   bool top_level),
                                    char* fname,
                                    dev_t parent_device,
                                    bool top_level)
{
  int rtn_stat;
  DIR* directory;
  struct dirent* result;
#ifdef USE_READDIR_R
  struct dirent* entry;
#endif
  char* link;
  int link_len;
  int len;
  dev_t our_device = ff_pkt->statp.st_dev;
  bool recurse = true;
  bool volhas_attrlist
      = ff_pkt->volhas_attrlist; /* Remember this if we recurse */

  /* Ignore this directory and everything below if the file .nobackup
   * (or what is defined for IgnoreDir in this fileset) exists */
  if (HaveIgnoredir(ff_pkt)) { return 1; /* Just ignore this directory */ }

  // Build a canonical directory name with a trailing slash in link var
  len = strlen(fname);
  link_len = len + 200;
  link = (char*)malloc(link_len + 2);
  bstrncpy(link, fname, link_len);

  // Strip all trailing slashes
  while (len >= 1 && IsPathSeparator(link[len - 1])) { len--; }
  link[len++] = '/'; /* add back one */
  link[len] = 0;

  ff_pkt->type = FT_DIREND;  // make sure CheckChanges knows its a directory
  ff_pkt->link_or_dir = link;
  if (!CheckChanges(jcr, ff_pkt)) {
    // Incremental/Full+Base option, directory entry not changed
    ff_pkt->type = FT_DIRNOCHG;
  } else {
    ff_pkt->type = FT_DIRBEGIN;
  }

  bool is_win32_mount_point = false;
#if defined(HAVE_WIN32)
  is_win32_mount_point
      = ff_pkt->statp.st_rdev & FILE_ATTRIBUTE_VOLUME_MOUNT_POINT;

  if (ff_pkt->statp.st_rdev & FILE_ATTRIBUTE_REPARSE_POINT) {
    ff_pkt->type = FT_REPARSE;
  }

  /* treat win32 mount points (Volume Mount Points) as directories */
  if (is_win32_mount_point) { ff_pkt->type = FT_DIRBEGIN; }
#endif

  /* Note, we return the directory to the calling program (HandleFile)
   * when we first see the directory (FT_DIRBEGIN.
   * This allows the program to apply matches and make a
   * choice whether or not to accept it.  If it is accepted, we
   * do not immediately save it, but do so only after everything
   * in the directory is seen (i.e. the FT_DIREND). */
  rtn_stat = HandleFile(jcr, ff_pkt, top_level);
  if (rtn_stat < 1 || ff_pkt->type == FT_REPARSE) { /* ignore or error status */
    free(link);

    return rtn_stat;
  }

  // Done with DIRBEGIN, next call will be DIREND
  if (ff_pkt->type == FT_DIRBEGIN) { ff_pkt->type = FT_DIREND; }

  /* Create a temporary ff packet for this directory
   * entry, and defer handling the directory until
   * we have recursed into it.  This saves the
   * directory after all files have been processed, and
   * during the restore, the directory permissions will
   * be reset after all the files have been restored. */
  Dmsg1(300, "Create temp ff packet for dir: %s\n", ff_pkt->fname);
  FindFilesPacket* dir_ff_pkt = new_dir_ff_pkt(ff_pkt);

  /* Do not descend into subdirectories (recurse) if the
   * user has turned it off for this directory.
   *
   * If we are crossing file systems, we are either not allowed
   * to cross, or we may be restricted by a list of permitted
   * file systems. */
  if (!top_level && BitIsSet(FO_NO_RECURSION, ff_pkt->flags)) {
    ff_pkt->type = FT_NORECURSE;
    recurse = false;
  } else if (!top_level
             && (parent_device != ff_pkt->statp.st_dev
                 || is_win32_mount_point)) {
    if (!BitIsSet(FO_MULTIFS, ff_pkt->flags)) {
      ff_pkt->type = FT_NOFSCHG;
      recurse = false;
    } else if (!AcceptFstype(ff_pkt, NULL)) {
      ff_pkt->type = FT_INVALIDFS;
      recurse = false;
    } else {
      ff_pkt->volhas_attrlist = VolumeHasAttrlist(fname);
    }
  }

  // If not recursing, just backup dir and return
  if (!recurse) {
    rtn_stat = HandleFile(jcr, ff_pkt, top_level);
    if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }
    free(link);
    FreeDirFfPkt(dir_ff_pkt);
    ff_pkt->link_or_dir = ff_pkt->fname; /* reset "link" */
    if (BitIsSet(FO_KEEPATIME, ff_pkt->flags)) {
      RestoreFileTimes(ff_pkt, fname);
    }
    return rtn_stat;
  }

  ff_pkt->link_or_dir = ff_pkt->fname; /* reset "link" */

  // Descend into or "recurse" into the directory to read all the files in it.
  errno = 0;
  if ((directory = opendir(fname)) == NULL) {
    ff_pkt->type = FT_NOOPEN;
    ff_pkt->ff_errno = errno;
    rtn_stat = HandleFile(jcr, ff_pkt, top_level);
    if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }
    free(link);
    FreeDirFfPkt(dir_ff_pkt);
    return rtn_stat;
  }

  /* Process all files in this directory entry (recursing).
   * This would possibly run faster if we chdir to the directory
   * before traversing it. */
  rtn_stat = 1;

  /* Allocate some extra room so an overflow of the d_name with more then
   * name_max bytes doesn't kill us right away. We check in the loop if
   * an overflow has not happened. */
#ifdef USE_READDIR_R
  int status;

  entry = (struct dirent*)malloc(sizeof(struct dirent) + name_max + 100);
  while (!jcr->IsJobCanceled()) {
    int name_length;

    status = Readdir_r(directory, entry, &result);
    if (status != 0 || result == NULL) { break; }

    name_length = (int)NAMELEN(entry);

    /* Some filesystems violate against the rules and return filenames
     * longer than _PC_NAME_MAX. Log the error and continue. */
    if ((name_max + 1) <= ((int)sizeof(struct dirent) + name_length)) {
      Jmsg2(jcr, M_ERROR, 0, T_("%s: File name too long [%d]\n"), entry->d_name,
            name_length);
      continue;
    }

    // Skip `.', `..', and excluded file names.
    if (entry->d_name[0] == '\0'
        || (entry->d_name[0] == '.'
            && (entry->d_name[1] == '\0'
                || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))) {
      continue;
    }

    // Make sure there is enough room to store the whole name.
    if (name_length + len >= link_len) {
      link_len = len + name_length + 1;
      link = (char*)realloc(link, link_len + 1);
    }

    memcpy(link + len, entry->d_name, name_length);
    link[len + name_length] = '\0';

    if (!FileIsExcluded(ff_pkt, link)) {
      rtn_stat = FindOneFile(jcr, ff_pkt, HandleFile, link, our_device, false);
    }
  }

  closedir(directory);
  free(link);
  free(entry);

#else

  while (!jcr->IsJobCanceled()) {
    int name_length;
    result = readdir(directory);
    if (result == NULL) { break; }

    name_length = (int)NAMELEN(result);

    /* Some filesystems violate against the rules and return filenames
     * longer than _PC_NAME_MAX. Log the error and continue. */
    if ((name_max + 1) <= ((int)sizeof(struct dirent) + name_length)) {
      Jmsg2(jcr, M_ERROR, 0, T_("%s: File name too long [%d]\n"),
            result->d_name, name_length);
      continue;
    }

    // Skip `.', `..', and excluded file names.
    if (result->d_name[0] == '\0'
        || (result->d_name[0] == '.'
            && (result->d_name[1] == '\0'
                || (result->d_name[1] == '.' && result->d_name[2] == '\0')))) {
      continue;
    }

    // Make sure there is enough room to store the whole name.
    if (name_length + len >= link_len) {
      link_len = len + name_length + 1;
      link = (char*)realloc(link, link_len + 1);
    }

    memcpy(link + len, result->d_name, name_length);
    link[len + name_length] = '\0';

    if (!FileIsExcluded(ff_pkt, link)) {
      rtn_stat = FindOneFile(jcr, ff_pkt, HandleFile, link, our_device, false);
      if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }
    }
  }

  closedir(directory);
  free(link);
#endif
  /* Now that we have recursed through all the files in the
   * directory, we "save" the directory so that after all
   * the files are restored, this entry will serve to reset
   * the directory modes and dates.  Temp directory values
   * were used without this record. */
  HandleFile(jcr, dir_ff_pkt, top_level); /* handle directory entry */
  if (dir_ff_pkt->linked) {
    dir_ff_pkt->linked->FileIndex = dir_ff_pkt->FileIndex;
  }
  FreeDirFfPkt(dir_ff_pkt);

  if (BitIsSet(FO_KEEPATIME, ff_pkt->flags)) {
    RestoreFileTimes(ff_pkt, fname);
  }
  ff_pkt->volhas_attrlist
      = volhas_attrlist; /* Restore value in case it changed. */

  return rtn_stat;
}

// Handling of a special file.
static inline int process_special_file(JobControlRecord* jcr,
                                       FindFilesPacket* ff_pkt,
                                       int HandleFile(JobControlRecord* jcr,
                                                      FindFilesPacket* ff,
                                                      bool top_level),
                                       char*,
                                       bool top_level)
{
  int rtn_stat;

  /* If it is explicitly mentioned (i.e. top_level) and is
   * a block device, we do a raw backup of it or if it is
   * a fifo, we simply read it. */
#ifdef HAVE_FREEBSD_OS
  /* On FreeBSD, all block devices are character devices, so
   * to be able to read a raw disk, we need the check for
   * a character device.
   *
   * crw-r----- 1 root  operator - 116, 0x00040002 Jun 9 19:32 /dev/ad0s3
   * crw-r----- 1 root  operator - 116, 0x00040002 Jun 9 19:32 /dev/rad0s3 */
  if (top_level
      && (S_ISBLK(ff_pkt->statp.st_mode) || S_ISCHR(ff_pkt->statp.st_mode))) {
#else
  if (top_level && S_ISBLK(ff_pkt->statp.st_mode)) {
#endif
    ff_pkt->type = FT_RAW; /* raw partition */
  } else if (top_level && S_ISFIFO(ff_pkt->statp.st_mode)
             && BitIsSet(FO_READFIFO, ff_pkt->flags)) {
    ff_pkt->type = FT_FIFO;
  } else {
    // The only remaining types are special (character, ...) files
    ff_pkt->type = FT_SPEC;
  }

  rtn_stat = HandleFile(jcr, ff_pkt, top_level);
  if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }

  return rtn_stat;
}

// See if we need to perform any processing for a given file.
static inline bool NeedsProcessing(JobControlRecord* jcr,
                                   FindFilesPacket* ff_pkt,
                                   char* fname)
{
  int loglevel = M_INFO;

  if (!AcceptFstype(ff_pkt, NULL)) {
    char fs[100];

    ff_pkt->type = FT_INVALIDFS;
    if (BitIsSet(FO_KEEPATIME, ff_pkt->flags)) {
      RestoreFileTimes(ff_pkt, fname);
    }

    if (!fstype(ff_pkt->fname, fs, sizeof(fs))) {
      bstrncpy(fs, "unknown", sizeof(fs));
      loglevel = M_WARNING;
    }

    Jmsg(jcr, loglevel, 0,
         T_("Top level directory \"%s\" has unlisted fstype \"%s\"\n"), fname,
         fs);
    return false; /* Just ignore this error - or the whole backup is cancelled
                   */
  }

  if (!AcceptDrivetype(ff_pkt, NULL)) {
    char dt[100];

    ff_pkt->type = FT_INVALIDDT;
    if (BitIsSet(FO_KEEPATIME, ff_pkt->flags)) {
      RestoreFileTimes(ff_pkt, fname);
    }

    if (!Drivetype(ff_pkt->fname, dt, sizeof(dt))) {
      bstrncpy(dt, "unknown", sizeof(dt));
      loglevel = M_WARNING;
    }

    Jmsg(jcr, loglevel, 0,
         T_("Top level directory \"%s\" has an unlisted drive type \"%s\"\n"),
         fname, dt);
    return false; /* Just ignore this error - or the whole backup is cancelled
                   */
  }

  ff_pkt->volhas_attrlist = VolumeHasAttrlist(fname);

  return true;
}

/**
 * Find a single file.
 *
 * HandleFile is the callback for handling the file.
 *    p is the filename
 *    parent_device is the device we are currently on
 *    top_level is 1 when not recursing or 0 when descending into a directory.
 */
int FindOneFile(JobControlRecord* jcr,
                FindFilesPacket* ff_pkt,
                int HandleFile(JobControlRecord* jcr,
                               FindFilesPacket* ff,
                               bool top_level),
                char* fname,
                dev_t parent_device,
                bool top_level)
{
  int rtn_stat;
  bool done = false;

  ff_pkt->link_or_dir = ff_pkt->fname = fname;
  ff_pkt->type = FT_UNSET;
  if (lstat(fname, &ff_pkt->statp) != 0) {
    // Cannot stat file
    ff_pkt->type = FT_NOSTAT;
    ff_pkt->ff_errno = errno;
    return HandleFile(jcr, ff_pkt, top_level);
  }

  Dmsg1(300, "File ----: %s\n", fname);

  /* We check for allowed fstypes and drivetypes at top_level and fstype change
   * (below). */
  if (top_level) {
    if (!NeedsProcessing(jcr, ff_pkt, fname)) { return 1; }
  }

  // Ignore this entry if no_dump() returns true
  if (no_dump(jcr, ff_pkt)) {
    Dmsg1(100, "'%s' ignored (NODUMP flag set)\n", ff_pkt->fname);
    return 1;
  }

  switch (ff_pkt->statp.st_mode & S_IFMT) {
    case S_IFDIR:
      break;
    case S_IFREG:
      if (!CheckSizeMatching(jcr, ff_pkt)) {
        Dmsg1(100, "'%s' ignored (Size doesn't match\n", ff_pkt->fname);
        return 1;
      }
      [[fallthrough]];
    default:
      /* If this is an Incremental backup, see if file was modified
       * since our last "save_time", presumably the last Full save
       * or Incremental. */
      if (!CheckChanges(jcr, ff_pkt)) {
        Dmsg1(500, "Non-directory incremental: %s\n", ff_pkt->fname);
        ff_pkt->type = FT_NOCHG;
        return HandleFile(jcr, ff_pkt, top_level);
      }
      break;
  }

#ifdef HAVE_DARWIN_OS
  if (BitIsSet(FO_HFSPLUS, ff_pkt->flags) && ff_pkt->volhas_attrlist
      && S_ISREG(ff_pkt->statp.st_mode)) {
    rtn_stat = process_hfsattributes(jcr, ff_pkt, HandleFile, fname, top_level);
    if (rtn_stat != -1) { return rtn_stat; }
  }
#endif

  ff_pkt->LinkFI = 0;
  // some codes accesses FileIndex even if the file was never send
  // If we did not reset it here, they would read the file index of the
  // last send file.  This is obviously not great and as such we
  // reset it to zero here before it can do any damage (0 is an invalid
  // FileIndex).
  ff_pkt->FileIndex = 0;
  ff_pkt->linked = nullptr;
  /* Handle hard linked files
   *
   * Maintain a list of hard linked files already backed up. This
   * allows us to ensure that the data of each file gets backed
   * up only once. */
  if (!BitIsSet(FO_NO_HARDLINK, ff_pkt->flags) && ff_pkt->statp.st_nlink > 1) {
    switch (ff_pkt->statp.st_mode & S_IFMT) {
      case S_IFREG:
      case S_IFCHR:
      case S_IFBLK:
      case S_IFIFO:
#ifdef S_IFSOCK
      case S_IFSOCK:
#endif
        /* Via the done variable the process_hardlink function returns
         * if file processing is done. If done is set to false we continue
         * with the normal processing of the file. */
        rtn_stat = process_hardlink(jcr, ff_pkt, HandleFile, fname, top_level,
                                    &done);
        if (done) { return rtn_stat; }
        break;
      default:
        ff_pkt->linked = NULL;
        break;
    }
  } else {
    ff_pkt->linked = NULL;
  }

  /* Based on the type of file call the correct function.
   * This is not a link to a previously dumped file, so dump it. */
  switch (ff_pkt->statp.st_mode & S_IFMT) {
    case S_IFREG:
      return process_regular_file(jcr, ff_pkt, HandleFile, fname, top_level);
#ifdef S_IFLNK
    case S_IFLNK:
      return process_symlink(jcr, ff_pkt, HandleFile, fname, top_level);
#endif
    case S_IFDIR:
      return process_directory(jcr, ff_pkt, HandleFile, fname, parent_device,
                               top_level);
    default:
      return process_special_file(jcr, ff_pkt, HandleFile, fname, top_level);
  }
}

void TermFindOne(FindFilesPacket* ff)
{
  if (ff->linkhash == nullptr) { return; }
  delete ff->linkhash;
  ff->linkhash = nullptr;
}
