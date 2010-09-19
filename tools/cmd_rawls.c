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

int cmd_rawls(int argc, char **argv);

struct usage_def rawls_usage[] =
  {
    {'l', 0u, NULL, "long display"},
    {'P', USAGE_PARAM, "prefix", "default unset"},
    {'D', USAGE_PARAM, "delimiter", "default unset"},
    {USAGE_NO_OPT, 0u, "bucket", "remote bucket"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def rawls_cmd = {"rawls", "list bucket (raw)", rawls_usage, cmd_rawls};

int 
cmd_rawls(int argc,
          char **argv)
{
  int ret;
  char opt;
  char *bucket = NULL;
  dpl_vec_t *objects = NULL;
  dpl_vec_t *common_prefixes = NULL;
  int i;
  size_t total_size = 0;
  int lflag = 0;
  char *prefix = NULL;
  char *delimiter = NULL;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(rawls_usage))) != -1)
    switch (opt)
      {
      case 'P':
        prefix = xstrdup(optarg);
        break ;
      case 'D':
        delimiter = xstrdup(optarg);
        break ;
      case 'l':
        lflag = 1;
        break ;
      case '?':
      default:
        usage_help(&rawls_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (1 == argc)
    {
      bucket = argv[0];
    }
  else if (0 == argc)
    {
      bucket = ctx->cur_bucket;
    }
  else
    {
      usage_help(&rawls_cmd);
      return SHELL_CONT;
    }
  
  ret = dpl_list_bucket(ctx, bucket, prefix, delimiter, &objects, &common_prefixes);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      return SHELL_CONT;
    }

  for (i = 0;i < objects->n_items;i++)
    {
      dpl_object_t *resource;
      struct tm *stm;

      resource = objects->array[i];
      if (1 == lflag)
        {
          stm = localtime(&resource->last_modified);
          printf("%12llu %04d-%02d-%02d %02d:%02d %s\n", (unsigned long long) resource->size, 1900 + stm->tm_year, 1 + stm->tm_mon, stm->tm_mday, stm->tm_hour, stm->tm_min, resource->key);
        }
      else
        {
          printf("%s\n", resource->key);
        }
      
      total_size += resource->size;
    }

  for (i = 0;i < common_prefixes->n_items;i++)
    {
      dpl_common_prefix_t *resource;

      resource = common_prefixes->array[i];
      printf("<PREFIX> %s\n", resource->prefix);
    }

  if (1 == lflag)
    {
      if (NULL != ctx->pricing)
        printf("Total %s Price %s\n", dpl_size_str(total_size), dpl_price_storage_str(ctx, total_size));
    }

  dpl_vec_objects_free(objects);
  dpl_vec_common_prefixes_free(common_prefixes);

  var_set("status", "0", VAR_CMD_SET, NULL);

  return SHELL_CONT;
}
