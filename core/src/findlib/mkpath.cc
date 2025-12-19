/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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

// Kern Sibbald, September MMVII

/*
 * This is tricky code, especially when writing from scratch. Fortunately,
 * a non-copyrighted version of mkdir was available to consult.
 *
 * ***FIXME*** the mkpath code could be significantly optimized by
 * walking up the path chain from the bottom until it either gets
 * to the top or finds an existing directory then walk back down
 * creating the path components.  Currently, it always starts at
 * the top, which can be rather inefficient for long path names.
 */
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/jcr.h"
#include "lib/attr.h"
#include "lib/berrno.h"
#include "lib/path_list.h"

#define debuglevel 50

// For old systems that don't have lchmod() or where it is a stub use chmod()
#if !defined(HAVE_LCHMOD) || defined(__stub_lchmod) || defined(__stub___lchmod)
#  define lchmod chmod
#endif

static bool makedir(JobControlRecord* jcr,
                    char* path,
                    [[maybe_unused]] mode_t mode,
                    int* created)
{
  struct stat statp;

  if (mkdir(path, mode) != 0) {
    BErrNo be;
    *created = false;
    if (stat(path, &statp) != 0) {
      Jmsg2(jcr, M_ERROR, 0, T_("Cannot create directory %s: ERR=%s\n"), path,
            be.bstrerror());
      return false;
    } else if (!S_ISDIR(statp.st_mode)) {
      Jmsg1(jcr, M_ERROR, 0, T_("%s exists but is not a directory.\n"), path);
      return false;
    }
    return true; /* directory exists */
  }

  if (jcr->keep_path_list) {
    // When replace = NEVER, we keep track of all directories newly created
    if (!jcr->path_list) { jcr->path_list = path_list_init(); }

    PathListAdd(jcr->path_list, strlen(path), path);
  }

  *created = true;
  return true;
}

static void SetOwnMod(Attributes* attr,
                      char* path,
                      uid_t owner,
                      gid_t group,
                      mode_t mode)
{
  if (lchown(path, owner, group) != 0 && attr->uid == 0) {
    BErrNo be;
    Jmsg2(attr->jcr, M_WARNING, 0,
          T_("Cannot change owner and/or group of %s: ERR=%s\n"), path,
          be.bstrerror());
  }
#if defined(HAVE_WIN32)
  if (win32_chmod(path, mode, 0) != 0 && attr->uid == 0) {
#else
  if (lchmod(path, mode) != 0 && attr->uid == 0) {
#endif
    BErrNo be;
    Jmsg2(attr->jcr, M_WARNING, 0,
          T_("Cannot change permissions of %s: ERR=%s\n"), path,
          be.bstrerror());
  }
}

/**
 * mode is the mode bits to use in creating a new directory
 * parent_mode are the parent's modes if we need to create parent directories.
 * owner and group are to set on any created dirs
 * keep_dir_modes if set means don't change mode bits if dir exists
 */
bool makepath(Attributes* attr,
              const char* apath,
              mode_t mode,
              mode_t parent_mode,
              uid_t owner,
              gid_t group,
              bool keep_dir_modes)
{
  struct stat statp;
  mode_t omask, tmode;
  char* path = (char*)apath;
  char* p;
  int len;
  bool ok = false;
  int created;
  char new_dir[5000];
  int ndir = 0;
  int i = 0;
  int max_dirs = (int)sizeof(new_dir);
  JobControlRecord* jcr = attr->jcr;

  if (stat(path, &statp) == 0) { /* Does dir exist? */
    if (!S_ISDIR(statp.st_mode)) {
      Jmsg1(jcr, M_ERROR, 0, T_("%s exists but is not a directory.\n"), path);
      return false;
    }
    /* Full path exists */
    if (keep_dir_modes) { return true; }
    SetOwnMod(attr, path, owner, group, mode);
    return true;
  }
  omask = umask(0);
  umask(omask);
  len = strlen(apath);
  path = (char*)alloca(len + 1);
  bstrncpy(path, apath, len + 1);
  StripTrailingSlashes(path);
  /* Now for one of the complexities. If we are not running as root,
   * then if the parent_mode does not have wx user perms, or we are
   * setting the userid or group, and the parent_mode has setuid, setgid,
   * or sticky bits, we must create the dir with open permissions, then
   * go back and patch all the dirs up with the correct perms.
   * Solution, set everything to 0777, then go back and reset them at the
   * end. */
  tmode = 0777;

#if defined(HAVE_WIN32)
  // Validate drive letter
  if (path[1] == ':') {
    char drive[4] = "X:\\";

    drive[0] = path[0];

    UINT drive_type = GetDriveType(drive);

    if (drive_type == DRIVE_UNKNOWN || drive_type == DRIVE_NO_ROOT_DIR) {
      Jmsg1(jcr, M_ERROR, 0, T_("%c: is not a valid drive.\n"), path[0]);
      goto bail_out;
    }

    if (path[2] == '\0') { /* attempt to create a drive */
      ok = true;
      goto bail_out; /* OK, it is already there */
    }

    p = &path[3];
  } else {
    p = path;
  }
#else
  p = path;
#endif

  // Skip leading slash(es)
  while (IsPathSeparator(*p)) { p++; }
  while ((p = first_path_separator(p))) {
    char save_p;
    save_p = *p;
    *p = 0;
    if (!makedir(jcr, path, tmode, &created)) { goto bail_out; }
    if (ndir < max_dirs) { new_dir[ndir++] = created; }
    *p = save_p;
    while (IsPathSeparator(*p)) { p++; }
  }
  // Create final component if not a junction/symlink
  if (attr->type != FT_JUNCTION) {
    if (!makedir(jcr, path, tmode, &created)) { goto bail_out; }
  }

  if (ndir < max_dirs) { new_dir[ndir++] = created; }
  if (ndir >= max_dirs) {
    Jmsg0(jcr, M_WARNING, 0,
          T_("Too many subdirectories. Some permissions not reset.\n"));
  }

  // Now set the proper owner and modes
#if defined(HAVE_WIN32)

  // Don't propagate the hidden attribute to parent directories
  parent_mode &= ~S_ISVTX;

  if (path[1] == ':') {
    p = &path[3];
  } else {
    p = path;
  }
#else
  p = path;
#endif
  // Skip leading slash(es)
  while (IsPathSeparator(*p)) { p++; }
  while ((p = first_path_separator(p))) {
    char save_p;
    save_p = *p;
    *p = 0;
    if (i < ndir && new_dir[i++] && !keep_dir_modes) {
      SetOwnMod(attr, path, owner, group, parent_mode);
    }
    *p = save_p;
    while (IsPathSeparator(*p)) { p++; }
  }

  // Set for final component
  if (i < ndir && new_dir[i++] && !keep_dir_modes) {
    SetOwnMod(attr, path, owner, group, mode);
  }

  ok = true;
bail_out:
  umask(omask);
  return ok;
}
