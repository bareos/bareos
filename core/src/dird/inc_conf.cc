/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, March MMIII
/*
 * @file
 * Configuration file parser for new and old Include and
 * Exclude records
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/director_jcr_impl.h"
#include "findlib/match.h"
#include "lib/parse_conf.h"
#include "include/compiler_macro.h"

#include "lib/bregex.h"

#include "findlib/find.h"

#include "inc_conf.h"
#include "lib/edit.h"

#include <cassert>

namespace directordaemon {

#define PERMITTED_VERIFY_OPTIONS (const char*)"ipnugsamcd51"
#define PERMITTED_ACCURATE_OPTIONS (const char*)"ipnugsamcd51A"

typedef struct {
  bool configured;
  std::string default_value;
} options_default_value_s;


// Define FileSet KeyWord values
enum
{
  INC_KW_NONE,
  INC_KW_COMPRESSION,
  INC_KW_DIGEST,
  INC_KW_ENCRYPTION,
  INC_KW_VERIFY,
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
static struct s_kw FS_option_kw[]
    = {{"compression", INC_KW_COMPRESSION},
       {"signature", INC_KW_DIGEST},
       {"encryption", INC_KW_ENCRYPTION},
       {"verify", INC_KW_VERIFY},
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

// Options for FileSet keywords
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
static struct s_fs_opt FS_options[]
    = {{"md5", INC_KW_DIGEST, "M"},
       {"sha1", INC_KW_DIGEST, "S"},
       {"sha256", INC_KW_DIGEST, "S2"},
       {"sha512", INC_KW_DIGEST, "S3"},
       {"xxh128", INC_KW_DIGEST, "S4"},
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

// Imported subroutines
extern void StoreInc(lexer* lc, const ResourceItem* item, int index, int pass);

/* We build the current new Include and Exclude items here */
static IncludeExcludeItem* res_incexe;

/* clang-format off */

/* new Include/Exclude items
 * name handler value code flags default_value */
const ResourceItem newinc_items[] = {
  { "File", CFG_TYPE_FNAME, 0, nullptr, {}},
  { "Plugin", CFG_TYPE_PLUGINNAME, 0, nullptr, {}},
  { "ExcludeDirContaining", CFG_TYPE_EXCLUDEDIR, 0, nullptr, {}},
  { "Options", CFG_TYPE_OPTIONS, 0, nullptr, {}},
  {}
};

/* Items that are valid in an Options resource
 * name handler value code flags default_value */
