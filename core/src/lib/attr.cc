/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, June MMIII  (code pulled from filed/restore.c and updated)
 */
/**
 * @file
 * attr.c  Unpack an Attribute record returned from the tape
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/breg.h"
#include "lib/edit.h"
#include "lib/util.h"

static const int debuglevel = 150;

Attributes *new_attr(JobControlRecord *jcr)
{
   Attributes *attr = (Attributes *)malloc(sizeof(Attributes));
   memset(attr, 0, sizeof(Attributes));
   attr->ofname = GetPoolMemory(PM_FNAME);
   attr->olname = GetPoolMemory(PM_FNAME);
   attr->attrEx = GetPoolMemory(PM_FNAME);
   attr->jcr = jcr;
   attr->uid = getuid();
   return attr;
}

void FreeAttr(Attributes *attr)
{
   FreePoolMemory(attr->olname);
   FreePoolMemory(attr->ofname);
   FreePoolMemory(attr->attrEx);
   free(attr);
}

int UnpackAttributesRecord(JobControlRecord *jcr, int32_t stream, char *rec, int32_t reclen, Attributes *attr)
{
   char *p;
   int object_len;
   /*
    * An Attributes record consists of:
    *    File_index
    *    Type   (FT_types)
    *    Filename
    *    Attributes
    *    Link name (if file linked i.e. FT_LNK)
    *    Extended attributes (Win32)
    *  plus optional values determined by AR_ flags in upper bits of Type
    *    Data_stream
    *
    */
   attr->stream = stream;
   Dmsg1(debuglevel, "Attr: %s\n", rec);
   if (sscanf(rec, "%d %d", &attr->file_index, &attr->type) != 2) {
      Jmsg(jcr, M_FATAL, 0, _("Error scanning attributes: %s\n"), rec);
      Dmsg1(debuglevel, "\nError scanning attributes. %s\n", rec);
      return 0;
   }
   Dmsg2(debuglevel, "Got Attr: FilInx=%d type=%d\n", attr->file_index, attr->type);
   /*
    * Note AR_DATA_STREAM should never be set since it is encoded
    *  at the end of the attributes.
    */
   if (attr->type & AR_DATA_STREAM) {
      attr->data_stream = 1;
   } else {
      attr->data_stream = 0;
   }
   attr->type &= FT_MASK;             /* keep only type bits */
   p = rec;
   while (*p++ != ' ')               /* skip record file index */
      { }
   while (*p++ != ' ')               /* skip type */
      { }

   attr->fname = p;                   /* set filname position */
   while (*p++ != 0)                  /* skip filename */
      { }
   attr->attr = p;                    /* set attributes position */
   while (*p++ != 0)                  /* skip attributes */
      { }
   attr->lname = p;                   /* set link position */
   while (*p++ != 0)                  /* skip link */
      { }
   attr->delta_seq = 0;
   if (attr->type == FT_RESTORE_FIRST) {
      /* We have an object, so do a binary copy */
      object_len = reclen + rec - p;
      attr->attrEx = CheckPoolMemorySize(attr->attrEx, object_len + 1);
      memcpy(attr->attrEx, p, object_len);
      /* Add a EOS for those who attempt to print the object */
      p = attr->attrEx + object_len;
      *p = 0;
   } else {
      PmStrcpy(attr->attrEx, p);     /* copy extended attributes, if any */
      if (attr->data_stream) {
         int64_t val;
         while (*p++ != 0)            /* skip extended attributes */
            { }
         from_base64(&val, p);
         attr->data_stream = (int32_t)val;
      } else {
         while (*p++ != 0)            /* skip extended attributes */
            { }
         if (p - rec < reclen) {
            attr->delta_seq = str_to_int32(p); /* delta_seq */
         }
      }
   }
   Dmsg8(debuglevel, "unpack_attr FI=%d Type=%d fname=%s attr=%s lname=%s attrEx=%s datastr=%d delta_seq=%d\n",
      attr->file_index, attr->type, attr->fname, attr->attr, attr->lname,
      attr->attrEx, attr->data_stream, attr->delta_seq);
   *attr->ofname = 0;
   *attr->olname = 0;
   return 1;
}

#if defined(HAVE_WIN32)
static void strip_double_slashes(char *fname)
{
   char *p = fname;
   while (p && *p) {
      p = strpbrk(p, "/\\");
      if (p != NULL) {
         if (IsPathSeparator(p[1])) {
            strcpy(p, p+1);
         }
         p++;
      }
   }
}
#endif

/**
 * Build attr->ofname from attr->fname and
 *       attr->olname from attr->olname
 */
