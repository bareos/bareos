/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
#include "lib/berrno.h"
#include "hardlink.h"

#include <memory>  // unique_ptr
#include <initializer_list>
#include <utility> // pair

#if defined(HAVE_DARWIN_OS)
/* the MacOS linker wants symbols for the destructors of these two types, so we
 * have to force template instantiation. */
template class alist<findFOPTS*>;
template class dlist<dlistString>;
#endif

static const int debuglevel = 450;

int32_t name_max; /* filename max length */
int32_t path_max; /* path name max length */

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

static void SetupFlags(FindFilesPacket* ff, findIncludeExcludeItem* incexe)
{
  // Here, we reset some values between two different Include{}
  strcpy(ff->VerifyOpts, "V");
  strcpy(ff->AccurateOpts, "Cmcs");  /* mtime+ctime+size by default */
  strcpy(ff->BaseJobOpts, "Jspug5"); /* size+perm+user+group+chk  */
  ff->plugin = NULL;
  ff->opt_plugin = false;

  /* By setting all options, we in effect OR the global options which is
   * what we want. */
  for (int j = 0; j < incexe->opts_list.size(); j++) {
    findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
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
      bstrncpy(ff->AccurateOpts, fo->AccurateOpts, sizeof(ff->AccurateOpts));
    }
    if (fo->BaseJobOpts[0]) {
      bstrncpy(ff->BaseJobOpts, fo->BaseJobOpts, sizeof(ff->BaseJobOpts));
    }
  }
}

/**
 * The code comes here for each file examined.
 * We filter the files, then call the user's callback if the file is included.
 */
