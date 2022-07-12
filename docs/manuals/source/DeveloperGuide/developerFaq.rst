.. _developer-guide-faq:

Developer FAQ
=============

Job run
-------

How to run a job that automatically fails?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
I want to run a job that automatically fails in order to test rescheduling on error.

To run a job that automatically fails, you need to use the directive :config:option:`dir/job/ClientRunBeforeJob`.
This directive launches a script on the client-side ( file daemon ) before running the job.
set the script to be “/bin/false”

.. code-block:: sh
   :caption: Job with client run before job directive

    Job {
        Name = "backup-bareos-fd"
        JobDefs = "DefaultJob"
        Client = "bareos-fd"
        Client Run Before Job = /bin/false
    }

Now you can add the reschedule directives as follows

.. code-block:: sh
   :caption: Job with reschedule directives

    Job {
        Name = "backup-bareos-fd"
        JobDefs = "DefaultJob"
        Client = "bareos-fd"
        Client Run Before Job = /bin/false
        Reschedule On Error = yes
        Reschedule Times = 4
        Reschedule Interval = 5 seconds
    }


