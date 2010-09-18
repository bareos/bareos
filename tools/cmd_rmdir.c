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
cmd_rmdir_usage()
{
  fprintf(stderr, "usage: rmdir path\n");
}

int
cmd_rmdir(void *cb_arg,
          int argc,
          char **argv)
{
  char opt;
  int ret;
  char *path = NULL;
  char *dir_name = NULL;
  dpl_ino_t parent_ino;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, "")) != -1)
    switch (opt)
      {
      case '?':
      default:
        cmd_rmdir_usage();
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      cmd_rmdir_usage();
      return SHELL_CONT;
    }

  path = argv[0];

  dir_name = rindex(path, '/');
  if (NULL != dir_name)
    dir_name++;
  else
    dir_name = path;

  ret = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, &parent_ino, NULL, NULL);
  if (0 != ret)
    {
      fprintf(stderr, "path resolve failed %s\n", path);
      return SHELL_CONT;
    }

  ret = dpl_vdir_rmdir(ctx, ctx->cur_bucket, parent_ino, dir_name);
  if (0 != ret)
    {
      fprintf(stderr, "rmdir %s failed: %s\n", dir_name, dpl_status_str(ret));
      goto end;
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