const ResourceItem options_items[] = {
  { "Compression", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Signature", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Accurate", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Verify", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "OneFs", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Recurse", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Sparse", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "HardLinks", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "ReadFifo", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Replace", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Portable", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "MtimeOnly", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "KeepAtime", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Regex", CFG_TYPE_REGEX, 0, nullptr, {config::Code{0}}},
  { "RegexDir", CFG_TYPE_REGEX, 0, nullptr, {config::Code{1}}},
  { "RegexFile", CFG_TYPE_REGEX, 0, nullptr, {config::Code{2}}},
  { "Wild", CFG_TYPE_WILD, 0, nullptr, {config::Code{0}}},
  { "WildDir", CFG_TYPE_WILD, 0, nullptr, {config::Code{1}}},
  { "WildFile", CFG_TYPE_WILD, 0, nullptr, {config::Code{2}}},
  { "Exclude", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "AclSupport", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Plugin", CFG_TYPE_PLUGIN, 0, nullptr, {}},
  { "IgnoreCase", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "FsType", CFG_TYPE_FSTYPE, 0, nullptr, {}},
  { "HfsPlusSupport", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "NoAtime", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "EnhancedWild", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "DriveType", CFG_TYPE_DRIVETYPE, 0, nullptr, {}},
  { "CheckFileChanges", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "StripPath", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "HonorNoDumpFlag", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "XAttrSupport", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Size", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Shadowing", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "AutoExclude", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "ForceEncryption", CFG_TYPE_OPTION, 0, nullptr, {}},
  { "Meta", CFG_TYPE_META, 0, nullptr, {}},
  {}
};

/* clang-format on */


struct OptionsDefaultValues {
  std::map<int, options_default_value_s> option_default_values
      = {{INC_KW_ACL, {false, "A"}},
         {INC_KW_HARDLINK, {false, "H"}},
         {INC_KW_XATTR, {false, "X"}}};
};

// determine used compression algorithms
// returns if compression is used at all.
bool FindUsedCompressalgos(PoolMem* compressalgos, JobControlRecord* jcr)
{
  int cnt = 0;
  IncludeExcludeItem* inc;
  FileOptions* fopts;
  FilesetResource* fs;
  struct s_fs_opt* fs_opt;

  if (!jcr->dir_impl->res.job || !jcr->dir_impl->res.job->fileset) {
    return false;
  }

  fs = jcr->dir_impl->res.job->fileset;
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

  return cnt > 0;
}

// Check if the configured options are valid.
static inline void IsInPermittedSet(lexer* lc,
                                    const char* SetType,
                                    const char* permitted_set)
{
  const char *p, *q;
  bool found;

  for (p = lc->str(); *p; p++) {
    found = false;
    for (q = permitted_set; *q; q++) {
      if (*p == *q) {
        found = true;
        break;
      }
    }

    if (!found) {
      scan_err(lc, T_("Illegal %s option %c, got option string: %s:"), SetType,
               *p, lc->str());
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
static void ScanIncludeOptions(ConfigurationParser* p,
                               lexer* lc,
                               int keyword,
                               char* opts,
                               int optlen)
{
  int i;
  char option[64];
  auto lcopts = lc->options;
  struct s_sz_matching size_matching;

  memset(option, 0, sizeof(option));
  lc->options.set(lexer::options::ForceString); /* force string */
  LexGetToken(lc, BCT_STRING);                  /* expect at least one option */
  p->PushString(lc->str());
  if (keyword == INC_KW_VERIFY) { /* special case */
    IsInPermittedSet(lc, T_("verify"), PERMITTED_VERIFY_OPTIONS);
    bstrncat(opts, "V", optlen); /* indicate Verify */
    bstrncat(opts, lc->str(), optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else if (keyword == INC_KW_ACCURATE) { /* special case */
    IsInPermittedSet(lc, T_("accurate"), PERMITTED_ACCURATE_OPTIONS);
    bstrncat(opts, "C", optlen); /* indicate Accurate */
    bstrncat(opts, lc->str(), optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else if (keyword == INC_KW_STRIPPATH) { /* special case */
    if (!IsAnInteger(lc->str())) {
      scan_err(lc, T_("Expected a strip path positive integer, got: %s:"),
               lc->str());
      return;
    }
    bstrncat(opts, "P", optlen); /* indicate strip path */
    bstrncat(opts, lc->str(), optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else if (keyword == INC_KW_SIZE) { /* special case */
    if (!ParseSizeMatch(lc->str(), &size_matching)) {
      scan_err(lc, T_("Expected a parseable size, got: %s:"), lc->str());
      return;
    }
    bstrncat(opts, "z", optlen); /* indicate size */
    bstrncat(opts, lc->str(), optlen);
    bstrncat(opts, ":", optlen); /* Terminate it */
    Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
  } else {
    // Standard keyword options for Include/Exclude
    for (i = 0; FS_options[i].name; i++) {
      if (FS_options[i].keyword == keyword
          && Bstrcasecmp(lc->str(), FS_options[i].name)) {
        bstrncpy(option, FS_options[i].option, sizeof(option));
        i = 0;
        break;
      }
    }
    if (i != 0) {
      scan_err(lc, T_("Expected a FileSet option keyword, got: %s:"),
               lc->str());
      return;
    } else { /* add option */
      bstrncat(opts, option, optlen);
      Dmsg3(900, "Catopts=%s option=%s optlen=%d\n", opts, option, optlen);
    }
  }
  lc->options = lcopts;

  // If option terminated by comma, eat it
  if (lc->ch() == ',') { LexGetToken(lc, BCT_ALL); /* yes, eat comma */ }
}

static void ParseIncludeOptions(ConfigurationParser* p, lexer* lc)
{
  auto lcopts = lc->options;
  lc->options.set(lexer::options::ForceString); /* force string */
  LexGetToken(lc, BCT_STRING);                  /* expect at least one option */
  p->PushString(lc->str());
  lc->options = lcopts;

  // If option terminated by comma, eat it
  if (lc->ch() == ',') { LexGetToken(lc, BCT_ALL); /* yes, eat comma */ }
}

// Store regex info
static void StoreRegex(ConfigurationParser* p,
                       lexer* lc,
                       const ResourceItem* item,
                       int pass)
{
  int token, rc;
  regex_t preg{};
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
        p->PushMergeArray();
        p->PushObject();
        p->PushLabel("value");
        p->PushString(lc->str());
        rc = regcomp(&preg, lc->str(), REG_EXTENDED);
        if (rc != 0) {
          regerror(rc, &preg, prbuf, sizeof(prbuf));
          regfree(&preg);
          scan_err(lc, T_("Regex compile error. ERR=%s\n"), prbuf);
          return;
        }
        regfree(&preg);
        if (item->code == 1) {
          type = "regexdir";
          res_incexe->current_opts->regexdir.append(strdup(lc->str()));
          newsize = res_incexe->current_opts->regexdir.size();
        } else if (item->code == 2) {
          type = "regexfile";
          res_incexe->current_opts->regexfile.append(strdup(lc->str()));
          newsize = res_incexe->current_opts->regexfile.size();
        } else {
          type = "regex";
          res_incexe->current_opts->regex.append(strdup(lc->str()));
          newsize = res_incexe->current_opts->regex.size();
        }
        p->PushLabel("type");
        p->PushString(type);
        p->PopObject();
        p->PopArray();
        Dmsg4(900, "set %s %p size=%d %s\n", type, res_incexe->current_opts,
              newsize, lc->str());
        break;
      default:
        scan_err(lc, T_("Expected a regex string, got: %s\n"), lc->str());
        return;
    }
  }
  ScanToEol(lc);
}

static void ParseRegex(ConfigurationParser* p,
                       lexer* lc,
                       const ResourceItem* item)
{
  int token, rc;
  regex_t preg{};
  char prbuf[500];
  const char* type;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  /* Pickup regex string
   */
  switch (token) {
    case BCT_IDENTIFIER:
    case BCT_UNQUOTED_STRING:
    case BCT_QUOTED_STRING:
      p->PushObject();
      p->PushLabel("value");
      p->PushString(lc->str());
      rc = regcomp(&preg, lc->str(), REG_EXTENDED);
      if (rc != 0) {
        regerror(rc, &preg, prbuf, sizeof(prbuf));
        regfree(&preg);
        scan_err(lc, T_("Regex compile error. ERR=%s\n"), prbuf);
        return;
      }
      regfree(&preg);
      if (item->code == 1) {
        type = "regexdir";
      } else if (item->code == 2) {
        type = "regexfile";
      } else {
        type = "regex";
      }
      p->PushLabel("type");
      p->PushString(type);
      p->PopObject();
      Dmsg4(900, "set %s %p %s\n", type, res_incexe->current_opts, lc->str());
      break;
    default:
      scan_err(lc, T_("Expected a regex string, got: %s\n"), lc->str());
      return;
  }
  ScanToEol(lc);
}

// Store reader info
static void StorePlugin(ConfigurationParser* p,
                        lexer* lc,
                        const ResourceItem*,
                        int pass)
{
  LexGetToken(lc, BCT_NAME);
  p->PushString(lc->str());
  if (pass == 1) {
    // Pickup plugin command
    res_incexe->current_opts->plugin = strdup(lc->str());
  }
  ScanToEol(lc);
}

static void ParsePlugin(ConfigurationParser* p, lexer* lc, const ResourceItem*)
{
  LexGetToken(lc, BCT_NAME);
  p->PushString(lc->str());
  ScanToEol(lc);
}

// Store Wild-card info
static void StoreWild(ConfigurationParser* p,
                      lexer* lc,
                      const ResourceItem* item,
                      int pass)
{
  int token;
  const char* type;
  int newsize;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (pass == 1) {
    // Pickup Wild-card string
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
        p->PushMergeArray();
        p->PushObject();
        p->PushLabel("value");
        p->PushString(lc->str());
        if (item->code == 1) {
          type = "wilddir";
          res_incexe->current_opts->wilddir.append(strdup(lc->str()));
          newsize = res_incexe->current_opts->wilddir.size();
        } else if (item->code == 2) {
          if (strpbrk(lc->str(), "/\\") != NULL) {
            type = "wildfile";
            res_incexe->current_opts->wildfile.append(strdup(lc->str()));
            newsize = res_incexe->current_opts->wildfile.size();
          } else {
            type = "wildbase";
            res_incexe->current_opts->wildbase.append(strdup(lc->str()));
            newsize = res_incexe->current_opts->wildbase.size();
          }
        } else {
          type = "wild";
          res_incexe->current_opts->wild.append(strdup(lc->str()));
          newsize = res_incexe->current_opts->wild.size();
        }
        p->PushLabel("type");
        p->PushString(type);
        p->PopObject();
        p->PopArray();
        Dmsg4(9, "set %s %p size=%d %s\n", type, res_incexe->current_opts,
              newsize, lc->str());
        break;
      default:
        scan_err(lc, T_("Expected a wild-card string, got: %s\n"), lc->str());
        return;
    }
  }
  ScanToEol(lc);
}

static void ParseWild(ConfigurationParser* p,
                      lexer* lc,
                      const ResourceItem* item)
{
  int token;
  const char* type;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  switch (token) {
    case BCT_IDENTIFIER:
    case BCT_UNQUOTED_STRING:
    case BCT_QUOTED_STRING:
      p->PushObject();
      p->PushLabel("value");
      p->PushString(lc->str());
      if (item->code == 1) {
        type = "wilddir";
      } else if (item->code == 2) {
        if (strpbrk(lc->str(), "/\\") != NULL) {
          type = "wildfile";
        } else {
          type = "wildbase";
        }
      } else {
        type = "wild";
      }
      p->PushLabel("type");
      p->PushString(type);
      p->PopObject();
      Dmsg4(9, "set %s %p %s\n", type, res_incexe->current_opts, lc->str());
      break;
    default:
      scan_err(lc, T_("Expected a wild-card string, got: %s\n"), lc->str());
      return;
  }
  ScanToEol(lc);
}

// Store fstype info
static void StoreFstype(ConfigurationParser* p,
                        lexer* lc,
                        const ResourceItem*,
                        int pass)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  p->PushMergeArray();
  p->PushString(lc->str());
  p->PopArray();
  if (pass == 1) {
    /* Pickup fstype string */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
        res_incexe->current_opts->fstype.append(strdup(lc->str()));
        Dmsg3(900, "set fstype %p size=%d %s\n", res_incexe->current_opts,
              res_incexe->current_opts->fstype.size(), lc->str());
        break;
      default:
        scan_err(lc, T_("Expected a fstype string, got: %s\n"), lc->str());
        return;
    }
  }
  ScanToEol(lc);
}

static void ParseFstype(ConfigurationParser* p, lexer* lc, const ResourceItem*)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  /* Pickup fstype string */
  switch (token) {
    case BCT_IDENTIFIER:
    case BCT_UNQUOTED_STRING:
    case BCT_QUOTED_STRING:
      p->PushString(lc->str());
      break;
    default:
      scan_err(lc, T_("Expected a fstype string, got: %s\n"), lc->str());
      return;
  }
  ScanToEol(lc);
}

// Store Drivetype info
static void StoreDrivetype(ConfigurationParser* p,
                           lexer* lc,
                           const ResourceItem*,
                           int pass)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  p->PushMergeArray();
  p->PushString(lc->str());
  p->PopArray();
  if (pass == 1) {
    /* Pickup Drivetype string */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
        res_incexe->current_opts->Drivetype.append(strdup(lc->str()));
        Dmsg3(900, "set Drivetype %p size=%d %s\n", res_incexe->current_opts,
              res_incexe->current_opts->Drivetype.size(), lc->str());
        break;
      default:
        scan_err(lc, T_("Expected a Drivetype string, got: %s\n"), lc->str());
        return;
    }
  }
  ScanToEol(lc);
}

static void ParseDrivetype(ConfigurationParser* p,
                           lexer* lc,
                           const ResourceItem*)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  /* Pickup Drivetype string */
  switch (token) {
    case BCT_IDENTIFIER:
    case BCT_UNQUOTED_STRING:
    case BCT_QUOTED_STRING:
      p->PushString(lc->str());
      break;
    default:
      scan_err(lc, T_("Expected a Drivetype string, got: %s\n"), lc->str());
      return;
  }
  ScanToEol(lc);
}

static void StoreMeta(ConfigurationParser* p,
                      lexer* lc,
                      const ResourceItem*,
                      int pass)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  p->PushMergeArray();
  p->PushString(lc->str());
  p->PopArray();
  if (pass == 1) {
    /* Pickup fstype string */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
      case BCT_QUOTED_STRING:
        res_incexe->current_opts->meta.append(strdup(lc->str()));
        Dmsg3(900, "set meta %p size=%d %s\n", res_incexe->current_opts,
              res_incexe->current_opts->meta.size(), lc->str());
        break;
      default:
        scan_err(lc, T_("Expected a meta string, got: %s\n"), lc->str());
        return;
    }
  }
  ScanToEol(lc);
}

