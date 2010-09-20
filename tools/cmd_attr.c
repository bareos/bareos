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

int cmd_attr(int argc, char **argv);

struct usage_def attr_usage[] =
  {
    {USAGE_NO_OPT, USAGE_MANDAT, "[bucket:]resource[?subresource]", "remote object"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def attr_cmd = {"attr", "dump metadata of object", attr_usage, cmd_attr};

static void
cb_attr(dpl_var_t *var,
        void *cb_arg)
{
  printf("%s=%s\n", var->key, var->value);
}

int
cmd_attr(int argc,
         char **argv)
{
  int ret;
  char opt;
  char *bucket = NULL;
  char *resource = NULL;
  char *subresource = NULL;
  dpl_dict_t *metadata = NULL;

  while ((opt = getopt(argc, argv, usage_getoptstr(attr_usage))) != -1)
    switch (opt)
      {
      case '?':
      default:
        usage_help(&attr_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      usage_help(&get_cmd);
      exit(1);
    }
  
  bucket = argv[0];
  resource = index(bucket, ':');
  if (NULL == resource)
    {
      resource = bucket;
      bucket = ctx->cur_bucket;
    }
  else
    {
      *resource++ = 0;
    }
  subresource = index(resource, '?');
  if (NULL != subresource)
    *subresource++ = 0;
  
  ret = dpl_head(ctx, bucket, resource, subresource, NULL, &metadata);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }

  dpl_dict_iterate(metadata, cb_attr, NULL);

 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return SHELL_CONT;
}
