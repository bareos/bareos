Configuration Parser
====================

Introduction
------------

The **Bareos** configuration parser saves the scanned configuration resources respectively their directives in dedicated classes. The following diagram shows the resource classes overview of the |sd|. All other daemons as well as the |bconsole| and the command line tools are similar with this.

Class Model of Storage Daemon Resources
---------------------------------------
The following diagram shows the class model of the Storage Daemon resources.

.. include:: configparser/sd_resources_class_model.puml


**Outdated: Old Class Model of Storage Daemon Resources**

The following diagram is added for historic documentation purposes only. It contains the old class model of the |sd| resources as it was existent until Bareos Version 18.2.

.. include:: configparser/sd_resources_class_model_outdated.puml

