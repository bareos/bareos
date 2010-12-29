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

int cmd_rb(int argc, char **argv);

struct usage_def rb_usage[] =
  {
    {USAGE_NO_OPT, 0u, "bucket", "remote bucket"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def rb_cmd = {"rb", "remove bucket", rb_usage, cmd_rb};

int
cmd_rb(int argc,
       char **argv)
{
  int ret;
  char opt;
  char *bucket = NULL;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(rb_usage))) != -1)
    switch (opt)
      {
      case '?':
      default:
        usage_help(&rb_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (1 == argc)
    {
      bucket = argv[0];
    }
  else if (0 == argc)
    {
      bucket = ctx->cur_bucket;
    }
  else
    {
      usage_help(&rb_cmd);
      return SHELL_CONT;
    }

  ret = dpl_deletebucket(ctx, bucket);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
