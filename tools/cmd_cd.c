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

int
cmd_cd(void *cb_arg,
       int argc,
       char **argv)
{
  int ret;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;
  char *path;
  
  var_set("status", "1", VAR_CMD_SET, NULL);

  if (2 != argc)
    {
      fprintf(stderr, "usage: cd path\n");
      return SHELL_CONT;
    }

  path = argv[1];

  ret = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, NULL, &obj_ino, &obj_type);
  if (0 != ret)
    {
      fprintf(stderr, "path resolve failed %s\n", path);
      return SHELL_CONT;
    }

  if (DPL_FTYPE_DIR != obj_type)
    {
      fprintf(stderr, "not a directory\n");
      goto end;
    }

  ctx->cur_ino = obj_ino;

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
