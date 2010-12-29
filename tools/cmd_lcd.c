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

int cmd_lcd(int argc, char **argv);

struct usage_def lcd_usage[] =
  {
    {USAGE_NO_OPT, USAGE_MANDAT, "path", "local path"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def lcd_cmd = {"lcd", "change directory", lcd_usage, cmd_lcd};

int
cmd_lcd(int argc,
        char **argv)
{
  int ret;

  var_set("status", "1", VAR_CMD_SET, NULL);

  if (2 != argc)
    {
      usage_help(&lcd_cmd);
      return SHELL_CONT;
    }

  ret = chdir(argv[1]);
  if (-1 == ret)
    {
      perror("chdir");
      goto end;
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
