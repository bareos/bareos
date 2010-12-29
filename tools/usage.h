/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __USAGE_H__
#define __USAGE_H__

struct usage_def
{
#define USAGE_NO_OPT 0x7f
  char opt;
#define USAGE_PARAM (1u<<0)
#define USAGE_MANDAT (1u<<1)
  unsigned int flags;
  char *param;
  char *long_descr;
};

struct cmd_def
{
  char *name;
  char *purpose;
  struct usage_def *defs;
  int	(*func)(int argc, char **argv);
};

/* PROTO usage.c */
/* ./usage.c */
char *usage_getoptstr(struct usage_def *defs);
void usage_help(struct cmd_def *cmd);
#endif
