/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
// Kern E. Sibbald, MM
/**
 * @file
 * Main routine for finding files on a file system.
 * The heart of the work to find the files on the
 * system is done in find_one.c. Here we have the
 * higher level control as well as the matching
 * routines for the new syntax Options resource.
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "find.h"
#include "findlib/find_one.h"
#include "lib/util.h"

static const int debuglevel = 450;

int32_t name_max; /* filename max length */
int32_t path_max; /* path name max length */

static int OurCallback(JobControlRecord* jcr,
                       FindFilesPacket* ff,
                       bool top_level);

static const int fnmode = 0;

// Initialize the find files "global" variables
FindFilesPacket* init_find_files()
{
  FindFilesPacket* ff;

  ff = (FindFilesPacket*)malloc(sizeof(FindFilesPacket));
  FindFilesPacket empty_ff;
  *ff = empty_ff;

  ff->sys_fname = GetPoolMemory(PM_FNAME);

  /* Get system path and filename maximum lengths */
  path_max = pathconf(".", _PC_PATH_MAX);
  if (path_max < 2048) { path_max = 2048; }

  name_max = pathconf(".", _PC_NAME_MAX);
  if (name_max < 2048) { name_max = 2048; }
  path_max++; /* add for EOS */
  name_max++; /* add for EOS */

  Dmsg1(debuglevel, "init_find_files ff=%p\n", ff);
  return ff;
}

/**
 * Set FindFiles options. For the moment, we only
 * provide for full/incremental saves, and setting
 * of save_time. For additional options, see above
 */
void SetFindOptions(FindFilesPacket* ff, bool incremental, time_t save_time)
{
  Dmsg0(debuglevel, "Enter SetFindOptions()\n");
  ff->incremental = incremental;
  ff->save_time = save_time;
  Dmsg0(debuglevel, "Leave SetFindOptions()\n");
}

void SetFindChangedFunction(FindFilesPacket* ff,
                            bool CheckFct(JobControlRecord* jcr,
                                          FindFilesPacket* ff))
{
  Dmsg0(debuglevel, "Enter SetFindChangedFunction()\n");
  ff->CheckFct = CheckFct;
}

/**
 * Call this subroutine with a callback subroutine as the first
 * argument and a packet as the second argument, this packet
 * will be passed back to the callback subroutine as the last
 * argument.
 */
int FindFiles(JobControlRecord* jcr,
              FindFilesPacket* ff,
              int FileSave(JobControlRecord* jcr,
                           FindFilesPacket* ff_pkt,
                           bool top_level),
              int PluginSave(JobControlRecord* jcr,
                             FindFilesPacket* ff_pkt,
                             bool top_level))
{
  ff->FileSave = FileSave;
  ff->PluginSave = PluginSave;

  /* This is the new way */
  findFILESET* fileset = ff->fileset;
  if (fileset) {
    int i, j;
    /*
     * TODO: We probably need be move the initialization in the fileset loop,
     * at this place flags options are "concatenated" accross Include {} blocks
     * (not only Options{} blocks inside a Include{})
     */
    ClearAllBits(FO_MAX, ff->flags);
    for (i = 0; i < fileset->include_list.size(); i++) {
      dlistString* node;
      findIncludeExcludeItem* incexe
          = (findIncludeExcludeItem*)fileset->include_list.get(i);
      fileset->incexe = incexe;

      // Here, we reset some values between two different Include{}
      strcpy(ff->VerifyOpts, "V");
      strcpy(ff->AccurateOpts, "Cmcs");  /* mtime+ctime+size by default */
      strcpy(ff->BaseJobOpts, "Jspug5"); /* size+perm+user+group+chk  */
      ff->plugin = NULL;
      ff->opt_plugin = false;

      /*
       * By setting all options, we in effect OR the global options which is
       * what we want.
       */
      for (j = 0; j < incexe->opts_list.size(); j++) {
        findFOPTS* fo;

        fo = (findFOPTS*)incexe->opts_list.get(j);
        CopyBits(FO_MAX, fo->flags, ff->flags);
        ff->Compress_algo = fo->Compress_algo;
        ff->Compress_level = fo->Compress_level;
        ff->StripPath = fo->StripPath;
        ff->size_match = fo->size_match;
        ff->fstypes = fo->fstype;
        ff->drivetypes = fo->Drivetype;
        if (fo->plugin != NULL) {
          ff->plugin = fo->plugin; /* TODO: generate a plugin event ? */
          ff->opt_plugin = true;
        }
        bstrncat(ff->VerifyOpts, fo->VerifyOpts,
                 sizeof(ff->VerifyOpts)); /* TODO: Concat or replace? */
        if (fo->AccurateOpts[0]) {
          bstrncpy(ff->AccurateOpts, fo->AccurateOpts,
                   sizeof(ff->AccurateOpts));
        }
        if (fo->BaseJobOpts[0]) {
          bstrncpy(ff->BaseJobOpts, fo->BaseJobOpts, sizeof(ff->BaseJobOpts));
        }
      }
      Dmsg4(50, "Verify=<%s> Accurate=<%s> BaseJob=<%s> flags=<%d>\n",
            ff->VerifyOpts, ff->AccurateOpts, ff->BaseJobOpts, ff->flags);

      foreach_dlist (node, &incexe->name_list) {
        char* fname = node->c_str();

        Dmsg1(debuglevel, "F %s\n", fname);
        ff->top_fname = fname;
        if (FindOneFile(jcr, ff, OurCallback, ff->top_fname, (dev_t)-1, true)
            == 0) {
          return 0; /* error return */
        }
        if (JobCanceled(jcr)) { return 0; }
      }

      foreach_dlist (node, &incexe->plugin_list) {
        char* fname = node->c_str();

        if (!PluginSave) {
          Jmsg(jcr, M_FATAL, 0, _("Plugin: \"%s\" not found.\n"), fname);
          return 0;
        }
        Dmsg1(debuglevel, "PluginCommand: %s\n", fname);
        ff->top_fname = fname;
        ff->cmd_plugin = true;
        PluginSave(jcr, ff, true);
        ff->cmd_plugin = false;
        if (JobCanceled(jcr)) { return 0; }
      }
    }
  }
  return 1;
}

