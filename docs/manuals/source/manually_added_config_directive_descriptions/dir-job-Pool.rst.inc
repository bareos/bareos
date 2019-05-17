The Pool directive defines the pool of Volumes where your data can be backed up. Many Bareos installations will use only the Default pool. However, if you want to specify a different set of Volumes for different Clients or different Jobs, you will probably want to use Pools. For additional details, see the :ref:`DirectorResourcePool` of this chapter. This directive is required.

In case of a Copy or Migration job, this setting determines what Pool will be examined for finding JobIds to migrate. The exception to this is when :config:option:`dir/job/SelectionType`\  = SQLQuery, and although a Pool directive must still be specified, no Pool is used, unless you specifically include it in the SQL query. Note, in any case, the Pool resource defined by the Pool directive must contain a :config:option:`dir/pool/NextPool`\  = ...
directive to define the Pool to which the data will be migrated.

