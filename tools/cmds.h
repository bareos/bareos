/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
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

extern tcmd_def	cmd_defs[];

/* PROTO cmds.c cmd_cd.c cmd_lcd.c cmd_pwd.c cmd_set.c cmd_unset.c cmd_ls.c cmd_mkdir.c cmd_rmdir.c */
/* ./cmds.c */
int cmd_quit(void *cb_arg, int argc, char **argv);
int cmd_echo(void *cb_arg, int argc, char **argv);
int cmd_status(void *cb_arg, int argc, char **argv);
int cmd_help(void *cb_arg, int argc, char **argv);
int do_cmd(void *cb_arg, int argc, char **argv);
/* ./cmd_cd.c */
int cmd_cd(void *cb_arg, int argc, char **argv);
/* ./cmd_lcd.c */
int cmd_lcd(void *cb_arg, int argc, char **argv);
/* ./cmd_pwd.c */
void cmd_pwd_usage(void);
int cmd_pwd(void *cb_arg, int argc, char **argv);
/* ./cmd_set.c */
void set_usage(void);
void var_print_cb(tvar *var, void *cb_arg);
int cmd_set(void *cb_arg, int argc, char **argv);
/* ./cmd_unset.c */
void unset_usage(void);
int cmd_unset(void *cb_arg, int argc, char **argv);
/* ./cmd_ls.c */
void cmd_ls_usage(void);
void do_ls_obj(dpl_dirent_t *entry, u_int flags);
int cmd_ls(void *cb_arg, int argc, char **argv);
/* ./cmd_mkdir.c */
void cmd_mkdir_usage(void);
int cmd_mkdir(void *cb_arg, int argc, char **argv);
/* ./cmd_rmdir.c */
void cmd_rmdir_usage(void);
int cmd_rmdir(void *cb_arg, int argc, char **argv);
#endif
