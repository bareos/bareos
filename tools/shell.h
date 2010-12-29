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

#ifndef __SHELL_H__
#define __SHELL_H__	1

#include "usage.h"

#define SHELL_MAX_ARG_LEN	128
#define SHELL_MAX_ARGV		16

enum shell_error
  {
    SHELL_ERROR_NONE,
    SHELL_ERROR_TOO_MANY_ARGS,
    SHELL_ERROR_ARG_TOO_LONG,
    SHELL_ERROR_DBL_QUOTE,
  };

#define SHELL_CONT	0
#define SHELL_EPARSE	-1
#define SHELL_RETURN	-2

/* PROTO shell.c */
/* ./shell.c */
const char *shell_error_str(enum shell_error err);
void shell_install_cmd_defs(struct cmd_def **defs);
char **shell_completion(const char *text, int start, int end);
int shell_do_cmd(struct cmd_def **defs, int argc, char **argv);
int shell_parse(struct cmd_def **defs, char *str, enum shell_error *errp);
void shell_do(struct cmd_def **defs);
#endif
