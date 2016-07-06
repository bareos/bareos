/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#include "dropletp.h"

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_method_t
dpl_method(char *str)
{
  if (!strcasecmp(str, "GET"))
    return DPL_METHOD_GET;
  else if (!strcasecmp(str, "PUT"))
    return DPL_METHOD_PUT;
  else if (!strcasecmp(str, "DELETE"))
    return DPL_METHOD_DELETE;
  else if (!strcasecmp(str, "HEAD"))
    return DPL_METHOD_HEAD;
  else if (!strcasecmp(str, "POST"))
    return DPL_METHOD_POST;

  return -1;
}

char *
dpl_method_str(dpl_method_t method)
{
  switch (method)
    {
    case DPL_METHOD_GET:
      return "GET";
    case DPL_METHOD_PUT:
      return "PUT";
    case DPL_METHOD_DELETE:
      return "DELETE";
    case DPL_METHOD_HEAD:
      return "HEAD";
    case DPL_METHOD_POST:
      return "POST";
    }

  return NULL;
}

/**/

dpl_location_constraint_t
dpl_location_constraint(char *str)
{
  if (!strcasecmp(str, ""))
    return DPL_LOCATION_CONSTRAINT_US_STANDARD;
  else if (!strcasecmp(str, "us-east-1"))
    return DPL_LOCATION_CONSTRAINT_US_EAST_1;
  else if (!strcasecmp(str, "us-west-1"))
    return DPL_LOCATION_CONSTRAINT_US_WEST_1;
  else if (!strcasecmp(str, "us-west-2"))
    return DPL_LOCATION_CONSTRAINT_US_WEST_2;
  else if (!strcasecmp(str, "EU"))
    return DPL_LOCATION_CONSTRAINT_EU;
  else if (!strcasecmp(str, "eu-west-1"))
    return DPL_LOCATION_CONSTRAINT_EU_WEST_1;
  else if (!strcasecmp(str, "eu-central-1"))
    return DPL_LOCATION_CONSTRAINT_EU_CENTRAL_1;
  else if (!strcasecmp(str, "ap-southeast-1"))
    return DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1;
  else if (!strcasecmp(str, "ap-southeast-2"))
    return DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_2;
  else if (!strcasecmp(str, "ap-northeast-1"))
    return DPL_LOCATION_CONSTRAINT_AP_NORTHEAST_1;
  else if (!strcasecmp(str, "sa-east-1"))
    return DPL_LOCATION_CONSTRAINT_SA_EAST_1;

  return -1;
}

char *
dpl_location_constraint_str(dpl_location_constraint_t location_constraint)
{
  switch (location_constraint)
    {
    case DPL_LOCATION_CONSTRAINT_UNDEF:
      return NULL;
    /* case DPL_LOCATION_CONSTRAINT_US_STANDARD: */
    case DPL_LOCATION_CONSTRAINT_US_EAST_1:
      return "us-east-1";
    case DPL_LOCATION_CONSTRAINT_US_WEST_1:
      return "us-west-1";
    case DPL_LOCATION_CONSTRAINT_US_WEST_2:
      return "us-west-2";
    /* case DPL_LOCATION_CONSTRAINT_EU: */
    case DPL_LOCATION_CONSTRAINT_EU_WEST_1:
      return "eu-west-1";
    case DPL_LOCATION_CONSTRAINT_EU_CENTRAL_1:
      return "eu-central-1";
    case DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1:
      return "ap-southeast-1";
    case DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_2:
      return "ap-southeast-2";
    case DPL_LOCATION_CONSTRAINT_AP_NORTHEAST_1:
      return "ap-northeast-1";
    case DPL_LOCATION_CONSTRAINT_SA_EAST_1:
      return "sa-east-1";
    }

  return NULL;
}

/**/

dpl_canned_acl_t
dpl_canned_acl(char *str)
{
  if (!strcasecmp(str, "private"))
    return DPL_CANNED_ACL_PRIVATE;
  else if (!strcasecmp(str, "public-read"))
    return DPL_CANNED_ACL_PUBLIC_READ;
  else if (!strcasecmp(str, "public-read-write"))
    return DPL_CANNED_ACL_PUBLIC_READ_WRITE;
  else if (!strcasecmp(str, "authenticated-read"))
    return DPL_CANNED_ACL_AUTHENTICATED_READ;
  else if (!strcasecmp(str, "bucket-owner-read"))
    return DPL_CANNED_ACL_BUCKET_OWNER_READ;
  else if (!strcasecmp(str, "bucket-owner-full-control"))
    return DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL;

  return -1;
}

