
struct usage_def get_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket:resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def get_cmd = {"dpl_get", "Get Object or subresource", get_usage, get_func};

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
