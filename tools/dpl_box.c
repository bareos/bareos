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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <droplets.h>
#include "usage.h"
#include "xfuncs.h"

int la_func(int argc, char **argv);
int ls_func(int argc, char **argv);
int mb_func(int argc, char **argv);
int get_func(int argc, char **argv);
int head_func(int argc, char **argv);
int put_func(int argc, char **argv);
int bput_func(int argc, char **argv);
int bget_func(int argc, char **argv);
int delete_func(int argc, char **argv);

#define VFLAG_HELP_STR "mask: combination of 0x1(conn),0x2(io),0x4(http),0x8(ssl),0x10(s3),0x20(vdir),0x40(buf)"

struct usage_def la_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {'l', 0u, NULL, "long display"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def la_cmd = {"dpl_la", "List All My Buckets", la_usage, la_func};

/**/

struct usage_def ls_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {'l', 0u, NULL, "long display"},
    {'P', USAGE_PARAM, "prefix", "default unset"},
    {'D', USAGE_PARAM, "delimiter", "default unset"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket", "remote bucket"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def ls_cmd = {"dpl_ls", "List Bucket and give Price", ls_usage, ls_func};

/**/

struct usage_def mb_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {'a', USAGE_PARAM, "canned_acl", "default is private"},
    {'A', 0u, NULL, "list available canned acls"},
    {'l', USAGE_PARAM, "location_constraint", "default is US Standard"},
    {'L', 0u, NULL, "list available location constraints"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket", "bucket name"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def mb_cmd = {"dpl_mb", "Make Bucket", mb_usage, mb_func};

/**/

struct usage_def get_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket:resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def get_cmd = {"dpl_get", "Get Object or subresource", get_usage, get_func};

/**/

struct usage_def head_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket:resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def head_cmd = {"dpl_head", "Dump metadata of object", head_usage, head_func};

/**/

struct usage_def put_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {'a', USAGE_PARAM, "canned_acl", "default is private"},
    {'A', 0u, NULL, "list available canned acls"},
    {'m', USAGE_PARAM, "metadata", "semicolon separated list of variables e.g. var1=val1;var2=val2;..."},
    {USAGE_NO_OPT, USAGE_MANDAT, "local_file", "local file"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket:resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def put_cmd = {"dpl_put", "Put Object", put_usage, put_func};

/**/

struct usage_def bput_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {'a', USAGE_PARAM, "canned_acl", "default is private"},
    {'A', 0u, NULL, "list available canned acls"},
    {'m', USAGE_PARAM, "metadata", "semicolon separated list of variables e.g. var1=val1;var2=val2;..."},
    {'b', USAGE_PARAM, "block_size", "default is 8192"},
    {'C', 0u, NULL, "show progress bar"},
    {USAGE_NO_OPT, USAGE_MANDAT, "local_file", "local file"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket:resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def bput_cmd = {"dpl_bput", "Streamed Put", bput_usage, bput_func};

/**/

struct usage_def bget_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {'b', USAGE_PARAM, "block_size", "default is 8192"},
    {'C', 0u, NULL, "show progress bar"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket:resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def bget_cmd = {"dpl_bget", "Streamed Get", bget_usage, bget_func};

/**/

struct usage_def delete_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {'i', 0u, NULL, "do not ask for confirmation"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket[:resource[?subresource]]", "remote bucket/dir/file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def delete_cmd = {"dpl_delete", "Delete Bucket or Object", delete_usage, delete_func};

/**/

struct cmd_def *cmds[] = 
  {
    &la_cmd,
    &ls_cmd,
    &mb_cmd,
    &get_cmd,
    &head_cmd,
    &put_cmd,
    &bput_cmd,
    &bget_cmd,
    &delete_cmd,
    NULL,
  }; 

/** 
 * ask for confirmation
 * 
 * @param str 
 * 
 * @return 0 if 'Yes', else 1
 */
int 
ask_for_confirmation(char *str)
{
  char buf[256];
  ssize_t cc;
  char *p;

  fprintf(stderr, "%s (please type 'Yes' to confirm) ", str);
  fflush(stderr);
  cc = read(0, buf, sizeof(buf));
  if (-1 == cc)
    {
      perror("read");
      exit(1);
    }
  
  buf[sizeof (buf) - 1] = 0;

  if ((p = index(buf, '\n')))
    *p = 0;
  
  if (!strcmp(buf, "Yes"))
    return 0;

  return 1;
}

int
write_all(int fd,
          char *buf,
          int len)
{
  ssize_t cc;
  int remain;

  remain = len;
  while (1)
    {
    again:
      cc = write(fd, buf, remain);
      if (-1 == cc)
        {
          if (EINTR == errno)
            goto again;

          return -1;
        }
      
      remain -= cc;
      buf += cc;

      if (0 == remain)
        return 0;
    }
}

int
read_all(int fd,
         char *buf,
         int len)
{
  ssize_t cc;
  int remain;
  
  remain = len;
  while (1)
    {
    again:
      cc = read(fd, buf, remain);
      if (-1 == cc)
        {
          if (EINTR == errno)
            goto again;

          return -1;
        }
      
      if (0 == cc && 0 != len)
        {
          return -2;
        }

      remain -= cc;
      buf += cc;

      if (0 == remain)
        {
          return 0;
        }
    }
}

int
read_fd(int fd,
        char **data_bufp,
        u_int *data_lenp)
{
  char buf[8192];
  ssize_t cc;
  char *data_buf = NULL;
  u_int data_len = 0;

  while (1)
    {
      cc = read(fd, buf, sizeof (buf));
      if (-1 == cc)
        {
          return -1;
        }
      
      if (0 == cc)
        {
          break ;
        }
      
      if (NULL == data_buf)
        {
          data_buf = malloc(cc);
          if (NULL == data_buf)
            return -1;
        }
      else
        {
          data_buf = realloc(data_buf, data_len + cc);
          if (NULL == data_buf)
            return -1;
        }

      memcpy(data_buf + data_len, buf, cc);
      data_len += cc;
    }

  if (NULL != data_bufp)
    *data_bufp = data_buf;
  
  if (NULL != data_lenp)
    *data_lenp = data_len;

  return 0;
}

int 
la_func(int argc,
        char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  dpl_vec_t *vec = NULL;
  int lflag = 0;
  int i;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(la_usage))) != -1)
    switch (opt)
      {
      case 'l':
        lflag = 1;
        break ;
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&la_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (0 != argc)
    {
      usage_help(&la_cmd);
      exit(1);
    }
  
  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  ret = dpl_list_all_my_buckets(ctx, &vec);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
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
  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

int
ls_func(int argc,
        char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  char *bucket;
  dpl_vec_t *objects = NULL;
  dpl_vec_t *common_prefixes = NULL;
  int i;
  size_t total_size = 0;
  int lflag = 0;
  char *prefix = NULL;
  char *delimiter = NULL;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(ls_usage))) != -1)
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
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&ls_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      usage_help(&ls_cmd);
      exit(1);
    }
  
  bucket = argv[0];

  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  ret = dpl_list_bucket(ctx, bucket, prefix, delimiter, &objects, &common_prefixes);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
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
      printf("<COMMONPREFIX> %s\n", resource->prefix);
    }

  if (1 == lflag)
    {
      if (NULL != ctx->pricing)
        printf("Total %s Price %s\n", dpl_size_str(total_size), dpl_price_storage_str(ctx, total_size));
    }

  dpl_vec_objects_free(objects);
  dpl_vec_common_prefixes_free(common_prefixes);
  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

int
mb_func(int argc,
        char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  char *bucket;
  char *resource;
  dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
  int Aflag = 0;
  int i;
  dpl_location_constraint_t location_constraint = DPL_LOCATION_CONSTRAINT_US_STANDARD;
  int Lflag = 0;  

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(mb_usage))) != -1)
    switch (opt)
      {
      case 'l':
        location_constraint = dpl_location_constraint(optarg);
        if (-1 == location_constraint)
          {
            fprintf(stderr, "bad location constraint '%s'\n", optarg);
            exit(1);
          }
        break ;
      case 'L':
        Lflag = 1;
        break ;
      case 'a':
        canned_acl = dpl_canned_acl(optarg);
        if (-1 == canned_acl)
          {
            fprintf(stderr, "bad canned acl '%s'\n", optarg);
            exit(1);
          }
        break ;
      case 'A':
        Aflag = 1;
        break ;
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&mb_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 == Aflag)
    {
      for (i = 0;i < DPL_N_CANNED_ACL;i++)
        printf("%s\n", dpl_canned_acl_str(i));
      exit(0);
    }

  if (1 == Lflag)
    {
      for (i = 0;i < DPL_N_LOCATION_CONSTRAINT;i++)
        printf("%s\n", dpl_location_constraint_str(i));
      exit(0);
    }
  
  if (1 != argc)
    {
      usage_help(&mb_cmd);
      exit(1);
    }
  
  bucket = argv[0];
  resource = "/";

  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  ret = dpl_make_bucket(ctx, bucket, location_constraint, canned_acl);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