char *
dpl_canned_acl_str(dpl_canned_acl_t canned_acl)
{
  switch (canned_acl)
    {
    case DPL_CANNED_ACL_UNDEF:
      return "undef";
    case DPL_CANNED_ACL_PRIVATE:
      return "private";
    case DPL_CANNED_ACL_PUBLIC_READ:
      return "public-read";
    case DPL_CANNED_ACL_PUBLIC_READ_WRITE:
      return "public-read-write";
    case DPL_CANNED_ACL_AUTHENTICATED_READ:
      return "authenticated-read";
    case DPL_CANNED_ACL_BUCKET_OWNER_READ:
      return "bucket-owner-read";
    case DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL:
      return "bucket-owner-full-control";
    }

  return NULL;
}

/**/

dpl_storage_class_t
dpl_storage_class(char *str)
{
  if (!strcasecmp(str, "standard"))
    return DPL_STORAGE_CLASS_STANDARD;
  else if (!strcasecmp(str, "standard_ia"))
    return DPL_STORAGE_CLASS_STANDARD_IA;
  else if (!strcasecmp(str, "reduced_redundancy"))
    return DPL_STORAGE_CLASS_REDUCED_REDUNDANCY;
  else if (!strcasecmp(str, "custom"))
    return DPL_STORAGE_CLASS_CUSTOM;

  return -1;
}

char *
dpl_storage_class_str(dpl_storage_class_t storage_class)
{
  switch (storage_class)
    {
    case DPL_STORAGE_CLASS_UNDEF:
      return NULL;
    case DPL_STORAGE_CLASS_STANDARD:
      return "STANDARD";
    case DPL_STORAGE_CLASS_STANDARD_IA:
      return "STANDARD_IA";
    case DPL_STORAGE_CLASS_REDUCED_REDUNDANCY:
      return "REDUCED_REDUNDANCY";
    case DPL_STORAGE_CLASS_CUSTOM:
      return "CUSTOM";
    }

  return NULL;
}

/**/

dpl_copy_directive_t
dpl_copy_directive(char *str)
{
  if (!strcasecmp(str, "copy"))
    return DPL_COPY_DIRECTIVE_COPY;
  else if (!strcasecmp(str, "metadata_replace"))
    return DPL_COPY_DIRECTIVE_METADATA_REPLACE;
  else if (!strcasecmp(str, "link"))
    return DPL_COPY_DIRECTIVE_LINK;
  else if (!strcasecmp(str, "symlink"))
    return DPL_COPY_DIRECTIVE_SYMLINK;
  else if (!strcasecmp(str, "move"))
    return DPL_COPY_DIRECTIVE_MOVE;
  else if (!strcasecmp(str, "mkdent"))
    return DPL_COPY_DIRECTIVE_MKDENT;
  else if (!strcasecmp(str, "rmdent"))
    return DPL_COPY_DIRECTIVE_RMDENT;
  else if (!strcasecmp(str, "mvdent"))
    return DPL_COPY_DIRECTIVE_MVDENT;

  return -1;
}

char *
dpl_copy_directive_str(dpl_copy_directive_t copy_directive)
{
  switch (copy_directive)
    {
    case DPL_COPY_DIRECTIVE_UNDEF:
      return NULL;
    case DPL_COPY_DIRECTIVE_COPY:
      return "COPY"; //case is important
    case DPL_COPY_DIRECTIVE_METADATA_REPLACE:
      return "METADATA_REPLACE"; //case is important
    case DPL_COPY_DIRECTIVE_LINK:
      return "LINK"; //case is important
    case DPL_COPY_DIRECTIVE_SYMLINK:
      return "SYMLINK"; //case is important
    case DPL_COPY_DIRECTIVE_MOVE:
      return "MOVE"; //case is important
    case DPL_COPY_DIRECTIVE_MKDENT:
      return "MKDENT"; //case is important
    case DPL_COPY_DIRECTIVE_RMDENT:
      return "RMDENT"; //case is important
    case DPL_COPY_DIRECTIVE_MVDENT:
      return "MVDENT"; //case is important
    }

  return NULL;
}

