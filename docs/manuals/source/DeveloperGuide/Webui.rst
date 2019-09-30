
.. _section-dev-webui:

Webui
=====

.. _section-dev-webui-command-usage-in-modules:

Command usage in modules and the according ACL requirements
-----------------------------------------------------------


Following is a table of bconsole commands and |webui| modules.
Commands are either marked as required, optional or empty if not used by the module.

Required commands are mandatory for proper module functionality and access to them should
not be denied by :config:option:`dir/profile/CommandACL` settings.

Access to commands marked as optional may be denied by :config:option:`dir/profile/CommandACL` settings.


.. csv-filter::
   :header-rows: 1
   :file: ../../../../webui/module/Application/config/commands.csv
