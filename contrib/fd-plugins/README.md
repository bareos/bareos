This directory is intended for Python FD plugins. Please use a subdirectory for each plugin.

Please add a little documentation with a working configuration snippet to your contributions.

### Introduction

The Bareos File Daemon plugin interface calls C-functions at certain entry points in the FD code.
Developing plugins required deep C-knowledge.
In Bareos this plugin interface has been extended with python bindings.
This allows developers to write FD plugins in Python.

The following presentation gives an overview about Bareos Python plugins:
https://archive.fosdem.org/2017/schedule/event/backup_dr_bareos_plugins/