/**/

dpl_ftype_t
dpl_object_type(char *str)
{
  if (!strcasecmp(str, "any"))
    return DPL_FTYPE_ANY;
  else if (!strcasecmp(str, "reg") || !strcasecmp(str, "object"))
    return DPL_FTYPE_REG;
  else if (!strcasecmp(str, "dir") || !strcasecmp(str, "container"))
    return DPL_FTYPE_DIR;
  else if (!strcasecmp(str, "cap")|| !strcasecmp(str, "capability"))
    return DPL_FTYPE_CAP;

  return -1;
}

char *
dpl_object_type_str(dpl_ftype_t object_type)
{
  switch (object_type)
    {
    case DPL_FTYPE_UNDEF:
      return NULL;
    case DPL_FTYPE_ANY:
      return "any";
    case DPL_FTYPE_REG:
      return "reg";
    case DPL_FTYPE_DIR:
      return "dir";
    case DPL_FTYPE_CAP:
      return "cap";
    case DPL_FTYPE_DOM:
      return "dom";
    case DPL_FTYPE_CHRDEV:
      return "chrdev";
    case DPL_FTYPE_BLKDEV:
      return "blkdev";
    case DPL_FTYPE_FIFO:
      return "fifo";
    case DPL_FTYPE_SOCKET:
      return "socket";
    case DPL_FTYPE_SYMLINK:
      return "symlink";
    }

  return NULL;
}

dpl_dict_t *
dpl_parse_metadata(char *metadata)
{
  char *saveptr = NULL;
  char *str, *tok, *p;
  int ret;
  dpl_dict_t *dict;
  char *nmetadata;

  nmetadata = strdup(metadata);
  if (NULL == nmetadata)
    return NULL;

  dict = dpl_dict_new(13);
  if (NULL == dict)
    {
      free(nmetadata);
      return NULL;
    }

  for (str = metadata;;str = NULL)
    {
      tok = strtok_r(str, ";,", &saveptr);
      if (NULL == tok)
        break ;

      DPRINTF("tok=%s\n", tok);

      p = index(tok, '=');
      if (NULL == p)
        p = "";
      else
        *p++ = 0;

      ret = dpl_dict_add(dict, tok, p, 0);
      if (DPL_SUCCESS != ret)
        {
          dpl_dict_free(dict);
          free(nmetadata);
          return NULL;
        }
    }

  free(nmetadata);

  return dict;
}

/**
 * parse a string of the form query_params1=value1;query_params2=value2...
 *
 * @param query_params
 *
 * @return
 */
dpl_dict_t *
dpl_parse_query_params(char *query_params)
{
  char *saveptr = NULL;
  char *str, *tok, *p;
  int ret;
  dpl_dict_t *dict;
  char *nquery_params;

  nquery_params = strdup(query_params);
  if (NULL == nquery_params)
    return NULL;

  dict = dpl_dict_new(13);
  if (NULL == dict)
    {
      free(nquery_params);
      return NULL;
    }

  for (str = query_params;;str = NULL)
    {
      tok = strtok_r(str, ";", &saveptr);
      if (NULL == tok)
        break ;

      DPRINTF("tok=%s\n", tok);

      p = index(tok, '=');
      if (NULL == p)
        p = "";
      else
        *p++ = 0;

      ret = dpl_dict_add(dict, tok, p, 0);
      if (DPL_SUCCESS != ret)
        {
          dpl_dict_free(dict);
          free(nquery_params);
          return NULL;
        }
    }

  free(nquery_params);

  return dict;
}

/**/

