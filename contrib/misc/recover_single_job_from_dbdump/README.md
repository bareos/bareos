How to recover a single accidentally deleted JobId
from the database dump.

- Cut the db dump into single files for easier handling with split_bareos_pg_dump.py

The following tables contain the jobid:

- basefiles
- file
- jobhisto
- jobmedia
- job
- jobstats
- log
- ndmpjobenvironment
- pathvisibility
- restoreobject
- unsavedfiles


- Example: JobID is 192626
- First recover the job and jobmedia entries for the job:
  - Find the job record in the job.sql file and add it to the database
     - head -n1 job.txt > addjob.sql
     - grep ^192626 >> addjob.sql
     - psql bareos < addjob.sql

  - Find the jobmedia records for the jobid and add them to the database
     - head -n1 jobmedia.sql > addjobmedia.sql
     - grep '^[0-9]*[[:space:]]\+192626[[:space:]]' jobmedia.sql  >> addjobmedia.sql
     - psql < addjobmedia.sql

- secondly recover file, filename, path, and restore objects for the job:

