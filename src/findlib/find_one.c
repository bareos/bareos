/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
 * This file was derived from GNU TAR source code. Except for a few key
 * ideas, it has been entirely rewritten for Bareos.
 *
 * Kern Sibbald, MM
 *
 * Thanks to the TAR programmers.
 */

#include "bareos.h"
#include "find.h"
#ifdef HAVE_DARWIN_OS
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/attr.h>
#endif

extern int32_t name_max;              /* filename max length */
extern int32_t path_max;              /* path name max length */

/*
 * Structure for keeping track of hard linked files, we
 * keep an entry for each hardlinked file that we save,
 * which is the first one found. For all the other files that
 * are linked to this one, we save only the directory
 * entry so we can link it.
 */
struct f_link {
    struct f_link *next;
    dev_t dev;                        /* device */
    ino_t ino;                        /* inode with device is unique */
    uint32_t FileIndex;               /* Bareos FileIndex of this file */
    int32_t digest_stream;            /* Digest type if needed */
    uint32_t digest_len;              /* Digest len if needed */
    char *digest;                     /* Checksum of the file if needed */
    char name[1];                     /* The name */
};

typedef struct f_link link_t;
#define LINK_HASHTABLE_BITS 16
#define LINK_HASHTABLE_SIZE (1<<LINK_HASHTABLE_BITS)
#define LINK_HASHTABLE_MASK (LINK_HASHTABLE_SIZE-1)

static inline int LINKHASH(const struct stat &info)
{
    int hash = info.st_dev;
    unsigned long long i = info.st_ino;
    hash ^= i;
    i >>= 16;
    hash ^= i;
    i >>= 16;
    hash ^= i;
    i >>= 16;
    hash ^= i;
    return hash & LINK_HASHTABLE_MASK;
}

/*
 * Create a new directory Find File packet, but copy
 * some of the essential info from the current packet.
 * However, be careful to zero out the rest of the
 * packet.
 */
static inline FF_PKT *new_dir_ff_pkt(FF_PKT *ff_pkt)
{
   FF_PKT *dir_ff_pkt = (FF_PKT *)bmalloc(sizeof(FF_PKT));
   memcpy(dir_ff_pkt, ff_pkt, sizeof(FF_PKT));
   dir_ff_pkt->fname = bstrdup(ff_pkt->fname);
   dir_ff_pkt->link = bstrdup(ff_pkt->link);
   dir_ff_pkt->sys_fname = get_pool_memory(PM_FNAME);
   dir_ff_pkt->included_files_list = NULL;
   dir_ff_pkt->excluded_files_list = NULL;
   dir_ff_pkt->excluded_paths_list = NULL;
   dir_ff_pkt->linkhash = NULL;
   dir_ff_pkt->fname_save = NULL;
   dir_ff_pkt->link_save = NULL;
   dir_ff_pkt->ignoredir_fname = NULL;
   return dir_ff_pkt;
}

/*
 * Free the temp directory ff_pkt
 */
static void free_dir_ff_pkt(FF_PKT *dir_ff_pkt)
{
   free(dir_ff_pkt->fname);
   free(dir_ff_pkt->link);
   free_pool_memory(dir_ff_pkt->sys_fname);
   if (dir_ff_pkt->fname_save) {
      free_pool_memory(dir_ff_pkt->fname_save);
   }
   if (dir_ff_pkt->link_save) {
      free_pool_memory(dir_ff_pkt->link_save);
   }
   if (dir_ff_pkt->ignoredir_fname) {
      free_pool_memory(dir_ff_pkt->ignoredir_fname);
   }
   free(dir_ff_pkt);
}

/*
 * Check to see if we allow the file system type of a file or directory.
 * If we do not have a list of file system types, we accept anything.
 */
static int accept_fstype(FF_PKT *ff, void *dummy) {
   int i;
   char fs[1000];
   bool accept = true;

   if (ff->fstypes.size()) {
      accept = false;
      if (!fstype(ff->fname, fs, sizeof(fs))) {
         Dmsg1(50, "Cannot determine file system type for \"%s\"\n", ff->fname);
      } else {
         for (i = 0; i < ff->fstypes.size(); ++i) {
            if (bstrcmp(fs, (char *)ff->fstypes.get(i))) {
               Dmsg2(100, "Accepting fstype %s for \"%s\"\n", fs, ff->fname);
               accept = true;
               break;
            }
            Dmsg3(200, "fstype %s for \"%s\" does not match %s\n", fs,
                  ff->fname, ff->fstypes.get(i));
         }
      }
   }
   return accept;
}

/*
 * Check to see if we allow the drive type of a file or directory.
 * If we do not have a list of drive types, we accept anything.
 */
