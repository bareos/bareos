Job Execution
=============

Introduction
------------
The different job types are executed differently and also the protocol and information exchange between the daemons differ depending on job options.
Based on a few example jobs the following documentation will try to describe the information exchange.

Job setup and start
-------------------
When a job is started the |dir| will invoke `RunJob()` what will call `SetupJob()` to initialize the job and then pass it to `JobqAdd()`.
After this has happened the job is in the job-queue waiting for a `jobq_server` to pick it up and actually run it.

.. uml:: /DeveloperGuide/jobexec/runjob.puml

After the jobq_server picks up the job a `job_thread` is started. That thread does some more setup work and then runs the type-specific job payload.

.. uml:: /DeveloperGuide/jobexec/job_thread.puml


Simple backup job
-----------------
As there are lots of configuration options that will change the job execution in subtle ways, we're going to assume several things.

* the |fd| is not an active client, so the |dir| initiates the connection
* the |fd| is not a passive client, so the |fd| initiated the connection to the |sd|

When such a job is run, the |dir| connects to the |sd| and does the initial job setup, then the |dir| connects to the |fd| to setup and start the job there. The |fd| then connects to the |sd| and sends it data there.

Overview
~~~~~~~~
.. uml:: /DeveloperGuide/jobexec/backup-short.puml


Detailed View
~~~~~~~~~~~~~
.. uml:: /DeveloperGuide/jobexec/backup-long.puml
