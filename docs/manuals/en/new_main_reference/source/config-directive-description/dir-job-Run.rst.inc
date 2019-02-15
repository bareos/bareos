:index:`[TAG=Clone a Job] <single: Clone a Job>` The Run directive (not to be confused with the Run option in a Schedule) allows you to start other jobs or to clone the current jobs.

The part after the equal sign must be enclosed in double quotes, and can contain any string or set of options (overrides) that you can specify when entering the :strong:`run` command from the console. For example storage=DDS-4 .... In addition, there are two special keywords that permit you to clone the current job. They are level=%l and since=%s. The %l in the level keyword permits entering the actual level of the current job and the %s in the since keyword permits putting the
same time for comparison as used on the current job. Note, in the case of the since keyword, the %s must be enclosed in double quotes, and thus they must be preceded by a backslash since they are already inside quotes. For example:



   .. literalinclude:: /_static/config/DirJobRun1.conf



A cloned job will not start additional clones, so it is not possible to recurse.

Jobs started by :config:option:`dir/job/Run`\  are submitted for running before the original job (while it is being initialized). This means that any clone job will actually start before the original job, and may even block the original job from starting. It evens ignores :config:option:`dir/job/Priority`\ .

If you are trying to prioritize jobs, you will find it much easier to do using a :config:option:`dir/job/RunScript`\  resource or a :config:option:`dir/job/RunBeforeJob`\  directive.

