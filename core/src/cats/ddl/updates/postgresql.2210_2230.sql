-- update db schema from 2210 to 2230
-- start transaction
begin;
update Version set VersionId = 2230;

-- lstat_decode part
-- This function emulate in plpgsql language the C++ code of FromBase64 function in core/src/lib/base64.cc
create or replace function bareos_frombase64(field text) returns bigint as $$
 declare
  i integer default 0;
  res bigint default 0;
  rf text default '';
  base64 text default 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  z integer := char_length(field);
 begin
  rf := substring(field from 0 for 1);
  -- skip negative
  if rf = '-' then i := i+1; end if;
  rf := substring(field from i for 1);
  while  ( rf != ' ' ) and ( i <= z ) loop
    res := res << 6;
    res := res + position(rf in base64) - 1;
    i := i + 1;
    rf := substring(field from i for 1);
  end loop;
  return res;
end
$$ language 'plpgsql' immutable;

-- This function emulate in pgpgsql language the C++ code of DecodeStat
-- function in core/src/lib/attrib.cc.
-- LinkFI is decode here as bigint which differs from the C code.
-- By default we return all fields, limit parameter used in params
-- to obtain the desired values
create or replace function decode_lstat(lst text,
  params text[] = array['st_dev','st_ino','st_mode','st_nlink',
                   'st_uid','st_gid','st_rdev','st_size',
                   'st_blksize','st_blocks','st_atime','st_mtime',
                   'st_ctime','linkfi','st_flags','data']
 )
 returns table(
   st_dev bigint,
   st_ino bigint,
   st_mode bigint,
   st_nlink bigint,
   st_uid bigint,
   st_gid bigint,
   st_rdev bigint,
   st_size bigint,
   st_blksize bigint,
   st_blocks bigint,
   st_atime bigint,
   st_mtime bigint,
   st_ctime bigint,
   linkfi bigint,
   st_flags bigint,
   data bigint
 ) as $$
 declare
  fields text[];
 begin
  fields = string_to_array(lst, ' ');
  return query
  select
    case
      when 'st_dev' = any (params) then
        bareos_frombase64(fields[1])
    end as st_dev,
    case
      when 'st_ino' = any (params) then
        bareos_frombase64(fields[2])
    end as st_ino,
    case
      when 'st_mode' = any (params) then
        bareos_frombase64(fields[3])
    end as st_mode,
    case
      when 'st_nlink' = any (params) then
        bareos_frombase64(fields[4])
    end as st_nlink,
    case
      when 'st_uid' = any (params) then
        bareos_frombase64(fields[5])
    end as st_uid,
    case
      when 'st_gid' = any (params) then
        bareos_frombase64(fields[6])
    end as st_gid,
    case
      when 'st_rdev' = any (params) then
        bareos_frombase64(fields[7])
    end as st_rdev,
    case
      when 'st_size' = any (params) then
        bareos_frombase64(fields[8])
    end as st_size,
    case
      when 'st_blksize' = any (params) then
        bareos_frombase64(fields[9])
    end as st_blksize,
    case
      when 'st_blocks' = any (params) then
        bareos_frombase64(fields[10])
    end as st_blocks,
    case
      when 'st_atime' = any (params) then
        bareos_frombase64(fields[11])
    end as st_atime,
    case
      when 'st_mtime' = any (params) then
        bareos_frombase64(fields[12])
    end as st_mtime,
    case
      when 'st_ctime' = any (params) then
        bareos_frombase64(fields[13])
    end as st_ctime,
    case
      when 'linkfi' = any (params) then
        bareos_frombase64(fields[14])
    end as linkfi,
    case
      when 'st_flags' = any (params) then
        bareos_frombase64(fields[15])
    end as st_flags,
    case
      when 'data' = any (params) then
        bareos_frombase64(fields[16])
    end as data
    ;
 end
$$ language 'plpgsql' immutable;

commit;

begin;
create or replace function pg_temp.exec(raw_query text) returns boolean as $$
begin
  execute raw_query;
  return true;
end
$$
language 'plpgsql';

select
    case when (select setting from pg_settings where name = 'server_version_num')::int > 100000
        then pg_temp.exec('alter sequence basefiles_baseid_seq as bigint;')
    else
        false
    end as baseid_seq
    ,
    case when (select setting from pg_settings where name = 'server_version_num')::int > 100000
        then pg_temp.exec('alter function decode_lstat parallel safe;')
    else
        false
    end as decode_parallel
    ,
    case when (select setting from pg_settings where name = 'server_version_num')::int > 100000
        then pg_temp.exec('alter function bareos_frombase64 parallel safe;')
    else
        false
    end as frombase64_parallel
;

commit;
set client_min_messages = warning;
analyze;
