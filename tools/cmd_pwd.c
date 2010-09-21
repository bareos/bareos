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

int cmd_pwd(int argc, char **argv);

struct usage_def pwd_usage[] =
  {
    {0, 0u, NULL, NULL},
  };

struct cmd_def pwd_cmd = {"pwd", "print working directory", pwd_usage, cmd_pwd};

int
cmd_pwd(int argc,
        char **argv)
{
  char opt;
  //int ret;
  //char path[DPL_MAXPATHLEN];

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, "")) != -1)
    switch (opt)
      {
      case '?':
      default:
        usage_help(&pwd_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (0 != argc)
    {
      usage_help(&pwd_cmd);
      return SHELL_CONT;
    }

#if 0  
  ret = dpl_vdir_iname(ctx, ctx->cur_bucket, ctx->cur_ino, path, sizeof (path));
  if (0 != ret)
    {
      fprintf(stderr, "iname failed for %s:%s %d\n", ctx->cur_bucket, ctx->cur_ino.key, ret);
      return SHELL_CONT;
    }
  printf("%s\n", path);
#else
  printf("%s:/%s\n", ctx->cur_bucket, ctx->cur_ino.key);
#endif

  var_set("status", "0", VAR_CMD_SET, NULL);

  return SHELL_CONT;
}
