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

#include "dpl_sh.h"

int
cmd_quit(void *cb_arg,
	 int argc,
	 char **argv)
{
  do_quit();

  return SHELL_RETURN;
}

int
cmd_echo(void *cb_arg,
         int argc,
         char **argv)
{
  int i;
  
  for (i = 1;i < argc;i++)
    printf("%s ", argv[i]);
  printf("\n");

  return SHELL_CONT;
}

int
cmd_status(void *cb_arg,
           int argc,
           char **argv)
{
  printf("%d\n", status);

  return SHELL_CONT;
}

int 
cmd_help(void *cb_arg,
	 int argc,
	 char **argv)
{
  if (argc == 1)
    {
      tcmd_def	*cmdp;
      int i;

      i = 0;
      for (cmdp = cmd_defs;cmdp->name;cmdp++)
	{
	  printf("%16s", cmdp->name);
	  i++;
	  if (i == 4)
	    {
	      printf("\n");
	      i = 0;
	    }
	}
      printf("\n");
    }
  else if (argc == 2)
    {
      tcmd_def	*cmdp;

      for (cmdp = cmd_defs;cmdp->name;cmdp++)
	{
	  if (!strcmp(argv[1], cmdp->name))
	    {
	      printf("%s\n", cmdp->help);
	      break ;
	    }
	}
    }

  return SHELL_CONT;
}

int
do_cmd(void *cb_arg,
       int argc,
       char **argv)
{
  tcmd_def	*cmdp;
  int ret;

  if (argc == 0)
    return SHELL_CONT;

  for (cmdp = cmd_defs;cmdp->name;cmdp++)
    {
      if (!strcmp(argv[0], cmdp->name))
	{
	  ret = cmdp->cmd(cb_arg, argc, argv);
	  return ret;
	}
    }

  fprintf(stderr, "cmd %s: not found\n", argv[0]);
  return SHELL_CONT;
}

tcmd_def	cmd_defs[] =
  {
    {"cd",      cmd_cd,         "change directory"},
    {"echo",    cmd_echo,       "echo arguments"},
    {"help",	cmd_help,	"gives help on a command"},
    {"lcd",     cmd_lcd,        "local chdir"},
    {"ls",      cmd_ls,         "list directory"},
    {"mkdir",   cmd_mkdir,      "make directory"},
    {"pwd",     cmd_pwd,        "print working dir"},
    {"quit",	cmd_quit,	"quit program"},
    {"rmdir",   cmd_rmdir,      "remove directory"},
    {"set",     cmd_set,        "list/set variables"},
    {"status",  cmd_status,     "get status of last command"},
    {"unset",   cmd_unset,      "remove variable"},
    {NULL,	NULL,		NULL},
  };

