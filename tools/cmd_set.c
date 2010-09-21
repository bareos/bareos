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

int cmd_set(int argc, char **argv);

struct usage_def set_usage[] =
  {
    {USAGE_NO_OPT, USAGE_MANDAT, "var", ""},
    {USAGE_NO_OPT, USAGE_MANDAT, "value", ""},
    {0, 0u, NULL, NULL},
  };

struct cmd_def set_cmd = {"set", "set variable", set_usage, cmd_set};

static void
var_print_cb(tvar *var,
             void *cb_arg)
{
  printf("%s=%s\n", var->key, var->value);
}

int
cmd_set(int argc,
        char **argv)
{
  if (3 == argc)
    {
      var_set(argv[1], argv[2], VAR_CMD_SET, NULL);
      goto end;
    }
  else if (1 == argc)
    {
      vars_iterate(var_print_cb, NULL);
      goto end;
    }
  else
    {
      usage_help(&set_cmd);
      goto end;
    }

 end:

  return SHELL_CONT;
}
