

struct usage_def head_usage[] =
  {
    {'v', USAGE_PARAM, "trace_level", VFLAG_HELP_STR},
    {'d', USAGE_PARAM, "droplets_dir", "default is '~/.droplets'"},
    {'p', USAGE_PARAM, "profile_name", "default is 'default'"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket:resource[?subresource]", "remote file"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def head_cmd = {"dpl_head", "Dump metadata of object", head_usage, head_func};
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
