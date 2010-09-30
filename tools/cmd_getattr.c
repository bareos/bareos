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

int cmd_getattr(int argc, char **argv);

struct usage_def getattr_usage[] =
  {
    {USAGE_NO_OPT, USAGE_MANDAT, "path", "remote object"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def getattr_cmd = {"getattr", "dump attributes of object", getattr_usage, cmd_getattr};

static void
cb_getattr(dpl_var_t *var,
        void *cb_arg)
{
  printf("%s=%s\n", var->key, var->value);
}

int
cmd_getattr(int argc,
         char **argv)
{
  int ret;
  char opt;
  char *path = NULL;
  dpl_dict_t *metadata = NULL;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(getattr_usage))) != -1)
    switch (opt)
      {
      case '?':
      default:
        usage_help(&getattr_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      usage_help(&getattr_cmd);
      exit(1);
    }
  
  path = argv[0];
  
  ret = dpl_getattr(ctx, path, NULL, &metadata);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }

  dpl_dict_iterate(metadata, cb_getattr, NULL);

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return SHELL_CONT;
}
