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

dpl_ctx_t *ctx = NULL;
int status = 0;
u_int block_size = 0;
int hash = 0;

int
do_quit()
{
  vars_save();
  vars_free();
  dpl_ctx_free(ctx);
  dpl_free();

  exit(status);
}

char *
var_set_status(char *value)
{
  status = strtoul(value, NULL, 0);
  return xstrdup(value);
}

char *
var_set_trace_level(char *value)
{
  ctx->trace_level = strtoul(value, NULL, 0);
  return xstrdup(value);
}

char *
var_set_block_size(char *value)
{
  block_size = strtoul(value, NULL, 0);
  return xstrdup(value);
}

char *
var_set_hash(char *value)
{
  hash = strtoul(value, NULL, 0) == 0 ? 0 : 1;
  return xstrdup(value);
}

char *
var_set_delim(char *value)
{
  free(ctx->delim);
  ctx->delim = xstrdup(value);
  return xstrdup(value);
}

struct usage_def main_usage[] =
  {
    {'e', USAGE_PARAM, "cmd", "execute command"},
    {'d', USAGE_PARAM, "droplet_dir", "default is '~/.droplet'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def main_cmd = {"dpl_sh", "Droplet Shell", main_usage};

int
main(int argc,
     char **argv)
{
  int ret;
  char opt;
  enum shell_error shell_err;
  char *cmd = NULL;
  optind = 0;
  char *droplet_dir = NULL;
  char *profile_name = NULL;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error initing droplet\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(main_usage))) != -1)
    switch (opt)
      {
      case 'e':
        cmd = xstrdup(optarg);
        break ;
      case 'd':
        droplet_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case '?':
      default:
        usage_help(&main_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

#if 0
  {
    extern void test_strrstr();
    test_strrstr();
    exit(0);
  }
#endif

  if (0 != argc)
    {
      usage_help(&main_cmd);
      exit(1);
    }

  ctx = dpl_ctx_new(droplet_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "error creating droplet ctx\n");
      exit(1);
    }

  var_set("status", NULL, VAR_CMD_SET_SPECIAL, var_set_status);
  var_set("status", "0", VAR_CMD_SET, NULL);
  var_set("trace_level", NULL, VAR_CMD_SET_SPECIAL, var_set_trace_level);
  var_set("trace_level", "0", VAR_CMD_SET, NULL);
  var_set("block_size", NULL, VAR_CMD_SET_SPECIAL, var_set_block_size);
  var_set("block_size", "8192", VAR_CMD_SET, NULL);
  var_set("hash", NULL, VAR_CMD_SET_SPECIAL, var_set_hash);
  var_set("hash", "1", VAR_CMD_SET, NULL);
  var_set("delim", NULL, VAR_CMD_SET_SPECIAL, var_set_delim);
  var_set("delim", DPL_DEFAULT_DELIM, VAR_CMD_SET, NULL);

  vars_load();

  if (NULL != cmd)
    {
      ret = shell_parse(cmd_defs, cmd, &shell_err);
      if (ret == SHELL_EPARSE)
        fprintf(stderr, "parsing: %s\n", shell_error_str(shell_err));
      free(cmd);
      do_quit();
    }

  shell_install_cmd_defs(cmd_defs);
  rl_attempted_completion_function = shell_completion;
  rl_completion_entry_function = file_completion;

  shell_do(cmd_defs);

  do_quit();

  return 0;
}
