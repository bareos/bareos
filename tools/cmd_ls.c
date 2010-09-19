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

int cmd_ls(int argc, char **argv);

struct usage_def ls_usage[] =
  {
    {'l', 0u, NULL, "long display"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def ls_cmd = {"ls", "list directory", ls_usage, cmd_ls};

#define LFLAG (1u<<0)

static void
do_ls_obj(dpl_dirent_t *entry,
          u_int flags)
{
  if (flags & LFLAG)
    {
      struct tm *stm;
      
      stm = localtime(&entry->last_modified);
      printf("%12llu %04d-%02d-%02d %02d:%02d %s\n", (unsigned long long) entry->size, 1900 + stm->tm_year, 1 + stm->tm_mon, stm->tm_mday, stm->tm_hour, stm->tm_min, entry->name);
    }
  else
    {
      printf("%s\n", entry->name);
    }
}

int
cmd_ls(int argc,
       char **argv)
{
  char opt;
  int ret;
  void *dir_hdl = NULL;
  dpl_dirent_t entry;
  char *path;
  u_int flags = 0u;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, "l")) != -1)
    switch (opt)
      {
      case 'l':
        flags |= LFLAG;
        break ;
      case '?':
      default:
        usage_help(&ls_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (0 == argc)
    {
      path = ".";
    }
  else if (1 == argc)
    {
      path = argv[0];
    }
  else
    {
      usage_help(&ls_cmd);
      return SHELL_CONT;
    }

  ret = dpl_opendir(ctx, path, &dir_hdl);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "opendir failure %s (%d)\n", dpl_status_str(ret), ret);
      return SHELL_CONT;
    }
  
  while (1 != dpl_vdir_eof(dir_hdl))
    {
      ret = dpl_vdir_readdir(dir_hdl, &entry);
      if (0 != ret)
        {
          fprintf(stderr, "read dir failure %d\n", ret);
          goto end;
        }
      
      do_ls_obj(&entry, flags);
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  if (NULL != dir_hdl)
    dpl_vdir_closedir(dir_hdl);

  return SHELL_CONT;
}