int
get_func(int argc,
         char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  char *bucket;
  char *resource;
  char *subresource = NULL;
  char *data_buf = NULL;
  u_int data_len;
  dpl_dict_t *metadata = NULL;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(get_usage))) != -1)
    switch (opt)
      {
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&get_cmd);
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
      usage_help(&get_cmd);
      exit(1);
    }
  *resource++ = 0;
  subresource = index(resource, '?');
  if (NULL != subresource)
    *subresource++ = 0;
  
  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  ret = dpl_get(ctx, bucket, resource, subresource, NULL, &data_buf, &data_len, &metadata);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  ret = write_all(1, data_buf, data_len);
  if (0 != ret)
    {
      fprintf(stderr, "write failed\n");
      exit(1);
    }

  if (NULL != data_buf)
    free(data_buf);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

static void
cb_head(dpl_var_t *var,
        void *cb_arg)
{
  printf("%s=%s\n", var->key, var->value);
}

int
head_func(int argc,
          char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  char *bucket;
  char *resource;
  char *subresource = NULL;
  dpl_dict_t *metadata = NULL;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(get_usage))) != -1)
    switch (opt)
      {
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&head_cmd);
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
      usage_help(&get_cmd);
      exit(1);
    }
  *resource++ = 0;
  subresource = index(resource, '?');
  if (NULL != subresource)
    *subresource++ = 0;
  
  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  ret = dpl_head(ctx, bucket, resource, subresource, NULL, &metadata);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  dpl_dict_iterate(metadata, cb_head, NULL);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

