/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2010 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, June MMIII
 */
/**
 * @file
 * Definition of attributes packet for unpacking from tape
 */

#ifndef BAREOS_LIB_ATTR_H_
#define BAREOS_LIB_ATTR_H_ 1


struct Attributes {
   int32_t stream;                    /**< attribute stream id */
   int32_t data_stream;               /**< id of data stream to follow */
   int32_t type;                      /**< file type FT */
   int32_t file_index;                /**< file index */
   int32_t LinkFI;                    /**< file index to data if hard link */
   int32_t delta_seq;                 /**< delta sequence numbr */
   uid_t uid;                         /**< userid */
   struct stat statp;                 /**< decoded stat packet */
   POOLMEM *attrEx;                   /**< extended attributes if any */
   POOLMEM *ofname;                   /**< output filename */
   POOLMEM *olname;                   /**< output link name */
   /*
    * Note the following three variables point into the
    *  current BareosSocket record, so they are invalid after
    *  the next socket read!
    */
   char *attr;                        /**< attributes position */
   char *fname;                       /**< filename */
   char *lname;                       /**< link name if any */
   JobControlRecord *jcr;                          /**< jcr pointer */
};

DLL_IMP_EXP Attributes *new_attr(JobControlRecord *jcr);
DLL_IMP_EXP void free_attr(Attributes *attr);
DLL_IMP_EXP int unpack_attributes_record(JobControlRecord *jcr, int32_t stream, char *rec, int32_t reclen, Attributes *attr);
DLL_IMP_EXP void build_attr_output_fnames(JobControlRecord *jcr, Attributes *attr);
DLL_IMP_EXP const char *attr_to_str(PoolMem &resultbuffer, JobControlRecord *jcr, Attributes *attr);
DLL_IMP_EXP void print_ls_output(JobControlRecord *jcr, Attributes *attr);

#endif /* BAREOS_LIB_ATTR_H_ */