/**
 * Test if the currently selected directory (in ff->fname) is
 * explicitly in the Include list or explicitly in the Exclude list.
 */
bool IsInFileset(FindFilesPacket* ff)
{
  int i;
  char* fname;
  dlistString* node;
  findIncludeExcludeItem* incexe;
  findFILESET* fileset = ff->fileset;

  if (fileset) {
    for (i = 0; i < fileset->include_list.size(); i++) {
      incexe = (findIncludeExcludeItem*)fileset->include_list.get(i);
      foreach_dlist (node, &incexe->name_list) {
        fname = node->c_str();
        Dmsg2(debuglevel, "Inc fname=%s ff->fname=%s\n", fname, ff->fname);
        if (bstrcmp(fname, ff->fname)) { return true; }
      }
    }
    for (i = 0; i < fileset->exclude_list.size(); i++) {
      incexe = (findIncludeExcludeItem*)fileset->exclude_list.get(i);
      foreach_dlist (node, &incexe->name_list) {
        fname = node->c_str();
        Dmsg2(debuglevel, "Exc fname=%s ff->fname=%s\n", fname, ff->fname);
        if (bstrcmp(fname, ff->fname)) { return true; }
      }
    }
  }

  return false;
}

bool AcceptFile(FindFilesPacket* ff)
{
  int i, j, k;
  int fnm_flags;
  const char* basename;
  findFILESET* fileset = ff->fileset;
  findIncludeExcludeItem* incexe = fileset->incexe;
  int (*match_func)(const char* pattern, const char* string, int flags);

  Dmsg1(debuglevel, "enter AcceptFile: fname=%s\n", ff->fname);
  if (BitIsSet(FO_ENHANCEDWILD, ff->flags)) {
    match_func = fnmatch;
    if ((basename = last_path_separator(ff->fname)) != NULL)
      basename++;
    else
      basename = ff->fname;
  } else {
    match_func = fnmatch;
    basename = ff->fname;
  }

  for (j = 0; j < incexe->opts_list.size(); j++) {
    findFOPTS* fo;

    fo = (findFOPTS*)incexe->opts_list.get(j);
    CopyBits(FO_MAX, fo->flags, ff->flags);
    ff->Compress_algo = fo->Compress_algo;
    ff->Compress_level = fo->Compress_level;
    ff->fstypes = fo->fstype;
    ff->drivetypes = fo->Drivetype;

    fnm_flags = BitIsSet(FO_IGNORECASE, ff->flags) ? FNM_CASEFOLD : 0;
    fnm_flags |= BitIsSet(FO_ENHANCEDWILD, ff->flags) ? FNM_PATHNAME : 0;

    if (S_ISDIR(ff->statp.st_mode)) {
      for (k = 0; k < fo->wilddir.size(); k++) {
        if (match_func((char*)fo->wilddir.get(k), ff->fname, fnmode | fnm_flags)
            == 0) {
          if (BitIsSet(FO_EXCLUDE, ff->flags)) {
            Dmsg2(debuglevel, "Exclude wilddir: %s file=%s\n",
                  (char*)fo->wilddir.get(k), ff->fname);
            return false; /* reject dir */
          }
          return true; /* accept dir */
        }
      }
    } else {
      for (k = 0; k < fo->wildfile.size(); k++) {
        if (match_func((char*)fo->wildfile.get(k), ff->fname,
                       fnmode | fnm_flags)
            == 0) {
          if (BitIsSet(FO_EXCLUDE, ff->flags)) {
            Dmsg2(debuglevel, "Exclude wildfile: %s file=%s\n",
                  (char*)fo->wildfile.get(k), ff->fname);
            return false; /* reject file */
          }
          return true; /* accept file */
        }
      }

      for (k = 0; k < fo->wildbase.size(); k++) {
        if (match_func((char*)fo->wildbase.get(k), basename, fnmode | fnm_flags)
            == 0) {
          if (BitIsSet(FO_EXCLUDE, ff->flags)) {
            Dmsg2(debuglevel, "Exclude wildbase: %s file=%s\n",
                  (char*)fo->wildbase.get(k), basename);
            return false; /* reject file */
          }
          return true; /* accept file */
        }
      }
    }

    for (k = 0; k < fo->wild.size(); k++) {
      if (match_func((char*)fo->wild.get(k), ff->fname, fnmode | fnm_flags)
          == 0) {
        if (BitIsSet(FO_EXCLUDE, ff->flags)) {
          Dmsg2(debuglevel, "Exclude wild: %s file=%s\n",
                (char*)fo->wild.get(k), ff->fname);
          return false; /* reject file */
        }
        return true; /* accept file */
      }
    }

    if (S_ISDIR(ff->statp.st_mode)) {
      for (k = 0; k < fo->regexdir.size(); k++) {
        if (regexec((regex_t*)fo->regexdir.get(k), ff->fname, 0, NULL, 0)
            == 0) {
          if (BitIsSet(FO_EXCLUDE, ff->flags)) {
            return false; /* reject file */
          }
          return true; /* accept file */
        }
      }
    } else {
      for (k = 0; k < fo->regexfile.size(); k++) {
        if (regexec((regex_t*)fo->regexfile.get(k), ff->fname, 0, NULL, 0)
            == 0) {
          if (BitIsSet(FO_EXCLUDE, ff->flags)) {
            return false; /* reject file */
          }
          return true; /* accept file */
        }
      }
    }

    for (k = 0; k < fo->regex.size(); k++) {
      if (regexec((regex_t*)fo->regex.get(k), ff->fname, 0, NULL, 0) == 0) {
        if (BitIsSet(FO_EXCLUDE, ff->flags)) { return false; /* reject file */ }
        return true; /* accept file */
      }
    }

    // If we have an empty Options clause with exclude, then exclude the file
    if (BitIsSet(FO_EXCLUDE, ff->flags) && fo->regex.size() == 0
        && fo->wild.size() == 0 && fo->regexdir.size() == 0
        && fo->wilddir.size() == 0 && fo->regexfile.size() == 0
        && fo->wildfile.size() == 0 && fo->wildbase.size() == 0) {
      Dmsg1(debuglevel, "Empty options, rejecting: %s\n", ff->fname);
      return false; /* reject file */
    }
  }

  // Now apply the Exclude { } directive
  for (i = 0; i < fileset->exclude_list.size(); i++) {
    dlistString* node;
    findIncludeExcludeItem* incexe
        = (findIncludeExcludeItem*)fileset->exclude_list.get(i);

    for (j = 0; j < incexe->opts_list.size(); j++) {
      findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
      fnm_flags = BitIsSet(FO_IGNORECASE, fo->flags) ? FNM_CASEFOLD : 0;
      for (k = 0; k < fo->wild.size(); k++) {
        if (fnmatch((char*)fo->wild.get(k), ff->fname, fnmode | fnm_flags)
            == 0) {
          Dmsg1(debuglevel, "Reject wild1: %s\n", ff->fname);
          return false; /* reject file */
        }
      }
    }
    fnm_flags = (incexe->current_opts != NULL
                 && BitIsSet(FO_IGNORECASE, incexe->current_opts->flags))
                    ? FNM_CASEFOLD
                    : 0;
    foreach_dlist (node, &incexe->name_list) {
      char* fname = node->c_str();

      if (fnmatch(fname, ff->fname, fnmode | fnm_flags) == 0) {
        Dmsg1(debuglevel, "Reject wild2: %s\n", ff->fname);
        return false; /* reject file */
      }
    }
  }

  return true;
}

