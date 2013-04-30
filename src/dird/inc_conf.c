/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

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
 *   Configuration file parser for new and old Include and
 *      Exclude records
 *
 *     Kern Sibbald, March MMIII
 *
 *     Version $Id$
 */

#include "bacula.h"
#include "dird.h"
#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

/* Forward referenced subroutines */

void store_inc(LEX *lc, RES_ITEM *item, int index, int pass);

static void store_newinc(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_regex(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_wild(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_fstype(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_drivetype(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_opts(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_base(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_plugin(LEX *lc, RES_ITEM *item, int index, int pass);
static void setup_current_opts(void);

/* Include and Exclude items */
static void store_fname(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude);
static void store_plugin_name(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude);
static void options_res(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude);
static void store_excludedir(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude);


/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
   extern URES res_all;
}
#else
extern URES res_all;
#endif
extern int32_t  res_all_size;

/* We build the current new Include and Exclude items here */
static INCEXE res_incexe;

/*
 * new Include/Exclude items
 *   name             handler     value    code flags default_value
 */
static RES_ITEM2 newinc_items[] = {
   {"file",            store_fname,       {0},      0, 0, 0},
   {"plugin",          store_plugin_name, {0},      0, 0, 0},
   {"excludedircontaining", store_excludedir,  {0}, 0, 0, 0},
   {"options",         options_res,       {0},      0, 0, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};

/*
 * Items that are valid in an Options resource
 */
static RES_ITEM options_items[] = {
   {"compression",     store_opts,    {0},     0, 0, 0},
   {"signature",       store_opts,    {0},     0, 0, 0},
   {"basejob",         store_opts,    {0},     0, 0, 0},
   {"accurate",        store_opts,    {0},     0, 0, 0},
   {"verify",          store_opts,    {0},     0, 0, 0},
   {"onefs",           store_opts,    {0},     0, 0, 0},
   {"recurse",         store_opts,    {0},     0, 0, 0},
   {"sparse",          store_opts,    {0},     0, 0, 0},
   {"hardlinks",       store_opts,    {0},     0, 0, 0},
   {"readfifo",        store_opts,    {0},     0, 0, 0},
   {"replace",         store_opts,    {0},     0, 0, 0},
   {"portable",        store_opts,    {0},     0, 0, 0},
   {"mtimeonly",       store_opts,    {0},     0, 0, 0},
   {"keepatime",       store_opts,    {0},     0, 0, 0},
   {"regex",           store_regex,   {0},     0, 0, 0},
   {"regexdir",        store_regex,   {0},     1, 0, 0},
   {"regexfile",       store_regex,   {0},     2, 0, 0},
   {"base",            store_base,    {0},     0, 0, 0},
   {"wild",            store_wild,    {0},     0, 0, 0},
   {"wilddir",         store_wild,    {0},     1, 0, 0},
   {"wildfile",        store_wild,    {0},     2, 0, 0},
   {"exclude",         store_opts,    {0},     0, 0, 0},
   {"aclsupport",      store_opts,    {0},     0, 0, 0},
   {"plugin",          store_plugin,  {0},     0, 0, 0},
   {"ignorecase",      store_opts,    {0},     0, 0, 0},
   {"fstype",          store_fstype,  {0},     0, 0, 0},
   {"hfsplussupport",  store_opts,    {0},     0, 0, 0},
   {"noatime",         store_opts,    {0},     0, 0, 0},
   {"enhancedwild",    store_opts,    {0},     0, 0, 0},
   {"drivetype",       store_drivetype, {0},   0, 0, 0},
   {"checkfilechanges",store_opts,    {0},     0, 0, 1},
   {"strippath",       store_opts,    {0},     0, 0, 0},
   {"honornodumpflag", store_opts,    {0},     0, 0, 0},
   {"xattrsupport",    store_opts,    {0},     0, 0, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};


/* Define FileSet KeyWord values */
enum {
   INC_KW_NONE,
   INC_KW_COMPRESSION,
   INC_KW_DIGEST,
   INC_KW_ENCRYPTION,
   INC_KW_VERIFY,
   INC_KW_BASEJOB,
   INC_KW_ACCURATE,
   INC_KW_ONEFS,
   INC_KW_RECURSE,
   INC_KW_SPARSE,
   INC_KW_HARDLINK,
   INC_KW_REPLACE,               /* restore options */
   INC_KW_READFIFO,              /* Causes fifo data to be read */
   INC_KW_PORTABLE,
   INC_KW_MTIMEONLY,
   INC_KW_KEEPATIME,
   INC_KW_EXCLUDE,
   INC_KW_ACL,
   INC_KW_IGNORECASE,
   INC_KW_HFSPLUS,
   INC_KW_NOATIME,
   INC_KW_ENHANCEDWILD,
   INC_KW_CHKCHANGES,
   INC_KW_STRIPPATH,
   INC_KW_HONOR_NODUMP,
   INC_KW_XATTR
};

/*
 * This is the list of options that can be stored by store_opts
 *   Note, now that the old style Include/Exclude code is gone,
 *   the INC_KW code could be put into the "code" field of the
 *   options given above.
 */
static struct s_kw FS_option_kw[] = {
   {"compression", INC_KW_COMPRESSION},
   {"signature",   INC_KW_DIGEST},
   {"encryption",  INC_KW_ENCRYPTION},
   {"verify",      INC_KW_VERIFY},
   {"basejob",     INC_KW_BASEJOB},
   {"accurate",    INC_KW_ACCURATE},
   {"onefs",       INC_KW_ONEFS},
   {"recurse",     INC_KW_RECURSE},
   {"sparse",      INC_KW_SPARSE},
   {"hardlinks",   INC_KW_HARDLINK},
   {"replace",     INC_KW_REPLACE},
   {"readfifo",    INC_KW_READFIFO},
   {"portable",    INC_KW_PORTABLE},
   {"mtimeonly",   INC_KW_MTIMEONLY},
   {"keepatime",   INC_KW_KEEPATIME},
   {"exclude",     INC_KW_EXCLUDE},
   {"aclsupport",  INC_KW_ACL},
   {"ignorecase",  INC_KW_IGNORECASE},
   {"hfsplussupport", INC_KW_HFSPLUS},
   {"noatime",     INC_KW_NOATIME},
   {"enhancedwild", INC_KW_ENHANCEDWILD},
   {"checkfilechanges", INC_KW_CHKCHANGES},
   {"strippath",   INC_KW_STRIPPATH},
   {"honornodumpflag", INC_KW_HONOR_NODUMP},
   {"xattrsupport", INC_KW_XATTR},
   {NULL,          0}
};

/* Options for FileSet keywords */

struct s_fs_opt {
   const char *name;
   int keyword;
   const char *option;
};

/*
 * Options permitted for each keyword and resulting value.
 * The output goes into opts, which are then transmitted to
 * the FD for application as options to the following list of
 * included files.
 */
static struct s_fs_opt FS_options[] = {
   {"md5",      INC_KW_DIGEST,        "M"},
   {"sha1",     INC_KW_DIGEST,        "S"},
   {"sha256",   INC_KW_DIGEST,       "S2"},
   {"sha512",   INC_KW_DIGEST,       "S3"},
   {"gzip",     INC_KW_COMPRESSION,  "Z6"},
   {"gzip1",    INC_KW_COMPRESSION,  "Z1"},
   {"gzip2",    INC_KW_COMPRESSION,  "Z2"},
   {"gzip3",    INC_KW_COMPRESSION,  "Z3"},
   {"gzip4",    INC_KW_COMPRESSION,  "Z4"},
   {"gzip5",    INC_KW_COMPRESSION,  "Z5"},
   {"gzip6",    INC_KW_COMPRESSION,  "Z6"},
   {"gzip7",    INC_KW_COMPRESSION,  "Z7"},
   {"gzip8",    INC_KW_COMPRESSION,  "Z8"},
   {"gzip9",    INC_KW_COMPRESSION,  "Z9"},
   {"lzo",      INC_KW_COMPRESSION,  "Zo"},
   {"blowfish", INC_KW_ENCRYPTION,    "B"},   /* ***FIXME*** not implemented */
   {"3des",     INC_KW_ENCRYPTION,    "3"},   /* ***FIXME*** not implemented */
   {"yes",      INC_KW_ONEFS,         "0"},
   {"no",       INC_KW_ONEFS,         "f"},
   {"yes",      INC_KW_RECURSE,       "0"},
   {"no",       INC_KW_RECURSE,       "h"},
   {"yes",      INC_KW_SPARSE,        "s"},
   {"no",       INC_KW_SPARSE,        "0"},
   {"yes",      INC_KW_HARDLINK,      "0"},
   {"no",       INC_KW_HARDLINK,      "H"},
   {"always",   INC_KW_REPLACE,       "a"},
   {"ifnewer",  INC_KW_REPLACE,       "w"},
   {"never",    INC_KW_REPLACE,       "n"},
   {"yes",      INC_KW_READFIFO,      "r"},
   {"no",       INC_KW_READFIFO,      "0"},
   {"yes",      INC_KW_PORTABLE,      "p"},
   {"no",       INC_KW_PORTABLE,      "0"},
   {"yes",      INC_KW_MTIMEONLY,     "m"},
   {"no",       INC_KW_MTIMEONLY,     "0"},
   {"yes",      INC_KW_KEEPATIME,     "k"},
   {"no",       INC_KW_KEEPATIME,     "0"},
   {"yes",      INC_KW_EXCLUDE,       "e"},
   {"no",       INC_KW_EXCLUDE,       "0"},
   {"yes",      INC_KW_ACL,           "A"},
   {"no",       INC_KW_ACL,           "0"},
   {"yes",      INC_KW_IGNORECASE,    "i"},
   {"no",       INC_KW_IGNORECASE,    "0"},
   {"yes",      INC_KW_HFSPLUS,       "R"},   /* "R" for resource fork */
   {"no",       INC_KW_HFSPLUS,       "0"},
   {"yes",      INC_KW_NOATIME,       "K"},
   {"no",       INC_KW_NOATIME,       "0"},
   {"yes",      INC_KW_ENHANCEDWILD,  "K"},
   {"no",       INC_KW_ENHANCEDWILD,  "0"},
   {"yes",      INC_KW_CHKCHANGES,    "c"},
   {"no",       INC_KW_CHKCHANGES,    "0"},
   {"yes",      INC_KW_HONOR_NODUMP,  "N"},
   {"no",       INC_KW_HONOR_NODUMP,  "0"},
   {"yes",      INC_KW_XATTR,         "X"},
   {"no",       INC_KW_XATTR,         "0"},
   {NULL,       0,                      0}
};



/*
 * Scan for right hand side of Include options (keyword=option) is
 *    converted into one or two characters. Verifyopts=xxxx is Vxxxx:
 *    Whatever is found is concatenated to the opts string.
 * This code is also used inside an Options resource.
 */
static void scan_include_options(LEX *lc, int keyword, char *opts, int optlen)
{
   int i;
   char option[3];
   int lcopts = lc->options;

   option[0] = 0;                     /* default option = none */
   option[2] = 0;                     /* terminate options */
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
   } else if (keyword == INC_KW_STRIPPATH) { /* another special case */
      if (!is_an_integer(lc->str)) {
         scan_err1(lc, _("Expected a strip path positive integer, got:%s:"), lc->str);
      }
      bstrncat(opts, "P", optlen);         /* indicate strip path */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
   /*
    * Standard keyword options for Include/Exclude
    */
   } else {
      for (i=0; FS_options[i].name; i++) {
         if (FS_options[i].keyword == keyword && strcasecmp(lc->str, FS_options[i].name) == 0) {
            /* NOTE! maximum 2 letters here or increase option[3] */
            option[0] = FS_options[i].option[0];
            option[1] = FS_options[i].option[1];
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

/*
 *
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


/*
 * Store new style FileSet Include/Exclude info
 *
 *  Note, when this routine is called, we are inside a FileSet
 *  resource.  We treat the Include/Execlude like a sort of
 *  mini-resource within the FileSet resource.
 */
static void store_newinc(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token, i;
   INCEXE *incexe;
   bool options;

   if (!res_all.res_fs.have_MD5) {
      MD5Init(&res_all.res_fs.md5c);
      res_all.res_fs.have_MD5 = true;
   }
   memset(&res_incexe, 0, sizeof(INCEXE));
   res_all.res_fs.new_include = true;
   while ((token = lex_get_token(lc, T_SKIP_EOL)) != T_EOF) {
      if (token == T_EOB) {
         break;
      }
      if (token != T_IDENTIFIER) {
         scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
      }
      for (i=0; newinc_items[i].name; i++) {
         options = strcasecmp(lc->str, "options") == 0;
         if (strcasecmp(newinc_items[i].name, lc->str) == 0) {
            if (!options) {
               token = lex_get_token(lc, T_SKIP_EOL);
               if (token != T_EQUALS) {
                  scan_err1(lc, _("expected an equals, got: %s"), lc->str);
               }
            }
            /* Call item handler */
            newinc_items[i].handler(lc, &newinc_items[i], i, pass, item->code);
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
         if (res_all.res_fs.num_includes == 0) {
            res_all.res_fs.include_items = (INCEXE **)malloc(sizeof(INCEXE *));
         } else {
            res_all.res_fs.include_items = (INCEXE **)realloc(res_all.res_fs.include_items,
                           sizeof(INCEXE *) * (res_all.res_fs.num_includes + 1));
         }
         res_all.res_fs.include_items[res_all.res_fs.num_includes++] = incexe;
         Dmsg1(900, "num_includes=%d\n", res_all.res_fs.num_includes);
      } else {    /* exclude */
         if (res_all.res_fs.num_excludes == 0) {
            res_all.res_fs.exclude_items = (INCEXE **)malloc(sizeof(INCEXE *));
         } else {
            res_all.res_fs.exclude_items = (INCEXE **)realloc(res_all.res_fs.exclude_items,
                           sizeof(INCEXE *) * (res_all.res_fs.num_excludes + 1));
         }
         res_all.res_fs.exclude_items[res_all.res_fs.num_excludes++] = incexe;
         Dmsg1(900, "num_excludes=%d\n", res_all.res_fs.num_excludes);
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
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
         scan_err1(lc, _("Expected an fstype string, got: %s\n"), lc->str);
      }
   }
   scan_to_eol(lc);
}

/* Store exclude directory containing  info */
static void store_excludedir(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude)
{

   if (exclude) {
      scan_err0(lc, _("ExcludeDirContaining directive not permitted in Exclude.\n"));
      /* NOT REACHED */
   }
   lex_get_token(lc, T_NAME);
   if (pass == 1) {
      res_incexe.ignoredir = bstrdup(lc->str);
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
         scan_err1(lc, _("Expected an drivetype string, got: %s\n"), lc->str);
      }
   }
   scan_to_eol(lc);
}

/*
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void store_fname(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude)
{
   int token;
   INCEXE *incexe;

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
         if (res_all.res_fs.have_MD5) {
            MD5Update(&res_all.res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
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
static void store_plugin_name(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude)
{
   int token;
   INCEXE *incexe;

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
         if (res_all.res_fs.have_MD5) {
            MD5Update(&res_all.res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
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



/*
 * Come here when Options seen in Include/Exclude
 */
static void options_res(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude)
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
         if (strcasecmp(options_items[i].name, lc->str) == 0) {
            token = lex_get_token(lc, T_SKIP_EOL);
            if (token != T_EQUALS) {
               scan_err1(lc, _("expected an equals, got: %s"), lc->str);
            }
            /* Call item handler */
            options_items[i].handler(lc, &options_items[i], i, pass);
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
 * New style options come here
 */
static void store_opts(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   int keyword;
   char inc_opts[100];

   inc_opts[0] = 0;
   keyword = INC_KW_NONE;
   /* Look up the keyword */
   for (i=0; FS_option_kw[i].name; i++) {
      if (strcasecmp(item->name, FS_option_kw[i].name) == 0) {
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
   res_incexe.current_opts = fo;
   if (res_incexe.num_opts == 0) {
      res_incexe.opts_list = (FOPTS **)malloc(sizeof(FOPTS *));
   } else {
      res_incexe.opts_list = (FOPTS **)realloc(res_incexe.opts_list,
                     sizeof(FOPTS *) * (res_incexe.num_opts + 1));
   }
   res_incexe.opts_list[res_incexe.num_opts++] = fo;
}
