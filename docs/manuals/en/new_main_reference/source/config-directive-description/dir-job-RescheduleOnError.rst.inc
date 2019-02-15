If this directive is enabled, and the job terminates in error, the job will be rescheduled as determined by the :config:option:`dir/job/RescheduleInterval`\  and :config:option:`dir/job/RescheduleTimes`\  directives. If you cancel the job, it will not be rescheduled.

This specification can be useful for portables, laptops, or other machines that are not always connected to the network or switched on.

