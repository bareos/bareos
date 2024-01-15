-- simple script to create activity during the backup.

create table testing(content text, stamp timestamptz, table_size bigint, index_size bigint);
select pg_sleep(0.25),clock_timestamp();

create unique index testing_content_index on testing(content);
select pg_sleep(0.15),clock_timestamp();

create index testing_stamp_index on testing(stamp desc);
select pg_sleep(0.15),clock_timestamp();

insert into testing(content,stamp,table_size)
  select
    (case when random() < 0.5
          then 'https://bareos.org/current_'
          else 'https://bareos.com/release_'
      end)
      || repeat('record:' ,2 )
      || generate_series(101,100000)
      || ' pretty_table_size: '
      || (select pg_size_pretty(pg_relation_size('testing')) as table_size)
      || ' pretty_index_size: '
      || (select pg_size_pretty(pg_indexes_size('testing_content_index')) as index_size)
      , clock_timestamp()
      , (select pg_relation_size('testing') as table_size)
;

select pg_sleep(0.25),clock_timestamp();

update testing set index_size=(select pg_indexes_size('testing_content_index')) where table_size > 0;

select pg_sleep(0.5),clock_timestamp();

reindex table testing;

select pg_sleep(1),clock_timestamp();

checkpoint;

drop table testing;
