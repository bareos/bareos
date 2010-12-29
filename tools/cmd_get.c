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

int cmd_get(int argc, char **argv);

struct usage_def get_usage[] =
  {
    {'k', 0u, NULL, "encrypt file"},
    {'s', USAGE_PARAM, "start", "range start offset"},
    {'e', USAGE_PARAM, "end", "range end offset"},
    {'m', 0u, NULL, "print metadata"},
    {USAGE_NO_OPT, USAGE_MANDAT, "path", "remote file"},
    {USAGE_NO_OPT, 0u, "local_file or - or |cmd", "local file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def get_cmd = {"get", "get file", get_usage, cmd_get};

struct get_data
{
  int fd;
  FILE *pipe;
};

static dpl_status_t
cb_get_buffered(void *cb_arg,
                char *buf,
                u_int len)
{
  struct get_data *get_data = (struct get_data *) cb_arg;
  int ret;

  if (NULL != get_data->pipe)
    {
      size_t s;

      s = fwrite(buf, 1, len, get_data->pipe);
      if (s != len)
        {
          perror("fwrite");
          return DPL_FAILURE;
        }
    }
  else
    {
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
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

static void
cb_print_metadata(dpl_var_t *var,
                  void *cb_arg)
{
  fprintf(stderr, "%s=%s\n", var->key, var->value);
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
  int do_stdout = 0;
  int kflag = 0;
  char *local_file = NULL;
  int start = -1;
  int start_inited = 0;
  int end = -1;
  int end_inited = 0;
  int mflag = 0;

  memset(&get_data, 0, sizeof (get_data));
  get_data.fd = -1;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(get_usage))) != -1)
    switch (opt)
      {
      case 'm':
        mflag = 1;
        break ;
      case 's':
        start = strtol(optarg, NULL, 0);
        start_inited = 1;
        break ;
      case 'e':
        end = strtol(optarg, NULL, 0);
        end_inited = 1;
        break ;
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

  if (start_inited != end_inited)
    {
      fprintf(stderr, "please provide -s and -e\n");
      return SHELL_CONT;
    }

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
      do_stdout = 1;
    }
  else if ('|' == local_file[0])
    {
      get_data.pipe = popen(local_file + 1, "w");
      if (NULL == get_data.pipe)
        {
          fprintf(stderr, "pipe failed\n");
          goto end;
        }
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

  if (1 == start_inited && 1 == end_inited)
    {
      char *data_buf;
      u_int data_len;

      ret = dpl_openread_range(ctx, path, (1 == kflag ? DPL_VFILE_FLAG_ENCRYPT : 0u), NULL, start, end, &data_buf, &data_len, &metadata);
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
          goto end;
        }
      ret = write_all(get_data.fd, data_buf, data_len);
      free(data_buf);
      if (0 != ret)
        {
          fprintf(stderr, "short write\n");
          goto end;
        }
      if (1 == mflag)
        dpl_dict_iterate(metadata, cb_print_metadata, NULL);
    }
  else
    {
      ret = dpl_openread(ctx, path, (1 == kflag ? DPL_VFILE_FLAG_ENCRYPT : 0u), NULL, cb_get_buffered, &get_data, &metadata);
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
          goto end;
        }
      if (1 == mflag)
        dpl_dict_iterate(metadata, cb_print_metadata, NULL);
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (0 == do_stdout)
    {
      if (-1 != get_data.fd)
        close(get_data.fd);

      if (NULL != get_data.pipe)
        pclose(get_data.pipe);
    }

  return SHELL_CONT;
}
