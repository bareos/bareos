The Run directive defines when a Job is to be run, and what overrides if any to apply. You may specify multiple run directives within a Schedule resource. If you do, they will all be applied (i.e. multiple schedules). If you have two Run directives that start at the same time, two Jobs will start at the same time (well, within one second of each other).

The Job-overrides permit overriding the Level, the Storage, the Messages, and the Pool specifications provided in the Job resource. In addition, the FullPool, the IncrementalPool, and the DifferentialPool specifications permit overriding the Pool specification according to what backup Job Level is in effect.

By the use of overrides, you may customize a particular Job. For example, you may specify a Messages override for your Incremental backups that outputs messages to a log file, but for your weekly or monthly Full backups, you may send the output by email by using a different Messages override.

Job-overrides are specified as: keyword=value where the keyword is Level, Storage, Messages, Pool, FullPool, DifferentialPool, or IncrementalPool, and the value is as defined on the respective directive formats for the Job resource. You may specify multiple Job-overrides on one Run directive by separating them with one or more spaces or by separating them with a trailing comma. For example:

Level=Full
   :index:`[TAG=Level] <single: Level>` :index:`[TAG=Directive->Level] <single: Directive; Level>` is all files in the FileSet whether or not they have changed.

Level=Incremental
   :index:`[TAG=Level] <single: Level>` :index:`[TAG=Directive->Level] <single: Directive; Level>` is all files that have changed since the last backup.

Pool=Weekly
   :index:`[TAG=Pool] <single: Pool>` :index:`[TAG=Directive->Pool] <single: Directive; Pool>` specifies to use the Pool named Weekly.

Storage=DLT_Drive
   :index:`[TAG=Storage] <single: Storage>` :index:`[TAG=Directive->Storage] <single: Directive; Storage>` specifies to use DLT_Drive for the storage device.

Messages=Verbose
   :index:`[TAG=Messages] <single: Messages>` :index:`[TAG=Directive->Messages] <single: Directive; Messages>` specifies to use the Verbose message resource for the Job.

FullPool=Full
   :index:`[TAG=FullPool] <single: FullPool>` :index:`[TAG=Directive->FullPool] <single: Directive; FullPool>` specifies to use the Pool named Full if the job is a full backup, or is upgraded from another type to a full backup.

DifferentialPool=Differential
   :index:`[TAG=DifferentialPool] <single: DifferentialPool>` :index:`[TAG=Directive->DifferentialPool] <single: Directive; DifferentialPool>` specifies to use the Pool named Differential if the job is a differential backup.

IncrementalPool=Incremental
   :index:`[TAG=IncrementalPool] <single: IncrementalPool>` :index:`[TAG=Directive->IncrementalPool] <single: Directive; IncrementalPool>` specifies to use the Pool named Incremental if the job is an incremental backup.

Accurate=yes|no
   :index:`[TAG=Accurate] <single: Accurate>` :index:`[TAG=Directive->Accurate] <single: Directive; Accurate>` tells Bareos to use or not the Accurate code for the specific job. It can allow you to save memory and and CPU resources on the catalog server in some cases.

SpoolData=yes|no
   :index:`[TAG=SpoolData] <single: SpoolData>` :index:`[TAG=Directive->Spool Data] <single: Directive; Spool Data>` tells Bareos to use or not to use spooling for the specific job.

Date-time-specification determines when the Job is to be run. The specification is a repetition, and as a default Bareos is set to run a job at the beginning of the hour of every hour of every day of every week of every month of every year. This is not normally what you want, so you must specify or limit when you want the job to run. Any specification given is assumed to be repetitive in nature and will serve to override or limit the default repetition. This is done by specifying masks or times
for the hour, day of the month, day of the week, week of the month, week of the year, and month when you want the job to run. By specifying one or more of the above, you can define a schedule to repeat at almost any frequency you want.

Basically, you must supply a month, day, hour, and minute the Job is to be run. Of these four items to be specified, day is special in that you may either specify a day of the month such as 1, 2, ... 31, or you may specify a day of the week such as Monday, Tuesday, ... Sunday. Finally, you may also specify a week qualifier to restrict the schedule to the first, second, third, fourth, or fifth week of the month.

