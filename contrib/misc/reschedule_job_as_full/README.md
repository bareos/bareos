# helper script to reschedule a failed job

This helper script can be used to reschedule a failed job
to level=Full.

This kind of situation is encountered often by plugin job
running an incremental and not suitable full is detected.
The job will fail on each run until a valid full is done.

So when the plugin detects such a state, it must emit a
job message that contains the string "A new full level
backup of this job is required." and it must ensure to
terminate the job with failure.

You can add this runscript resource in your job or jobdefs like this:

```
  RunScript {
    RunsWhen = After
    RunsOnFailure = Yes
    RunsOnSuccess = No
    RunsOnClient = No
    Command = "/usr/lib/bareos/scripts/reschedule_job_as_full.sh '%e' '%i' '%n' '%l' '%t'"
  }
```

An additional last argument on the command line can be a path for a debug log file.
