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

int cmd_rawput(int argc, char **argv);

struct usage_def rawput_usage[] =
  {
    {'a', USAGE_PARAM, "canned_acl", "default is private"},
    {'A', 0u, NULL, "list available canned acls"},
    {'m', USAGE_PARAM, "metadata", "semicolon separated list of variables e.g. var1=val1;var2=val2;..."},
    {USAGE_NO_OPT, USAGE_MANDAT, "local_file", "local file"},
    {USAGE_NO_OPT, USAGE_MANDAT, "[bucket:]resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def rawput_cmd = {"rawput", "put file (raw)", rawput_usage, cmd_rawput};

int
cmd_rawput(int argc,
           char **argv)
{
  int ret;
  char opt;
  char *bucket = NULL;
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

  var_set("status", "1", VAR_CMD_SET, NULL);
  
  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(rawput_usage))) != -1)
    switch (opt)
      {
      case 'm':
        metadata = dpl_parse_metadata(optarg);
        if (NULL == metadata)
          {
            fprintf(stderr, "error parsing metadata\n");
            return SHELL_CONT;
          }
        break ;
      case 'a':
        canned_acl = dpl_canned_acl(optarg);
        if (-1 == canned_acl)
          {
            fprintf(stderr, "bad canned acl '%s'\n", optarg);
            return SHELL_CONT;
          }
        break ;
      case 'A':
        Aflag = 1;
        break ;
      case '?':
      default:
        usage_help(&rawput_cmd);
        exit(1);
      }
  argc -= optind;
  argv += optind;

  if (1 == Aflag)
    {
      for (i = 0;i < DPL_N_CANNED_ACL;i++)
        printf("%s\n", dpl_canned_acl_str(i));
      return SHELL_CONT;
    }
  
  if (2 != argc)
    {
      usage_help(&rawput_cmd);
      return SHELL_CONT;
    }
  
  local_file = argv[0];
  bucket = argv[1];
  resource = index(bucket, ':');
  if (NULL == resource)
    {
      resource = bucket;
      bucket = ctx->cur_bucket;
    }
  *resource++ = 0;
  subresource = index(resource, '?');
  if (NULL != subresource)
    *subresource++ = 0;

  if (!strcmp(resource, ""))
    resource = local_file;

  fd = open(local_file, O_RDONLY);
  if (-1 == fd)
    {
      perror("open");
      return SHELL_CONT;
    }

  ret = read_fd(fd, &data_buf, &data_len);
  if (0 != ret)
    {
      fprintf(stderr, "error reading file\n");
      close(fd);
      return SHELL_CONT;
    }

  ret = dpl_put(ctx, bucket, resource, subresource, metadata, canned_acl, data_buf, data_len);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      close(fd);
      return SHELL_CONT;
    }

  if (NULL != data_buf)
    free(data_buf);

  close(fd);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  var_set("status", "0", VAR_CMD_SET, NULL);

  return SHELL_CONT;
}