static void ParseMeta(ConfigurationParser* p, lexer* lc, const ResourceItem*)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  /* Pickup fstype string */
  switch (token) {
    case BCT_IDENTIFIER:
    case BCT_UNQUOTED_STRING:
    case BCT_QUOTED_STRING:
      p->PushString(lc->str());
      break;
    default:
      scan_err(lc, T_("Expected a meta string, got: %s\n"), lc->str());
      return;
  }
  ScanToEol(lc);
}

// New style options come here
static void StoreOption(
    ConfigurationParser* p,
    lexer* lc,
    const ResourceItem* item,
    int pass,
    std::map<int, options_default_value_s>& option_default_values)
{
  int i;
  int keyword;
  char inc_opts[100];

  inc_opts[0] = 0;
  keyword = INC_KW_NONE;

  // Look up the keyword
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
    scan_err(lc, T_("Expected a FileSet keyword, got: %s"), lc->str());
    return;
  }

  // Now scan for the value
  ScanIncludeOptions(p, lc, keyword, inc_opts, sizeof(inc_opts));
  if (pass == 1) {
    bstrncat(res_incexe->current_opts->opts, inc_opts, MAX_FOPTS);
    Dmsg2(900, "new pass=%d incexe opts=%s\n", pass,
          res_incexe->current_opts->opts);
  }

  ScanToEol(lc);
}

