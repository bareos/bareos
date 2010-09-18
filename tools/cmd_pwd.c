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
cmd_pwd_usage()
{
  fprintf(stderr, "usage: pwd\n");
}

int
cmd_pwd(void *cb_arg,
       int argc,
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
        cmd_pwd_usage();
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (0 != argc)
    {
      cmd_pwd_usage();
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
  printf("/%s\n", ctx->cur_ino.key);
#endif

  var_set("status", "0", VAR_CMD_SET, NULL);

  return SHELL_CONT;
}
