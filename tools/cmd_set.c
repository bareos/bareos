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

void
set_usage()
{
  fprintf(stderr, "usage: set [var value]\n");
}

void
var_print_cb(tvar *var,
             void *cb_arg)
{
  printf("%s=%s\n", var->key, var->value);
}

int
cmd_set(void *cb_arg,
        int argc,
        char **argv)
{
  var_set("status", "1", VAR_CMD_SET, NULL);

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
      set_usage();
      goto end;
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