int
put_func(int argc,
         char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  char *bucket;
  char *resource;
  char *subresource = NULL;
  dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
  int Aflag = 0;
  int i;
  char *data_buf = NULL;
  u_int data_len = 0;
  int fd;
  dpl_dict_t *metadata = NULL;
  char *local_file;
  
  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(put_usage))) != -1)
    switch (opt)
      {
      case 'm':
        metadata = dpl_parse_metadata(optarg);
        if (NULL == metadata)
          {
            fprintf(stderr, "error parsing metadata\n");
            exit(1);
          }
        break ;
      case 'a':
        canned_acl = dpl_canned_acl(optarg);
        if (-1 == canned_acl)
          {
            fprintf(stderr, "bad canned acl '%s'\n", optarg);
            exit(1);
          }
        break ;
      case 'A':
        Aflag = 1;
        break ;
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&put_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 == Aflag)
    {
      for (i = 0;i < DPL_N_CANNED_ACL;i++)
        printf("%s\n", dpl_canned_acl_str(i));
      exit(0);
    }
  
  if (2 != argc)
    {
      usage_help(&put_cmd);
      exit(1);
    }
  
  local_file = argv[0];
  bucket = argv[1];
  resource = index(bucket, ':');
  if (NULL == resource)
    {
      usage_help(&put_cmd);
      exit(1);
    }
  *resource++ = 0;
  subresource = index(resource, '?');
  if (NULL != subresource)
    *subresource++ = 0;

  if (!strcmp(resource, ""))
    resource = local_file;

  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  fd = open(local_file, O_RDONLY);
  if (-1 == fd)
    {
      perror("open");
      exit(1);
    }

  ret = read_fd(fd, &data_buf, &data_len);
  if (0 != ret)
    {
      fprintf(stderr, "error reading file\n");
      exit(1);
    }

  ret = dpl_put(ctx, bucket, resource, subresource, metadata, canned_acl, data_buf, data_len);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  if (NULL != data_buf)
    free(data_buf);

  if (0 != fd)
    close(fd);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

