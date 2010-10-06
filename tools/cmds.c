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

#include "dplsh.h"

int cmd_quit(int argc, char **argv);
int cmd_help(int argc, char **argv);

struct usage_def quit_usage[] =
  {
    {0, 0u, NULL, NULL},
  };

struct cmd_def quit_cmd = {"quit", "quit program", quit_usage, cmd_quit};

int
cmd_quit(int argc,
	 char **argv)
{
  return SHELL_RETURN;
}

struct usage_def help_usage[] =
  {
    {0, 0u, NULL, NULL},
  };

struct cmd_def help_cmd = {"help", "help", help_usage, cmd_help};

int 
cmd_help(int argc,
	 char **argv)
{
  if (argc == 1)
    {
      struct cmd_def *cmdp;
      int i, j;

      j = 0;
      for (i = 0;cmd_defs[i];i++)
	{
          cmdp = cmd_defs[i];
	  printf("%16s", cmdp->name);
	  j++;
	  if (j == 4)
	    {
	      printf("\n");
	      j = 0;
	    }
	}
      printf("\n");
    }
  else if (argc == 2)
    {
      struct cmd_def *cmdp;
      int i;

      for (i = 0;cmd_defs[i];i++)
	{
          cmdp = cmd_defs[i];
	  if (!strcmp(argv[1], cmdp->name))
	    {
	      printf("%s\n", cmdp->purpose);
	      break ;
	    }
	}
    }

  return SHELL_CONT;
}

struct cmd_def	*cmd_defs[] =
  {
    &cd_cmd,
    &genurl_cmd,
    &get_cmd,
    &getattr_cmd,
    &help_cmd,
    &lcd_cmd,
    &la_cmd,
    &ls_cmd,
    &mb_cmd,
    &mkdir_cmd,
    &put_cmd,
    &pwd_cmd,
    &quit_cmd,
    &rb_cmd,
    &rm_cmd,
    &rmdir_cmd,
    &set_cmd,
    &unset_cmd,
    NULL,
  };

