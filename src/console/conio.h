/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2006 Free Software Foundation Europe e.V.

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
