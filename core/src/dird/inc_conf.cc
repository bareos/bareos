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
#include "dird/dird_globals.h"
#include "dird/jcr_private.h"
#include "findlib/match.h"
#include "lib/parse_conf.h"

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif
#include "findlib/find.h"

#include "inc_conf.h"
#include "lib/edit.h"

#include <cassert>

namespace directordaemon {

#define PERMITTED_VERIFY_OPTIONS (const char*)"ipnugsamcd51"
#define PERMITTED_ACCURATE_OPTIONS (const char*)"ipnugsamcd51A"
#define PERMITTED_BASEJOB_OPTIONS (const char*)"ipnugsamcd51"

typedef struct {
  bool configured;
  std::string default_value;
} options_default_value_s;


/*
 * Define FileSet KeyWord values
 */
enum
{
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
  INC_KW_REPLACE,  /* restore options */
  INC_KW_READFIFO, /* Causes fifo data to be read */
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
  INC_KW_XATTR,
  INC_KW_SIZE,
  INC_KW_SHADOWING,
  INC_KW_AUTO_EXCLUDE,
  INC_KW_FORCE_ENCRYPTION
};

/*
 * This is the list of options that can be stored by store_opts
 * Note, now that the old style Include/Exclude code is gone,
 * the INC_KW code could be put into the "code" field of the
 * options given above.
 */
static struct s_kw FS_option_kw[] = {
    {"compression", INC_KW_COMPRESSION},
    {"signature", INC_KW_DIGEST},
    {"encryption", INC_KW_ENCRYPTION},
    {"verify", INC_KW_VERIFY},
    {"basejob", INC_KW_BASEJOB},
    {"accurate", INC_KW_ACCURATE},
    {"onefs", INC_KW_ONEFS},
    {"recurse", INC_KW_RECURSE},
    {"sparse", INC_KW_SPARSE},
    {"hardlinks", INC_KW_HARDLINK},
    {"replace", INC_KW_REPLACE},
    {"readfifo", INC_KW_READFIFO},
    {"portable", INC_KW_PORTABLE},
    {"mtimeonly", INC_KW_MTIMEONLY},
    {"keepatime", INC_KW_KEEPATIME},
    {"exclude", INC_KW_EXCLUDE},
    {"aclsupport", INC_KW_ACL},
    {"ignorecase", INC_KW_IGNORECASE},
    {"hfsplussupport", INC_KW_HFSPLUS},
    {"noatime", INC_KW_NOATIME},
    {"enhancedwild", INC_KW_ENHANCEDWILD},
    {"checkfilechanges", INC_KW_CHKCHANGES},
    {"strippath", INC_KW_STRIPPATH},
    {"honornodumpflag", INC_KW_HONOR_NODUMP},
    {"xattrsupport", INC_KW_XATTR},
    {"size", INC_KW_SIZE},
    {"shadowing", INC_KW_SHADOWING},
    {"autoexclude", INC_KW_AUTO_EXCLUDE},
    {"forceencryption", INC_KW_FORCE_ENCRYPTION},
    {NULL, 0}};

/*
 * Options for FileSet keywords
 */
struct s_fs_opt {
  const char* name;
  int keyword;
  const char* option;
};

/*
 * Options permitted for each keyword and resulting value.
 * The output goes into opts, which are then transmitted to
 * the FD for application as options to the following list of
 * included files.
 */
static struct s_fs_opt FS_options[] = {
    {"md5", INC_KW_DIGEST, "M"},
    {"sha1", INC_KW_DIGEST, "S"},
    {"sha256", INC_KW_DIGEST, "S2"},
    {"sha512", INC_KW_DIGEST, "S3"},
    {"gzip", INC_KW_COMPRESSION, "Z6"},
    {"gzip1", INC_KW_COMPRESSION, "Z1"},
    {"gzip2", INC_KW_COMPRESSION, "Z2"},
    {"gzip3", INC_KW_COMPRESSION, "Z3"},
    {"gzip4", INC_KW_COMPRESSION, "Z4"},
    {"gzip5", INC_KW_COMPRESSION, "Z5"},
    {"gzip6", INC_KW_COMPRESSION, "Z6"},
    {"gzip7", INC_KW_COMPRESSION, "Z7"},
    {"gzip8", INC_KW_COMPRESSION, "Z8"},
    {"gzip9", INC_KW_COMPRESSION, "Z9"},
    {"lzo", INC_KW_COMPRESSION, "Zo"},
    {"lzfast", INC_KW_COMPRESSION, "Zff"},
    {"lz4", INC_KW_COMPRESSION, "Zf4"},
    {"lz4hc", INC_KW_COMPRESSION, "Zfh"},
    {"blowfish", INC_KW_ENCRYPTION, "Eb"},
    {"3des", INC_KW_ENCRYPTION, "E3"},
    {"aes128", INC_KW_ENCRYPTION, "Ea1"},
    {"aes192", INC_KW_ENCRYPTION, "Ea2"},
    {"aes256", INC_KW_ENCRYPTION, "Ea3"},
    {"camellia128", INC_KW_ENCRYPTION, "Ec1"},
    {"camellia192", INC_KW_ENCRYPTION, "Ec2"},
    {"camellia256", INC_KW_ENCRYPTION, "Ec3"},
    {"aes128hmacsha1", INC_KW_ENCRYPTION, "Eh1"},
    {"aes256hmacsha1", INC_KW_ENCRYPTION, "Eh2"},
    {"yes", INC_KW_ONEFS, "0"},
    {"no", INC_KW_ONEFS, "f"},
    {"yes", INC_KW_RECURSE, "0"},
    {"no", INC_KW_RECURSE, "h"},
    {"yes", INC_KW_SPARSE, "s"},
    {"no", INC_KW_SPARSE, "0"},
    {"yes", INC_KW_HARDLINK, "0"},
    {"no", INC_KW_HARDLINK, "H"},
    {"always", INC_KW_REPLACE, "a"},
    {"ifnewer", INC_KW_REPLACE, "w"},
    {"never", INC_KW_REPLACE, "n"},
    {"yes", INC_KW_READFIFO, "r"},
    {"no", INC_KW_READFIFO, "0"},
    {"yes", INC_KW_PORTABLE, "p"},
    {"no", INC_KW_PORTABLE, "0"},
    {"yes", INC_KW_MTIMEONLY, "m"},
    {"no", INC_KW_MTIMEONLY, "0"},
    {"yes", INC_KW_KEEPATIME, "k"},
    {"no", INC_KW_KEEPATIME, "0"},
    {"yes", INC_KW_EXCLUDE, "e"},
    {"no", INC_KW_EXCLUDE, "0"},
    {"yes", INC_KW_ACL, "A"},
    {"no", INC_KW_ACL, "0"},
    {"yes", INC_KW_IGNORECASE, "i"},
    {"no", INC_KW_IGNORECASE, "0"},
    {"yes", INC_KW_HFSPLUS, "R"}, /* "R" for resource fork */
    {"no", INC_KW_HFSPLUS, "0"},
    {"yes", INC_KW_NOATIME, "K"},
    {"no", INC_KW_NOATIME, "0"},
    {"yes", INC_KW_ENHANCEDWILD, "K"},
    {"no", INC_KW_ENHANCEDWILD, "0"},
    {"yes", INC_KW_CHKCHANGES, "c"},
    {"no", INC_KW_CHKCHANGES, "0"},
    {"yes", INC_KW_HONOR_NODUMP, "N"},
    {"no", INC_KW_HONOR_NODUMP, "0"},
    {"yes", INC_KW_XATTR, "X"},
    {"no", INC_KW_XATTR, "0"},
    {"localwarn", INC_KW_SHADOWING, "d1"},
    {"localremove", INC_KW_SHADOWING, "d2"},
    {"globalwarn", INC_KW_SHADOWING, "d3"},
    {"globalremove", INC_KW_SHADOWING, "d4"},
    {"none", INC_KW_SHADOWING, "0"},
    {"yes", INC_KW_AUTO_EXCLUDE, "0"},
    {"no", INC_KW_AUTO_EXCLUDE, "x"},
    {"yes", INC_KW_FORCE_ENCRYPTION, "Ef"},
    {"no", INC_KW_FORCE_ENCRYPTION, "0"},
    {NULL, 0, 0}};

/*
 * Imported subroutines
 */
extern void StoreInc(LEX* lc, ResourceItem* item, int index, int pass);

/* We build the current new Include and Exclude items here */
static IncludeExcludeItem* res_incexe;

/* clang-format off */

/*
 * new Include/Exclude items
 * name handler value code flags default_value
 */
ResourceItem newinc_items[] = {
  { "File", CFG_TYPE_FNAME, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Plugin", CFG_TYPE_PLUGINNAME, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "ExcludeDirContaining", CFG_TYPE_EXCLUDEDIR,  0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Options", CFG_TYPE_OPTIONS, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { NULL, 0, 0, nullptr, 0, 0, NULL, NULL, NULL }
};

/*
 * Items that are valid in an Options resource
 * name handler value code flags default_value
 */
ResourceItem options_items[] = {
  { "Compression", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Signature", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "BaseJob", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Accurate", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Verify", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "OneFs", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Recurse", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Sparse", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "HardLinks", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "ReadFifo", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Replace", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Portable", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "MtimeOnly", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "KeepAtime", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Regex", CFG_TYPE_REGEX, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "RegexDir", CFG_TYPE_REGEX, 0, nullptr, 1, 0, NULL, NULL, NULL },
  { "RegexFile", CFG_TYPE_REGEX, 0, nullptr, 2, 0, NULL, NULL, NULL },
  { "Base", CFG_TYPE_BASE, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Wild", CFG_TYPE_WILD, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "WildDir", CFG_TYPE_WILD, 0, nullptr, 1, 0, NULL, NULL, NULL },
  { "WildFile", CFG_TYPE_WILD, 0, nullptr, 2, 0, NULL, NULL, NULL },
  { "Exclude", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "AclSupport", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Plugin", CFG_TYPE_PLUGIN, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "IgnoreCase", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "FsType", CFG_TYPE_FSTYPE, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "HfsPlusSupport", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "NoAtime", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "EnhancedWild", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "DriveType", CFG_TYPE_DRIVETYPE, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "CheckFileChanges", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "StripPath", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "HonornoDumpFlag", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "XAttrSupport", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Size", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Shadowing", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "AutoExclude", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "ForceEncryption", CFG_TYPE_OPTION, 0, nullptr, 0, 0, NULL, NULL, NULL },
  { "Meta", CFG_TYPE_META, 0, nullptr, 0, 0, 0, NULL, NULL },
  { NULL, 0, 0, nullptr, 0, 0, NULL, NULL, NULL }
};

/* clang-format on */

/**
 * determine used compression algorithms
 */
void FindUsedCompressalgos(PoolMem* compressalgos, JobControlRecord* jcr)
{
  int cnt = 0;
  IncludeExcludeItem* inc;
  FileOptions* fopts;
  FilesetResource* fs;
  struct s_fs_opt* fs_opt;

  if (!jcr->impl->res.job || !jcr->impl->res.job->fileset) { return; }

  fs = jcr->impl->res.job->fileset;
  for (std::size_t i = 0; i < fs->include_items.size(); i++) {
    inc = fs->include_items[i];

    for (std::size_t j = 0; j < inc->file_options_list.size(); j++) {
      fopts = inc->file_options_list[j];

      for (char* k = fopts->opts; *k; k++) { /* Try to find one request */
        switch (*k) {
          case 'Z': /* Compression */
            for (fs_opt = FS_options; fs_opt->name; fs_opt++) {
              if (fs_opt->keyword != INC_KW_COMPRESSION) { continue; }

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

  if (cnt > 0) { compressalgos->strcat(")"); }
}

/**
 * Check if the configured options are valid.
 */
static inline void IsInPermittedSet(LEX* lc,
                                    const char* SetType,
                                    const char* permitted_set)
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
      scan_err3(lc, _("Illegal %s option %c, got option string: %s:"), SetType,
                *p, lc->str);
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
static void ScanIncludeOptions(LEX* lc, int keyword, char* opts, int optlen)
{
  int i;
  char option[64];
  int lcopts = lc->options;
  struct s_sz_matching size_matching;

  memset(option, 0, sizeof(option));
  lc->options |= LOPT_STRING;     /* force string */
  LexGetToken(lc, BCT_STRING);    /* expect at least one option */
  if (keyword == INC_KW_VERIFY) { /* special case */
    IsInPermittedSet(lc, _("verify"), PERMITTED_VERIFY_OPTIONS);
    bstrncat(opts, "V", optlen); /* indicate Verify */
    bstrncat(opts, lc->str, optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else if (keyword == INC_KW_ACCURATE) { /* special case */
    IsInPermittedSet(lc, _("accurate"), PERMITTED_ACCURATE_OPTIONS);
    bstrncat(opts, "C", optlen); /* indicate Accurate */
    bstrncat(opts, lc->str, optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else if (keyword == INC_KW_BASEJOB) { /* special case */
    IsInPermittedSet(lc, _("base job"), PERMITTED_BASEJOB_OPTIONS);
    bstrncat(opts, "J", optlen); /* indicate BaseJob */
    bstrncat(opts, lc->str, optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else if (keyword == INC_KW_STRIPPATH) { /* special case */
    if (!IsAnInteger(lc->str)) {
      scan_err1(lc, _("Expected a strip path positive integer, got: %s:"),
                lc->str);
    }
    bstrncat(opts, "P", optlen); /* indicate strip path */
    bstrncat(opts, lc->str, optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else if (keyword == INC_KW_SIZE) { /* special case */
    if (!ParseSizeMatch(lc->str, &size_matching)) {
      scan_err1(lc, _("Expected a parseable size, got: %s:"), lc->str);
    }
    bstrncat(opts, "z", optlen); /* indicate size */
    bstrncat(opts, lc->str, optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else {
    /*
     * Standard keyword options for Include/Exclude
     */
    for (i = 0; FS_options[i].name; i++) {
      if (FS_options[i].keyword == keyword &&
          Bstrcasecmp(lc->str, FS_options[i].name)) {
        bstrncpy(option, FS_options[i].option, sizeof(option));
        i = 0;
        break;
      }
    }
    if (i != 0) {
      scan_err1(lc, _("Expected a FileSet option keyword, got: %s:"), lc->str);
    } else { /* add option */
      bstrncat(opts, option, optlen);
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
    }
  }
  lc->options = lcopts;

  /*
   * If option terminated by comma, eat it
   */
  if (lc->ch == ',') { LexGetToken(lc, BCT_ALL); /* yes, eat comma */ }
}

/**
 * Store regex info
 */
static void StoreRegex(LEX* lc, ResourceItem* item, int index, int pass)
{
  int token, rc;
  regex_t preg;
  char prbuf[500];
  const char* type;
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
          res_incexe->current_opts->regexdir.append(strdup(lc->str));
          newsize = res_incexe->current_opts->regexdir.size();
        } else if (item->code == 2) {
          type = "regexfile";
          res_incexe->current_opts->regexfile.append(strdup(lc->str));
          newsize = res_incexe->current_opts->regexfile.size();
        } else {
          type = "regex";
          res_incexe->current_opts->regex.append(strdup(lc->str));
          newsize = res_incexe->current_opts->regex.size();
        }
        Dmsg4(900, "set %s %p size=%d %s\n", type, res_incexe->current_opts,
              newsize, lc->str);
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
static void StoreBase(LEX* lc, ResourceItem* item, int index, int pass)
{
  LexGetToken(lc, BCT_NAME);
  if (pass == 1) {
    /*
     * Pickup Base Job Name
     */
    res_incexe->current_opts->base.append(strdup(lc->str));
  }
  ScanToEol(lc);
}

/**
 * Store reader info
 */
static void StorePlugin(LEX* lc, ResourceItem* item, int index, int pass)
{
  LexGetToken(lc, BCT_NAME);
  if (pass == 1) {
    /*
     * Pickup plugin command
     */
    res_incexe->current_opts->plugin = strdup(lc->str);
  }
  ScanToEol(lc);
}

/**
 * Store Wild-card info
 */
static void StoreWild(LEX* lc, ResourceItem* item, int index, int pass)
{
  int token;
  const char* type;
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
          res_incexe->current_opts->wilddir.append(strdup(lc->str));
          newsize = res_incexe->current_opts->wilddir.size();
        } else if (item->code == 2) {
          if (strpbrk(lc->str, "/\\") != NULL) {
            type = "wildfile";
            res_incexe->current_opts->wildfile.append(strdup(lc->str));
            newsize = res_incexe->current_opts->wildfile.size();
          } else {
            type = "wildbase";
            res_incexe->current_opts->wildbase.append(strdup(lc->str));
            newsize = res_incexe->current_opts->wildbase.size();
          }
        } else {
          type = "wild";
          res_incexe->current_opts->wild.append(strdup(lc->str));
          newsize = res_incexe->current_opts->wild.size();
        }
        Dmsg4(9, "set %s %p size=%d %s\n", type, res_incexe->current_opts,
              newsize, lc->str);
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
static void StoreFstype(LEX* lc, ResourceItem* item, int index, int pass)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (pass == 1) {
    /* Pickup fstype string */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
        res_incexe->current_opts->fstype.append(strdup(lc->str));
        Dmsg3(900, "set fstype %p size=%d %s\n", res_incexe->current_opts,
              res_incexe->current_opts->fstype.size(), lc->str);
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
static void StoreDrivetype(LEX* lc, ResourceItem* item, int index, int pass)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (pass == 1) {
    /* Pickup Drivetype string */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
        res_incexe->current_opts->Drivetype.append(strdup(lc->str));
        Dmsg3(900, "set Drivetype %p size=%d %s\n", res_incexe->current_opts,
              res_incexe->current_opts->Drivetype.size(), lc->str);
        break;
      default:
        scan_err1(lc, _("Expected a Drivetype string, got: %s\n"), lc->str);
    }
  }
  ScanToEol(lc);
}

static void StoreMeta(LEX* lc, ResourceItem* item, int index, int pass)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (pass == 1) {
    /* Pickup fstype string */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
        res_incexe->current_opts->meta.append(strdup(lc->str));
        Dmsg3(900, "set meta %p size=%d %s\n", res_incexe->current_opts,
              res_incexe->current_opts->meta.size(), lc->str);
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
static void StoreOption(
    LEX* lc,
    ResourceItem* item,
    int index,
    int pass,
    std::map<int, options_default_value_s>& option_default_values)
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
      if (option_default_values.find(keyword) != option_default_values.end()) {
        option_default_values[keyword].configured = true;
      }
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
    bstrncat(res_incexe->current_opts->opts, inc_opts, MAX_FOPTS);
    Dmsg2(900, "new pass=%d incexe opts=%s\n", pass,
          res_incexe->current_opts->opts);
  }

  ScanToEol(lc);
}

/**
 * If current_opts not defined, create first entry
 */
static void SetupCurrentOpts(void)
{
  FileOptions* fo = new FileOptions;
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
  res_incexe->current_opts = fo;
  res_incexe->file_options_list.push_back(fo);
}

/**
 * Come here when Options seen in Include/Exclude
 */
static void StoreOptionsRes(LEX* lc,
                            ResourceItem* item,
                            int index,
                            int pass,
                            bool exclude)
{
  int token, i;
  std::map<int, options_default_value_s> option_default_values = {
      {INC_KW_ACL, {false, "A"}}, {INC_KW_XATTR, {false, "X"}}};

  if (exclude) {
    scan_err0(lc, _("Options section not permitted in Exclude\n"));
    /* NOT REACHED */
  }
  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (token != BCT_BOB) {
    scan_err1(lc, _("Expecting open brace. Got %s"), lc->str);
  }

  if (pass == 1) { SetupCurrentOpts(); }

  while ((token = LexGetToken(lc, BCT_ALL)) != BCT_EOF) {
    if (token == BCT_EOL) { continue; }
    if (token == BCT_EOB) { break; }
    if (token != BCT_IDENTIFIER) {
      scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
    }
    bool found = false;
    for (i = 0; options_items[i].name; i++) {
      if (Bstrcasecmp(options_items[i].name, lc->str)) {
        token = LexGetToken(lc, BCT_SKIP_EOL);
        if (token != BCT_EQUALS) {
          scan_err1(lc, _("expected an equals, got: %s"), lc->str);
        }
        /* Call item handler */
        switch (options_items[i].type) {
          case CFG_TYPE_OPTION:
            StoreOption(lc, &options_items[i], i, pass, option_default_values);
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
        found = true;
        break;
      }
    }
    if (!found) {
      scan_err1(lc, _("Keyword %s not permitted in this resource"), lc->str);
    }
  }

  /* apply default values for unset options */
  if (pass == 1) {
    for (auto const& o : option_default_values) {
      int keyword_id = o.first;
      bool was_set_in_config = o.second.configured;
      std::string default_value = o.second.default_value;
      if (!was_set_in_config) {
        bstrncat(res_incexe->current_opts->opts, default_value.c_str(),
                 MAX_FOPTS);
        Dmsg2(900, "setting default value for keyword-id=%d, %s\n", keyword_id,
              default_value.c_str());
      }
    }
  }
}

static FilesetResource* GetStaticFilesetResource()
{
  FilesetResource* res_fs = nullptr;
  ResourceTable* t = my_config->GetResourceTable("FileSet");
  assert(t);
  if (t) { res_fs = dynamic_cast<FilesetResource*>(*t->allocated_resource_); }
  assert(res_fs);
  return res_fs;
}

/**
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void StoreFname(LEX* lc,
                       ResourceItem* item,
                       int index,
                       int pass,
                       bool exclude)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (pass == 1) {
    /* Pickup Filename string
     */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
        if (strchr(lc->str, '\\')) {
          scan_err1(lc,
                    _("Backslash found. Use forward slashes or quote the "
                      "string.: %s\n"),
                    lc->str);
          /* NOT REACHED */
        }
      case BCT_QUOTED_STRING: {
        FilesetResource* res_fs = GetStaticFilesetResource();
        if (res_fs->have_MD5) {
          MD5_Update(&res_fs->md5c, (unsigned char*)lc->str, lc->str_len);
        }

        if (res_incexe->name_list.size() == 0) {
          res_incexe->name_list.init(10, true);
        }
        res_incexe->name_list.append(strdup(lc->str));
        Dmsg1(900, "Add to name_list %s\n", lc->str);
        break;
      }
      default:
        scan_err1(lc, _("Expected a filename, got: %s"), lc->str);
    }
  }
  ScanToEol(lc);
}  // namespace directordaemon

/**
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void StorePluginName(LEX* lc,
                            ResourceItem* item,
                            int index,
                            int pass,
                            bool exclude)
{
  int token;

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
          scan_err1(lc,
                    _("Backslash found. Use forward slashes or quote the "
                      "string.: %s\n"),
                    lc->str);
          /* NOT REACHED */
        }
      case BCT_QUOTED_STRING: {
        FilesetResource* res_fs = GetStaticFilesetResource();

        if (res_fs->have_MD5) {
          MD5_Update(&res_fs->md5c, (unsigned char*)lc->str, lc->str_len);
        }
        if (res_incexe->plugin_list.size() == 0) {
          res_incexe->plugin_list.init(10, true);
        }
        res_incexe->plugin_list.append(strdup(lc->str));
        Dmsg1(900, "Add to plugin_list %s\n", lc->str);
        break;
      }
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
static void StoreExcludedir(LEX* lc,
                            ResourceItem* item,
                            int index,
                            int pass,
                            bool exclude)
{
  if (exclude) {
    scan_err0(lc,
              _("ExcludeDirContaining directive not permitted in Exclude.\n"));
    /* NOT REACHED */
    return;
  }

  LexGetToken(lc, BCT_NAME);
  if (pass == 1) {
    if (res_incexe->ignoredir.size() == 0) {
      res_incexe->ignoredir.init(10, true);
    }
    res_incexe->ignoredir.append(strdup(lc->str));
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
static void StoreNewinc(LEX* lc, ResourceItem* item, int index, int pass)
{
  FilesetResource* res_fs = GetStaticFilesetResource();

  // Store.. functions below only store in pass = 1
  if (pass == 1) { res_incexe = new IncludeExcludeItem; }

  if (!res_fs->have_MD5) {
    MD5_Init(&res_fs->md5c);
    res_fs->have_MD5 = true;
  }
  res_fs->new_include = true;
  int token;
  while ((token = LexGetToken(lc, BCT_SKIP_EOL)) != BCT_EOF) {
    if (token == BCT_EOB) { break; }
    if (token != BCT_IDENTIFIER) {
      scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
    }
    bool found = false;
    for (int i = 0; newinc_items[i].name; i++) {
      bool options = Bstrcasecmp(lc->str, "options");
      if (Bstrcasecmp(newinc_items[i].name, lc->str)) {
        if (!options) {
          token = LexGetToken(lc, BCT_SKIP_EOL);
          if (token != BCT_EQUALS) {
            scan_err1(lc, _("expected an equals, got: %s"), lc->str);
          }
        }
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
        found = true;
        break;
      }
    }
    if (!found) {
      scan_err1(lc, _("Keyword %s not permitted in this resource"), lc->str);
    }
  }

  if (pass == 1) {
    // store the pointer from res_incexe in each appropriate container
    if (item->code == 0) { /* include */
      res_fs->include_items.push_back(res_incexe);
      Dmsg1(900, "num_includes=%d\n", res_fs->include_items.size());
    } else { /* exclude */
      res_fs->exclude_items.push_back(res_incexe);
      Dmsg1(900, "num_excludes=%d\n", res_fs->exclude_items.size());
    }
    res_incexe = nullptr;
  }

  // all pointers must push_back above
  ASSERT(!res_incexe);

  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/**
 * Store FileSet Include/Exclude info
 *  new style includes are handled in StoreNewinc()
 */
void StoreInc(LEX* lc, ResourceItem* item, int index, int pass)
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
json_t* json_incexc(const int type)
{
  return json_datatype(type, newinc_items);
}

json_t* json_options(const int type)
{
  return json_datatype(type, options_items);
}
#endif
} /* namespace directordaemon */
