This directive specifies the maximum number of Jobs that should run concurrently. Each contact from the Director (e.g. status request, job start request) is considered as a Job, so if you want to be able to do a :bcommand:`status` request in the console at the same time as a Job is running, you will need to set this value greater than 1.

