/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 *  Kern Sibbald, MM
 */
/**
 * @file
 * Stream definitions.  Split from baconfig.h Nov 2010
 */

#ifndef __BSTREAMS_H
#define __BSTREAMS_H 1

/**
 * Stream bits  -- these bits are new as of 24Nov10
 */
#define STREAM_BIT_64                         (1 << 30) /**< 64 bit stream (not yet implemented) */
#define STREAM_BIT_BITS                       (1 << 29) /**< Following bits may be set */
#define STREAM_BIT_PLUGIN                     (1 << 28) /**< Item written by a plugin */
#define STREAM_BIT_DELTA                      (1 << 27) /**< Stream contains delta data */
#define STREAM_BIT_OFFSETS                    (1 << 26) /**< Stream has data offset */
#define STREAM_BIT_PORTABLE_DATA              (1 << 25) /**< Data is portable */

/**
 * TYPE represents our current (old) stream types -- e.g. values 0 - 2047
 */
#define STREAMBASE_TYPE                         0       /**< base for types */
#define STREAMBITS_TYPE                         11      /**< type bit size */
#define STREAMMASK_TYPE                         (~((~0U)<< STREAMBITS_TYPE) << STREAMBASE_TYPE)
/**
 * Note additional base, bits, and masks can be defined for new
 * ranges or subranges of stream attributes.
 */

/**
 * Old, but currently used Stream definitions. Once defined these must NEVER
 * change as they go on the storage media.
 *
 * Note, the following streams are passed from the SD to the DIR
 * so that they may be put into the catalog (actually only the
 * stat packet part of the attr record is put in the catalog.
 *
 * STREAM_UNIX_ATTRIBUTES
 * STREAM_UNIX_ATTRIBUTES_EX
 * STREAM_MD5_DIGEST
 * STREAM_SHA1_DIGEST
 * STREAM_SHA256_DIGEST
 * STREAM_SHA512_DIGEST
 */
#define STREAM_NONE                             0       /**< Reserved Non-Stream */
#define STREAM_UNIX_ATTRIBUTES                  1       /**< Generic Unix attributes */
#define STREAM_FILE_DATA                        2       /**< Standard uncompressed data */
#define STREAM_MD5_SIGNATURE                    3       /**< MD5 signature - Deprecated */
#define STREAM_MD5_DIGEST                       3       /**< MD5 digest for the file */
#define STREAM_GZIP_DATA                        4       /**< GZip compressed file data - Deprecated */
#define STREAM_UNIX_ATTRIBUTES_EX               5       /**< Extended Unix attr for Win32 EX - Deprecated */
#define STREAM_SPARSE_DATA                      6       /**< Sparse data stream */
#define STREAM_SPARSE_GZIP_DATA                 7       /**< Sparse gzipped data stream - Deprecated */
#define STREAM_PROGRAM_NAMES                    8       /**< Program names for program data */
#define STREAM_PROGRAM_DATA                     9       /**< Data needing program */
#define STREAM_SHA1_SIGNATURE                  10       /**< SHA1 signature - Deprecated */
#define STREAM_SHA1_DIGEST                     10       /**< SHA1 digest for the file */
#define STREAM_WIN32_DATA                      11       /**< Win32 BackupRead data */
#define STREAM_WIN32_GZIP_DATA                 12       /**< Gzipped Win32 BackupRead data - Deprecated */
#define STREAM_MACOS_FORK_DATA                 13       /**< Mac resource fork */
#define STREAM_HFSPLUS_ATTRIBUTES              14       /**< Mac OS extra attributes */
#define STREAM_UNIX_ACCESS_ACL                 15       /**< Standard ACL attributes on UNIX - Deprecated */
#define STREAM_UNIX_DEFAULT_ACL                16       /**< Default ACL attributes on UNIX - Deprecated */
#define STREAM_SHA256_DIGEST                   17       /**< SHA-256 digest for the file */
#define STREAM_SHA512_DIGEST                   18       /**< SHA-512 digest for the file */
#define STREAM_SIGNED_DIGEST                   19       /**< Signed File Digest, ASN.1, DER Encoded */
#define STREAM_ENCRYPTED_FILE_DATA             20       /**< Encrypted, uncompressed data */
#define STREAM_ENCRYPTED_WIN32_DATA            21       /**< Encrypted, uncompressed Win32 BackupRead data */
#define STREAM_ENCRYPTED_SESSION_DATA          22       /**< Encrypted, Session Data, ASN.1, DER Encoded */
#define STREAM_ENCRYPTED_FILE_GZIP_DATA        23       /**< Encrypted, compressed data - Deprecated */
#define STREAM_ENCRYPTED_WIN32_GZIP_DATA       24       /**< Encrypted, compressed Win32 BackupRead data - Deprecated */
#define STREAM_ENCRYPTED_MACOS_FORK_DATA       25       /**< Encrypted, uncompressed Mac resource fork */
#define STREAM_PLUGIN_NAME                     26       /**< Plugin "file" string */
#define STREAM_PLUGIN_DATA                     27       /**< Plugin specific data */
#define STREAM_RESTORE_OBJECT                  28       /**< Plugin restore object */

/**
 * Compressed streams. These streams can handle arbitrary compression algorithm data
 * as an additional header is stored at the beginning of the stream.
 * See stream_compressed_header definition for more details.
 */