int
bput_func(int argc,
          char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  char *bucket;
  char *resource;
  char *subresource = NULL;
  dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
  int Aflag = 0;
  int i;
  char *buf = NULL;
  size_t block_size = 8192;
  ssize_t cc;
  struct stat st;
  int fd;
  dpl_vfile_t *vfile = NULL;
  int connection_close = 0;
  int Cflag = 0;
  int n_cols = 78;
  int cur_col = 0;
  int new_col;
  double percent;
  size_t size_processed;
  dpl_dict_t *metadata = NULL;
  dpl_dict_t *headers_returned = NULL;
  char *local_file;
  
  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(bput_usage))) != -1)
    switch (opt)
      {
      case 'm':
        metadata = dpl_parse_metadata(optarg);
        if (NULL == metadata)
          {
            fprintf(stderr, "error parsing metadata\n");
            exit(1);
          }
        break ;
      case 'C':
        Cflag = 1;
        break ;
      case 'b':
        block_size = strtoul(optarg, NULL, 0);
        break ;
      case 'a':
        canned_acl = dpl_canned_acl(optarg);
        if (-1 == canned_acl)
          {
            fprintf(stderr, "bad canned acl '%s'\n", optarg);
            exit(1);
          }
        break ;
      case 'A':
        Aflag = 1;
        break ;
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&bput_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 == Aflag)
    {
      for (i = 0;i < DPL_N_CANNED_ACL;i++)
        printf("%s\n", dpl_canned_acl_str(i));
      exit(0);
    }
  
  if (2 != argc)
    {
      usage_help(&put_cmd);
      exit(1);
    }
  
  local_file = argv[0];
  bucket = argv[1];
  resource = index(bucket, ':');
  if (NULL == resource)
    {
      usage_help(&put_cmd);
      exit(1);
    }
  *resource++ = 0;
  subresource = index(resource, '?');
  if (NULL != subresource)
    *subresource++ = 0;

  if (!strcmp(resource, ""))
    resource = local_file;

  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  buf = alloca(block_size);

  fd = open(local_file, O_RDONLY);
  if (-1 == fd)
    {
      perror("open");
      exit(1);
    }

  ret = fstat(fd, &st);
  if (-1 == ret)
    {
      perror("fstat");
      exit(1);
    }

  ret = dpl_vfile_put(ctx, bucket, resource, subresource, metadata, canned_acl, st.st_size, &vfile);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  percent = 0;
  size_processed = 0;

  while (1)
    {
      cc = read(fd, buf, block_size);
      if (-1 == cc)
        {
          return -1;
        }
      
      if (0 == cc)
        {
          break ;
        }
      
      ret = dpl_vfile_write_all(vfile, buf, cc);
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "write failed\n");
          exit(1);
        }

      size_processed += cc;
      percent = (double) size_processed/(double) st.st_size;
      new_col = (int) (percent * n_cols);

      if (Cflag)
        {
          if (new_col > cur_col)
            {
              int i;
              
              if (cur_col != 0)
                {
                  printf("\b\b\b\b\b\b\b\b");
                }
              
              for (i = 0;i < (new_col-cur_col);i++)
                printf("=");
              
              printf("> %.02f%%", percent*100);
              
              cur_col = new_col;
              
              fflush(stdout);
            }
        }

    }

  if (Cflag)
    printf("\b\b\b\b\b\b\b\b\b=>100.00%%\n");

  close(fd);
  
  ret = dpl_read_http_reply(vfile->conn, NULL, NULL, &headers_returned);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "request failed %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }
  else
    {
      connection_close = dpl_connection_close(headers_returned);
    }

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  //printf("connection_close %d\n", connection_close);
  
