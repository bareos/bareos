/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2006 Free Software Foundation Europe e.V.

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

#ifndef __CONIO_H
#define __CONIO_H
extern int  input_line(char *line, int len);
extern void con_init(FILE *input);

extern "C" {
extern void con_term();
}

extern void con_set_zed_keys();
extern void t_sendl(char *buf, int len);
extern void t_send(char *buf);
extern void t_char(char c);
extern int  usrbrk(void);
extern void clrbrk(void);
extern void trapctlc(void);
#endif