For example, if you specify only a day of the week, such as Tuesday the Job will be run every hour of every Tuesday of every Month. That is the month and hour remain set to the defaults of every month and all hours.

Note, by default with no other specification, your job will run at the beginning of every hour. If you wish your job to run more than once in any given hour, you will need to specify multiple run specifications each with a different minute.

The date/time to run the Job can be specified in the following way in pseudo-BNF:

# Tabular in LaTex format (original)

::

   \begin{longtable}{ l @{ ::= } p{0.5\textwidth} }
      <week-keyword>    & 1st | 2nd | 3rd | 4th | 5th | first |
                           second | third | fourth | fifth | last \\
      <wday-keyword>    & sun | mon | tue | wed | thu | fri | sat |
                          sunday | monday | tuesday | wednesday |
                          thursday | friday | saturday \\
      <week-of-year-keyword> & w00 | w01 | ... w52 | w53 \\
      <month-keyword>   & jan | feb | mar | apr | may | jun | jul |
                          aug | sep | oct | nov | dec |
                          january | february | ... | december \\
      <digit>           & 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 \\
      <number>          & <digit> | <digit><number> \\
      <12hour>          & 0 | 1 | 2 | ... 12 \\
      <hour>            & 0 | 1 | 2 | ... 23 \\
      <minute>          & 0 | 1 | 2 | ... 59 \\
      <day>             & 1 | 2 | ... 31 \\
      <time>            & <hour>:<minute> |
                          <12hour>:<minute>am |
                          <12hour>:<minute>pm \\
      <time-spec>       & at <time> | hourly \\
      % ??? <date-keyword>    & on | weekly \\
      <day-range>       & <day>-<day> \\
      <month-range>     & <month-keyword>-<month-keyword> \\
      <wday-range>      & <wday-keyword>-<wday-keyword> \\
      <range>           & <day-range> | <month-range> |
                                <wday-range> \\
      <modulo>          & <day>/<day> | <week-of-year-keyword>/<week-of-year-keyword> \\
      <date>            & <date-keyword> | <day> | <range> \\
      <date-spec>       & <date> | <date-spec> \\
      <day-spec>        & <day> | <wday-keyword> |
                          <day> | <wday-range> |
                          <week-keyword> <wday-keyword> |
                          <week-keyword> <wday-range> |
                          daily \\
      <month-spec>      & <month-keyword> | <month-range> | monthly \\
      <date-time-spec>  & <month-spec> <day-spec> <time-spec> \\
      \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

========================================== =========================================================================================================================================================================================================================================================================================================================================================================================
<week-keyword>         1st | 2nd | 3rd | 4th | 5th | first | second | third | fourth | fifth | last
<wday-keyword>         sun | mon | tue | wed | thu | fri | sat | sunday | monday | tuesday | wednesday | thursday | friday | saturday
<week-of-year-keyword> w00 | w01 | ... w52 | w53
<month-keyword>        jan | feb | mar | apr | may | jun | jul | aug | sep | oct | nov | dec | january | february | ... | december
<digit>                1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0
<number>               <digit> | <digit><number>
<12hour>               0 | 1 | 2 | ... 12
<hour>                 0 | 1 | 2 | ... 23
<minute>               0 | 1 | 2 | ... 59
<day>                  1 | 2 | ... 31
<time>                 <hour>:<minute> | <12hour>:<minute>am | <12hour>:<minute>pm
<time-spec>            at <time> | hourly
<day-range>            <day>-<day>
<month-range>          <month-keyword>-<month-keyword>
<wday-range>           <wday-keyword>-<wday-keyword>
<range>                <day-range> | <month-range> | <wday-range>
<modulo>               <day>/<day> | <week-of-year-keyword>/<week-of-year-keyword>
<date>                 <date-keyword> | <day> | <range>
<date-spec>            <date> | <date-spec>
<day-spec>             <day> | <wday-keyword> | <day> | <wday-range> | <week-keyword> <wday-keyword> | <week-keyword> <wday-range> | daily
<month-spec>           <month-keyword> | <month-range> | monthly
<date-time-spec>       <month-spec> <day-spec> <time-spec>
========================================== =========================================================================================================================================================================================================================================================================================================================================================================================

