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

void
usage()
{
  fprintf(stderr, "usage: dpl_sh [-e cmd] bucket\n");
  exit(1);
}

int 
main(int argc, 
     char **argv)
{
  int ret;
  char opt;

  optind = 0;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error initing droplets\n");
      exit(1);
    }

  ctx = dpl_ctx_new(NULL, NULL);
  if (NULL == ctx)
    {
      fprintf(stderr, "error creating droplets ctx\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, "e:")) != -1)
    switch (opt)
      {
      case 'e':
        {
          tshell_error shell_err;
          char *line;
          
          line = xstrdup(optarg);
          ret = shell_parse(cmd_defs, line, NULL, &shell_err);
          if (ret == SHELL_EPARSE)
            fprintf(stderr, "parsing: %s\n", shell_error_str(shell_err));
          free(line);
          do_quit();
          exit(status);
        }
        break ;
      case '?':
      default:
        usage();
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      usage();
      exit(1);
    }

  ctx->cur_bucket = xstrdup(argv[0]);
  ctx->cur_ino = DPL_ROOT_INO;
  //ctx->trace_level = 0xffffffff & ~0x40;
  //ctx->trace_level = 0xffffffff;

  shell_install_cmd_defs(cmd_defs);
  //rl_attempted_completion_function = shell_completion;
  //rl_completion_entry_function = file_completion;

  shell_do(cmd_defs, "dpl_sh> ", NULL);

  exit(status);
}
