/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald MMI
 */
/**
 * @file
 * File types as returned by find_files()
 */

#ifndef __FIND_H
#define __FIND_H

#include "fileopts.h"
#include "bfile.h"

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif

#include <sys/file.h>
#if !defined(HAVE_WIN32) || defined(HAVE_MINGW)
#include <sys/param.h>
#endif

#if !defined(HAVE_UTIMES) && !defined(HAVE_LUTIMES)
#if HAVE_UTIME_H
#include <utime.h>
#else
struct utimbuf {
   long actime;
   long modtime;
};
#endif
#endif

#define MODE_RALL (S_IRUSR|S_IRGRP|S_IROTH)

#include "lib/fnmatch.h"

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif
#ifdef USE_READDIR_R
#ifndef HAVE_READDIR_R
DLL_IMP_EXP int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif
#endif
/**
 * For options FO_xxx values see src/fileopts.h
 */
enum {
   state_none,
   state_options,
   state_include,
   state_error
};

typedef enum {
   check_shadow_none,
   check_shadow_local_warn,
   check_shadow_local_remove,
   check_shadow_global_warn,
   check_shadow_global_remove
} b_fileset_shadow_type;

typedef enum {
   size_match_none,
   size_match_approx,
   size_match_smaller,
   size_match_greater,
   size_match_range
} b_sz_match_type;

struct s_sz_matching {
   b_sz_match_type type;
   uint64_t begin_size;
   uint64_t end_size;
};

struct s_included_file {
   struct s_included_file *next;
   char options[FOPTS_BYTES];         /**< Backup options */
   uint32_t cipher;                   /**< Encryption cipher forced by fileset */
   uint32_t algo;                     /**< Compression algorithm. 4 letters stored as an integer */
   int level;                         /**< Compression level */
   int len;                           /**< Length of fname */
   int pattern;                       /**< Set if wild card pattern */
   struct s_sz_matching *size_match;  /**< Perform size matching ? */
   b_fileset_shadow_type shadow_type; /**< Perform fileset shadowing check ? */
   char VerifyOpts[20];               /**< Options for verify */
   char fname[1];
};

struct s_excluded_file {
   struct s_excluded_file *next;
   int len;
   char fname[1];
};

#define MAX_OPTS 20

/**
 * File options structure
 */
struct findFOPTS {
   char flags[FOPTS_BYTES];           /**< Backup options */
   uint32_t Encryption_cipher;        /**< Encryption cipher forced by fileset */
   uint32_t Compress_algo;            /**< Compression algorithm. 4 letters stored as an integer */
   int Compress_level;                /**< Compression level */
   int strip_path;                    /**< Strip path count */
   struct s_sz_matching *size_match;  /**< Perform size matching ? */
   b_fileset_shadow_type shadow_type; /**< Perform fileset shadowing check ? */
   char VerifyOpts[MAX_OPTS];         /**< Verify options */
   char AccurateOpts[MAX_OPTS] ;      /**< Accurate mode options */
   char BaseJobOpts[MAX_OPTS];        /**< Basejob mode options */
   char *plugin;                      /**< Plugin that handle this section */
   alist regex;                       /**< Regex string(s) */
   alist regexdir;                    /**< Regex string(s) for directories */
   alist regexfile;                   /**< Regex string(s) for files */
   alist wild;                        /**< Wild card strings */
   alist wilddir;                     /**< Wild card strings for directories */
   alist wildfile;                    /**< Wild card strings for files */
   alist wildbase;                    /**< Wild card strings for basenames */
   alist base;                        /**< List of base names */
   alist fstype;                      /**< File system type limitation */
   alist drivetype;                   /**< Drive type limitation */
};

/**
 * This is either an include item or an exclude item
 */
struct findIncludeExcludeItem {
   findFOPTS *current_opts;           /**< Points to current options structure */
   alist opts_list;                   /**< Options list */
   dlist name_list;                   /**< Filename list -- holds dlistString */
   dlist plugin_list;                 /**< Plugin list -- holds dlistString */
   alist ignoredir;                   /**< Ignore directories with this file(s) */
};

/**
 * FileSet Resource
 */
struct findFILESET {
   int state;
   findIncludeExcludeItem *incexe;                /**< Current item */
   alist include_list;
   alist exclude_list;
};

/**
 * OSX resource fork.
 */
struct HfsPlusInfo {
   unsigned long length;              /**< Mandatory field */
   char fndrinfo[32];                 /**< Finder Info */
   off_t rsrclength;                  /**< Size of resource fork */
};

