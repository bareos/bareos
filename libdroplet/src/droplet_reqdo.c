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

#include "dropletp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

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

dpl_status_t
dpl_add_metadata_to_headers(dpl_dict_t *metadata,
                            dpl_dict_t *headers)
                            
{
  int bucket;
  dpl_var_t *var;
  char header[1024];
  int ret;

  for (bucket = 0;bucket < metadata->n_buckets;bucket++)
    {
      for (var = metadata->buckets[bucket];var;var = var->prev)
        {
          snprintf(header, sizeof (header), "x-amz-meta-%s", var->key);

          ret = dpl_dict_add(headers, header, var->value, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
    }

  return DPL_SUCCESS;
}


dpl_status_t
dpl_get_metadata_from_headers(dpl_dict_t *headers,
                              dpl_dict_t *metadata)
{
  int bucket;
  dpl_var_t *var;
  int ret;

  for (bucket = 0;bucket < headers->n_buckets;bucket++)
    {
      for (var = headers->buckets[bucket];var;var = var->prev)
        {
          int x_amz_meta_len;

#define X_AMZ_META "x-amz-meta-"
          x_amz_meta_len = strlen(X_AMZ_META);
          if (!strncmp(var->key, X_AMZ_META, x_amz_meta_len))
            {
              char *p;

              p = var->key + x_amz_meta_len;

              ret = dpl_dict_add(metadata, p, var->value, 0);
              if (DPL_SUCCESS != ret)
                {
                  return DPL_FAILURE;
                }
            }
        }
    }

  return DPL_SUCCESS;
}

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

dpl_status_t
dpl_add_condition_to_headers(dpl_condition_t *condition,
                             dpl_dict_t *headers)
{
  int ret;

  if (condition->mask & (DPL_CONDITION_IF_MODIFIED_SINCE|
                         DPL_CONDITION_IF_UNMODIFIED_SINCE))
    {
      char date_str[128];
      struct tm tm_buf;

      ret = strftime(date_str, sizeof (date_str), "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&condition->time, &tm_buf));
      if (0 == ret)
        return DPL_FAILURE;
      
      if (condition->mask & DPL_CONDITION_IF_MODIFIED_SINCE)
        {
          ret = dpl_dict_add(headers, "If-Modified-Since", date_str, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
      
      if (condition->mask & DPL_CONDITION_IF_UNMODIFIED_SINCE)
        {
          ret = dpl_dict_add(headers, "If-Unodified-Since", date_str, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
    }

  if (condition->mask & DPL_CONDITION_IF_MATCH)
    {
      ret = dpl_dict_add(headers, "If-Match", condition->etag, 0);
      if (DPL_SUCCESS != ret)
        {
          return DPL_FAILURE;
        }
    }

  if (condition->mask & DPL_CONDITION_IF_NONE_MATCH)
    {
      ret = dpl_dict_add(headers, "If-None-Match", condition->etag, 0);
      if (DPL_SUCCESS != ret)
        {
          return DPL_FAILURE;
        }
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_do(dpl_req_t *req)
{
  return DPL_FAILURE;
}
