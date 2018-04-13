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

#ifndef __ATTR_H_
#define __ATTR_H_ 1


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

#endif /* __ATTR_H_ */
