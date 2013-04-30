use mysql
grant all privileges on bacula.* to bacula@localhost;
grant all privileges on bacula.* to bacula@"%";
select * from user;
flush privileges;