static dpl_status_t
condition_add(dpl_condition_t *cond,
              time_t *now,
              char *key,
              char *value)
{
  dpl_condition_type_t type;
  size_t len;
  time_t dt;

  if (cond->n_conds >= DPL_COND_MAX)
    {
      return DPL_ENAMETOOLONG;
    }

  type = 0;

  if (!strcmp(key, "if-modified-since"))
    type = DPL_CONDITION_IF_MODIFIED_SINCE;
  else if (!strcmp(key, "if-unmodified-since"))
    type = DPL_CONDITION_IF_UNMODIFIED_SINCE;
  else if (!strcmp(key, "if-match"))
    type = DPL_CONDITION_IF_MATCH;
  else if (!strcmp(key, "if-none-match"))
    type = DPL_CONDITION_IF_NONE_MATCH;
  else
    return DPL_EINVAL;

  switch (type)
    {
    case DPL_CONDITION_IF_MODIFIED_SINCE:
    case DPL_CONDITION_IF_UNMODIFIED_SINCE:
      dt = dpl_get_date(value, now);
      if (dt < 0)
	return DPL_EINVAL;
      cond->conds[cond->n_conds].time = dt;
      break ;
    case DPL_CONDITION_IF_MATCH:
    case DPL_CONDITION_IF_NONE_MATCH:
      len = strlen(value);
      if (len > DPL_ETAG_SIZE)
	return DPL_EINVAL;
      strcpy(cond->conds[cond->n_conds].etag, value);
      break ;
    }

  cond->conds[cond->n_conds].type = type;

  cond->n_conds++;
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_parse_condition(const char *str,
                    dpl_condition_t *condp)
{
  char *nstr = NULL;
  dpl_status_t ret, ret2;
  char *saveptr, *s, *tok, *p;
  dpl_condition_t cond;
  time_t now = time(0);

  memset(&cond, 0, sizeof (cond));

  nstr = strdup(str);
  if (NULL == nstr)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  for (s = nstr;;s = NULL)
    {
      tok = strtok_r(s, ";, ", &saveptr);
      if (NULL == tok)
        break ;

      p = index(tok, ':');
      if (NULL == p)
        {
          ret = DPL_EINVAL;
          goto end;
        }

      *p++ = 0;
      
      ret2 = condition_add(&cond, &now, tok, p);
      if (0 != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != condp)
    memcpy(condp, &cond, sizeof (cond));
  
  ret = DPL_SUCCESS;
  
 end:

  free(nstr);

  return ret;
}

/**/

static dpl_status_t
option_add(dpl_option_t *opt,
           char *key,
           char *value)
{
  if (!strcmp(key, "lazy"))
    opt->mask |= DPL_OPTION_LAZY;
  else if (!strcmp(key, "http_compat"))
    opt->mask |= DPL_OPTION_HTTP_COMPAT;
  else if (!strcmp(key, "raw"))
    opt->mask |= DPL_OPTION_RAW;
  else if (!strcmp(key, "append_metadata"))
    opt->mask |= DPL_OPTION_APPEND_METADATA;
  else if (!strcmp(key, "consistent"))
    opt->mask |= DPL_OPTION_CONSISTENT;
  else if (!strcmp(key, "expect_version"))
    {
      opt->mask |= DPL_OPTION_EXPECT_VERSION;
      snprintf(opt->expect_version, sizeof (opt->expect_version), "%s", value);
    }
  else if (!strcmp(key, "force_version"))
    {
      opt->mask |= DPL_OPTION_FORCE_VERSION;
      snprintf(opt->force_version, sizeof (opt->force_version), "%s", value);
    }
  else
    return DPL_EINVAL;
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_parse_option(const char *str,
                 dpl_option_t *optp)
{
  char *nstr = NULL;
  dpl_status_t ret, ret2;
  char *saveptr, *s, *tok, *p;
  dpl_option_t opt;

  memset(&opt, 0, sizeof (opt));

  nstr = strdup(str);
  if (NULL == nstr)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  for (s = nstr;;s = NULL)
    {
      tok = strtok_r(s, ";, ", &saveptr);
      if (NULL == tok)
        break ;

      p = index(tok, ':');
      if (NULL == p)
        {
          ret = DPL_EINVAL;
          goto end;
        }

      *p++ = 0;
      
      ret2 = option_add(&opt, tok, p);
      if (0 != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != optp)
    memcpy(optp, &opt, sizeof (opt));
  
  ret = DPL_SUCCESS;
  
 end:

  free(nstr);

  return ret;
}