static void ParseOption(ConfigurationParser* p,
                        lexer* lc,
                        const ResourceItem* item)
{
  int i;
  int keyword;
  keyword = INC_KW_NONE;

  // Look up the keyword
  for (i = 0; FS_option_kw[i].name; i++) {
    if (Bstrcasecmp(item->name, FS_option_kw[i].name)) {
      keyword = FS_option_kw[i].token;
      break;
    }
  }

  if (keyword == INC_KW_NONE) {
    scan_err(lc, T_("Expected a FileSet keyword, got: %s"), lc->str());
    return;
  }

  // Now scan for the value
  ParseIncludeOptions(p, lc);

  ScanToEol(lc);
}

// If current_opts not defined, create first entry
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

static void ApplyDefaultValuesForUnsetOptions(
    OptionsDefaultValues default_values)
{
  for (auto const& option : default_values.option_default_values) {
    int keyword_id = option.first;
    bool was_set_in_config = option.second.configured;
    std::string default_value = option.second.default_value;
    if (!was_set_in_config) {
      bstrncat(res_incexe->current_opts->opts, default_value.c_str(),
               MAX_FOPTS);
      Dmsg2(900, "setting default value for keyword-id=%d, %s\n", keyword_id,
            default_value.c_str());
    }
  }
}

