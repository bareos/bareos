_triggerjob_ is a Python script that allows you to perform a backup for a connected client if a definable time has passed since the last backup. 

It will look for all clients connected to the Director. If it finds a job named **backup-{clientname}** that did not successfully run during the specified time period, it will trigger this job. This way, clients that are not regularly connected to the director, such as notebooks, can be reliably backed up.

_triggerjob_ should be executed regularly to detect newly connected clients. To do so, a **cronjob** should run the script repeatedly.

Note: _triggerjob_ **requires a connection between director and client**. Therefore, activate [Client Initiated Connections](https://docs.bareos.org/TasksAndConcepts/NetworkSetup.html#client-initiated-connection) to automatically establish a connection whenever possible. Otherwise no jobs will be started

Sample usage:

```
$ ./triggerjob.py -p PASSWORD --hours 24 localhost
```

```
$ ./triggerjob.py -h
usage: triggerjob.py [-h] [-d] [--name NAME] -p PASSWORD [--port PORT]
                     [--dirname DIRNAME] [--hours HOURS]
                     [address]

Console to Bareos Director.

positional arguments:
  address               Bareos Director network address

optional arguments:
  -h, --help            show this help message and exit
  -d, --debug           enable debugging output
  --name NAME           use this to access a specific Bareos director named
                        console. Otherwise it connects to the default console
                        ("*UserAgent*")
  -p PASSWORD, --password PASSWORD
                        password to authenticate to a Bareos Director console
  --port PORT           Bareos Director network port
  --dirname DIRNAME     Bareos Director name
  --hours HOURS         Minimum time since last backup in hours
```
