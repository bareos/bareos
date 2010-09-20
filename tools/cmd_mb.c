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

int cmd_mb(int argc, char **argv);

struct usage_def mb_usage[] =
  {
    {'a', USAGE_PARAM, "canned_acl", "default is private"},
    {'A', 0u, NULL, "list available canned acls"},
    {'l', USAGE_PARAM, "location_constraint", "default is US Standard"},
    {'L', 0u, NULL, "list available location constraints"},
    {USAGE_NO_OPT, USAGE_MANDAT, "bucket", "bucket name"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def mb_cmd = {"mb", "make bucket", mb_usage, cmd_mb};

int
cmd_mb(int argc,
       char **argv)
{
  int ret;
  char opt;
  char *bucket;
  char *resource;
  dpl_canned_acl_t canned_acl = DPL_CANNED_ACL_PRIVATE;
  int Aflag = 0;
  int i;
  dpl_location_constraint_t location_constraint = DPL_LOCATION_CONSTRAINT_US_STANDARD;
  int Lflag = 0;  

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;
  
  while ((opt = getopt(argc, argv, usage_getoptstr(mb_usage))) != -1)
    switch (opt)
      {
      case 'l':
        location_constraint = dpl_location_constraint(optarg);
        if (-1 == location_constraint)
          {
            fprintf(stderr, "bad location constraint '%s'\n", optarg);
            return SHELL_CONT;
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
            return SHELL_CONT;
          }
        break ;
      case 'A':
        Aflag = 1;
        break ;
      case '?':
      default:
        usage_help(&mb_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (1 == Aflag)
    {
      for (i = 0;i < DPL_N_CANNED_ACL;i++)
        printf("%s\n", dpl_canned_acl_str(i));
      return SHELL_CONT;
    }

  if (1 == Lflag)
    {
      for (i = 0;i < DPL_N_LOCATION_CONSTRAINT;i++)
        printf("%s\n", dpl_location_constraint_str(i));
      return SHELL_CONT;
    }
  
  if (1 != argc)
    {
      usage_help(&mb_cmd);
      return SHELL_CONT;
    }
  
  bucket = argv[0];
  resource = "/";

  ret = dpl_make_bucket(ctx, bucket, location_constraint, canned_acl);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "status: %s (%d)\n", dpl_status_str(ret), ret);
      return SHELL_CONT;
    }

  var_set("status", "0", VAR_CMD_SET, NULL);
  
  return SHELL_CONT;
}

