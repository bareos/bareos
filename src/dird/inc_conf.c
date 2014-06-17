/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Configuration file parser for new and old Include and
 * Exclude records
 *
 * Kern Sibbald, March MMIII
 */

#include "bareos.h"
#include "dird.h"
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
extern void store_inc(LEX *lc, RES_ITEM *item, int index, int pass);

/* We build the current new Include and Exclude items here */
static INCEXE res_incexe;

/*
 * new Include/Exclude items
 * name handler value code flags default_value
 */
static RES_ITEM newinc_items[] = {
   { "file", CFG_TYPE_FNAME, { 0 }, 0, 0, NULL },
   { "plugin", CFG_TYPE_PLUGINNAME, { 0 }, 0, 0, NULL },
   { "excludedircontaining", CFG_TYPE_EXCLUDEDIR,  { 0 }, 0, 0, NULL },
   { "options", CFG_TYPE_OPTIONS, { 0 }, 0, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Items that are valid in an Options resource
 * name handler value code flags default_value
 */
static RES_ITEM options_items[] = {
   { "compression", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "signature", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "basejob", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "accurate", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "verify", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "onefs", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "recurse", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "sparse", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "hardlinks", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "readfifo", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "replace", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "portable", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "mtimeonly", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "keepatime", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "regex", CFG_TYPE_REGEX, { 0 }, 0, 0, NULL },
   { "regexdir", CFG_TYPE_REGEX, { 0 }, 1, 0, NULL },
   { "regexfile", CFG_TYPE_REGEX, { 0 }, 2, 0, NULL },
   { "base", CFG_TYPE_BASE, { 0 }, 0, 0, NULL },
   { "wild", CFG_TYPE_WILD, { 0 }, 0, 0, NULL },
   { "wilddir", CFG_TYPE_WILD, { 0 }, 1, 0, NULL },
   { "wildfile", CFG_TYPE_WILD, { 0 }, 2, 0, NULL },
   { "exclude", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "aclsupport", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "plugin", CFG_TYPE_PLUGIN, { 0 }, 0, 0, NULL },
   { "ignorecase", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "fstype", CFG_TYPE_FSTYPE, { 0 }, 0, 0, NULL },
   { "hfsplussupport", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "noatime", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "enhancedwild", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "drivetype", CFG_TYPE_DRIVETYPE, { 0 }, 0, 0, NULL },
   { "checkfilechanges", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "strippath", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "honornodumpflag", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "xattrsupport", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "size", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "shadowing", CFG_TYPE_OPTION, { 0 }, 0, 0, NULL },
   { "meta", CFG_TYPE_META, { 0 }, 0, 0, 0 },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

#include "inc_conf.h"

/*
 * determine used compression algorithms
 */
void find_used_compressalgos(POOL_MEM *compressalgos, JCR *jcr)
{
   int cnt = 0;
   INCEXE *inc;
   FOPTS *fopts;
   FILESETRES *fs;
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

/*
 * Scan for right hand side of Include options (keyword=option) is
 *    converted into one or two characters. Verifyopts=xxxx is Vxxxx:
 *    Whatever is found is concatenated to the opts string.
 * This code is also used inside an Options resource.
 */
static void scan_include_options(LEX *lc, int keyword, char *opts, int optlen)
{
   int i;
   char option[64];
   int lcopts = lc->options;
   struct s_sz_matching size_matching;

   memset(option, 0, sizeof(option));
   lc->options |= LOPT_STRING;        /* force string */
   lex_get_token(lc, T_STRING);       /* expect at least one option */
   if (keyword == INC_KW_VERIFY) { /* special case */
      /* ***FIXME**** ensure these are in permitted set */
      bstrncat(opts, "V", optlen);         /* indicate Verify */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_ACCURATE) { /* special case */
      /* ***FIXME**** ensure these are in permitted set */
      bstrncat(opts, "C", optlen);         /* indicate Accurate */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_BASEJOB) { /* special case */
      /* ***FIXME**** ensure these are in permitted set */
      bstrncat(opts, "J", optlen);         /* indicate BaseJob */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_STRIPPATH) { /* special case */
      if (!is_an_integer(lc->str)) {
         scan_err1(lc, _("Expected a strip path positive integer, got:%s:"), lc->str);
      }
      bstrncat(opts, "P", optlen);         /* indicate strip path */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else if (keyword == INC_KW_SIZE) { /* special case */
      if (!parse_size_match(lc->str, &size_matching)) {
         scan_err1(lc, _("Expected a parseable size, got:%s:"), lc->str);
      }
      bstrncat(opts, "z", optlen);         /* indicate size */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   } else {
      /*
       * Standard keyword options for Include/Exclude
       */
      for (i = 0; FS_options[i].name; i++) {
         if (FS_options[i].keyword == keyword && bstrcasecmp(lc->str, FS_options[i].name)) {
            bstrncpy(option, FS_options[i].option, sizeof(option));
            i = 0;
            break;
         }
      }
      if (i != 0) {
         scan_err1(lc, _("Expected a FileSet option keyword, got:%s:"), lc->str);
      } else { /* add option */
         bstrncat(opts, option, optlen);
         Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
      }
   }
   lc->options = lcopts;

   /* If option terminated by comma, eat it */
   if (lc->ch == ',') {
      lex_get_token(lc, T_ALL);      /* yes, eat comma */
   }
}

/* Store regex info */
static void store_regex(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token, rc;
   regex_t preg;
   char prbuf[500];
   const char *type;
   int newsize;

   token = lex_get_token(lc, T_SKIP_EOL);
   if (pass == 1) {
      /* Pickup regex string
       */
      switch (token) {
      case T_IDENTIFIER:
      case T_UNQUOTED_STRING:
      case T_QUOTED_STRING:
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
   scan_to_eol(lc);
}

/* Store Base info */
static void store_base(LEX *lc, RES_ITEM *item, int index, int pass)
{

   lex_get_token(lc, T_NAME);
   if (pass == 1) {
      /*
       * Pickup Base Job Name
       */
      res_incexe.current_opts->base.append(bstrdup(lc->str));
   }
   scan_to_eol(lc);
}

/* Store reader info */
static void store_plugin(LEX *lc, RES_ITEM *item, int index, int pass)
{

   lex_get_token(lc, T_NAME);
   if (pass == 1) {
      /*
       * Pickup plugin command
       */
      res_incexe.current_opts->plugin = bstrdup(lc->str);
   }
   scan_to_eol(lc);
}

/* Store Wild-card info */
static void store_wild(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   const char *type;
   int newsize;

   token = lex_get_token(lc, T_SKIP_EOL);
   if (pass == 1) {
      /*
       * Pickup Wild-card string
       */
      switch (token) {
      case T_IDENTIFIER:
      case T_UNQUOTED_STRING:
      case T_QUOTED_STRING:
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
   scan_to_eol(lc);
}

/* Store fstype info */
static void store_fstype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;

   token = lex_get_token(lc, T_SKIP_EOL);
   if (pass == 1) {
      /* Pickup fstype string */
      switch (token) {
      case T_IDENTIFIER:
      case T_UNQUOTED_STRING:
      case T_QUOTED_STRING:
         res_incexe.current_opts->fstype.append(bstrdup(lc->str));
         Dmsg3(900, "set fstype %p size=%d %s\n",
            res_incexe.current_opts, res_incexe.current_opts->fstype.size(), lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a fstype string, got: %s\n"), lc->str);
      }
   }
   scan_to_eol(lc);
}

/* Store drivetype info */
static void store_drivetype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;

   token = lex_get_token(lc, T_SKIP_EOL);
   if (pass == 1) {
      /* Pickup drivetype string */
      switch (token) {
      case T_IDENTIFIER:
      case T_UNQUOTED_STRING:
      case T_QUOTED_STRING:
         res_incexe.current_opts->drivetype.append(bstrdup(lc->str));
         Dmsg3(900, "set drivetype %p size=%d %s\n",
            res_incexe.current_opts, res_incexe.current_opts->drivetype.size(), lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a drivetype string, got: %s\n"), lc->str);
      }
   }
   scan_to_eol(lc);
}

static void store_meta(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;

   token = lex_get_token(lc, T_SKIP_EOL);
   if (pass == 1) {
      /* Pickup fstype string */
      switch (token) {
      case T_IDENTIFIER:
      case T_UNQUOTED_STRING:
      case T_QUOTED_STRING:
         res_incexe.current_opts->meta.append(bstrdup(lc->str));
         Dmsg3(900, "set meta %p size=%d %s\n",
            res_incexe.current_opts, res_incexe.current_opts->meta.size(), lc->str);
         break;
      default:
         scan_err1(lc, _("Expected a meta string, got: %s\n"), lc->str);
      }
   }
   scan_to_eol(lc);
}

/*
 * New style options come here
 */
static void store_option(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   int keyword;
   char inc_opts[100];

   inc_opts[0] = 0;
   keyword = INC_KW_NONE;
   /* Look up the keyword */
   for (i=0; FS_option_kw[i].name; i++) {
      if (bstrcasecmp(item->name, FS_option_kw[i].name)) {
         keyword = FS_option_kw[i].token;
         break;
      }
   }
   if (keyword == INC_KW_NONE) {
      scan_err1(lc, _("Expected a FileSet keyword, got: %s"), lc->str);
   }
   /* Now scan for the value */
   scan_include_options(lc, keyword, inc_opts, sizeof(inc_opts));
   if (pass == 1) {
      bstrncat(res_incexe.current_opts->opts, inc_opts, MAX_FOPTS);
      Dmsg2(900, "new pass=%d incexe opts=%s\n", pass, res_incexe.current_opts->opts);
   }
   scan_to_eol(lc);
}

/* If current_opts not defined, create first entry */
static void setup_current_opts(void)
{
   FOPTS *fo = (FOPTS *)malloc(sizeof(FOPTS));
   memset(fo, 0, sizeof(FOPTS));
   fo->regex.init(1, true);
   fo->regexdir.init(1, true);
   fo->regexfile.init(1, true);
   fo->wild.init(1, true);
   fo->wilddir.init(1, true);
   fo->wildfile.init(1, true);
   fo->wildbase.init(1, true);
   fo->base.init(1, true);
   fo->fstype.init(1, true);
   fo->drivetype.init(1, true);
   fo->meta.init(1, true);
   res_incexe.current_opts = fo;
   if (res_incexe.num_opts == 0) {
      res_incexe.opts_list = (FOPTS **)malloc(sizeof(FOPTS *));
   } else {
      res_incexe.opts_list = (FOPTS **)realloc(res_incexe.opts_list,
                     sizeof(FOPTS *) * (res_incexe.num_opts + 1));
   }
   res_incexe.opts_list[res_incexe.num_opts++] = fo;
}

/*
 * Come here when Options seen in Include/Exclude
 */
static void store_options_res(LEX *lc, RES_ITEM *item, int index, int pass, bool exclude)
{
   int token, i;

   if (exclude) {
      scan_err0(lc, _("Options section not permitted in Exclude\n"));
      /* NOT REACHED */
   }
   token = lex_get_token(lc, T_SKIP_EOL);
   if (token != T_BOB) {
      scan_err1(lc, _("Expecting open brace. Got %s"), lc->str);
   }

   if (pass == 1) {
      setup_current_opts();
   }

   while ((token = lex_get_token(lc, T_ALL)) != T_EOF) {
      if (token == T_EOL) {
         continue;
      }
      if (token == T_EOB) {
         break;
      }
      if (token != T_IDENTIFIER) {
         scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
      }
      for (i=0; options_items[i].name; i++) {
         if (bstrcasecmp(options_items[i].name, lc->str)) {
            token = lex_get_token(lc, T_SKIP_EOL);
            if (token != T_EQUALS) {
               scan_err1(lc, _("expected an equals, got: %s"), lc->str);
            }
            /* Call item handler */
            switch (options_items[i].type) {
            case CFG_TYPE_OPTION:
               store_option(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_REGEX:
               store_regex(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_BASE:
               store_base(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_WILD:
               store_wild(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_PLUGIN:
               store_plugin(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_FSTYPE:
               store_fstype(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_DRIVETYPE:
               store_drivetype(lc, &options_items[i], i, pass);
               break;
            case CFG_TYPE_META:
               store_meta(lc, &options_items[i], i, pass);
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

/*
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void store_fname(LEX *lc, RES_ITEM *item, int index, int pass, bool exclude)
{
   int token;
   INCEXE *incexe;
   URES *res_all = (URES *)my_config->m_res_all;

   token = lex_get_token(lc, T_SKIP_EOL);
   if (pass == 1) {
      /* Pickup Filename string
       */
      switch (token) {
      case T_IDENTIFIER:
      case T_UNQUOTED_STRING:
         if (strchr(lc->str, '\\')) {
            scan_err1(lc, _("Backslash found. Use forward slashes or quote the string.: %s\n"), lc->str);
            /* NOT REACHED */
         }
      case T_QUOTED_STRING:
         if (res_all->res_fs.have_MD5) {
            MD5Update(&res_all->res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
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
   scan_to_eol(lc);
}

/*
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void store_plugin_name(LEX *lc, RES_ITEM *item, int index, int pass, bool exclude)
{
   int token;
   INCEXE *incexe;
   URES *res_all = (URES *)my_config->m_res_all;

   if (exclude) {
      scan_err0(lc, _("Plugin directive not permitted in Exclude\n"));
      /* NOT REACHED */
   }
   token = lex_get_token(lc, T_SKIP_EOL);
   if (pass == 1) {
      /* Pickup Filename string
       */
      switch (token) {
      case T_IDENTIFIER:
      case T_UNQUOTED_STRING:
         if (strchr(lc->str, '\\')) {
            scan_err1(lc, _("Backslash found. Use forward slashes or quote the string.: %s\n"), lc->str);
            /* NOT REACHED */
         }
      case T_QUOTED_STRING:
         if (res_all->res_fs.have_MD5) {
            MD5Update(&res_all->res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
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
   scan_to_eol(lc);
}

/* Store exclude directory containing  info */
static void store_excludedir(LEX *lc, RES_ITEM *item, int index, int pass, bool exclude)
{
   if (exclude) {
      scan_err0(lc, _("ExcludeDirContaining directive not permitted in Exclude.\n"));
      /* NOT REACHED */
      return;
   }

   lex_get_token(lc, T_NAME);
   if (pass == 1) {
      res_incexe.ignoredir.append(bstrdup(lc->str));
   }
   scan_to_eol(lc);
}

/*
 * Store new style FileSet Include/Exclude info
 *
 *  Note, when this routine is called, we are inside a FileSet
 *  resource.  We treat the Include/Exclude like a sort of
 *  mini-resource within the FileSet resource.
 */
static void store_newinc(LEX *lc, RES_ITEM *item, int index, int pass)
{
   bool options;
   int token, i;
   INCEXE *incexe;
   URES *res_all = (URES *)my_config->m_res_all;

   if (!res_all->res_fs.have_MD5) {
      MD5Init(&res_all->res_fs.md5c);
      res_all->res_fs.have_MD5 = true;
   }
   memset(&res_incexe, 0, sizeof(INCEXE));
   res_all->res_fs.new_include = true;
   while ((token = lex_get_token(lc, T_SKIP_EOL)) != T_EOF) {
      if (token == T_EOB) {
         break;
      }
      if (token != T_IDENTIFIER) {
         scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
      }
      for (i=0; newinc_items[i].name; i++) {
         options = bstrcasecmp(lc->str, "options");
         if (bstrcasecmp(newinc_items[i].name, lc->str)) {
            if (!options) {
               token = lex_get_token(lc, T_SKIP_EOL);
               if (token != T_EQUALS) {
                  scan_err1(lc, _("expected an equals, got: %s"), lc->str);
               }
            }

            /* Call item handler */
            switch (newinc_items[i].type) {
            case CFG_TYPE_FNAME:
               store_fname(lc, &newinc_items[i], i, pass, item->code);
               break;
            case CFG_TYPE_PLUGINNAME:
               store_plugin_name(lc, &newinc_items[i], i, pass, item->code);
               break;
            case CFG_TYPE_EXCLUDEDIR:
               store_excludedir(lc, &newinc_items[i], i, pass, item->code);
               break;
            case CFG_TYPE_OPTIONS:
               store_options_res(lc, &newinc_items[i], i, pass, item->code);
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
      incexe = (INCEXE *)malloc(sizeof(INCEXE));
      memcpy(incexe, &res_incexe, sizeof(INCEXE));
      memset(&res_incexe, 0, sizeof(INCEXE));
      if (item->code == 0) { /* include */
         if (res_all->res_fs.num_includes == 0) {
            res_all->res_fs.include_items = (INCEXE **)malloc(sizeof(INCEXE *));
         } else {
            res_all->res_fs.include_items = (INCEXE **)realloc(res_all->res_fs.include_items,
                           sizeof(INCEXE *) * (res_all->res_fs.num_includes + 1));
         }
         res_all->res_fs.include_items[res_all->res_fs.num_includes++] = incexe;
         Dmsg1(900, "num_includes=%d\n", res_all->res_fs.num_includes);
      } else {    /* exclude */
         if (res_all->res_fs.num_excludes == 0) {
            res_all->res_fs.exclude_items = (INCEXE **)malloc(sizeof(INCEXE *));
         } else {
            res_all->res_fs.exclude_items = (INCEXE **)realloc(res_all->res_fs.exclude_items,
                           sizeof(INCEXE *) * (res_all->res_fs.num_excludes + 1));
         }
         res_all->res_fs.exclude_items[res_all->res_fs.num_excludes++] = incexe;
         Dmsg1(900, "num_excludes=%d\n", res_all->res_fs.num_excludes);
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
}

/*
 * Store FileSet Include/Exclude info
 *  new style includes are handled in store_newinc()
 */
void store_inc(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;

   /*
    * Decide if we are doing a new Include or an old include. The
    *  new Include is followed immediately by open brace, whereas the
    *  old include has options following the Include.
    */
   token = lex_get_token(lc, T_SKIP_EOL);
   if (token == T_BOB) {
      store_newinc(lc, item, index, pass);
      return;
   }
   scan_err0(lc, _("Old style Include/Exclude not supported\n"));
}
