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

#include "droplet.h"
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

dpl_ctx_t *ctx = NULL;

void
usage()
{
  fprintf(stderr, "usage: test_req [-m method][-b bucket][-r resource][-s subresource][-M (compute MD5)][-c chunk_file][-C content_type][-e (expect)][-t trace_level][-q query_params]\n");
  exit(1);
}

void
xperror(char *str)
{
  perror(str);
  exit(1);
}

char *
xstrdup(char *str)
{
  char *nstr;

  nstr = strdup(str);
  if (NULL == nstr)
    {
      perror("strdup");
      exit(1);
    }

  return nstr;
}

void *
xmalloc(size_t size)
{
  void *ptr;

  ptr = malloc(size);
  if (NULL == ptr)
    xperror("malloc");

  return ptr;
}

int
main(int argc,
     char **argv)
{
  int ret;
  char opt;
  dpl_req_t *req = NULL;
  dpl_dict_t *headers_request = NULL;
  dpl_dict_t *query_params = NULL;
  char header[8192];
  u_int header_len;

  optind = 0;

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error initing droplet\n");
      exit(1);
    }

  ctx = dpl_ctx_new(NULL, NULL);
  if (NULL == ctx)
    {
      fprintf(stderr, "error creating droplet ctx\n");
      exit(1);
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      fprintf(stderr, "error creating req\n");
      exit(1);
    }

  while ((opt = getopt(argc, argv, "m:b:r:s:Mc:C:et:q:")) != -1)
    switch (opt)
      {
      case 'm':
        {
          dpl_method_t method;

          method = dpl_method(optarg);
          if (-1 == method)
            {
              fprintf(stderr, "unknown method %s\n", optarg);
              break ;
            }
          dpl_req_set_method(req, method);
          break ;
        }
      case 'b':
        {
          ret = dpl_req_set_bucket(req, optarg);
          if (DPL_SUCCESS != ret)
            {
              fprintf(stderr, "error setting bucket %s\n", optarg);
              exit(1);
            }
          break ;
        }
      case 'r':
        {
          ret = dpl_req_set_resource(req, optarg);
          if (DPL_SUCCESS != ret)
            {
              fprintf(stderr, "error setting resource %s: %s (%d)\n", optarg, dpl_status_str(ret), ret);
              exit(1);
            }
          break ;
        }
      case 's':
        {
          ret = dpl_req_set_subresource(req, optarg);
          if (DPL_SUCCESS != ret)
            {
              fprintf(stderr, "error setting subresource %s: %s (%d)\n", optarg, dpl_status_str(ret), ret);
              exit(1);
            }
          break ;
        }
      case 'M':
        {
          dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);
          break ;
        }
      case 'c':
        {
          int fd;
          struct stat st;
          dpl_chunk_t *chunk;
          ssize_t cc;

          fd = open(optarg, O_RDONLY);
          if (-1 == fd)
            xperror("open");
          ret = fstat(fd, &st);
          if (-1 == ret)
            xperror("stat");
          chunk = xmalloc(sizeof (*chunk));
          chunk->buf = xmalloc(st.st_size);
          cc = read(fd, chunk->buf, st.st_size);
          if (cc != st.st_size)
            {
              fprintf(stderr, "short read\n");
              exit(1);
            }
          chunk->len = st.st_size;
          dpl_req_set_chunk(req, chunk);
          close(fd);
          break ;
        }
      case 'C':
        {
          ret = dpl_req_set_content_type(req, optarg);
          if (DPL_SUCCESS != ret)
            {
              fprintf(stderr, "error setting content_type %s: %s (%d)\n", optarg, dpl_status_str(ret), ret);
              exit(1);
            }
          break ;
        }
      case 'e':
        {
          dpl_req_add_behavior(req, DPL_BEHAVIOR_EXPECT);
          break ;
        }
      case 't':
        {
          ctx->trace_level = strtoul(optarg, NULL, 0);
          break ;
        }
      case 'q':
        {
          query_params = dpl_parse_query_params(optarg);
          if (NULL == query_params)
            {
              fprintf(stderr, "error parsing query_params\n");
              exit(1);
            }
          break ;
        }
      case '?':
      default:
        usage();
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
      usage();
    }

  ret = dpl_req_build(req, &headers_request);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error in request: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  ret = dpl_req_gen_http_request(req, headers_request, query_params, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error in request: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  ret = fwrite(header, header_len, 1, stdout);
  if (1 != ret)
    {
      fprintf(stderr, "short write\n");
      exit(1);
    }

  dpl_dict_free(headers_request);
  if (NULL != query_params)
    dpl_dict_free(query_params);
  if (NULL != req->chunk)
    {
      free(req->chunk->buf);
      free(req->chunk);
    }
  dpl_req_free(req);
  dpl_ctx_free(ctx);
  dpl_free();

  return 0;
}
