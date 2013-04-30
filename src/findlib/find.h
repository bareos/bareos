/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

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
 * File types as returned by find_files()
 *
 *     Kern Sibbald MMI
 */

#ifndef __FILES_H
#define __FILES_H

#include "jcr.h"
#include "fileopts.h"
#include "bfile.h"
#include "../filed/fd_plugins.h"

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif

#include <sys/file.h>
#if !defined(HAVE_WIN32) || defined(HAVE_MINGW)
#include <sys/param.h>
#endif
#if HAVE_UTIME_H
#include <utime.h>
#else
struct utimbuf {
    long actime;
    long modtime;
};
#endif

#define MODE_RALL (S_IRUSR|S_IRGRP|S_IROTH)

#include "lib/fnmatch.h"
// #include "lib/enh_fnmatch.h"

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

#ifndef HAVE_READDIR_R
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif

/* For options FO_xxx values see src/fileopts.h */

struct s_included_file {
   struct s_included_file *next;
   uint32_t options;                  /* backup options */
   uint32_t algo;                     /* compression algorithm. 4 letters stored as an interger */
   int level;                         /* compression level */
   int len;                           /* length of fname */
   int pattern;                       /* set if wild card pattern */
   char VerifyOpts[20];               /* Options for verify */
   char fname[1];
};

struct s_excluded_file {
   struct s_excluded_file *next;
   int len;
   char fname[1];
};

/* FileSet definitions very similar to the resource
 *  contained in the Director because the components
 *  of the structure are passed by the Director to the
 *  File daemon and recompiled back into this structure
 */
#undef  MAX_FOPTS
#define MAX_FOPTS 30

enum {
   state_none,
   state_options,
   state_include,
   state_error
};

/* File options structure */
struct findFOPTS {
   uint32_t flags;                    /* options in bits */
   uint32_t Compress_algo;            /* compression algorithm. 4 letters stored as an interger */
   int Compress_level;                /* compression level */
   int strip_path;                    /* strip path count */
   char VerifyOpts[MAX_FOPTS];        /* verify options */
   char AccurateOpts[MAX_FOPTS];      /* accurate mode options */
   char BaseJobOpts[MAX_FOPTS];       /* basejob mode options */
   char *plugin;                      /* Plugin that handle this section */
   alist regex;                       /* regex string(s) */
   alist regexdir;                    /* regex string(s) for directories */
   alist regexfile;                   /* regex string(s) for files */
   alist wild;                        /* wild card strings */
   alist wilddir;                     /* wild card strings for directories */
   alist wildfile;                    /* wild card strings for files */
   alist wildbase;                    /* wild card strings for basenames */
   alist base;                        /* list of base names */
   alist fstype;                      /* file system type limitation */
   alist drivetype;                   /* drive type limitation */
};


/* This is either an include item or an exclude item */
struct findINCEXE {
   findFOPTS *current_opts;           /* points to current options structure */
   alist opts_list;                   /* options list */
   dlist name_list;                   /* filename list -- holds dlistString */
   dlist plugin_list;                 /* plugin list -- holds dlistString */
   char *ignoredir;                   /* ignore directories with this file */
};

/*
 *   FileSet Resource
 *
 */
struct findFILESET {
   int state;
   findINCEXE *incexe;                /* current item */
   alist include_list;
   alist exclude_list;
};

struct HFSPLUS_INFO {
   unsigned long length;              /* Mandatory field */
   char fndrinfo[32];                 /* Finder Info */
   off_t rsrclength;                  /* Size of resource fork */
};

/*
 * Definition of the find_files packet passed as the
 * first argument to the find_files callback subroutine.
 */
struct FF_PKT {
   char *top_fname;                   /* full filename before descending */
   char *fname;                       /* full filename */
   char *link;                        /* link if file linked */
   char *object_name;                 /* Object name */
   char *object;                      /* restore object */
   char *plugin;                      /* Current Options{Plugin=} name */
   POOLMEM *sys_fname;                /* system filename */
   POOLMEM *fname_save;               /* save when stripping path */
   POOLMEM *link_save;                /* save when stripping path */
   POOLMEM *ignoredir_fname;          /* used to ignore directories */
   char *digest;                      /* set to file digest when the file is a hardlink */
   struct stat statp;                 /* stat packet */
   uint32_t digest_len;               /* set to the digest len when the file is a hardlink*/
   int32_t digest_stream;             /* set to digest type when the file is hardlink */
   int32_t FileIndex;                 /* FileIndex of this file */
   int32_t LinkFI;                    /* FileIndex of main hard linked file */
   int32_t delta_seq;                 /* Delta Sequence number */
   int32_t object_index;              /* Object index */
   int32_t object_len;                /* Object length */
   int32_t object_compression;        /* Type of compression for object */
   struct f_link *linked;             /* Set if this file is hard linked */
   int type;                          /* FT_ type from above */
   int ff_errno;                      /* errno */
   BFILE bfd;                         /* Bacula file descriptor */
   time_t save_time;                  /* start of incremental time */
   bool accurate_found;               /* Found in the accurate hash (valid after check_changes()) */
   bool dereference;                  /* follow links (not implemented) */
   bool null_output_device;           /* using null output device */
   bool incremental;                  /* incremental save */
   bool no_read;                      /* Do not read this file when using Plugin */
   char VerifyOpts[20];
   char AccurateOpts[20];
   char BaseJobOpts[20];
   struct s_included_file *included_files_list;
   struct s_excluded_file *excluded_files_list;
   struct s_excluded_file *excluded_paths_list;
   findFILESET *fileset;
   int (*file_save)(JCR *, FF_PKT *, bool); /* User's callback */
   int (*plugin_save)(JCR *, FF_PKT *, bool); /* User's callback */
   bool (*check_fct)(JCR *, FF_PKT *); /* optionnal user fct to check file changes */

   /* Values set by accept_file while processing Options */
   uint32_t flags;                    /* backup options */
   uint32_t Compress_algo;            /* compression algorithm. 4 letters stored as an interger */
   int Compress_level;                /* compression level */
   int strip_path;                    /* strip path count */
   bool cmd_plugin;                   /* set if we have a command plugin */
   bool opt_plugin;                   /* set if we have an option plugin */
   alist fstypes;                     /* allowed file system types */
   alist drivetypes;                  /* allowed drive types */

   /* List of all hard linked files found */
   struct f_link **linkhash;          /* hard linked files */

   /* Darwin specific things.
    * To avoid clutter, we always include rsrc_bfd and volhas_attrlist */
   BFILE rsrc_bfd;                    /* fd for resource forks */
   bool volhas_attrlist;              /* Volume supports getattrlist() */
   struct HFSPLUS_INFO hfsinfo;       /* Finder Info and resource fork size */
};


#include "protos.h"

#endif /* __FILES_H */
