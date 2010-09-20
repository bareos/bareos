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

#include "dplsh.h"

int cmd_get(int argc, char **argv);

struct usage_def get_usage[] =
  {
    {'k', 0u, NULL, "encrypt file"},
    {USAGE_NO_OPT, USAGE_MANDAT, "path", "remote file"},
    {USAGE_NO_OPT, 0u, "local_file or -", "local file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def get_cmd = {"get", "get file", get_usage, cmd_get};

struct get_data
{
  int fd;
};

static dpl_status_t
cb_get_buffered(void *cb_arg,
                char *buf,
                u_int len)
{
  struct get_data *get_data = (struct get_data *) cb_arg;
  int ret;

  if (1 == hash)
    {
      fprintf(stderr, "#");
      fflush(stderr);
    }
  
  ret = write_all(get_data->fd, buf, len);
  if (0 != ret)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

int
cmd_get(int argc,
        char **argv)
{
  int ret;
  char opt;
  char *path = NULL;
  dpl_dict_t *metadata = NULL;
  struct get_data get_data;
  int local_is_stdout = 0;
  int kflag = 0;
  char *local_file = NULL;

  get_data.fd = -1;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(get_usage))) != -1)
    switch (opt)
      {
      case 'k':
        kflag = 1;
        break ;
      case '?':
      default:
        usage_help(&get_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (2 == argc)
    {
      path = argv[0];
      local_file = argv[1];
    }
  else if (1 == argc)
    {
      path = argv[0];
      local_file = rindex(path, '/');
      if (NULL != local_file)
        local_file++;
      else
        local_file = path;
    }
  else
    {
      usage_help(&get_cmd);
      return SHELL_CONT;
    }

  if (!strcmp(local_file, "-"))
    {
      get_data.fd = 1;
      local_is_stdout = 1;
    }
  else
    {
      ret = access(local_file, F_OK);
      if (0 == ret)
        {
          if (1 == ask_for_confirmation("file already exists, overwrite?"))
            return SHELL_CONT;
        }

      get_data.fd = open(local_file, O_WRONLY|O_CREAT, 0600);
      if (-1 == get_data.fd)
        {
          perror("open");
          goto end;
        }
    }

  ret = dpl_openread(ctx, path, (1 == kflag ? DPL_VFILE_FLAG_ENCRYPT : 0u), NULL, cb_get_buffered, &get_data);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }
  
  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (0 == local_is_stdout)
    {
      if (-1 != get_data.fd)
        close(get_data.fd);
    }

  return SHELL_CONT;
}