static inline int accept_drivetype(FF_PKT *ff, void *dummy) {
   int i;
   char dt[100];
   bool accept = true;

   if (ff->drivetypes.size()) {
      accept = false;
      if (!drivetype(ff->fname, dt, sizeof(dt))) {
         Dmsg1(50, "Cannot determine drive type for \"%s\"\n", ff->fname);
      } else {
         for (i = 0; i < ff->drivetypes.size(); ++i) {
            if (bstrcmp(dt, (char *)ff->drivetypes.get(i))) {
               Dmsg2(100, "Accepting drive type %s for \"%s\"\n", dt, ff->fname);
               accept = true;
               break;
            }
            Dmsg3(200, "drive type %s for \"%s\" does not match %s\n", dt,
                  ff->fname, ff->drivetypes.get(i));
         }
      }
   }
   return accept;
}

/*
 * This function determines whether we can use getattrlist()
 * It's odd, but we have to use the function to determine that...
 * Also, the man pages talk about things as if they were implemented.
 *
 * On Mac OS X, this succesfully differentiates between HFS+ and UFS
 * volumes, which makes me trust it is OK for others, too.
 */
static bool volume_has_attrlist(const char *fname)
{
#ifdef HAVE_DARWIN_OS
   struct statfs st;
   struct volinfo_struct {
      unsigned long length;               /* Mandatory field */
      vol_capabilities_attr_t info;       /* Volume capabilities */
   } vol;
   struct attrlist attrList;

   memset(&attrList, 0, sizeof(attrList));
   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.volattr = ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES;
   if (statfs(fname, &st) == 0) {
      /*
       * We need to check on the mount point
       */
      if (getattrlist(st.f_mntonname, &attrList, &vol, sizeof(vol), FSOPT_NOFOLLOW) == 0 &&
         (vol.info.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_ATTRLIST) &&
         (vol.info.valid[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_ATTRLIST)) {
         return true;
      }
   }
#endif
   return false;
}

/*
 * check for BSD nodump flag
 */
static inline bool no_dump(JCR *jcr, FF_PKT *ff_pkt)
{
#if defined(HAVE_CHFLAGS) && defined(UF_NODUMP)
   if ( (ff_pkt->flags & FO_HONOR_NODUMP) &&
        (ff_pkt->statp.st_flags & UF_NODUMP) ) {
      Jmsg(jcr, M_INFO, 1, _("     NODUMP flag set - will not process %s\n"),
           ff_pkt->fname);
      return true;                    /* do not backup this file */
   }
#endif
   return false;                      /* do backup */
}

/*
 * check for sizes
 */
static inline bool check_size_matching(JCR *jcr, FF_PKT *ff_pkt)
{
   ssize_t begin_size, end_size, difference;

   /*
    * See if size matching is turned on.
    */
   if (!ff_pkt->size_match) {
      return true;
   }

   /*
    * Loose the unsigned bits to keep the compiler from warning
    * about comparing signed and unsigned. As a size of a file
    * can only be positive the unsigned is not really to interesting.
    */
   begin_size = ff_pkt->size_match->begin_size;
   end_size = ff_pkt->size_match->end_size;

   /*
    * See what kind of matching should be done.
    */
   switch (ff_pkt->size_match->type) {
   case size_match_approx:
      /*
       * Calculate the fraction this size is of the wanted size.
       */
      if ((ssize_t)ff_pkt->statp.st_size > begin_size) {
         difference = ff_pkt->statp.st_size - begin_size;
      } else {
         difference = begin_size - ff_pkt->statp.st_size;
      }

      /*
       * See if the difference is less then 1% of the total.
       */
      return (difference < (begin_size / 100));
   case size_match_smaller:
      return (ssize_t)ff_pkt->statp.st_size < begin_size;
   case size_match_greater:
      return (ssize_t)ff_pkt->statp.st_size > begin_size;
   case size_match_range:
      return ((ssize_t)ff_pkt->statp.st_size >= begin_size) &&
             ((ssize_t)ff_pkt->statp.st_size <= end_size);
   default:
      return true;
   }
}

/*
 * Check if a file have changed during backup and display an error
 */
bool has_file_changed(JCR *jcr, FF_PKT *ff_pkt)
{
   struct stat statp;
   Dmsg1(500, "has_file_changed fname=%s\n",ff_pkt->fname);

   if (ff_pkt->type != FT_REG) { /* not a regular file */
      return false;
   }

   if (lstat(ff_pkt->fname, &statp) != 0) {
      berrno be;
      Jmsg(jcr, M_WARNING, 0,
           _("Cannot stat file %s: ERR=%s\n"),ff_pkt->fname,be.bstrerror());
      return true;
   }

   if (statp.st_mtime != ff_pkt->statp.st_mtime) {
      Jmsg(jcr, M_ERROR, 0, _("%s: mtime changed during backup.\n"), ff_pkt->fname);
      Dmsg3(50, "%s mtime (%lld) changed during backup (%lld).\n", ff_pkt->fname,
            (int64_t)ff_pkt->statp.st_mtime, (int64_t)statp.st_mtime);
      return true;
   }

   if (statp.st_ctime != ff_pkt->statp.st_ctime) {
      Jmsg(jcr, M_ERROR, 0, _("%s: ctime changed during backup.\n"), ff_pkt->fname);
      Dmsg3(50, "%s ctime (%lld) changed during backup (%lld).\n", ff_pkt->fname,
            (int64_t)ff_pkt->statp.st_ctime, (int64_t)statp.st_ctime);
      return true;
   }

   if (statp.st_size != ff_pkt->statp.st_size) {
      /* TODO: add size change */
      Jmsg(jcr, M_ERROR, 0, _("%s: size changed during backup.\n"),ff_pkt->fname);
      Dmsg3(50, "%s size (%lld) changed during backup (%lld).\n", ff_pkt->fname,
            (int64_t)ff_pkt->statp.st_size, (int64_t)statp.st_size);
      return true;
   }

   if ((statp.st_blksize != ff_pkt->statp.st_blksize) ||
       (statp.st_blocks  != ff_pkt->statp.st_blocks)) {
      Jmsg(jcr, M_ERROR, 0, _("%s: size changed during backup.\n"),ff_pkt->fname);
      Dmsg3(50, "%s size (%lld) changed during backup (%lld).\n", ff_pkt->fname,
            (int64_t)ff_pkt->statp.st_blocks, (int64_t)statp.st_blocks);
      return true;
   }

   return false;
}

/*
 * For incremental/diffential or accurate backups, we
 * determine if the current file has changed.
 */
bool check_changes(JCR *jcr, FF_PKT *ff_pkt)
{
   /*
    * In special mode (like accurate backup), the programmer can
    * choose his comparison function.
    */
   if (ff_pkt->check_fct) {
      return ff_pkt->check_fct(jcr, ff_pkt);
   }

   /*
    * For normal backups (incr/diff), we use this default behaviour
    */
   if (ff_pkt->incremental &&
      (ff_pkt->statp.st_mtime < ff_pkt->save_time &&
      ((ff_pkt->flags & FO_MTIMEONLY) ||
        ff_pkt->statp.st_ctime < ff_pkt->save_time))) {
      return false;
   }

   return true;
}

static inline bool have_ignoredir(FF_PKT *ff_pkt)
{
   struct stat sb;
   char *ignoredir;

   /*
    * Ensure that pointers are defined
    */
   if (!ff_pkt->fileset || !ff_pkt->fileset->incexe) {
      return false;
   }

   for (int i = 0; i < ff_pkt->fileset->incexe->ignoredir.size(); i++) {
      ignoredir = (char *)ff_pkt->fileset->incexe->ignoredir.get(i);

      if (ignoredir) {
         if (!ff_pkt->ignoredir_fname) {
            ff_pkt->ignoredir_fname = get_pool_memory(PM_FNAME);
         }
         Mmsg(ff_pkt->ignoredir_fname, "%s/%s", ff_pkt->fname, ignoredir);
         if (stat(ff_pkt->ignoredir_fname, &sb) == 0) {
            Dmsg2(100, "Directory '%s' ignored (found %s)\n",
                  ff_pkt->fname, ignoredir);
            return true;      /* Just ignore this directory */
         }
      }
   }

   return false;
}

/*
 * When the current file is a hardlink, the backup code can compute
 * the checksum and store it into the link_t structure.
 */
void ff_pkt_set_link_digest(FF_PKT *ff_pkt, int32_t digest_stream,
                            const char *digest, uint32_t len)
{
   if (ff_pkt->linked && !ff_pkt->linked->digest) {     /* is a hardlink */
      ff_pkt->linked->digest = (char *) bmalloc(len);
      memcpy(ff_pkt->linked->digest, digest, len);
      ff_pkt->linked->digest_len = len;
      ff_pkt->linked->digest_stream = digest_stream;
   }
}

#ifdef HAVE_DARWIN_OS
/*
 * Handling of a HFS+ attributes.
 */
static inline int process_hfsattributes(JCR *jcr, FF_PKT *ff_pkt,
                                        int handle_file(JCR *jcr, FF_PKT *ff, bool top_level),
                                        char *fname, bool top_level)
{
    /*
     * TODO: initialise attrList once elsewhere?
     */
    struct attrlist attrList;

    memset(&attrList, 0, sizeof(attrList));
    attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrList.commonattr = ATTR_CMN_FNDRINFO;
    attrList.fileattr = ATTR_FILE_RSRCLENGTH;
    if (getattrlist(fname, &attrList, &ff_pkt->hfsinfo,
                    sizeof(ff_pkt->hfsinfo), FSOPT_NOFOLLOW) != 0) {
       ff_pkt->type = FT_NOSTAT;
       ff_pkt->ff_errno = errno;
       return handle_file(jcr, ff_pkt, top_level);
    }

    return -1;
}
#endif

/*
 * Handling of a hardlinked file.
 */
static inline int process_hardlink(JCR *jcr, FF_PKT *ff_pkt,
                                   int handle_file(JCR *jcr, FF_PKT *ff, bool top_level),
                                   char *fname, bool top_level, bool *done)
{
   int rtn_stat = 0;
   int len;
   struct f_link *lp;
   const int linkhash = LINKHASH(ff_pkt->statp);

   if (ff_pkt->linkhash == NULL) {
       ff_pkt->linkhash = (link_t **)bmalloc(LINK_HASHTABLE_SIZE * sizeof(link_t *));
       memset(ff_pkt->linkhash, 0, LINK_HASHTABLE_SIZE * sizeof(link_t *));
   }

   /*
    * Search link list of hard linked files
    */
   for (lp = ff_pkt->linkhash[linkhash]; lp; lp = lp->next) {
      if (lp->ino == (ino_t)ff_pkt->statp.st_ino &&
          lp->dev == (dev_t)ff_pkt->statp.st_dev) {
         /*
          * If we have already backed up the hard linked file don't do it again
          */
         if (bstrcmp(lp->name, fname)) {
            Dmsg2(400, "== Name identical skip FI=%d file=%s\n", lp->FileIndex, fname);
            *done = true;
            return 1; /* ignore */
         }
         ff_pkt->link = lp->name;
         ff_pkt->type = FT_LNKSAVED; /* Handle link, file already saved */
         ff_pkt->LinkFI = lp->FileIndex;
         ff_pkt->linked = 0;
         ff_pkt->digest = lp->digest;
         ff_pkt->digest_stream = lp->digest_stream;
         ff_pkt->digest_len = lp->digest_len;
         rtn_stat = handle_file(jcr, ff_pkt, top_level);
         Dmsg3(400, "FT_LNKSAVED FI=%d LinkFI=%d file=%s\n",
               ff_pkt->FileIndex, lp->FileIndex, lp->name);
         *done = true;
         return rtn_stat;
      }
   }

   /*
    * File not previously dumped. Chain it into our list.
    */
   len = strlen(fname) + 1;
   lp = (struct f_link *)bmalloc(sizeof(struct f_link) + len);
   lp->digest = NULL;                /* set later */
   lp->digest_stream = 0;            /* set later */
   lp->digest_len = 0;               /* set later */
   lp->ino = ff_pkt->statp.st_ino;
   lp->dev = ff_pkt->statp.st_dev;
   lp->FileIndex = 0;                /* set later */
   bstrncpy(lp->name, fname, len);
   lp->next = ff_pkt->linkhash[linkhash];
   ff_pkt->linkhash[linkhash] = lp;
   ff_pkt->linked = lp;              /* mark saved link */
   Dmsg2(400, "added to hash FI=%d file=%s\n", ff_pkt->FileIndex, lp->name);
   *done = false;
   return rtn_stat;
}

/*
 * Handling of a regular file.
 */
static inline int process_regular_file(JCR *jcr, FF_PKT *ff_pkt,
                                       int handle_file(JCR *jcr, FF_PKT *ff, bool top_level),
                                       char *fname, bool top_level,
                                       struct utimbuf *restore_times)
{
   int rtn_stat;
   boffset_t sizeleft;

   sizeleft = ff_pkt->statp.st_size;

   /*
    * Don't bother opening empty, world readable files.  Also do not open
    * files when archive is meant for /dev/null.
    */
   if (ff_pkt->null_output_device || (sizeleft == 0 &&
       MODE_RALL == (MODE_RALL & ff_pkt->statp.st_mode))) {
      ff_pkt->type = FT_REGE;
   } else {
      ff_pkt->type = FT_REG;
   }
   rtn_stat = handle_file(jcr, ff_pkt, top_level);
   if (ff_pkt->linked) {
      ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
   }
   Dmsg3(400, "FT_REG FI=%d linked=%d file=%s\n",
         ff_pkt->FileIndex, ff_pkt->linked ? 1 : 0, fname);
   if (ff_pkt->flags & FO_KEEPATIME) {
      utime(fname, restore_times);
   }
   return rtn_stat;
}

/*
 * Handling of a symlink.
 */
static inline int process_symlink(JCR *jcr, FF_PKT *ff_pkt,
                                  int handle_file(JCR *jcr, FF_PKT *ff, bool top_level),
                                  char *fname, bool top_level)
{
   int rtn_stat;
   int size;
   char *buffer = (char *)alloca(path_max + name_max + 102);

   size = readlink(fname, buffer, path_max + name_max + 101);
   if (size < 0) {
      /*
       * Could not follow link
       */
      ff_pkt->type = FT_NOFOLLOW;
      ff_pkt->ff_errno = errno;
      rtn_stat = handle_file(jcr, ff_pkt, top_level);
      if (ff_pkt->linked) {
         ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
      }
      return rtn_stat;
   }
   buffer[size] = 0;
   ff_pkt->link = buffer;          /* point to link */
   ff_pkt->type = FT_LNK;          /* got a real link */
   rtn_stat = handle_file(jcr, ff_pkt, top_level);
   if (ff_pkt->linked) {
      ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
   }
   return rtn_stat;
}

/*
 * Handling of a directory.
 */
static inline int process_directory(JCR *jcr, FF_PKT *ff_pkt,
                                    int handle_file(JCR *jcr, FF_PKT *ff, bool top_level),
                                    char *fname, dev_t parent_device, bool top_level,
                                    struct utimbuf *restore_times)
{
   int rtn_stat;
   DIR *directory;
   struct dirent *entry, *result;
   char *link;
   int link_len;
   int len;
   int status;
   dev_t our_device = ff_pkt->statp.st_dev;
   bool recurse = true;
   bool volhas_attrlist = ff_pkt->volhas_attrlist;    /* Remember this if we recurse */

   /*
    * Ignore this directory and everything below if the file .nobackup
    * (or what is defined for IgnoreDir in this fileset) exists
    */
   if (have_ignoredir(ff_pkt)) {
      return 1; /* Just ignore this directory */
   }

   /*
    * Build a canonical directory name with a trailing slash in link var
    */
   len = strlen(fname);
   link_len = len + 200;
   link = (char *)bmalloc(link_len + 2);
   bstrncpy(link, fname, link_len);

   /*
    * Strip all trailing slashes
    */
   while (len >= 1 && IsPathSeparator(link[len - 1])) {
     len--;
   }
   link[len++] = '/';             /* add back one */
   link[len] = 0;

   ff_pkt->link = link;
   if (!check_changes(jcr, ff_pkt)) {
      /*
       * Incremental/Full+Base option, directory entry not changed
       */
      ff_pkt->type = FT_DIRNOCHG;
   } else {
      ff_pkt->type = FT_DIRBEGIN;
   }
#if defined(HAVE_WIN32)
   /*
    * We have set st_rdev to 1 if it is a reparse point, otherwise 0,
    * if st_rdev is 2, it is a mount point
    *
    * A reparse point (WIN32_REPARSE_POINT)
    * is something special like one of the following:
    *  IO_REPARSE_TAG_DFS              0x8000000A
    *  IO_REPARSE_TAG_DFSR             0x80000012
    *  IO_REPARSE_TAG_HSM              0xC0000004
    *  IO_REPARSE_TAG_HSM2             0x80000006
    *  IO_REPARSE_TAG_SIS              0x80000007
    *  IO_REPARSE_TAG_SYMLINK          0xA000000C
    *
    * A junction point is a:
    *  IO_REPARSE_TAG_MOUNT_POINT      0xA0000003
    * which can be either a link to a Volume (WIN32_MOUNT_POINT)
    * or a link to a directory (WIN32_JUNCTION_POINT)
    *
    * Ignore WIN32_REPARSE_POINT and WIN32_JUNCTION_POINT
    */
   if (ff_pkt->statp.st_rdev == WIN32_REPARSE_POINT) {
      ff_pkt->type = FT_REPARSE;
   } else if (ff_pkt->statp.st_rdev == WIN32_JUNCTION_POINT) {
      ff_pkt->type = FT_JUNCTION;
   }
#endif
   /*
    * Note, we return the directory to the calling program (handle_file)
    * when we first see the directory (FT_DIRBEGIN.
    * This allows the program to apply matches and make a
    * choice whether or not to accept it.  If it is accepted, we
    * do not immediately save it, but do so only after everything
    * in the directory is seen (i.e. the FT_DIREND).
    */
   rtn_stat = handle_file(jcr, ff_pkt, top_level);
   if (rtn_stat < 1 || ff_pkt->type == FT_REPARSE ||
       ff_pkt->type == FT_JUNCTION) {   /* ignore or error status */
      free(link);
      return rtn_stat;
   }
   /*
    * Done with DIRBEGIN, next call will be DIREND
    */
   if (ff_pkt->type == FT_DIRBEGIN) {
      ff_pkt->type = FT_DIREND;
   }

   /*
    * Create a temporary ff packet for this directory
    * entry, and defer handling the directory until
    * we have recursed into it.  This saves the
    * directory after all files have been processed, and
    * during the restore, the directory permissions will
    * be reset after all the files have been restored.
    */
   Dmsg1(300, "Create temp ff packet for dir: %s\n", ff_pkt->fname);
   FF_PKT *dir_ff_pkt = new_dir_ff_pkt(ff_pkt);

   /*
    * Do not descend into subdirectories (recurse) if the
    * user has turned it off for this directory.
    *
    * If we are crossing file systems, we are either not allowed
    * to cross, or we may be restricted by a list of permitted
    * file systems.
    */
   bool is_win32_mount_point = false;
#if defined(HAVE_WIN32)
   is_win32_mount_point = ff_pkt->statp.st_rdev == WIN32_MOUNT_POINT;
#endif
   if (!top_level && ff_pkt->flags & FO_NO_RECURSION) {
      ff_pkt->type = FT_NORECURSE;
      recurse = false;
   } else if (!top_level && (parent_device != ff_pkt->statp.st_dev ||
              is_win32_mount_point)) {
      if(!(ff_pkt->flags & FO_MULTIFS)) {
         ff_pkt->type = FT_NOFSCHG;
         recurse = false;
      } else if (!accept_fstype(ff_pkt, NULL)) {
         ff_pkt->type = FT_INVALIDFS;
         recurse = false;
      } else {
         ff_pkt->volhas_attrlist = volume_has_attrlist(fname);
      }
   }
   /*
    * If not recursing, just backup dir and return
    */
   if (!recurse) {
      rtn_stat = handle_file(jcr, ff_pkt, top_level);
      if (ff_pkt->linked) {
         ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
      }
      free(link);
      free_dir_ff_pkt(dir_ff_pkt);
      ff_pkt->link = ff_pkt->fname;     /* reset "link" */
      if (ff_pkt->flags & FO_KEEPATIME) {
         utime(fname, restore_times);
      }
      return rtn_stat;
   }

   ff_pkt->link = ff_pkt->fname;     /* reset "link" */

   /*
    * Descend into or "recurse" into the directory to read all the files in it.
    */
   errno = 0;
   if ((directory = opendir(fname)) == NULL) {
      ff_pkt->type = FT_NOOPEN;
      ff_pkt->ff_errno = errno;
      rtn_stat = handle_file(jcr, ff_pkt, top_level);
      if (ff_pkt->linked) {
         ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
      }
      free(link);
      free_dir_ff_pkt(dir_ff_pkt);
      return rtn_stat;
   }

   /*
    * Process all files in this directory entry (recursing).
    * This would possibly run faster if we chdir to the directory
    * before traversing it.
    */
   rtn_stat = 1;

   /*
    * Allocate some extra room so an overflow of the d_name with more then
    * name_max bytes doesn't kill us right away. We check in the loop if
    * an overflow has not happened.
    */
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 100);
   while (!job_canceled(jcr)) {
      int name_length;

      status = readdir_r(directory, entry, &result);
      if (status != 0 || result == NULL) {
//          Dmsg2(99, "readdir returned stat=%d result=0x%x\n",
//             status, (long)result);
         break;
      }

      name_length = (int)NAMELEN(entry);

      /*
       * Some filesystems violate against the rules and return filenames
       * longer then _PC_NAME_MAX. Log the error and continue.
       */
      if ((name_max + 1) <= ((int)sizeof(struct dirent) + name_length)) {
         Jmsg2(jcr, M_ERROR, 0, _("%s: File name too long [%d]\n"), entry->d_name, name_length);
         continue;
      }

      /*
       * Skip `.', `..', and excluded file names.
       */
      if (entry->d_name[0] == '\0' ||
         (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' ||
         (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))) {
         continue;
      }

      /*
       * Make sure there is enough room to store the whole name.
       */
      if (name_length + len >= link_len) {
         link_len = len + name_length + 1;
         link = (char *)brealloc(link, link_len + 1);
      }

      memcpy(link + len, entry->d_name, name_length);
      link[len + name_length] = '\0';

      if (!file_is_excluded(ff_pkt, link)) {
         rtn_stat = find_one_file(jcr, ff_pkt, handle_file, link, our_device, false);
         if (ff_pkt->linked) {
            ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
         }
      }
   }

   closedir(directory);
   free(link);
   free(entry);

   /*
    * Now that we have recursed through all the files in the
    * directory, we "save" the directory so that after all
    * the files are restored, this entry will serve to reset
    * the directory modes and dates.  Temp directory values
    * were used without this record.
    */
   handle_file(jcr, dir_ff_pkt, top_level);       /* handle directory entry */
   if (ff_pkt->linked) {
      ff_pkt->linked->FileIndex = dir_ff_pkt->FileIndex;
   }
   free_dir_ff_pkt(dir_ff_pkt);

   if (ff_pkt->flags & FO_KEEPATIME) {
      utime(fname, restore_times);
   }
   ff_pkt->volhas_attrlist = volhas_attrlist;      /* Restore value in case it changed. */
   return rtn_stat;
}