/**
 * Structure for keeping track of hard linked files, we
 * keep an entry for each hardlinked file that we save,
 * which is the first one found. For all the other files that
 * are linked to this one, we save only the directory
 * entry so we can link it.
 */
struct CurLink {
    struct hlink link;
    dev_t dev;                        /**< Device */
    ino_t ino;                        /**< Inode with device is unique */
    uint32_t FileIndex;               /**< Bareos FileIndex of this file */
    int32_t digest_stream;            /**< Digest type if needed */
    uint32_t digest_len;              /**< Digest len if needed */
    char *digest;                     /**< Checksum of the file if needed */
    char name[1];                     /**< The name */
};

/**
 * Definition of the find_files packet passed as the
 * first argument to the find_files callback subroutine.
 */
struct FindFilesPacket {
   char *top_fname;                   /**< Full filename before descending */
   char *fname;                       /**< Full filename */
   char *link;                        /**< Link if file linked */
   char *object_name;                 /**< Object name */
   char *object;                      /**< Restore object */
   char *plugin;                      /**< Current Options{Plugin=} name */
   POOLMEM *sys_fname;                /**< System filename */
   POOLMEM *fname_save;               /**< Save when stripping path */
   POOLMEM *link_save;                /**< Save when stripping path */
   POOLMEM *ignoredir_fname;          /**< Used to ignore directories */
   char *digest;                      /**< Set to file digest when the file is a hardlink */
   struct stat statp;                 /**< Stat packet */
   uint32_t digest_len;               /**< Set to the digest len when the file is a hardlink*/
   int32_t digest_stream;             /**< Set to digest type when the file is hardlink */
   int32_t FileIndex;                 /**< FileIndex of this file */
   int32_t LinkFI;                    /**< FileIndex of main hard linked file */
   int32_t delta_seq;                 /**< Delta Sequence number */
   int32_t object_index;              /**< Object index */
   int32_t object_len;                /**< Object length */
   int32_t object_compression;        /**< Type of compression for object */
   int type;                          /**< FT_ type from above */
   int ff_errno;                      /**< Errno */
   BareosWinFilePacket bfd;                         /**< Bareos file descriptor */
   time_t save_time;                  /**< Start of incremental time */
   bool accurate_found;               /**< Found in the accurate hash (valid after check_changes()) */
   bool dereference;                  /**< Follow links (not implemented) */
   bool null_output_device;           /**< Using null output device */
   bool incremental;                  /**< Incremental save */
   bool no_read;                      /**< Do not read this file when using Plugin */
   char VerifyOpts[MAX_OPTS];
   char AccurateOpts[MAX_OPTS];
   char BaseJobOpts[MAX_OPTS];
   struct s_included_file *included_files_list;
   struct s_excluded_file *excluded_files_list;
   struct s_excluded_file *excluded_paths_list;
   findFILESET *fileset;
   int (*file_save)(JobControlRecord *, FindFilesPacket *, bool); /**< User's callback */
   int (*plugin_save)(JobControlRecord *, FindFilesPacket *, bool); /**< User's callback */
   bool (*check_fct)(JobControlRecord *, FindFilesPacket *); /**< Optionnal user fct to check file changes */

   /*
    * Values set by accept_file while processing Options
    */
   char flags[FOPTS_BYTES];           /**< Backup options */
   uint32_t Compress_algo;            /**< Compression algorithm. 4 letters stored as an integer */
   int Compress_level;                /**< Compression level */
   int strip_path;                    /**< Strip path count */
   struct s_sz_matching *size_match;  /**< Perform size matching ? */
   bool cmd_plugin;                   /**< Set if we have a command plugin */
   bool opt_plugin;                   /**< Set if we have an option plugin */
   alist fstypes;                     /**< Allowed file system types */
   alist drivetypes;                  /**< Allowed drive types */

   /*
    * List of all hard linked files found
    */
   htable *linkhash;                  /**< Hard linked files */
   struct CurLink *linked;            /**< Set if this file is hard linked */

   /*
    * Darwin specific things.
    * To avoid clutter, we always include rsrc_bfd and volhas_attrlist.
    */
   BareosWinFilePacket rsrc_bfd;                    /**< Fd for resource forks */
   bool volhas_attrlist;              /**< Volume supports getattrlist() */
   struct HfsPlusInfo hfsinfo;       /**< Finder Info and resource fork size */
};

#include "acl.h"
#include "xattr.h"
#include "protos.h"

#endif /* __FILES_H */
