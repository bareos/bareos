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

int cmd_la(int argc, char **argv);

struct usage_def la_usage[] =
  {
    {'l', 0u, NULL, "long display"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def la_cmd = {"la", "list all my buckets", la_usage, cmd_la};

int 
cmd_la(int argc,
       char **argv)
{
  int ret;
  char opt;
  dpl_vec_t *vec = NULL;
  int lflag = 0;
  int i;

  optind = 0;

  var_set("status", "1", VAR_CMD_SET, NULL);

  while ((opt = getopt(argc, argv, usage_getoptstr(la_usage))) != -1)
    switch (opt)
      {
      case 'l':
        lflag = 1;
        break ;
      case '?':
      default:
        usage_help(&la_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (0 != argc)
    {
      usage_help(&la_cmd);
      return SHELL_CONT;
    }
  
  ret = dpl_list_all_my_buckets(ctx, &vec);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      return SHELL_CONT;
    }
  
  for (i = 0;i < vec->n_items;i++)
    {
      dpl_bucket_t *bucket;

      bucket = vec->array[i];
      if (1 == lflag)
        {
          struct tm *stm;

          stm = localtime(&bucket->creation_time);
          printf("%04d-%02d-%02d %02d:%02d %s\n", 1900 + stm->tm_year, 1 + stm->tm_mon, stm->tm_mday, stm->tm_hour, stm->tm_min, bucket->name);
        }
      else
        {
          printf("%s\n", bucket->name);
        }
    }

  dpl_vec_buckets_free(vec);

  var_set("status", "0", VAR_CMD_SET, NULL);

  return SHELL_CONT;
}
