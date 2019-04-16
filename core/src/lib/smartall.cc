/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
                         S M A R T A L L O C
                        Smart Memory Allocator

        Evolved   over   several  years,  starting  with  the  initial
        SMARTALLOC code for AutoSketch in 1986, guided  by  the  Blind
        Watchbreaker,  John  Walker.  Isolated in this general-purpose
        form in  September  of  1989.   Updated  with  be  more  POSIX
        compliant  and  to  include Web-friendly HTML documentation in
        October  of  1998  by  the  same  culprit.    For   additional
        information and the current version visit the Web page:

                  http://www.fourmilab.ch/smartall/
*/

#include "lib/smartall.h"

uint64_t sm_max_bytes = 0;
uint64_t sm_bytes = 0;
uint32_t sm_max_buffers = 0;
uint32_t sm_buffers = 0;
