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

#ifndef __CMDS_H__
#define __CMDS_H__	1

extern struct cmd_def cd_cmd;
extern struct cmd_def lcd_cmd;
extern struct cmd_def la_cmd;
extern struct cmd_def put_cmd;
extern struct cmd_def get_cmd;
extern struct cmd_def rm_cmd;
extern struct cmd_def rb_cmd;
extern struct cmd_def getattr_cmd;
extern struct cmd_def mb_cmd;
extern struct cmd_def ls_cmd;
extern struct cmd_def mkdir_cmd;
extern struct cmd_def pwd_cmd;
extern struct cmd_def rmdir_cmd;
extern struct cmd_def set_cmd;
extern struct cmd_def unset_cmd;

extern struct cmd_def	*cmd_defs[];

/* PROTO cmds.c */
/* ./cmds.c */
int cmd_quit(int argc, char **argv);
int cmd_help(int argc, char **argv);
#endif
