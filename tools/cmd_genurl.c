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

int cmd_genurl(int argc, char **argv);

struct usage_def genurl_usage[] =
  {
    {'d', USAGE_PARAM, "duration", "expiration from now in seconds"},
    {USAGE_NO_OPT, USAGE_MANDAT, "path", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def genurl_cmd = {"genurl", "generate a delegation url", genurl_usage, cmd_genurl};

int
cmd_genurl(int argc,
           char **argv)
{
  char opt;
  int ret;
  char *path;
  char buf[4096];
  u_int len;
  int duration = 10*60;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, "d:")) != -1)
    switch (opt)
      {
      case 'd':
        duration = atoi(optarg);
        break ;
      case '?':
      default:
        usage_help(&genurl_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      usage_help(&genurl_cmd);
      return SHELL_CONT;
    }

  path = argv[0];

  ret = dpl_vfile_genurl(ctx, path, time(0) + duration, buf, sizeof (buf), &len);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "genurl failed %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }

  printf("%.*s\n", len, buf);

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