static void StoreDefaultOptions()
{
  SetupCurrentOpts();
  ApplyDefaultValuesForUnsetOptions(OptionsDefaultValues{});
}

// Come here when Options seen in Include/Exclude
static void StoreOptionsRes(ConfigurationParser* p,
                            lexer* lc,
                            const ResourceItem*,
                            int pass,
                            bool exclude)
{
  int token;
  OptionsDefaultValues default_values;

  if (exclude) {
    scan_err(lc, T_("Options section not permitted in Exclude\n"));
    return;
  }
  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (token != BCT_BOB) {
    scan_err(lc, T_("Expecting open brace. Got %s"), lc->str());
    return;
  }
  p->PushObject();

  if (pass == 1) { SetupCurrentOpts(); }

  while ((token = LexGetToken(lc, BCT_ALL)) != BCT_EOF) {
    if (token == BCT_EOL) { continue; }
    if (token == BCT_EOB) { break; }
    if (token != BCT_IDENTIFIER) {
      scan_err(lc, T_("Expecting keyword, got: %s\n"), lc->str());
      return;
    }
    bool found = false;
    for (int i = 0; options_items[i].name; i++) {
      if (Bstrcasecmp(options_items[i].name, lc->str())) {
        p->PushLabel(options_items[i].name);
        token = LexGetToken(lc, BCT_SKIP_EOL);
        if (token != BCT_EQUALS) {
          scan_err(lc, T_("expected an equals, got: %s"), lc->str());
          return;
        }
        /* Call item handler */
        switch (options_items[i].type) {
          case CFG_TYPE_OPTION:
            StoreOption(p, lc, &options_items[i], pass,
                        default_values.option_default_values);
            break;
          case CFG_TYPE_REGEX:
            StoreRegex(p, lc, &options_items[i], pass);
            break;
          case CFG_TYPE_WILD:
            StoreWild(p, lc, &options_items[i], pass);
            break;
          case CFG_TYPE_PLUGIN:
            StorePlugin(p, lc, &options_items[i], pass);
            break;
          case CFG_TYPE_FSTYPE:
            StoreFstype(p, lc, &options_items[i], pass);
            break;
          case CFG_TYPE_DRIVETYPE:
            StoreDrivetype(p, lc, &options_items[i], pass);
            break;
          case CFG_TYPE_META:
            StoreMeta(p, lc, &options_items[i], pass);
            break;
          default:
            break;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      scan_err(lc, T_("Keyword %s not permitted in this resource"), lc->str());
      return;
    }
  }
  p->PopObject();

  if (pass == 1) { ApplyDefaultValuesForUnsetOptions(default_values); }
}

static void ParseOptionsRes(ConfigurationParser* p, lexer* lc, bool exclude)
{
  int token;

  if (exclude) {
    scan_err(lc, T_("Options section not permitted in Exclude\n"));
    return;
  }
  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (token != BCT_BOB) {
    scan_err(lc, T_("Expecting open brace. Got %s"), lc->str());
    return;
  }
  p->PushObject();

  while ((token = LexGetToken(lc, BCT_ALL)) != BCT_EOF) {
    if (token == BCT_EOL) { continue; }
    if (token == BCT_EOB) { break; }
    if (token != BCT_IDENTIFIER) {
      scan_err(lc, T_("Expecting keyword, got: %s\n"), lc->str());
      return;
    }
    bool found = false;
    for (int i = 0; options_items[i].name; i++) {
      if (Bstrcasecmp(options_items[i].name, lc->str())) {
        p->PushLabel(options_items[i].name);
        token = LexGetToken(lc, BCT_SKIP_EOL);
        if (token != BCT_EQUALS) {
          scan_err(lc, T_("expected an equals, got: %s"), lc->str());
          return;
        }
        /* Call item handler */
        switch (options_items[i].type) {
          case CFG_TYPE_OPTION:
            ParseOption(p, lc, &options_items[i]);
            break;
          case CFG_TYPE_REGEX:
            ParseRegex(p, lc, &options_items[i]);
            break;
          case CFG_TYPE_WILD:
            ParseWild(p, lc, &options_items[i]);
            break;
          case CFG_TYPE_PLUGIN:
            ParsePlugin(p, lc, &options_items[i]);
            break;
          case CFG_TYPE_FSTYPE:
            ParseFstype(p, lc, &options_items[i]);
            break;
          case CFG_TYPE_DRIVETYPE:
            ParseDrivetype(p, lc, &options_items[i]);
            break;
          case CFG_TYPE_META:
            ParseMeta(p, lc, &options_items[i]);
            break;
          default:
            break;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      scan_err(lc, T_("Keyword %s not permitted in this resource"), lc->str());
      return;
    }
  }
  p->PopObject();
}

static FilesetResource* GetStaticFilesetResource(ConfigurationParser* p)
{
  FilesetResource* res_fs = nullptr;
  const ResourceTable* t = p->GetResourceTable("FileSet");
  ASSERT(t);
  if (t) { res_fs = dynamic_cast<FilesetResource*>(*t->allocated_resource_); }
  assert(res_fs);
  return res_fs;
}

/**
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void StoreFname(ConfigurationParser* p,
                       lexer* lc,
                       const ResourceItem*,
                       int pass,
                       bool)
{
  int token;

  token = LexGetToken(lc, BCT_SKIP_EOL);
  p->PushString(lc->str());
  if (pass == 1) {
    /* Pickup Filename string
     */
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
        if (strchr(lc->str(), '\\')) {
          scan_err(lc,
                   T_("Backslash found. Use forward slashes or quote the "
                      "string.: %s\n"),
                   lc->str());
          return;
        }
        FALLTHROUGH_INTENDED;
      case BCT_QUOTED_STRING: {
        FilesetResource* res_fs = GetStaticFilesetResource(p);
        if (res_fs->have_MD5) {
          IGNORE_DEPRECATED_ON;
          MD5_Update(&res_fs->md5c, (unsigned char*)lc->str(), lc->str_len());
          IGNORE_DEPRECATED_OFF;
        }

        if (res_incexe->name_list.size() == 0) {
          res_incexe->name_list.init(10, true);
        }
        res_incexe->name_list.append(strdup(lc->str()));
        Dmsg1(900, "Add to name_list %s\n", lc->str());
        break;
      }
      default:
        scan_err(lc, T_("Expected a filename, got: %s"), lc->str());
        return;
    }
  }
  ScanToEol(lc);
}

static void ParseFname(ConfigurationParser* p, lexer* lc, bool)
{
  LexGetToken(lc, BCT_SKIP_EOL);
  p->PushString(lc->str());
  ScanToEol(lc);
}

/**
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void StorePluginName(ConfigurationParser* p,
                            lexer* lc,
                            const ResourceItem*,
                            int pass,
                            bool exclude)
{
  int token;

  if (exclude) {
    scan_err(lc, T_("Plugin directive not permitted in Exclude\n"));
    return;
  }
  token = LexGetToken(lc, BCT_SKIP_EOL);
  p->PushString(lc->str());
  if (pass == 1) {
    // Pickup Filename string
    switch (token) {
      case BCT_IDENTIFIER:
      case BCT_UNQUOTED_STRING:
        if (strchr(lc->str(), '\\')) {
          scan_err(lc,
                   T_("Backslash found. Use forward slashes or quote the "
                      "string.: %s\n"),
                   lc->str());
          return;
        }
        FALLTHROUGH_INTENDED;
      case BCT_QUOTED_STRING: {
        FilesetResource* res_fs = GetStaticFilesetResource(p);

        if (res_fs->have_MD5) {
          IGNORE_DEPRECATED_ON;
          MD5_Update(&res_fs->md5c, (unsigned char*)lc->str(), lc->str_len());
          IGNORE_DEPRECATED_OFF;
        }
        if (res_incexe->plugin_list.size() == 0) {
          res_incexe->plugin_list.init(10, true);
        }
        res_incexe->plugin_list.append(strdup(lc->str()));
        Dmsg1(900, "Add to plugin_list %s\n", lc->str());
        break;
      }
      default:
        scan_err(lc, T_("Expected a filename, got: %s"), lc->str());
        return;
    }
  }
  ScanToEol(lc);
}

static void ParsePluginName(ConfigurationParser* p, lexer* lc, bool exclude)
{
  if (exclude) {
    scan_err(lc, T_("Plugin directive not permitted in Exclude\n"));
    return;
  }
  LexGetToken(lc, BCT_SKIP_EOL);
  p->PushString(lc->str());
  ScanToEol(lc);
}

// Store exclude directory containing info
static void StoreExcludedir(ConfigurationParser* p,
                            lexer* lc,
                            const ResourceItem*,
                            int pass,
                            bool exclude)
{
  if (exclude) {
    scan_err(lc,
             T_("ExcludeDirContaining directive not permitted in Exclude.\n"));
    return;
  }

  LexGetToken(lc, BCT_NAME);
  p->PushString(lc->str());
  if (pass == 1) {
    if (res_incexe->ignoredir.size() == 0) {
      res_incexe->ignoredir.init(10, true);
    }
    res_incexe->ignoredir.append(strdup(lc->str()));
    Dmsg1(900, "Add to ignoredir_list %s\n", lc->str());
  }
  ScanToEol(lc);
}

static void ParseExcludedir(ConfigurationParser* p, lexer* lc, bool exclude)
{
  if (exclude) {
    scan_err(lc,
             T_("ExcludeDirContaining directive not permitted in Exclude.\n"));
    return;
  }

  LexGetToken(lc, BCT_NAME);
  p->PushString(lc->str());
  ScanToEol(lc);
}

/**
 * Store new style FileSet Include/Exclude info
 *
 *  Note, when this routine is called, we are inside a FileSet
 *  resource.  We treat the Include/Exclude like a sort of
 *  mini-resource within the FileSet resource.
 */
static void StoreNewinc(ConfigurationParser* p,
                        lexer* lc,
                        const ResourceItem* item,
                        int index,
                        int pass)
{
  FilesetResource* res_fs = GetStaticFilesetResource(p);

  // Store.. functions below only store in pass = 1
  if (pass == 1) { res_incexe = new IncludeExcludeItem; }

  if (!res_fs->have_MD5) {
    IGNORE_DEPRECATED_ON;
    MD5_Init(&res_fs->md5c);
    IGNORE_DEPRECATED_OFF;
    res_fs->have_MD5 = true;
  }
  res_fs->new_include = true;
  int token;
  bool has_options = false;
  p->PushMergeArray();
  p->PushObject();
  while ((token = LexGetToken(lc, BCT_SKIP_EOL)) != BCT_EOF) {
    if (token == BCT_EOB) { break; }
    if (token != BCT_IDENTIFIER) {
      scan_err(lc, T_("Expecting keyword, got: %s\n"), lc->str());
      return;
    }
    bool found = false;
    for (int i = 0; newinc_items[i].name; i++) {
      bool options = Bstrcasecmp(lc->str(), "options");
      if (Bstrcasecmp(newinc_items[i].name, lc->str())) {
        p->PushLabel(newinc_items[i].name);
        if (!options) {
          token = LexGetToken(lc, BCT_SKIP_EOL);
          if (token != BCT_EQUALS) {
            scan_err(lc, T_("expected an equals, got: %s"), lc->str());
            return;
          }
        }

        p->PushMergeArray();
        switch (newinc_items[i].type) {
          case CFG_TYPE_FNAME:
            StoreFname(p, lc, &newinc_items[i], pass, item->code);
            break;
          case CFG_TYPE_PLUGINNAME:
            StorePluginName(p, lc, &newinc_items[i], pass, item->code);
            break;
          case CFG_TYPE_EXCLUDEDIR:
            StoreExcludedir(p, lc, &newinc_items[i], pass, item->code);
            break;
          case CFG_TYPE_OPTIONS:
            StoreOptionsRes(p, lc, &newinc_items[i], pass, item->code);
            has_options = true;
            break;
          default:
            break;
        }
        p->PopArray();
        found = true;

        break;
      }
    }
    if (!found) {
      scan_err(lc, T_("Keyword %s not permitted in this resource"), lc->str());
      return;
    }
  }
  p->PopObject();
  p->PopArray();

  if (!has_options) {
    if (pass == 1) { StoreDefaultOptions(); }
  }

  if (pass == 1) {
    // store the pointer from res_incexe in each appropriate container
    if (item->code == 0) { /* include */
      res_fs->include_items.push_back(res_incexe);
      Dmsg1(900, "num_includes=%" PRIuz "\n", res_fs->include_items.size());
    } else { /* exclude */
      res_fs->exclude_items.push_back(res_incexe);
      Dmsg1(900, "num_excludes=%" PRIuz "\n", res_fs->exclude_items.size());
    }
    res_incexe = nullptr;
  }

  // all pointers must push_back above
  ASSERT(!res_incexe);

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseNewinc(ConfigurationParser* p,
                        lexer* lc,
                        const ResourceItem* item,
                        int,
                        int)
{
  int token;
  p->PushObject();
  while ((token = LexGetToken(lc, BCT_SKIP_EOL)) != BCT_EOF) {
    if (token == BCT_EOB) { break; }
    if (token != BCT_IDENTIFIER) {
      scan_err(lc, T_("Expecting keyword, got: %s\n"), lc->str());
      return;
    }
    bool found = false;
    for (int i = 0; newinc_items[i].name; i++) {
      bool options = Bstrcasecmp(lc->str(), "options");
      if (Bstrcasecmp(newinc_items[i].name, lc->str())) {
        p->PushLabel(newinc_items[i].name);
        if (!options) {
          token = LexGetToken(lc, BCT_SKIP_EOL);
          if (token != BCT_EQUALS) {
            scan_err(lc, T_("expected an equals, got: %s"), lc->str());
            return;
          }
        }
        switch (newinc_items[i].type) {
          case CFG_TYPE_FNAME:
            ParseFname(p, lc, item->code);
            break;
          case CFG_TYPE_PLUGINNAME:
            ParsePluginName(p, lc, item->code);
            break;
          case CFG_TYPE_EXCLUDEDIR:
            ParseExcludedir(p, lc, item->code);
            break;
          case CFG_TYPE_OPTIONS:
            ParseOptionsRes(p, lc, item->code);
            break;
          default:
            break;
        }
        found = true;

        break;
      }
    }
    if (!found) {
      scan_err(lc, T_("Keyword %s not permitted in this resource"), lc->str());
      return;
    }
  }
  p->PopObject();

  ScanToEol(lc);
}

/**
 * Store FileSet Include/Exclude info
 *  new style includes are handled in StoreNewinc()
 */
void StoreInc(ConfigurationParser* p,
              lexer* lc,
              const ResourceItem* item,
              int index,
              int pass)
{
  int token;

  /* Decide if we are doing a new Include or an old include. The
   *  new Include is followed immediately by open brace, whereas the
   *  old include has options following the Include. */
  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (token == BCT_BOB) {
    StoreNewinc(p, lc, item, index, pass);
    return;
  }
  scan_err(lc, T_("Old style Include/Exclude not supported\n"));
}

void ParseInc(ConfigurationParser* p,
              lexer* lc,
              const ResourceItem* item,
              int index,
              int pass)
{
  int token;

  /* Decide if we are doing a new Include or an old include. The
   *  new Include is followed immediately by open brace, whereas the
   *  old include has options following the Include. */
  token = LexGetToken(lc, BCT_SKIP_EOL);
  if (token == BCT_BOB) {
    ParseNewinc(p, lc, item, index, pass);
    return;
  }
  scan_err(lc, T_("Old style Include/Exclude not supported\n"));
}

json_t* json_incexc(const int type)
{
  return json_datatype(type, newinc_items);
}

json_t* json_options(const int type)
{
  return json_datatype(type, options_items);
}

} /* namespace directordaemon */
