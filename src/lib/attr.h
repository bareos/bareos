/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2010 Free Software Foundation Europe e.V.

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
 *   attr.h Definition of attributes packet for unpacking from tape
 *
 *    Kern Sibbald, June MMIII
 *
 */

#ifndef __ATTR_H_
#define __ATTR_H_ 1


struct ATTR {
   int32_t stream;                    /* attribute stream id */
   int32_t data_stream;               /* id of data stream to follow */
   int32_t type;                      /* file type FT */
   int32_t file_index;                /* file index */
   int32_t LinkFI;                    /* file index to data if hard link */
   int32_t delta_seq;                 /* delta sequence numbr */
   uid_t uid;                         /* userid */
   struct stat statp;                 /* decoded stat packet */
   POOLMEM *attrEx;                   /* extended attributes if any */
   POOLMEM *ofname;                   /* output filename */
   POOLMEM *olname;                   /* output link name */
   /*
    * Note the following three variables point into the
    *  current BSOCK record, so they are invalid after
    *  the next socket read!
    */
   char *attr;                        /* attributes position */
   char *fname;                       /* filename */
   char *lname;                       /* link name if any */
   JCR *jcr;                          /* jcr pointer */
};

#endif /* __ATTR_H_ */
