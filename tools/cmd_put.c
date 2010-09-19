
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

