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

int cmd_setattr(int argc, char **argv);

struct usage_def setattr_usage[] =
  {
    {'m', USAGE_PARAM, "metadata", "semicolon separated list of variables e.g. var1=val1;var2=val2;..."},
    {USAGE_NO_OPT, USAGE_MANDAT, "path", "remote object"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def setattr_cmd = {"setattr", "dump attributes of object", setattr_usage, cmd_setattr};

int
cmd_setattr(int argc,
         char **argv)
{
  int ret;
  char opt;
  char *path = NULL;
  dpl_dict_t *metadata = NULL;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(setattr_usage))) != -1)
    switch (opt)
      {
      case 'm':
        metadata = dpl_parse_metadata(optarg);
        if (NULL == metadata)
          {
            fprintf(stderr, "error parsing metadata\n");
            return SHELL_CONT;
          }
        break ;
      case '?':
      default:
        usage_help(&setattr_cmd);
      return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      usage_help(&setattr_cmd);
      return SHELL_CONT;
    }

  path = argv[0];

  ret = dpl_setattr(ctx, path, metadata);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return SHELL_CONT;
}
