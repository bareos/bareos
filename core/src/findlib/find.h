/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
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
// Kern Sibbald MMI
/**
 * @file
 * File types as returned by FindFiles()
 */

#ifndef BAREOS_FINDLIB_FIND_H_
#define BAREOS_FINDLIB_FIND_H_

#include "include/fileopts.h"
#include "bfile.h"
#include "lib/htable.h"
#include "lib/dlist.h"
#include "lib/alist.h"
#include "findlib/hardlink.h"

#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))

#include <sys/file.h>
#if !defined(HAVE_WIN32) || defined(HAVE_MINGW)
#  include <sys/param.h>
#endif

#if defined(HAVE_MSVC)
extern "C" {
#  include <sys/utime.h>
}
#elif defined(HAVE_WIN32)
#  include <utime.h>
#endif


#define MODE_RALL (S_IRUSR | S_IRGRP | S_IROTH)

#include "lib/fnmatch.h"

#if __has_include(<regex.h>)
#  include <regex.h>
#else
#  include "lib/bregex.h"
#endif
#ifdef USE_READDIR_R
#  ifndef HAVE_READDIR_R
int Readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result);
#  endif
#endif

// For options FO_xxx values see src/fileopts.h
enum
{
  state_none,
  state_options,
  state_include,
  state_error
};

typedef enum
{
  check_shadow_none,
  check_shadow_local_warn,
  check_shadow_local_remove,
  check_shadow_global_warn,
  check_shadow_global_remove
} b_fileset_shadow_type;

typedef enum
{
  size_match_none,
  size_match_approx,
  size_match_smaller,
  size_match_greater,
  size_match_range
} b_sz_match_type;

struct s_sz_matching {
  b_sz_match_type type{size_match_none};
  uint64_t begin_size{};
  uint64_t end_size{};
};

struct s_included_file {
  struct s_included_file* next;
  char options[FOPTS_BYTES]; /**< Backup options */
  uint32_t cipher;           /**< Encryption cipher forced by fileset */
  uint32_t algo; /**< Compression algorithm. 4 letters stored as an integer */
  int level;     /**< Compression level */
  int len;       /**< Length of fname */
  int pattern;   /**< Set if wild card pattern */
  struct s_sz_matching* size_match;  /**< Perform size matching ? */
  b_fileset_shadow_type shadow_type; /**< Perform fileset shadowing check ? */
  char VerifyOpts[20];               /**< Options for verify */
  char fname[1];
};

struct s_excluded_file {
  struct s_excluded_file* next;
  int len;
  char fname[1];
};

#define MAX_OPTS 20

// File options structure
struct findFOPTS {
  char flags[FOPTS_BYTES]{};    /**< Backup options */
  uint32_t Encryption_cipher{}; /**< Encryption cipher forced by fileset */
  uint32_t Compress_algo{}; /**< Compression algorithm. 4 letters stored as an
                             integer */
  int Compress_level{};     /**< Compression level */
  int StripPath{};          /**< Strip path count */
  struct s_sz_matching* size_match{}; /**< Perform size matching ? */
  b_fileset_shadow_type shadow_type{
      check_shadow_none};        /**< Perform fileset shadowing check ? */
  char VerifyOpts[MAX_OPTS]{};   /**< Verify options */
  char AccurateOpts[MAX_OPTS]{}; /**< Accurate mode options */
  char* plugin{};                /**< Plugin that handle this section */
  alist<regex_t*> regex;         /**< Regex string(s) */
  alist<regex_t*> regexdir;      /**< Regex string(s) for directories */
  alist<regex_t*> regexfile;     /**< Regex string(s) for files */
  alist<const char*> wild;       /**< Wild card strings */
  alist<const char*> wilddir;    /**< Wild card strings for directories */
  alist<const char*> wildfile;   /**< Wild card strings for files */
  alist<const char*> wildbase;   /**< Wild card strings for basenames */
  alist<const char*> fstype;     /**< File system type limitation */
  alist<const char*> Drivetype;  /**< Drive type limitation */
};

// This is either an include item or an exclude item
struct findIncludeExcludeItem {
  findFOPTS* current_opts;        /**< Points to current options structure */
  alist<findFOPTS*> opts_list;    /**< Options list */
  dlist<dlistString> name_list;   /**< Filename list -- holds dlistString */
  dlist<dlistString> plugin_list; /**< Plugin list -- holds dlistString */
  alist<const char*> ignoredir;   /**< Ignore directories with this file(s) */
};

// FileSet Resource
struct findFILESET {
  int state;
  findIncludeExcludeItem* incexe; /**< Current item */
  alist<findIncludeExcludeItem*> include_list;
  alist<findIncludeExcludeItem*> exclude_list;
};

// OSX resource fork.
struct HfsPlusInfo {
  unsigned long length{0}; /**< Mandatory field */
  char fndrinfo[32]{};     /**< Finder Info */
  off_t rsrclength{0};     /**< Size of resource fork */
};

/**
 * Definition of the FindFiles packet passed as the
 * first argument to the FindFiles callback subroutine.
 */
