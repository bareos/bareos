This directory is intended for Python FD plugins. Please use a subdirectory for each plugin.

Please add a little documentation with a working configuration snippet to your contributions.


## Sample pyhon plugins for Bareos FD

### Introduction

Bacula's FD plugin interface calls C-functions at certain entry points in the FD code. Developing plugins required deep C-knowledge. In Bareos this plugin interface has been extended with python bindings (since version 13.4, you need the latest master version, it's not in the current stable release 13.2). This allows developers to write FD plugins in Python.

Documentation is yet to be written, this README gives some starting points and examples but is not intended to replace full documentation. We hope to encourage people to use the Python interface to develop their own plugins to suite their needs.

Any help by means of documentation, new plugins, feedback is appreciated.


### The sample plugin shipped with Bareos


The package `bareos-filedaemon-python-plugin` contains a sample plugin `/usr/lib64/bareos/plugins/bareos-fd.py` which implements needed functions for a bareos-fd-plugin in python. It takes the plugin option 'filename' and adds it to the fileset. 

A class based approach

This sample plugin consists of set of functions that are called by the Bareos FD at certain points. Since I prefer to use classes; I've written a little wrapper class, that makes it hopefully easier to program python plugins using a base class.

The base class and a class based example is now part of the package `bareos-filedaemon-python-plugin`

Please read the Wiki page to learn more about the plugins: https://github.com/aussendorf/bareos-fd-python-plugins/wiki

