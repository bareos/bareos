/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, March MMIII
 */
/*
 * @file
 * Configuration file parser for new and old Include and
 * Exclude records
 */

#include "include/bareos.h"
#include "dird.h"
#include "findlib/match.h"

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif
#include "findlib/find.h"

/* Forward referenced subroutines */

/*
 * Imported subroutines
 */
extern void StoreInc(LEX *lc, ResourceItem *item, int index, int pass);

/* We build the current new Include and Exclude items here */
static IncludeExcludeItem res_incexe;

/*
 * new Include/Exclude items
 * name handler value code flags default_value
 */
ResourceItem newinc_items[] = {
   { "File", CFG_TYPE_FNAME, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Plugin", CFG_TYPE_PLUGINNAME, { 0 }, 0, 0, NULL, NULL, NULL },
   { "ExcludeDirContaining", CFG_TYPE_EXCLUDEDIR,  { 0 }, 0, 0, NULL, NULL, NULL },
   { "Options", CFG_TYPE_OPTIONS, { 0 }, 0, 0, NULL, NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Items that are valid in an Options resource
 * name handler value code flags default_value
 */
ResourceItem options_items[] = {
   { "Compression", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Signature", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "BaseJob", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Accurate", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Verify", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "OneFs", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Recurse", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Sparse", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "HardLinks", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "ReadFifo", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Replace", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Portable", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "MtimeOnly", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "KeepAtime", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Regex", CFG_TYPE_REGEX, { 0 }, 0, 0, NULL, NULL, NULL },
   { "RegexDir", CFG_TYPE_REGEX, { 0 }, 1, 0, NULL, NULL, NULL },
   { "RegexFile", CFG_TYPE_REGEX, { 0 }, 2, 0, NULL, NULL, NULL },
   { "Base", CFG_TYPE_BASE, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Wild", CFG_TYPE_WILD, { 0 }, 0, 0, NULL, NULL, NULL },
   { "WildDir", CFG_TYPE_WILD, { 0 }, 1, 0, NULL, NULL, NULL },
   { "WildFile", CFG_TYPE_WILD, { 0 }, 2, 0, NULL, NULL, NULL },
   { "Exclude", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "AclSupport", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Plugin", CFG_TYPE_PLUGIN, { 0 }, 0, 0, NULL, NULL, NULL },
   { "IgnoreCase", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "FsType", CFG_TYPE_FSTYPE, { 0 }, 0, 0, NULL, NULL, NULL },
   { "HfsPlusSupport", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "NoAtime", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "EnhancedWild", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "DriveType", CFG_TYPE_DRIVETYPE, { 0 }, 0, 0, NULL, NULL, NULL },
   { "CheckFileChanges", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "StripPath", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "HonornoDumpFlag", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "XAttrSupport", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Size", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Shadowing", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "AutoExclude", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "ForceEncryption", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL, NULL, NULL },
   { "Meta", CFG_TYPE_META, { 0 }, 0, 0, 0, NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

#include "inc_conf.h"
#include "lib/edit.h"

/**
 * determine used compression algorithms
 */
void FindUsedCompressalgos(PoolMem *compressalgos, JobControlRecord *jcr)
{
   int cnt = 0;
   IncludeExcludeItem *inc;
   FileOptions *fopts;
   FilesetResource *fs;
   struct s_fs_opt *fs_opt;

   if (!jcr->res.job || !jcr->res.job->fileset) {
      return;
   }

   fs = jcr->res.job->fileset;
   for (int i = 0; i < fs->num_includes; i++) { /* Parse all Include {} */
      inc = fs->include_items[i];

      for (int j = 0; j < inc->num_opts; j++) { /* Parse all Options {} */
         fopts = inc->opts_list[j];

         for (char *k = fopts->opts; *k; k++) { /* Try to find one request */
           switch (*k) {
            case 'Z':           /* Compression */
               for (fs_opt = FS_options; fs_opt->name; fs_opt++) {
                  if (fs_opt->keyword != INC_KW_COMPRESSION) {
                    continue;
                  }

                  if (bstrncmp(k, fs_opt->option, strlen(fs_opt->option))) {
                     if (cnt > 0) {
                        compressalgos->strcat(",");
                     } else {
                        compressalgos->strcat(" (");
                     }
                     compressalgos->strcat(fs_opt->name);
                     k += strlen(fs_opt->option) - 1;
                     cnt++;
                     continue;
                  }
               }
               break;
            default:
               break;
            }
         }
      }
   }

   if (cnt > 0) {
      compressalgos->strcat(")");
   }
}

/**
 * Check if the configured options are valid.
 */
static inline void IsInPermittedSet(LEX *lc, const char *SetType, const char *permitted_set)
{
   const char *p, *q;
   bool found;

   for (p = lc->str; *p; p++) {
      found = false;
      for (q = permitted_set; *q; q++) {
         if (*p == *q) {
            found = true;
            break;
         }
      }

      if (!found) {
         scan_err3(lc, _("Illegal %s option %c, got option string: %s:"),
                   SetType, *p, lc->str);
      }
   }
}

/**
 * Scan for right hand side of Include options (keyword=option) is
 * converted into one or two characters. Verifyopts=xxxx is Vxxxx:
 * Whatever is found is concatenated to the opts string.
 *
 * This code is also used inside an Options resource.
 */
static void ScanIncludeOptions(LEX *lc, int keyword, char *opts, int optlen)
{
   int i;
   char option[64];
   int lcopts = lc->options;
   struct s_sz_matching size_matching;

   memset(option, 0, sizeof(option));
   lc->options |= LOPT_STRING;             /* force string */
   LexGetToken(lc, BCT_STRING);            /* expect at least one option */
   if (keyword == INC_KW_VERIFY) {         /* special case */
      IsInPermittedSet(lc, _("verify"), PERMITTED_VERIFY_OPTIONS);
      bstrncat(opts, "V", optlen);         /* indicate Verify */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* Terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_ACCURATE) { /* special case */
      IsInPermittedSet(lc, _("accurate"), PERMITTED_ACCURATE_OPTIONS);
      bstrncat(opts, "C", optlen);         /* indicate Accurate */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* Terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_BASEJOB) { /* special case */
      IsInPermittedSet(lc, _("base job"), PERMITTED_BASEJOB_OPTIONS);
      bstrncat(opts, "J", optlen);         /* indicate BaseJob */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* Terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_STRIPPATH) { /* special case */
      if (!IsAnInteger(lc->str)) {
         scan_err1(lc, _("Expected a strip path positive integer, got: %s:"), lc->str);
      }
      bstrncat(opts, "P", optlen);         /* indicate strip path */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* Terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_SIZE) { /* special case */
      if (!ParseSizeMatch(lc->str, &size_matching)) {
         scan_err1(lc, _("Expected a parseable size, got: %s:"), lc->str);
      }
      bstrncat(opts, "z", optlen);         /* indicate size */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* Terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else {
      /*
       * Standard keyword options for Include/Exclude
       */
      for (i = 0; FS_options[i].name; i++) {
         if (FS_options[i].keyword == keyword && Bstrcasecmp(lc->str, FS_options[i].name)) {
            bstrncpy(option, FS_options[i].option, sizeof(option));
            i = 0;
            break;
         }
      }
      if (i != 0) {
         scan_err1(lc, _("Expected a FileSet option keyword, got: %s:"), lc->str);
      } else { /* add option */
         bstrncat(opts, option, optlen);
         Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
      }
   }
   lc->options = lcopts;

   /*
    * If option terminated by comma, eat it
    */
   if (lc->ch == ',') {
      LexGetToken(lc, BCT_ALL);      /* yes, eat comma */
   }
}

/**
 * Store regex info
 */
static void StoreRegex(LEX *lc, ResourceItem *item, int index, int pass)
{
   int token, rc;
   regex_t preg;
   char prbuf[500];
   const char *type;
   int newsize;

   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (pass == 1) {
      /* Pickup regex string
       */
      switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
         rc = regcomp(&preg, lc->str, REG_EXTENDED);
         if (rc != 0) {
            regerror(rc, &preg, prbuf, sizeof(prbuf));
            regfree(&preg);
            scan_err1(lc, _("Regex compile error. ERR=%s\n"), prbuf);
            break;
         }
         regfree(&preg);
         if (item->code == 1) {
            type = "regexdir";
            res_incexe.current_opts->regexdir.append(bstrdup(lc->str));
            newsize = res_incexe.current_opts->regexdir.size();
         } else if (item->code == 2) {
            type = "regexfile";
            res_incexe.current_opts->regexfile.append(bstrdup(lc->str));
            newsize = res_incexe.current_opts->regexfile.size();
         } else {
            type = "regex";
            res_incexe.current_opts->regex.append(bstrdup(lc->str));
            newsize = res_incexe.current_opts->regex.size();
         }
         Dmsg4(900, "set %s %p size=%d %s\n",
            type, res_incexe.current_opts, newsize, lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a regex string, got: %s\n"), lc->str);
      }
   }
   ScanToEol(lc);
}

/**
 * Store Base info
 */
static void StoreBase(LEX *lc, ResourceItem *item, int index, int pass)
{

   LexGetToken(lc, BCT_NAME);
   if (pass == 1) {
      /*
       * Pickup Base Job Name
       */
      res_incexe.current_opts->base.append(bstrdup(lc->str));
   }
   ScanToEol(lc);
}

/**
 * Store reader info
 */
static void StorePlugin(LEX *lc, ResourceItem *item, int index, int pass)
{

   LexGetToken(lc, BCT_NAME);
   if (pass == 1) {
      /*
       * Pickup plugin command
       */
      res_incexe.current_opts->plugin = bstrdup(lc->str);
   }
   ScanToEol(lc);
}

/**
 * Store Wild-card info
 */
static void StoreWild(LEX *lc, ResourceItem *item, int index, int pass)
{
   int token;
   const char *type;
   int newsize;

   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (pass == 1) {
      /*
       * Pickup Wild-card string
       */
      switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
         if (item->code == 1) {
            type = "wilddir";
            res_incexe.current_opts->wilddir.append(bstrdup(lc->str));
            newsize = res_incexe.current_opts->wilddir.size();
         } else if (item->code == 2) {
            if (strpbrk(lc->str, "/\\") != NULL) {
               type = "wildfile";
               res_incexe.current_opts->wildfile.append(bstrdup(lc->str));
               newsize = res_incexe.current_opts->wildfile.size();
            } else {
               type = "wildbase";
               res_incexe.current_opts->wildbase.append(bstrdup(lc->str));
               newsize = res_incexe.current_opts->wildbase.size();
            }
         } else {
            type = "wild";
            res_incexe.current_opts->wild.append(bstrdup(lc->str));
            newsize = res_incexe.current_opts->wild.size();
         }
         Dmsg4(9, "set %s %p size=%d %s\n",
            type, res_incexe.current_opts, newsize, lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a wild-card string, got: %s\n"), lc->str);
      }
   }
   ScanToEol(lc);
}

/**
 * Store fstype info
 */
static void StoreFstype(LEX *lc, ResourceItem *item, int index, int pass)
{
   int token;

   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (pass == 1) {
      /* Pickup fstype string */
      switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
         res_incexe.current_opts->fstype.append(bstrdup(lc->str));
         Dmsg3(900, "set fstype %p size=%d %s\n",
            res_incexe.current_opts, res_incexe.current_opts->fstype.size(), lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a fstype string, got: %s\n"), lc->str);
      }
   }
   ScanToEol(lc);
}

/**
 * Store Drivetype info
 */
static void StoreDrivetype(LEX *lc, ResourceItem *item, int index, int pass)
{
   int token;

   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (pass == 1) {
      /* Pickup Drivetype string */
      switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
         res_incexe.current_opts->Drivetype.append(bstrdup(lc->str));
         Dmsg3(900, "set Drivetype %p size=%d %s\n",
            res_incexe.current_opts, res_incexe.current_opts->Drivetype.size(), lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a Drivetype string, got: %s\n"), lc->str);
      }
   }
   ScanToEol(lc);
}

static void StoreMeta(LEX *lc, ResourceItem *item, int index, int pass)
{
   int token;

   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (pass == 1) {
      /* Pickup fstype string */
      switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
         res_incexe.current_opts->meta.append(bstrdup(lc->str));
         Dmsg3(900, "set meta %p size=%d %s\n",
            res_incexe.current_opts, res_incexe.current_opts->meta.size(), lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a meta string, got: %s\n"), lc->str);
      }
   }
   ScanToEol(lc);
}

/**
 * New style options come here
 */
static void StoreOption(LEX *lc, ResourceItem *item, int index, int pass)
{
   int i;
   int keyword;
   char inc_opts[100];

   inc_opts[0] = 0;
   keyword = INC_KW_NONE;

   /*
    * Look up the keyword
    */
   for (i = 0; FS_option_kw[i].name; i++) {
      if (Bstrcasecmp(item->name, FS_option_kw[i].name)) {
         keyword = FS_option_kw[i].token;
         break;
      }
   }

   if (keyword == INC_KW_NONE) {
      scan_err1(lc, _("Expected a FileSet keyword, got: %s"), lc->str);
   }

   /*
    * Now scan for the value
    */
   ScanIncludeOptions(lc, keyword, inc_opts, sizeof(inc_opts));
   if (pass == 1) {
      bstrncat(res_incexe.current_opts->opts, inc_opts, MAX_FOPTS);
      Dmsg2(900, "new pass=%d incexe opts=%s\n", pass, res_incexe.current_opts->opts);
   }

   ScanToEol(lc);
}

/**
 * If current_opts not defined, create first entry
 */
static void SetupCurrentOpts(void)
{
   FileOptions *fo = (FileOptions *)malloc(sizeof(FileOptions));
   memset(fo, 0, sizeof(FileOptions));
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
   fo->meta.init(1, true);
   res_incexe.current_opts = fo;
   if (res_incexe.num_opts == 0) {
      res_incexe.opts_list = (FileOptions **)malloc(sizeof(FileOptions *));
   } else {
      res_incexe.opts_list = (FileOptions **)realloc(res_incexe.opts_list,
                     sizeof(FileOptions *) * (res_incexe.num_opts + 1));
   }
   res_incexe.opts_list[res_incexe.num_opts++] = fo;
}

/**
 * Come here when Options seen in Include/Exclude
 */
static void StoreOptionsRes(LEX *lc, ResourceItem *item, int index, int pass, bool exclude)
{
   int token, i;

   if (exclude) {
      scan_err0(lc, _("Options section not permitted in Exclude\n"));
      /* NOT REACHED */
   }
   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (token != BCT_BOB) {
      scan_err1(lc, _("Expecting open brace. Got %s"), lc->str);
   }

   if (pass == 1) {
      SetupCurrentOpts();
   }

   while ((token = LexGetToken(lc, BCT_ALL)) != BCT_EOF) {
      if (token == BCT_EOL) {
         continue;
      }
      if (token == BCT_EOB) {
         break;
      }
      if (token != BCT_IDENTIFIER) {
         scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
      }
      for (i=0; options_items[i].name; i++) {
         if (Bstrcasecmp(options_items[i].name, lc->str)) {
            token = LexGetToken(lc, BCT_SKIP_EOL);
            if (token != BCT_EQUALS) {
               scan_err1(lc, _("expected an equals, got: %s"), lc->str);
            }
            /* Call item handler */
            switch (options_items[i].type) {
            case CFG_TYPE_OPTION:
               StoreOption(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_REGEX:
               StoreRegex(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_BASE:
               StoreBase(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_WILD:
               StoreWild(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_PLUGIN:
               StorePlugin(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_FSTYPE:
               StoreFstype(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_DRIVETYPE:
               StoreDrivetype(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_META:
               StoreMeta(lc, &options_items[i], i, pass);
               break;
            default:
               break;
            }
            i = -1;
            break;
         }
      }
      if (i >=0) {
         scan_err1(lc, _("Keyword %s not permitted in this resource"), lc->str);
      }
   }
}

/**
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void StoreFname(LEX *lc, ResourceItem *item, int index, int pass, bool exclude)
{
   int token;
   IncludeExcludeItem *incexe;
   UnionOfResources *res_all = (UnionOfResources *)my_config->res_all_;

   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (pass == 1) {
      /* Pickup Filename string
       */
      switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
         if (strchr(lc->str, '\\')) {
            scan_err1(lc, _("Backslash found. Use forward slashes or quote the string.: %s\n"), lc->str);
            /* NOT REACHED */
         }
      case BCT_QUOTED_STRING:
         if (res_all->res_fs.have_MD5) {
            MD5_Update(&res_all->res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
         }
         incexe = &res_incexe;
         if (incexe->name_list.size() == 0) {
            incexe->name_list.init(10, true);
         }
         incexe->name_list.append(bstrdup(lc->str));
         Dmsg1(900, "Add to name_list %s\n", lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a filename, got: %s"), lc->str);
      }
   }
   ScanToEol(lc);
}

/**
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void StorePluginName(LEX *lc, ResourceItem *item, int index, int pass, bool exclude)
{
   int token;
   IncludeExcludeItem *incexe;
   UnionOfResources *res_all = (UnionOfResources *)my_config->res_all_;

   if (exclude) {
      scan_err0(lc, _("Plugin directive not permitted in Exclude\n"));
      /* NOT REACHED */
   }
   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (pass == 1) {
      /* Pickup Filename string
       */
      switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
         if (strchr(lc->str, '\\')) {
            scan_err1(lc, _("Backslash found. Use forward slashes or quote the string.: %s\n"), lc->str);
            /* NOT REACHED */
         }
      case BCT_QUOTED_STRING:
         if (res_all->res_fs.have_MD5) {
            MD5_Update(&res_all->res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
         }
         incexe = &res_incexe;
         if (incexe->plugin_list.size() == 0) {
            incexe->plugin_list.init(10, true);
         }
         incexe->plugin_list.append(bstrdup(lc->str));
         Dmsg1(900, "Add to plugin_list %s\n", lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a filename, got: %s"), lc->str);
         /* NOT REACHED */
      }
   }
   ScanToEol(lc);
}

/**
 * Store exclude directory containing info
 */
static void StoreExcludedir(LEX *lc, ResourceItem *item, int index, int pass, bool exclude)
{
   IncludeExcludeItem *incexe;

   if (exclude) {
      scan_err0(lc, _("ExcludeDirContaining directive not permitted in Exclude.\n"));
      /* NOT REACHED */
      return;
   }

   LexGetToken(lc, BCT_NAME);
   if (pass == 1) {
      incexe = &res_incexe;
      if (incexe->ignoredir.size() == 0) {
         incexe->ignoredir.init(10, true);
      }
      incexe->ignoredir.append(bstrdup(lc->str));
      Dmsg1(900, "Add to ignoredir_list %s\n", lc->str);
   }
   ScanToEol(lc);
}

/**
 * Store new style FileSet Include/Exclude info
 *
 *  Note, when this routine is called, we are inside a FileSet
 *  resource.  We treat the Include/Exclude like a sort of
 *  mini-resource within the FileSet resource.
 */
static void StoreNewinc(LEX *lc, ResourceItem *item, int index, int pass)
{
   bool options;
   int token, i;
   IncludeExcludeItem *incexe;
   UnionOfResources *res_all = (UnionOfResources *)my_config->res_all_;

   if (!res_all->res_fs.have_MD5) {
      MD5_Init(&res_all->res_fs.md5c);
      res_all->res_fs.have_MD5 = true;
   }
   memset(&res_incexe, 0, sizeof(res_incexe));
   res_all->res_fs.new_include = true;
   while ((token = LexGetToken(lc, BCT_SKIP_EOL)) != BCT_EOF) {
      if (token == BCT_EOB) {
         break;
      }
      if (token != BCT_IDENTIFIER) {
         scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
      }
      for (i=0; newinc_items[i].name; i++) {
         options = Bstrcasecmp(lc->str, "options");
         if (Bstrcasecmp(newinc_items[i].name, lc->str)) {
            if (!options) {
               token = LexGetToken(lc, BCT_SKIP_EOL);
               if (token != BCT_EQUALS) {
                  scan_err1(lc, _("expected an equals, got: %s"), lc->str);
               }
            }

            /* Call item handler */
            switch (newinc_items[i].type) {
            case CFG_TYPE_FNAME:
               StoreFname(lc, &newinc_items[i], i, pass, item->code);
               break;
            case CFG_TYPE_PLUGINNAME:
               StorePluginName(lc, &newinc_items[i], i, pass, item->code);
               break;
            case CFG_TYPE_EXCLUDEDIR:
               StoreExcludedir(lc, &newinc_items[i], i, pass, item->code);
               break;
            case CFG_TYPE_OPTIONS:
               StoreOptionsRes(lc, &newinc_items[i], i, pass, item->code);
               break;
            default:
               break;
            }
            i = -1;
            break;
         }
      }
      if (i >=0) {
         scan_err1(lc, _("Keyword %s not permitted in this resource"), lc->str);
      }
   }
   if (pass == 1) {
      incexe = (IncludeExcludeItem *)malloc(sizeof(IncludeExcludeItem));
      memcpy(incexe, &res_incexe, sizeof(IncludeExcludeItem));
      memset(&res_incexe, 0, sizeof(res_incexe));
      if (item->code == 0) { /* include */
         if (res_all->res_fs.num_includes == 0) {
            res_all->res_fs.include_items = (IncludeExcludeItem **)malloc(sizeof(IncludeExcludeItem *));
         } else {
            res_all->res_fs.include_items = (IncludeExcludeItem **)realloc(res_all->res_fs.include_items,
                           sizeof(IncludeExcludeItem *) * (res_all->res_fs.num_includes + 1));
         }
         res_all->res_fs.include_items[res_all->res_fs.num_includes++] = incexe;
         Dmsg1(900, "num_includes=%d\n", res_all->res_fs.num_includes);
      } else {    /* exclude */
         if (res_all->res_fs.num_excludes == 0) {
            res_all->res_fs.exclude_items = (IncludeExcludeItem **)malloc(sizeof(IncludeExcludeItem *));
         } else {
            res_all->res_fs.exclude_items = (IncludeExcludeItem **)realloc(res_all->res_fs.exclude_items,
                           sizeof(IncludeExcludeItem *) * (res_all->res_fs.num_excludes + 1));
         }
         res_all->res_fs.exclude_items[res_all->res_fs.num_excludes++] = incexe;
         Dmsg1(900, "num_excludes=%d\n", res_all->res_fs.num_excludes);
      }
   }
   ScanToEol(lc);
   SetBit(index, res_all->hdr.item_present);
   ClearBit(index, res_all->hdr.inherit_content);
}

/**
 * Store FileSet Include/Exclude info
 *  new style includes are handled in StoreNewinc()
 */
void StoreInc(LEX *lc, ResourceItem *item, int index, int pass)
{
   int token;

   /*
    * Decide if we are doing a new Include or an old include. The
    *  new Include is followed immediately by open brace, whereas the
    *  old include has options following the Include.
    */
   token = LexGetToken(lc, BCT_SKIP_EOL);
   if (token == BCT_BOB) {
      StoreNewinc(lc, item, index, pass);
      return;
   }
   scan_err0(lc, _("Old style Include/Exclude not supported\n"));
}

#ifdef HAVE_JANSSON
json_t *json_incexc(const int type)
{
   return json_datatype(type, newinc_items);
}

json_t *json_options(const int type)
{
   return json_datatype(type, options_items);
}
#endif
