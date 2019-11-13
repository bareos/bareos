_triggerjob_ is a Python script that allows you to perform a backup for a connected client if a definable time has passed since the last backup. 

This is especially useful for Client Initiated Connections, as the job is only executed when the client is available and not just when a certain amount of time has passed.

Sample usage:

```
$ ./triggerjob.py -p secret --hours 24 localhost
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