/* clang-format off */
struct FindFilesPacket {
  char* top_fname{nullptr};          /**< Full filename before descending */
  char* fname{nullptr};              /**< Full filename */
  char* link_or_dir{nullptr};               /**< Link if file linked, or canonical directory path */
  char* object_name{nullptr};        /**< Object name */
  char* object{nullptr};             /**< Restore object */
  char* plugin{nullptr};             /**< Current Options{Plugin=} name */
  POOLMEM* sys_fname{nullptr};       /**< System filename */
  POOLMEM* fname_save{nullptr};      /**< Save when stripping path */
  POOLMEM* link_save{nullptr};       /**< Save when stripping path */
  POOLMEM* ignoredir_fname{nullptr}; /**< Used to ignore directories */
  char* digest{nullptr};  /**< Set to file digest when the file is a hardlink */
  struct stat statp{};    /**< Stat packet */
  uint32_t digest_len{0}; /**< Set to the digest len when the file is a hardlink*/
  int32_t digest_stream{0}; /**< Set to digest type when the file is hardlink */
  int32_t FileIndex{0};     /**< FileIndex of this file */
  int32_t LinkFI{0};        /**< FileIndex of main hard linked file */
  int32_t delta_seq{0};     /**< Delta Sequence number */
  int32_t object_index{0};  /**< Object index */
  int32_t object_len{0};    /**< Object length */
  int32_t object_compression{0};  /**< Type of compression for object */
  int type{0};                    /**< FT_ type from above */
  int ff_errno{0};                /**< Errno */
  BareosFilePacket bfd;        /**< Bareos file descriptor */
  time_t save_time{0};            /**< Start of incremental time */
  bool accurate_found{false};     /**< Found in the accurate hash (valid after
                                       CheckChanges()) */
  bool incremental{false};        /**< Incremental save */
  bool no_read{false};            /**< Do not read this file when using Plugin */
  char VerifyOpts[MAX_OPTS]{};
  char AccurateOpts[MAX_OPTS]{};
  char BaseJobOpts[MAX_OPTS]{};
  struct s_included_file* included_files_list{nullptr};
  struct s_excluded_file* excluded_files_list{nullptr};
  struct s_excluded_file* excluded_paths_list{nullptr};
  findFILESET* fileset{nullptr};
  int (*FileSave)(JobControlRecord*,
                  FindFilesPacket*,
                  bool){};   /**< User's callback */
  bool (*CheckFct)(
      JobControlRecord*,
      FindFilesPacket*){};   /**< Optional user fct to check file changes */

  // Values set by AcceptFile while processing Options
  char flags[FOPTS_BYTES]{}; /**< Backup options */
  uint32_t Compress_algo{0}; /**< Compression algorithm. 4 letters stored as an integer */
  int Compress_level{0};     /**< Compression level */
  int StripPath{0};          /**< Strip path count */
  struct s_sz_matching* size_match{nullptr}; /**< Perform size matching ? */
  bool cmd_plugin{false}; /**< Set if we have a command plugin */
  bool opt_plugin{false}; /**< Set if we have an option plugin */
  alist<const char*> fstypes;          /**< Allowed file system types */
  alist<const char*> drivetypes;       /**< Allowed drive types */

  // List of all hard linked files found
  LinkHash* linkhash{nullptr};       /**< Hard linked files */
  struct CurLink* linked{nullptr}; /**< Set if this file is hard linked */

  /* Darwin specific things.
   * To avoid clutter, we always include rsrc_bfd and volhas_attrlist. */
  bool volhas_attrlist{false};  /**< Volume supports getattrlist() */
  HfsPlusInfo hfsinfo;          /**< Finder Info and resource fork size */
};
/* clang-format on */

FindFilesPacket* init_find_files();
void SetFindOptions(FindFilesPacket* ff, bool incremental, time_t mtime);
void SetFindChangedFunction(FindFilesPacket* ff,
                            bool CheckFct(JobControlRecord* jcr,
                                          FindFilesPacket* ff));
int FindFiles(JobControlRecord* jcr,
              FindFilesPacket* ff,
              int file_sub(JobControlRecord*, FindFilesPacket* ff_pkt, bool),
              int PluginSub(JobControlRecord*, FindFilesPacket* ff_pkt, bool));
bool MatchFiles(JobControlRecord* jcr,
                FindFilesPacket* ff,
                int sub(JobControlRecord*, FindFilesPacket* ff_pkt, bool));
void TermFindFiles(FindFilesPacket* ff);
bool IsInFileset(FindFilesPacket* ff);
bool AcceptFile(FindFilesPacket* ff);
findIncludeExcludeItem* allocate_new_incexe(void);
findIncludeExcludeItem* new_exclude(findFILESET* fileset);
findIncludeExcludeItem* new_include(findFILESET* fileset);
findIncludeExcludeItem* new_preinclude(findFILESET* fileset);
findFOPTS* start_options(FindFilesPacket* ff);
void NewOptions(FindFilesPacket* ff, findIncludeExcludeItem* incexe);

#include "acl.h"
#include "xattr.h"

#endif  // BAREOS_FINDLIB_FIND_H_
