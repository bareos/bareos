
.. _section-dev-webui:

Webui
=====

.. _section-dev-webui-translation:

Translation
-----------

Where do I find the translation?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The localization files are located in :file:`webui/module/Application/language`.
You can find the languages that are currently
in translation in our online project, which is addressed in the next section.

How should I edit translation files?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You are welcome to join our translation community via https://poeditor.com/join/project/ELnLNbvQJb.
After selecting the languages you want to contribute to,
you can add and modify translation strings.
You can mark strings as fuzzy and leave comments, in case there is need for further discussions.
In doubt, contact us via bareos-devel@googlegroups.com.

How can I add a new language?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Via https://poeditor.com/join/project/ELnLNbvQJb you can also add new languages.
When this is done, we make the necessary adjustments to the |webui|.

Where can I find further information?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See https://poeditor.com for more info on POEditor.

How to update files in `module/Application/language` if there were changes in source that affect translation?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run the script `webui/scripts/localization_update.sh` to scan for changes and generate new \*.po(t) files.
The Bareos team takes care of this task and the synchronization with POEditor.

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