/**
 * The code comes here for each file examined.
 * We filter the files, then call the user's callback if the file is included.
 */
static int OurCallback(JobControlRecord* jcr,
                       FindFilesPacket* ff,
                       bool top_level)
{
  if (top_level) { return ff->FileSave(jcr, ff, top_level); /* accept file */ }
  switch (ff->type) {
    case FT_NOACCESS:
    case FT_NOFOLLOW:
    case FT_NOSTAT:
    case FT_NOCHG:
    case FT_ISARCH:
    case FT_NORECURSE:
    case FT_NOFSCHG:
    case FT_INVALIDFS:
    case FT_INVALIDDT:
    case FT_NOOPEN:
      //    return ff->FileSave(jcr, ff, top_level);

    /* These items can be filtered */
    case FT_LNKSAVED:
    case FT_REGE:
    case FT_REG:
    case FT_LNK:
    case FT_DIRBEGIN:
    case FT_DIREND:
    case FT_RAW:
    case FT_FIFO:
    case FT_SPEC:
    case FT_DIRNOCHG:
    case FT_REPARSE:
    case FT_JUNCTION:
      if (AcceptFile(ff)) {
        return ff->FileSave(jcr, ff, top_level);
      } else {
        Dmsg1(debuglevel, "Skip file %s\n", ff->fname);
        return -1; /* ignore this file */
      }

    default:
      Dmsg1(000, "Unknown FT code %d\n", ff->type);
      return 0;
  }
}