void BuildAttrOutputFnames(JobControlRecord *jcr, Attributes *attr)
{
   /*
    * Prepend the where directory so that the
    * files are put where the user wants.
    *
    * We do a little jig here to handle Win32 files with
    *   a drive letter -- we simply change the drive
    *   from, for example, c: to c/ for
    *   every filename if a prefix is supplied.
    *
    */

   if (jcr->where_bregexp) {
      char *ret;
      ApplyBregexps(attr->fname, jcr->where_bregexp, &ret);
      PmStrcpy(attr->ofname, ret);

      if (attr->type == FT_LNKSAVED || attr->type == FT_LNK) {
         /* Always add prefix to hard links (FT_LNKSAVED) and
          *  on user request to soft links
          */

         if ((attr->type == FT_LNKSAVED || jcr->prefix_links)) {
            ApplyBregexps(attr->lname, jcr->where_bregexp, &ret);
            PmStrcpy(attr->olname, ret);

         } else {
            PmStrcpy(attr->olname, attr->lname);
         }
      }

   } else if (jcr->where[0] == 0) {
      PmStrcpy(attr->ofname, attr->fname);
      PmStrcpy(attr->olname, attr->lname);

   } else {
      const char *fn;
      int wherelen = strlen(jcr->where);
      PmStrcpy(attr->ofname, jcr->where);  /* copy prefix */
#if defined(HAVE_WIN32)
      if (attr->fname[1] == ':') {
         attr->fname[1] = '/';     /* convert : to / */
      }
#endif
      fn = attr->fname;            /* take whole name */
      /* Ensure where is terminated with a slash */
      if (!IsPathSeparator(jcr->where[wherelen-1]) && !IsPathSeparator(fn[0])) {
         PmStrcat(attr->ofname, "/");
      }
      PmStrcat(attr->ofname, fn); /* copy rest of name */
      /*
       * Fixup link name -- if it is an absolute path
       */
      if (attr->type == FT_LNKSAVED || attr->type == FT_LNK) {
         bool add_link;
         /* Always add prefix to hard links (FT_LNKSAVED) and
          *  on user request to soft links
          */
         if (IsPathSeparator(attr->lname[0]) &&
             (attr->type == FT_LNKSAVED || jcr->prefix_links)) {
            PmStrcpy(attr->olname, jcr->where);
            add_link = true;
         } else {
            attr->olname[0] = 0;
            add_link = false;
         }
         fn = attr->lname;       /* take whole name */
         /* Ensure where is terminated with a slash */
         if (add_link &&
            !IsPathSeparator(jcr->where[wherelen-1]) &&
            !IsPathSeparator(fn[0])) {
            PmStrcat(attr->olname, "/");
         }
         PmStrcat(attr->olname, fn);     /* copy rest of link */
      }
   }
#if defined(HAVE_WIN32)
   strip_double_slashes(attr->ofname);
   strip_double_slashes(attr->olname);
#endif
}

extern char *getuser(uid_t uid, char *name, int len);
extern char *getgroup(gid_t gid, char *name, int len);

static const char *attr_stat_to_str(PoolMem &resultbuffer, JobControlRecord *jcr, Attributes *attr)
{
   char buf[5000];
   char ec1[30];
   char en1[30], en2[30];
   char *p;
   guid_list *guid;

   if (attr->type == FT_DELETED) { /* TODO: change this to get last seen values */
      resultbuffer.strcat("----------   - -        -                - ---------- --------");
      return resultbuffer.c_str();
   }

   if (!jcr->id_list) {
      jcr->id_list = new_guid_list();
   }
   guid = jcr->id_list;
   p = encode_mode(attr->statp.st_mode, buf);
   p += sprintf(p, "  %2d ", (uint32_t)attr->statp.st_nlink);
   p += sprintf(p, "%-8.8s %-8.8s",
                guid->uid_to_name(attr->statp.st_uid, en1, sizeof(en1)),
                guid->gid_to_name(attr->statp.st_gid, en2, sizeof(en2)));
   p += sprintf(p, "%12.12s ", edit_int64(attr->statp.st_size, ec1));
   p = encode_time(attr->statp.st_ctime, p);

   resultbuffer.strcat(buf);
   return resultbuffer.c_str();
}

static const char *attr_file_to_str(PoolMem &resultbuffer, Attributes *attr)
{
   resultbuffer.strcat(attr->ofname);
   if (attr->type == FT_LNK) {
      resultbuffer.strcat(" -> ");
      resultbuffer.strcat(attr->olname);
   }
   return resultbuffer.c_str();
}


const char *attr_to_str(PoolMem &resultbuffer, JobControlRecord *jcr, Attributes *attr)
{
   attr_stat_to_str(resultbuffer, jcr, attr);
   resultbuffer.strcat("\n");
   attr_file_to_str(resultbuffer, attr);

   return resultbuffer.c_str();
}


static const char *attr_to_ls_output(PoolMem &resultbuffer, JobControlRecord *jcr, Attributes *attr)
{
   attr_stat_to_str(resultbuffer, jcr, attr);
   resultbuffer.strcat("  ");
   attr_file_to_str(resultbuffer, attr);
   resultbuffer.strcat("\n");

   return resultbuffer.c_str();
}

/**
 * Print an ls style message, also send M_RESTORED
 */
void PrintLsOutput(JobControlRecord *jcr, Attributes *attr)
{
   PoolMem resultbuffer(PM_MESSAGE);
   attr_to_ls_output(resultbuffer, jcr, attr);
   Dmsg1(debuglevel, "%s", resultbuffer.c_str());
   Jmsg(jcr, M_RESTORED, 1, "%s", resultbuffer.c_str());
}
