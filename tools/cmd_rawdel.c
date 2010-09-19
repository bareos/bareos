
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