// Terminate FindFiles() and release all allocated memory
int TermFindFiles(FindFilesPacket* ff)
{
  int hard_links = 0;

  if (ff) {
    FreePoolMemory(ff->sys_fname);
    if (ff->fname_save) { FreePoolMemory(ff->fname_save); }
    if (ff->link_save) { FreePoolMemory(ff->link_save); }
    if (ff->ignoredir_fname) { FreePoolMemory(ff->ignoredir_fname); }
    hard_links = TermFindOne(ff);
    free(ff);
  }

  return hard_links;
}

// Allocate a new include/exclude block.
findIncludeExcludeItem* allocate_new_incexe(void)
{
  findIncludeExcludeItem* incexe;

  incexe = (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
  *incexe = findIncludeExcludeItem{};
  incexe->opts_list.init(1, true);
  incexe->name_list.init();
  incexe->plugin_list.init();

  return incexe;
}

// Define a new Exclude block in the FileSet
findIncludeExcludeItem* new_exclude(findFILESET* fileset)
{
  // New exclude
  fileset->incexe = allocate_new_incexe();
  fileset->exclude_list.append(fileset->incexe);

  return fileset->incexe;
}

// Define a new Include block in the FileSet
findIncludeExcludeItem* new_include(findFILESET* fileset)
{
  // New include
  fileset->incexe = allocate_new_incexe();
  fileset->include_list.append(fileset->incexe);

  return fileset->incexe;
}

/**
 * Define a new preInclude block in the FileSet.
 * That is the include is prepended to the other
 * Includes. This is used for plugin exclusions.
 */
findIncludeExcludeItem* new_preinclude(findFILESET* fileset)
{
  // New pre-include
  fileset->incexe = allocate_new_incexe();
  fileset->include_list.prepend(fileset->incexe);

  return fileset->incexe;
}

// Create a new exclude block and prepend it to the list of exclude blocks.
findIncludeExcludeItem* new_preexclude(findFILESET* fileset)
{
  // New pre-exclude
  fileset->incexe = allocate_new_incexe();
  fileset->exclude_list.prepend(fileset->incexe);

  return fileset->incexe;
}

findFOPTS* start_options(FindFilesPacket* ff)
{
  int state = ff->fileset->state;
  findIncludeExcludeItem* incexe = ff->fileset->incexe;

  if (state != state_options) {
    ff->fileset->state = state_options;
    findFOPTS* fo = (findFOPTS*)malloc(sizeof(findFOPTS));
    *fo = findFOPTS{};
    fo->regex.init(1, true);
    fo->regexdir.init(1, true);
    fo->regexfile.init(1, true);
    fo->wild.init(1, true);
    fo->wilddir.init(1, true);
    fo->wildfile.init(1, true);
    fo->wildbase.init(1, true);
    fo->base.init(1, true);
    fo->fstype.init(1, true);
    fo->Drivetype.init(1, true);
    incexe->current_opts = fo;
    incexe->opts_list.append(fo);
  }

  return incexe->current_opts;
}

// Used by plugins to define a new options block
void NewOptions(FindFilesPacket* ff, findIncludeExcludeItem* incexe)
{
  findFOPTS* fo;

  fo = (findFOPTS*)malloc(sizeof(findFOPTS));
  *fo = findFOPTS{};
  fo->regex.init(1, true);
  fo->regexdir.init(1, true);
  fo->regexfile.init(1, true);
  fo->wild.init(1, true);
  fo->wilddir.init(1, true);
  fo->wildfile.init(1, true);
  fo->wildbase.init(1, true);
  fo->base.init(1, true);
  fo->fstype.init(1, true);
  fo->Drivetype.init(1, true);
  incexe->current_opts = fo;
  incexe->opts_list.prepend(fo);
  ff->fileset->state = state_options;
}