#define STREAM_COMPRESSED_DATA                 29       /**< Compressed file data */
#define STREAM_SPARSE_COMPRESSED_DATA          30       /**< Sparse compressed data stream */
#define STREAM_WIN32_COMPRESSED_DATA           31       /**< Compressed Win32 BackupRead data */
#define STREAM_ENCRYPTED_FILE_COMPRESSED_DATA  32       /**< Encrypted, compressed data */
#define STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA 33       /**< Encrypted, compressed Win32 BackupRead data */

#define STREAM_NDMP_SEPARATOR                 999       /**< NDMP separator between multiple data streams of one job */

/**
 * The Stream numbers from 1000-1999 are reserved for ACL and extended attribute streams.
 * Each different platform has its own stream id(s), if a platform supports multiple stream types
 * it should supply different handlers for each type it supports and this should be called
 * from the stream dispatch function. Currently in this reserved space we allocate the
 * different acl streams from 1000 on and the different extended attributes streams from
 * 1999 down. So the two naming spaces grow towards each other.
 */
#define STREAM_ACL_AIX_TEXT                  1000       /**< AIX specific string representation from acl_get */
#define STREAM_ACL_DARWIN_ACCESS_ACL         1001       /**< Darwin (OSX) specific acl_t string representation
                                                         * from acl_to_text (POSIX acl)
                                                         */
#define STREAM_ACL_FREEBSD_DEFAULT_ACL       1002       /**< FreeBSD specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for default acls.
                                                         */
#define STREAM_ACL_FREEBSD_ACCESS_ACL        1003       /**< FreeBSD specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for access acls.
                                                         */
#define STREAM_ACL_HPUX_ACL_ENTRY            1004       /**< HPUX specific acl_entry string representation
                                                         * from acltostr (POSIX acl)
                                                         */
#define STREAM_ACL_IRIX_DEFAULT_ACL          1005       /**< IRIX specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for default acls.
                                                         */
#define STREAM_ACL_IRIX_ACCESS_ACL           1006       /**< IRIX specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for access acls.
                                                         */
#define STREAM_ACL_LINUX_DEFAULT_ACL         1007       /**< Linux specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for default acls.
                                                         */
#define STREAM_ACL_LINUX_ACCESS_ACL          1008       /**< Linux specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for access acls.
                                                         */
#define STREAM_ACL_TRU64_DEFAULT_ACL         1009       /**< Tru64 specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for default acls.
                                                         */
#define STREAM_ACL_TRU64_DEFAULT_DIR_ACL     1010       /**< Tru64 specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for default acls.
                                                         */
#define STREAM_ACL_TRU64_ACCESS_ACL          1011       /**< Tru64 specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for access acls.
                                                         */
#define STREAM_ACL_SOLARIS_ACLENT            1012       /**< Solaris specific aclent_t string representation
                                                         * from acltotext or acl_totext (POSIX acl)
                                                         */
#define STREAM_ACL_SOLARIS_ACE               1013       /**< Solaris specific ace_t string representation from
                                                         * from acl_totext (NFSv4 or ZFS acl)
                                                         */
#define STREAM_ACL_AFS_TEXT                  1014       /**< AFS specific string representation from pioctl */
#define STREAM_ACL_AIX_AIXC                  1015       /**< AIX specific string representation from
                                                         * aclx_printStr (POSIX acl)
                                                         */
#define STREAM_ACL_AIX_NFS4                  1016       /**< AIX specific string representation from
                                                         * aclx_printStr (NFSv4 acl)
                                                         */
#define STREAM_ACL_FREEBSD_NFS4_ACL          1017       /**< FreeBSD specific acl_t string representation
                                                         * from acl_to_text (NFSv4 or ZFS acl)
                                                         */
#define STREAM_ACL_HURD_DEFAULT_ACL          1018       /**< GNU HURD specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for default acls.
                                                         */
#define STREAM_ACL_HURD_ACCESS_ACL           1019       /**< GNU HURD specific acl_t string representation
                                                         * from acl_to_text (POSIX acl) for access acls.
                                                         */
#define STREAM_ACL_PLUGIN                    1020       /**< Plugin specific acl encoding */
#define STREAM_XATTR_PLUGIN                  1988       /**< Plugin specific extended attributes */
#define STREAM_XATTR_HURD                    1989       /**< GNU HURD specific extended attributes */
#define STREAM_XATTR_IRIX                    1990       /**< IRIX specific extended attributes */
#define STREAM_XATTR_TRU64                   1991       /**< TRU64 specific extended attributes */
#define STREAM_XATTR_AIX                     1992       /**< AIX specific extended attributes */
#define STREAM_XATTR_OPENBSD                 1993       /**< OpenBSD specific extended attributes */
#define STREAM_XATTR_SOLARIS_SYS             1994       /**< Solaris specific extensible attributes or
                                                         * otherwise named extended system attributes.
                                                         */
#define STREAM_XATTR_SOLARIS                 1995       /**< Solaris specific extented attributes */
#define STREAM_XATTR_DARWIN                  1996       /**< Darwin (OSX) specific extended attributes */
#define STREAM_XATTR_FREEBSD                 1997       /**< FreeBSD specific extended attributes */
#define STREAM_XATTR_LINUX                   1998       /**< Linux specific extended attributes */
#define STREAM_XATTR_NETBSD                  1999       /**< NetBSD specific extended attributes */

/**
 * WARNING!!! do not define more than 2047 of these old types
 */

#endif /* __BSTREAMS_H */
