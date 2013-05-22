# bareos-fuse

a python-fuse based virtual filesystem for recovery

current status:  prototype/proof of concept

to demonstrate access to backuped files information

## What is working?
- Bareos (or Bacula) restore filesets can be mounted and accessed

## What is not working?
- only readdir and stat. No reading or opening of files
- currently, only most recent backup for a client is implemented.
  Change the selection nummers in the source code to change this
- relies on cached information. Cached is filled by readdir.
  If you want to access a subdirectory, make sure, you have read the upper directories before

## Prerequisites:
- bconsole must be installed and configured
- python-fuse must be installed

## Usage:
./bareos-fuse.py /mnt
ls -la /mnt
ls -la /mnt/usr
...
fusermount -u /mnt


For debugging purposes, better use
./baculafs.py -f -s -d /mnt

Also take a look at the log files /tmp/bareos-fuse.log and /tmp/bareos-fuse-bconsole.log
