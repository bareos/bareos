The Run directive defines when a Job is to be run, and what overrides if any to apply. You may specify multiple run directives within a Schedule resource. If you do, they will all be applied (i.e. multiple schedules). If you have two Run directives that start at the same time, two Jobs will start at the same time (well, within one second of each other).

The Job-overrides permit overriding the Level, the Storage, the Messages, and the Pool specifications provided in the Job resource. In addition, the FullPool, the IncrementalPool, and the DifferentialPool specifications permit overriding the Pool specification according to what backup Job Level is in effect.

By the use of overrides, you may customize a particular Job. For example, you may specify a Messages override for your Incremental backups that outputs messages to a log file, but for your weekly or monthly Full backups, you may send the output by email by using a different Messages override.

Job-overrides are specified as: keyword=value where the keyword is Level, Storage, Messages, Pool, FullPool, DifferentialPool, or IncrementalPool, and the value is as defined on the respective directive formats for the Job resource. You may specify multiple Job-overrides on one Run directive by separating them with one or more spaces or by separating them with a trailing comma. For example:

Level=Full
   :index:`\ <single: Level>`\  :index:`\ <single: Directive; Level>`\  is all files in the FileSet whether or not they have changed.

Level=Incremental
   :index:`\ <single: Level>`\  :index:`\ <single: Directive; Level>`\  is all files that have changed since the last backup.

Pool=Weekly
   :index:`\ <single: Pool>`\  :index:`\ <single: Directive; Pool>`\  specifies to use the Pool named Weekly.

Storage=DLT_Drive
   :index:`\ <single: Storage>`\  :index:`\ <single: Directive; Storage>`\  specifies to use DLT_Drive for the storage device.

Messages=Verbose
   :index:`\ <single: Messages>`\  :index:`\ <single: Directive; Messages>`\  specifies to use the Verbose message resource for the Job.

FullPool=Full
   :index:`\ <single: FullPool>`\  :index:`\ <single: Directive; FullPool>`\  specifies to use the Pool named Full if the job is a full backup, or is upgraded from another type to a full backup.

DifferentialPool=Differential
   :index:`\ <single: DifferentialPool>`\  :index:`\ <single: Directive; DifferentialPool>`\  specifies to use the Pool named Differential if the job is a differential backup.

IncrementalPool=Incremental
   :index:`\ <single: IncrementalPool>`\  :index:`\ <single: Directive; IncrementalPool>`\  specifies to use the Pool named Incremental if the job is an incremental backup.

Accurate=yes|no
   :index:`\ <single: Accurate>`\  :index:`\ <single: Directive; Accurate>`\  tells Bareos to use or not the Accurate code for the specific job. It can allow you to save memory and and CPU resources on the catalog server in some cases.

SpoolData=yes|no
   :index:`\ <single: SpoolData>`\  :index:`\ <single: Directive; Spool Data>`\  tells Bareos to use or not to use spooling for the specific job.


To define on what day the schedule is triggered, you can supply specifiers from the following categories:
- month of year (jan, feb, ...)
- week of year (w00, w01, ...)
- week of month (1st, ..., 5th, last)
- day of month (1, 2, ...)
- day of week (mon, tue, ...)

If you do not specify any of these, the schedule triggers every day.

For a schedule to be triggered on a certain day, it must match with a specifier from all specified categories:
``jan mon``
triggers every monday in january

You can specify multiple keywords from the same category:
``jan, feb mon``
triggers every monday in january and february

You can also specify a range for a category:
``mar-dec``
triggers every day from march to december
``jan, mar, jun-aug``
triggers every day in january, march, june, juli, august

The order you specify keywords is irrelevant:
``jan, feb mon``
is the same as
``feb mon jan``
Though for the sake of readabilty it makes sense to specify them in the order the categories are listed above.

For advanced schedules, you can specify a modulo operation:
``w00/w02``
triggers every day on even weeks (of the year).
``1/2``
triggers every odd day (of the month).

If a schedule triggers on a day, you can specify at which time(s) the schedule is triggered by supplying specifiers from the following categories:
- time of day (at 12:15, at 09:45pm, ...)
- hourly

If you do not specify any of these, the schedule is triggers at 00:00.

For example
``mon at 12:15``
triggers every monday at 12:15
``mon at 18:00, at 20:15``
triggers every monday at 18:00 and 20:15, so runs twice on mondays!

``mon hourly``
runs every monday every full hour, so 24 times on mondays (at 00:00, ..., at 23:00)! 

``hourly at :15``
or
``hourly at 00:15``
or
``hourly at 13:15``
triggers every day, every hour at quater past (at 00:15, ..., at 23:15)!
Note that specifying a hour in that case has no effect whatsoever.

``hourly at :15, :30``
triggers twice an hour at quarter past and half past.

Again, the order you specify keywords is irrelevant:
``jan first mon at 20:15``
is the same as
``mon at 20:15 first jan``

The following Pseudo-BNF defines the syntax for specifying a schedule:

.. code-block:: bnf
   :caption: schedule run

   <month-of-year>        ::= jan | feb | mar | apr | may | jun | jul | aug | sep | oct | nov | dec | january | ... | december
   <week-of-year>         ::= w00 | ... | w53 // where w01 is the first week of the year according to ISO8601
   <week-of-month>        ::= 1st | 2nd | 3rd | 4th | 5th | first | second | third | fourth | fifth | last
   <day-of-month>         ::= 1 | ... | 31
   <day-of-week>          ::= sun | mon | tue | wed | thu | fri | sat | sunday | ... | saturday

   <range>                ::= <month-of-year>-<month-of-year> | ... | <day-of-week>-<day-of-week>
   <modulo>               ::= <week-of-year>/<week-of-year> | <day-of-month>/<day-of-month>

   <time-of-day-24>       ::= [(0 | ... | 23)]:(0 | ... | 59)
   <time-of-day-12>       ::= (0 | ... | 12):(0 | ... | 59)(am | pm)
   <time-of-day>          ::= at(<time-of-day-24> | <time-of-day-12>)
   
   <cycle>                ::= monthly | weekly | daily // these keywords are deprecated since they have not effect on the schedule
   <schedule>             ::= [on] ((<month-of-year> | ... | <day-of-week>) | <range> | <modulo> | <time-of-day> | hourly | <cycle>) [[,]<schedule>] 

if no schedule is specified, we fall back to "at 00:00" (i.e. run every day at 00:00).