/*
 * Handling of a special file.
 */
static inline int process_special_file(JCR *jcr, FF_PKT *ff_pkt,
                                       int handle_file(JCR *jcr, FF_PKT *ff, bool top_level),
                                       char *fname, bool top_level)
{
   int rtn_stat;

   /*
    * If it is explicitly mentioned (i.e. top_level) and is
    * a block device, we do a raw backup of it or if it is
    * a fifo, we simply read it.
    */
#ifdef HAVE_FREEBSD_OS
   /*
    * On FreeBSD, all block devices are character devices, so
    * to be able to read a raw disk, we need the check for
    * a character device.
    *
    * crw-r----- 1 root  operator - 116, 0x00040002 Jun 9 19:32 /dev/ad0s3
    * crw-r----- 1 root  operator - 116, 0x00040002 Jun 9 19:32 /dev/rad0s3
    */
   if (top_level && (S_ISBLK(ff_pkt->statp.st_mode) || S_ISCHR(ff_pkt->statp.st_mode))) {
#else
   if (top_level && S_ISBLK(ff_pkt->statp.st_mode)) {
#endif
      ff_pkt->type = FT_RAW;          /* raw partition */
   } else if (top_level && S_ISFIFO(ff_pkt->statp.st_mode) &&
              ff_pkt->flags & FO_READFIFO) {
      ff_pkt->type = FT_FIFO;
   } else {
      /*
       * The only remaining types are special (character, ...) files
       */
      ff_pkt->type = FT_SPEC;
   }
   rtn_stat = handle_file(jcr, ff_pkt, top_level);
   if (ff_pkt->linked) {
     ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
   }
   return rtn_stat;
}

/*
 * See if we need to perform any processing for a given file.
 */
static inline int needs_processing(JCR *jcr, FF_PKT *ff_pkt,
                                   char *fname,
                                   struct utimbuf *restore_times)
{
   if (!accept_fstype(ff_pkt, NULL)) {
      ff_pkt->type = FT_INVALIDFS;
      if (ff_pkt->flags & FO_KEEPATIME) {
         utime(fname, restore_times);
      }

      char fs[100];

      if (!fstype(ff_pkt->fname, fs, sizeof(fs))) {
          bstrncpy(fs, "unknown", sizeof(fs));
      }

      Jmsg(jcr, M_INFO, 0, _("Top level directory \"%s\" has unlisted fstype \"%s\"\n"), fname, fs);
      return 1;      /* Just ignore this error - or the whole backup is cancelled */
   }
   if (!accept_drivetype(ff_pkt, NULL)) {
      ff_pkt->type = FT_INVALIDDT;
      if (ff_pkt->flags & FO_KEEPATIME) {
         utime(fname, restore_times);
      }

      char dt[100];

      if (!drivetype(ff_pkt->fname, dt, sizeof(dt))) {
          bstrncpy(dt, "unknown", sizeof(dt));
      }

      Jmsg(jcr, M_INFO, 0, _("Top level directory \"%s\" has an unlisted drive type \"%s\"\n"), fname, dt);
      return 1;      /* Just ignore this error - or the whole backup is cancelled */
   }
   ff_pkt->volhas_attrlist = volume_has_attrlist(fname);

   return 0;
}

/*
 * Find a single file.
 *
 * handle_file is the callback for handling the file.
 *    p is the filename
 *    parent_device is the device we are currently on
 *    top_level is 1 when not recursing or 0 when descending into a directory.
 */
int find_one_file(JCR *jcr, FF_PKT *ff_pkt,
                  int handle_file(JCR *jcr, FF_PKT *ff, bool top_level),
                  char *fname, dev_t parent_device, bool top_level)
{
   int rtn_stat;
   bool done = false;
   struct utimbuf restore_times;

   ff_pkt->fname = ff_pkt->link = fname;
   if (lstat(fname, &ff_pkt->statp) != 0) {
       /*
        * Cannot stat file
        */
       ff_pkt->type = FT_NOSTAT;
       ff_pkt->ff_errno = errno;
       return handle_file(jcr, ff_pkt, top_level);
   }

   Dmsg1(300, "File ----: %s\n", fname);

   /*
    * Save current times of this directory in case we need to
    * reset them because the user doesn't want them changed.
    */
   restore_times.actime = ff_pkt->statp.st_atime;
   restore_times.modtime = ff_pkt->statp.st_mtime;

   /*
    * We check for allowed fstypes and drivetypes at top_level and fstype change (below).
    */
   if (top_level) {
      if (needs_processing(jcr, ff_pkt, fname, &restore_times) == 1)
         return 1;
   }

   /*
    * Ignore this entry if no_dump() returns true
    */
   if (no_dump(jcr, ff_pkt)) {
      Dmsg1(100, "'%s' ignored (NODUMP flag set)\n", ff_pkt->fname);
      return 1;
   }

   switch (ff_pkt->statp.st_mode & S_IFMT) {
   case S_IFDIR:
      break;
   case S_IFREG:
      if (!check_size_matching(jcr, ff_pkt)) {
         Dmsg1(100, "'%s' ignored (Size doesn't match\n", ff_pkt->fname);
         return 1;
      }
      /*
       * Fall Through
       */
   default:
      /*
       * If this is an Incremental backup, see if file was modified
       * since our last "save_time", presumably the last Full save
       * or Incremental.
       */
      if (!check_changes(jcr, ff_pkt)) {
         Dmsg1(500, "Non-directory incremental: %s\n", ff_pkt->fname);
         ff_pkt->type = FT_NOCHG;
         return handle_file(jcr, ff_pkt, top_level);
      }
      break;
   }

#ifdef HAVE_DARWIN_OS
   if (ff_pkt->flags & FO_HFSPLUS &&
       ff_pkt->volhas_attrlist &&
       S_ISREG(ff_pkt->statp.st_mode)) {
      rtn_stat = process_hfsattributes(jcr, ff_pkt, handle_file, fname, top_level);
      if (rtn_stat != -1) {
         return rtn_stat;
      }
   }
#endif

   ff_pkt->LinkFI = 0;
   /*
    * Handle hard linked files
    *
    * Maintain a list of hard linked files already backed up. This
    * allows us to ensure that the data of each file gets backed
    * up only once.
    */
   if (!(ff_pkt->flags & FO_NO_HARDLINK) && ff_pkt->statp.st_nlink > 1) {
      switch (ff_pkt->statp.st_mode & S_IFMT) {
      case S_IFREG:
      case S_IFCHR:
      case S_IFBLK:
      case S_IFIFO:
#ifdef S_IFSOCK
      case S_IFSOCK:
#endif
         /*
          * Via the done variable the process_hardlink function returns
          * if file processing is done. If done is set to false we continue
          * with the normal processing of the file.
          */
         rtn_stat = process_hardlink(jcr, ff_pkt, handle_file, fname, top_level, &done);
         if (done) {
            return rtn_stat;
         }
         break;
      default:
         ff_pkt->linked = NULL;
         break;
      }
   } else {
      ff_pkt->linked = NULL;
   }

   /*
    * Based on the type of file call the correct function.
    * This is not a link to a previously dumped file, so dump it.
    */
   switch (ff_pkt->statp.st_mode & S_IFMT) {
   case S_IFREG:
      return process_regular_file(jcr, ff_pkt, handle_file, fname, top_level, &restore_times);
#ifdef S_IFLNK
   case S_IFLNK:
      return process_symlink(jcr, ff_pkt, handle_file, fname, top_level);
#endif
   case S_IFDIR:
      return process_directory(jcr, ff_pkt, handle_file, fname, parent_device, top_level, &restore_times);
   default:
      return process_special_file(jcr, ff_pkt, handle_file, fname, top_level);
   }
}

int term_find_one(FF_PKT *ff)
{
   struct f_link *lp, *lc;
   int count = 0;
   int i;

   if (ff->linkhash == NULL) {
      return 0;
   }

   /*
    * Free up list of hard linked files
    */
   for (i = 0; i < LINK_HASHTABLE_SIZE; i ++) {
      lp = ff->linkhash[i];
      while (lp) {
         lc = lp;
         lp = lp->next;
         if (lc) {
            if (lc->digest) {
               free(lc->digest);
            }
            free(lc);
            count++;
         }
      }
      ff->linkhash[i] = NULL;
   }
   free(ff->linkhash);
   ff->linkhash = NULL;
   return count;
}