#include <functional>
static auto CreateCallback(std::function<int(JobControlRecord* jcr,
                                             FindFilesPacket* ff,
                                             bool top_level)> SaveFile)
{
  return
      [SaveFile](JobControlRecord* jcr, FindFilesPacket* ff, bool top_level) {
        if (top_level) {
          return SaveFile(jcr, ff, top_level); /* accept file */
        }
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
              return SaveFile(jcr, ff, top_level);
            } else {
              Dmsg1(debuglevel, "Skip file %s\n", ff->fname);
              return -1; /* ignore this file */
            }

          default:
            Dmsg1(000, "Unknown FT code %d\n", ff->type);
            return 0;
        }
      };
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
  /* This is the new way */
  findFILESET* fileset = ff->fileset;
  if (fileset) {
    /* TODO: We probably need be move the initialization in the fileset loop,
     * at this place flags options are "concatenated" accross Include {} blocks
     * (not only Options{} blocks inside a Include{}) */
    ClearAllBits(FO_MAX, ff->flags);
    for (int i = 0; i < fileset->include_list.size(); i++) {
      dlistString* node;
      findIncludeExcludeItem* incexe
          = (findIncludeExcludeItem*)fileset->include_list.get(i);
      fileset->incexe = incexe;
      SetupFlags(ff, incexe);

      Dmsg4(50, "Verify=<%s> Accurate=<%s> BaseJob=<%s> flags=<%d>\n",
            ff->VerifyOpts, ff->AccurateOpts, ff->BaseJobOpts, ff->flags);

      foreach_dlist (node, &incexe->name_list) {
        char* fname = node->c_str();

        Dmsg1(debuglevel, "F %s\n", fname);
        ff->top_fname = fname;
        if (FindOneFile(jcr, ff, CreateCallback(FileSave), ff->top_fname,
                        (dev_t)-1, true)
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

std::optional<const char*> FindMatch(int (*match_func)(const char* pattern, const char* str, int flags),
		   alist<const char*>& patterns, const char* str, int flags)
{
      for (int k = 0; k < patterns.size(); k++) {
	const char* pattern = patterns.get(k);
        if (match_func((char*)pattern, str, flags) == 0) {
		return std::make_optional(pattern);
        }
      }
      return std::nullopt;
}

std::optional<const regex_t*> FindRegexMatch(alist<regex_t*>& regexs, const char* str)
{
      for (int k = 0; k < regexs.size(); k++) {
	regex_t* regex = regexs.get(k);
        if (regexec(regex, str, 0, NULL, 0) == 0) {
		return std::make_optional(regex);
	}
      }
      return std::nullopt;
}

bool AcceptFile(FindFilesPacket* ff)
{
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

  for (int j = 0; j < incexe->opts_list.size(); j++) {
    findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
    CopyBits(FO_MAX, fo->flags, ff->flags);
    ff->Compress_algo = fo->Compress_algo;
    ff->Compress_level = fo->Compress_level;
    ff->fstypes = fo->fstype;
    ff->drivetypes = fo->Drivetype;

    const int fnm_flags = (BitIsSet(FO_IGNORECASE, ff->flags) ? FNM_CASEFOLD : 0)
	    | (BitIsSet(FO_ENHANCEDWILD, ff->flags) ? FNM_PATHNAME : 0);

    bool do_exclude = BitIsSet(FO_EXCLUDE, ff->flags);

    if (S_ISDIR(ff->statp.st_mode)) {
	    for (auto [patterns, name] : std::initializer_list<std::pair<alist<const char*>&, const char*>>{
			    {fo->wilddir, "wilddir"},
			    {fo->wild, "wild"}
			    })
	    {
		    if (std::optional match = FindMatch(match_func, patterns, ff->fname, fnmode | fnm_flags); match)
		    {
			    if (do_exclude) {
				    Dmsg2(debuglevel, "Exclude %s: %s file=%s\n",
					  name, (char*)match.value(), ff->fname);
			    }
			    return !do_exclude;
		    }
	    }

	    for (auto [patterns, name] : std::initializer_list<std::pair<alist<regex_t*>&, const char*>>{
			    {fo->regexdir, "regexdir"},
			    {fo->regex, "regex"}
			    })
	    {
		    if (std::optional match = FindRegexMatch(patterns, ff->fname); match)
		    {
			    if (do_exclude) {
				    Dmsg2(debuglevel, "Exclude %s: file=%s\n",
					  name, ff->fname);
			    }
			    return !do_exclude;
		    }
	    }
    } else {
	    for (auto [patterns, name] : std::initializer_list<std::pair<alist<const char*>&, const char*>>{
			    {fo->wildfile, "wildfile"},
			    {fo->wildbase, "wildbase"},
			    {fo->wild, "wild"}
			    })
	    {
		    if (std::optional match = FindMatch(match_func, patterns, ff->fname, fnmode | fnm_flags); match)
		    {
			    if (do_exclude) {
				    Dmsg2(debuglevel, "Exclude %s: %s file=%s\n",
					  name, (char*)match.value(), ff->fname);
			    }
			    return !do_exclude;
		    }
	    }
	    for (auto [patterns, name] : std::initializer_list<std::pair<alist<regex_t*>&, const char*>>{
			    {fo->regexfile, "regexfile"},
			    {fo->regex, "regex"}
			    })
	    {
		    if (std::optional match = FindRegexMatch(patterns, ff->fname); match)
		    {
			    if (do_exclude) {
				    Dmsg2(debuglevel, "Exclude %s: file=%s\n",
					  name, ff->fname);
			    }
			    return !do_exclude;
		    }
	    }
    }

    // If we have an empty Options clause with exclude, then exclude the file
    if (do_exclude && fo->regex.size() == 0
        && fo->wild.size() == 0 && fo->regexdir.size() == 0
        && fo->wilddir.size() == 0 && fo->regexfile.size() == 0
        && fo->wildfile.size() == 0 && fo->wildbase.size() == 0) {
      Dmsg1(debuglevel, "Empty options, rejecting: %s\n", ff->fname);
      return false; /* reject file */
    }
  }

  // Now apply the Exclude { } directive
  for (int i = 0; i < fileset->exclude_list.size(); i++) {
    dlistString* node;
    findIncludeExcludeItem* incexe
        = (findIncludeExcludeItem*)fileset->exclude_list.get(i);

    for (int j = 0; j < incexe->opts_list.size(); j++) {
      findFOPTS* fo = (findFOPTS*)incexe->opts_list.get(j);
      const int fnm_flags = BitIsSet(FO_IGNORECASE, fo->flags) ? FNM_CASEFOLD : 0;
      for (int k = 0; k < fo->wild.size(); k++) {
        if (fnmatch((char*)fo->wild.get(k), ff->fname, fnmode | fnm_flags)
            == 0) {
          Dmsg1(debuglevel, "Reject wild1: %s\n", ff->fname);
          return false; /* reject file */
        }
      }
    }
    const int fnm_flags = (incexe->current_opts != NULL
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

// Terminate FindFiles() and release all allocated memory
void TermFindFiles(FindFilesPacket* ff)
{
  if (ff) {
    FreePoolMemory(ff->sys_fname);
    if (ff->ignoredir_fname) { FreePoolMemory(ff->ignoredir_fname); }
    TermFindOne(ff);
    free(ff);
  }
}

// Allocate a new include/exclude block.
findIncludeExcludeItem* allocate_new_incexe(void)
{
  findIncludeExcludeItem* incexe
      = (findIncludeExcludeItem*)malloc(sizeof(findIncludeExcludeItem));
  new (incexe) findIncludeExcludeItem{};

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

auto SaveInList(channel::in<stated_file>& in, std::atomic<std::size_t>& num_skipped)
{
  return [&in, &num_skipped](JobControlRecord* jcr, FindFilesPacket* ff_pkt, bool) {
    switch (ff_pkt->type) {
      case FT_LNKSAVED: /* Hard linked, file already saved */
        Dmsg2(130, "FT_LNKSAVED hard link: %s => %s\n", ff_pkt->fname,
              ff_pkt->link);
        break;
      case FT_REGE:
        Dmsg1(130, "FT_REGE saving: %s\n", ff_pkt->fname);
        break;
      case FT_REG:
        Dmsg1(130, "FT_REG saving: %s\n", ff_pkt->fname);
        break;
      case FT_LNK:
        Dmsg2(130, "FT_LNK saving: %s -> %s\n", ff_pkt->fname, ff_pkt->link);
        break;
      case FT_RESTORE_FIRST:
        Dmsg1(100, "FT_RESTORE_FIRST saving: %s\n", ff_pkt->fname);
        break;
      case FT_PLUGIN_CONFIG:
        Dmsg1(100, "FT_PLUGIN_CONFIG saving: %s\n", ff_pkt->fname);
        break;
      case FT_DIRBEGIN:
	/* this is skipped, so num_skipped is not increased */
        return 1; /* not used */
      case FT_NORECURSE:
        Jmsg(jcr, M_INFO, 1,
             _("     Recursion turned off. Will not descend from %s into %s\n"),
             ff_pkt->top_fname, ff_pkt->fname);
        ff_pkt->type = FT_DIREND; /* Backup only the directory entry */
        break;
      case FT_NOFSCHG:
        /* Suppress message for /dev filesystems */
        if (!IsInFileset(ff_pkt)) {
          Jmsg(jcr, M_INFO, 1,
               _("     %s is a different filesystem. Will not descend from %s "
                 "into it.\n"),
               ff_pkt->fname, ff_pkt->top_fname);
        }
        ff_pkt->type = FT_DIREND; /* Backup only the directory entry */
        break;
      case FT_INVALIDFS:
        Jmsg(
            jcr, M_INFO, 1,
            _("     Disallowed filesystem. Will not descend from %s into %s\n"),
            ff_pkt->top_fname, ff_pkt->fname);
        ff_pkt->type = FT_DIREND; /* Backup only the directory entry */
        break;
      case FT_INVALIDDT:
        Jmsg(jcr, M_INFO, 1,
             _("     Disallowed drive type. Will not descend into %s\n"),
             ff_pkt->fname);
        break;
      case FT_REPARSE:
      case FT_JUNCTION:
      case FT_DIREND:
        Dmsg1(130, "FT_DIREND: %s\n", ff_pkt->link);
        break;
      case FT_SPEC:
        Dmsg1(130, "FT_SPEC saving: %s\n", ff_pkt->fname);
        if (S_ISSOCK(ff_pkt->statp.st_mode)) {
          Jmsg(jcr, M_SKIPPED, 1, _("     Socket file skipped: %s\n"),
               ff_pkt->fname);
	  num_skipped++;
          return 1;
        }
        break;
      case FT_RAW:
        Dmsg1(130, "FT_RAW saving: %s\n", ff_pkt->fname);
        break;
      case FT_FIFO:
        Dmsg1(130, "FT_FIFO saving: %s\n", ff_pkt->fname);
        break;
      case FT_NOACCESS: {
        BErrNo be;
        Jmsg(jcr, M_NOTSAVED, 0, _("     Could not access \"%s\": ERR=%s\n"),
             ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
        jcr->JobErrors++;
	num_skipped++;
        return 1;
      }
      case FT_NOFOLLOW: {
        BErrNo be;
        Jmsg(jcr, M_NOTSAVED, 0,
             _("     Could not follow link \"%s\": ERR=%s\n"), ff_pkt->fname,
             be.bstrerror(ff_pkt->ff_errno));
        jcr->JobErrors++;
	num_skipped++;
        return 1;
      }
      case FT_NOSTAT: {
        BErrNo be;
        Jmsg(jcr, M_NOTSAVED, 0, _("     Could not stat \"%s\": ERR=%s\n"),
             ff_pkt->fname, be.bstrerror(ff_pkt->ff_errno));
        jcr->JobErrors++;
	num_skipped++;
        return 1;
      }
      case FT_DIRNOCHG:
      case FT_NOCHG:
        Jmsg(jcr, M_SKIPPED, 1, _("     Unchanged file skipped: %s\n"),
             ff_pkt->fname);
	num_skipped++;
        return 1;
      case FT_ISARCH:
        Jmsg(jcr, M_NOTSAVED, 0, _("     Archive file not saved: %s\n"),
             ff_pkt->fname);
        return 1;
      case FT_NOOPEN: {
        BErrNo be;
        Jmsg(jcr, M_NOTSAVED, 0,
             _("     Could not open directory \"%s\": ERR=%s\n"), ff_pkt->fname,
             be.bstrerror(ff_pkt->ff_errno));
        jcr->JobErrors++;
	num_skipped++;
        return 1;
      }
      case FT_DELETED:
        Dmsg1(130, "FT_DELETED: %s\n", ff_pkt->fname);
        break;
      default:
        Jmsg(jcr, M_NOTSAVED, 0,
             _("     Unknown file type %d; not saved: %s\n"), ff_pkt->type,
             ff_pkt->fname);
        jcr->JobErrors++;
	num_skipped++;
        return 1;
    }

    try {
	    in.put({ff_pkt->fname, ff_pkt->statp, ff_pkt->delta_seq, ff_pkt->type,
			    ff_pkt->volhas_attrlist ? std::make_optional(ff_pkt->hfsinfo) : std::nullopt});
    } catch (...) {
      num_skipped++;
      return 0;
    }
    return 1;
  };
}

#include <thread>

class bomb {
public:
	bomb(std::atomic<bool>& ref) : target(ref), defused(false) {}
	void defuse() { defused = true; }
	~bomb() { if (!defused) { target = false; } }
private:
	std::atomic<bool>& target;
	bool defused;
};

static void ListFromIncexe(JobControlRecord* jcr,
			   findFILESET* fileset,
			   FindFilesPacket* ff,
			   findIncludeExcludeItem* incexe,
			   channel::in<stated_file> in,
			   std::atomic<bool>& all_ok,
			   std::atomic<std::size_t>& num_skipped)
{
	bomb b(all_ok);
	dlistString* node;
	fileset->incexe = incexe;
	SetupFlags(ff, incexe);

	Dmsg4(50, "Verify=<%s> Accurate=<%s> BaseJob=<%s> flags=<%d>\n",
	      ff->VerifyOpts, ff->AccurateOpts, ff->BaseJobOpts, ff->flags);

	foreach_dlist (node, &incexe->name_list) {
		char* fname = (char*)node->c_str();
		Dmsg1(debuglevel, "F %s\n", fname);
		ff->top_fname = fname;
		if (FindOneFile(jcr, ff, CreateCallback(SaveInList(in, num_skipped)),
				ff->top_fname, (dev_t)-1, true)
		    == 0) {
			return;
		}
		if (JobCanceled(jcr)) { return; }
	}
	b.defuse();
}

std::optional<std::size_t>
ListFiles(JobControlRecord* jcr,
	  findFILESET* fileset,
	  bool incremental,
	  time_t save_time,
	  std::optional<bool (*)(JobControlRecord*, FindFilesPacket*)> check_changed,
	  std::vector<channel::in<stated_file>> ins)
{
  ASSERT(ins.size() == (std::size_t)fileset->include_list.size());
  std::atomic<std::size_t> num_skipped{0};
  std::atomic<bool> all_ok = true;
  /* This is the new way */
  if (fileset) {
    struct ff_cleanup {
      void operator()(FindFilesPacket* ff) { TermFindFiles(ff); }
    };
    std::vector<std::unique_ptr<FindFilesPacket, ff_cleanup>> ffs;
    std::vector<std::thread> listing_threads;
	    /* TODO: We probably need be move the initialization in the fileset loop,
     * at this place flags options are "concatenated" accross Include {} blocks
     * (not only Options{} blocks inside a Include{}) */
    for (int i = 0; i < fileset->include_list.size(); i++) {
	    auto ff_pkt = ffs.emplace_back(init_find_files(),
					   ff_cleanup{}).get();
	    ClearAllBits(FO_MAX, ff_pkt->flags);
	    ff_pkt->fileset     = fileset;
	    SetFindOptions(ff_pkt, incremental, save_time);
	    if (check_changed) SetFindChangedFunction(ff_pkt, check_changed.value());


	    listing_threads.emplace_back(ListFromIncexe,
					 jcr,
					 fileset,
					 ff_pkt,
					 (findIncludeExcludeItem*)fileset->include_list.get(i),
					 std::move(ins[i]),
					 std::ref(all_ok),
					 std::ref(num_skipped)
					);

    }
    for (auto& thread : listing_threads)
    {
	    thread.join();
    }
    if (!all_ok)
	    return std::nullopt;
    else
	    return std::optional{num_skipped.load()};
  } else {
    return std::nullopt;
  }
}

int SendPluginInfo(JobControlRecord* jcr,
		   FindFilesPacket* ff,
		   int PluginSave(JobControlRecord*, FindFilesPacket*, bool))
{
	findFILESET* fileset = ff->fileset;
    for (int i = 0; i < fileset->include_list.size(); i++) {
      dlistString* node;
      findIncludeExcludeItem* incexe
          = (findIncludeExcludeItem*)fileset->include_list.get(i);
      fileset->incexe = incexe;
      SetupFlags(ff, incexe);
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

    return 1;
}

static bool CanBeHardLinked(struct stat& statp)
{
	bool hardlinked = false;
	switch (statp.st_mode & S_IFMT)
	{
		case S_IFREG:
		case S_IFCHR:
		case S_IFBLK:
		case S_IFIFO:
#ifdef S_IFSOCK
		case S_IFSOCK:
#endif
		{
			hardlinked = true;
		} break;
	}
	return hardlinked;
}

static bool SetupLink(FindFilesPacket* ff)
{
	switch (ff->statp.st_mode & S_IFMT)
	{
	case S_IFDIR: {
		std::size_t len = strlen(ff->fname);
		std::size_t max_len = len + 2; // for trailing / and 0
		ff->link = (char*) malloc(max_len);
		if (!ff->link) return false;
		bstrncpy(ff->link, ff->fname, max_len);

		// strip all trailing slashes and then add one
		while (len >= 1 && IsPathSeparator(ff->link[len - 1])) { len--; }
		ff->link[len++] = '/'; /* add back one */
		ff->link[len] = '\0';
	} break;
	case S_IFLNK: {
		ff->link = (char*)malloc(path_max + name_max + 102);
		if (!ff->link) return false;
		int size = readlink(ff->fname, ff->link, path_max + name_max + 101);
		if (size < 0) {
			free(ff->link);
			return false;
		}
		ff->link[size] = 0;
	} break;
	default: {
		// this check is *should* be the same as ff->type != LNKSAVED
		if (ff->link == nullptr) ff->link = ff->fname;
	} break;
	}
	return true;
}

static void CleanupLink(FindFilesPacket* ff)
{
	switch (ff->statp.st_mode & S_IFMT)
	{
	case S_IFDIR:
	case S_IFLNK: {
		free(ff->link);
	} break;
	}
}

static bool SetupFFPkt(JobControlRecord* jcr,
		       FindFilesPacket* ff, char *fname, struct stat& statp,
		       int delta_seq, int type, std::optional<HfsPlusInfo> hfsinfo)
{
	ff->fname     = fname;
	ff->statp     = statp;
	ff->delta_seq = delta_seq;
	ff->type      = type;

	ff->LinkFI  = 0;
	ff->no_read = false;
	ff->linked  = nullptr;
	ff->link    = nullptr; // we may need to allocate memory here instead
	if (!BitIsSet(FO_NO_HARDLINK, ff->flags) && statp.st_nlink > 1
	    && CanBeHardLinked(statp)) {
		CurLink* hl = lookup_hardlink(jcr, ff, statp.st_ino,
					      statp.st_dev);
		if (hl) {
			/* If we have already backed up the hard linked file don't do it
			 * again */
			if (bstrcmp(hl->name, fname)) {
				Dmsg2(400, "== Name identical skip FI=%d file=%s\n",
				      hl->FileIndex, fname);
				return false;
			} else {
				ff->link = hl->name;
				ff->type
					= FT_LNKSAVED; /* Handle link, file already saved */
				ff->LinkFI = hl->FileIndex;
				ff->linked = NULL;
				ff->digest = hl->digest;
				ff->digest_stream = hl->digest_stream;
				ff->digest_len = hl->digest_len;

				Dmsg3(400, "FT_LNKSAVED FI=%d LinkFI=%d file=%s\n",
				      ff->FileIndex, hl->FileIndex, hl->name);
			}
		} else {
			// File not previously dumped. Chain it into our list.
			hl = new_hardlink(jcr, ff, fname, ff->statp.st_ino,
					  ff->statp.st_dev);
			ff->linked = hl; /* Mark saved link */
			Dmsg2(400, "Added to hash FI=%d file=%s\n", ff->FileIndex,
			      hl->name);
		}
	}

	if (!SetupLink(ff)) return false;

	if (hfsinfo)
	{
		ff->volhas_attrlist = true;
		ff->hfsinfo = hfsinfo.value();
	}
	else
	{
		ff->volhas_attrlist = false;
	}

	return true;
}

int SendFiles(JobControlRecord* jcr,
              FindFilesPacket* ff,
              std::vector<channel::out<stated_file>> outs,
              int FileSave(JobControlRecord*, FindFilesPacket* ff_pkt, bool),
              int PluginSave(JobControlRecord*, FindFilesPacket* ff_pkt, bool))
{
  /* This is the new way */
  findFILESET* fileset = ff->fileset;
  if (fileset) {
    /* TODO: We probably need be move the initialization in the fileset loop,
     * at this place flags options are "concatenated" accross Include {} blocks
     * (not only Options{} blocks inside a Include{}) */
    ASSERT(outs.size() == (std::size_t)fileset->include_list.size());
    ClearAllBits(FO_MAX, ff->flags);
    SendPluginInfo(jcr, ff, PluginSave);
    ClearAllBits(FO_MAX, ff->flags);
    int closed_channels = 0;
    while (closed_channels != fileset->include_list.size())
    {
	    for (int i = 0; i < fileset->include_list.size(); i++) {
		    channel::out<stated_file>& list = outs[i];
		    if (list.empty()) continue;
		    findIncludeExcludeItem* incexe
			    = (findIncludeExcludeItem*)fileset->include_list.get(i);
		    fileset->incexe = incexe;

		    // we only send the files that were supplied to us.
		    // for this reason we disable recursion here!

		    std::optional<stated_file> file;
		    while ((file = list.try_get())) {
			    char* fname = file->name.data();
			    Dmsg1(debuglevel, "F %s\n", fname);
			    if (!SetupFFPkt(jcr, ff, fname, file->statp, file->delta_seq,
					    file->type, file->hfsinfo))
			    {
				    Dmsg1(debuglevel, "Error: Could not setup ffpkt for file %s",
					  ff->fname);
				    return 0;
			    }
			    if (!AcceptFile(ff))
			    {
				    Dmsg1(debuglevel, "Did not accept file %s; skipping.",
					 ff->fname);
				    continue;
			    }
			    else
			    {
				    Dmsg4(50, "flags=<%d>\n", ff->flags);
			    }
			    binit(&ff->bfd);
			    if (BitIsSet(FO_PORTABLE, ff->flags)) {
				    SetPortableBackup(&ff->bfd); /* disable Win32 BackupRead() */
			    }
			    bool do_read = false;
			    {
				    /* Open any file with data that we intend to save, then save it.
				     *
				     * Note, if is_win32_backup, we must open the Directory so that
				     * the BackupRead will save its permissions and ownership streams. */
				    if (ff->type != FT_LNKSAVED && S_ISREG(ff->statp.st_mode)) {
#ifdef HAVE_WIN32
					    do_read = !IsPortableBackup(&ff->bfd) || ff->statp.st_size > 0;
#else
					    do_read = ff->statp.st_size > 0;
#endif
				    } else if (ff->type == FT_RAW || ff->type == FT_FIFO
					       || ff->type == FT_REPARSE || ff->type == FT_JUNCTION
					       || (!IsPortableBackup(&ff->bfd)
						   && ff->type == FT_DIREND)) {
					    do_read = true;
				    }
			    }
			    // fifos will block on open until some data is
			    // written
			    // insead of handling this here, we handle it in
			    // the FileSave procedure
			    // is this a good idea ?
			    if (do_read && ff->type != FT_FIFO)
			    {
				    int noatime = BitIsSet(FO_NOATIME, ff->flags) ? O_NOATIME : 0;
				    if (bopen(&ff->bfd, ff->fname, O_RDONLY | O_BINARY | noatime, 0,
					      ff->statp.st_rdev) < 0)
				    {
					    ff->ff_errno = errno;
					    BErrNo be;
					    Jmsg(jcr, M_NOTSAVED, 0, _("     Cannot open \"%s\": ERR=%s.\n"),
						 ff->fname, be.bstrerror());
					    jcr->JobErrors++;
					    CleanupLink(ff);
					    continue;
				    }
			    }
			    if (!FileSave(jcr, ff, false))
			    {
				    if (IsBopen(&ff->bfd))
				    {
					    bclose(&ff->bfd);
				    }
				    CleanupLink(ff);
				    Dmsg1(debuglevel, "Error: Could not save file %s",
					  ff->fname);
				    return 0;
			    }
			    else
			    {
				    CleanupLink(ff);
				    if (ff->linked) { ff->linked->FileIndex = ff->FileIndex; }
			    }
			    if (JobCanceled(jcr)) { return 0; }
		    }
		    if (list.empty()) closed_channels += 1;
	    }

    }
  }
  return 1;
}
