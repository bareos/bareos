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

dpl_ctx_t *ctx = NULL;
int status = 0;

int
do_quit()
{
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

struct usage_def main_usage[] =
  {
    {'e', USAGE_PARAM, "cmd", "execute command"},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def main_cmd = {"dpl_sh", "Droplets Shell", main_usage};

int 
main(int argc, 
     char **argv)
{
  int ret;
  char opt;
  enum shell_error shell_err;
  char *cmd = NULL;
  optind = 0;
  char *droplets_dir = NULL;
  char *profile_name = NULL;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error initing droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(main_usage))) != -1)
    switch (opt)
      {
      case 'e':
        cmd = xstrdup(optarg);
        break ;
      case 'd':
        droplets_dir = xstrdup(optarg);
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

  if (0 != argc)
    {
      usage_help(&main_cmd);
      exit(1);
    }

  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "error creating droplets ctx\n");
      exit(1);
    }

  var_set("status", NULL, VAR_CMD_SET_SPECIAL, var_set_status);
  var_set("status", "0", VAR_CMD_SET, NULL);
  var_set("trace_level", NULL, VAR_CMD_SET_SPECIAL, var_set_trace_level);
  var_set("trace_level", "0", VAR_CMD_SET, NULL);

  if (NULL != cmd)
    {
      ret = shell_parse(cmd_defs, cmd, &shell_err);
      if (ret == SHELL_EPARSE)
        fprintf(stderr, "parsing: %s\n", shell_error_str(shell_err));
      free(cmd);
      do_quit();
    }

  ctx->cur_bucket = xstrdup("");
  ctx->cur_ino = DPL_ROOT_INO;
  
  shell_install_cmd_defs(cmd_defs);
  rl_attempted_completion_function = shell_completion;
  //rl_completion_entry_function = file_completion;

  shell_do(cmd_defs);

  do_quit();

  return 0;
}
