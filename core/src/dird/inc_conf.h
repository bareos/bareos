/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * Marco van Wieringen, June 2013
 */
/**
 * @file
 * FileSet include handling
 */
#ifndef _INC_CONF_H
#define _INC_CONF_H 1

/*
 * Define FileSet KeyWord values
 */
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
   INC_KW_REPLACE, /* restore options */
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
   { "compression", INC_KW_COMPRESSION },
   { "signature", INC_KW_DIGEST },
   { "encryption", INC_KW_ENCRYPTION },
   { "verify", INC_KW_VERIFY },
   { "basejob", INC_KW_BASEJOB },
   { "accurate", INC_KW_ACCURATE },
   { "onefs", INC_KW_ONEFS },
   { "recurse", INC_KW_RECURSE },
   { "sparse", INC_KW_SPARSE },
   { "hardlinks", INC_KW_HARDLINK },
   { "replace", INC_KW_REPLACE },
   { "readfifo", INC_KW_READFIFO },
   { "portable", INC_KW_PORTABLE },
   { "mtimeonly", INC_KW_MTIMEONLY },
   { "keepatime", INC_KW_KEEPATIME },
   { "exclude", INC_KW_EXCLUDE },
   { "aclsupport", INC_KW_ACL },
   { "ignorecase", INC_KW_IGNORECASE },
   { "hfsplussupport", INC_KW_HFSPLUS },
   { "noatime", INC_KW_NOATIME },
   { "enhancedwild", INC_KW_ENHANCEDWILD },
   { "checkfilechanges", INC_KW_CHKCHANGES },
   { "strippath", INC_KW_STRIPPATH },
   { "honornodumpflag", INC_KW_HONOR_NODUMP },
   { "xattrsupport", INC_KW_XATTR },
   { "size", INC_KW_SIZE },
   { "shadowing", INC_KW_SHADOWING },
   { "autoexclude", INC_KW_AUTO_EXCLUDE },
   { "forceencryption", INC_KW_FORCE_ENCRYPTION },
   { NULL, 0 }
};

/*
 * Options for FileSet keywords
 */
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
   { "md5", INC_KW_DIGEST, "M" },
   { "sha1", INC_KW_DIGEST, "S" },
   { "sha256", INC_KW_DIGEST, "S2" },
   { "sha512", INC_KW_DIGEST, "S3" },
   { "gzip", INC_KW_COMPRESSION, "Z6" },
   { "gzip1", INC_KW_COMPRESSION, "Z1" },
   { "gzip2", INC_KW_COMPRESSION, "Z2" },
   { "gzip3", INC_KW_COMPRESSION, "Z3" },
   { "gzip4", INC_KW_COMPRESSION, "Z4" },
   { "gzip5", INC_KW_COMPRESSION, "Z5" },
   { "gzip6", INC_KW_COMPRESSION, "Z6" },
   { "gzip7", INC_KW_COMPRESSION, "Z7" },
   { "gzip8", INC_KW_COMPRESSION, "Z8" },
   { "gzip9", INC_KW_COMPRESSION, "Z9" },
   { "lzo", INC_KW_COMPRESSION, "Zo" },
   { "lzfast", INC_KW_COMPRESSION, "Zff" },
   { "lz4", INC_KW_COMPRESSION, "Zf4" },
   { "lz4hc", INC_KW_COMPRESSION, "Zfh" },
   { "blowfish", INC_KW_ENCRYPTION, "Eb" },
   { "3des", INC_KW_ENCRYPTION, "E3" },
   { "aes128", INC_KW_ENCRYPTION, "Ea1" },
   { "aes192", INC_KW_ENCRYPTION, "Ea2" },
   { "aes256", INC_KW_ENCRYPTION, "Ea3" },
   { "camellia128", INC_KW_ENCRYPTION, "Ec1" },
   { "camellia192", INC_KW_ENCRYPTION, "Ec2" },
   { "camellia256", INC_KW_ENCRYPTION, "Ec3" },
   { "aes128hmacsha1", INC_KW_ENCRYPTION, "Eh1" },
   { "aes256hmacsha1", INC_KW_ENCRYPTION, "Eh2" },
   { "yes", INC_KW_ONEFS, "0" },
   { "no", INC_KW_ONEFS, "f" },
   { "yes", INC_KW_RECURSE, "0" },
   { "no", INC_KW_RECURSE, "h" },
   { "yes", INC_KW_SPARSE, "s" },
   { "no", INC_KW_SPARSE, "0" },
   { "yes", INC_KW_HARDLINK, "0" },
   { "no", INC_KW_HARDLINK, "H" },
   { "always", INC_KW_REPLACE, "a" },
   { "ifnewer", INC_KW_REPLACE, "w" },
   { "never", INC_KW_REPLACE, "n" },
   { "yes", INC_KW_READFIFO, "r" },
   { "no", INC_KW_READFIFO, "0" },
   { "yes", INC_KW_PORTABLE, "p" },
   { "no", INC_KW_PORTABLE, "0" },
   { "yes", INC_KW_MTIMEONLY, "m" },
   { "no", INC_KW_MTIMEONLY, "0" },
   { "yes", INC_KW_KEEPATIME, "k" },
   { "no", INC_KW_KEEPATIME, "0" },
   { "yes", INC_KW_EXCLUDE, "e" },
   { "no", INC_KW_EXCLUDE, "0" },
   { "yes", INC_KW_ACL, "A" },
   { "no", INC_KW_ACL, "0" },
   { "yes", INC_KW_IGNORECASE, "i" },
   { "no", INC_KW_IGNORECASE, "0" },
   { "yes", INC_KW_HFSPLUS, "R"}, /* "R" for resource fork */
   { "no", INC_KW_HFSPLUS, "0" },
   { "yes", INC_KW_NOATIME, "K" },
   { "no", INC_KW_NOATIME, "0" },
   { "yes", INC_KW_ENHANCEDWILD, "K" },
   { "no", INC_KW_ENHANCEDWILD, "0" },
   { "yes", INC_KW_CHKCHANGES, "c" },
   { "no", INC_KW_CHKCHANGES, "0" },
   { "yes", INC_KW_HONOR_NODUMP, "N" },
   { "no", INC_KW_HONOR_NODUMP, "0" },
   { "yes", INC_KW_XATTR, "X" },
   { "no", INC_KW_XATTR, "0" },
   { "localwarn", INC_KW_SHADOWING, "d1" },
   { "localremove", INC_KW_SHADOWING, "d2" },
   { "globalwarn", INC_KW_SHADOWING, "d3" },
   { "globalremove", INC_KW_SHADOWING, "d4" },
   { "none", INC_KW_SHADOWING, "0" },
   { "yes", INC_KW_AUTO_EXCLUDE, "0" },
   { "no", INC_KW_AUTO_EXCLUDE, "x" },
   { "yes", INC_KW_FORCE_ENCRYPTION, "Ef" },
   { "no", INC_KW_FORCE_ENCRYPTION, "0" },
   { NULL, 0, 0 }
};

#define PERMITTED_VERIFY_OPTIONS (const char *)"ipnugsamcd51"
#define PERMITTED_ACCURATE_OPTIONS (const char *)"ipnugsamcd51A"
#define PERMITTED_BASEJOB_OPTIONS (const char *)"ipnugsamcd51"

#endif /* _INC_CONF_H */