#if 0 //XXX
  if (1 == connection_close)
    dpl_conn_terminate(vfile->conn);
  else
    dpl_conn_release(vfile->conn);
#endif

  dpl_vfile_free(vfile);

  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

int
bget_func(int argc,
          char **argv)
{
  return -1;
}

int
delete_func(int argc,
            char **argv)
{
  int ret;
  char opt;
  dpl_ctx_t *ctx;
  u_int trace_level = 0u;
  char *droplets_dir = NULL;
  char *profile_name = NULL;
  char *bucket;
  char *resource;
  char *subresource = NULL;
  int iflag = 1;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "unable to init droplets\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, usage_getoptstr(delete_usage))) != -1)
    switch (opt)
      {
      case 'i':
        iflag = 0;
        break ;
      case 'd':
        droplets_dir = xstrdup(optarg);
        break ;
      case 'p':
        profile_name = xstrdup(optarg);
        break ;
      case 'v':
        trace_level = strtoul(optarg, NULL, 0);
        break ;
      case '?':
      default:
        usage_help(&delete_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 != argc)
    {
      usage_help(&delete_cmd);
      exit(1);
    }
  
  bucket = argv[0];
  resource = index(bucket, ':');
  if (NULL == resource)
    {
      resource = "/";
    }
  else
    {
      *resource++ = 0;
      subresource = index(resource, '?');
      if (NULL != subresource)
        *subresource++ = 0;
    }

  if (1 == iflag)
    {
      if (0 != ask_for_confirmation("Really?"))
        exit(0);
    }

  ctx = dpl_ctx_new(droplets_dir, profile_name);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  ctx->trace_level = trace_level;

  ret = dpl_delete(ctx, bucket, resource, subresource);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  dpl_ctx_free(ctx);
  dpl_free();

  return (-1 == ret) ? 1 : 0;
}

struct usage_def default_usage[] =
  {
    {'l', 0u, NULL, "list built-in commands"},
    {0, 0u, NULL, NULL},
  };

int cmd_default(int argc, char **argv);

struct cmd_def default_cmd = {"dpl_box", "Droplets Tool Box", default_usage, cmd_default};

int 
do_cmd(char *command, 
       int argc, 
       char **argv)
{
  int i;
  char *p;

  p = rindex(argv[0], '/');
  if (NULL == p)
    p = argv[0];
  else
    p++;

  i = 0;
  while (1)
    {
      if (NULL == cmds[i])
        break ;

      if (!strcmp(cmds[i]->name, p))
        return cmds[i]->func(argc, argv);

      i++;
    }

  return cmd_default(argc, argv);
}

int 
cmd_default(int argc, 
            char **argv)
{
  char opt;
  int lflag, i;
  char *command;

  command = NULL;

  lflag = 0;
  while ((opt = getopt(argc, argv, usage_getoptstr(default_usage))) != -1)
    switch (opt)
      {
      case 'l':
        lflag = 1;
        break ;
      case '?':
      default:
        usage_help(&default_cmd);
        exit(1);
      }

  argc -= optind;
  argv += optind;

  if (lflag)
    {
      i = 0;
      while (1)
        {
          if (NULL == cmds[i])
            break ;
          printf("%s ", cmds[i]->name);
          i++;
        }
      printf("\n");
    }

  return 0;
}

int
main(int argc,
     char **argv)
{
  return do_cmd(argv[0], argc, argv);
}
