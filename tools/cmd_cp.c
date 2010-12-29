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

int cmd_cp(int argc, char **argv);

struct usage_def cp_usage[] =
  {
    {USAGE_NO_OPT, USAGE_MANDAT, "local_file", "local file"},
    {USAGE_NO_OPT, USAGE_MANDAT, "remote_file", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def cp_cmd = {"cp", "server side copy", cp_usage, cmd_cp};

int
cmd_cp(int argc,
           char **argv)
{
  int ret;
  char opt;
  char *src_path = NULL;
  char *dst_path = NULL;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(cp_usage))) != -1)
    switch (opt)
      {
      case '?':
      default:
        usage_help(&cp_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (2 != argc)
    {
      usage_help(&cp_cmd);
      goto end;
    }

  src_path = argv[0];
  dst_path = argv[1];

  if (!strcmp(dst_path, "."))
    {
      char *p;

      p = rindex(src_path, '/');
      if (NULL != p)
        {
          p++;
          dst_path = p;
        }
      else
        {
          dst_path = src_path;
        }
    }

  ret = dpl_fcopy(ctx, src_path, dst_path);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
