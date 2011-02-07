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
    }

  return NULL;
}

/**/

dpl_location_constraint_t
dpl_location_constraint(char *str)
{
  if (!strcasecmp(str, ""))
    return DPL_LOCATION_CONSTRAINT_US_STANDARD;
  else if (!strcasecmp(str, "EU"))
    return DPL_LOCATION_CONSTRAINT_EU;
  else if (!strcasecmp(str, "us-west-1"))
    return DPL_LOCATION_CONSTRAINT_US_WEST_1;
  else if (!strcasecmp(str, "ap-southeast-1"))
    return DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1;

  return -1;
}

char *
dpl_location_constraint_str(dpl_location_constraint_t location_constraint)
{
  switch (location_constraint)
    {
    case DPL_LOCATION_CONSTRAINT_UNDEF:
      return NULL;
    case DPL_LOCATION_CONSTRAINT_US_STANDARD:
      return "";
    case DPL_LOCATION_CONSTRAINT_EU:
      return "EU";
    case DPL_LOCATION_CONSTRAINT_US_WEST_1:
      return "us-west-1";
    case DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1:
      return "ap-southeast-1";
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
  else if (!strcasecmp(str, "autenticated-read"))
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
      return NULL;
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
  else if (!strcasecmp(str, "reduced_redundancy"))
    return DPL_STORAGE_CLASS_REDUCED_REDUNDANCY;

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
      return "standard";
    case DPL_STORAGE_CLASS_REDUCED_REDUNDANCY:
      return "reduced_redundancy";
    }

  return NULL;
}

/**/

dpl_metadata_directive_t
dpl_metadata_directive(char *str)
{
  if (!strcasecmp(str, "copy"))
    return DPL_METADATA_DIRECTIVE_COPY;
  else if (!strcasecmp(str, "replace"))
    return DPL_METADATA_DIRECTIVE_REPLACE;

  return -1;
}

char *
dpl_metadata_directive_str(dpl_metadata_directive_t metadata_directive)
{
  switch (metadata_directive)
    {
    case DPL_METADATA_DIRECTIVE_UNDEF:
      return NULL;
    case DPL_METADATA_DIRECTIVE_COPY:
      return "COPY"; //case is important
    case DPL_METADATA_DIRECTIVE_REPLACE:
      return "REPLACE"; //case is important
    }

  return NULL;
}

/**
 * parse a string of the form metadata1=value1;metadata2=value2...
 *
 * @param metadata
 *
 * @return
 */
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
          free(nmetadata);
          return NULL;
        }
    }

  free(nmetadata);

  return dict;
}

/**/

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

